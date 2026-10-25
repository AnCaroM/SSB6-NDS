#ifndef SSB_MOTION_SCENE_H
#define SSB_MOTION_SCENE_H
#include <stdint.h>
#include "stage.h"
#include "fighter.h"
#define SCENE_CATCH_TICKS 8

/* Experimental scene: numeric primitives come from the decomp; the scene
 * scheduler, floor and reduced state machine are provisional integration. */
typedef enum { SCENE_WAIT, SCENE_WALK, SCENE_KNEE, SCENE_AIR, SCENE_LAND,
               SCENE_CLIFF, SCENE_CLIMB, SCENE_SPECIAL_HI, SCENE_SPECIAL_FALL, SCENE_SQUAT } SceneState;
typedef struct { int stick_x; int jump_held; } SceneInput;
typedef struct {
    uint32_t tick;
    SceneState state;
    unsigned state_ticks;
    float x, y, vx, vy;
    /* Separate external impulse: ordinary gravity must not clamp knockback. */
    float hit_vx,hit_vy;
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
    const FighterAttributes *attr; /* physics numbers of this fighter */
    uint8_t kind;                  /* FighterKind: selects the up-special physics */
    unsigned special_ticks;        /* length of a non-Mario aerial up-special */
    int squat_held;                /* down held this tick (set by the caller): Squat */
} MotionScene;

void scene_init(MotionScene *scene);
void scene_init_stage(MotionScene *scene, StageId stage);
void scene_drop_through(MotionScene *scene);
void scene_step(MotionScene *scene, SceneInput input);
const char *scene_state_name(SceneState state);
int scene_special_hi(MotionScene *scene,int stick_x);
/* Link SpecialAirHi (ftlinkspecialhi.c): vertical launch, then freefall. */
int scene_special_hi_rise(MotionScene *scene,unsigned ticks);
#endif
