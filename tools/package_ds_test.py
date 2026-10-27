"""Prepare a local, self-contained hardware-test bundle from the manual build."""
import hashlib
import argparse
import json
from pathlib import Path
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--version',choices=['01','02','03','04','05','06','07','08','09','10','11','12','13','14','15','16','17','18','19','20'],default='20')
    args=parser.parse_args()
    stem='ssb-scene' if args.version=='01' else 'ssb-combat' if args.version=='02' else 'ssb-versus' if args.version in ('10','11','12','13','14','15','16','17','18','19','20') else 'ssb-dreamland'
    release='SSB-NDS-Prueba'+args.version
    source = ROOT / f'build/nds/{stem}.nds'
    info = json.loads((ROOT / f'build/nds/{stem}-build-info.json').read_text())
    if hashlib.sha256(source.read_bytes()).hexdigest() != info['nds_sha256']:
        raise ValueError('The manual build does not match its recorded hash')
    for path, expected in info['sources'].items():
        if hashlib.sha256((ROOT / path).read_bytes()).hexdigest() != expected:
            raise ValueError('Source changed since build: ' + path)
    output = ROOT / 'Para tu DS'
    if args.version!='01': output=output/('Prueba'+args.version)
    output.mkdir(exist_ok=True,parents=True)
    rom = output / (release+'.nds')
    shutil.copyfile(source, rom)
    doc={'01':'prueba-ds-dspico.md','02':'combate-prueba02.md','03':'dreamland-prueba03.md','04':'camara-prueba04.md','05':'bordes-prueba05.md','06':'recuperacion-prueba06.md','07':'especial-prueba07.md','08':'ataques-prueba08.md','09':'animaciones-prueba09.md','10':'versus-prueba10.md','11':'menu-prueba11.md','12':'impactos-prueba12.md','13':'animaciones-prueba13.md','14':'sonido-prueba14.md','15':'huesos-prueba15.md','16':'link-prueba16.md','17':'link-prueba17.md','18':'link-prueba18.md','19':'escudo-prueba19.md','20':'kirby-prueba20.md'}[args.version]
    instructions = (ROOT / 'docs' / doc).read_text(encoding='utf-8')
    (output / 'LEEME.txt').write_text(instructions, encoding='utf-8')
    (output / 'SHA256SUMS.txt').write_text(info['nds_sha256'] + '  ' + rom.name + '\n')
    if args.version!='01':
        budget=json.loads((ROOT/f'build/nds/{stem}-budget.json').read_text())
        info['budget']=budget
    info.update({'release': 'Prueba'+args.version, 'hardware_tested': False,
                 'target': 'Nintendo DS / DS Lite', 'external_assets_required': False})
    (output / 'compilacion.json').write_text(json.dumps(info, indent=2))
    files = [rom, output/'LEEME.txt', output/'SHA256SUMS.txt', output/'compilacion.json']
    archive = output / (release+'.zip')
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as package:
        for file in files:
            package.write(file, arcname=file.name)
    with zipfile.ZipFile(archive) as package:
        if package.testzip() is not None:
            raise ValueError('Archive validation failed')
        if package.read(rom.name) != source.read_bytes():
            raise ValueError('Packaged ROM mismatch')
    print(f'{rom}: {rom.stat().st_size} bytes')
    print(f'SHA256 {info["nds_sha256"]}')
    print(archive)


if __name__ == '__main__':
    main()
