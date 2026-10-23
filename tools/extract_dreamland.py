"""Extract Dream Land's exact US resources and resolve relocation provenance."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
from prepare_rom import EXPECTED_SHA1

ROOT=Path(__file__).resolve().parents[1]
NAMES={88:'StageDreamLand',103:'StagePupupuImages',104:'StagePupupuFile2',152:'StagePupupuFile3',255:'GRPupupuMap'}


def main():
    rom=(ROOT/'build/rom/baserom.us.z64').read_bytes()
    if hashlib.sha1(rom).hexdigest()!=EXPECTED_SHA1: raise ValueError('Expected verified US ROM')
    out=ROOT/'build/assets/dreamland';out.mkdir(parents=True,exist_ok=True)
    manifest={}
    for fid,name in NAMES.items():
        flags,intern,packed,extern,decoded=struct.unpack_from('>I4H',rom,0x1ac870+fid*12)
        stop=struct.unpack_from('>I',rom,0x1ac870+(fid+1)*12)[0]&0x7fffffff
        start=flags&0x7fffffff
        if not 0<=start<stop<0xac7340-0x1b2c6c:raise ValueError('Invalid file extent')
        raw=rom[0x1b2c6c+start:0x1b2c6c+stop]
        path=out/f'{fid:04d}_{name}.bin'
        if flags>>31:
            compressed=path.with_suffix('.vpk0');compressed.write_bytes(raw)
            subprocess.run([str(ROOT/'build/assets-tools'/('vpk_decode.exe' if os.name=='nt' else 'vpk_decode')),str(compressed),str(path)],check=True)
        else:path.write_bytes(raw[:decoded*4])
        data=path.read_bytes()
        if len(data)!=decoded*4:raise ValueError('Invalid decoded length')
        pointers={}
        for kind,head in [('internal',intern),('external',extern)]:
            cursor=head;seen=set();index=0
            while cursor!=65535:
                offset=cursor*4
                if offset in seen or offset+4>len(data):raise ValueError('Invalid reloc chain')
                seen.add(offset);word=struct.unpack_from('>I',data,offset)[0]
                target_fid=fid if kind=='internal' else struct.unpack_from('>H',raw,packed*4+index*2)[0]
                pointers[str(offset)]={'file':target_fid,'offset':(word&65535)*4}
                cursor=word>>16;index+=1
        manifest[str(fid)]={'name':name,'file':path.name,'sha256':hashlib.sha256(data).hexdigest(),
                            'bytes':len(data),'pointers':pointers}
    (out/'manifest.json').write_text(json.dumps({'rom_sha1':EXPECTED_SHA1,'resources':manifest},indent=2))
    print([(fid,m['bytes']) for fid,m in manifest.items()])


if __name__=='__main__':main()
