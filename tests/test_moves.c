#include "combat.h"
#include "pose_sample.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static CombatInput button(int x,int y,int smash,int attack)
{CombatInput i={0};i.motion.stick_x=x;i.up=y>0;i.down=y<0;i.smash=smash;i.attack=attack;return i;}
int main(void)
{
    PoseSample sample=pose_sample(3,4,2,3,0);
    assert(pose_decode(0,-32768,65535)==-32768);
    assert(pose_decode(255,-32768,65535)==32767);
    assert(pose_decode(128,-128,255)==0);
    assert(sample.a==1 && sample.b==2 && sample.part==1 && sample.span==1);
    assert(pose_blend(500,-500,sample)==-500);
    sample=pose_sample(1,5,2,3,0);assert(pose_blend(500,-500,sample)==0);
    sample=pose_sample(25,25,2,13,1);assert(sample.a==0 && sample.part==0);
    CombatScene s;combat_init(&s);
    combat_update(&s,button(80,0,0,1));assert(s.move==MOVE_FTILT);
    combat_init(&s);combat_update(&s,button(0,1,0,1));assert(s.move==MOVE_UTILT);
    combat_init(&s);combat_update(&s,button(0,-1,1,1));assert(s.move==MOVE_DSMASH);
    combat_init(&s);combat_update(&s,button(80,1,1,1));assert(s.move==MOVE_FSMASH_HI);
    combat_init(&s);combat_update(&s,button(80,0,0,0));combat_update(&s,button(0,0,0,0));
    combat_update(&s,button(80,0,0,1));assert(s.move==MOVE_DASH);
    combat_init(&s);s.player.state=SCENE_AIR;s.player.y=1200;s.player.jumps_used=1;
    combat_update(&s,button(-80,0,0,0));assert(s.player.facing==1);
    combat_update(&s,button(-80,0,0,1));assert(s.move==MOVE_BAIR);
    combat_init_stage(&s,STAGE_DREAMLAND);s.player.x=-2000;s.target.x=2000;
    combat_update(&s,button(80,0,0,0));combat_update(&s,button(0,0,0,0));
    for(int t=0;t<35;++t)combat_update(&s,button(80,0,0,0));
    assert(s.dash_ticks && s.run_age==35 && s.player.vx==44);
    combat_update(&s,button(0,0,0,0));assert(!s.dash_ticks && s.brake_ticks);
    combat_init(&s);s.captured=1;combat_start_move(&s,MOVE_GRAB_HOLD);
    for(int t=0;t<60;++t)combat_update(&s,(CombatInput){0});
    assert(s.captured && s.attack_frame==fighter_mario.moves[MOVE_GRAB_HOLD].ticks-1);
    combat_init(&s);s.target.x=1500;
    int seen2=0,seen3=0;
    for(int t=0;t<90;++t) {
        combat_update(&s,button(0,0,0,t==0 || t==5 || t==18));
        if(s.move==MOVE_JAB2)seen2=1;if(s.move==MOVE_JAB3)seen3=1;
    }
    assert(seen2 && seen3);
    FILE *report=fopen("build/scene/moves.csv","w");assert(report);
    fputs("move,hits,peak_percent,captured,ko\n",report);
    /* Slots from MOVE_UP_FALL on are Kirby statuses that Mario never starts. */
    for(int m=0;m<MOVE_UP_FALL;++m) {
        combat_demo_begin(&s,(unsigned)m);assert(s.attack_frame==0);
        int captured=s.captured,visible=0;unsigned peak=s.percent;
        for(int t=0;t<240;++t) {
            if(combat_attack_active(&s))visible=1;
            CombatInput in={0};
            if(s.captured && s.move==MOVE_GRAB_HOLD && t==40)in.attack=1;
            combat_update(&s,in);if(s.captured)captured=1;
            if(s.percent>peak)peak=s.percent;
            assert(isfinite(s.target.x) && isfinite(s.player.y));
        }
        fprintf(report,"%s,%u,%u,%d,%u\n",fighter_mario.moves[m].name,s.hits,peak,captured,s.knockouts);
        if(m==MOVE_GRAB)assert(captured);
        if(m==MOVE_THROW_F)assert(peak==12);
        if(m==MOVE_THROW_B)assert(peak==16);
        if(m==MOVE_FIRE || m==MOVE_FIRE_AIR)assert(s.hits>0);
        if(m==MOVE_DAIR || m==MOVE_TORNADO || m==MOVE_TORNADO_AIR)assert(s.hits>1);
        if(m<=MOVE_DAIR || m==MOVE_UP || m==MOVE_GETUP_D || m==MOVE_GETUP_U)assert(visible);
    }
    fclose(report);
    /* A single projectile cannot hit twice, and a held B cannot create more. */
    combat_init(&s);CombatInput fire={0};fire.special=1;
    for(int t=0;t<150;++t)combat_update(&s,fire);
    assert(s.hits==1 && s.percent==7);
    puts("PASS: move selection, jab combo, all move playback, throws, projectiles and multihits");
    return 0;
}
