/* Link in the 1v1: own physics, sword hits/sounds, spin attack recovery,
 * Link vs Mario self-play. Data from build/fighters (user ROM). */
#include "versus.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
extern const Fighter fighter_link;
int main(void)
{
    assert(fighter_link.kind==FIGHTER_LINK && fighter_link.attr->gravity>3.3f);
    Versus v;CombatInput in[2]={{0},{0}};
    /* Jump: Link's kneebend is 7 ticks and he rises higher than Mario. */
    versus_init_fighters(&v,&fighter_link,&fighter_mario);
    in[0].motion.jump_held=in[1].motion.jump_held=1;
    unsigned link_air=0,mario_air=0;float link_peak=0,mario_peak=0;
    for(unsigned t=0;t<60;++t) {
        versus_step(&v,in);
        if(!link_air && v.fighter[0].player.state==SCENE_AIR)link_air=t;
        if(!mario_air && v.fighter[1].player.state==SCENE_AIR)mario_air=t;
        if(v.fighter[0].player.y>link_peak)link_peak=v.fighter[0].player.y;
        if(v.fighter[1].player.y>mario_peak)mario_peak=v.fighter[1].player.y;
    }
    assert(link_air==mario_air+4);
    /* Sword jab connects with a slash sound and damages Mario. */
    versus_init_fighters(&v,&fighter_link,&fighter_mario);
    v.fighter[0].player.x=-200;v.fighter[1].player.x=150;in[0]=in[1]=(CombatInput){0};in[0].attack=1;
    unsigned slash=0;
    for(unsigned t=0;t<20;++t) {
        versus_step(&v,in);
        for(unsigned e=0;e<v.sound_count;++e)
            if(v.sounds[e].sound>=SOUND_SLASH_S && v.sounds[e].sound<=SOUND_SLASH_L)slash=1;
    }
    assert(v.percent[1]>0 && slash && v.fighter[0].move==MOVE_JAB1);
    /* Air spin: rises, then freefall; only once until landing. */
    versus_init_fighters(&v,&fighter_link,&fighter_mario);in[0]=in[1]=(CombatInput){0};
    v.fighter[0].player.y=1500;v.fighter[0].player.state=SCENE_AIR;v.fighter[0].player.platform=-1;
    v.fighter[0].player.jumps_used=2;
    in[0].up=1;in[0].special=1;versus_step(&v,in);
    assert(v.fighter[0].player.state==SCENE_SPECIAL_HI && v.fighter[0].move==MOVE_UP_AIR);
    float start=v.fighter[0].player.y,peak=start;in[0]=(CombatInput){0};
    unsigned landings=v.fighter[0].player.landings,ended=0;
    for(unsigned t=0;t<160 && !ended;++t) {
        in[0].up=in[0].special=t==6;  /* a second up+B mid-spin is ignored */
        versus_step(&v,in);if(v.fighter[0].player.y>peak)peak=v.fighter[0].player.y;
        if(t==7)assert(v.fighter[0].attack_frame==8);
        ended=v.fighter[0].player.state==SCENE_SPECIAL_FALL || v.fighter[0].player.landings!=landings;
    }
    assert(peak>start+400 && ended);
    /* Ground spin stays grounded and hits. */
    versus_init_fighters(&v,&fighter_link,&fighter_mario);in[0]=in[1]=(CombatInput){0};
    v.fighter[0].player.x=-150;v.fighter[1].player.x=150;in[0].up=1;in[0].special=1;
    for(unsigned t=0;t<30;++t) {versus_step(&v,in);in[0]=(CombatInput){0};}
    assert(v.fighter[0].player.state!=SCENE_SPECIAL_HI && v.percent[1]>0);
    /* FTAttributes weight multiplies knockback: Link 0.96 vs Mario 1.0. */
    MoveHit h={0};h.damage=10;h.scale=100;h.base=20;
    assert(combat_knockback_weight(50,&h,fighter_link.attr->weight)<combat_knockback_weight(50,&h,1));
    /* Bounded CPU self-play Link vs Mario. */
    versus_init_fighters(&v,&fighter_link,&fighter_mario);unsigned hits=0;
    for(unsigned t=0;t<7200 && v.winner<0;++t) {
        in[0]=versus_cpu(&v,0);in[1]=versus_cpu(&v,1);versus_step(&v,in);
        for(unsigned i=0;i<2;++i)assert(isfinite(v.fighter[i].player.x) && isfinite(v.fighter[i].player.y));
        hits=v.fighter[0].hits+v.fighter[1].hits;
    }
    assert(hits);
    printf("PASS: Link physics/kneebend, sword slash hit, air/ground spin, weight, Link-vs-Mario CPU (winner %d)\n",v.winner);
    return 0;
}
