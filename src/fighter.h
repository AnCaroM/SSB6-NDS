#ifndef SSB_FIGHTER_H
#define SSB_FIGHTER_H
#include <stdint.h>
#include "moves.h"
/* Per-character data shared by physics, combat and the 1v1. Attributes are
 * copied from the original FTAttributes (relocData/<fighter>Main.c, checked by
 * tests/test_fighter_attr.py); moves, clips and sounds are generated from the
 * user ROM by tools/convert_fighter.py (or the legacy Mario pipeline). */
typedef struct {
    float walk_speed_mul,traction,run_speed,jump_vel_x,jump_height_mul,jump_height_base;
    float jumpaerial_vel_x,jumpaerial_height,air_accel,air_speed_max_x,air_friction;
    float gravity,tvel_base,weight;
    unsigned kneebend_ticks;
    /* Local integration values (not FTAttributes fields): constant dash
     * velocity of the reduced dash/run and the fall-special air speed cap. */
    float dash_velocity,fall_special_speed;
} FighterAttributes;
typedef enum { FIGHTER_MARIO, FIGHTER_LINK, FIGHTER_KIND_COUNT } FighterKind;
/* Clip ids for the reduced scene states (indices into the fighter's clips). */
typedef struct {
    uint16_t wait,walk,jump,fall,land,cliff_wait,cliff_catch,jump_aerial_f,jump_aerial_b;
    uint16_t special_fall,dash,run,brake,capture,damage_fall,squat,squat_wait,squat_rv;
    uint16_t item_throw,item_throw_air,damage[17];
} FighterClipMap;
/* FightSound ids: script-independent voices and effects (FTAttributes). */
typedef struct { uint8_t jump,jump_air,land,damage,dead,dead_slam,smash[3]; } FighterSounds;
typedef struct {
    const char *name;
    FighterKind kind;
    const FighterAttributes *attr;
    const MoveHit *hits; const MoveFrame *frames; const MoveDesc *moves;
    const unsigned *damage_ticks;   /* 17 damage reaction clip lengths */
    FighterClipMap clips;
    FighterSounds sounds;
    MoveHit throws[2];              /* FTThrowHitDesc of ThrowF / ThrowB */
} Fighter;
extern const FighterAttributes fighter_attr_mario,fighter_attr_link;
/* Mario data linked into the build (legacy or generic pipeline). */
extern const Fighter fighter_mario;
#endif
