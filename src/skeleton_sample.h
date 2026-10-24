#ifndef SSB_SKELETON_SAMPLE_H
#define SSB_SKELETON_SAMPLE_H
#include <stdint.h>
#include <string.h>
/* Bone-pose decode shared by the DS renderer and host tests. A clip stores a
 * base row of joint channels plus, per tick, only the channels that change.
 * Channel c of joint j is row[j*9+c]: tx,ty,tz,rx,ry,rz,sx,sy,sz. */
#define SKELETON_CHANNELS 9
#define SKELETON_TURN 32768

static inline unsigned skeleton_tick(unsigned age,unsigned ticks,int loop,unsigned stored)
{
    unsigned tick=loop?age%ticks:(age<ticks?age:ticks-1);
    return tick<stored?tick:stored-1;
}

static inline void skeleton_decode(const int16_t *base,const uint16_t *channels,const int16_t *frames,
                                   unsigned varying,unsigned tick,unsigned count,int16_t *row)
{
    memcpy(row,base,count*sizeof *row);
    if(!varying)return;
    const int16_t *values=frames+tick*varying;
    for(unsigned i=0;i<varying;++i)row[channels[i]]=values[i];
}

/* weight 0 keeps `from`, 256 reaches `to`; rotations take the short way round. */
static inline void skeleton_blend(const int16_t *from,int16_t *to,unsigned count,unsigned weight)
{
    for(unsigned i=0;i<count;++i) {
        int a=from[i],d=to[i]-a,c=i%SKELETON_CHANNELS;
        if(c>=3 && c<6) {
            d%=SKELETON_TURN;
            if(d>=SKELETON_TURN/2)d-=SKELETON_TURN;
            if(d<-SKELETON_TURN/2)d+=SKELETON_TURN;
        }
        to[i]=(int16_t)(a+d*(int)weight/256);
    }
}
#endif
