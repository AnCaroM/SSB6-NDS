import json
import struct
import sys
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from mario_moves import SPECS,scripts,timeline,frames

class MoveTests(unittest.TestCase):
    def test_attack_fields_exist_in_original_binary(self):
        path=ROOT/'build/assets/us/0202_MarioMainMotion.bin'
        if not path.exists():self.skipTest('Own ROM required')
        data=path.read_bytes();table=scripts(ROOT);count=0
        for _,_,name in SPECS:
            for _,op,v in timeline(table,name):
                if op!='ftMotionCommandMakeAttackColl':continue
                a,g,j,d,r,e,sz,x,y,z,ang,scale,weight,ga,sd,level,kind,base=v
                # FTMotionEventMakeAttackColl=3, each field packed independently
                # according to ftdef.h. All five words must occur in ROM data.
                words=[3<<26|a<<23|g<<20|j<<13|d<<5|r<<4|e,
                       (sz&65535)<<16|(x&65535),(y&65535)<<16|(z&65535),
                       (ang&1023)<<22|scale<<12|weight<<2|ga,
                       sd<<24|level<<21|kind<<17|base<<7]
                self.assertIn(struct.pack('>5I',*words),data,op+' '+str(v));count+=1
        self.assertGreater(count,100)
        for row in ((52,12,45,70,0,80,0),(55,16,45,70,0,70,0)):
            self.assertIn(struct.pack('>7i',*row),data)

    def test_timing_late_hits_multihits_and_fireball(self):
        table=scripts(ROOT)
        jab=frames(table,'Jab1',19)
        self.assertEqual([i for i,f in enumerate(jab) if f['hits']],[2,3])
        nair=frames(table,'AttackAirN',51)
        self.assertEqual(nair[3]['hits'][0]['v'][3],14)
        self.assertEqual(nair[11]['hits'][0]['v'][3],11)
        self.assertEqual(nair[3]['hits'][0]['strike'],nair[11]['hits'][0]['strike'])
        dair=frames(table,'AttackAirD',40)
        self.assertEqual(len({h['strike'] for f in dair for h in f['hits']}),8)
        for name in ('FireballGround','FireballAir'):
            fire=frames(table,name,47)
            self.assertFalse(fire[15]['flags']&1);self.assertTrue(fire[16]['flags']&1)

    def test_original_fireball_parameters(self):
        path=next((ROOT/'build/assets/us').glob('0204*bin'),None)
        if path is None:self.skipTest('Own ROM required')
        w0,w1,_,w3=struct.unpack_from('>4I',path.read_bytes(),36)
        self.assertEqual((w0>>16,(w0>>6)&1023,w1>>22,(w1>>14)&255,w1&1023,w3>>22),
                         (200,361,25,7,0,10))

    def test_compact_pose_map_preserves_every_render_vertex(self):
        path=ROOT/'build/model/pose-map.json'
        if not path.exists():self.skipTest('Converted model required')
        from convert_mario_model import Model
        model=Model((ROOT/'build/assets/us/0296_MarioModel.bin').read_bytes());model.convert()
        expected=[(v[3][0],list(v[3][1])) for _,tri in model.triangles for v in tri]
        info=json.loads(path.read_text());sources=info['sources']
        actual=[(sources[i]['joint'],sources[i]['position']) for i in info['mapping']]
        self.assertEqual(actual,expected);self.assertEqual(len(actual),960)
        self.assertEqual(len(sources),189)

    def test_every_move_has_clip_and_bounded_pose_memory(self):
        path=ROOT/'build/model/moves-info.json'
        if not path.exists():self.skipTest('Converted model required')
        moves=json.loads(path.read_text());clips=json.loads((ROOT/'build/model/animation-info.json').read_text())
        self.assertEqual([m['move'] for m in moves],[s[0] for s in SPECS])
        for m in moves:self.assertEqual(clips[m['clip']]['id'],m['resource'])
        # 67 shared clips, including 17 damage reactions. Compact bank is the
        # hardware target; keep its measured allocation below 800 KiB.
        self.assertLess(sum(c['compact_pose_bytes'] for c in clips),800*1024)

if __name__=='__main__':unittest.main()
