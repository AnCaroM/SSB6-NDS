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
    /* Crouch for both: down held on the ground squats, down+X is the down tilt,
     * releasing stands up; a tap on a raised platform still drops through. */
    for(unsigned k=0;k<2;++k) {
        versus_init_fighters(&v,k?&fighter_link:&fighter_mario,&fighter_mario);in[0]=in[1]=(CombatInput){0};
        in[0].down=1;
        for(unsigned t=0;t<5;++t)versus_step(&v,in);
        assert(v.fighter[0].player.state==SCENE_SQUAT);
        in[0].attack=1;versus_step(&v,in);assert(v.fighter[0].move==MOVE_DTILT && v.fighter[0].attack_frame>=0);
        in[0]=(CombatInput){0};for(unsigned t=0;t<60;++t)versus_step(&v,in);
        assert(v.fighter[0].player.state==SCENE_WAIT);
    }
    /* Boomerang: spawns on SpecialN flag0 (tick 26), flies out, turns, homes
     * back and is caught; a second B while it is out throws nothing. */
    versus_init_fighters(&v,&fighter_link,&fighter_mario);in[0]=in[1]=(CombatInput){0};
    v.fighter[1].player.x=3000;  /* out of the way */
    in[0].special=1;versus_step(&v,in);in[0]=(CombatInput){0};
    unsigned spawn=0,turned=0,caught=0;float far=0;
    for(unsigned t=1;t<400 && !caught;++t) {
        if(t==40) {in[0].special=1;} else in[0].special=0;
        versus_step(&v,in);const Boomerang *b=&v.fighter[0].boomerang;
        if(b->active && !spawn)spawn=t;
        if(b->active && b->x-v.fighter[0].player.x>far)far=b->x-v.fighter[0].player.x;
        if(b->returning && !turned)turned=t;
        for(unsigned e=0;e<v.sound_count;++e)if(v.sounds[e].sound==SOUND_BOOMERANG_CATCH)caught=t;
    }
    assert(spawn>=25 && spawn<=27 && turned>spawn+40 && caught>turned && far>2000);
    /* Boomerang hits Mario on the way out (9%) and turns back. */
    versus_init_fighters(&v,&fighter_link,&fighter_mario);in[0]=in[1]=(CombatInput){0};
    v.fighter[1].player.x=900;in[0].special=1;versus_step(&v,in);in[0]=(CombatInput){0};
    for(unsigned t=0;t<80;++t)versus_step(&v,in);
    assert(v.percent[1]==9 && v.fighter[0].boomerang.returning);
    /* Bomb: down-B pulls it (tick 29), X throws it forward (release tick 8);
     * a fast bomb explodes on Mario. */
    versus_init_fighters(&v,&fighter_link,&fighter_mario);in[0]=in[1]=(CombatInput){0};
    v.fighter[1].player.x=600;in[0].down=1;in[0].special=1;versus_step(&v,in);in[0]=(CombatInput){0};
    for(unsigned t=0;t<60;++t)versus_step(&v,in);
    assert(v.fighter[0].bomb.state==BOMB_HELD);
    in[0].attack=1;in[0].smash=1;versus_step(&v,in);in[0]=(CombatInput){0};
    unsigned boom=0;
    for(unsigned t=0;t<60 && !boom;++t) {
        versus_step(&v,in);
        for(unsigned e=0;e<v.sound_count;++e)if(v.sounds[e].sound==SOUND_EXPLODE_L)boom=1;
    }
    for(unsigned t=0;t<4;++t)versus_step(&v,in);  /* blast radius applies next tick */
    assert(boom && v.percent[1]>0);
    /* A held bomb explodes in Link's hand when its 300-tick fuse ends. */
    versus_init_fighters(&v,&fighter_link,&fighter_mario);in[0]=in[1]=(CombatInput){0};
    v.fighter[1].player.x=2000;in[0].down=1;in[0].special=1;versus_step(&v,in);in[0]=(CombatInput){0};
    unsigned fuse=0;
    for(unsigned t=1;t<400 && !fuse;++t) {
        versus_step(&v,in);
        for(unsigned e=0;e<v.sound_count;++e)if(v.sounds[e].sound==SOUND_EXPLODE_L)fuse=t;
    }
    assert(fuse>=325 && fuse<=332);
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
    printf("PASS: Link physics/kneebend, crouch, sword slash hit, air/ground spin, boomerang out/return/catch/hit, bomb pull/throw/explode/fuse, weight, Link-vs-Mario CPU (winner %d)\n",v.winner);
    return 0;
}
