"""Checks against the original resource's independent collision metadata."""
import json
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]


class DreamLandTests(unittest.TestCase):
    def test_original_collision_matches_runtime(self):
        path=ROOT/'build/dreamland/conversion-info.json'
        if not path.exists():self.skipTest('Own ROM conversion required')
        info=json.loads(path.read_text())
        source=(ROOT/'src/stage.c').read_text()
        block=re.search(r'dreamland\[\]\s*=\s*\{(.*?)\};',source,re.S)[1]
        actual=[[int(n) for n in triplet] for triplet in re.findall(r'\{\s*(-?\d+),\s*(-?\d+),\s*(-?\d+)\s*\}',block)]
        self.assertEqual(actual,info['platforms'])

    def test_stage_geometry_and_texture_limits(self):
        path=ROOT/'build/dreamland/conversion-info.json'
        if not path.exists():self.skipTest('Own ROM conversion required')
        info=json.loads(path.read_text())
        self.assertEqual(info['triangles'],sum(info['layers'].values()))
        self.assertLess(640+info['triangles']+1,1400)
        self.assertLess(1920+info['vertices']+4,4000)
        self.assertLess(8192+info['texture_bytes']+info['wallpaper_bytes'],262144)
        self.assertEqual(len(info['resource_sha256']),5)

    def test_original_camera_bounds_match_runtime(self):
        path=ROOT/'build/dreamland/conversion-info.json'
        if not path.exists():self.skipTest('Own ROM conversion required')
        info=json.loads(path.read_text())
        self.assertEqual(info['camera_bounds'],[4000,-2000,3900,-3900])
        source=(ROOT/'src/camera.c').read_text()
        self.assertIn('-3900*Q,3900*Q',source)
        self.assertIn('-2000*Q,4000*Q',source)

    def test_solid_contour_matches_rom(self):
        path=ROOT/'build/dreamland/conversion-info.json'
        if not path.exists():self.skipTest('Own ROM conversion required')
        info=json.loads(path.read_text())
        block=re.search(r'island\[\]\s*=\s*\{(.*?)\};',(ROOT/'src/stage.c').read_text(),re.S)[1]
        actual=[[int(x),int(y)] for x,y in re.findall(r'\{\s*(-?\d+),\s*(-?\d+)\s*\}',block)]
        self.assertEqual(actual,info['solid_contour'])
        self.assertTrue(info['ledge_ground_flags'] & 0x8000)


if __name__=='__main__':unittest.main()
