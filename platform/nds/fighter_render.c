#include <nds.h>
#include <stdlib.h>
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

static unsigned tinted(unsigned color,unsigned tint)
{
    if(tint==0x7fff)return color;
    return RGB15((color&31)*(tint&31)/31,((color>>5)&31)*((tint>>5)&31)/31,((color>>10)&31)*((tint>>10)&31)/31);
}

/* Packed GX command list writer (GBATEK "DS 3D Geometry Commands"): four
 * command ids per header word, then their parameters. With out==NULL it only
 * counts words, so each list is sized exactly before its single allocation. */
#define FIFO_MTX_RESTORE REG2ID(MATRIX_RESTORE)
typedef struct {uint32_t *out;unsigned words,n,np;uint8_t ids[4];uint32_t params[8];} Packer;
static void pack_flush(Packer *p)
{
    if(!p->n)return;
    while(p->n<4)p->ids[p->n++]=FIFO_NOP;
    if(p->out) {
        p->out[p->words]=FIFO_COMMAND_PACK(p->ids[0],p->ids[1],p->ids[2],p->ids[3]);
        for(unsigned i=0;i<p->np;++i)p->out[p->words+1+i]=p->params[i];
    }
    p->words+=1+p->np;p->n=p->np=0;
}
static void pack(Packer *p,uint8_t id,uint32_t a,uint32_t b,unsigned count)
{
    p->ids[p->n++]=id;
    if(count>0)p->params[p->np++]=a;
    if(count>1)p->params[p->np++]=b;
    if(p->n==4)pack_flush(p);
}
/* One part bucket (32 = triangles not tied to a model part), in model order:
 * texture switches, matrix-slot restores and per-vertex colour/uv/position. */
static unsigned bucket_list(const FighterRender *r,unsigned bucket,unsigned tint,uint32_t *out)
{
    const FighterModel *m=r->model;Packer p={out?out+1:NULL,0,0,0,{0},{0}};
    int previous=-2,slot=-1;
    for(unsigned i=0;i<m->vertex_count;i+=3) {
        unsigned part=m->triangle_parts[i/3];
        if((part==255?32u:part)!=bucket)continue;
        int texture=m->triangle_textures[i/3];
        if(texture!=previous) {
            pack(&p,FIFO_TEX_FORMAT,texture<0?0:r->texture_params[texture],0,1);
            pack(&p,FIFO_BEGIN,GL_TRIANGLE,0,1);previous=texture;
        }
        for(unsigned j=0;j<3;++j) {
            const FighterVertex *v=&m->vertices[i+j];
            const FighterLocalVertex *q=&m->locals[m->local_map[i+j]];
            if(q->slot!=slot) {slot=q->slot;pack(&p,FIFO_MTX_RESTORE,(uint32_t)slot,0,1);}
            pack(&p,FIFO_COLOR,tinted(v->color,tint),0,1);
            pack(&p,FIFO_TEX_COORD,(uint16_t)v->u|(uint32_t)(uint16_t)v->v<<16,0,1);
            pack(&p,FIFO_VERTEX16,(uint16_t)q->x|(uint32_t)(uint16_t)q->y<<16,(uint16_t)q->z,2);
        }
    }
    pack_flush(&p);
    if(out)out[0]=p.words;
    return p.words;
}

int fighter_render_load(FighterRender *r,const FighterModel *m,const unsigned *tints,unsigned tint_count)
{
    if(m->texture_count>FIGHTER_MAX_TEXTURES || (m->joint_count+1)*SKELETON_CHANNELS>MAX_ROW || m->slot_count>30 ||
       tint_count>FIGHTER_LIST_TINTS)return 0;
    r->model=m;r->loaded=0;
    if(m->texture_count && !glGenTextures(m->texture_count,r->textures))return 0;
    r->loaded=1;
    for(unsigned i=0;i<m->texture_count;++i) {
        const FighterTexture *t=&m->textures[i];
        glBindTexture(0,r->textures[i]);
        if(!glTexImage2D(0,0,GL_RGBA,texture_size(t->width),texture_size(t->height),0,t->param,t->pixels)) {
            fighter_render_unload(r);return 0;
        }
        r->texture_params[i]=glGetTexParameter();
    }
    /* Prebuilt lists: per tint, one per model part. Only the joint matrices
     * change per frame, so a draw is a few DMA transfers to the GX FIFO. */
    for(unsigned k=0;k<tint_count;++k) {
        r->tints[k]=tints[k];
        for(unsigned b=0;b<FIGHTER_BUCKETS;++b) {
            unsigned words=bucket_list(r,b,tints[k],NULL);
            if(!words) {r->lists[k][b]=NULL;continue;}
            uint32_t *list=malloc((words+1)*4);
            if(!list) {fighter_render_unload(r);return 0;}
            bucket_list(r,b,tints[k],list);DC_FlushRange(list,(words+1)*4);r->lists[k][b]=list;
        }
    }
    r->tint_count=tint_count;
    return 1;
}

void fighter_render_unload(FighterRender *r)
{
    for(unsigned k=0;k<FIGHTER_LIST_TINTS;++k)for(unsigned b=0;b<FIGHTER_BUCKETS;++b) {
        free(r->lists[k][b]);r->lists[k][b]=NULL;
    }
    if(r->loaded && r->model && r->model->texture_count)glDeleteTextures(r->model->texture_count,r->textures);
    r->loaded=0;r->tint_count=0;
}

/* glCallList without its per-call cache flush (lists are flushed once when
 * built): DMA the packed words into the geometry FIFO. */
static void call_list(const uint32_t *list)
{
    while((DMA_CR(0)&DMA_BUSY)||(DMA_CR(1)&DMA_BUSY)||(DMA_CR(2)&DMA_BUSY)||(DMA_CR(3)&DMA_BUSY));
    DMA_SRC(0)=(u32)(list+1);DMA_DEST(0)=0x4000400;DMA_CR(0)=DMA_FIFO|list[0];
    while(DMA_CR(0)&DMA_BUSY);
}

static void decode(const FighterModel *m,unsigned clip,unsigned age,int16_t *out)
{
    const FighterClip *c=&m->clips[clip];
    skeleton_decode(c->base,c->channels,c->frames,c->varying,skeleton_tick(age,c->ticks,c->loop,c->stored),
                    (m->joint_count+1)*SKELETON_CHANNELS,out);
}

static void joint_matrix(const int16_t *q,int32_t translate)
{
    /* Translation is model units*8; the skeleton runs in model/divisor units. */
    glTranslatef32(q[0]*translate,q[1]*translate,q[2]*translate);
    glRotateZi(q[5]);glRotateYi(q[4]);glRotateXi(q[3]);
    if(q[6]!=4096 || q[7]!=4096 || q[8]!=4096)glScalef32(q[6],q[7],q[8]);
}

/* Visible model parts at this tick: last (tick, mask) change not after it. */
static uint32_t part_mask(const FighterModel *m,const FighterClip *c,unsigned age)
{
    uint32_t mask=m->base_parts;
    unsigned tick=skeleton_tick(age,c->ticks,c->loop,c->ticks);
    for(unsigned i=0;i<c->part_changes && c->parts[i*2]<=tick;++i)mask=c->parts[i*2+1];
    return mask;
}

void fighter_render_draw(const FighterRender *r,unsigned clip,unsigned age,unsigned tint,const FighterBlend *from,unsigned flags)
{
    const FighterModel *m=r->model;
    if(clip>=m->clip_count)return;
    unsigned count=(m->joint_count+1)*SKELETON_CHANNELS;
    decode(m,clip,age,row);
    if(from && from->clip<m->clip_count && from->weight<256) {
        decode(m,from->clip,from->tick,old_row);skeleton_blend(old_row,row,count,from->weight);
    }
    const FighterClip *c=&m->clips[clip];
    uint32_t mask=(part_mask(m,c,age)|r->part_set)&~r->part_clear;
    glScalef32(m->scale,m->scale,m->scale);
    /* TransN/XRotN inserted joint; gameplay owns the movement-only TransN. */
    if(c->root!=2 || (flags&FIGHTER_DRAW_ROOT_MOTION))joint_matrix(row+m->joint_count*SKELETON_CHANNELS,m->translate);
    for(unsigned j=0;j<m->joint_count;++j) {
        const FighterJoint *k=&m->joints[j];
        if(k->skip)continue;
        if(k->restore>=0)glRestoreMatrix(k->restore);
        joint_matrix(row+j*SKELETON_CHANNELS,m->translate);
        if(k->store>=0)glStoreMatrix(k->store);
    }
    glPolyFmt(POLY_ALPHA(31)|POLY_CULL_NONE);
    for(unsigned k=0;k<r->tint_count;++k)if(r->tints[k]==tint) {
        for(unsigned b=0;b<FIGHTER_BUCKETS;++b)if(r->lists[k][b] && (b==32 || (mask>>b&1)))call_list(r->lists[k][b]);
        /* The lists set TEXIMAGE_PARAM behind libnds's back: leave hardware and
         * libnds agreeing on one bound texture so later glBindTexture calls apply. */
        if(m->texture_count) {glBindTexture(0,r->textures[0]);GFX_TEX_FORMAT=r->texture_params[0];}
        return;
    }
    /* Uncached tint: immediate submission (same commands, CPU writes). */
    int previous=-2,slot=-1;
    for(unsigned i=0;i<m->vertex_count;i+=3) {
        unsigned part=m->triangle_parts[i/3];
        if(part!=255 && !(mask>>part&1))continue;
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
            glColor(tinted(v->color,tint));glTexCoord2t16(v->u,v->v);
            /* Rigid skinning: each vertex is submitted under its joint's matrix. */
            if(p->slot!=slot) {slot=p->slot;glRestoreMatrix(slot);}
            glVertex3v16(p->x,p->y,p->z);
        }
    }
    if(previous!=-2)glEnd();
}

void fighter_render_draw_part(const FighterRender *r,unsigned part,unsigned tint)
{
    const FighterModel *m=r->model;int previous=-2;
    int32_t sum[3]={0,0,0},count=0;
    for(unsigned i=0;i<m->vertex_count;++i)if(m->triangle_parts[i/3]==part) {
        const FighterLocalVertex *p=&m->locals[m->local_map[i]];sum[0]+=p->x;sum[1]+=p->y;sum[2]+=p->z;++count;
    }
    if(!count)return;
    glScalef32(m->scale,m->scale,m->scale);
    glTranslatef32(-sum[0]/count,-sum[1]/count,-sum[2]/count);
    glPolyFmt(POLY_ALPHA(31)|POLY_CULL_NONE);
    for(unsigned i=0;i<m->vertex_count;i+=3) {
        if(m->triangle_parts[i/3]!=part)continue;
        int texture=m->triangle_textures[i/3];
        if(texture!=previous) {
            if(previous!=-2)glEnd();
            glBindTexture(0,texture<0?0:r->textures[texture]);
            previous=texture;glBegin(GL_TRIANGLE);
        }
        for(unsigned j=0;j<3;++j) {
            const FighterVertex *v=&m->vertices[i+j];const FighterLocalVertex *p=&m->locals[m->local_map[i+j]];
            glColor(tinted(v->color,tint));glTexCoord2t16(v->u,v->v);glVertex3v16(p->x,p->y,p->z);
        }
    }
    if(previous!=-2)glEnd();
}
