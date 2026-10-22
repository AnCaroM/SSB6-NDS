/* Scalar adaptation of resource 637 TransN and ftPhysicsGetAirVelTransN.
 * Source: ssb-decomp-re 572a747d8171137718fad472c5d7473dc7309ffd.
 * Curves independently compared against decoded ROM Figatree tracks.
 * Constants are animation units (1/4 values, 1/32 derivatives).
 */
#include "special_hi.h"

static float cubic(float time,float duration,float a,float b,float da,float db)
{
    float t=time/duration,t2=t*t,t3=t2*t;
    return (2*t3-3*t2+1)*a+(-2*t3+3*t2)*b+
           (t3-2*t2+t)*duration*da+(t3-t2)*duration*db;
}

void special_hi_root(unsigned tick,float *y,float *z)
{
    if(tick>SPECIAL_HI_TICKS)tick=SPECIAL_HI_TICKS;
    if(tick<=6) {*y=*z=0;return;}
    *y=cubic((float)tick-6,40,0,3371/4.0f,6415/32.0f,-882/32.0f);
    if(tick<=20)*z=cubic((float)tick-6,14,0,2369/4.0f,2053/32.0f,876/32.0f);
    else *z=cubic((float)tick-20,26,2369/4.0f,3620/4.0f,876/32.0f,217/32.0f);
}

void special_hi_velocity(unsigned tick,int facing,int tilt,float *vx,float *vy)
{
    float y,z,py,pz;special_hi_root(tick,&y,&z);
    special_hi_root(tick?tick-1:0,&py,&pz);
    if(tilt>18)tilt=18;
    if(tilt<-18)tilt=-18;
    float a=tilt*.01745329252f,a2=a*a;
    /* Bounded polynomial rotation at +/-18 degrees, no runtime trig. */
    float sn=a*(1-a2/6+a2*a2/120),cs=1-a2/2+a2*a2/24;
    float dx=(z-pz)*facing*1.12f,dy=(y-py)*1.12f;
    *vx=dx*cs-dy*sn;*vy=dx*sn+dy*cs;
}
