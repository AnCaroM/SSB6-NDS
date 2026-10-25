"""Bone-pose channel quantization (ROM-free) and generated bank bounds."""
import json
import math
import sys
from pathlib import Path
import unittest
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from mario_skeleton import quantize,local_matrix,_float_matrix,TURN

class SkeletonTests(unittest.TestCase):
    def test_quantized_matrix_matches_float(self):
        rng=np.random.default_rng(7)
        for _ in range(200):
            v=[*rng.uniform(-900,900,3),*rng.uniform(-7,7,3),*rng.uniform(.5,1.5,3)]
            q=quantize(v)
            self.assertTrue(-TURN//2<=q[3]<TURN//2)
            diff=np.abs(local_matrix(q)-_float_matrix(v))
            self.assertLess(diff[:3,:3].max(),2e-3);self.assertLessEqual(diff[:3,3].max(),1/16)

    def test_angle_wrap(self):
        self.assertEqual(int(quantize([0,0,0,math.pi,0,0,1,1,1])[3]),-TURN//2)
        self.assertEqual(int(quantize([0,0,0,-2*math.pi+.001,0,0,1,1,1])[3]),5)

    def test_generated_bank(self):
        info=ROOT/'build/model/skeleton-info.json'
        if not info.exists():self.skipTest('model not converted (needs user ROM)')
        data=json.loads(info.read_text())
        self.assertLessEqual(data['max_world_error'],.5)
        self.assertLess(data['bytes'],400*1024)
        for clip in data['clips']:self.assertLessEqual(clip['varying'],26*9)
if __name__=='__main__':unittest.main()
