"""Offline QA rasterizer: a converted fighter as the DS would draw it.

Uses the generated model data only (local vertices, joint channels, vertex
colors, texture pixels/params, uv) with a z-buffer at 256x192, orthographic
side view, DS texture repeat/flip rules, color modulation and 1-bit alpha.
No antialiasing/fog/edge marking; not a substitute for the emulator.

    python tools/raster_fighter.py link out.png Attack11:0,4,8 AttackS4:20
"""
from pathlib import Path
import re
import sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import json
from render_fighter import rows, world

ROOT=Path(__file__).resolve().parents[1]

def parse(text,name,group):
    body=re.search(name+r'\[\]=\{(.*?)\};',text,re.S)[1]
    return np.array(list(map(int,re.findall(r'-?\d+',body)))).reshape(-1,group)

def textures(text):
    out=[]
    for i,(w,h,param) in enumerate(re.findall(r'\{texture_\d+,(\d+),(\d+),(\d+)\}',text)):
        pix=np.array(list(map(int,re.search(rf'texture_{i}\[\][^=]*=\{{([^}}]*)\}}',text)[1].split(','))),dtype=np.uint32)
        rgb=np.stack([(pix&31),(pix>>5)&31,(pix>>10)&31],-1)/31
        out.append((int(w),int(h),int(param),rgb.reshape(int(h),int(w),3),(pix>>15).reshape(int(h),int(w))))
    return out

def address(c,size,repeat,flip):
    c=np.floor(c).astype(int)
    if not repeat:return np.clip(c,0,size-1)
    if flip:
        c%=size*2;return np.where(c<size,c,size*2-1-c)
    return c%size

def draw(name,clip_ticks,path):
    folder=ROOT/'build/fighters'/name;text=(folder/f'{name}_model.c').read_text()
    info=json.loads((folder/f'{name}-info.json').read_text());clips={c['motion']:c for c in info['clips']}
    parents=list(map(int,re.search(r'parents\[\]=\{([^}]*)\}',text)[1].split(',')))
    colors=parse(text,'static const FighterVertex vertices',3)
    local=parse(text,'static const FighterLocalVertex locals',4);lmap=parse(text,'static const uint16_t local_map',1)[:,0]
    tex_of=parse(text,'static const int16_t triangle_textures',1)[:,0];texs=textures(text)
    part_of=parse(text,'static const uint8_t triangle_parts',1)[:,0]
    plan=parse(text,'static const FighterJoint joints',4);joint_of_slot={int(st):j for j,(_,st,_,_) in enumerate(plan) if st>=0}
    joint_of=[joint_of_slot[local[lmap[i],3]] for i in range(len(lmap))]
    divisor=512//int(re.search(r'const FighterModel \w+=\{.*,(\d+)\};',text)[1])
    base_mask=int(re.search(r'triangle_parts,(\d+)u',text)[1])
    panels=[(m,int(t)) for spec in clip_ticks for m,ts in [spec.split(':')] for t in ts.split(',')]
    fig,axes=plt.subplots(1,len(panels),figsize=(3.2*len(panels),3.4),squeeze=False)
    for ax,(motion,tick) in zip(axes[0],panels):
        c=clips[motion];index=c.get('index',c.get('same'));bank=rows(text,index)
        source=next(x for x in info['clips'] if x.get('index')==index)
        row=bank[min(tick,len(bank)-1)].copy().reshape(-1,9)
        if source['root']==2:row[len(parents)]=[0,0,0,0,0,0,4096,4096,4096] if not ROOT_MOTION else row[len(parents)]
        w=world(row.flatten(),parents)
        mask=base_mask;found=re.search(rf'parts_{index}\[\]=\{{([^}}]*)\}}',text)
        if found:
            pairs=list(map(int,found[1].split(',')))
            for t,m in zip(pairs[0::2],pairs[1::2]):
                if t<=tick:mask=m
        pts=np.array([(w[j]@np.array([*(local[lmap[i],:3]*divisor/4096),1.0]))[:3] for i,j in enumerate(joint_of)])
        W,H=256,192;scale=H/560;img=np.ones((H,W,3))*[.45,.65,.85];z=np.full((H,W),-1e9)
        # Facing right on DS: model +Z -> world +X, camera looks down -Z, so nearer = smaller model X.
        sx=W/2+pts[:,2]*scale;sy=H-20-pts[:,1]*scale;depth=-pts[:,0]
        for k in range(0,len(pts),3):
            if part_of[k//3]!=255 and not mask>>part_of[k//3]&1:continue
            xs,ys,zs=sx[k:k+3],sy[k:k+3],depth[k:k+3]
            x0,x1=int(max(0,np.floor(xs.min()))),int(min(W-1,np.ceil(xs.max())));y0,y1=int(max(0,np.floor(ys.min()))),int(min(H-1,np.ceil(ys.max())))
            if x0>x1 or y0>y1:continue
            gx,gy=np.meshgrid(np.arange(x0,x1+1)+.5,np.arange(y0,y1+1)+.5)
            d=(ys[1]-ys[2])*(xs[0]-xs[2])+(xs[2]-xs[1])*(ys[0]-ys[2])
            if abs(d)<1e-9:continue
            a=((ys[1]-ys[2])*(gx-xs[2])+(xs[2]-xs[1])*(gy-ys[2]))/d;b=((ys[2]-ys[0])*(gx-xs[2])+(xs[0]-xs[2])*(gy-ys[2]))/d;g=1-a-b
            inside=(a>=0)&(b>=0)&(g>=0)
            zz=a*zs[0]+b*zs[1]+g*zs[2];yy,xx=gy.astype(int),gx.astype(int)
            col=np.stack([(colors[k+i,0]>>s&31)/31 for i in range(3) for s in (0,5,10)]).reshape(3,3)
            shade=a[...,None]*col[0]+b[...,None]*col[1]+g[...,None]*col[2]
            t=tex_of[k//3]
            if t>=0:
                tw,th,param,rgb,alpha=texs[t]
                u=(a*colors[k,1]+b*colors[k+1,1]+g*colors[k+2,1])/16;v=(a*colors[k,2]+b*colors[k+1,2]+g*colors[k+2,2])/16
                uu=address(u,tw,param>>16&1,param>>18&1);vv=address(v,th,param>>17&1,param>>19&1)
                shade=shade*rgb[vv,uu];inside&=alpha[vv,uu]==1
            write=inside&(zz>z[yy,xx])
            img[yy[write],xx[write]]=shade[write];z[yy[write],xx[write]]=zz[write]
        ax.imshow(img,interpolation='nearest');ax.set_title(f'{motion} {tick}',fontsize=8);ax.axis('off')
    fig.savefig(path,dpi=80,bbox_inches='tight');plt.close(fig)

ROOT_MOTION=True  # viewer-style: show movement-only TransN
if __name__=='__main__':draw(sys.argv[1],sys.argv[3:],Path(sys.argv[2]))
