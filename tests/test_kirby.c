/* Kirby in the 1v1 (Prueba20): multi-jump, Final Cutter, Stone, Inhale and
 * CPU self-play. Data from build/fighters (user ROM). */
#include "versus.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
extern const Fighter fighter_link,fighter_kirby;

static int has_sound(const Versus *v,unsigned sound)
{for(unsigned e=0;e<v->sound_count;++e)if(v->sounds[e].sound==sound)return 1;return 0;}
static void steps(Versus *v,CombatInput in[2],unsigned n) {for(unsigned t=0;t<n;++t)versus_step(v,in);}
int main(void)
{
    assert(fighter_kirby.kind==FIGHTER_KIRBY && fighter_kirby.attr->jumps_max==6 && fighter_kirby.actions);
    Versus v;CombatInput in[2]={{0},{0}};
    /* Five air jumps, the later ones 28 ticks apart, each from the table. */
    versus_init_fighters(&v,&fighter_kirby,&fighter_mario);
    in[0].motion.jump_held=1;steps(&v,in,6);in[0].motion.jump_held=0;
    assert(v.fighter[0].player.state==SCENE_AIR && v.fighter[0].player.jumps_used==1);
    unsigned jumps=0;
    for(unsigned t=0;t<400 && v.fighter[0].player.state==SCENE_AIR;++t) {
        in[0].motion.jump_held=t%30==0;versus_step(&v,in);
        if(v.fighter[0].player.air_jumps!=jumps) {
            jumps=v.fighter[0].player.air_jumps;
            if(jumps>=2)assert(fabsf(v.fighter[0].player.vy-fighter_kirby.attr->multi_jump_vel[jumps-2]+fighter_kirby.attr->gravity)<1);
        }
    }
    assert(jumps==5 && v.fighter[0].player.landings==1); /* a 7th press did nothing */
    /* Final Cutter: the clip's TransN lifts Kirby, he plunges, lands into
     * SpecialHiLanding and the shockwave hits Mario on the ground ahead. */
    versus_init_fighters(&v,&fighter_kirby,&fighter_mario);in[0]=in[1]=(CombatInput){0};
    v.fighter[0].player.x=-600;v.fighter[1].player.x=-150;
    in[0].up=in[0].special=1;versus_step(&v,in);in[0]=(CombatInput){0};
    assert(v.fighter[0].move==MOVE_UP && v.fighter[0].player.state==SCENE_SPECIAL_HI);
    float peak=0;int landed=0,beam=0;unsigned percent=v.percent[1];
    for(unsigned t=0;t<150;++t) {
        versus_step(&v,in);if(v.fighter[0].player.y>peak)peak=v.fighter[0].player.y;
        if(v.fighter[0].move==MOVE_UP_LAND)landed=1;
        beam|=v.fighter[0].beam.active;
    }
    assert(peak>300 && landed && beam && v.percent[1]>percent);
    assert(v.fighter[0].player.special_used==0 && v.fighter[0].player.y==0);
    /* Stone: Mario's hits are absorbed until 38 HP, Kirby takes no damage. */
    versus_init_fighters(&v,&fighter_kirby,&fighter_mario);in[0]=in[1]=(CombatInput){0};
    v.fighter[0].player.x=150;v.fighter[1].player.x=-150;v.fighter[1].player.facing=1;
    in[0].down=in[0].special=1;versus_step(&v,in);in[0]=(CombatInput){0};
    for(unsigned t=0;t<40 && !combat_stone(&v.fighter[0]);++t)versus_step(&v,in);
    assert(combat_stone(&v.fighter[0]) && v.fighter[0].stone_resist==38 && has_sound(&v,SOUND_KIRBY_STONE_LAND)==0);
    in[1].attack=1;
    for(unsigned t=0;t<12;++t) {versus_step(&v,in);in[1].attack=!in[1].attack;}
    assert(!v.percent[0] && v.fighter[0].stone_resist<38 && combat_stone(&v.fighter[0]));
    /* B ends it after the minimum time. */
    in[1]=(CombatInput){0};steps(&v,in,25);in[0].special=1;versus_step(&v,in);in[0]=(CombatInput){0};
    assert(!combat_stone(&v.fighter[0]) && v.fighter[0].move==MOVE_STONE_END);
    /* An airborne stone falls at 140 per tick. */
    versus_init_fighters(&v,&fighter_kirby,&fighter_mario);in[0]=(CombatInput){0};
    v.fighter[0].player.y=1500;v.fighter[0].player.state=SCENE_AIR;v.fighter[0].player.platform=-1;
    v.fighter[1].player.x=-2000;
    in[0].down=in[0].special=1;versus_step(&v,in);in[0]=(CombatInput){0};
    for(unsigned t=0;t<40 && v.fighter[0].move!=MOVE_STONE_FALL;++t)versus_step(&v,in);
    assert(v.fighter[0].move==MOVE_STONE_FALL);
    float y=v.fighter[0].player.y;versus_step(&v,in);
    assert(fabsf(y-v.fighter[0].player.y-140)<1);
    for(unsigned t=0;t<20;++t)versus_step(&v,in);
    assert(v.fighter[0].player.y==0 && v.fighter[0].move==MOVE_STONE_HOLD);
    /* Inhale: Mario is caught, held inside (hidden) and spat as a star. */
    versus_init_fighters(&v,&fighter_kirby,&fighter_mario);in[0]=in[1]=(CombatInput){0};
    v.fighter[0].player.x=-300;v.fighter[1].player.x=0;
    in[0].special=1;
    for(unsigned t=0;t<60 && !v.fighter[0].inhaled;++t)versus_step(&v,in);
    assert(v.fighter[0].inhaled && versus_captured(&v,1));
    in[0].special=0;
    for(unsigned t=0;t<40 && v.fighter[0].move!=MOVE_INHALE_WAIT;++t)versus_step(&v,in);
    assert(v.fighter[0].move==MOVE_INHALE_WAIT && v.fighter[0].inhaled);
    in[0].attack=1;versus_step(&v,in);in[0].attack=0;
    for(unsigned t=0;t<20 && v.fighter[0].inhaled;++t)versus_step(&v,in);
    assert(!v.fighter[0].inhaled && v.percent[1]==10 && v.launch[1]>60 && !versus_captured(&v,1));
    /* Mashing gets out sooner than waiting. */
    unsigned waited=0,mashed=0;
    for(unsigned mash=0;mash<2;++mash) {
        versus_init_fighters(&v,&fighter_kirby,&fighter_mario);in[0]=in[1]=(CombatInput){0};
        v.fighter[0].player.x=-300;v.fighter[1].player.x=0;in[0].special=1;
        for(unsigned t=0;t<60 && !v.fighter[0].inhaled;++t)versus_step(&v,in);
        in[0].special=0;unsigned t=0;
        while(t<700 && v.fighter[0].inhaled) {in[1].attack=mash && (t&1);versus_step(&v,in);++t;}
        assert(!v.fighter[0].inhaled);
        if(mash)mashed=t;else waited=t;
    }
    assert(mashed<waited && waited>400);
    /* Shields cannot stop the inhale. */
    versus_init_fighters(&v,&fighter_kirby,&fighter_mario);in[0]=in[1]=(CombatInput){0};
    v.fighter[0].player.x=-300;v.fighter[1].player.x=0;in[1].shield=1;steps(&v,in,8);
    in[0].special=1;
    for(unsigned t=0;t<60 && !v.fighter[0].inhaled;++t)versus_step(&v,in);
    assert(v.fighter[0].inhaled);
    /* Bounded CPU self-play in all Kirby pairings. */
    const Fighter *others[3]={&fighter_mario,&fighter_link,&fighter_kirby};int winners=0;
    for(unsigned k=0;k<3;++k) {
        versus_init_fighters(&v,&fighter_kirby,others[k]);
        for(unsigned t=0;t<12000 && v.winner<0;++t) {
            CombatInput cpu[2]={versus_cpu(&v,0),versus_cpu(&v,1)};versus_step(&v,cpu);
            for(unsigned i=0;i<2;++i)assert(isfinite(v.fighter[i].player.x) && v.percent[i]<=999 && v.fighter[i].inhale_breakout<=500);
        }
        winners+=v.winner>=0;
    }
    assert(winners>=2);
    puts("PASS kirby: six jumps, Final Cutter rise/plunge/landing beam, stone resist/fall/release, inhale catch/spit/escape, CPU");
}
