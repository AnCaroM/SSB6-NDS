#include "motion_scene.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    MotionScene s;
    scene_init_stage(&s, STAGE_GARDEN);
    s.state = SCENE_AIR; s.y = 1030; s.vy = -40;
    scene_step(&s, (SceneInput){0, 0});
    assert(s.platform == 3 && s.y == 1000 && s.state == SCENE_LAND);
    /* One-way platforms must allow an ascending fighter to pass through. */
    scene_init_stage(&s, STAGE_GARDEN);
    s.state = SCENE_AIR; s.x = -400; s.y = 480; s.vy = 40;
    scene_step(&s, (SceneInput){0, 0});
    assert(s.state == SCENE_AIR && s.y > 500 && s.landings == 0);
    /* A fall above the center must land on the upper platform. */
    s.x = 0; s.y = 1020; s.vy = -1000;
    scene_step(&s, (SceneInput){0, 0});
    assert(s.platform == 3 && s.y == 1000);
    /* Down drops through just the supporting platform, then the ground catches. */
    scene_drop_through(&s);
    assert(s.state == SCENE_AIR && s.ignored_platform == 3);
    for (int i = 0; i < 100; ++i) scene_step(&s, (SceneInput){0, 0});
    assert(s.platform == 0 && s.y == 0 && s.respawns == 0);
    scene_drop_through(&s);
    assert(s.state != SCENE_AIR); /* Main island cannot be dropped through. */
    /* Leaving a raised platform falls rather than sticking to its edge. */
    s.x = -740; s.y = 500; s.platform = 1; s.state = SCENE_WAIT;
    for (int i = 0; i < 6; ++i) scene_step(&s, (SceneInput){-80, 0});
    assert(s.state == SCENE_AIR && s.y < 500);
    /* Respawn preserves the selected map. */
    s.y = -1801;
    scene_step(&s, (SceneInput){0, 0});
    assert(s.stage == STAGE_GARDEN && s.respawns == 1 && s.platform == 0);
    /* A real jump, rather than a state placed above a surface, reaches and
     * lands on the upper platform. Drop-through then returns to the island. */
    scene_init_stage(&s, STAGE_GARDEN);
    for (int i = 0; i < 100; ++i) scene_step(&s, (SceneInput){0, i < 10});
    assert(s.platform == 3 && s.y == 1000 && s.landings == 1 && s.respawns == 0);
    scene_drop_through(&s);
    for (int i = 0; i < 100; ++i) scene_step(&s, (SceneInput){0, 0});
    assert(s.platform == 0 && s.y == 0 && s.landings == 2 && s.respawns == 0);
    /* Original Dream Land ground coordinates, including unequal side heights. */
    scene_init_stage(&s,STAGE_DREAMLAND);
    s.state=SCENE_AIR;s.x=-1396;s.y=920;s.vy=-40;
    scene_step(&s,(SceneInput){0,0});
    assert(s.platform==1 && s.y==904);
    s.state=SCENE_AIR;s.x=1421;s.y=920;s.vy=-40;
    scene_step(&s,(SceneInput){0,0});
    assert(s.platform==2 && s.y==907);
    /* Reach upper original platform from the left original platform. */
    scene_init_stage(&s,STAGE_DREAMLAND);
    s.x=-1100;s.y=904;s.platform=1;
    int landed=0;
    for(int i=0;i<100;++i) {
        scene_step(&s,(SceneInput){80,i<10});
        if(s.platform==3 && s.state==SCENE_LAND) {landed=1;break;}
    }
    assert(landed && s.y==1542);
    s.y=-2000;s.state=SCENE_AIR;
    scene_step(&s,(SceneInput){0,0});assert(s.respawns==0);
    s.y=-3501;scene_step(&s,(SceneInput){0,0});
    assert(s.respawns==1 && s.stage==STAGE_DREAMLAND);
    puts("PASS: platform landing, upward pass, drop-through, edges and map respawn");
    return 0;
}
