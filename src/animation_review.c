#include "animation_review.h"
int animation_review_step(Versus *v,unsigned wall)
{
    static const MarioMove moves[]={MOVE_JAB1,MOVE_FTILT,MOVE_FSMASH,MOVE_USMASH,MOVE_FAIR,MOVE_DAIR};
    if(wall%3)return 0;
    unsigned sequence=wall/(144*3),age=wall/3%144;
    if(!age) {
        versus_init(v);
        CombatScene *s=&v->fighter[0];
        MarioMove move=moves[sequence%6];
        combat_demo_begin(s,move);s->duel_mode=1;
        int dir=sequence/6%2?-1:1;
        s->player.x*=dir;s->target.x*=dir;
        s->player.facing=s->facing=s->attack_facing=dir;s->target.facing=-dir;
        v->fighter[1].player=s->target;
        v->percent[1]=sequence%6>=2 && sequence%6<=3?80:0;
    }
    int advances=!v->freeze && v->winner<0;
    CombatInput input[2]={0};versus_step(v,input);
    return advances;
}
