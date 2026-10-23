#ifndef SSB_VERSUS_H
#define SSB_VERSUS_H
#include "combat.h"
typedef enum { SOUND_SWING,SOUND_HIT,SOUND_HEAVY,SOUND_FIRE,SOUND_JUMP,SOUND_LAND,SOUND_KO,SOUND_COUNT } FightSound;
typedef struct { unsigned sound,fighter; } FightSoundEvent;
typedef struct {unsigned pose,age,active;float decay_x,decay_y;} DamageReaction;
typedef struct {
    CombatScene fighter[2];
    unsigned percent[2],stun[2],stocks[2],invulnerable[2];
    float launch[2];
    float launch_y[2];
    DamageReaction reaction[2];
    FightSoundEvent sounds[8];
    unsigned sound_count;
    unsigned tick,freeze,cpu_decisions;
    int winner; /* -1 playing, 0/1 winner, 2 draw */
} Versus;
void versus_init(Versus *v);
void versus_step(Versus *v,const CombatInput input[2]);
CombatInput versus_cpu(const Versus *v,unsigned who);
int versus_captured(const Versus *v,unsigned who);
/* -1 ordinary animation, 0..16 damage, 17 DamageFall continuation. */
int versus_damage_pose(const Versus *v,unsigned who);
#endif
