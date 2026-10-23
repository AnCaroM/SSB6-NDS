"""Build and exercise the provisional movement scene on the host PC."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    build = ROOT / 'build/scene'
    build.mkdir(parents=True, exist_ok=True)
    suffix = '.exe' if os.name == 'nt' else ''
    zig = ROOT / '.tools/zig/ziglang' / ('zig' + suffix)
    compiler = [str(zig), 'cc'] if zig.is_file() else [shutil.which('cc') or shutil.which('clang') or shutil.which('gcc') or 'cc']
    env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(ROOT / '.cache/zig'),
               ZIG_LOCAL_CACHE_DIR=str(ROOT / '.cache/zig-local'))
    flags = ['-std=c11', '-O2', '-UNDEBUG', '-Wall', '-Wextra', '-Werror', '-ffp-contract=off',
             '-fno-fast-math', '-Isrc', 'src/motion.c', 'src/stage.c', 'src/motion_scene.c', 'src/combat.c','src/versus.c', 'src/animation_review.c', 'src/camera.c', 'src/recovery_demo.c', 'src/special_hi.c', 'build/model/mario_moves.c']
    test = build / ('test_scene' + suffix)
    trace = build / ('scene_trace' + suffix)
    map_test = build / ('test_stage' + suffix)
    combat_test = build / ('test_combat' + suffix)
    for source, output in [('tests/test_scene.c', test), ('tests/test_stage.c', map_test),
                           ('tests/test_combat.c',combat_test),
                           ('tests/test_island.c',build/('test_island'+suffix)),
                           ('tests/test_recovery.c',build/('test_recovery'+suffix)),
                           ('tests/test_special_hi.c',build/('test_special_hi'+suffix)),
                           ('tests/test_moves.c',build/('test_moves'+suffix)),
                           ('tests/test_versus.c',build/('test_versus'+suffix)),
                           ('tests/test_damage.c',build/('test_damage'+suffix)),
                           ('tests/test_animation_review.c',build/('test_animation_review'+suffix)),
                           ('tests/test_camera.c',build/('test_camera'+suffix)), ('src/scene_trace.c', trace)]:
        subprocess.run(compiler + flags + [source, '-o', str(output), '-lm'],
                       cwd=ROOT, env=env, check=True)
    result = subprocess.check_output([str(test)], text=True)
    result += subprocess.check_output([str(map_test)], text=True)
    result += subprocess.check_output([str(combat_test)], text=True)
    result += subprocess.check_output([str(build/('test_camera'+suffix))], text=True)
    result += subprocess.check_output([str(build/('test_island'+suffix))], text=True)
    result += subprocess.check_output([str(build/('test_recovery'+suffix))], text=True)
    result += subprocess.check_output([str(build/('test_special_hi'+suffix))], text=True)
    result += subprocess.check_output([str(build/('test_moves'+suffix))], text=True)
    result += subprocess.check_output([str(build/('test_versus'+suffix))], text=True)
    result += subprocess.check_output([str(build/('test_damage'+suffix))], text=True)
    result += subprocess.check_output([str(build/('test_animation_review'+suffix))], text=True)
    pose_test=build/('test_pose_bank'+suffix)
    subprocess.run(compiler+['-std=c11','-O2','-UNDEBUG','-DMARIO_COMPACT_POSES=1',
        '-Isrc','-Ibuild/model','build/model/mario_clips.c','tests/test_pose_bank.c',
        '-o',str(pose_test)],cwd=ROOT,env=env,check=True)
    result += subprocess.check_output([str(pose_test)],text=True)
    subprocess.run([sys.executable,str(ROOT/'tools/generate_fight_audio.py')],check=True)
    audio_test=build/('test_audio'+suffix)
    subprocess.run(compiler+['-std=c11','-O2','-UNDEBUG','-Wall','-Wextra','-Werror',
        '-Isrc','-Itests/nds_audio','-Iplatform/nds','-Ibuild/audio',
        'platform/nds/fight_audio.c','build/audio/fight_samples.c','tests/test_audio.c',
        '-o',str(audio_test)],cwd=ROOT,env=env,check=True)
    result += subprocess.check_output([str(audio_test)],text=True)
    print(result, end='')
    hashes = {}
    for scenario in ['long', 'short', 'ledge']:
        data = subprocess.check_output([str(trace), scenario])
        (build / (scenario + '.csv')).write_bytes(data)
        hashes[scenario] = hashlib.sha256(data).hexdigest()
    for scenario in ['recovery-left','recovery-right','special-hi','special-left','special-right','moves','versus']:
        hashes[scenario]=hashlib.sha256((build/(scenario+'.csv')).read_bytes()).hexdigest()
    (build / 'verification.json').write_text(json.dumps({
        'result': result.strip(), 'trace_sha256': hashes,
        'compiler': subprocess.check_output(compiler + ['--version'], text=True).splitlines()[0],
        'scope': 'Provisional scene integration on host; not original game or NDS runtime equivalence'
    }, indent=2))
    print('Generated 300-tick traces: build/scene/{long,short,ledge}.csv')


if __name__ == '__main__':
    main()
