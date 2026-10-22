#ifndef SSB_MOTION_SCENE_H
#define SSB_MOTION_SCENE_H
#include <stdint.h>
#include "stage.h"
#define SCENE_CATCH_TICKS 8

/* Experimental scene: numeric primitives come from the decomp; the scene
 * scheduler, floor and reduced state machine are provisional integration. */
typedef enum { SCENE_WAIT, SCENE_WALK, SCENE_KNEE, SCENE_AIR, SCENE_LAND,
               SCENE_CLIFF, SCENE_CLIMB, SCENE_SPECIAL_HI, SCENE_SPECIAL_FALL } SceneState;
typedef struct { int stick_x; int jump_held; } SceneInput;
typedef struct {
    uint32_t tick;
    SceneState state;
    unsigned state_ticks;
    float x, y, vx, vy;
    int jump_was_held, short_hop;
    unsigned landings, respawns;
    StageId stage;
    int platform, ignored_platform;
    unsigned drop_ticks;
    int facing,cliff_side,cliff_ready;
    unsigned cliff_cooldown,cliff_grabs;
    unsigned jumps_used,air_jump_ticks,air_jumps;
    int air_jump; /* 0 ordinary fall, 1 JumpAerialF, 2 JumpAerialB. */
    unsigned special_uses,landing_ticks;
    int special_used,special_air,special_tilt;
} MotionScene;

void scene_init(MotionScene *scene);
void scene_init_stage(MotionScene *scene, StageId stage);
void scene_drop_through(MotionScene *scene);
void scene_step(MotionScene *scene, SceneInput input);
const char *scene_state_name(SceneState state);
int scene_special_hi(MotionScene *scene,int stick_x);
#endif
