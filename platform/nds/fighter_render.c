#include <nds.h>
#include "fighter_render.h"
#include "skeleton_sample.h"

#define MAX_ROW (40*SKELETON_CHANNELS)
static int16_t row[MAX_ROW],old_row[MAX_ROW];

static int texture_size(unsigned size)
{
    int result=0;
    for(unsigned n=8;n<size;n*=2)++result;
    return result;
}

int fighter_render_init(FighterRender *r,const FighterModel *m)
{
    if(m->texture_count>FIGHTER_MAX_TEXTURES || (m->joint_count+1)*SKELETON_CHANNELS>MAX_ROW || m->slot_count>30)return 0;
    r->model=m;
    if(m->texture_count && !glGenTextures(m->texture_count,r->textures))return 0;
    for(unsigned i=0;i<m->texture_count;++i) {
        const FighterTexture *t=&m->textures[i];
        glBindTexture(0,r->textures[i]);
        if(!glTexImage2D(0,0,GL_RGBA,texture_size(t->width),texture_size(t->height),0,t->param,t->pixels))return 0;
    }
    return 1;
}

static void decode(const FighterModel *m,unsigned clip,unsigned age,int16_t *out)
{
    const FighterClip *c=&m->clips[clip];
    skeleton_decode(c->base,c->channels,c->frames,c->varying,skeleton_tick(age,c->ticks,c->loop,c->stored),
                    (m->joint_count+1)*SKELETON_CHANNELS,out);
}

static void joint_matrix(const int16_t *q)
{
    /* Translation is model units*8; the skeleton runs in model/64 units (f32). */
    glTranslatef32(q[0]*8,q[1]*8,q[2]*8);
    glRotateZi(q[5]);glRotateYi(q[4]);glRotateXi(q[3]);
    if(q[6]!=4096 || q[7]!=4096 || q[8]!=4096)glScalef32(q[6],q[7],q[8]);
}

void fighter_render_draw(const FighterRender *r,unsigned clip,unsigned age,unsigned tint,const FighterBlend *from)
{
    const FighterModel *m=r->model;
    if(clip>=m->clip_count)return;
    unsigned count=(m->joint_count+1)*SKELETON_CHANNELS;
    decode(m,clip,age,row);
    if(from && from->clip<m->clip_count && from->weight<256) {
        decode(m,from->clip,from->tick,old_row);skeleton_blend(old_row,row,count,from->weight);
    }
    glScalef32(m->scale,m->scale,m->scale);
    joint_matrix(row+m->joint_count*SKELETON_CHANNELS); /* TransN/XRotN prefix */
    for(unsigned j=0;j<m->joint_count;++j) {
        const FighterJoint *k=&m->joints[j];
        if(k->skip)continue;
        if(k->restore>=0)glRestoreMatrix(k->restore);
        joint_matrix(row+j*SKELETON_CHANNELS);
        if(k->store>=0)glStoreMatrix(k->store);
    }
    int previous=-2,slot=-1;
    glPolyFmt(POLY_ALPHA(31)|POLY_CULL_NONE);
    for(unsigned i=0;i<m->vertex_count;i+=3) {
        int texture=m->triangle_textures[i/3];
        if(texture!=previous) {
            if(previous!=-2)glEnd();
            /* GL_TEXTURE_2D is frame-wide on DS: texture 0 draws untextured. */
            glBindTexture(0,texture<0?0:r->textures[texture]);
            previous=texture;glBegin(GL_TRIANGLE);
        }
        for(unsigned j=0;j<3;++j) {
            const FighterVertex *v=&m->vertices[i+j];
            const FighterLocalVertex *p=&m->locals[m->local_map[i+j]];
            unsigned color=v->color;
            if(tint!=0x7fff) {
                unsigned red=(color&31)*(tint&31)/31,green=((color>>5)&31)*((tint>>5)&31)/31;
                unsigned blue=((color>>10)&31)*((tint>>10)&31)/31;
                color=RGB15(red,green,blue);
            }
            glColor(color);glTexCoord2t16(v->u,v->v);
            /* Rigid skinning: each vertex is submitted under its joint's matrix. */
            if(p->slot!=slot) {slot=p->slot;glRestoreMatrix(slot);}
            glVertex3v16(p->x,p->y,p->z);
        }
    }
    if(previous!=-2)glEnd();
}
