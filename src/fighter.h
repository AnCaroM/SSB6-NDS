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
    float shield_size,shield_break_vel_y;
} FighterAttributes;
/* Shield, rolls, techs and knockdowns (ftcommonguard1/2, escape, passive,
 * down*, shieldbreak* and furafura .c). Each entry names the clip of one
 * status; GUARD and DOWN_WAIT_* hold the last frame of GuardOn / DownBounce. */
typedef enum {
    ACTION_NONE,ACTION_GUARD_ON,ACTION_GUARD,ACTION_GUARD_OFF,ACTION_ESCAPE_F,ACTION_ESCAPE_B,
    ACTION_PASSIVE,ACTION_PASSIVE_F,ACTION_PASSIVE_B,
    ACTION_DOWN_BOUNCE_D,ACTION_DOWN_BOUNCE_U,ACTION_DOWN_WAIT_D,ACTION_DOWN_WAIT_U,
    ACTION_DOWN_STAND_D,ACTION_DOWN_STAND_U,ACTION_DOWN_FORWARD_D,ACTION_DOWN_FORWARD_U,
    ACTION_DOWN_BACK_D,ACTION_DOWN_BACK_U,
    ACTION_SHIELD_BREAK_FLY,ACTION_SHIELD_BREAK_FALL,ACTION_SHIELD_BREAK_DOWN_D,ACTION_SHIELD_BREAK_DOWN_U,
    ACTION_SHIELD_BREAK_STAND_D,ACTION_SHIELD_BREAK_STAND_U,ACTION_FURAFURA,ACTION_COUNT
} FighterActionId;
typedef struct {
    uint16_t clip,ticks;
    uint8_t intangible_from,intangible_to; /* SetHitStatusAll(3) .. (1); equal = none */
    uint8_t turn_tick;                     /* SetFlag1 tick (facing flips), 255 none */
    uint8_t sound[2];                      /* FightSound played at tick 0, 255 none */
    const int16_t *dx;                     /* per-tick TransN movement (facing units) or NULL */
} FighterAction;
typedef enum { FIGHTER_MARIO, FIGHTER_LINK, FIGHTER_KIND_COUNT } FighterKind;
/* Clip ids for the reduced scene states (indices into the fighter's clips). */
typedef struct {
    uint16_t wait,walk,jump,fall,land,cliff_wait,cliff_catch,jump_aerial_f,jump_aerial_b;
    uint16_t special_fall,dash,run,brake,capture,damage_fall,squat,squat_wait,squat_rv;
    uint16_t item_throw,item_throw_air,damage[17];
} FighterClipMap;
/* FightSound ids: script-independent voices and effects (FTAttributes). */
typedef struct { uint8_t jump,jump_air,land,damage,dead,dead_slam,smash[3],down_bounce; } FighterSounds;
typedef struct {
    const char *name;
    FighterKind kind;
    const FighterAttributes *attr;
    const MoveHit *hits; const MoveFrame *frames; const MoveDesc *moves;
    const unsigned *damage_ticks;   /* 17 damage reaction clip lengths */
    FighterClipMap clips;
    FighterSounds sounds;
    MoveHit throws[2];              /* FTThrowHitDesc of ThrowF / ThrowB */
    const FighterAction *actions;   /* ACTION_COUNT entries, NULL for the legacy demos */
    int16_t shield_x,shield_y;      /* YRotN at the end of GuardOn: shield centre */
} Fighter;
extern const FighterAttributes fighter_attr_mario,fighter_attr_link;
/* Mario data linked into the build (legacy or generic pipeline). */
extern const Fighter fighter_mario;
#endif
