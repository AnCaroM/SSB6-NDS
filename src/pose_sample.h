#ifndef SSB_POSE_SAMPLE_H
#define SSB_POSE_SAMPLE_H
#include <stdint.h>
/* Uniform offline samples plus the exact endpoint of non-looping clips. */
typedef struct {unsigned a,b,part,span;} PoseSample;
static inline PoseSample pose_sample(unsigned age,unsigned ticks,unsigned step,unsigned frames,int loop)
{
    if(loop)age%=ticks;
    else if(age>=ticks)age=ticks-1;
    PoseSample p={age/step,0,age%step,step};
    p.b=p.a+1;
    if(!loop && p.a*step+step>=ticks)p.span=ticks-1-p.a*step;
    if(p.b>=frames)p.b=loop?0:p.a;
    if(!p.span)p.part=0;
    return p;
}
static inline int16_t pose_blend(int16_t a,int16_t b,PoseSample p)
{return p.part?(int16_t)(a+((int)b-a)*(int)p.part/(int)p.span):a;}
static inline int16_t pose_decode(uint8_t value,int16_t low,uint16_t span)
{return (int16_t)(low+(int32_t)(((uint32_t)value*span+127)/255));}
#endif
