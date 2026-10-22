#ifndef SSB_VERSUS_H
#define SSB_VERSUS_H
#include "combat.h"
typedef struct {
    CombatScene fighter[2];
    unsigned percent[2],stun[2],stocks[2],invulnerable[2];
    float launch[2];
    unsigned tick,freeze,cpu_decisions;
    int winner; /* -1 playing, 0/1 winner, 2 draw */
} Versus;
void versus_init(Versus *v);
void versus_step(Versus *v,const CombatInput input[2]);
CombatInput versus_cpu(const Versus *v,unsigned who);
int versus_captured(const Versus *v,unsigned who);
#endif
