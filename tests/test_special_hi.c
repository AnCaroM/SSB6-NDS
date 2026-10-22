#include "motion_scene.h"
#include "special_hi.h"
#include "recovery_demo.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    FILE *curve=fopen("build/scene/special-hi.csv","w");assert(curve);
    fputs("tick,root_y,root_z,vx,vy\n",curve);
    for(unsigned t=0;t<=40;++t) {
        float y,z,vx,vy;special_hi_root(t,&y,&z);special_hi_velocity(t,1,0,&vx,&vy);
        fprintf(curve,"%u,%.7f,%.7f,%.7f,%.7f\n",t,y,z,vx,vy);
        float lx,ly;special_hi_velocity(t,-1,18,&lx,&ly);
        assert(isfinite(lx) && isfinite(ly));
    }
    fclose(curve);
    MotionScene s;scene_init_stage(&s,STAGE_DREAMLAND);
    assert(scene_special_hi(&s,0));assert(!scene_special_hi(&s,80));
    for(int i=0;i<7;++i)scene_step(&s,(SceneInput){0,1});
    assert(s.state==SCENE_SPECIAL_HI && s.y>0 && s.special_uses==1 && !s.air_jumps);
    int saw_fall=0,saw_land=0;
    for(int i=0;i<200;++i) {
        if(s.state==SCENE_SPECIAL_FALL) {
            saw_fall=1;assert(!scene_special_hi(&s,80));
            unsigned used=s.air_jumps;scene_step(&s,(SceneInput){0,i&1});assert(s.air_jumps==used);
        } else scene_step(&s,(SceneInput){0,0});
        if(s.state==SCENE_LAND) {
            saw_land=1;assert(s.landing_ticks==25);
            assert(!scene_special_hi(&s,0));
            scene_drop_through(&s);assert(s.state==SCENE_LAND);
        }
    }
    assert(saw_fall && saw_land && s.state==SCENE_WAIT && !s.respawns && !s.special_used);
    assert(scene_special_hi(&s,0)); /* Landing restores the move. */
    for(int side=-1;side<=1;side+=2) {
        recovery_demo_begin(&s,side);
        FILE *trace=fopen(side<0?"build/scene/special-left.csv":"build/scene/special-right.csv","w");assert(trace);
        fputs("tick,x,y,state,special_uses,air_jumps,grabs\n",trace);
        int freefall=0,returned=0;
        for(unsigned t=0;t<240;++t) {
            SceneInput in=recovery_demo_special_input(&s,t,side);
            if(t==RECOVERY_SPECIAL_FRAME)assert(scene_special_hi(&s,in.stick_x));
            scene_step(&s,in);
            if(s.state==SCENE_SPECIAL_FALL)freefall=1;
            if(s.special_uses && s.state==SCENE_WAIT && s.y>=0)returned=1;
            fprintf(trace,"%u,%.6f,%.6f,%d,%u,%u,%u\n",t,s.x,s.y,s.state,s.special_uses,s.air_jumps,s.cliff_grabs);
        }
        fclose(trace);
        fprintf(stderr,"Special recovery %d: x=%.1f y=%.1f state=%d return=%d KO=%u uses=%u\n",side,s.x,s.y,s.state,returned,s.respawns,s.special_uses);
        assert(freefall && returned && !s.respawns && s.air_jumps==1 && s.special_uses==1);
        /* Without the special, the identical deeper route cannot recover. */
        recovery_demo_begin(&s,side);
        for(unsigned t=0;t<240;++t)scene_step(&s,recovery_demo_special_input(&s,t,side));
        assert(s.respawns>0);
    }
    puts("PASS: root curve, special launch/lockout/freefall/landing and both deep recoveries");
    return 0;
}
