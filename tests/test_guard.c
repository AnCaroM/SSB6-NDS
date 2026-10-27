/* Prueba19: shield, rolls, techs, knockdowns and shield break in the 1v1
 * (reduced ftcommonguard/escape/passive/down/shieldbreak/furafura). */
#include "versus.h"
#include <assert.h>
#include <stdio.h>
extern const Fighter fighter_link;

static int has_sound(const Versus *v,unsigned sound)
{for(unsigned e=0;e<v->sound_count;++e)if(v->sounds[e].sound==sound)return 1;return 0;}
/* Mario (side 1) launched by Mario's up smash, as in test_damage.c. */
static Versus launched(void)
{
    Versus v;versus_init(&v);CombatScene s;combat_demo_begin(&s,MOVE_USMASH);s.duel_mode=1;
    unsigned t=0;
    while(t<fighter_mario.moves[MOVE_USMASH].ticks && !fighter_mario.frames[fighter_mario.moves[MOVE_USMASH].first+t].count)++t;
    s.attack_frame=(int)t-1;v.fighter[0]=s;v.fighter[1].player=s.target;v.percent[1]=100;
    CombatInput in[2]={{0},{0}};versus_step(&v,in);
    assert(v.stun[1] && v.fighter[1].player.state==SCENE_AIR);v.freeze=0;
    return v;
}
int main(void)
{
    const FighterAction *acts=fighter_mario.actions;
    assert(acts && fighter_link.actions && acts[ACTION_GUARD_ON].ticks==5 && acts[ACTION_ESCAPE_F].dx);
    assert(acts[ACTION_ESCAPE_F].intangible_from==4 && acts[ACTION_ESCAPE_F].intangible_to==20 && acts[ACTION_ESCAPE_F].turn_tick==20);
    Versus v;CombatInput in[2]={{0},{0}};
    /* Shield up: GuardOn (5 ticks) then Guard while held, GuardOff on release. */
    versus_init(&v);in[0].shield=1;
    versus_step(&v,in);
    assert(v.fighter[0].action==ACTION_GUARD_ON && has_sound(&v,SOUND_GUARD_ON) && combat_shield_radius(&v.fighter[0])>250);
    for(unsigned t=0;t<6;++t)versus_step(&v,in);
    assert(v.fighter[0].action==ACTION_GUARD && v.fighter[0].shield_health==55);
    for(unsigned t=0;t<16;++t)versus_step(&v,in);
    assert(v.fighter[0].shield_health==54); /* one point every 16 ticks */
    in[0].shield=0;versus_step(&v,in);
    assert(v.fighter[0].action==ACTION_GUARD_OFF && has_sound(&v,SOUND_GUARD_OFF) && combat_shield_radius(&v.fighter[0])==0);
    for(unsigned t=0;t<13;++t)versus_step(&v,in);
    assert(v.fighter[0].action==ACTION_NONE);
    for(unsigned t=0;t<10;++t)versus_step(&v,in);
    assert(v.fighter[0].shield_health==55); /* heals one point per 10 ticks */
    /* Jumping and grabbing leave the shield; a stick tap rolls. */
    versus_init(&v);in[0]=(CombatInput){0};in[0].shield=1;
    for(unsigned t=0;t<8;++t)versus_step(&v,in);
    in[0].motion.jump_held=1;versus_step(&v,in);
    assert(v.fighter[0].action==ACTION_NONE && v.fighter[0].player.state==SCENE_KNEE);
    versus_init(&v);in[0]=(CombatInput){0};in[0].shield=1;
    for(unsigned t=0;t<8;++t)versus_step(&v,in);
    in[0].grab=1;versus_step(&v,in);
    assert(v.fighter[0].action==ACTION_NONE && v.fighter[0].move==MOVE_GRAB && v.fighter[0].attack_frame>=0);
    /* Forward roll: TransN movement, intangible 4..20, turns at 20, stays on the stage. */
    versus_init(&v);in[0]=(CombatInput){0};in[0].shield=1;
    for(unsigned t=0;t<8;++t)versus_step(&v,in);
    float x0=v.fighter[0].player.x;in[0].motion.stick_x=80;
    versus_step(&v,in);
    assert(v.fighter[0].action==ACTION_ESCAPE_F && has_sound(&v,SOUND_ESCAPE) && !combat_intangible(&v.fighter[0]));
    in[0].motion.stick_x=0;in[0].shield=0; /* still held after the roll would shield again */
    for(unsigned t=0;t<8;++t)versus_step(&v,in);
    assert(combat_intangible(&v.fighter[0]) && v.fighter[0].player.facing==1);
    for(unsigned t=0;t<16;++t)versus_step(&v,in);
    assert(v.fighter[0].action==ACTION_ESCAPE_F && v.fighter[0].player.facing==1); /* drawn facing holds until the end */
    for(unsigned t=0;t<14;++t)versus_step(&v,in);
    assert(v.fighter[0].action==ACTION_NONE && v.fighter[0].player.facing==-1);
    assert(v.fighter[0].player.x-x0>850 && v.fighter[0].player.x-x0<950); /* the full 900 of RollF's TransN */
    v.fighter[0].player.x=2250;in[0].shield=1;
    for(unsigned t=0;t<8;++t)versus_step(&v,in);
    in[0].motion.stick_x=80;versus_step(&v,in); /* facing -1: the back roll goes right */
    assert(v.fighter[0].action==ACTION_ESCAPE_B);in[0].motion.stick_x=0;in[0].shield=0;
    for(unsigned t=0;t<40;++t)versus_step(&v,in);
    assert(v.fighter[0].player.x<=2318 && v.fighter[0].player.state!=SCENE_AIR);
    /* Blocked jab: no damage, shield loses the hit's damage, push-back and hitlag. */
    versus_init_fighters(&v,&fighter_link,&fighter_mario);in[0]=in[1]=(CombatInput){0};
    v.fighter[0].player.x=-200;v.fighter[1].player.x=150;in[1].shield=1;
    for(unsigned t=0;t<8;++t)versus_step(&v,in);
    in[0].attack=1;unsigned blocked=0;
    for(unsigned t=0;t<24 && !blocked;++t) {versus_step(&v,in);blocked=v.fighter[1].setoff_ticks;}
    assert(blocked && v.percent[1]==0 && v.fighter[1].shield_health<55 && v.fighter[1].player.vx>0 && v.freeze);
    assert(v.fighter[1].action==ACTION_GUARD && v.fighter[0].attack_connected);
    /* Grabs go through shields. */
    versus_init_fighters(&v,&fighter_link,&fighter_mario);in[0]=in[1]=(CombatInput){0};
    v.fighter[0].player.x=-150;v.fighter[1].player.x=150;in[1].shield=1;
    for(unsigned t=0;t<8;++t)versus_step(&v,in);
    in[0].grab=1;
    for(unsigned t=0;t<30 && !v.fighter[0].captured;++t)versus_step(&v,in);
    assert(v.fighter[0].captured && v.fighter[1].action==ACTION_NONE);
    /* Shield break: launched straight up, lands, stands and is dizzy; mashing shortens it. */
    for(unsigned mash=0;mash<2;++mash) {
        versus_init_fighters(&v,&fighter_link,&fighter_mario);in[0]=in[1]=(CombatInput){0};
        v.fighter[0].player.x=-200;v.fighter[1].player.x=150;in[1].shield=1;
        for(unsigned t=0;t<8;++t)versus_step(&v,in);
        v.fighter[1].shield_health=1;in[0].attack=1;
        for(unsigned t=0;t<24 && v.fighter[1].action!=ACTION_SHIELD_BREAK_FLY;++t)versus_step(&v,in);
        assert(v.fighter[1].action==ACTION_SHIELD_BREAK_FLY && has_sound(&v,SOUND_SHIELD_BREAK));
        assert(v.fighter[1].player.state==SCENE_AIR && v.fighter[1].player.vy==70 && v.fighter[1].shield_health==30);
        in[0]=(CombatInput){0};in[1]=(CombatInput){0};v.fighter[0].player.x=-900;
        unsigned t=0;
        while(t<400 && v.fighter[1].action!=ACTION_FURAFURA) {versus_step(&v,in);++t;}
        assert(v.fighter[1].action==ACTION_FURAFURA && v.fighter[1].breakout_wait==490);
        unsigned dizzy=0;
        while(dizzy<600 && v.fighter[1].action==ACTION_FURAFURA) {in[1].attack=mash && (dizzy&1);versus_step(&v,in);++dizzy;}
        assert(mash?dizzy<300:dizzy>=490);
        assert(v.fighter[1].shield_health==30 && v.fighter[1].action==ACTION_NONE);
    }
    /* Holding the shield until it is empty breaks it too. */
    versus_init(&v);in[0]=in[1]=(CombatInput){0};in[0].shield=1;
    for(unsigned t=0;t<55*16+8 && v.fighter[0].action!=ACTION_SHIELD_BREAK_FLY;++t)versus_step(&v,in);
    assert(v.fighter[0].action==ACTION_SHIELD_BREAK_FLY);
    /* Landing from a launch: knockdown, wait, then get-up attack / stand / roll. */
    v=launched();in[0]=in[1]=(CombatInput){0};
    v.fighter[1].player.y=10;v.fighter[1].player.x=0;v.fighter[1].player.vy=-20;v.launch_y[1]=-40;
    versus_step(&v,in);
    assert(v.fighter[1].action==ACTION_DOWN_BOUNCE_U && !v.stun[1] && has_sound(&v,SOUND_DOWN_BOUNCE));
    for(unsigned t=0;t<27;++t)versus_step(&v,in);
    assert(v.fighter[1].action==ACTION_DOWN_WAIT_U && v.fighter[1].down_wait==180);
    in[1].attack=1;versus_step(&v,in);
    assert(v.fighter[1].action==ACTION_NONE && v.fighter[1].move==MOVE_GETUP_U && v.fighter[1].attack_frame>=0);
    v=launched();in[0]=in[1]=(CombatInput){0};
    v.fighter[1].player.y=10;v.fighter[1].player.x=0;v.fighter[1].player.vy=-20;v.launch_y[1]=-40;
    for(unsigned t=0;t<28;++t)versus_step(&v,in);
    assert(v.fighter[1].action==ACTION_DOWN_WAIT_U);
    in[1].up=1;versus_step(&v,in);in[1].up=0;
    assert(v.fighter[1].action==ACTION_DOWN_STAND_U);
    for(unsigned t=0;t<31;++t)versus_step(&v,in);
    assert(v.fighter[1].action==ACTION_NONE);
    v=launched();in[0]=in[1]=(CombatInput){0};
    v.fighter[1].player.y=10;v.fighter[1].player.x=0;v.fighter[1].player.vy=-20;v.launch_y[1]=-40;
    for(unsigned t=0;t<28;++t)versus_step(&v,in);
    float before=v.fighter[1].player.x;int facing=v.fighter[1].player.facing;
    in[1].motion.stick_x=-80*facing;versus_step(&v,in);
    assert(v.fighter[1].action==ACTION_DOWN_BACK_U);in[1].motion.stick_x=0;
    for(unsigned t=0;t<40;++t)versus_step(&v,in);
    assert(v.fighter[1].action==ACTION_NONE && (v.fighter[1].player.x-before)*facing< -900); /* 1050 by TransN */
    v=launched();in[0]=in[1]=(CombatInput){0};
    v.fighter[1].player.y=10;v.fighter[1].player.x=0;v.fighter[1].player.vy=-20;v.launch_y[1]=-40;
    for(unsigned t=0;t<1+27+179;++t)versus_step(&v,in);
    assert(v.fighter[1].action==ACTION_DOWN_WAIT_U && v.fighter[1].down_wait==1);
    versus_step(&v,in);
    assert(v.fighter[1].action==ACTION_DOWN_STAND_U); /* automatic stand-up after 180 ticks */
    /* Tech: shield pressed within 20 ticks of landing; with the stick it rolls. */
    v=launched();in[0]=in[1]=(CombatInput){0};
    v.fighter[1].player.y=10;v.fighter[1].player.x=0;v.fighter[1].player.vy=-20;v.launch_y[1]=-40;
    in[1].shield=1;versus_step(&v,in);
    assert(v.fighter[1].action==ACTION_PASSIVE && combat_intangible(&v.fighter[1]) && has_sound(&v,SOUND_VOICE_JUMP));
    v=launched();in[0]=in[1]=(CombatInput){0};
    v.fighter[1].player.y=10;v.fighter[1].player.x=0;v.fighter[1].player.vy=-20;v.launch_y[1]=-40;
    in[1].shield=1;in[1].motion.stick_x=80*v.fighter[1].player.facing;versus_step(&v,in);
    assert(v.fighter[1].action==ACTION_PASSIVE_F);
    /* CPU self-play with shields stays bounded and uses the shield. */
    versus_init_fighters(&v,&fighter_link,&fighter_mario);unsigned guards=0;
    for(unsigned t=0;t<4000 && v.winner<0;++t) {
        CombatInput cpu[2]={versus_cpu(&v,0),versus_cpu(&v,1)};versus_step(&v,cpu);
        for(unsigned i=0;i<2;++i)guards+=v.fighter[i].action==ACTION_GUARD_ON;
        for(unsigned i=0;i<2;++i)assert(v.fighter[i].shield_health>=0 && v.fighter[i].shield_health<=55 && v.fighter[i].action<ACTION_COUNT);
    }
    assert(guards);
    puts("PASS guard: shield timing/decay/heal, blocked hits, rolls, techs, knockdown, shield break and CPU");
}
