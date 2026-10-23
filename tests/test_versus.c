#include "versus.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
static void close_pair(Versus *v)
{versus_init(v);v->fighter[0].player.x=-190;v->fighter[1].player.x=190;}
/* Step and record which sounds each fighter emitted (bit per FightSound). */
static void heard(Versus *v,const CombatInput in[2],unsigned ticks,uint64_t got[2])
{
    for(unsigned t=0;t<ticks;++t) {
        versus_step(v,in);assert(v->sound_count<=8);
        for(unsigned e=0;e<v->sound_count;++e) {
            assert(v->sounds[e].sound<SOUND_COUNT && v->sounds[e].fighter<2);
            got[v->sounds[e].fighter]|=UINT64_C(1)<<v->sounds[e].sound;
        }
    }
}
#define HAS(set,sound) (((set)>>(sound))&1)
int main(void)
{
    Versus v;CombatInput in[2]={{0},{0}};
    close_pair(&v);in[0].attack=in[1].attack=1;
    for(int t=0;t<4;++t)versus_step(&v,in);
    assert(v.percent[0]==2 && v.percent[1]==2); /* Simultaneous jabs trade. */
    assert(v.stun[0] && v.stun[1]);
    unsigned a=v.fighter[0].player.tick;versus_step(&v,in);
    assert(v.fighter[0].player.tick==a); /* Shared hitstop. */
    for(int who=0;who<2;++who) {
        close_pair(&v);in[0]=in[1]=(CombatInput){0};in[who].attack=1;
        for(int t=0;t<4;++t)versus_step(&v,in);
        assert(v.percent[1-who]==2 && v.percent[who]==0);
        for(int t=0;t<10;++t)versus_step(&v,in);
        assert(v.fighter[1-who].attack_frame<0);
    }
    close_pair(&v);v.invulnerable[1]=30;in[0].attack=1;in[1]=(CombatInput){0};
    for(int t=0;t<10;++t)versus_step(&v,in);
    assert(!v.percent[1]);
    for(int who=0;who<2;++who) {
        close_pair(&v);in[0]=in[1]=(CombatInput){0};in[who].grab=1;
        int captured=0;
        for(int t=0;t<12;++t) {versus_step(&v,in);captured|=versus_captured(&v,1-who);}
        assert(captured);
        in[who]=(CombatInput){0};versus_step(&v,in);in[who].attack=1;
        for(int t=0;t<35;++t)versus_step(&v,in);
        assert(v.percent[1-who]==12 && !versus_captured(&v,1-who));
    }
    versus_init(&v);in[0]=in[1]=(CombatInput){0};
    for(int n=0;n<3;++n) {
        v.fighter[1].player.y=-4000;versus_step(&v,in);
        assert(v.stocks[1]==(unsigned)(2-n));
    }
    assert(v.winner==0);
    versus_init(&v);v.stocks[0]=v.stocks[1]=1;
    v.fighter[0].player.y=v.fighter[1].player.y=-4000;versus_step(&v,in);assert(v.winner==2);
    versus_init(&v);unsigned hits=0;
    for(int t=0;t<1800 && v.winner<0;++t) {
        in[0]=(CombatInput){0};in[1]=versus_cpu(&v,1);versus_step(&v,in);
        hits|=v.fighter[1].hits;
    }
    assert(hits);
    for(int side=-1;side<=1;side+=2) {
        versus_init(&v);MotionScene *p=&v.fighter[1].player;
        p->x=side*2700;p->y=-500;p->vy=-5;p->state=SCENE_AIR;
        p->platform=-1;p->jumps_used=2;p->facing=-side;
        unsigned ticks=0;
        for(;ticks<250 && p->state!=SCENE_CLIFF && p->y<0;++ticks) {
            in[0]=(CombatInput){0};in[1]=versus_cpu(&v,1);versus_step(&v,in);
        }
        assert(v.stocks[1]==3 && ticks<250);
    }
    /* Original motion-script and hit sounds reach the event queue. */
    {
        uint64_t got[2]={0};close_pair(&v);in[0]=in[1]=(CombatInput){0};in[0].attack=1;
        heard(&v,in,12,got);
        assert(HAS(got[0],SOUND_SWING_S)); /* Jab1 script tick2 LightSwingS. */
        unsigned hit=mario_move_hits[mario_move_frames[mario_moves[MOVE_JAB1].first+2].first].sound;
        assert(hit>=SOUND_PUNCH_S && hit<=SOUND_KICK_L && HAS(got[1],hit));
        assert(!HAS(got[1],SOUND_VOICE_DAMAGE)); /* Below hitstun 80. */
        got[0]=got[1]=0;versus_init(&v);in[0]=(CombatInput){0};in[0].motion.jump_held=1;
        heard(&v,in,20,got);assert(HAS(got[0],SOUND_VOICE_JUMP));
        in[0]=(CombatInput){0};heard(&v,in,2,got);in[0].motion.jump_held=1;
        heard(&v,in,2,got);assert(HAS(got[0],SOUND_VOICE_JUMP_AIR));
        in[0]=(CombatInput){0};heard(&v,in,120,got);assert(HAS(got[0],SOUND_LAND));
        got[0]=got[1]=0;versus_init(&v);v.fighter[1].player.y=-4000;in[0]=in[1]=(CombatInput){0};
        heard(&v,in,1,got);
        assert(HAS(got[1],SOUND_VOICE_DEAD) && HAS(got[1],SOUND_DEAD_SLAM) && HAS(got[1],SOUND_DEAD_EXPLODE));
        /* PlaySmashVoice rotates attr smash_sfx[0..2]; FSmash tick16. */
        uint64_t voices=0;
        for(int n=0;n<3;++n) {
            got[0]=got[1]=0;unsigned keep=v.smash_voice;versus_init(&v);v.smash_voice=keep;
            assert(combat_start_move(&v.fighter[0],MOVE_FSMASH));heard(&v,in,20,got);
            assert(HAS(got[0],SOUND_SWING_L));voices|=got[0]&(UINT64_C(7)<<SOUND_VOICE_SMASH1);
        }
        assert(voices==(UINT64_C(7)<<SOUND_VOICE_SMASH1));
    }
    versus_init(&v);unsigned total=0;
    unsigned last_event=0,previous_hits=0;
    FILE *trace=fopen("build/scene/versus.csv","w");assert(trace);
    fputs("tick,p1_percent,p2_percent,p1_stocks,p2_stocks,x1,y1,x2,y2\n",trace);
    for(int t=0;t<7200 && v.winner<0;++t) {
        in[0]=versus_cpu(&v,0);in[1]=versus_cpu(&v,1);versus_step(&v,in);
        for(int i=0;i<2;++i) {
            assert(isfinite(v.fighter[i].player.x) && isfinite(v.fighter[i].player.y));
            assert(v.percent[i]<=999 && v.stocks[i]<=3);
        }
        total+=v.fighter[0].hits+v.fighter[1].hits;
        unsigned count=v.fighter[0].hits+v.fighter[1].hits+6-v.stocks[0]-v.stocks[1];
        if(count!=previous_hits) {last_event=(unsigned)t;previous_hits=count;}
        if(t%30==0)fprintf(trace,"%d,%u,%u,%u,%u,%.2f,%.2f,%.2f,%.2f\n",t,v.percent[0],v.percent[1],v.stocks[0],v.stocks[1],v.fighter[0].player.x,v.fighter[0].player.y,v.fighter[1].player.x,v.fighter[1].player.y);
    }
    fclose(trace);assert(total);
    assert(v.winner>=0 || last_event>6000);
    printf("PASS: versus trades, bidirectional damage, hitstop, grab/throw, respawn, immunity, victory/draw, original sound events, CPU and self-play; state=%u bytes\n",(unsigned)sizeof(Versus));
    return 0;
}
