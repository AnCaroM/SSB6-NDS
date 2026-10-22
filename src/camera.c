/* Adapted interest bounds and asymmetric zoom from ssb-decomp-re gmcamera.c,
 * revision 572a747d8171137718fad472c5d7473dc7309ffd.
 * Orthographic DS framing/safety margins and fixed-point smoothing are local.
 * Dream Land camera bounds: ROM resource 255, MapHeader 0x14 + 0x6c.
 */
#include "camera.h"

#define Q 16
static int32_t min(int32_t a,int32_t b) { return a<b?a:b; }
static int32_t max(int32_t a,int32_t b) { return a>b?a:b; }
static int32_t clamp(int32_t x,int32_t lo,int32_t hi) { return min(max(x,lo),hi); }
static int32_t absq(int32_t x) { return x<0?-x:x; }
static int32_t approach(int32_t a,int32_t b,int numerator,int denominator)
{
    int32_t d=b-a;
    if(absq(d)<=Q) return b;
    return a+d*numerator/denominator;
}

void camera_reset(FightCamera *c) { *c=(FightCamera){0,0,2800*Q,0}; }

void camera_step(FightCamera *c,const CameraFighter *f,unsigned count)
{
    /* Original per-player ranges: 1.50, 1.32, 1.16, 1.00. Four max. */
    static const int zoom[]={0,150,132,116,100};
    if(!count) return;
    if(count>4) count=4;
    int32_t left=1000000,right=-1000000,bottom=1000000,top=-1000000;
    int32_t safe_l=left,safe_r=right,safe_b=bottom,safe_t=top;
    for(unsigned i=0;i<count;++i) {
        int32_t x=clamp(f[i].x,-3900*Q,3900*Q);
        /* Body focus offset 200 is provisional for Mario, not an extracted attr. */
        int32_t y=clamp(f[i].y+200*Q,-2000*Q,4000*Q);
        int32_t back=700*Q*zoom[count]/100,front=1000*Q*zoom[count]/100;
        left=min(left,x-(f[i].facing<0?front:back));
        right=max(right,x+(f[i].facing<0?back:front));
        bottom=min(bottom,y-back); top=max(top,y+back);
        safe_l=min(safe_l,x-360*Q); safe_r=max(safe_r,x+360*Q);
        safe_b=min(safe_b,y-420*Q); safe_t=max(safe_t,y+500*Q);
    }
    int32_t wanted=max(2800*Q,max(top-bottom,(right-left)*3/4));
    int32_t tx=(left+right)/2,ty=(bottom+top)/2;
    /* Original vertical interest bias ramps from 0 at 1000 to .0682 at 2000. */
    int32_t extent=max((right-left)/2,(top-bottom)/2);
    int bias=clamp((extent-1000*Q)*682/(1000*Q),0,682);
    ty-=ty*(2*bias)/10000;
    if(!c->ready) { c->x=tx;c->y=ty;c->height=wanted;c->ready=1; }
    else {
        c->x=approach(c->x,tx,8,100); c->y=approach(c->y,ty,8,100);
        /* Original zoom expands immediately and contracts by .075 per tick. */
        c->height=wanted>c->height?wanted:approach(c->height,wanted,75,1000);
    }
    /* Keep bodies visible during pan lag, jumps and opposing knockback.
     * Targets beyond original stage bounds deliberately stop pulling the view. */
    int32_t hw=max(absq(safe_l-c->x),absq(safe_r-c->x));
    int32_t hh=max(absq(safe_b-c->y),absq(safe_t-c->y));
    c->height=max(c->height,max((hw*3+1)/2,hh*2));
}
