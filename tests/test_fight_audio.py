"""Bank bounds and deterministic synthesis; playback still needs real DS."""
import sys
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from generate_fight_audio import samples,NAMES,RATE

class FightAudioTests(unittest.TestCase):
    def test_bounded_and_reproducible_bank(self):
        total=0
        self.assertEqual(len(NAMES),7)
        for kind in range(len(NAMES)):
            data=samples(kind)
            self.assertEqual(data,samples(kind))
            self.assertEqual(len(data)%4,0)
            self.assertEqual(data[0],0);self.assertEqual(data[-1],0)
            self.assertGreater(max(data),10);self.assertLess(min(data),-10)
            self.assertLessEqual(max(map(abs,data)),110)
            self.assertLess(abs(sum(data)/len(data)),2)
            self.assertLessEqual(len(data),RATE)
            total+=len(data)
        self.assertEqual(total,27360)
        self.assertLess(total,65536)
if __name__=='__main__':unittest.main()
