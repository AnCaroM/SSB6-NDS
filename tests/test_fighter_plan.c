/* Generic fighter data: the DS joint plan (restore/store/skip over matrix-stack
 * slots) must give the same joint matrices as plain parent-recursive FK, and
 * every vertex must reference a stored slot. Uses build/fighters/<f>. */
#include "fighter_model.h"
#include "skeleton_sample.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
extern const FighterModel link_model;
typedef struct {double m[3][4];} Mat;
static Mat mul(Mat a,Mat b)
{
    Mat r;
    for(int i=0;i<3;++i)for(int j=0;j<4;++j) {
        r.m[i][j]=j==3?a.m[i][3]:0;for(int k=0;k<3;++k)r.m[i][j]+=a.m[i][k]*b.m[k][j];
    }
    return r;
}
static Mat joint(const int16_t *q)
{
    double a[3];for(int i=0;i<3;++i)a[i]=q[3+i]*2*M_PI/SKELETON_TURN;
    double cx=cos(a[0]),sx=sin(a[0]),cy=cos(a[1]),sy=sin(a[1]),cz=cos(a[2]),sz=sin(a[2]);
    Mat t={{{1,0,0,q[0]/8.0},{0,1,0,q[1]/8.0},{0,0,1,q[2]/8.0}}};
    Mat z={{{cz,-sz,0,0},{sz,cz,0,0},{0,0,1,0}}},y={{{cy,0,sy,0},{0,1,0,0},{-sy,0,cy,0}}};
    Mat x={{{1,0,0,0},{0,cx,-sx,0},{0,sx,cx,0}}},s={{{q[6]/4096.0,0,0,0},{0,q[7]/4096.0,0,0},{0,0,q[8]/4096.0,0}}};
    return mul(mul(mul(mul(t,z),y),x),s);
}
static unsigned check(const FighterModel *m)
{
    static int16_t row[41*SKELETON_CHANNELS];Mat slots[32],world[40];unsigned checked=0;
    assert(m->joint_count<=40 && m->slot_count<=30);
    for(unsigned i=0;i<m->vertex_count;++i) {
        int slot=m->locals[m->local_map[i]].slot;assert(slot>=1 && slot<=(int)m->slot_count);
    }
    for(unsigned c=0;c<m->clip_count;++c) {
        const FighterClip *clip=&m->clips[c];
        for(unsigned t=0;t<clip->stored;++t) {
            skeleton_decode(clip->base,clip->channels,clip->frames,clip->varying,t,(m->joint_count+1)*SKELETON_CHANNELS,row);
            Mat prefix=joint(row+m->joint_count*SKELETON_CHANNELS),current=prefix;
            for(unsigned j=0;j<m->joint_count;++j) {
                int p=m->parents[j];world[j]=mul(p<0?prefix:world[p],joint(row+j*SKELETON_CHANNELS));
                const FighterJoint *k=&m->joints[j];if(k->skip)continue;
                if(k->restore>=0)current=slots[k->restore];
                current=mul(current,joint(row+j*SKELETON_CHANNELS));
                for(int a=0;a<3;++a)for(int b=0;b<4;++b)assert(fabs(current.m[a][b]-world[j].m[a][b])<1e-6);
                if(k->store>=0)slots[k->store]=current;
            }
            ++checked;
        }
    }
    return checked;
}
int main(void)
{
    unsigned link=check(&link_model);
    assert(!strcmp(link_model.name,"Link") && link_model.joint_count==32 && link_model.clip_count>=60);
    printf("PASS: fighter joint plan = parent FK for Link (%u poses, %u slots)\n",link,link_model.slot_count);
    return 0;
}
