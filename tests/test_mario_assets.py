"""Format/curve checks independent of the mesh viewer and DS renderer."""
import json
from pathlib import Path
import struct
import sys
import unittest
import csv

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from mario_animation import decode_script, evaluate


class FigatreeTests(unittest.TestCase):
    def test_compact_pose_error_and_exact_axis_endpoints(self):
        import numpy as np
        folder=ROOT/'build/model'
        if not (folder/'animation-audit.npz').exists():self.skipTest('Converted bank required')
        clips=json.loads((folder/'animation-info.json').read_text())
        mesh=json.loads((folder/'conversion-info.json').read_text())
        banks=np.load(folder/'animation-audit.npz')
        total=0
        for c in clips:
            source=banks[str(c['id'])].astype(np.int32)
            low=np.array(c['compact_low']);span=np.array(c['compact_span'])
            quant=np.rint((source-low)*255/span).astype(np.int32)
            restored=low+(quant*span+127)//255
            self.assertGreaterEqual(int(quant.min()),0);self.assertLessEqual(int(quant.max()),255)
            error=float(np.abs(source-restored).max())/4096/mesh['normalization_scale']
            self.assertLessEqual(error,2.1,c['name'])
            self.assertAlmostEqual(error,c['compact_max_axis_error_world'])
            total+=source.size+12
        self.assertLess(total,800*1024)  # 67 shared clips, including damage.

    def test_native_compact_decode_matches_offline_quantization(self):
        import numpy as np
        path=ROOT/'build/scene/poses-decoded.bin'
        if not path.exists():self.skipTest('Run verify_scene.py first')
        clips=json.loads((ROOT/'build/model/animation-info.json').read_text())
        banks=np.load(ROOT/'build/model/animation-audit.npz');expected=[]
        for c in clips:
            bank=banks[str(c['id'])].astype(np.int32)
            low=np.array(c['compact_low']);span=np.array(c['compact_span'])
            q=np.rint((bank-low)*255/span).astype(np.int32)
            expected.append((low+(q*span+127)//255).astype('<i2').tobytes())
        self.assertEqual(path.read_bytes(),b''.join(expected))
    def test_hidden_joint_layout_and_endpoints(self):
        import re
        path=ROOT/'build/model/animation-info.json'
        if not path.exists():self.skipTest('Converted model required')
        descriptions=(ROOT/'upstream/ssb-decomp-re/tools/relocFileDescriptions.us.txt').read_text()
        names={int(i):n for i,n in re.findall(r'-(\d+): FTMarioAnim(\w+)',descriptions)}
        source=(ROOT/'upstream/ssb-decomp-re/src/ft/ftdata.c').read_text().split('FTMotionDesc dFTMarioMotionDescs[] =')[1].split('};')[0]
        flags=dict(re.findall(r'\{ &llFTMarioAnim(\w+)FileID, [^,]+, ([^}]+)\}',source))
        clips=json.loads(path.read_text())
        self.assertEqual(len(clips),67)
        for c in clips:
            original=flags.get(names[c['id']],'')
            expected='TransN' if 'XROTN_JOINT' in original else 'XRotN' if 'TRANSN_JOINT' in original else None
            # The source macro names and the actual FTAnimDesc bits are swapped:
            # hiddenparts[0] -> root2 (XRotN), hiddenparts[1] -> root1 (TransN).
            self.assertEqual(c['auxiliary'],expected,c['name'])
            self.assertEqual(c['sample_ticks'][0],0)
            if not c['loop']:self.assertEqual(c['sample_ticks'][-1],c['ticks']-1)
            self.assertEqual(c['frames'],len(c['sample_ticks']))

    def test_super_jump_curve_against_decoded_rom(self):
        path=ROOT/'build/assets/us/0637_FTMarioAnimSuperJumpPunchAir.bin'
        trace=ROOT/'build/scene/special-hi.csv'
        if not path.exists() or not trace.exists():self.skipTest('ROM and native special test required')
        data=path.read_bytes()
        offset=(struct.unpack_from('>I',data,0)[0]&65535)*4
        tracks,end,_=decode_script(data,offset)
        self.assertEqual(end,40)
        previous_y=previous_z=0
        for row in csv.DictReader(trace.read_text().splitlines()):
            t=int(row['tick']);y=evaluate(tracks[5],t);z=evaluate(tracks[6],t)
            self.assertAlmostEqual(float(row['root_y']),y,delta=.0005)
            self.assertAlmostEqual(float(row['root_z']),z,delta=.0005)
            self.assertAlmostEqual(float(row['vy']),(y-previous_y)*1.12,delta=.001)
            self.assertAlmostEqual(float(row['vx']),(z-previous_z)*1.12,delta=.001)
            previous_y,previous_z=y,z

    def test_target_rate_does_not_change_previous_samples(self):
        words=[0x4802,0,0x3803,5,0,0x3002,512,0x2003,5,5120,0,0]
        tracks,_,_=decode_script(struct.pack('>'+'H'*len(words),*words),0)
        self.assertEqual(evaluate(tracks[0],2),0)
        self.assertEqual(evaluate(tracks[0],5),0)

    def test_original_mario_air_jump_attributes(self):
        path=ROOT/'build/assets/us/0203_MarioMain.bin'
        if not path.exists():self.skipTest('Local ROM required')
        data=path.read_bytes()
        actual=struct.unpack_from('>4f',data,0x428+15*4)
        for a,b in zip(actual,[.7,26,.35,.9]):self.assertAlmostEqual(a,b,places=6)
        self.assertEqual(struct.unpack_from('>i',data,0x428+25*4)[0],2)

    def test_track_time_shift_starts_at_event(self):
        # X: 0 -> 20 over 20 ticks. At tick 5 shift elapsed length forward 4.
        words=[0x4802,0,0x1803,20,10240,0x0801,5,0x5803,4,0x0801,15,0]
        tracks,_,_=decode_script(struct.pack('>'+'H'*len(words),*words),0)
        self.assertEqual(evaluate(tracks[0],4),4)
        self.assertEqual(evaluate(tracks[0],5),9)

    def test_blocking_and_parallel_tracks(self):
        # At t=0 set X=1 rad, Y=2 rad, then interpolate X to 3 over 10
        # frames. Y runs in parallel to 4 over 20 frames, followed by a wait.
        words = [0x4806, 512, 1024, 0x4005, 20, 2048, 0x3803, 10, 1536, 0x0801, 20, 0]
        tracks, end, loop = decode_script(struct.pack('>'+'H'*len(words), *words), 0)
        self.assertEqual(end, 30)
        self.assertFalse(loop)
        self.assertAlmostEqual(evaluate(tracks[0], 0), 1)
        self.assertAlmostEqual(evaluate(tracks[0], 5), 2)
        self.assertAlmostEqual(evaluate(tracks[0], 10), 3)
        self.assertAlmostEqual(evaluate(tracks[1], 10), 3)
        self.assertAlmostEqual(evaluate(tracks[1], 20), 4)

    def test_step_does_not_apply_target_early(self):
        words = [0x4802, 512, 0x4803, 4, 1536, 0]
        tracks, end, _ = decode_script(struct.pack('>6H', *words), 0)
        self.assertEqual(end, 4)
        self.assertEqual(evaluate(tracks[0], 3), 1)
        self.assertEqual(evaluate(tracks[0], 4), 3)

    def test_hermite_endpoints_and_slopes(self):
        s = dict(start=0, duration=10, base=2, target=7, rate_base=.2, rate_target=-.3, kind='cubic')
        self.assertEqual(evaluate([s], 0), 2)
        self.assertAlmostEqual(evaluate([s], 10), 7)
        epsilon = 1e-5
        self.assertAlmostEqual((evaluate([s], epsilon)-2)/epsilon, .2, places=5)
        self.assertAlmostEqual((7-evaluate([s], 10-epsilon))/epsilon, -.3, places=5)

    def test_known_rom_wait_keyframe(self):
        path = ROOT / 'build/assets/us/0499_FTMarioAnimWait.bin'
        if not path.exists(): self.skipTest('Local ROM assets required')
        data = path.read_bytes()
        # Independent values from 499_FTMarioAnimWait.c, first joint:
        # TraY=540/4, RotY=223/512, loop length 50.
        tracks, end, loop = decode_script(data, 0x60)
        self.assertEqual(end, 50)
        self.assertTrue(loop)
        self.assertEqual(evaluate(tracks[5], 0), 135)
        self.assertEqual(evaluate(tracks[1], 0), 223/512)

    def test_generated_pose_limits_and_changes(self):
        path = ROOT / 'build/model/animation-info.json'
        if not path.exists(): self.skipTest('Local converted assets required')
        clips = json.loads(path.read_text())
        self.assertEqual([c['frames'] for c in clips[:12]], [50, 60, 41, 10, 8, 19, 1, 31, 46, 9, 21, 3])
        for clip in clips:
            if clip['id']!=573:self.assertGreater(clip['max_change_from_first_fixed'], 0)
            else:self.assertEqual(clip['frames'],1)
            # Model must remain a compact fighter, not explode from bad pointers
            # or a misinterpreted angle/translation factor.
            lower, upper = clip['bounds']
            # Some original attack clips carry forward/root travel (dash and
            # get-up), so the sequence bounds exceed the standing body size.
            legacy={499,501,509,513,518,606,573,511,512,572,637,541}
            self.assertLess(max(upper[i]-lower[i] for i in range(3)),800 if clip['id'] in legacy else 1400)
        self.assertEqual([(c['ticks'],c['frame_step']) for c in clips[7:12]],[(61,2),(91,2),(9,1),(41,2),(6,2)])


if __name__ == '__main__': unittest.main()
