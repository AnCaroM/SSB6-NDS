"""Generic fighter converter: Mario output must equal the legacy bone bank,
and the motion table must resolve the hand-picked Mario resource IDs."""
import json
import re
import sys
from pathlib import Path
import unittest
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))

def banks(path,base,channels,frames):
    text=path.read_text();out={}
    for m in re.finditer(base+r'_(\d+)\[\d+\]=\{([^}]*)\}',text):
        i=int(m[1]);row=np.array(list(map(int,m[2].split(','))))
        c=re.search(channels+rf'_{i}\[\]=\{{([^}}]*)\}}',text)
        if not c:out[i]=row[None];continue
        idx=list(map(int,c[1].split(',')))
        values=np.array(list(map(int,re.search(frames+rf'_{i}\[\]=\{{([^}}]*)\}}',text)[1].split(',')))).reshape(-1,len(idx))
        rows=np.repeat(row[None],len(values),axis=0);rows[:,idx]=values;out[i]=rows
    return out

class FighterTests(unittest.TestCase):
    def test_motion_table_matches_mario_resources(self):
        if not (ROOT/'upstream/ssb-decomp-re/src/ft/ftdata.c').exists():self.skipTest('reference checkout missing')
        import fighter_motions as fm
        m=fm.motions('Mario','dFTMarioMotionDescs')
        for name,fid in {'Wait':499,'WalkMiddle':501,'Attack11':606,'Attack13':608,'CliffWait':573,
                         'SpecialAirHi':637,'CapturePulled':565,'DamageFall':540,'AttackAirN':622}.items():
            self.assertEqual(m[name]['file'],fid,name)
        link=fm.motions('Link','dFTLinkMotionDescs')
        self.assertEqual(link['Attack11']['script'],'dLinkMainMotion_Jab1')
        self.assertEqual(link['SpecialLw']['script'],'dLinkMainMotion_Bomb')

    def test_generic_mario_equals_legacy_bank(self):
        new_info=ROOT/'build/fighters/mario/mario-info.json';old_info=ROOT/'build/model/skeleton-info.json'
        if not new_info.exists() or not old_info.exists():self.skipTest('fighters not converted (needs user ROM)')
        old=banks(ROOT/'build/model/mario_skeleton.c','skel_base','skel_channels','skel_frames')
        new=banks(ROOT/'build/fighters/mario/mario_model.c','base','channels','frames')
        ids={c['id']:i for i,c in enumerate(json.loads(old_info.read_text())['clips'])}
        compared=0
        for clip in json.loads(new_info.read_text())['clips']:
            if 'same' in clip or clip['file'] not in ids:continue
            np.testing.assert_array_equal(new[clip['index']],old[ids[clip['file']]],clip['motion']);compared+=1
        self.assertGreater(compared,50)
if __name__=='__main__':unittest.main()
