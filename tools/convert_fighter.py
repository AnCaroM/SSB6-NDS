"""Convert a fighter (model, textures, bone-pose clips) for the generic DS renderer.

Table driven: the motion list comes from the fighter's FTMotionDesc table
(fighter_motions.py), hidden-joint insertion from its FTANIM flags, and the
model from its FTCommonPartContainer hi-detail JointTree. Mario's legacy
pipeline (convert_mario_model.py) is kept for the existing 1v1; running this
tool with --fighter mario checks that both pipelines agree.

Output (build/fighters/<name>/): <name>_model.c with a FighterModel, a JSON
report and an OBJ of the bind pose. Generated from the user ROM; never publish.
"""
import argparse
import hashlib
import json
import math
import re
from pathlib import Path
import struct
import subprocess
import sys
import numpy as np
from prepare_rom import EXPECTED_SHA1
from convert_mario_model import Model
from mario_animation import decode_script, evaluate
from mario_skeleton import quantize, local_matrix, _float_matrix, TRANSLATE_SCALE
import fighter_motions as fm

ROOT=Path(__file__).resolve().parents[1]
FILE_TABLE=0x1ac870;FILE_BASE=0x1b2c6c
LOCAL_DIVISOR=64
WORLD_UNIT=1024  # versus_main.c WORLD_UNIT: model unit -> 1/1024 GL unit
FIRST_SLOT=1     # slot 0 holds the caller's pushed matrix
MAX_ERROR=2.1
# Ledge phases are drawn relative to the ledge with their authored TransN
# (convert_mario_model/mario_animation: 572, 573, 580, 582 for Mario).
LEDGE_PREFIX={'CliffCatch','CliffWait','CliffAttackQuick1','CliffAttackSlow1'}
SINGLE_FRAME={'CliffWait'}  # one hanging pose, as in the Mario 1v1
COMMON_CLIPS=['Wait','WalkMiddle','Dash','Run','RunBrake','KneeBend','JumpF','JumpB','JumpAerialF',
    'JumpAerialB','Fall','LandingLight','LandingHeavy','DamageHi1','DamageHi2','DamageHi3','DamageN1',
    'DamageN2','DamageN3','DamageLw1','DamageLw2','DamageLw3','DamageAir1','DamageAir2','DamageAir3',
    'DamageFlyHi','DamageFlyN','DamageFlyLw','DamageFlyTop','DamageFlyRoll','DamageFall','FallSpecial',
    'CliffCatch','CliffWait','CliffAttackQuick1','CliffAttackQuick2','CliffAttackSlow1','CliffAttackSlow2',
    'DownAttackD','DownAttackU','Catch','CatchPull','ThrowF','ThrowB','CapturePulled','Attack11','Attack12',
    'AttackDash','AttackS3','AttackHi3','AttackLw3','AttackS4','AttackHi4','AttackLw4','AttackAirN',
    'AttackAirF','AttackAirB','AttackAirHi','AttackAirLw']
FIGHTERS={
    'mario':dict(Name='Mario',model=296,main='203_MarioMain.c',tree=0x2200,matanim=0x2680,table='dFTMarioMotionDescs',
                 special=['Attack13','SpecialN','SpecialAirN','SpecialHi','SpecialAirHi','SpecialLw','SpecialAirLw'],parts={}),
    # 324_LinkModel.c: dLinkModel_JointTree @0x3AE8; anim table +0x5B8 (225_LinkMain.c).
    'link':dict(Name='Link',model=324,main='225_LinkMain.c',tree=0x3AE8,matanim=0x3AE8+0x5B8,table='dFTLinkMotionDescs',
                special=['Attack13','Attack100Start','Attack100Loop','Attack100End','SpecialHi','SpecialHiEnd',
                         'SpecialAirHi','SpecialN','SpecialNGet','SpecialNEmpty','SpecialAirN','SpecialLw','SpecialAirLw'],
                parts={}),
}

def rom_file(rom,fid,decoder,scratch):
    flags,intern,_,_,decoded=struct.unpack_from('>I4H',rom,FILE_TABLE+fid*12)
    stop=struct.unpack_from('>I',rom,FILE_TABLE+(fid+1)*12)[0]&0x7fffffff
    start=FILE_BASE+(flags&0x7fffffff);end=FILE_BASE+stop
    if flags>>31:
        packed=scratch/f'{fid:04d}.vpk0';out=scratch/f'{fid:04d}.bin'
        packed.write_bytes(rom[start:end]);subprocess.run([str(decoder),str(packed),str(out)],check=True)
        data=out.read_bytes()
    else:data=rom[start:start+decoded*4]
    if len(data)!=decoded*4:raise ValueError(f'File {fid} length mismatch')
    return data,intern

def chain(data,word):
    pointers={}
    while word!=0xffff:
        if word*4 in pointers:raise ValueError('Cyclic relocations')
        raw=struct.unpack_from('>I',data,word*4)[0];pointers[word*4]=(raw&0xffff)*4;word=raw>>16
    return pointers

def hidden_parts(Name):
    """Root fighter joint of each FTHiddenPart; bit (31-k) of FTANIM flags creates part k."""
    text=fm.us_source((fm.UPSTREAM/f'src/relocData/{FIGHTERS[Name.lower()]["main"]}').read_text())
    body=re.search(r'FTHiddenPart d'+Name+r'Main_hiddenparts\[\d+\]\s*=\s*\{(.*?)\n\};',text,re.S)[1]
    return [int(row.split(',')[0],0) for row in re.findall(r'\{([^{}]*)\}',body)]

def animated_joints(flags,roots,count):
    """Animation tables list the DObjs that exist for this motion, in tree order
    (ftmain hidden parts 0..2 are XRotN/TransN/YRotN at the front; later parts
    are optional tree joints such as Mario's joint28, absent unless enabled)."""
    aux=[k for k in range(min(3,len(roots))) if flags&(1<<(31-k))]
    enabled={roots[k] for k in range(3,len(roots)) if flags&(1<<(31-k))}
    optional=set(roots[3:])
    return aux,[j for j in range(count) if j+4 not in optional or j+4 in enabled]

def sample(data,word,depths,defaults,flags,ledge,single,roots):
    """Local channels [frame][joint+prefix][9] for one clip (mario_animation semantics)."""
    pointers=chain(data,word)
    aux,existing=animated_joints(flags,roots,len(depths))
    if len(aux)>1:raise ValueError('More than one inserted joint is not supported')
    xrotn=bool(flags&fm.INSERT_XROTN)
    tables=[]
    for slot in range(len(aux)+len(existing)):
        ptr=pointers.get(slot*4)
        if ptr is None and struct.unpack_from('>I',data,slot*4)[0]:raise ValueError('Unresolved animation pointer')
        tables.append(decode_script(data,ptr) if ptr is not None else None)
    if not any(tables):raise ValueError('Empty animation')
    duration=max(s[1] for s in tables if s);looping=any(s[2] for s in tables if s)
    auxiliary=tables.pop(0) if aux else None
    scripts=[None]*len(depths)
    for joint,script in zip(existing,tables):scripts[joint]=script
    if not 1<=duration<=400:raise ValueError('Unexpected clip duration')
    ticks=duration if looping else duration+1
    fields={0:3,1:4,2:5,4:0,5:1,6:2,7:6,8:7,9:8}
    frames=[]
    for frame in range(1 if single else ticks):
        prefix=[0,0,0,0,0,0,1,1,1]
        if (xrotn or ledge) and auxiliary is not None:
            for track,field in fields.items():
                value=evaluate(auxiliary[0][track],min(frame,auxiliary[1]))
                if value is not None:prefix[field]=value
        rows=[]
        for joint,values in enumerate(defaults):
            values=list(values)
            if scripts[joint] is not None:
                tracks,end,_=scripts[joint]
                for track,field in fields.items():
                    value=evaluate(tracks[track],min(frame,end))
                    if value is not None:values[field]=value
            rows.append(values)
        frames.append(rows+[prefix])
    return frames,ticks,looping

def world(rows,parents,make):
    prefix=make(rows[len(parents)]);out=[]
    for joint,parent in enumerate(parents):out.append((out[parent] if parent>=0 else prefix)@make(rows[joint]))
    return out

def plan_joints(parents,has_geometry):
    needed=list(has_geometry)
    for joint in reversed(range(len(parents))):
        if needed[joint] and parents[joint]>=0:needed[parents[joint]]=True
    order=[j for j in range(len(parents)) if needed[j]]
    if [j for j in order if parents[j]<0]!=[order[0]]:raise ValueError('Expected a single root joint')
    after={a:b for a,b in zip(order,order[1:])}
    store=[has_geometry[j] or any(parents[c]==j and needed[c] and after.get(j)!=c for c in order) for j in range(len(parents))]
    # A child evaluated right after its parent continues from the current matrix.
    slots={};plan=[]
    for j in range(len(parents)):
        if store[j]:slots[j]=FIRST_SLOT+len(slots)
    if FIRST_SLOT+len(slots)>31:raise ValueError(f'{len(slots)} matrix slots exceed the DS stack')
    previous=None
    for j in range(len(parents)):
        if not needed[j]:plan.append((-1,-1,1));continue
        p=parents[j];restore=-1 if p==previous or (p<0 and previous is None) else slots[p]
        plan.append((restore,slots.get(j,-1),0));previous=j
    return plan,slots

def convert(name,check_mario=False):
    cfg=FIGHTERS[name];Name=cfg['Name']
    rom=(ROOT/'build/rom/baserom.us.z64').read_bytes()
    if hashlib.sha1(rom).hexdigest()!=EXPECTED_SHA1:raise ValueError('Expected verified US ROM')
    out=ROOT/'build/fighters'/name;out.mkdir(parents=True,exist_ok=True)
    scratch=out/'rom';scratch.mkdir(exist_ok=True)
    decoder=ROOT/'build/assets-tools'/('vpk_decode.exe' if sys.platform=='win32' else 'vpk_decode')
    data,word=rom_file(rom,cfg['model'],decoder,scratch)
    model=Model(data,word,cfg['tree'],cfg['matanim'],cfg['parts']);model.convert()
    depths=[j['depth'] for j in model.joints];parents=[];last={}
    for d in depths:parents.append(last[d-1] if d else -1);last[d]=len(parents)-1
    defaults=[list(struct.unpack_from('>9f',data,cfg['tree']+8+i*44)) for i in range(len(depths))]
    sources=[v[3] for _,tri in model.triangles for v in tri]
    unique={};local_map=[];locals_=[]
    for joint,xyz in sources:
        key=(joint,*xyz)
        if key not in unique:unique[key]=len(locals_);locals_.append((joint,xyz))
        local_map.append(unique[key])
    geometry=[any(j==k for j,_ in locals_) for k in range(len(depths))]
    plan,slots=plan_joints(parents,geometry)
    local=np.array([[*xyz,1.0] for _,xyz in locals_]);ljoint=np.array([j for j,_ in locals_])
    fixed=np.rint(local[:,:3]*4096/LOCAL_DIVISOR)
    if np.abs(fixed).max()>32767:raise ValueError('Local vertex outside v16')
    restored=np.c_[fixed*LOCAL_DIVISOR/4096,np.ones(len(local))]
    motions=fm.motions(Name,cfg['table']);roots=hidden_parts(Name)
    wanted=COMMON_CLIPS+cfg['special'];clips=[];seen={};worst=0.0;total=0
    lines=['/* Generated from the user ROM; do not distribute. */','#include <stddef.h>','#include "fighter_model.h"']
    for motion in wanted:
        info=motions[motion]
        if info['file'] is None:raise ValueError(f'{Name} {motion} has no animation')
        key=(info['file'],info['flags']&(fm.INSERT_TRANSN|fm.INSERT_XROTN|fm.CATCH_JOINT),motion in LEDGE_PREFIX,motion in SINGLE_FRAME)
        if key in seen:clips.append(dict(motion=motion,same=seen[key]));continue
        anim,aword=rom_file(rom,info['file'],decoder,scratch)
        frames,ticks,looping=sample(anim,aword,depths,defaults,info['flags'],motion in LEDGE_PREFIX,motion in SINGLE_FRAME,roots)
        values=np.array(frames,dtype=np.float64);q=quantize(values);flat=q.reshape(len(q),-1)
        varying=[c for c in range(flat.shape[1]) if (flat[:,c]!=flat[0,c]).any()]
        error=0.0
        for f in range(len(q)):
            exact=world(values[f],parents,_float_matrix);approx=world(q[f],parents,local_matrix)
            for j in range(len(parents)):
                sel=ljoint==j
                if sel.any():
                    error=max(error,float(np.abs((local[sel]@exact[j].T)[:,:3]-(restored[sel]@approx[j].T)[:,:3]).max()))
        if error>MAX_ERROR:raise ValueError(f'{Name} {motion}: skeleton error {error:.3f}')
        worst=max(worst,error);index=len([c for c in clips if 'same' not in c])
        lines.append(f'static const int16_t base_{index}[{flat.shape[1]}]={{'+','.join(map(str,flat[0]))+'};')
        if varying:
            lines.append(f'static const uint16_t channels_{index}[]={{'+','.join(map(str,varying))+'};')
            lines.append(f'static const int16_t frames_{index}[]={{'+','.join(map(str,flat[:,varying].flatten()))+'};')
        size=flat.shape[1]*2+len(varying)*(2+2*len(q));total+=size
        seen[key]=index
        clips.append(dict(motion=motion,index=index,file=info['file'],flags=info['flags'],ticks=ticks,stored=len(q),
                          loop=looping,varying=len(varying),bytes=size,max_world_error=error,
                          channels=q if check_mario else None))
    entries=[]
    for c in clips:
        src=next(x for x in clips if x.get('index')==(c['same'] if 'same' in c else c['index']) and 'same' not in x)
        i=src['index'];ref=lambda n:f'{n}_{i}' if src['varying'] else 'NULL'
        entries.append(f'{{{ref("frames")},{ref("channels")},base_{i},{src["ticks"]},{src["stored"]},{src["varying"]},{int(src["loop"])}}}, /* {c["motion"]} */')
    lines.append('static const FighterClip clips[]={');lines+=entries;lines.append('};')
    lines.append('static const char *const clip_names[]={'+','.join(f'"{c["motion"]}"' for c in clips)+'};')
    # Draw vertices: color/uv per expanded vertex; position via local_map.
    tri_vertices=[v for _,tri in model.triangles for v in tri]
    lines.append('static const FighterVertex vertices[]={'+','.join(
        '{%d,%d,%d}'%(((rgb[0]>>3)|((rgb[1]>>3)<<5)|((rgb[2]>>3)<<10)),round(uv[0]*16),round(uv[1]*16))
        for _,rgb,uv,_ in tri_vertices)+'};')
    lines.append('static const int16_t triangle_textures[]={'+','.join(map(str,model.triangle_textures))+'};')
    for i,t in enumerate(model.textures):
        lines.append(f'static const uint16_t texture_{i}[] __attribute__((aligned(4)))={{'+','.join(map(str,t['pixels']))+'};')
    lines.append('static const FighterTexture textures[]={'+','.join(
        f'{{texture_{i},{t["padded_width"]},{t["padded_height"]},{t["param"]}}}' for i,t in enumerate(model.textures))+'};')
    lines.append('static const uint16_t local_map[]={'+','.join(map(str,local_map))+'};')
    lines.append('static const FighterLocalVertex locals[]={'+','.join(
        '{%d,%d,%d,%d}'%(*map(int,xyz),slots[j]) for xyz,j in zip(fixed,ljoint))+'};')
    lines.append('static const FighterJoint joints[]={'+','.join('{%d,%d,%d,0}'%p for p in plan)+'};')
    lines.append('static const int8_t parents[]={'+','.join(map(str,parents))+'};')
    scale=round(LOCAL_DIVISOR/WORLD_UNIT*4096)
    lines.append(f'const FighterModel {name}_model={{"{Name}",vertices,{len(tri_vertices)},triangle_textures,'
                 f'textures,{len(model.textures)},local_map,locals,clips,{len(clips)},joints,parents,{len(depths)},'
                 f'{len(slots)},{scale},{{0,0,0}}}};')
    lines.append(f'const char *const *{name}_clip_names=clip_names;')
    (out/f'{name}_model.c').write_text('\n'.join(lines)+'\n')
    texture_bytes=sum(t['padded_width']*t['padded_height']*2 for t in model.textures)
    report=dict(fighter=Name,model_file=cfg['model'],joints=len(depths),slots=len(slots),triangles=len(model.triangles),
                draw_vertices=len(tri_vertices),local_vertices=len(locals_),textures=len(model.textures),
                texture_bytes=texture_bytes,pose_bytes=total,max_world_error=worst,
                clips=[{k:v for k,v in c.items() if k!='channels'} for c in clips])
    (out/f'{name}-info.json').write_text(json.dumps(report,indent=2))
    header=[f'#ifndef SSB_{name.upper()}_MODEL_H',f'#define SSB_{name.upper()}_MODEL_H','#include "fighter_model.h"',
            f'extern const FighterModel {name}_model;',f'extern const char *const *{name}_clip_names;']
    header+=[f'#define {name.upper()}_CLIP_{c["motion"].upper()} {i}' for i,c in enumerate(clips)]
    (out/f'{name}_model.h').write_text('\n'.join(header+['#endif'])+'\n')
    print(f'{Name}: {len(depths)} joints ({len(slots)} slots), {len(model.triangles)} triangles, '
          f'{texture_bytes} texture bytes, {len(clips)} clips, {total} pose bytes, max error {worst:.3f}')
    return report,clips

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--fighter',choices=sorted(FIGHTERS),required=True)
    args=parser.parse_args();convert(args.fighter)

if __name__=='__main__':main()
