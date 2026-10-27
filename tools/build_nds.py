"""Compile the research scene with a current devkitPro/libnds/calico install.

Uses argument lists instead of GNU make so Windows paths with spaces work.
Mirrors devkitarm-rules/ds_rules; no SDK download or global configuration.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--devkitpro', default=os.environ.get('DEVKITPRO'))
    parser.add_argument('--check', action='store_true', help='Only check SDK files')
    parser.add_argument('--demo', choices=['motion', 'mario', 'scene', 'combat', 'dreamland','versus'], default='motion')
    parser.add_argument('--autoplay', action='store_true', help='Start the integrated scene with scripted input')
    parser.add_argument('--compact-poses',action='store_true',help='Use bounded-error 8-bit pose bank with integer decode')
    parser.add_argument('--skeletal-poses',action='store_true',help='Bone-pose bank rendered with the DS matrix stack')
    parser.add_argument('--animation-review',action='store_true',help='Separate slow scripted attack/reaction diagnostic ROM')
    args = parser.parse_args()
    if not args.devkitpro:
        parser.error('Set DEVKITPRO or pass --devkitpro C:/devkitPro. SDK is not installed by this script.')
    sdk = Path(args.devkitpro).resolve()
    arm = Path(os.environ.get('DEVKITARM', str(sdk / 'devkitARM'))).resolve()
    suffix = '.exe' if os.name == 'nt' else ''
    gcc = arm / 'bin' / ('arm-none-eabi-gcc' + suffix)
    size = arm / 'bin' / ('arm-none-eabi-size' + suffix)
    ndstool = sdk / 'tools' / 'bin' / ('ndstool' + suffix)
    calico = sdk / 'calico'
    required = [gcc, size, ndstool, sdk / 'libnds/include/nds.h',
                sdk / 'libnds/lib/libnds9.a', calico / 'lib/libcalico_ds9.a',
                calico / 'share/ds9.specs', calico / 'bin/ds7_maine.elf',
                calico / 'share/nds-icon.bmp']
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        print('Missing current devkitPro NDS SDK files:\n' + '\n'.join(missing), file=sys.stderr)
        return 2
    if args.check:
        print('SDK prerequisites present (compilation not attempted).')
        return 0
    build = ROOT / 'build/nds'
    build.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, DEVKITPRO=sdk.as_posix(), DEVKITARM=arm.as_posix())
    env['PATH'] = str(arm / 'bin') + os.pathsep + env.get('PATH', '')
    flags = ['-std=gnu11', '-O2', '-g', '-Wall', '-Wextra', '-Werror',
             '-march=armv5te', '-mtune=arm946e-s', '-mfloat-abi=soft',
             '-DARM9', '-D__NDS__', '-ffp-contract=off', '-fno-fast-math',
             '-ffunction-sections', '-fdata-sections', '-I' + str(ROOT / 'src'),
             '-I' + str(sdk / 'libnds/include'), '-I' + str(calico / 'include')]
    sources = ['src/motion.c', 'src/stage.c', 'src/motion_scene.c', 'src/fighter_attr.c', 'src/special_hi.c', 'platform/nds/main.c']
    stem = 'ssb-' + args.demo
    if args.animation_review:
        if args.demo!='versus':parser.error('--animation-review requires --demo versus')
        flags+=['-DANIMATION_REVIEW=1','-DSCENE_AUTOPLAY=1']
        stem+='-review'
    if args.compact_poses and args.skeletal_poses:parser.error('Choose --compact-poses or --skeletal-poses')
    if args.compact_poses:flags.append('-DMARIO_COMPACT_POSES=1')
    if args.skeletal_poses:flags.append('-DMARIO_SKELETAL=1')
    if args.autoplay:
        if args.demo not in ('scene','combat','dreamland','versus'): parser.error('--autoplay requires an integrated scene')
        stem += '-auto'
        flags.append('-DSCENE_AUTOPLAY=1')
    if args.demo in ('mario', 'scene', 'combat', 'dreamland','versus'):
        subprocess.run([sys.executable, str(ROOT / 'tools/extract_motion_assets.py')], check=True)
        subprocess.run([sys.executable, str(ROOT / 'tools/convert_mario_model.py')], check=True)
        sources = ['platform/nds/model_main.c', 'platform/nds/mario_render.c', 'build/model/mario_mesh.c', 'build/model/mario_clips.c']
        if args.demo in ('scene','combat','dreamland','versus'):
            sources = ['src/motion.c', 'src/stage.c', 'src/motion_scene.c', 'src/fighter_attr.c', 'src/special_hi.c', 'platform/nds/scene_main.c', 'platform/nds/stage_render.c',
                       'platform/nds/mario_render.c', 'build/model/mario_mesh.c', 'build/model/mario_clips.c']
            if args.demo in ('combat','dreamland','versus'):
                sources[sources.index('platform/nds/scene_main.c')] = 'platform/nds/combat_main.c'
                sources.append('src/combat.c')
                sources.append('build/model/mario_moves.c')
                sources.append('src/fighter_mario_legacy.c')
                sources.append('src/recovery_demo.c')
            if args.demo=='versus':
                # Mario and Link come from the generic converter (bone renderer);
                # the legacy Mario mesh/clips/moves are not linked.
                for legacy in ('platform/nds/combat_main.c','platform/nds/mario_render.c','build/model/mario_mesh.c',
                               'build/model/mario_clips.c','build/model/mario_moves.c','src/fighter_mario_legacy.c'):
                    sources.remove(legacy)
                sources.insert(0,'platform/nds/versus_main.c')
                subprocess.run([sys.executable,str(ROOT/'tools/extract_fight_audio.py')],check=True)
                flags.append('-I'+str(ROOT/'build/audio'))
                sources+=['src/versus.c','src/menu.c','platform/nds/fight_audio.c','build/audio/fight_samples.c']
                for fighter in ('mario','link','kirby'):
                    subprocess.run([sys.executable,str(ROOT/'tools/convert_fighter.py'),'--fighter',fighter],check=True)
                    flags.append('-I'+str(ROOT/'build/fighters'/fighter))
                    sources.append(f'build/fighters/{fighter}/{fighter}_model.c')
                sources.append('platform/nds/fighter_render.c')
                if args.animation_review:sources.append('src/animation_review.c')
            if args.demo in ('dreamland','versus'):
                subprocess.run([sys.executable,str(ROOT/'tools/extract_dreamland.py')],check=True)
                subprocess.run([sys.executable,str(ROOT/'tools/convert_dreamland.py')],check=True)
                sources.remove('platform/nds/stage_render.c')
                sources+=['src/camera.c','platform/nds/dreamland_render.c','build/dreamland/dreamland_mesh.c']
                flags+=['-DORIGINAL_DREAMLAND=1','-I'+str(ROOT/'build/dreamland')]
        if args.skeletal_poses and args.demo!='versus':sources.append('build/model/mario_skeleton.c')
        flags.append('-I' + str(ROOT / 'build/model'))
    objects = []
    for index, source in enumerate(sources):
        obj = build / f'{stem}-{index}.o'
        subprocess.run([str(gcc), *flags, '-c', str(ROOT / source), '-o', str(obj)], env=env, check=True)
        objects.append(str(obj))
    elf, nds = build / (stem + '.elf'), build / (stem + '.nds')
    subprocess.run([str(gcc), '-march=armv5te', '-mtune=arm946e-s',
                    '-specs=' + str(calico / 'share/ds9.specs'),
                    '-L' + str(calico / 'lib'), '-L' + str(sdk / 'libnds/lib'),
                    '-Wl,--gc-sections', '-Wl,-Map,' + str(build / (stem + '.map')),
                    *objects, '-lnds9', '-lcalico_ds9', '-lm', '-o', str(elf)], env=env, check=True)
    subprocess.run([str(ndstool), '-c', str(nds), '-9', str(elf), '-7',
                    str(calico / 'bin/ds7_maine.elf'), '-b', str(calico / 'share/nds-icon.bmp'),
                    ({'mario': 'SSB Mario;Model viewer;Static bind pose',
                      'scene': 'SSB Mario;Integration;Original model and clips',
                      'combat': 'SSB NDS;Prueba 02;Mario - combate de prueba',
                      'dreamland': 'SSB NDS;Prueba 09;Mario - animaciones revisadas',
                      'versus': 'SSB NDS;Prueba 20;Kirby, escudo y explorar',
                      'motion': 'SSB Motion;Research scene;Provisional movement'}[args.demo])], env=env, check=True)
    size_output = subprocess.check_output([str(size), str(elf)], text=True)
    inputs=sources + [p.relative_to(ROOT).as_posix() for folder in
        ['src','platform/nds','build/model','build/dreamland','build/audio','build/fighters/mario','build/fighters/link','build/fighters/kirby']
        for p in (ROOT/folder).glob('*.h')]
    (build / (stem + '-build-info.json')).write_text(json.dumps({
        'compiler': subprocess.check_output([str(gcc), '--version'], text=True).splitlines()[0],
        'sources': {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in inputs},
        'nds_sha256': hashlib.sha256(nds.read_bytes()).hexdigest(), 'size': size_output,
        'demo': args.demo, 'autoplay': args.autoplay,'compact_poses':args.compact_poses,'skeletal_poses':args.skeletal_poses,
        'audio_enabled':args.demo=='versus','animation_review':args.animation_review,
        'runtime_tested': False}, indent=2))
    print(size_output)
    print(nds)
    return 0


if __name__ == '__main__':
    sys.exit(main())
