"""Build a host experiment and compare with functions read from pinned upstream.

No ROM or third-party Python packages are needed. Generated reference types are
only a numeric test fixture, never a replacement for the N64 structures' ABI.
"""
from pathlib import Path
import json
import os
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / 'upstream' / 'ssb-decomp-re'
REVISION = '572a747d8171137718fad472c5d7473dc7309ffd'
BUILD = ROOT / 'build' / 'host'


def function(text, name):
    match = re.search(r'^(?:void|f32|sb32)\s+' + re.escape(name) + r'\([^;]*?\)\s*\{', text, re.M)
    if not match:
        raise RuntimeError(f'Cannot find upstream function {name}')
    depth = 1
    end = match.end()
    while depth:
        if text[end] == '{':
            depth += 1
        elif text[end] == '}':
            depth -= 1
        end += 1
    return text[match.start():end]


def main():
    revision = subprocess.check_output(['git', '-C', str(UPSTREAM), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != REVISION:
        raise RuntimeError(f'Expected upstream {REVISION}, found {revision}')
    inputs = ['src/ft/ftphysics.c', 'src/ft/ftcommon/ftcommonjump.c', 'src/ft/ftcommon.h', 'include/macros.h']
    subprocess.run(['git', '-C', str(UPSTREAM), 'diff', '--exit-code', 'HEAD', '--', *inputs], check=True)
    BUILD.mkdir(parents=True, exist_ok=True)
    zig = ROOT / '.tools' / 'zig' / 'ziglang' / ('zig.exe' if os.name == 'nt' else 'zig')
    compiler = [str(zig), 'cc'] if zig.exists() else [shutil.which('cc') or shutil.which('clang') or shutil.which('gcc') or 'cc']
    env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(ROOT / '.cache' / 'zig'), ZIG_LOCAL_CACHE_DIR=str(ROOT / '.cache' / 'zig-local'))
    physics = (UPSTREAM / inputs[0]).read_text()
    jump = (UPSTREAM / inputs[1]).read_text()
    names = ['ftPhysicsClampGroundVel', 'ftPhysicsSetGroundVelFriction',
             'ftPhysicsSetGroundVelAbsStickRange', 'ftPhysicsApplyGravityClampTVel',
             'ftPhysicsClampAirVelXStickRange', 'ftPhysicsApplyAirVelXFriction',
             'ftCommonJumpGetJumpForceButton']
    bodies = [function(jump if name.startswith('ftCommon') else physics, name) for name in names]
    # Read exact numeric constants from the reference, rather than duplicating them.
    common = (UPSTREAM / inputs[2]).read_text()
    constants = []
    for name in re.findall(r'\bFTCOMMON_\w+', bodies[-1]):
        line = re.search(r'^#define\s+' + name + r'\s+([^\s/]+)', common, re.M)
        if not line:
            raise RuntimeError(f'Missing upstream constant {name}')
        constants.append(f'#define {name} {line[1]}')
    macros = (UPSTREAM / inputs[3]).read_text()
    controller_max = re.search(r'^#define I_CONTROLLER_RANGE_MAX\s+(\d+)', macros, re.M)[1]
    header = '''#include <math.h>
typedef float f32;
typedef int s32;
typedef int sb32;
typedef struct { struct { struct { float x, y; } vel_ground, vel_air; } physics;
                 struct { struct { struct { int x; } stick_range; } pl; } input; } FTStruct;
typedef struct { float air_friction; } FTAttributes;
#define FALSE 0
#define TRUE 1
#define ABS(x) ((x) < 0 ? -(x) : (x))
#define SQUARE(x) ((x) * (x))
'''
    header += f'#define F_CONTROLLER_RANGE_MAX ((float){controller_max})\n'
    header += '\n'.join(dict.fromkeys(constants)) + '\n'
    (BUILD / 'reference.h').write_text(header + '\n'.join(body[:body.index('{')].strip() + ';' for body in bodies))
    (BUILD / 'reference.c').write_text('#include "reference.h"\n' + '\n\n'.join(bodies))
    flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-ffp-contract=off', '-fno-fast-math', '-Isrc', '-Ibuild/host']
    suffix = '.exe' if os.name == 'nt' else ''
    test = BUILD / ('compare_motion' + suffix)
    lab = BUILD / ('motion_lab' + suffix)
    for sources, output in [(['src/motion.c', 'tests/compare_motion.c', 'build/host/reference.c'], test),
                            (['src/motion.c', 'src/motion_lab.c'], lab)]:
        subprocess.run(compiler + flags + sources + ['-o', str(output), '-lm'], cwd=ROOT, env=env, check=True)
    result = subprocess.check_output([str(test)], cwd=ROOT, text=True)
    print(result, end='')
    with (BUILD / 'jump-experiment.csv').open('w', newline='') as output:
        subprocess.run([str(lab)], stdout=output, check=True)
    (BUILD / 'verification.json').write_text(json.dumps({
        'upstream_revision': revision, 'compiler': subprocess.check_output(compiler + ['--version'], text=True).splitlines()[0],
        'functions': names, 'result': result.strip(), 'scope': 'Host C comparison only; not N64 or DS runtime validation'
    }, indent=2))
    print('Generated build/host/jump-experiment.csv and verification.json')


if __name__ == '__main__':
    main()
