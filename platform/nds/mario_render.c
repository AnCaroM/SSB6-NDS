#include <nds.h>
#include "mario_mesh.h"
#include "mario_render.h"
#include "pose_sample.h"

static int textures[8],fireball_texture;
/* One shared scratch pose; integer interpolation restores intervening ticks. */
static int16_t blended[189*3];

static int texture_size(unsigned size)
{
    int result = 0;
    for (unsigned n = 8; n < size; n *= 2) ++result;
    return result;
}

int mario_render_init(void)
{
    if (mario_texture_count > sizeof textures / sizeof *textures || mario_pose_vertex_count>189) return 0;
    vramSetBankA(VRAM_A_TEXTURE);
    glGenTextures(mario_texture_count, textures);
    for (unsigned i = 0; i < mario_texture_count; ++i) {
        const MarioTexture *t = &mario_textures[i];
        glBindTexture(0, textures[i]);
        if (!glTexImage2D(0, 0, GL_RGBA, texture_size(t->width), texture_size(t->height),
                          0, t->param, t->pixels)) return 0;
    }
    glEnable(GL_TEXTURE_2D);
    if(!glGenTextures(1,&fireball_texture))return 0;
    glBindTexture(0,fireball_texture);
    if(!glTexImage2D(0,0,GL_RGBA,TEXTURE_SIZE_16,TEXTURE_SIZE_16,0,0,mario_fireball_pixels))return 0;
    return 1;
}

static void draw(const int16_t *positions, unsigned tint)
{
    int previous = -2;
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);
    for (unsigned i = 0; i < mario_vertex_count; i += 3) {
        int texture = mario_triangle_textures[i / 3];
        if (texture != previous) {
            if(previous!=-2)glEnd();
            /* GL_TEXTURE_2D controls the whole rendered frame on DS. Use an
             * empty texture format for individual untextured polygons. */
            if (texture < 0) glBindTexture(0, 0);
            else {
                glBindTexture(0, textures[texture]);
            }
            previous = texture;
            glBegin(GL_TRIANGLE);
        }
        for (unsigned j = 0; j < 3; ++j) {
            const MarioVertex *v = &mario_vertices[i+j];
            unsigned color = v->color;
            if (tint != 0x7fff) {
                unsigned r = (color & 31)*(tint & 31)/31;
                unsigned g = ((color >> 5) & 31)*((tint >> 5) & 31)/31;
                unsigned b = ((color >> 10) & 31)*((tint >> 10) & 31)/31;
                color = RGB15(r,g,b);
            }
            glColor(color);
            glTexCoord2t16(v->u, v->v);
            if (positions) {
                const int16_t *p = positions + mario_pose_vertex_map[i+j]*3;
                glVertex3v16(p[0], p[1], p[2]);
            } else glVertex3v16(v->x, v->y, v->z);
        }
    }
    if(previous!=-2)glEnd();
}

void mario_render_draw(void) { draw(NULL,0x7fff); }

void mario_render_fireball(float x,float y,float unit,unsigned age)
{
    glPushMatrix();glTranslatef32(floattof32(x/unit),floattof32(y/unit),0);
    glRotateZi((age*1820)&32767);
    glBindTexture(0,fireball_texture);glColor(RGB15(31,31,31));
    glPolyFmt(POLY_ALPHA(31)|POLY_CULL_NONE|POLY_ID(4));
    int r=floattov16(100/unit);
    glBegin(GL_QUADS);
    glTexCoord2t16(0,16<<4);glVertex3v16(-r,-r,0);
    glTexCoord2t16(16<<4,16<<4);glVertex3v16(r,-r,0);
    glTexCoord2t16(16<<4,0);glVertex3v16(r,r,0);
    glTexCoord2t16(0,0);glVertex3v16(-r,r,0);glEnd();glPopMatrix(1);
}

void mario_render_draw_pose(unsigned clip, unsigned frame)
{
    mario_render_draw_pose_tint(clip,frame,0x7fff);
}

void mario_render_draw_pose_tint(unsigned clip, unsigned frame, unsigned tint)
{
    if (clip >= mario_clip_count) { draw(NULL,tint); return; }
    const MarioClip *animation = &mario_clips[clip];
    PoseSample p=pose_sample(frame,animation->ticks,animation->frame_step,animation->frames,animation->loop);
    const MarioPose *a=animation->positions+p.a*mario_pose_vertex_count*3;
#ifndef MARIO_COMPACT_POSES
    if(!p.part || p.b==p.a) {draw(a,tint);return;}
#endif
    const MarioPose *b=animation->positions+p.b*mario_pose_vertex_count*3;
    for(unsigned i=0;i<mario_pose_vertex_count*3;++i)
#ifdef MARIO_COMPACT_POSES
        blended[i]=pose_blend(pose_decode(a[i],animation->low[i%3],animation->span[i%3]),
                             pose_decode(b[i],animation->low[i%3],animation->span[i%3]),p);
#else
        blended[i]=pose_blend(a[i],b[i],p);
#endif
    draw(blended,tint);
}
