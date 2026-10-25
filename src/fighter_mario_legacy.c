/* Mario from the legacy pipeline (build/model/mario_moves.c) for the older
 * demo builds; the 1v1 and host tests use the generic build/fighters data.
 * Clip ids are unused by those demos. */
#include "fighter.h"
const Fighter fighter_mario={"Mario",FIGHTER_MARIO,&fighter_attr_mario,mario_move_hits,mario_move_frames,mario_moves,
    mario_damage_ticks,{0},{SOUND_VOICE_JUMP,SOUND_VOICE_JUMP_AIR,SOUND_LAND,SOUND_VOICE_DAMAGE,SOUND_VOICE_DEAD,SOUND_DEAD_SLAM,
    {SOUND_VOICE_SMASH1,SOUND_VOICE_SMASH2,SOUND_VOICE_SMASH3}},
    /* 202_MarioMainMotion.c FTThrowHitDesc 0x0C30 / 0x0CC4 (US) */
    {{0,0,0,12,45,70,0,80,0,3,2896,2896,SOUND_NONE},{0,0,0,16,45,70,0,70,0,3,2896,2896,SOUND_NONE}}};
