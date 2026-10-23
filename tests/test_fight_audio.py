"""Sound list/enum agreement and offline rendering; ROM bank checked if built."""
import json
import re
import sys
from pathlib import Path
import unittest
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from fight_sounds import SOUNDS,HIT_KINDS,NONE,SMASH_RANDOM,hit_sound,INDEX

class FightAudioTests(unittest.TestCase):
    def test_enum_matches_python_list(self):
        text=(ROOT/'src/fight_sound.h').read_text()
        body=re.search(r'typedef enum \{(.*?)\} FightSound;',text,re.S)[1]
        names=[n.strip() for n in re.sub(r'/\*.*?\*/','',body,flags=re.S).split(',') if n.strip()]
        self.assertEqual(names[:len(SOUNDS)],['SOUND_'+k for k,*_ in SOUNDS])
        self.assertEqual(names[len(SOUNDS)],'SOUND_COUNT')
        self.assertIn(f'SOUND_SMASH_RANDOM={SMASH_RANDOM}',names)
        self.assertIn(f'SOUND_NONE={NONE}',names)
        self.assertLess(len(SOUNDS),SMASH_RANDOM)

    def test_hit_table(self):
        # dFTMainHitCollisionFGMs: punch/kick/coin/burn rows, levels S/M/L.
        self.assertEqual(hit_sound(0,0),INDEX['PUNCH_S']);self.assertEqual(hit_sound(1,2),INDEX['KICK_L'])
        self.assertEqual(hit_sound(2,1),INDEX['COIN']);self.assertEqual(hit_sound(3,1),INDEX['BURN_M'])
        self.assertEqual(hit_sound(9,0),NONE)
        for row in HIT_KINDS.values():
            for key in row:self.assertIn(key,INDEX)

    def test_render_pitch_and_padding(self):
        try:import extract_fight_audio as e
        except Exception as error:self.skipTest(f'reference tools unavailable: {error}')
        tone=np.sin(np.arange(44100)/44100*2*np.pi*441).astype(np.float32)
        base=e.render(lambda _:tone,[(0,0,255)],2.0)
        octave=e.render(lambda _:tone,[(0,1200,255)],2.0)
        self.assertEqual(len(base)%4,0);self.assertEqual(len(octave)%4,0)
        self.assertAlmostEqual(len(octave)/len(base),.5,delta=.01)
        self.assertLessEqual(int(np.abs(base.astype(int)).max()),127)
        self.assertEqual(len(e.render(lambda _:tone,[(0,0,255)],.25)),4000)

    def test_generated_bank(self):
        info=ROOT/'build/audio/audio-info.json'
        if not info.exists():self.skipTest('audio not converted (needs user ROM)')
        data=json.loads(info.read_text())
        self.assertEqual([s['sound'] for s in data['samples']],[k for k,*_ in SOUNDS])
        for sample,(_,_,priority,limit) in zip(data['samples'],SOUNDS):
            self.assertEqual(sample['bytes']%4,0)
            self.assertLessEqual(sample['bytes'],limit*data['rate']+4)
            self.assertLessEqual(sample['volume'],127);self.assertEqual(sample['priority'],priority)
            self.assertTrue(sample['layers'])
        self.assertLessEqual(data['bytes'],262144)
        self.assertIn('not distributable',data['provenance'])
if __name__=='__main__':unittest.main()
