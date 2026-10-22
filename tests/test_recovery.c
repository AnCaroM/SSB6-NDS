#include "recovery_demo.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    MotionScene s;scene_init_stage(&s,STAGE_DREAMLAND);
    for(int i=0;i<4;++i)scene_step(&s,(SceneInput){0,1});
    assert(s.state==SCENE_AIR && s.jumps_used==1);
    for(int i=0;i<10;++i)scene_step(&s,(SceneInput){0,1});
    assert(s.air_jumps==0); /* Holding A never consumes the air jump. */
    scene_step(&s,(SceneInput){0,0});
    scene_step(&s,(SceneInput){0,1});
    assert(s.jumps_used==2 && s.air_jumps==1 && s.air_jump==1);
    assert(fabsf(s.vy-71.4f)<.001f); /* Original 73.8 impulse, then gravity 2.4. */
    scene_step(&s,(SceneInput){0,0});
    float vy=s.vy;
    scene_step(&s,(SceneInput){0,1});
    assert(s.air_jumps==1 && s.vy<vy); /* No third jump. */
    for(int t=0;t<200;++t)scene_step(&s,(SceneInput){0,0});
    assert(s.jumps_used==0 && s.air_jump==0 && s.respawns==0);

    for(int side=-1;side<=1;side+=2) {
        recovery_demo_begin(&s,side);
        FILE *trace=fopen(side<0?"build/scene/recovery-left.csv":"build/scene/recovery-right.csv","w");
        assert(trace);fputs("tick,x,y,vy,state,jumps_used,air_jumps,grabs\n",trace);
        int left_ground=0,boosted=0,caught=0,stood=0;
        for(unsigned t=0;t<240;++t) {
            scene_step(&s,recovery_demo_input(&s,t,side));
            fprintf(trace,"%u,%.5f,%.5f,%.5f,%d,%u,%u,%u\n",t,s.x,s.y,s.vy,s.state,s.jumps_used,s.air_jumps,s.cliff_grabs);
            if(s.state==SCENE_AIR && s.y<0)left_ground=1;
            if(s.air_jumps)boosted=1;
            if(s.state==SCENE_CLIFF)caught=1;
            if(caught && s.state==SCENE_WAIT && s.y==0)stood=1;
        }
        fclose(trace);
        printf("Recovery side %d: left=%d boost=%d catch=%d stood=%d x=%.2f y=%.2f KO=%u\n",
               side,left_ground,boosted,caught,stood,s.x,s.y,s.respawns);
        assert(left_ground && boosted && caught && stood && !s.respawns);
        assert(s.air_jumps==1 && s.jumps_used==0);
    }
    /* Backward clip selection uses facing before the prototype's air turn. */
    scene_init_stage(&s,STAGE_DREAMLAND);
    s.state=SCENE_AIR;s.y=1200;s.facing=1;s.jumps_used=1;
    scene_step(&s,(SceneInput){-80,1});
    assert(s.air_jump==2 && s.vx<0);
    scene_init_stage(&s,STAGE_DREAMLAND);
    s.state=SCENE_AIR;s.y=-350;s.x=2500;s.vy=-30;s.facing=-1;s.jumps_used=2;
    scene_step(&s,(SceneInput){0,0});
    assert(s.state==SCENE_CLIFF && s.jumps_used==1);
    puts("PASS: double jump, held/third jump rejection, reset and both recovery paths");
    return 0;
}
