"""Offline sampling of Mario's compact AObjEvent16 (Figatree) clips.

Semantics: ft/ftanim.c ftAnimParseDObjFigatree/ftAnimGetTargetValue and
sys/objanim.c gcPlayDObjAnimJoint. Floating point evaluation uses Python doubles;
this converter is not a bit-exact reproduction of the N64 interpreter.
"""
import hashlib
import json
import math
import struct
import numpy as np


def transform(values):
    tx, ty, tz, rx, ry, rz, sx, sy, sz = values
    cr, sr, cp, sp, cy, syaw = math.cos(rx), math.sin(rx), math.cos(ry), math.sin(ry), math.cos(rz), math.sin(rz)
    matrix = np.eye(4)
    matrix[:3, :3] = np.array([[cp*cy, sr*sp*cy-cr*syaw, cr*sp*cy+sr*syaw],
                              [cp*syaw, sr*sp*syaw+cr*cy, cr*sp*syaw-sr*cy],
                              [-sp, sr*cp, cr*cp]]) @ np.diag([sx, sy, sz])
    matrix[:3, 3] = [tx, ty, tz]
    return matrix


def decode_script(data, start):
    tracks = [[] for _ in range(10)]
    time, p = 0, start
    def read(signed=False):
        nonlocal p
        value = struct.unpack_from('>h' if signed else '>H', data, p)[0]
        p += 2
        return value
    for _ in range(1024):
        command = read()
        opcode, flags, toggle = command >> 11, (command >> 1) & 1023, command & 1
        if opcode in (0, 13):
            if opcode == 13:
                target = p + read(True)
                if not start <= target < p:
                    raise ValueError('Unexpected clip loop target')
            return tracks, time, opcode == 13
        duration = read() if toggle else 0
        if opcode == 1:
            time += duration
            continue
        if opcode == 11:
            # nGCAnimEvent1611 adds payload to the active track's elapsed
            # length. Preserve earlier samples; the shift takes effect now.
            for track in range(10):
                if flags & (1 << track) and tracks[track]:
                    old=tracks[track][-1]
                    tracks[track].append(dict(old,start=old['start']-duration,active=time))
            continue
        if opcode not in (2, 3, 4, 5, 6, 7, 8, 9, 10):
            raise ValueError(f'Unsupported Figatree event {opcode} at {p-2:#x}')
        for track in range(10):
            if not flags & (1 << track):
                continue
            if track == 3:
                raise ValueError('Cubic translation paths are not supported')
            previous = tracks[track][-1] if tracks[track] else None
            base, rate_base = (previous['target'], previous['rate_target']) if previous else (0, 0)
            factor = 1/512 if track < 3 else (1/4 if track < 7 else 1/4096)
            rate_factor = 1/512 if track < 3 else (1/32 if track < 7 else 1/8192)
            if opcode == 6:
                rate = read(True) * rate_factor
                if previous is None:
                    raise ValueError('Rate-only event without preceding value')
                # Rate changes take effect at the command's time, not on
                # samples preceding it (important for SuperJump's startup).
                tracks[track].append(dict(previous,rate_target=rate,active=time))
                continue
            target = read(True) * factor
            rate_target = read(True) * rate_factor if opcode in (4, 5) else 0
            kind = 'linear' if opcode in (2, 3) else ('step' if opcode in (9, 10) else 'cubic')
            tracks[track].append(dict(start=time, duration=duration, base=base, target=target,
                                      rate_base=rate_base, rate_target=rate_target, kind=kind))
        if opcode in (2, 4, 7, 9):
            time += duration
    raise ValueError('Animation script did not terminate')


def evaluate(segments, time):
    segment = next((s for s in reversed(segments) if s.get('active',s['start']) <= time), None)
    if segment is None:
        return None
    start, duration, a, b = (segment[k] for k in ('start', 'duration', 'base', 'target'))
    elapsed = time - start
    if duration == 0:
        return b
    if segment['kind'] == 'step':
        return b if elapsed >= duration else a
    if segment['kind'] == 'linear':
        return a + (b-a) * elapsed/duration
    t = elapsed/duration
    # Hermite form of gcGetInterpValueCubic / gcPlayDObjAnimJoint.
    return ((2*t**3-3*t**2+1)*a + (-2*t**3+3*t**2)*b +
            (t**3-2*t**2+t)*duration*segment['rate_base'] +
            (t**3-t**2)*duration*segment['rate_target'])


def export_clips(root, model, center, scale, output):
    manifest = json.loads((root / 'build/assets/us/motion-extraction-info.json').read_text())
    if manifest['rom_sha1'] != 'e2929e10fccc0aa84e5776227e798abc07cedabf':
        raise ValueError('Animation provenance mismatch')
    vertices = [v for _, triangle in model.triangles for v in triangle]
    unique={};mapping=[];compact=[]
    for v in vertices:
        key=(v[3][0],*v[3][1])
        if key not in unique:unique[key]=len(compact);compact.append(v)
        mapping.append(unique[key])
    vertices=compact
    (output/'pose-map.json').write_text(json.dumps({'mapping':mapping,'sources':[
        {'joint':int(v[3][0]),'position':[float(x) for x in v[3][1]]} for v in vertices]},indent=2))
    source_joints = np.array([v[3][0] for v in vertices])
    source_xyz = np.array([[*v[3][1], 1] for v in vertices])
    defaults = [list(struct.unpack_from('>9f', model.data, 0x2208 + i*44)) for i in range(25)]
    # Track indices: RPY, translate interpolation, XYZ, scale XYZ.
    fields = {0:3, 1:4, 2:5, 4:0, 5:1, 6:2, 7:6, 8:7, 9:8}
    lines = ['/* Generated from the user ROM. */', '#include "mario_mesh.h"']
    clips = [];bones={};audit_poses={}
    for index, asset in enumerate(manifest['clips']):
        data = (root / 'build/assets/us' / asset['file']).read_bytes()
        if hashlib.sha256(data).hexdigest() != asset['sha256']:
            raise ValueError('Animation hash mismatch')
        pointers, word = {}, asset['internal_chain_word']
        while word != 65535:
            if word*4 in pointers:
                raise ValueError('Cyclic animation relocations')
            raw = struct.unpack_from('>I', data, word*4)[0]
            pointers[word*4] = (raw & 65535)*4
            word = raw >> 16
        # ftMain hiddenparts: bit30 inserts TransN before common joints and
        # detaches it for physics; bit31 inserts XRotN, retained in the tree.
        # Grab bit28 enables common joint28, last child of common root4.
        transn=asset['id'] in {572,573,580,581,582,583,609,613,614,637}
        xrotn=asset['id'] in {565,626}
        catch=asset['id'] in {561,562,563,564}
        scripts = []
        for joint in range(24+int(transn or xrotn or catch)):
            ptr = pointers.get(joint*4)
            if ptr is None and struct.unpack_from('>I', data, joint*4)[0] != 0:
                raise ValueError('Unresolved animation table pointer')
            scripts.append(decode_script(data, ptr) if ptr is not None else None)
        duration = max(script[1] for script in scripts if script is not None)
        looping = any(script[2] for script in scripts if script is not None)
        auxiliary=scripts.pop(0) if transn or xrotn else None
        if not 1 <= duration <= 200:
            raise ValueError('Unexpected clip duration')
        frames = duration if looping else duration+1
        # One original hanging pose adds 5760 shared bytes, rather than caching
        # the entire long idle clip. Ambient ledge animation is pending.
        if asset['id']==573:frames=1;looping=True
        legacy={499,501,509,513,518,606,573,572}
        step=1 if asset['id'] in legacy else 2
        ticks=frames
        sample_frames=list(range(0,frames,step))
        if not looping and sample_frames[-1]!=frames-1:sample_frames.append(frames-1)
        frames=len(sample_frames)
        lines.append(f'#ifndef MARIO_COMPACT_POSES\nstatic const int16_t pose_{index}[] = {{')
        lower, upper = np.ones(3)*np.inf, np.ones(3)*-np.inf
        first = None
        delta = 0
        sampled=[]
        bones[asset['id']]=[]
        for frame in range(ticks):
            world, stack = [], {}
            prefix=np.eye(4)
            # Hanging/catch phases are rendered relative to the stage ledge;
            # retain their authored TransN offset in that visual coordinate.
            if (xrotn or asset['id'] in {572,573,580,582}) and auxiliary is not None:
                values=[0,0,0,0,0,0,1,1,1]
                for track,field in fields.items():
                    value=evaluate(auxiliary[0][track],min(frame,auxiliary[1]))
                    if value is not None:values[field]=value
                prefix=transform(values)
            for joint, values in enumerate(defaults):
                values = list(values)
                if joint < len(scripts) and scripts[joint] is not None:
                    tracks, end, _ = scripts[joint]
                    sample_time = min(frame, end)
                    for track, field in fields.items():
                        value = evaluate(tracks[track], sample_time)
                        if value is not None:
                            values[field] = value
                local = transform(values)
                depth = model.joints[joint]['depth']
                matrix = stack[depth-1] @ local if depth else prefix @ local
                stack[depth] = matrix
                world.append(matrix)
            bones[asset['id']].append(world)
            if frame not in sample_frames:continue
            posed = np.empty((len(vertices), 3))
            for joint, matrix in enumerate(world):
                selected = source_joints == joint
                posed[selected] = (source_xyz[selected] @ matrix.T)[:, :3]
            lower, upper = np.minimum(lower, posed.min(axis=0)), np.maximum(upper, posed.max(axis=0))
            fixed = np.rint((posed-center)*scale*4096).astype(np.int64)
            if not np.isfinite(posed).all() or np.abs(fixed).max() > 32767:
                raise ValueError('Animated vertices outside DS limits')
            if first is None: first = fixed
            delta = max(delta, int(np.abs(fixed-first).max()))
            sampled.append(fixed)
            lines.append(','.join(str(x) for x in fixed.flatten()) + ',')
        lines.append('};')
        bank=np.array(sampled,dtype=np.int32)
        audit_poses[str(asset['id'])]=bank.astype(np.int16)
        low=bank.min(axis=(0,1));span=np.maximum(bank.max(axis=(0,1))-low,1)
        quant=np.rint((bank-low)*255/span).astype(np.uint8)
        restored=low+(quant.astype(np.int32)*span+127)//255
        error=float(np.abs(bank-restored).max())/4096/scale
        if error>2.1:raise ValueError('Compact pose quantization error exceeded')
        lines.append(f'#else\nstatic const uint8_t pose_{index}[] = {{')
        lines.extend(','.join(map(str,frame.flatten()))+',' for frame in quant)
        lines.append('};\n#endif')
        clips.append(dict(name=asset['name'], id=asset['id'], frames=frames, loop=looping,
                          frame_step=step,ticks=ticks,
                          sample_ticks=sample_frames,
                          compact_low=low.tolist(),compact_span=span.tolist(),
                          compact_pose_bytes=int(quant.size)+12,
                          compact_max_axis_error_world=error,
                          pose_vertices=len(vertices),
                          source_duration=duration,
                          auxiliary='TransN' if transn else 'XRotN' if xrotn else None,
                          resource_sha256=asset['sha256'], bounds=[lower.tolist(), upper.tolist()],
                          max_change_from_first_fixed=delta))
    lines += ['const uint16_t mario_pose_vertex_map[] = {'+','.join(map(str,mapping))+'};',
              f'const unsigned mario_pose_vertex_count={len(vertices)};']
    weapon=next(a for a in manifest['weapons'] if a['id']==297)
    raw=(root/'build/assets/us'/weapon['file']).read_bytes()
    if hashlib.sha256(raw).hexdigest()!=weapon['sha256']:raise ValueError('Weapon hash mismatch')
    palette=struct.unpack_from('>16H',raw,0x30)
    pixels=[]
    for byte in raw[0x58:0x58+128]:
        for index in (byte>>4,byte&15):
            c=palette[index];pixels.append((c>>11&31)|((c>>6&31)<<5)|((c>>1&31)<<10)|(32768 if c&1 else 0))
    lines.append('const uint16_t mario_fireball_pixels[256] __attribute__((aligned(4))) = {'+','.join(map(str,pixels))+'};')
    lines.append('const MarioClip mario_clips[] = {')
    for i, clip in enumerate(clips):
        lo=','.join(map(str,clip['compact_low']));sp=','.join(map(str,clip['compact_span']))
        lines.append(f'{{pose_{i},{clip["frames"]},{int(clip["loop"])},{clip["frame_step"]},{clip["ticks"]},{{{lo}}},{{{sp}}}}},')
    lines += ['};', f'const unsigned mario_clip_count = {len(clips)};']
    for name,fid in [('dash',504),('run',505),('brake',506),('capture',565)]:
        lines.append(f'const unsigned mario_{name}_clip={next(i for i,c in enumerate(clips) if c["id"]==fid)};')
    (output / 'mario_clips.c').write_text('\n'.join(lines)+'\n')
    (output / 'animation-info.json').write_text(json.dumps(clips, indent=2))
    np.savez_compressed(output/'animation-audit.npz',**audit_poses)
    from mario_moves import export
    export(root,output,clips,bones)
    print('Original clips:', [(c['name'], c['frames']) for c in clips])
