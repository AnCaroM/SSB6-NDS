"""Decode original Mario locomotion clips using the already-built VPK tool."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
from mario_moves import SPECS

from prepare_rom import EXPECTED_SHA1

ROOT = Path(__file__).resolve().parents[1]
CLIPS = {499: 'Wait', 501: 'Walk2', 509: 'JumpF', 513: 'Fall', 518: 'LandingAirX',
         606: 'Jab1', 573: 'CliffWait', 511: 'JumpAerialF', 512: 'JumpAerialB', 572: 'CliffCatch',
         637: 'SuperJumpPunchAir', 541: 'FallSpecial'}
for _,fid,name in SPECS:
    if fid not in CLIPS:CLIPS[fid]=name or 'CatchPull'
CLIPS.update({504:'Dash',505:'Run',506:'RunBrake',565:'CapturePulled'})
# Order follows dFTMarioMotionDescs / nFTCommonMotionDamage*, not file labels.
DAMAGE_IDS = list(range(523,540))
for fid,name in zip(DAMAGE_IDS, ['DamageHi1','DamageHi2','DamageHi3',
    'DamageN1','DamageN2','DamageN3','DamageLw1','DamageLw2','DamageLw3',
    'DamageAir1','DamageAir2','DamageAir3','DamageFlyHi','DamageFlyN',
    'DamageFlyLw','DamageFlyTop','DamageFlyRoll']):
    CLIPS[fid]=name
CLIPS[540]='DamageFall'


def main():
    rom = (ROOT / 'build/rom/baserom.us.z64').read_bytes()
    if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
        raise ValueError('Expected the verified US v1.0 ROM')
    output = ROOT / 'build/assets/us'
    decoder = ROOT / 'build/assets-tools' / ('vpk_decode.exe' if os.name == 'nt' else 'vpk_decode')
    report = [];weapons=[]
    for fid, name in {**CLIPS,297:'MarioSpecial3',204:'MarioSpecial1'}.items():
        flags, intern, packed, extern, decoded = struct.unpack_from('>I4H', rom, 0x1ac870 + fid*12)
        next_offset = struct.unpack_from('>I', rom, 0x1ac870 + (fid+1)*12)[0] & 0x7fffffff
        start = 0x1b2c6c + (flags & 0x7fffffff)
        end = 0x1b2c6c + next_offset
        if not 0x1b2c6c <= start < end <= 0xac7340:
            raise ValueError('Invalid asset extent')
        stem = f'{fid:04d}_FTMarioAnim{name}'
        path = output / (stem + '.bin')
        if flags >> 31:
            compressed = output / (stem + '.vpk0')
            compressed.write_bytes(rom[start:end])
            subprocess.run([str(decoder), str(compressed), str(path)], check=True)
        else:
            path.write_bytes(rom[start:start + decoded*4])
        if path.stat().st_size != decoded*4:
            raise ValueError('Decoded animation length mismatch')
        (report if fid in CLIPS else weapons).append({'id': fid, 'name': name, 'file': path.name,
                       'internal_chain_word': intern, 'external_chain_word': extern,
                       'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'bytes': decoded*4})
    (output / 'motion-extraction-info.json').write_text(json.dumps({'rom_sha1': EXPECTED_SHA1, 'clips': report,'weapons':weapons}, indent=2))
    print([(clip['name'], clip['bytes']) for clip in report])


if __name__ == '__main__':
    main()
