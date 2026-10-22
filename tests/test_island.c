#include "motion_scene.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static MotionScene falling(int side)
{
    MotionScene s;scene_init_stage(&s,STAGE_DREAMLAND);
    s.state=SCENE_AIR;s.x=side*2500;s.y=-350;s.vy=-30;s.facing=-side;
    return s;
}

int main(void)
{
    float x=0,y=-1200,vx=0,vy=400;
    stage_resolve_solid(STAGE_DREAMLAND,0,-1600,&x,&y,&vx,&vy);
    assert(y<=-1391.99f && vy<=0); /* 320-high head meets underside at -1072. */
    x=0;y=-100;vx=0;vy=-1100;
    stage_resolve_solid(STAGE_DREAMLAND,0,1000,&x,&y,&vx,&vy);
    assert(fabsf(y)<.01f && vy==0); /* No tunneling through the main floor. */
    for(int side=-1;side<=1;side+=2) {
        x=side*1500;y=-500;vx=-side*1500;vy=0;
        stage_resolve_solid(STAGE_DREAMLAND,side*3000,-500,&x,&y,&vx,&vy);
        assert(x*side>2200 && isfinite(x) && isfinite(y));
        MotionScene s=falling(side);
        scene_step(&s,(SceneInput){0,0});
        assert(s.state==SCENE_CLIFF && s.cliff_grabs==1 && s.cliff_side==side);
        assert(s.vx==0 && s.vy==0 && s.y==-300);
        for(int i=0;i<10;++i)scene_step(&s,(SceneInput){0,0});
        assert(s.state==SCENE_CLIFF && s.y==-300);
        scene_step(&s,(SceneInput){-side*80,0});
        assert(s.state==SCENE_CLIMB);
        for(int i=0;i<12;++i)scene_step(&s,(SceneInput){0,0});
        assert(s.state==SCENE_LAND && s.y==0 && s.x*side<2318);
        s=falling(side);scene_step(&s,(SceneInput){0,0});
        scene_drop_through(&s);
        assert(s.state==SCENE_AIR && s.cliff_cooldown==30);
        for(int i=0;i<10;++i)scene_step(&s,(SceneInput){0,0});
        assert(s.state==SCENE_AIR && s.cliff_grabs==1 && s.y<-360);
        /* Facing away and regrab cooldown block capture. */
        s=falling(side);s.facing=side;scene_step(&s,(SceneInput){0,0});
        assert(s.state==SCENE_AIR);
        s=falling(side);s.cliff_cooldown=20;scene_step(&s,(SceneInput){0,0});
        assert(s.state==SCENE_AIR);
        /* Rising hands cannot grab. */
        s=falling(side);s.y=-380;s.vy=40;scene_step(&s,(SceneInput){0,0});
        assert(s.state==SCENE_AIR);
    }
    MotionScene s=falling(-1);scene_step(&s,(SceneInput){0,0});
    for(int i=0;i<1080;++i)scene_step(&s,(SceneInput){0,0});
    assert(s.state==SCENE_AIR && s.cliff_cooldown==30);
    /* Passing one-way platforms and moving above the island remain free. */
    x=1000;y=10;vx=1000;vy=0;
    stage_resolve_solid(STAGE_DREAMLAND,0,10,&x,&y,&vx,&vy);
    assert(x==1000 && y==10);
    x=0;y=-100;vx=0;vy=-1100;
    stage_resolve_solid(STAGE_GARDEN,0,1000,&x,&y,&vx,&vy);
    assert(y==-100); /* The old garden path is untouched. */
    puts("PASS: swept island walls/ceiling, both ledges, climb, drop, cooldown and timeout");
    return 0;
}
