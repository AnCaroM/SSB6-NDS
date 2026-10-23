"""Extract a bounded first resource set from the verified US ROM.

Table interpretation follows ssb-decomp-re/tools/relocData.py and BattleShip's
tools/examine_blob_layout.py. Bytes stay big-endian with unresolved reloc chains.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import shutil
import subprocess
from prepare_rom import EXPECTED_SHA1

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / 'upstream/ssb-decomp-re'
TORCH = ROOT / 'upstream/BattleShip/torch'
TORCH_REV = '3aa9c9739af95659f19adc73afeb12df93957fc3'
IDS = [202, 203, 255, 296, 298, 299]


def main():
    rom = (ROOT / 'build/rom/baserom.us.z64').read_bytes()
    if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
        raise RuntimeError('Only the verified US v1.0 ROM is supported')
    revision = subprocess.check_output(['git', '-C', str(TORCH), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != TORCH_REV:
        raise RuntimeError('Unexpected Torch revision: ' + revision)
    subprocess.run(['git', '-C', str(TORCH), 'diff', '--exit-code', 'HEAD', '--', 'lib/libvpk0'], check=True)
    build = ROOT / 'build/assets-tools'
    output = ROOT / 'build/assets/us'
    build.mkdir(parents=True, exist_ok=True)
    output.mkdir(parents=True, exist_ok=True)
    suffix = '.exe' if os.name == 'nt' else ''
    zig = ROOT / '.tools/zig/ziglang' / ('zig' + suffix)
    decoder = build / ('vpk_decode' + suffix)
    env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(ROOT / '.cache/zig'),
               ZIG_LOCAL_CACHE_DIR=str(ROOT / '.cache/zig-local'))
    compiler = [str(zig), 'cc'] if zig.is_file() else [shutil.which('cc') or shutil.which('clang') or shutil.which('gcc') or 'cc']
    subprocess.run([*compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(TORCH / 'lib/libvpk0'), str(ROOT / 'tools/vpk_decode.c'),
                    str(TORCH / 'lib/libvpk0/vpk0.c'), '-o', str(decoder)], env=env, check=True)
    descriptions = (UPSTREAM / 'tools/relocFileDescriptions.us.txt').read_text()
    names = {int(fid): name for fid, name in re.findall(r'^-(\d+):\s*(\S+)', descriptions, re.M)}
    count = int(re.search(r'FILE_COUNT:\s*(\d+)', descriptions)[1])
    base = 0x1AC870
    data_start = base + (count + 1) * 12
    entries = []
    for fid in range(count + 1):
        flags, intern, packed_words, extern, decoded_words = struct.unpack_from('>IHHHH', rom, base + fid * 12)
        entries.append({'id': fid, 'name': names.get(fid), 'compressed': bool(flags & 0x80000000),
                        'relative_offset': flags & 0x7fffffff, 'internal_chain_word': intern,
                        'packed_words': packed_words, 'external_chain_word': extern,
                        'decoded_bytes': decoded_words * 4})
    for entry, next_entry in zip(entries, entries[1:]):
        start = data_start + entry['relative_offset']
        end = data_start + next_entry['relative_offset']
        if not data_start <= start <= end <= 0xAC7340:
            raise RuntimeError('Invalid resource table extent')
        entry['rom_offset'] = start
        entry['stored_extent_bytes'] = end - start
    (output / 'resource-catalog.json').write_text(json.dumps(entries[:-1], indent=2))
    extracted = []
    for fid in IDS:
        item = dict(entries[fid])
        stem = f'{fid:04d}_{item["name"]}'
        raw = rom[item['rom_offset']:item['rom_offset'] + item['stored_extent_bytes']]
        path = output / (stem + '.bin')
        if item['compressed']:
            compressed = output / (stem + '.vpk0')
            compressed.write_bytes(raw)
            subprocess.run([str(decoder), str(compressed), str(path)], check=True)
        else:
            path.write_bytes(raw[:item['decoded_bytes']])
        data = path.read_bytes()
        if len(data) != item['decoded_bytes']:
            raise RuntimeError('Decoded size mismatch: ' + stem)
        item['output'] = str(path.relative_to(ROOT))
        item['sha256'] = hashlib.sha256(data).hexdigest()
        extracted.append(item)
        print(f'{stem}: {len(data)} decoded bytes', flush=True)
    # Cross-check scalar attributes against values studied in the C source.
    # FTAttributes begins at file offset 0x428 in US MarioMain.
    mario = (output / '0203_MarioMain.bin').read_bytes()
    attr_base = 0x428
    fields = {'walk_speed_mul': (0x20, 0.3), 'traction': (0x24, 1.5),
              'jump_vel_x': (0x38, 0.35), 'jump_height_mul': (0x3c, 0.7),
              'jump_height_base': (0x40, 26.0), 'air_accel': (0x4c, 0.025),
              'air_speed_max_x': (0x50, 30.0), 'air_friction': (0x54, 0.2),
              'gravity': (0x58, 2.4), 'tvel_base': (0x5c, 44.0), 'tvel_fast': (0x60, 70.0)}
    attributes = {}
    for name, (offset, expected) in fields.items():
        value = struct.unpack_from('>f', mario, attr_base + offset)[0]
        expected_bits = struct.pack('>f', expected)
        if mario[attr_base+offset:attr_base+offset+4] != expected_bits:
            raise RuntimeError('Mario attribute mismatch: ' + name)
        attributes[name] = {'offset': hex(attr_base + offset), 'value': value}
    (output / 'extraction-info.json').write_text(json.dumps({
        'rom_sha1': EXPECTED_SHA1, 'torch_revision': revision,
        'decoder_source_sha256': hashlib.sha256((TORCH / 'lib/libvpk0/vpk0.c').read_bytes()).hexdigest(),
        'table_entries': count, 'extracted': extracted, 'mario_attributes': attributes,
        'scope': 'Decompressed source resources only; no DS conversion, relocation or runtime loading'
    }, indent=2))
    print(f'PASS: {len(extracted)} resource sizes and {len(attributes)} Mario attribute bit patterns verified')


if __name__ == '__main__':
    main()
