#include "versus.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static CombatScene first_hit(MarioMove move,unsigned percent,int airborne)
{
    CombatScene s;combat_demo_begin(&s,move);s.duel_mode=1;s.percent=percent;
    if(airborne) {s.target.state=SCENE_AIR;s.target.platform=-1;s.target.jumps_used=2;}
    for(unsigned t=0;t<mario_moves[move].ticks && !s.hits;++t) {
        s.attack_frame=(int)t;combat_resolve(&s);
    }
    assert(s.hits);return s;
}
static Versus strike(MarioMove move,unsigned percent)
{
    Versus v;versus_init(&v);
    CombatScene s;combat_demo_begin(&s,move);s.duel_mode=1;
    unsigned t=0;
    while(t<mario_moves[move].ticks && !mario_move_frames[mario_moves[move].first+t].count)++t;
    assert(t && t<mario_moves[move].ticks);
    s.attack_frame=(int)t-1;v.fighter[0]=s;v.fighter[1].player=s.target;
    v.percent[1]=percent;
    CombatInput input[2]={{0},{0}};versus_step(&v,input);
    assert(v.stun[1] && v.percent[1]>percent);return v;
}
int main(void)
{
    CombatScene jab=first_hit(MOVE_JAB1,0,0);
    assert(jab.hitstop==5 && jab.hitstun==9 && jab.damage_vy==0);
    assert(jab.damage_pose<9 && jab.target.facing==-1);
    CombatScene air=first_hit(MOVE_JAB1,0,1);
    assert(air.damage_pose==9 && air.target.vy==0);
    assert(air.target.jumps_used==2); /* Damage cannot grant another air jump. */
    assert(fabsf(air.damage_vy/air.damage_vx-.932515f)<.001f); /* 43 degrees. */
    const SceneState aerial_states[]={SCENE_SPECIAL_HI,SCENE_SPECIAL_FALL,SCENE_CLIFF,SCENE_CLIMB};
    for(unsigned i=0;i<sizeof aerial_states/sizeof *aerial_states;++i) {
        CombatScene s;combat_demo_begin(&s,MOVE_JAB1);s.duel_mode=1;s.target.state=aerial_states[i];
        for(unsigned t=0;t<mario_moves[MOVE_JAB1].ticks && !s.hits;++t) {
            s.attack_frame=(int)t;combat_resolve(&s);
        }
        assert(s.hits && s.damage_pose==9 && s.damage_vy>0);
    }
    CombatScene hard=first_hit(MOVE_FSMASH,100,0);
    assert(hard.damage_kb>jab.damage_kb && hard.damage_pose>=12);
    assert(hard.hitstop==hard.damage_amount/3+5);
    CombatScene up=first_hit(MOVE_USMASH,100,0);
    assert(up.damage_pose==15 && up.damage_vy>44 && up.target.vy==0);
    Versus v=strike(MOVE_USMASH,100);
    CombatInput input[2]={{0},{0}};
    float y=v.fighter[1].player.y;
    unsigned age=v.reaction[1].age,freeze=v.freeze;
    assert(v.sound_count && v.sound_count<=8);
    for(unsigned t=0;t<freeze;++t) {
        versus_step(&v,input);
        assert(v.fighter[1].player.y==y && v.reaction[1].age==age && !v.sound_count);
    }
    float launch=v.launch_y[1];
    input[1].motion.jump_held=input[1].attack=1;
    versus_step(&v,input);
    assert(v.fighter[1].player.y-y>44); /* Terminal fall speed cannot clip a launch. */
    assert(v.launch_y[1]<launch && v.launch_y[1]>0);
    assert(v.fighter[1].attack_frame<0 && v.reaction[1].age==age+1);
    /* Solid landing cancels vertical impulse and starts bounded recovery. */
    v.fighter[1].player.y=10;v.fighter[1].player.x=0;
    v.fighter[1].player.vy=-20;v.launch_y[1]=-40;
    v.reaction[1].decay_y=-1.7f;
    versus_step(&v,input);
    assert(v.fighter[1].player.state==SCENE_LAND && !v.launch_y[1]);
    assert(v.stun[1]==6 && !v.reaction[1].active && versus_damage_pose(&v,1)==-1);
    /* After vector exhaustion there is no sign reversal / phantom impulse. */
    v.fighter[1].player.y=2000;v.fighter[1].player.state=SCENE_AIR;
    v.launch[1]=.5f;v.launch_y[1]=0;v.reaction[1].decay_x=1.7f;
    versus_step(&v,input);assert(v.launch[1]==0 && v.launch_y[1]==0);
    v=strike(MOVE_JAB1,0);v.freeze=0;input[1]=(CombatInput){0};
    for(unsigned t=0;t<9;++t)versus_step(&v,input);
    assert(!v.stun[1] && versus_damage_pose(&v,1)>=0); /* Finish short reaction. */
    for(unsigned t=0;t<15;++t)versus_step(&v,input);
    assert(versus_damage_pose(&v,1)==-1);
    v=strike(MOVE_USMASH,100);v.freeze=0;v.stun[1]=0;
    v.reaction[1].age=mario_damage_ticks[v.reaction[1].pose];
    assert(versus_damage_pose(&v,1)==17); /* Strong launch continues as tumble. */
    input[1].attack=1;versus_step(&v,input);
    assert(v.fighter[1].attack_frame>=0 && versus_damage_pose(&v,1)==-1);
    puts("PASS: source damage angles/levels/hitlag, vertical launch, frozen reactions/audio, input lock, landing and decay");
}
