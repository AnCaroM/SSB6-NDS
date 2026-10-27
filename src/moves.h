#ifndef SSB_MOVES_H
#define SSB_MOVES_H
#include <stdint.h>
#include "fight_sound.h"
typedef enum {
 MOVE_JAB1,MOVE_JAB2,MOVE_JAB3,MOVE_DASH,
 MOVE_FTILT_HI,MOVE_FTILT,MOVE_FTILT_LO,MOVE_UTILT,MOVE_DTILT,
 MOVE_FSMASH_HI,MOVE_FSMASH_MHI,MOVE_FSMASH,MOVE_FSMASH_MLO,MOVE_FSMASH_LO,MOVE_USMASH,MOVE_DSMASH,
 MOVE_NAIR,MOVE_FAIR,MOVE_BAIR,MOVE_UAIR,MOVE_DAIR,
 MOVE_GRAB,MOVE_THROW_F,MOVE_THROW_B,MOVE_FIRE,MOVE_FIRE_AIR,MOVE_UP,
 MOVE_TORNADO,MOVE_TORNADO_AIR,MOVE_GETUP_D,MOVE_GETUP_U,
 MOVE_LEDGE_START,MOVE_LEDGE,MOVE_LEDGE_SLOW_START,MOVE_LEDGE_SLOW,MOVE_GRAB_HOLD,MOVE_UP_AIR,
 MOVE_UP_FALL,MOVE_UP_LAND,MOVE_STONE_HOLD,MOVE_STONE_FALL,MOVE_STONE_END,
 MOVE_INHALE,MOVE_INHALE_EAT,MOVE_INHALE_WAIT,MOVE_INHALE_THROW,MOVE_INHALE_END,MOVE_COUNT
} MarioMove;
typedef struct {
 int16_t x,y,radius,damage,angle,scale,weight,base;
 uint8_t strike,ground_air;
 int16_t cos_angle,sin_angle;
 uint8_t sound; /* FightSound for fgm_kind/level, 255 none */
} MoveHit;
/* dx,dy: per-tick TransN movement for moves listed in the fighter's
 * root_motion (Kirby's Final Cutter), 0 otherwise. */
typedef struct { uint16_t first; uint8_t count,flags; int16_t spawn_x,spawn_y,grab_x,grab_y; uint8_t sound[2]; int16_t dx,dy; } MoveFrame;
typedef struct { const char *name; uint16_t clip,ticks,first; } MoveDesc;
extern const MoveHit mario_move_hits[];
extern const MoveFrame mario_move_frames[];
extern const MoveDesc mario_moves[MOVE_COUNT];
extern const uint8_t mario_move_demo_order[MOVE_COUNT];
extern const unsigned mario_damage_ticks[17];
#endif
