"""Convert US Mario's high detail bind pose to an explicit DS triangle mesh.

This is a bounded F3DEX2 reader, not an N64 renderer. Skeleton transforms follow
syMatrixTraRotRpyRScaF. Colors use the initial material script and a fixed light;
CI4 textures and initial material frames are converted to DS direct color.
RDP combiners and animation are not fully emulated.
Resource-derived output stays in ignored build/ and requires the user's ROM.
"""
import hashlib
import json
import math
from pathlib import Path
import struct

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = '157287d186182c4966a4019b478668e364c7f23624bf54ed598be2a170be526f'


class Model:
    def __init__(self, data, internal_chain_word=2):
        self.data = data
        self.pointers = {}
        word = internal_chain_word
        while word != 0xffff:
            offset = word * 4
            if offset in self.pointers:
                raise ValueError('Cyclic relocation chain')
            raw = self.u32(offset)
            self.pointers[offset] = (raw & 0xffff) * 4
            word = raw >> 16
        self.triangles = []
        self.joints = []
        self.opcodes = {}
        # RSP vertices retain their already-transformed position across joints:
        # elbow/knee lists reuse the previous part's ring to join the meshes.
        self.cache = {}
        self.diffuse = np.ones(3)
        self.ambient = np.ones(3) * .3
        self.texture_on = False
        self.texture_scale = np.ones(2)
        self.tile_origin = np.zeros(2)
        self.tile_extent = (0, 0)
        self.tile = None
        self.image = None
        self.palette = None
        self.loaded_image = None
        self.textures = []
        self.texture_keys = {}
        self.triangle_textures = []

    def u32(self, offset):
        return struct.unpack_from('>I', self.data, offset)[0]

    def ptr(self, offset):
        if offset in self.pointers:
            return self.pointers[offset]
        if self.u32(offset) == 0:
            return None
        raise ValueError(f'Unresolved pointer at {offset:#x}')

    def colors(self, joint):
        dispatch = self.ptr(joint * 4)
        anim = self.ptr(0x2680 + joint * 4)
        result = []
        if dispatch is None:
            return result
        for index in range(8):
            material = self.ptr(dispatch + index * 4)
            if material is None:
                break
            color = list(self.data[material + 0x50:material + 0x54])
            light1 = list(self.data[material + 0x60:material + 0x64])
            light2 = list(self.data[material + 0x64:material + 0x68])
            script = self.ptr(anim + index * 4) if anim is not None else None
            # Only initial (time zero) immediate color events. Stop at first wait.
            if script is not None:
                for _ in range(16):
                    event = self.u32(script)
                    script += 4
                    opcode, flags, duration = event >> 25, (event >> 15) & 1023, event & 32767
                    if opcode == 0:
                        break
                    # At frame zero a positive-duration step still uses its
                    # preceding value (gcPlayMObjMatAnim / nGCAnimKindStep).
                    if duration:
                        break
                    if opcode not in (3, 4, 8, 9, 10, 11, 18, 19, 20, 21):
                        raise ValueError(f'Unsupported initial material event {opcode}')
                    for bit in range(10):
                        if flags & (1 << bit):
                            if opcode >= 18 and bit == 0:
                                color = list(self.data[script:script + 4])
                            if opcode >= 18 and bit == 3:
                                light1 = list(self.data[script:script + 4])
                            if opcode >= 18 and bit == 4:
                                light2 = list(self.data[script:script + 4])
                            script += 4
                    if duration:
                        break
            result.append({'offset': material, 'color': color, 'light1': light1, 'light2': light2,
                           'flags': struct.unpack_from('>H', self.data, material + 0x30)[0]})
        return result

    def use_material(self, material):
        """Time-zero subset of gcDrawMObjForDObj's generated segment E lists."""
        p, flags = material['offset'], material['flags']
        if flags == 0:
            flags = 0xa1
        if flags & 4:
            self.image = self.ptr(self.ptr(p + 0x2c))
            if flags & 3:
                self.palette = self.image
        if flags & 0x11:
            self.image = self.ptr(self.ptr(p + 4))
        if flags & 0xe0:
            unk08, bias, width, height = struct.unpack_from('>4H', self.data, p + 8)
            mode = self.u32(p + 0x10)
            u, v, su, sv, unk24, unk28 = struct.unpack_from('>6f', self.data, p + 0x14)
            if mode == 1:
                su *= .5
                u = (u - unk24 + 1 - unk28 * .5) * .5
            if flags & 0x20:
                self.tile_extent = (width, height)
                if mode == 2:
                    self.tile_origin = np.maximum(0, [int(width*u/su*4)/4, int(height*v/sv*4)/4])
                else:
                    self.tile_origin = np.array([int((width*u+bias)/su*4)/4,
                                                 int(((1-sv-v)*height+bias)/sv*4)/4])
            if flags & 0x80:
                unit = np.array([width*64, height*64]) if mode == 2 else np.ones(2)*2097152/unk08
                self.texture_scale = np.minimum(65535, np.trunc(unit / [su, sv])) / 65536
                self.texture_on = True

    def current_texture(self):
        if not self.texture_on:
            return -1
        if self.tile is None or self.loaded_image is None or self.palette is None:
            raise ValueError('Incomplete texture state')
        fmt, size, line, cms, masks, cmt, maskt = self.tile
        if (fmt, size) != (2, 0):
            raise ValueError(f'Only CI4 render tiles supported, got {fmt}/{size}')
        image, byte_count = self.loaded_image
        width = line * 16
        height, remainder = divmod(byte_count * 2, width)
        if remainder or not width or not height:
            raise ValueError('Invalid CI4 texture extent')
        key = (image, self.palette, width, height, cms, masks, cmt, maskt, self.tile_extent)
        if key not in self.texture_keys:
            palette = struct.unpack_from('>16H', self.data, self.palette)
            # N64 RGBA5551 -> DS A1B5G5R5.
            colors = [((c >> 11) & 31) | (((c >> 6) & 31) << 5) |
                      (((c >> 1) & 31) << 10) | ((c & 1) << 15) for c in palette]
            raw = self.data[image:image+byte_count]
            if len(raw) != byte_count:
                raise ValueError('Texture outside resource')
            pixels = [colors[index] for byte in raw for index in (byte >> 4, byte & 15)]
            # N64 can clamp after a mirrored period, e.g. 32-texel face data
            # in a 64-texel tile. DS clamps at physical texture size instead.
            # Materialize that mirrored interval before padding to a DS size.
            ew = max(width, self.tile_extent[0]) if cms == 3 else width
            eh = max(height, self.tile_extent[1]) if cmt == 3 else height
            pw, ph = max(8, 1 << (ew-1).bit_length()), max(8, 1 << (eh-1).bit_length())
            def address(value, length, extent, mode):
                value = min(value, extent-1)
                if mode == 3:
                    value %= length*2
                    return value if value < length else length*2-1-value
                return min(value, length-1)
            padded = [pixels[address(y,height,eh,cmt)*width + address(x,width,ew,cms)]
                      for y in range(ph) for x in range(pw)]
            # DS wrap/flip mirrors N64 mask behavior for these power-of-two S axes.
            param = (1 << 16 if not cms & 2 else 0) | (1 << 17 if not cmt & 2 else 0)
            if cms == 1: param |= 1 << 18
            if cmt == 1: param |= 1 << 19
            self.texture_keys[key] = len(self.textures)
            self.textures.append({'image': image, 'palette': self.palette, 'width': width, 'height': height,
                                  'padded_width': pw, 'padded_height': ph, 'param': param, 'pixels': padded})
        return self.texture_keys[key]

    def display_list(self, offset, matrix, joint):
        cache = self.cache
        colors = self.colors(joint)
        color = [255, 255, 255, 255]
        light = np.array([.25, .65, 1.0])
        light /= np.linalg.norm(light)
        count = 0
        while True:
            count += 1
            if count > 1024:
                raise ValueError('Display list did not terminate')
            w0, w1 = struct.unpack_from('>II', self.data, offset)
            opcode = w0 >> 24
            self.opcodes[hex(opcode)] = self.opcodes.get(hex(opcode), 0) + 1
            if opcode == 0xdf:
                return
            if opcode == 0x01:
                n = (w0 >> 12) & 255
                start = ((w0 >> 1) & 127) - n
                vertex = self.ptr(offset + 4)
                if vertex is None or not 0 <= start <= start + n <= 32:
                    raise ValueError('Invalid vertex load')
                for i in range(n):
                    xyz = struct.unpack_from('>hhh', self.data, vertex + i * 16)
                    normal = np.array(struct.unpack_from('>bbb', self.data, vertex + i * 16 + 12)) / 127
                    normal = matrix[:3, :3] @ normal
                    shade = np.minimum(1, self.ambient + self.diffuse * .7 * max(0, np.dot(normal, light)))
                    position = (matrix @ np.array([*xyz, 1.0]))[:3]
                    st = np.array(struct.unpack_from('>hh', self.data, vertex + i * 16 + 8)) / 32
                    # RSP scales ST when loading a vertex, before tile origin.
                    cache[start + i] = (position, shade, st * self.texture_scale, (joint, xyz))
            elif opcode in (0x05, 0x06):
                for tri in ([w0] if opcode == 5 else [w0, w1]):
                    indices = [(tri >> shift & 255) // 2 for shift in (16, 8, 0)]
                    vertices = []
                    for i in indices:
                        if i not in cache:
                            raise ValueError(f'Missing vertex {i} at {offset:#x}, joint {joint}; cache={list(cache)}')
                        position, shade, st, source = cache[i]
                        rgb = [round(min(255, max(0, c * s))) for c, s in zip(color[:3], shade)]
                        vertices.append((position.copy(), rgb, st - self.tile_origin, source))
                    self.triangles.append((joint, vertices))
                    self.triangle_textures.append(self.current_texture())
            elif opcode == 0xde:
                if w1 >> 24 != 0x0e or (w1 & 0xffffff) % 8:
                    raise ValueError(f'Unsupported display list branch at {offset:#x}')
                index = (w1 & 0xffffff) // 8
                material = colors[index]
                self.use_material(material)
                if material['flags'] & 0x218:
                    color = material['color']
                if material['flags'] & 0x1000:
                    self.diffuse = np.array(material['light1'][:3]) / 255
                if material['flags'] & 0x2000:
                    self.ambient = np.array(material['light2'][:3]) / 255
            elif opcode == 0xfa:
                color = list(struct.pack('>I', w1))
            elif opcode == 0xdb:
                if (w0 >> 16) & 255 == 0x0a:
                    rgb = np.array(list(struct.pack('>I', w1))[:3]) / 255
                    if (w0 & 0xffff) in (0, 4):
                        self.diffuse = rgb
                    elif (w0 & 0xffff) in (0x18, 0x1c):
                        self.ambient = rgb
            elif opcode == 0x02:
                if (w0 >> 16) & 255 != 0x14:
                    raise ValueError('Only texture-coordinate vertex edits supported')
                index = (w0 & 0xffff) // 2
                position, shade, _, source = cache[index]
                cache[index] = (position, shade, np.array(struct.unpack('>hh', struct.pack('>I', w1))) / 32, source)
            elif opcode == 0xd7:
                self.texture_on = bool(w0 & 2)
                self.texture_scale = np.array([w1 >> 16, w1 & 65535]) / 65536
            elif opcode == 0xfd:
                self.image = self.ptr(offset + 4)
            elif opcode == 0xf0:
                self.palette = self.image
            elif opcode == 0xf3:
                # LoadBlock uses 16-bit load tiles for the CI4 source.
                self.loaded_image = (self.image, (((w1 >> 12) & 4095) + 1) * 2)
            elif opcode == 0xf5 and (w1 >> 24) & 7 == 0:
                self.tile = ((w0 >> 21) & 7, (w0 >> 19) & 3, (w0 >> 9) & 511,
                             (w1 >> 8) & 3, (w1 >> 4) & 15, (w1 >> 18) & 3, (w1 >> 14) & 15)
            elif opcode == 0xf2 and (w1 >> 24) & 7 == 0:
                self.tile_origin = np.array([(w0 >> 12) & 4095, w0 & 4095]) / 4
                self.tile_extent = tuple(int(x) for x in
                                         ((np.array([(w1 >> 12) & 4095, w1 & 4095]) / 4) - self.tile_origin + 1))
            elif opcode not in (0xe7, 0xdb, 0xe3, 0xfc, 0xe8, 0xf5, 0xe6,
                                0xf0, 0xd7, 0xf2, 0xfd, 0xf3, 0xd9, 0xfb):
                raise ValueError(f'Unsupported F3DEX2 command {opcode:#x} at {offset:#x}')
            offset += 8

    def convert(self):
        stack = {}
        for joint in range(32):
            offset = 0x2200 + joint * 44
            fields = struct.unpack_from('>II9f', self.data, offset)
            if fields[0] == 18:
                break
            depth = fields[0] & 0xfff
            tx, ty, tz, rx, ry, rz, sx, sy, sz = fields[2:]
            cr, sr, cp, sp, cy, syaw = math.cos(rx), math.sin(rx), math.cos(ry), math.sin(ry), math.cos(rz), math.sin(rz)
            rotation = np.array([[cp*cy, sr*sp*cy-cr*syaw, cr*sp*cy+sr*syaw],
                                 [cp*syaw, sr*sp*syaw+cr*cy, cr*sp*syaw-sr*cy],
                                 [-sp, sr*cp, cr*cp]])
            local = np.eye(4)
            local[:3, :3] = rotation @ np.diag([sx, sy, sz])
            local[:3, 3] = [tx, ty, tz]
            world = stack[depth - 1] @ local if depth else local
            stack[depth] = world
            dl = self.ptr(offset + 4)
            self.joints.append({'index': joint, 'depth': depth, 'display_list': dl})
            if dl is not None:
                self.display_list(dl, world, joint)
        else:
            raise ValueError('Skeleton terminator missing')


def main():
    path = ROOT / 'build/assets/us/0296_MarioModel.bin'
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != EXPECTED:
        raise ValueError('Expected the verified US v1.0 MarioModel resource')
    model = Model(data)
    model.convert()
    positions = np.array([v[0] for _, triangle in model.triangles for v in triangle])
    if not np.isfinite(positions).all() or not len(positions):
        raise ValueError('Empty or non-finite mesh')
    lower, upper = positions.min(axis=0), positions.max(axis=0)
    center = (lower + upper) / 2
    scale = 2 / max(upper - lower)
    output = ROOT / 'build/model'
    output.mkdir(parents=True, exist_ok=True)
    header = '''/* Generated from the user's ROM; do not distribute. */
#pragma once
#include <stdint.h>
typedef struct { int16_t x,y,z; uint16_t color; int16_t u,v; } MarioVertex;
typedef struct { const uint16_t *pixels; uint16_t width,height; uint32_t param; } MarioTexture;
extern const MarioVertex mario_vertices[];
extern const unsigned mario_vertex_count;
extern const int16_t mario_triangle_textures[];
extern const MarioTexture mario_textures[];
extern const unsigned mario_texture_count;
#ifdef MARIO_COMPACT_POSES
typedef uint8_t MarioPose;
#else
typedef int16_t MarioPose;
#endif
typedef struct { const MarioPose *positions; uint16_t frames, loop, frame_step, ticks; int16_t low[3]; uint16_t span[3]; } MarioClip;
extern const MarioClip mario_clips[];
extern const unsigned mario_clip_count;
extern const uint16_t mario_pose_vertex_map[];
extern const unsigned mario_pose_vertex_count;
extern const uint16_t mario_fireball_pixels[256];
extern const unsigned mario_dash_clip,mario_run_clip,mario_brake_clip,mario_capture_clip;
'''
    (output / 'mario_mesh.h').write_text(header)
    with (output / 'mario_mesh.h').open('a') as f:
        f.write(f'#define MARIO_RESTORE_SCALE {1/scale:.9f}f\n')
        for axis, value in zip('XYZ', center):
            f.write(f'#define MARIO_CENTER_{axis} {value:.9f}f\n')
    lines = ['/* Generated from the user ROM; do not distribute. */', '#include "mario_mesh.h"', 'const MarioVertex mario_vertices[] = {']
    obj = ['# Mario US v1.0 high detail bind pose. Vertex colors are approximate.']
    for _, triangle in model.triangles:
        for position, rgb, uv, _ in triangle:
            xyz = [round(x) for x in ((position - center) * scale * 4096)]
            if not all(-32768 <= x <= 32767 for x in xyz):
                raise ValueError('Position outside DS vertex range')
            color = (rgb[0] >> 3) | ((rgb[1] >> 3) << 5) | ((rgb[2] >> 3) << 10)
            st = [round(x * 16) for x in uv]
            if not all(-32768 <= x <= 32767 for x in st):
                raise ValueError('Texture coordinate outside DS range')
            lines.append('    {%d,%d,%d,%d,%d,%d},' % (*xyz, color, *st))
            obj.append('v %f %f %f %f %f %f' % (*position, *(c / 255 for c in rgb)))
    for start in range(1, len(positions) + 1, 3):
        obj.append(f'f {start} {start+1} {start+2}')
    lines += ['};', f'const unsigned mario_vertex_count = {len(positions)};']
    lines += ['const int16_t mario_triangle_textures[] = {', ','.join(map(str, model.triangle_textures)), '};']
    for i, texture in enumerate(model.textures):
        lines += [f'static const uint16_t texture_{i}[] __attribute__((aligned(4))) = {{',
                  ','.join(map(str, texture['pixels'])), '};']
    lines += ['const MarioTexture mario_textures[] = {']
    for i, t in enumerate(model.textures):
        lines += [f'{{texture_{i},{t["padded_width"]},{t["padded_height"]},{t["param"]}}},']
    lines += ['};', f'const unsigned mario_texture_count = {len(model.textures)};']
    (output / 'mario_mesh.c').write_text('\n'.join(lines) + '\n')
    (output / 'mario-bind-pose.obj').write_text('\n'.join(obj) + '\n')
    report = {'resource_sha256': EXPECTED, 'pose': 'bind pose (no animation)',
              'triangles': len(model.triangles), 'vertices_expanded': len(positions),
              'bounds': [lower.tolist(), upper.tolist()], 'normalization_center': center.tolist(),
              'normalization_scale': scale, 'joints': model.joints, 'opcodes': model.opcodes,
              'textures': [{k:v for k,v in t.items() if k != 'pixels'} for t in model.textures],
              'limitations': ['RDP combiner approximated with texture modulation', 'Approximate material colors and fixed lighting',
                              'No animation, physics or gameplay in this viewer']}
    (output / 'conversion-info.json').write_text(json.dumps(report, indent=2))
    from mario_animation import export_clips
    export_clips(ROOT, model, center, scale, output)
    print(f'{len(model.joints)} joints, {len(model.triangles)} triangles; {output}')


if __name__ == '__main__':
    main()
