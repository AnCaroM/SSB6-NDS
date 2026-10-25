#include <nds.h>
#include "mario_mesh.h"
#include "mario_render.h"
#include "pose_sample.h"
#include "skeleton_sample.h"

static int textures[8],fireball_texture;
#ifdef MARIO_SKELETAL
/* Matrix stack slots. Callers' glPushMatrix uses the low slots (actor pushes
 * once, this renderer once more); keep caller nesting at depth <= 2. */
#define SLOT_BASE 3
#define SLOT_PREFIX 4
#define SLOT_JOINT 5
#define SKELETON_ROW (26*SKELETON_CHANNELS)
static int16_t row[SKELETON_ROW],old_row[SKELETON_ROW];
#else
/* One shared scratch pose; integer interpolation restores intervening ticks. */
static int16_t blended[189*3];
#endif

static int texture_size(unsigned size)
{
    int result = 0;
    for (unsigned n = 8; n < size; n *= 2) ++result;
    return result;
}

int mario_render_init(void)
{
    if (mario_texture_count > sizeof textures / sizeof *textures || mario_pose_vertex_count>189) return 0;
#ifdef MARIO_SKELETAL
    if (mario_joint_count+SLOT_JOINT>31) return 0;
#endif
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

/* positions: animated vertex bank pose; skeletal: joint-local vertices under
 * the joint matrices stored by build_skeleton(). Neither: bind pose. */
static void draw(const int16_t *positions, unsigned tint, int skeletal)
{
#ifdef MARIO_SKELETAL
    int joint=-1;
#else
    (void)skeletal;
#endif
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
#ifdef MARIO_SKELETAL
            if (skeletal) {
                const MarioSkelVertex *k = &mario_skel_vertices[mario_pose_vertex_map[i+j]];
                /* Matrix restores between vertices give rigid per-joint skinning. */
                if (k->joint != joint) { joint = k->joint; glRestoreMatrix(SLOT_JOINT + joint); }
                glVertex3v16(k->x, k->y, k->z);
            } else
#endif
            if (positions) {
                const int16_t *p = positions + mario_pose_vertex_map[i+j]*3;
                glVertex3v16(p[0], p[1], p[2]);
            } else glVertex3v16(v->x, v->y, v->z);
        }
    }
    if(previous!=-2)glEnd();
}

void mario_render_draw(void) { draw(NULL,0x7fff,0); }

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
{mario_render_draw_pose_mix(clip,frame,tint,NULL);}

#ifdef MARIO_SKELETAL
static void joint_matrix(const int16_t *q)
{
    /* Translation is model units*8; the skeleton runs in model/64 units (f32). */
    glTranslatef32(q[0]*8,q[1]*8,q[2]*8);
    glRotateZi(q[5]);glRotateYi(q[4]);glRotateXi(q[3]);
    if(q[6]!=4096 || q[7]!=4096 || q[8]!=4096)glScalef32(q[6],q[7],q[8]);
}

static void build_skeleton(const int16_t *q)
{
    glScalef32(mario_skel_scale,mario_skel_scale,mario_skel_scale);
    glTranslatef32(mario_skel_offset[0],mario_skel_offset[1],mario_skel_offset[2]);
    glStoreMatrix(SLOT_BASE);
    joint_matrix(q+mario_joint_count*SKELETON_CHANNELS);glStoreMatrix(SLOT_PREFIX);
    for(unsigned j=0;j<mario_joint_count;++j) {
        int parent=mario_joint_parent[j];
        glRestoreMatrix(parent<0?SLOT_PREFIX:SLOT_JOINT+parent);
        joint_matrix(q+j*SKELETON_CHANNELS);glStoreMatrix(SLOT_JOINT+j);
    }
}

static void decode(unsigned clip,unsigned age,int16_t *out)
{
    const MarioClip *c=&mario_clips[clip];const MarioSkelClip *k=&mario_skel_clips[clip];
    skeleton_decode(k->base,k->channels,k->frames,k->varying,
                    skeleton_tick(age,c->ticks,c->loop,k->ticks),SKELETON_ROW,out);
}

void mario_render_draw_pose_mix(unsigned clip,unsigned frame,unsigned tint,const MarioPoseBlend *from)
{
    if (clip >= mario_clip_count) { draw(NULL,tint,0); return; }
    decode(clip,frame,row);
    if(from && from->clip<mario_clip_count && from->weight<256) {
        decode(from->clip,from->tick,old_row);
        skeleton_blend(old_row,row,SKELETON_ROW,from->weight);
    }
    glPushMatrix();build_skeleton(row);draw(NULL,tint,1);glPopMatrix(1);
}
#else
static int16_t coordinate(const MarioClip *animation,PoseSample p,unsigned i)
{
    const MarioPose *a=animation->positions+p.a*mario_pose_vertex_count*3;
    const MarioPose *b=animation->positions+p.b*mario_pose_vertex_count*3;
#ifdef MARIO_COMPACT_POSES
    return pose_blend(pose_decode(a[i],animation->low[i%3],animation->span[i%3]),
                      pose_decode(b[i],animation->low[i%3],animation->span[i%3]),p);
#else
    return pose_blend(a[i],b[i],p);
#endif
}

void mario_render_draw_pose_mix(unsigned clip,unsigned frame,unsigned tint,const MarioPoseBlend *from)
{
    if (clip >= mario_clip_count) { draw(NULL,tint,0); return; }
    const MarioClip *animation = &mario_clips[clip];
    PoseSample p=pose_sample(frame,animation->ticks,animation->frame_step,animation->frames,animation->loop);
    const MarioPose *a=animation->positions+p.a*mario_pose_vertex_count*3;
#ifndef MARIO_COMPACT_POSES
    if(!from && (!p.part || p.b==p.a)) {draw(a,tint,0);return;}
#endif
    const MarioPose *b=animation->positions+p.b*mario_pose_vertex_count*3;
    for(unsigned i=0;i<mario_pose_vertex_count*3;++i)
#ifdef MARIO_COMPACT_POSES
        blended[i]=pose_blend(pose_decode(a[i],animation->low[i%3],animation->span[i%3]),
                             pose_decode(b[i],animation->low[i%3],animation->span[i%3]),p);
#else
        blended[i]=pose_blend(a[i],b[i],p);
#endif
    if(from && from->clip<mario_clip_count && from->weight<256) {
        const MarioClip *old=&mario_clips[from->clip];
        PoseSample sample=pose_sample(from->tick,old->ticks,old->frame_step,old->frames,old->loop);
        PoseSample mix={0,0,from->weight,256};
        for(unsigned i=0;i<mario_pose_vertex_count*3;++i)
            blended[i]=pose_blend(coordinate(old,sample,i),blended[i],mix);
    }
    draw(blended,tint,0);
}
#endif
