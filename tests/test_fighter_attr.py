"""src/fighter_attr.c must copy the US FTAttributes of the pinned reference."""
import re
import sys
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
FIELDS={'walk_speed_mul':'walk_speed_mul','traction':'traction','run_speed':'run_speed','jump_vel_x':'jump_vel_x',
        'jump_height_mul':'jump_height_mul','jump_height_base':'jump_height_base','jumpaerial_vel_x':'jumpaerial_vel_x',
        'jumpaerial_height':'jumpaerial_height','air_accel':'air_accel','air_speed_max_x':'air_speed_max_x',
        'air_friction':'air_friction','gravity':'gravity','tvel_base':'tvel_base','weight':'weight',
        'kneebend_ticks':'kneebend_anim_length'}

def ours(name):
    body=re.search(r'fighter_attr_'+name+r'=\{(.*?)\};',(ROOT/'src/fighter_attr.c').read_text(),re.S)[1]
    body=re.sub(r'/\*.*?\*/','',body,flags=re.S)
    return {k:float(v.rstrip('f')) for k,v in re.findall(r'\.(\w+)=([-\d.]+f?)',body)}

class AttributeTests(unittest.TestCase):
    def test_matches_reference(self):
        path=ROOT/'upstream/ssb-decomp-re/src/relocData'
        if not path.exists():self.skipTest('reference checkout missing')
        from mario_moves import us_source
        for name,file in (('mario','203_MarioMain.c'),('link','225_LinkMain.c')):
            text=us_source((path/file).read_text())
            body=re.search(r'FTAttributes d\w+Main_attr = \{(.*?)\n\};',text,re.S)[1]
            ref={k:float(v.rstrip('f')) for v,k in re.findall(r'^\s*([-\d.]+)f?,\s*/\*\s*(\w+)\s*\*/',body,re.M)}
            mine=ours(name)
            for field,source in FIELDS.items():self.assertAlmostEqual(mine[field],ref[source],6,f'{name} {field}')
if __name__=='__main__':unittest.main()
