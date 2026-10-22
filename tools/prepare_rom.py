"""Validate a user-supplied Smash 64 US ROM and save a normalized work copy."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
EXPECTED_SHA1 = 'e2929e10fccc0aa84e5776227e798abc07cedabf'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    member = None
    if source.suffix.lower() == '.zip':
        with zipfile.ZipFile(source) as archive:
            candidates = [i for i in archive.infolist() if not i.is_dir() and Path(i.filename).suffix.lower() in {'.z64', '.n64', '.v64'}]
            if len(candidates) != 1:
                raise RuntimeError('Expected exactly one ROM in the ZIP; found ' + str(len(candidates)))
            entry = candidates[0]
            if entry.file_size != 16 * 1024 * 1024:
                raise RuntimeError('Expected a 16 MiB US ROM')
            member = entry.filename
            data = archive.read(entry)
    else:
        if source.stat().st_size != 16 * 1024 * 1024:
            raise RuntimeError('Expected a 16 MiB US ROM')
        data = source.read_bytes()
    original_sha256 = hashlib.sha256(data).hexdigest()
    magic = data[:4].hex()
    if magic == '80371240':
        byte_order = 'z64 big-endian'
    elif magic == '37804012':
        byte_order = 'v64 byte-swapped; normalized to z64'
        result = bytearray(len(data))
        result[0::2], result[1::2] = data[1::2], data[0::2]
        data = bytes(result)
    elif magic == '40123780':
        byte_order = 'n64 little-endian; normalized to z64'
        result = bytearray(len(data))
        for offset in range(4):
            result[offset::4] = data[3-offset::4]
        data = bytes(result)
    else:
        raise RuntimeError('Unrecognized ROM byte-order header: ' + magic)
    sha1 = hashlib.sha1(data).hexdigest()
    if sha1 != EXPECTED_SHA1:
        raise RuntimeError('ROM does not match supported US v1.0: SHA1=' + sha1)
    output = ROOT / 'build/rom'
    output.mkdir(parents=True, exist_ok=True)
    target = output / 'baserom.us.z64'
    target.write_bytes(data)
    result = {'source': str(source), 'zip_member': member, 'bytes': len(data),
              'input_byte_order': byte_order, 'input_rom_sha256': original_sha256,
              'normalized_sha1': sha1, 'normalized_md5': hashlib.md5(data).hexdigest(),
              'normalized_sha256': hashlib.sha256(data).hexdigest(),
              'internal_title': data[0x20:0x34].decode('ascii').strip(),
              'game_code': data[0x3b:0x3f].decode('ascii'), 'revision_byte': data[0x3f],
              'work_copy': str(target), 'matches_upstream_us_reference': True}
    (output / 'rom-info.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
