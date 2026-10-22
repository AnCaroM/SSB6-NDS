"""Offline contact sheets of every converted clip; not emulator evidence."""
import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

ROOT=Path(__file__).resolve().parents[1]
def main():
    folder=ROOT/'build/model'
    clips=json.loads((folder/'animation-info.json').read_text())
    poses=np.load(folder/'animation-audit.npz')
    mapping=np.array(json.loads((folder/'pose-map.json').read_text())['mapping'])
    mesh=json.loads((folder/'conversion-info.json').read_text())
    colors=np.array([[float(x) for x in line.split()[4:7]] for line in
                    (folder/'mario-bind-pose.obj').read_text().splitlines() if line.startswith('v ')])
    colors=(colors.reshape(-1,3,3).mean(axis=1)*255).astype(int)
    out=ROOT/'build/animation-review';out.mkdir(exist_ok=True)
    rows=[]
    for page in range((len(clips)+7)//8):
        image=Image.new('RGB',(1100,8*180),(232,237,243));draw=ImageDraw.Draw(image)
        for row,c in enumerate(clips[page*8:page*8+8]):
            bank=poses[str(c['id'])].astype(float)/4096/mesh['normalization_scale']+mesh['normalization_center']
            assert np.isfinite(bank).all() and bank.shape[1]==189
            rows.append({'id':c['id'],'name':c['name'],'samples':len(bank),'ticks':c['ticks'],
                         'max_adjacent_vertex_distance':float(np.linalg.norm(np.diff(bank,axis=0),axis=2).max()) if len(bank)>1 else 0})
            draw.text((8,row*180+4),f"{c['id']} {c['name']} ({c['ticks']} ticks)",fill='black')
            for col,index in enumerate(np.linspace(0,len(bank)-1,5).astype(int)):
                p=bank[index][mapping].reshape(-1,3,3)
                # Fixed side projection and scale; root travel remains visible.
                for face in np.argsort(p[:,:,0].mean(axis=1)):
                    xy=[(col*220+75+v[2]*.21,row*180+150-v[1]*.21) for v in p[face]]
                    draw.polygon(xy,fill=tuple(colors[face]),outline=(75,75,85))
                draw.text((col*220+10,row*180+162),f't={c["sample_ticks"][index]}',fill='black')
        image.save(out/f'clips-{page+1}.png')
    (out/'audit.json').write_text(json.dumps({'scope':'All converted samples, offline mesh projection; no hardware validation','clips':rows},indent=2))
    print(f'{len(rows)} clips checked; contact sheets: {out}')
if __name__=='__main__':main()
