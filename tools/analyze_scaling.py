"""Offline size/error experiments only; does not alter approved game assets."""
import json
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
def main():
    clips=json.loads((ROOT/'build/model/animation-info.json').read_text())
    mesh=json.loads((ROOT/'build/model/conversion-info.json').read_text())
    banks=np.load(ROOT/'build/model/animation-audit.npz')
    raw=0;unique=set();sample_count=0;quant_bytes=0;max_error=0;rows=[]
    for c in clips:
        bank=banks[str(c['id'])].astype(np.int32);raw+=bank.nbytes//2
        sample_count+=len(bank)
        for pose in bank:unique.add(pose.astype(np.int16).tobytes())
        # Per-clip axis bounds, 8-bit quantization, fixed point integer decode.
        low=bank.min(axis=(0,1));span=bank.max(axis=(0,1))-low
        span=np.maximum(span,1)
        quant=np.rint((bank-low)*255/span).astype(np.int32)
        restored=low+(quant*span+127)//255
        error=float(np.abs(bank-restored).max())/4096/mesh['normalization_scale']
        max_error=max(max_error,error);quant_bytes+=bank.size+12
        rows.append({'id':c['id'],'name':c['name'],'max_axis_error_world':error})
    result={'scope':'Offline estimates only, no format or visual changes applied',
            'raw_pose_bytes':raw,'samples':sample_count,
            'lossless_global_dictionary_bytes':len(unique)*189*6+sample_count*2,
            'unique_samples':len(unique),'experimental_8bit_pose_bytes':quant_bytes,
            'experimental_8bit_max_axis_error_world':max_error,
            'four_fighter_geometry':[],'per_clip_quantization':rows}
    for triangles in [320,250,240,220]:
        result['four_fighter_geometry'].append({'triangles_per_fighter':triangles,
             'polygons_with_reserve':4*triangles+175+1+120+16,
             'vertices_with_reserve':4*triangles*3+525+4+400+16*4})
    folder=ROOT/'build/optimization';folder.mkdir(exist_ok=True)
    (folder/'scaling.json').write_text(json.dumps(result,indent=2))
    print(json.dumps({k:v for k,v in result.items() if k!='per_clip_quantization'},indent=2))
if __name__=='__main__':main()
