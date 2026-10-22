"""Bounded static F3DEX2 conversion of the original US Dream Land stage.

Geometry, textures and platform coordinates come from the verified ROM. The
RDP combiner, billboard flags and stage animation are approximated/static.
"""
import hashlib
import json
from pathlib import Path
import struct
import numpy as np
from PIL import Image
from convert_mario_model import Model, ROOT
from mario_animation import transform


class DreamLand(Model):
    def __init__(self):
        assets=ROOT/'build/assets/dreamland'
        manifest=json.loads((assets/'manifest.json').read_text())
        self.resources=manifest['resources'];self.bases={};pool=bytearray()
        for fid,asset in self.resources.items():
            data=(assets/asset['file']).read_bytes()
            if hashlib.sha256(data).hexdigest()!=asset['sha256']:raise ValueError('Asset hash mismatch')
            pool.extend(bytes((-len(pool))%16));self.bases[int(fid)]=len(pool);pool.extend(data)
        super().__init__(bytes(pool),65535)
        for fid,asset in self.resources.items():
            for offset,target in asset['pointers'].items():
                if target['file'] in self.bases:
                    self.pointers[self.bases[int(fid)]+int(offset)]=self.bases[target['file']]+target['offset']
        self.geometry=0;self.prim=np.ones(3);self.material_table=None
        self.tris=[];self.layers=[];self.unknown=set()

    def materials(self,joint):
        if self.material_table is None:return []
        dispatch=self.ptr(self.material_table+joint*4)
        if dispatch is None:return []
        result=[]
        for i in range(8):
            p=self.ptr(dispatch+i*4)
            if p is None:break
            result.append({'offset':p,'flags':struct.unpack_from('>H',self.data,p+0x30)[0]})
        return result

    def current_texture(self):
        # Stage tiles may repeat a masked texture across a larger clamped tile.
        # Base conversion already handles mirrored clamp; extend the non-mirror
        # clamped interval here after decoding the original CI4 pixels.
        index=super().current_texture()
        if index<0:return index
        t=self.textures[index]
        fmt,size,line,cms,ms,cmt,mt=self.tile
        ew=max(t['width'],self.tile_extent[0]) if cms&2 else t['width']
        eh=max(t['height'],self.tile_extent[1]) if cmt&2 else t['height']
        pw=max(8,1<<(ew-1).bit_length());ph=max(8,1<<(eh-1).bit_length())
        if pw!=t['padded_width'] or ph!=t['padded_height']:
            if pw>1024 or ph>1024:raise ValueError('Texture tile too large')
            old=t['pixels'];ow=t['padded_width'];oh=t['padded_height']
            def address(x,n,extent,mode,mask):
                x=min(x,extent-1)
                if mask:
                    period=1<<mask
                    if mode&1 and (x//period)&1:x=period-1-x%period
                    else:x%=period
                return min(x,n-1)
            t['pixels']=[old[address(y,t['height'],eh,cmt,mt)*ow+address(x,t['width'],ew,cms,ms)]
                         for y in range(ph) for x in range(pw)]
            t['padded_width']=pw;t['padded_height']=ph
        return index

    def display(self,p,matrix,joint,layer,depth=0):
        if depth>12:raise ValueError('Display-list recursion too deep')
        materials=self.materials(joint)
        for _ in range(2048):
            w0,w1=struct.unpack_from('>II',self.data,p);op=w0>>24
            self.opcodes[hex(op)]=self.opcodes.get(hex(op),0)+1
            if op==0xdf:return
            if op==0xde:
                if w1>>24==14:
                    material=materials[(w1&0xffffff)//8]
                    self.use_material(material)
                    if material['flags']&0x218:
                        self.prim=np.array(list(self.data[material['offset']+0x50:material['offset']+0x53]))/255
                else:
                    self.display(self.ptr(p+4),matrix,joint,layer,depth+1)
                    if w0&0x10000:return
            elif op==1:
                n=(w0>>12)&255;start=((w0>>1)&127)-n;v=self.ptr(p+4)
                if not 0<=start<=start+n<=32:raise ValueError('Invalid stage vertex load')
                for i in range(n):
                    xyz=struct.unpack_from('>hhh',self.data,v+i*16)
                    rgb=np.array(list(self.data[v+i*16+12:v+i*16+15]),dtype=float)
                    if self.geometry&0x20000:
                        normal=np.array(struct.unpack_from('>bbb',self.data,v+i*16+12))/127
                        rgb=np.ones(3)*255*(.45+.55*max(0,(matrix[:3,:3]@normal)[1]))
                    st=np.array(struct.unpack_from('>hh',self.data,v+i*16+8))/32*self.texture_scale
                    self.cache[start+i]=((matrix@np.array([*xyz,1]))[:3],rgb,st)
            elif op in (5,6):
                texture=self.current_texture()
                for tri in ([w0] if op==5 else [w0,w1]):
                    vertices=[]
                    for shift in (16,8,0):
                        pos,rgb,st=self.cache[(tri>>shift&255)//2]
                        vertices.append((pos.copy(),rgb.copy(),st-self.tile_origin))
                    self.tris.append((texture,vertices));self.layers.append(layer)
            elif op==0xd9:self.geometry=(self.geometry&(w0&0xffffff))|w1
            elif op==0xd7:
                self.texture_on=bool(w0&2);self.texture_scale=np.array([w1>>16,w1&65535])/65536
            elif op==0xfd:self.image=self.ptr(p+4)
            elif op==0xf0:self.palette=self.image
            elif op==0xf3:
                if (w1>>24)&7==7:self.loaded_image=(self.image,(((w1>>12)&4095)+1)*2)
            elif op==0xf5 and (w1>>24)&7==0:
                self.tile=((w0>>21)&7,(w0>>19)&3,(w0>>9)&511,(w1>>8)&3,(w1>>4)&15,(w1>>18)&3,(w1>>14)&15)
            elif op==0xf2 and (w1>>24)&7==0:
                self.tile_origin=np.array([(w0>>12)&4095,w0&4095])/4
                self.tile_extent=tuple(int(x) for x in np.array([(w1>>12)&4095,w1&4095])/4-self.tile_origin+1)
            elif op==2:
                if (w0>>16)&255!=0x14:raise ValueError('Unexpected stage ModifyVtx')
                i=(w0&65535)//2;pos,rgb,_=self.cache[i]
                self.cache[i]=(pos,rgb,np.array(struct.unpack('>hh',struct.pack('>I',w1)))/32)
            elif op==0xfa:self.prim=np.array(list(struct.pack('>I',w1))[:3])/255
            elif op not in (0xe7,0xe2,0xe3,0xf9,0xe8,0xf5,0xe6,0xf2,0xf3,0xfc,0xfb,0xdb,0xdc):
                raise ValueError(f'Unsupported stage command {op:#x} at {p:#x}')
            p+=8
        raise ValueError('Unterminated stage display list')

    def convert(self):
        base=self.bases[104]
        for layer,(root,materials) in enumerate([(0x1008,None),(0x1ce0,None),(0x2450,0x1f50),(0x2bf8,None)]):
            self.material_table=base+materials if materials is not None else None
            stack={}
            for i in range(32):
                p=base+root+i*44;values=struct.unpack_from('>II9f',self.data,p)
                if values[0]==18:break
                level=values[0]&4095;local=transform(values[2:])
                world=stack[level-1]@local if level else local;stack[level]=world
                dl=self.ptr(p+4)
                if dl is not None:self.display(dl,world,i,layer)
            else:raise ValueError('Missing stage tree terminator')


def indexed_texture(texture):
    pixels=texture['pixels'];transparent=any(not p&32768 for p in pixels)
    colors=([0] if transparent else [])
    for p in pixels:
        if transparent and not p&32768:continue
        if p not in colors:colors.append(p)
    if len(colors)>16:raise ValueError('CI4 conversion exceeds palette')
    indices=[0 if transparent and not p&32768 else colors.index(p) for p in pixels]
    packed=[indices[i]|indices[i+1]<<4 for i in range(0,len(indices),2)]
    return packed,[p&32767 for p in colors]+[0]*(16-len(colors)),transparent


def wallpaper(stage):
    p=stage.bases[88]+0x26c88
    width,height=struct.unpack_from('>hh',stage.data,p+4)
    count=struct.unpack_from('>h',stage.data,p+40)[0]
    used_height=struct.unpack_from('>h',stage.data,p+44)[0]
    fmt,size=struct.unpack_from('>BB',stage.data,p+48)
    table=stage.ptr(p+52)
    if (fmt,size)!=(0,2):raise ValueError('Expected RGBA16 wallpaper')
    rows=[]
    for i in range(count):
        b=table+i*16;w,stride,_,_=struct.unpack_from('>4h',stage.data,b)
        data=stage.ptr(b+8);h=struct.unpack_from('>h',stage.data,b+12)[0]
        if w!=width:raise ValueError('Unexpected wallpaper strip width')
        for y in range(min(h,used_height)):
            row=struct.unpack_from('>'+str(stride)+'H',stage.data,data+y*stride*2)
            rows.append([((c>>11&31)*255//31,(c>>6&31)*255//31,(c>>1&31)*255//31) for c in row[:width]])
    if len(rows)!=height:raise ValueError(f'Wallpaper height mismatch {len(rows)}/{height}')
    image=Image.fromarray(np.array(rows,dtype=np.uint8)).resize((256,192),Image.Resampling.BILINEAR)
    quant=image.quantize(colors=256)
    indices=list(quant.getdata());indices+=indices[-256:]*64
    pal=quant.getpalette()
    palette=[(pal[i]>>3)|((pal[i+1]>>3)<<5)|((pal[i+2]>>3)<<10) for i in range(0,768,3)]
    return indices,palette,image


def main():
    stage=DreamLand();stage.convert()
    output=ROOT/'build/dreamland';output.mkdir(parents=True,exist_ok=True)
    lines=['/* Generated from the user ROM. */','#include "dreamland_mesh.h"','const DreamVertex dream_vertices[] = {']
    for texture,tri in stage.tris:
        for pos,rgb,uv in tri:
            xyz=[round(x/1024*4096) for x in pos];st=[round(x*16) for x in uv]
            if not all(-32768<=x<=32767 for x in xyz+st):raise ValueError('Stage fixed-point range exceeded')
            c=[min(31,max(0,round(x/8))) for x in rgb];color=c[0]|c[1]<<5|c[2]<<10
            lines.append('{%d,%d,%d,%d,%d,%d},'%(*xyz,color,*st))
    lines+=['};','const int16_t dream_tri_textures[] = {'+','.join(str(t) for t,_ in stage.tris)+'};',
            f'const unsigned dream_vertex_count={len(stage.tris)*3};']
    textures=[]
    for i,t in enumerate(stage.textures):
        packed,palette,transparent=indexed_texture(t)
        lines += [f'static const uint8_t tex_{i}[] __attribute__((aligned(4))) = {{'+','.join(map(str,packed))+'};',
                  f'static const uint16_t pal_{i}[] __attribute__((aligned(4))) = {{'+','.join(map(str,palette))+'};']
        textures.append((t['padded_width'],t['padded_height'],t['param']|(1<<29 if transparent else 0),len(packed)))
    lines+=['const DreamTexture dream_textures[] = {']
    for i,(w,h,flags,_) in enumerate(textures):lines.append(f'{{tex_{i},pal_{i},{w},{h},{flags}}},')
    lines+=['};',f'const unsigned dream_texture_count={len(textures)};']
    pixels,palette,image=wallpaper(stage)
    lines+=['const uint8_t dream_wallpaper[] __attribute__((aligned(4))) = {'+','.join(map(str,pixels))+'};',
            'const uint16_t dream_wallpaper_palette[] __attribute__((aligned(4))) = {'+','.join(map(str,palette))+'};']
    (output/'dreamland_mesh.c').write_text('\n'.join(lines)+'\n')
    (output/'dreamland_mesh.h').write_text('''#pragma once
#include <stdint.h>
typedef struct { int16_t x,y,z; uint16_t color; int16_t u,v; } DreamVertex;
typedef struct { const uint8_t *pixels; const uint16_t *palette; uint16_t width,height; uint32_t param; } DreamTexture;
extern const DreamVertex dream_vertices[];
extern const int16_t dream_tri_textures[];
extern const unsigned dream_vertex_count, dream_texture_count;
extern const DreamTexture dream_textures[];
extern const uint8_t dream_wallpaper[];
extern const uint16_t dream_wallpaper_palette[];
''')
    image.save(output/'wallpaper-preview.png')
    points=np.array([v[0] for _,tri in stage.tris for v in tri])
    report={'triangles':len(stage.tris),'vertices':len(stage.tris)*3,'textures':len(textures),
            'texture_bytes':sum(t[3] for t in textures),'wallpaper_bytes':len(pixels),
            'bounds':[points.min(axis=0).tolist(),points.max(axis=0).tolist()],
            'layers':{str(i):stage.layers.count(i) for i in range(4)},'opcodes':stage.opcodes,
            'limitations':['Static stage pose','Approximate RDP combiner and billboard transform','No wind, stage effects or audio yet']}
    # Independent collision extraction, in native ground-line order (top,
    # right platform, left platform, island). No values are guessed from art.
    base=stage.bases[104];surfaces=[]
    for line in (3,2,1,0):
        first,count=struct.unpack_from('>2H',stage.data,base+0x1e08+line*4)
        ids=struct.unpack_from('>'+str(count)+'H',stage.data,base+0x1dd8+first*2)
        positions=[struct.unpack_from('>hh',stage.data,base+0x1d64+i*6) for i in ids]
        if len({y for _,y in positions})!=1:raise ValueError('Expected horizontal Dream Land platforms')
        surfaces.append([min(x for x,_ in positions),max(x for x,_ in positions),positions[0][1]])
    report['platforms']=surfaces
    # Clockwise contour assembled from ground 3, ceiling 4 and wall lines 5/6.
    report['solid_contour']=[list(struct.unpack_from('>hh',stage.data,base+0x1d64+i*6))
                             for i in (5,6,7,8,9,0,1,2,3,4)]
    report['ledge_ground_flags']=struct.unpack_from('>H',stage.data,base+0x1d64+5*6+4)[0]
    # MPGroundData starts at MapHeader 0x14 in resource 255. Normal camera
    # bounds are s16 top,bottom,right,left at member offset 0x6c.
    report['camera_bounds']=list(struct.unpack_from('>4h',stage.data,stage.bases[255]+0x14+0x6c))
    report['resource_sha256']={k:v['sha256'] for k,v in stage.resources.items()}
    (output/'conversion-info.json').write_text(json.dumps(report,indent=2))
    print(json.dumps({k:v for k,v in report.items() if k!='opcodes'},indent=2))


if __name__=='__main__':main()
