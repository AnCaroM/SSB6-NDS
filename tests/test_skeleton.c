/* Bone-pose bank vs the 16-bit vertex bank, using DS-like 20.12 fixed point:
 * decode -> per-joint T*Rz*Ry*Rx*S with 4.12 trig -> joint-local v16 vertices. */
#include "mario_mesh.h"
#include "skeleton_sample.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
typedef struct {int64_t m[3][4];} Mat; /* rows of a 3x4 affine matrix, 20.12 */
static int32_t fx_sin(int angle)
{return (int32_t)lround(sin(angle*2*M_PI/SKELETON_TURN)*4096);}
static Mat mul(Mat a,Mat b)
{
    Mat r;
    for(int i=0;i<3;++i)for(int j=0;j<4;++j) {
        int64_t v=0;for(int k=0;k<3;++k)v+=a.m[i][k]*b.m[k][j];
        r.m[i][j]=(v>>12)+(j==3?a.m[i][3]:0);
    }
    return r;
}
static Mat ident(void) {Mat r={{{4096,0,0,0},{0,4096,0,0},{0,0,4096,0}}};return r;}
static Mat joint(const int16_t *q)
{
    int32_t cx=fx_sin(q[3]+SKELETON_TURN/4),sx=fx_sin(q[3]),cy=fx_sin(q[4]+SKELETON_TURN/4),sy=fx_sin(q[4]);
    int32_t cz=fx_sin(q[5]+SKELETON_TURN/4),sz=fx_sin(q[5]);
    Mat t=ident();t.m[0][3]=q[0]*8;t.m[1][3]=q[1]*8;t.m[2][3]=q[2]*8;
    Mat z={{{cz,-sz,0,0},{sz,cz,0,0},{0,0,4096,0}}},y={{{cy,0,sy,0},{0,4096,0,0},{-sy,0,cy,0}}};
    Mat x={{{4096,0,0,0},{0,cx,-sx,0},{0,sx,cx,0}}},s={{{q[6],0,0,0},{0,q[7],0,0},{0,0,q[8],0}}};
    return mul(mul(mul(mul(t,z),y),x),s);
}
int main(void)
{
    static int16_t row[26*SKELETON_CHANNELS],from[26*SKELETON_CHANNELS];
    const unsigned count=(mario_joint_count+1)*SKELETON_CHANNELS;
    assert(mario_joint_count==25 && count<=sizeof row/sizeof *row);
    double worst=0;unsigned checked=0;
    for(unsigned c=0;c<mario_clip_count;++c) {
        const MarioClip *clip=&mario_clips[c];const MarioSkelClip *k=&mario_skel_clips[c];
        assert(k->ticks==clip->ticks || k->ticks==1);
        for(unsigned f=0;f<clip->frames;++f) {
            unsigned tick=f*clip->frame_step;if(tick>=clip->ticks)tick=clip->ticks-1;
            skeleton_decode(k->base,k->channels,k->frames,k->varying,
                            skeleton_tick(tick,clip->ticks,clip->loop,k->ticks),count,row);
            Mat base=ident();
            for(int i=0;i<3;++i) {base.m[i][i]=mario_skel_scale;base.m[i][3]=0;}
            Mat offset=ident();for(int i=0;i<3;++i)offset.m[i][3]=mario_skel_offset[i];
            base=mul(base,offset);
            Mat world[25],prefix=mul(base,joint(row+25*SKELETON_CHANNELS));
            for(unsigned j=0;j<mario_joint_count;++j) {
                int parent=mario_joint_parent[j];assert(parent<(int)j);
                world[j]=mul(parent<0?prefix:world[parent],joint(row+j*SKELETON_CHANNELS));
            }
            for(unsigned v=0;v<mario_pose_vertex_count;++v) {
                const MarioSkelVertex *s=&mario_skel_vertices[v];const Mat *m=&world[s->joint];
                for(int axis=0;axis<3;++axis) {
                    int64_t p=((m->m[axis][0]*s->x+m->m[axis][1]*s->y+m->m[axis][2]*s->z)>>12)+m->m[axis][3];
                    int16_t expect=clip->positions[(f*mario_pose_vertex_count+v)*3+axis];
                    double error=fabs((double)p-expect)/4096*MARIO_RESTORE_SCALE;
                    if(error>worst)worst=error;
                }
            }
            ++checked;
        }
    }
    /* Float-matrix check gives ~0.35 units; chained 20.12 matrices with 4.12
     * trig (as on the DS geometry engine) stay within the 2.1 project bound. */
    assert(worst<2.1);
    /* Blend: endpoints exact, angles wrap the short way, translation linear. */
    for(unsigned i=0;i<count;++i) {from[i]=(int16_t)(i*37);row[i]=(int16_t)(i*37+100);}
    from[3]=16000;row[3]=-16000;
    int16_t keep[26*SKELETON_CHANNELS];for(unsigned i=0;i<count;++i)keep[i]=row[i];
    skeleton_blend(from,row,count,256); /* Same angle, possibly another turn. */
    for(unsigned i=0;i<count;++i)assert((row[i]-keep[i])%SKELETON_TURN==0);
    for(unsigned i=0;i<count;++i)row[i]=keep[i];
    skeleton_blend(from,row,count,0);for(unsigned i=0;i<count;++i)assert(row[i]==from[i]);
    for(unsigned i=0;i<count;++i)row[i]=keep[i];
    skeleton_blend(from,row,count,128);
    assert(row[3]==16000+384); /* 16000 -> -16000 is +768 through the wrap. */
    assert(row[0]==from[0]+50);
    printf("PASS: bone-pose bank matches 16-bit vertex bank in fixed point (%u poses, max %.2f units), blend wrap\n",checked,worst);
    return 0;
}
