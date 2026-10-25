"""Offline QA: contact sheets of a converted fighter's clips.

Decodes build/fighters/<name>/<name>_model.c exactly as the DS does (base row
+ varying channels, joint plan validated by test_fighter_plan.c) and draws the
triangles with flat vertex colors, side view. Not a texture/lighting preview.

    python tools/render_fighter.py link build/fighters/link/qa Attack11,AttackS4
"""
import json
from pathlib import Path
import re
import struct
import sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mario_skeleton import local_matrix
from convert_mario_model import Model
from convert_fighter import FIGHTERS

ROOT=Path(__file__).resolve().parents[1]

def load(name):
    folder=ROOT/'build/fighters'/name;text=(folder/f'{name}_model.c').read_text()
    info=json.loads((folder/f'{name}-info.json').read_text());cfg=FIGHTERS[name]
    parents=list(map(int,re.search(r'parents\[\]=\{([^}]*)\}',text)[1].split(',')))
    rom=(ROOT/'build/rom/baserom.us.z64').read_bytes()
    word=struct.unpack_from('>I4H',rom,0x1ac870+cfg['model']*12)[1]
    model=Model((folder/'rom'/f'{cfg["model"]:04d}.bin').read_bytes(),word,cfg['tree'],cfg['matanim'],cfg['parts'])
    model.convert()
    return text,info,parents,model

def rows(text,index):
    base=np.array(list(map(int,re.search(rf'base_{index}\[\d+\]=\{{([^}}]*)\}}',text)[1].split(','))))
    found=re.search(rf'channels_{index}\[\]=\{{([^}}]*)\}}',text)
    if not found:return base[None]
    channels=list(map(int,found[1].split(',')))
    frames=np.array(list(map(int,re.search(rf'frames_{index}\[\]=\{{([^}}]*)\}}',text)[1].split(',')))).reshape(-1,len(channels))
    out=np.repeat(base[None],len(frames),0);out[:,channels]=frames
    return out

def world(row,parents):
    row=row.reshape(-1,9);prefix=local_matrix(row[len(parents)]);out=[]
    for joint,parent in enumerate(parents):out.append((out[parent] if parent>=0 else prefix)@local_matrix(row[joint]))
    return out

def main():
    name,out=sys.argv[1],Path(sys.argv[2]);wanted=sys.argv[3].split(',') if len(sys.argv)>3 else None
    out.mkdir(parents=True,exist_ok=True)
    text,info,parents,model=load(name)
    for clip in info['clips']:
        if wanted and clip['motion'] not in wanted:continue
        bank=rows(text,clip.get('index',clip.get('same')))
        ticks=sorted(set(np.linspace(0,len(bank)-1,min(8,len(bank))).astype(int)))
        fig,axes=plt.subplots(1,len(ticks),figsize=(2.6*len(ticks),3.2),squeeze=False)
        for ax,tick in zip(axes[0],ticks):
            w=world(bank[tick],parents);polys=[]
            for _,triangle in model.triangles:
                pts=np.array([(w[v[3][0]]@np.array([*v[3][1],1.0]))[:3] for v in triangle])
                polys.append((pts[:,0].mean(),pts,np.array(triangle[0][1])/255))
            for _,pts,color in sorted(polys,key=lambda p:p[0]):  # painter's order seen from +X
                ax.fill(pts[:,2],pts[:,1],color=color,edgecolor='k',linewidth=.15)
            ax.set_aspect('equal');ax.set_title(str(tick),fontsize=7);ax.axis('off')
        fig.suptitle(f"{name} {clip['motion']} ({len(bank)} tics)",fontsize=9)
        fig.savefig(out/f"{clip['motion']}.png",dpi=55,bbox_inches='tight');plt.close(fig)

if __name__=='__main__':main()
