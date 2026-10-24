"""Bone-pose bank: per-tick local joint channels for the DS matrix-stack renderer.

Instead of storing every animated vertex position (pose bank), each tick keeps
the 9 local channels (translate, roll/pitch/yaw, scale) of each joint plus the
TransN/XRotN prefix. Only channels that change inside a clip are stored per
tick; the rest are a per-clip base row. The DS rebuilds joint matrices with
hardware matrix commands and submits joint-local vertices (rigid skinning).

Fixed-point formats:
  translation  int16, model units * 8          (0.125 unit steps)
  rotation     int16, 32768 per turn (libnds glRotate*i units, wrapped)
  scale        int16, 4.12
  vertex       int16 v16, model units / LOCAL_DIVISOR in 4.12
The offline check rebuilds every vertex with the quantized channels and
4.12 sine/cosine and compares it with the float pose; error must stay <=2.1
world units like the compact vertex bank.
"""
import json
import math
import numpy as np

CHANNELS=9
LOCAL_DIVISOR=64  # skeleton evaluated in model/64 units so vertices fit v16
TRANSLATE_SCALE=8
TURN=32768
MAX_ERROR=2.1

def quantize(values):
    """values: [..., 9] floats (tx,ty,tz,rx,ry,rz,sx,sy,sz) -> int16."""
    values=np.asarray(values,dtype=np.float64)
    q=np.empty(values.shape,dtype=np.int64)
    q[...,0:3]=np.rint(values[...,0:3]*TRANSLATE_SCALE)
    q[...,3:6]=(np.rint(values[...,3:6]*TURN/(2*math.pi)).astype(np.int64)+TURN//2)%TURN-TURN//2
    q[...,6:9]=np.rint(values[...,6:9]*4096)
    if np.abs(q).max()>32767:raise ValueError('Skeleton channel outside int16')
    return q.astype(np.int16)

def _trig(units,fixed):
    angle=units*2*math.pi/TURN
    c,s=math.cos(angle),math.sin(angle)
    # libnds sinLerp/cosLerp return 4.12 values.
    return (round(c*4096)/4096,round(s*4096)/4096) if fixed else (c,s)

def local_matrix(q,fixed=True):
    """Matrix from quantized channels, composed as T*Rz*Ry*Rx*S (transform())."""
    cr,sr=_trig(int(q[3]),fixed);cp,sp=_trig(int(q[4]),fixed);cy,sy=_trig(int(q[5]),fixed)
    rx=np.array([[1,0,0],[0,cr,-sr],[0,sr,cr]]);ry=np.array([[cp,0,sp],[0,1,0],[-sp,0,cp]])
    rz=np.array([[cy,-sy,0],[sy,cy,0],[0,0,1]])
    m=np.eye(4);m[:3,:3]=rz@ry@rx@np.diag(q[6:9]/4096)
    m[:3,3]=q[0:3]/TRANSLATE_SCALE
    return m

def pose(frame,parents,fixed=True,quantized=True):
    """World matrices for one tick; frame is [26][9] (joint 25 = prefix)."""
    rows=frame if quantized else frame
    make=(lambda r:local_matrix(r,fixed)) if quantized else (lambda r:_float_matrix(r))
    prefix=make(rows[len(parents)]);world=[]
    for joint,parent in enumerate(parents):
        local=make(rows[joint])
        world.append((world[parent] if parent>=0 else prefix)@local)
    return world

def _float_matrix(values):
    tx,ty,tz,rx,ry,rz,sx,sy,sz=values
    cr,sr,cp,sp,cy,syaw=math.cos(rx),math.sin(rx),math.cos(ry),math.sin(ry),math.cos(rz),math.sin(rz)
    m=np.eye(4)
    m[:3,:3]=np.array([[cp*cy,sr*sp*cy-cr*syaw,cr*sp*cy+sr*syaw],
                      [cp*syaw,sr*sp*syaw+cr*cy,cr*sp*syaw-sr*cy],
                      [-sp,sr*cp,cr*cp]])@np.diag([sx,sy,sz])
    m[:3,3]=[tx,ty,tz]
    return m

def parents_from(model):
    parents=[];last={}
    for joint in model.joints:
        depth=joint['depth'];parents.append(last[depth-1] if depth else -1);last[depth]=len(parents)-1
    return parents

def export_skeleton(output,model,vertices,mapping,clips,skeleton,center,scale):
    parents=parents_from(model)
    joints=np.array([v[3][0] for v in vertices]);local=np.array([[*v[3][1],1.0] for v in vertices])
    fixed_local=np.rint(local[:,:3]*4096/LOCAL_DIVISOR)
    if np.abs(fixed_local).max()>32767:raise ValueError('Joint-local vertex outside v16')
    restored_local=np.c_[fixed_local*LOCAL_DIVISOR/4096,np.ones(len(local))]
    lines=['/* Generated from the user ROM; bone-pose bank. Do not distribute. */','#include <stddef.h>','#include "mario_mesh.h"']
    table=[];report=[];total=0;worst=0.0
    for index,clip in enumerate(clips):
        data=skeleton[clip['id']];frames=np.array(data['frames'],dtype=np.float64)
        q=quantize(frames)  # [ticks][26][9]
        flat=q.reshape(len(q),-1)
        varying=[c for c in range(flat.shape[1]) if (flat[:,c]!=flat[0,c]).any()]
        # Offline accuracy: quantized + 4.12 trig against float evaluation.
        error=0.0
        for f in range(len(q)):
            exact=pose(frames[f],parents,quantized=False);approx=pose(q[f],parents)
            for j in range(len(parents)):
                sel=joints==j
                if not sel.any():continue
                a=(local[sel]@exact[j].T)[:,:3];b=(restored_local[sel]@approx[j].T)[:,:3]
                error=max(error,float(np.abs(a-b).max()))
        if error>MAX_ERROR:raise ValueError(f'Skeleton error {error:.3f} in {clip["name"]}')
        worst=max(worst,error)
        lines.append(f'static const int16_t skel_base_{index}[{flat.shape[1]}]={{'+','.join(map(str,flat[0]))+'};')
        if varying:
            lines.append(f'static const uint16_t skel_channels_{index}[]={{'+','.join(map(str,varying))+'};')
            lines.append(f'static const int16_t skel_frames_{index}[]={{'+
                         ','.join(map(str,flat[:,varying].flatten()))+'};')
        bytes_=flat.shape[1]*2+len(varying)*(2+2*len(q))
        total+=bytes_
        ref=lambda name:f'{name}_{index}' if varying else 'NULL'
        table.append(f'{{{ref("skel_frames")},{ref("skel_channels")},skel_base_{index},{len(q)},{len(varying)}}},')
        report.append(dict(name=clip['name'],id=clip['id'],ticks=len(q),varying=len(varying),bytes=bytes_,
                           max_world_error=error))
    lines.append('const MarioSkelClip mario_skel_clips[]={');lines+=table;lines.append('};')
    lines.append('const int8_t mario_joint_parent[]={'+','.join(map(str,parents))+'};')
    lines.append(f'const unsigned mario_joint_count={len(parents)};')
    lines.append('const MarioSkelVertex mario_skel_vertices[]={'+
                 ','.join('{%d,%d,%d,%d}'%(*map(int,xyz),int(j)) for xyz,j in zip(fixed_local,joints))+'};')
    divisor_scale=scale*LOCAL_DIVISOR
    lines.append(f'const int32_t mario_skel_scale={round(divisor_scale*4096)};')
    lines.append('const int32_t mario_skel_offset[3]={'+','.join(str(round(-c/LOCAL_DIVISOR*4096)) for c in center)+'};')
    (output/'mario_skeleton.c').write_text('\n'.join(lines)+'\n')
    (output/'skeleton-info.json').write_text(json.dumps(dict(
        format='local joint channels per tick; joint 25 is TransN/XRotN prefix',
        local_divisor=LOCAL_DIVISOR,translate_scale=TRANSLATE_SCALE,turn=TURN,
        bytes=total,max_world_error=worst,clips=report),indent=2))
    print(f'Bone-pose bank: {total} bytes, max error {worst:.3f} world units')
