"""Small host regression suite that needs no ROM or reference checkout."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--compiler',type=Path,help='Optional compiler executable (zig, gcc or clang)')
    args=parser.parse_args()
    build=ROOT/'build/source-tests';build.mkdir(parents=True,exist_ok=True)
    (ROOT/'build/scene').mkdir(parents=True,exist_ok=True)
    zig=ROOT/'.tools/zig/ziglang'/('zig.exe' if os.name=='nt' else 'zig')
    compiler=[str(zig),'cc'] if zig.is_file() else [shutil.which('cc') or shutil.which('clang') or shutil.which('gcc') or 'cc']
    if args.compiler:
        selected=args.compiler.resolve(strict=True)
        compiler=[str(selected),'cc'] if selected.stem.lower()=='zig' else [str(selected)]
    env=dict(os.environ,ZIG_GLOBAL_CACHE_DIR=str(ROOT/'.cache/zig'),ZIG_LOCAL_CACHE_DIR=str(ROOT/'.cache/zig-local'))
    sources=['src/motion.c','src/stage.c','src/motion_scene.c','src/special_hi.c','src/camera.c','src/recovery_demo.c']
    tests=['scene','stage','camera','island','recovery','special_hi']
    for name in tests:
        output=build/('test_'+name+('.exe' if os.name=='nt' else ''))
        subprocess.run(compiler+['-std=c11','-O2','-UNDEBUG','-Wall','-Wextra','-Werror',
            '-ffp-contract=off','-fno-fast-math','-Isrc',*sources,'tests/test_'+name+'.c',
            '-o',str(output),'-lm'],cwd=ROOT,env=env,check=True)
        subprocess.run([str(output)],cwd=ROOT,check=True)
    print('PASS: source-only suites; no ROM, generated model or upstream checkout required.')

if __name__=='__main__':main()
