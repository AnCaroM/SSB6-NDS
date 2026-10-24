"""Conservative static DS budgets, not a claim of peak RAM or GPU frame time."""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--demo', choices=['scene','combat','dreamland','versus'], default='combat')
    args = parser.parse_args()
    build = ROOT / 'build/nds'
    info = json.loads((build / f'ssb-{args.demo}-build-info.json').read_text())
    sizes = [int(x) for x in info['size'].splitlines()[-1].split()[:3]]
    mesh = json.loads((ROOT / 'build/model/conversion-info.json').read_text())
    clips = json.loads((ROOT / 'build/model/animation-info.json').read_text())
    instances = 2 if args.demo in ('combat','dreamland','versus') else 1
    stage_poly,stage_vertices,stage_textures,texture_capacity=200,700,0,131072
    stage_info=None
    if args.demo in ('dreamland','versus'):
        stage_info=json.loads((ROOT/'build/dreamland/conversion-info.json').read_text())
        stage_poly=stage_info['triangles']+1+120
        stage_vertices=stage_info['vertices']+4+400
        stage_textures=stage_info['texture_bytes']+stage_info['wallpaper_bytes']
        texture_capacity=262144
    # Reserve 200 polygons/700 vertices for the current training stage. Its
    # explicit primitives submit 136 polygons/431 vertices before clipping.
    projectile_count=8 if args.demo=='versus' else 4 if instances==2 else 0
    report = {'demo':args.demo, 'arm9_static_bytes':sum(sizes), 'arm9_static_budget':2621440,
              'shared_texture_bytes':stage_textures+512+sum(t['padded_width']*t['padded_height']*2 for t in mesh['textures']),
              'texture_capacity_bytes':texture_capacity, 'instances':instances,
              'shared_pose_bytes':json.loads((ROOT/'build/model/skeleton-info.json').read_text())['bytes'] if info.get('skeletal_poses') else
                  sum(c['compact_pose_bytes'] if info.get('compact_poses') else c['frames']*c.get('pose_vertices',mesh['vertices_expanded'])*6 for c in clips),
              'skeletal_poses':info.get('skeletal_poses',False),
              'compact_poses':info.get('compact_poses',False),
              'polygons_with_stage_reserve':instances*mesh['triangles']+stage_poly+projectile_count,
              'vertices_with_stage_reserve':instances*mesh['vertices_expanded']+stage_vertices+projectile_count*4,
              'polygon_budget':1400,'vertex_budget':4000,
              'limits':['Static ELF is not peak main RAM','Stage reserve must be revised if stage geometry changes',
                        'Submitted geometry differs from clipped geometry','Runtime GPU/CPU time needs hardware measurement']}
    if report['arm9_static_bytes']>report['arm9_static_budget']: raise ValueError('ARM9 static RAM budget exceeded')
    if 'build/fighters/link/link_model.c' in info['sources']:
        # Link viewer: textures share the A+B banks; its bone-pose bank is separate.
        link=json.loads((ROOT/'build/fighters/link/link-info.json').read_text())
        report['shared_texture_bytes']+=link['texture_bytes'];report['link_pose_bytes']=link['pose_bytes']
        report['link_texture_bytes']=link['texture_bytes']
    if args.demo=='versus' and info.get('audio_enabled',True):
        audio=json.loads((ROOT/'build/audio/audio-info.json').read_text())
        report['shared_audio_pcm_bytes']=audio['bytes']
        report['audio_channels']=audio['channel_limit']
        # Original ROM effects/voices, PCM8 16 kHz. Revisit if more fighters add voices.
        if audio['bytes']>262144:raise ValueError('Audio bank exceeded 256 KiB')
    elif args.demo=='versus':
        report['shared_audio_pcm_bytes']=0;report['audio_channels']=0
    if report['shared_texture_bytes']>report['texture_capacity_bytes']: raise ValueError('Texture banks exceeded')
    if report['polygons_with_stage_reserve']>1400 or report['vertices_with_stage_reserve']>4000:
        raise ValueError('Geometry submission budget exceeded')
    (build / f'ssb-{args.demo}-budget.json').write_text(json.dumps(report,indent=2))
    print(json.dumps({k:v for k,v in report.items() if k!='limits'},indent=2))


if __name__ == '__main__': main()
