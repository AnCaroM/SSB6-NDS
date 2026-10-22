#include "motion_scene.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *scenario = argc > 1 ? argv[1] : "long";
    if (strcmp(scenario, "long") && strcmp(scenario, "short") && strcmp(scenario, "ledge")) {
        fputs("Usage: scene_trace [long|short|ledge]\n", stderr);
        return 2;
    }
    MotionScene scene;
    scene_init(&scene);
    puts("tick,stick_x,jump,state,state_ticks,x,y,vx,vy,short_hop,landings,respawns");
    for (int t = 0; t < 300; ++t) {
        SceneInput input = {0, 0};
        if (!strcmp(scenario, "ledge")) input.stick_x = t < 45 ? 80 : 0;
        else input.jump_held = t < (!strcmp(scenario, "short") ? 1 : 20);
        scene_step(&scene, input);
        printf("%lu,%d,%d,%s,%u,%.9g,%.9g,%.9g,%.9g,%d,%u,%u\n",
               (unsigned long)scene.tick, input.stick_x, input.jump_held,
               scene_state_name(scene.state), scene.state_ticks,
               scene.x, scene.y, scene.vx, scene.vy,
               scene.short_hop, scene.landings, scene.respawns);
    }
    return 0;
}
