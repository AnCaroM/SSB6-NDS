"""Fetch pinned references into ignored folders, without resetting existing work."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]

def setup(check=False,with_reference_ports=False):
    manifest=json.loads((ROOT/'upstream.lock.json').read_text())
    for source in manifest['sources']:
        if not source['required'] and not with_reference_ports:continue
        target=ROOT/source['path']
        if not (target/'.git').exists():
            if check:raise RuntimeError('Missing source: '+source['path'])
            if target.exists() and any(target.iterdir()):
                raise RuntimeError('Refusing to replace nonempty directory: '+str(target))
            target.parent.mkdir(parents=True,exist_ok=True)
            subprocess.run(['git','clone','--no-checkout',source['url'],str(target)],check=True)
            subprocess.run(['git','-C',str(target),'checkout','--detach',source['revision']],check=True)
        revision=subprocess.check_output(['git','-C',str(target),'rev-parse','HEAD'],text=True).strip()
        if revision!=source['revision']:
            raise RuntimeError('Unexpected revision, existing checkout left untouched: '+source['path'])
        print(source['name']+': '+revision)
    if with_reference_ports:
        # Only the decomp reference is needed for compare_battleship.py.
        subprocess.run(['git','-C',str(ROOT/'upstream/BattleShip'),
                        'submodule','update','--init','decomp'],check=True) if not check else None

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--check',action='store_true',help='Read-only verification; no downloads')
    parser.add_argument('--with-reference-ports',action='store_true',help='Also prepare optional BattleShip comparison')
    args=parser.parse_args();setup(args.check,args.with_reference_ports)

if __name__=='__main__':main()
