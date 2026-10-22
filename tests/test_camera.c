#include "camera.h"
#include <assert.h>
#include <stdio.h>
#define Q 16

static void visible(const FightCamera *c,const CameraFighter *f)
{
    /* A 400-unit body and lateral breathing room remain inside the DS 4:3 view. */
    int hw=c->height*2/3,hh=c->height/2;
    assert(f->x-150*Q>=c->x-hw && f->x+150*Q<=c->x+hw);
    assert(f->y-100*Q>=c->y-hh && f->y+400*Q<=c->y+hh);
}

int main(void)
{
    FightCamera c;camera_reset(&c);
    CameraFighter f[]={{-200*Q,0,1},{180*Q,0,-1}};
    camera_step(&c,f,2);
    assert(c.height==2800*Q); /* Closer than Prueba03 near view (3400). */
    int initial=c.x;
    /* Walk/jump apart: follow both axes, preserve bodies during pan lag. */
    for(int t=0;t<180;++t) {
        f[0].x=(-200-t*12)*Q;f[0].y=(t<90?t:180-t)*25*Q;
        f[1].x=(180+t*10)*Q;
        camera_step(&c,f,2);visible(&c,&f[0]);visible(&c,&f[1]);
    }
    assert(c.height>3400*Q && c.x<initial);
    /* Returning together zooms in gradually, rather than cutting immediately. */
    int wide=c.height;
    f[0]=(CameraFighter){-200*Q,0,1};f[1]=(CameraFighter){180*Q,0,-1};
    camera_step(&c,f,2);assert(c.height<wide && c.height>2800*Q);
    for(int t=0;t<240;++t)camera_step(&c,f,2);
    assert(c.height==2800*Q);
    /* A shared translation pans smoothly, not directly to the new midpoint. */
    initial=c.x;f[0].x+=1000*Q;f[1].x+=1000*Q;
    camera_step(&c,f,2);assert(c.x>initial && c.x<initial+1000*Q);
    /* Opposite extremes of the original camera bounds remain visible. */
    f[0]=(CameraFighter){-3900*Q,-2000*Q,1};
    f[1]=(CameraFighter){3900*Q,3700*Q,-1};
    camera_step(&c,f,2);visible(&c,&f[0]);visible(&c,&f[1]);
    for(int t=0;t<240;++t)camera_step(&c,f,2);
    int bounded=c.height;
    f[0].x=-100000*Q;f[1].x=100000*Q;
    for(int t=0;t<240;++t)camera_step(&c,f,2);
    assert(c.height<=bounded); /* Eliminated fighters cannot pull forever. */
    camera_reset(&c);camera_step(&c,f,0);assert(!c.ready);
    f[0]=(CameraFighter){0,0,1};camera_step(&c,f,1);
    assert(c.ready);visible(&c,&f[0]);
    puts("Camera: close framing, separation, jump, pan, smooth return and bounds passed.");
    return 0;
}
