"""Audit tracked/staged public files; never scans ignored local game assets."""
from pathlib import Path, PurePosixPath
import re
import subprocess

ROOT=Path(__file__).resolve().parents[1]
BLOCKED_ROOTS={'build','upstream','.tools','.cache','Para tu DS','.venv','venv','.agents','.codex'}
BLOCKED_SUFFIXES={'.nds','.elf','.z64','.v64','.n64','.vpk0','.bin','.npz','.exe','.dll','.o','.a','.map',
                  '.zip','.7z','.sav','.srm','.dsv','.pem','.p12','.key'}

def main():
    entries=subprocess.check_output(['git','ls-files','--stage','-z'],cwd=ROOT).decode('utf-8').split('\0')
    indexed=[]
    for entry in filter(None,entries):
        metadata,name=entry.split('\t',1);mode,oid,stage=metadata.split()
        if mode not in {'100644','100755'} or stage!='0':
            raise SystemExit('Unsupported index entry (submodule, symlink or conflict): '+name)
        indexed.append((name,oid))
    raw=subprocess.check_output(['git','cat-file','--batch'],cwd=ROOT,
        input=('\n'.join(oid for _,oid in indexed)+'\n').encode()) if indexed else b''
    cursor=0
    errors=[];total=0;count=0
    token=re.compile(r'(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----)')
    for name,oid in indexed:
        end=raw.index(b'\n',cursor);header=raw[cursor:end].split();cursor=end+1
        if len(header)!=3 or header[0].decode()!=oid or header[1]!=b'blob':
            raise SystemExit('Cannot read indexed blob: '+name)
        size=int(header[2]);data=raw[cursor:cursor+size];cursor+=size+1
        path=PurePosixPath(name);count+=1
        if path.parts[0] in BLOCKED_ROOTS or path.suffix.lower() in BLOCKED_SUFFIXES or path.name.startswith('.env'):
            errors.append('Excluded local/private artifact is tracked: '+name);continue
        total+=len(data)
        if b'\0' in data or data[:2]==b'MZ':errors.append('Unexpected binary file: '+name)
        if len(data)>1024*1024:errors.append('Unexpected file over 1 MiB: '+name)
        text=data.decode('utf-8',errors='replace')
        if token.search(text):errors.append('Potential credential: '+name)
        if re.search(r'[A-Za-z]:[/\\]Users[/\\][^/\\\s]+',text):errors.append('Personal absolute user path: '+name)
    if not count:raise SystemExit('No indexed files. Stage the intended public sources first.')
    if errors:raise SystemExit('\n'.join(errors))
    print(f'PASS: {count} indexed text files, {total:,} bytes; no blocked artifacts or recognized secrets.')
    print('This targeted check is not a guarantee against every possible secret.')

if __name__=='__main__':main()
