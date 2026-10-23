/* Original offline animation/event data; local reduced combat integration.
 * The passive target uses a 2D hurt capsule, not the full N64 damage engine. */
#include "combat.h"
#include "motion.h"
#include <string.h>
#include "damage_angles.h"
static int air(const MotionScene *p) {return p->state==SCENE_AIR;}
static int ground(const MotionScene *p) {return p->state==SCENE_WAIT || p->state==SCENE_WALK;}
static int hurt_air(const MotionScene *p)
{return p->state==SCENE_AIR || p->state==SCENE_SPECIAL_HI || p->state==SCENE_SPECIAL_FALL || p->state==SCENE_CLIFF || p->state==SCENE_CLIMB;}
static int tornado(MarioMove m) {return m==MOVE_TORNADO || m==MOVE_TORNADO_AIR;}
static const MoveFrame *at(MarioMove m,unsigned t)
{const MoveDesc *d=&mario_moves[m];if(t>=d->ticks)t=d->ticks-1;return &mario_move_frames[d->first+t];}

float combat_knockback(unsigned p,const MoveHit *h)
{
    float n=h->weight?1+h->weight*.5f:(p+h->damage)*.1f+(p+h->damage)*h->damage*.05f;
    float kb=(n*1.4f+18)*h->scale*.01f+h->base;return kb>2500?2500:kb;
}
float combat_jab_knockback(unsigned p)
{MoveHit h={0};h.damage=2;h.scale=50;h.base=8;return combat_knockback(p,&h);}
void combat_init_stage(CombatScene *s,StageId stage)
{
    memset(s,0,sizeof *s);scene_init_stage(&s->player,stage);scene_init_stage(&s->target,stage);
    s->player.x=-200;s->target.x=180;s->facing=s->attack_facing=1;s->target.facing=-1;
    s->attack_frame=-1;s->tornado_charge=1;s->tap_age=100;
}
void combat_init(CombatScene *s) {combat_init_stage(s,STAGE_GARDEN);}

int combat_start_move(CombatScene *s,MarioMove m)
{
    if((unsigned)m>=MOVE_COUNT || s->hitstop)return 0;
    if(m==MOVE_UP) {
        if(!scene_special_hi(&s->player,s->player.facing*80))return 0;
    } else if(m==MOVE_LEDGE_START || m==MOVE_LEDGE_SLOW_START) {
        if(s->player.state!=SCENE_CLIFF || s->player.state_ticks<SCENE_CATCH_TICKS)return 0;
    } else if(m==MOVE_LEDGE || m==MOVE_LEDGE_SLOW) {
        s->player.x=s->player.cliff_side*(2318-180);s->player.y=0;
        s->player.vx=s->player.vy=0;s->player.state=SCENE_WAIT;s->player.platform=0;
    } else if(m==MOVE_THROW_F || m==MOVE_THROW_B || m==MOVE_GRAB_HOLD) {
        if(!s->captured)return 0;
    } else if((m>=MOVE_NAIR && m<=MOVE_DAIR) || m==MOVE_FIRE_AIR || m==MOVE_TORNADO_AIR) {
        if(!air(&s->player))return 0;
    } else if(!ground(&s->player))return 0;
    s->move=m;s->attack_frame=0;s->attack_connected=0;s->connected_strikes=0;
    s->attack_facing=s->player.facing;s->queued_combo=0;s->spawned=0;
    if(m==MOVE_DASH)s->player.vx=s->attack_facing*44;
    if(tornado(m)) {
        s->player.vx=motion_ground_clamp(s->player.vx,17);
        if(air(&s->player))s->player.vy=-7;
    }
    return 1;
}
unsigned combat_player_clip(const CombatScene *s)
{return s->attack_frame<0?0:mario_moves[s->move].clip;}
int combat_attack_active(const CombatScene *s)
{return s->attack_frame>=0 && at(s->move,(unsigned)s->attack_frame)->count>0;}

static int overlap(const MotionScene *p,float x,float y,float radius)
{
    float yy=y;if(yy<p->y+80)yy=p->y+80;if(yy>p->y+330)yy=p->y+330;
    float dx=p->x-x,dy=yy-y,r=radius+70;return dx*dx+dy*dy<=r*r;
}
static void damage(CombatScene *s,const MoveHit *h,int dir,float impact_y)
{
    float kb=combat_knockback(s->percent,h),cs=h->cos_angle/4096.0f,sn=h->sin_angle/4096.0f;
    int airborne=hurt_air(&s->target);
    /* US ftCommonDamageGetKnockbackAngle: 43 air / 0..42.5 ground.
     * Half-degree lookup bounds angle error to .25 degrees, no runtime trig. */
    if(h->angle==361) {
        float angle=airborne?43:kb<32?0:(kb-32)/.099998474f*42.5f+1;
        if(!airborne && angle>42.5f)angle=42.5f;
        unsigned index=(unsigned)(angle*2+.5f);
        cs=damage_angles[index][0]/4096.0f;sn=damage_angles[index][1]/4096.0f;
    }
    s->damage_kb=kb;s->damage_vx=kb*cs*dir;s->damage_vy=kb*sn;
    s->target.vx=0;s->target.vy=s->duel_mode?0:s->damage_vy;
    s->target.hit_vx=s->target.hit_vy=0;
    s->target.facing=-dir;s->target.air_jump=0;
    if(s->damage_vy!=0 || airborne) {
        s->target.state=SCENE_AIR;s->target.platform=-1;
        if(!airborne)s->target.jumps_used=1;
    } else {
        s->target.state=SCENE_WAIT;s->target.state_ticks=0;
    }
    s->hitstun=(unsigned)(kb/1.875f);if(!s->hitstun)s->hitstun=1;
    unsigned level=s->hitstun<12?0:s->hitstun<24?1:s->hitstun<32?2:3;
    float height=impact_y-s->target.y;
    unsigned zone=height>280?0:height<140?2:1;
    s->damage_pose=level<3?(airborne?9+level:zone*3+level):12+zone;
    if(level==3 && sn>.93969f)s->damage_pose=15;
    s->damage_amount=h->damage;s->damage_sound=h->sound;
    s->percent+=h->damage;if(s->percent>999)s->percent=999;
    ++s->hits;s->hitstop=h->damage/3+5;
}
static void release_throw(CombatScene *s)
{
    MoveHit h={0};h.damage=s->move==MOVE_THROW_B?16:12;h.angle=45;h.scale=70;
    h.base=s->move==MOVE_THROW_B?70:80;h.cos_angle=h.sin_angle=2896;
    int dir=s->attack_facing*(s->move==MOVE_THROW_B?-1:1);
    s->target.x=s->player.x+dir*220;s->captured=0;damage(s,&h,dir,s->target.y+180);
}
static void spawn(CombatScene *s,const MoveFrame *f)
{
    for(unsigned i=0;i<4;++i)if(!s->fireballs[i].life) {
        Fireball *p=&s->fireballs[i];p->x=s->player.x+f->spawn_x*s->attack_facing;
        p->y=s->player.y+f->spawn_y;p->vx=s->attack_facing*49.8097349f;
        p->vy=-4.3577871f;p->life=140;p->facing=s->attack_facing;break;
    }
}
static void projectiles(CombatScene *s)
{
    for(unsigned j=0;j<4;++j) {
        Fireball *b=&s->fireballs[j];if(!b->life)continue;--b->life;
        float old_y=b->y;b->vy=motion_gravity(b->vy,1.2f,55);b->x+=b->vx;b->y+=b->vy;
        unsigned n;const StagePlatform *p=stage_platforms(s->player.stage,&n);
        for(unsigned i=0;i<n;++i)if(b->vy<0 && b->x>=p[i].left && b->x<=p[i].right &&
           old_y-50>=p[i].height && b->y-50<=p[i].height) {
            b->y=p[i].height+50;b->vy=-b->vy*.85f;b->vx*=.85f;
            if(b->vx*b->vx+b->vy*b->vy<900)b->life=0;
            break;
        }
        if(b->life && !s->captured && !s->target_immune && overlap(&s->target,b->x,b->y,100)) {
            /* US MarioSpecial1 fireball attributes: sfx 28 = BurnS. */
            MoveHit h={0};h.damage=7;h.angle=361;h.scale=25;h.base=10;h.sound=SOUND_BURN_S;
            damage(s,&h,b->facing,b->y);b->life=0;
        }
    }
}
static MarioMove select_normal(const CombatScene *s,CombatInput in)
{
    if(s->player.state==SCENE_CLIFF)return in.smash?MOVE_LEDGE_SLOW_START:MOVE_LEDGE_START;
    int x=in.motion.stick_x;
    if(air(&s->player)) {
        if(in.up)return MOVE_UAIR;
        if(in.down)return MOVE_DAIR;
        if(x)return x*s->player.facing>=0?MOVE_FAIR:MOVE_BAIR;
        return MOVE_NAIR;
    }
    if(in.grab && in.smash && !x)return in.down?MOVE_GETUP_D:MOVE_GETUP_U;
    if(in.grab && !x && !in.up && !in.down)return MOVE_GETUP_D;
    if(in.smash) {
        if(x) {
            if(in.up)return in.variant?MOVE_FSMASH_MHI:MOVE_FSMASH_HI;
            if(in.down)return in.variant?MOVE_FSMASH_MLO:MOVE_FSMASH_LO;
            return MOVE_FSMASH;
        }
        return in.up?MOVE_USMASH:in.down?MOVE_DSMASH:MOVE_FSMASH;
    }
    if(x) {
        if(s->dash_ticks)return MOVE_DASH;
        return in.up?MOVE_FTILT_HI:in.down?MOVE_FTILT_LO:MOVE_FTILT;
    }
    return in.up?MOVE_UTILT:in.down?MOVE_DTILT:MOVE_JAB1;
}

void combat_update(CombatScene *s,CombatInput in)
{
    ++s->tick;
    int pressed=in.attack && !s->attack_was_held;
    int special=in.special && !s->special_was_held,grab=in.grab && !s->grab_was_held;
    s->attack_was_held=!!in.attack;s->special_was_held=!!in.special;s->grab_was_held=!!in.grab;
    if(pressed && s->attack_frame>=0 && (s->move==MOVE_JAB1 || s->move==MOVE_JAB2))s->queued_combo=1;
    if(s->hitstop) {--s->hitstop;return;}
    if(s->hitstun)--s->hitstun;
    if(s->brake_ticks)--s->brake_ticks;
    if(s->tap_age<100)++s->tap_age;
    int dir=in.motion.stick_x>0?1:in.motion.stick_x<0?-1:0;
    if(s->dash_ticks && (!dir || dir!=s->last_stick || !ground(&s->player) || s->attack_frame>=0)) {
        if(!dir && ground(&s->player) && s->attack_frame<0)s->brake_ticks=24;
        s->dash_ticks=0;s->run_age=0;
    }
    if(dir && dir!=s->last_stick && ground(&s->player)) {
        if(dir==s->last_tap && s->tap_age<=10 && s->attack_frame<0) {s->dash_ticks=1;s->run_age=0;}
        s->last_tap=dir;s->tap_age=0;
    }
    s->last_stick=dir;
    if(dir)s->brake_ticks=0;
    if(s->dash_ticks)++s->run_age;
    if(s->player.state==SCENE_SPECIAL_HI && s->attack_frame<0) {
        s->move=MOVE_UP;s->attack_frame=(int)s->player.state_ticks;
        s->attack_facing=s->player.facing;s->connected_strikes=0;s->spawned=0;
    }
    if(s->attack_frame>=0) {
        if(s->queued_combo && ((s->move==MOVE_JAB1 && s->attack_frame>=10) ||
            (s->move==MOVE_JAB2 && s->attack_frame>=8)))
            combat_start_move(s,s->move==MOVE_JAB1?MOVE_JAB2:MOVE_JAB3);
        else if(++s->attack_frame>=mario_moves[s->move].ticks) {
            MarioMove old=s->move;s->attack_frame=-1;
            if(old==MOVE_LEDGE_START || old==MOVE_LEDGE_SLOW_START)
                combat_start_move(s,old==MOVE_LEDGE_START?MOVE_LEDGE:MOVE_LEDGE_SLOW);
            else if(old==MOVE_GRAB && s->captured)combat_start_move(s,MOVE_GRAB_HOLD);
            else if(old==MOVE_GRAB_HOLD && s->captured)s->attack_frame=mario_moves[old].ticks-1;
            if(tornado(old) && air(&s->player))s->tornado_charge=0;
        }
    }
    if(s->captured && s->move==MOVE_GRAB_HOLD) {
        ++s->capture_age;
        if(pressed || (dir && s->capture_age>8))
            combat_start_move(s,dir*s->attack_facing<0?MOVE_THROW_B:MOVE_THROW_F);
        else if(s->capture_age>90) {s->captured=0;s->attack_frame=-1;s->target.state=SCENE_AIR;}
    }
    if(s->attack_frame<0) {
        if(special) {
            MarioMove m=in.up?MOVE_UP:in.down?(air(&s->player)?MOVE_TORNADO_AIR:MOVE_TORNADO):
                (air(&s->player)?MOVE_FIRE_AIR:MOVE_FIRE);
            if(dir && m!=MOVE_UP)s->player.facing=dir;
            if(m==MOVE_UP) {
                if(scene_special_hi(&s->player,in.motion.stick_x)) {
                    s->move=m;s->attack_frame=0;s->attack_facing=s->player.facing;s->connected_strikes=0;
                }
            } else combat_start_move(s,m);
        } else if(pressed) {
            MarioMove m=select_normal(s,in);
            if(!air(&s->player) && dir)s->player.facing=dir;
            combat_start_move(s,m);
        } else if(grab && !in.smash)combat_start_move(s,MOVE_GRAB);
    }
    const MoveFrame *f=s->attack_frame>=0?at(s->move,(unsigned)s->attack_frame):0;
    unsigned old_respawns=s->player.respawns,old_landings=s->player.landings,old_grabs=s->player.cliff_grabs;
    SceneInput motion=in.motion;
    if(f) {
        motion.jump_held=0;if(!air(&s->player))motion.stick_x=0;
        if(tornado(s->move)) {
            if(special && s->tornado_charge && (f->flags&8)) {
                s->player.vy+=22;if(s->player.vy>40)s->player.vy=40;
                s->player.state=SCENE_AIR;s->player.platform=-1;s->player.jumps_used=1;
            }
            float vx=motion_ground_clamp(s->player.vx+in.motion.stick_x*(air(&s->player)?.03f:.025f),17);
            s->player.vx=vx+(air(&s->player)?(vx>0?.2f:vx<0?-.2f:0):(vx>0?6:vx<0?-6:0));
            motion.stick_x=0;
        }
        if(s->move==MOVE_GRAB_HOLD || s->move==MOVE_THROW_F || s->move==MOVE_THROW_B)s->player.vx=0;
    } else if(s->dash_ticks && dir && ground(&s->player)) {
        /* Bypass the walking clamp: scene_step otherwise replaces 44 with 24. */
        s->player.vx=dir*50;motion.stick_x=0;s->player.facing=dir;
    }
    /* Preserve aerial facing while steering; back-air remains reachable when
     * the direction is pressed before X, rather than only on the same tick. */
    int previous_facing=s->player.facing;
    if(in.drop && !f)scene_drop_through(&s->player);
    scene_step(&s->player,motion);
    if(air(&s->player))s->player.facing=previous_facing;
    if(f)s->player.facing=s->attack_facing;
    s->facing=s->player.facing;
    if(s->player.respawns!=old_respawns) {s->attack_frame=-1;s->captured=0;f=0;}
    if(s->player.landings!=old_landings || s->player.cliff_grabs!=old_grabs) {
        s->tornado_charge=1;
        if(s->attack_frame>=0 && (s->player.cliff_grabs!=old_grabs ||
           (s->move>=MOVE_NAIR && s->move<=MOVE_DAIR) || s->move==MOVE_UP)) {
            s->attack_frame=-1;f=0;
        }
    }
    unsigned target_respawns=s->target.respawns;
    if(!s->captured && !s->duel_mode) {
        if(!air(&s->target) && s->damage_vx!=0)s->target.vx=s->damage_vx+(s->damage_vx>0?6:-6);
        else s->target.vx=s->damage_vx;
        if(s->hitstun || ((s->player.state==SCENE_CLIFF || s->player.state==SCENE_CLIMB) &&
            s->player.cliff_side==(s->target.x<0?-1:1)))s->target.cliff_cooldown=2;
        scene_step(&s->target,(SceneInput){0,0});s->damage_vx=motion_ground_friction(s->damage_vx,.8f);
    } else if(s->captured) {
        /* Original joint28 attachment position; target orientation remains a
         * reduced 2D approximation of CapturePulledRotateScale. */
        s->target.x=s->player.x+s->attack_facing*(f?f->grab_x:180);
        s->target.y=s->player.y+(f?f->grab_y-180:40);
        s->target.vx=s->target.vy=0;
    }
    if(s->target.respawns!=target_respawns) {
        ++s->knockouts;s->percent=s->hitstun=0;s->damage_vx=0;s->target.x=180;
    }
    if(!s->duel_mode)combat_resolve(s);
}
void combat_resolve(CombatScene *s)
{
    const MoveFrame *f=s->attack_frame>=0?at(s->move,(unsigned)s->attack_frame):0;
    if(s->captured && f) {
        s->target.x=s->player.x+s->attack_facing*f->grab_x;
        s->target.y=s->player.y+f->grab_y-180;
        s->target.vx=s->target.vy=0;
    }
    if(f) {
        if((s->move==MOVE_FIRE || s->move==MOVE_FIRE_AIR) && !s->spawned && (f->flags&1)) {spawn(s,f);s->spawned=1;}
        if((s->move==MOVE_THROW_F || s->move==MOVE_THROW_B) && s->captured && (f->flags&4))release_throw(s);
        for(unsigned i=0;i<f->count && !s->captured && !s->target_immune;++i) {
            const MoveHit *h=&mario_move_hits[f->first+i];uint64_t bit=UINT64_C(1)<<h->strike;
            if(s->connected_strikes&bit)continue;
            if(!(h->ground_air&(hurt_air(&s->target)?1:2)))continue;
            if(overlap(&s->target,s->player.x+h->x*s->attack_facing,s->player.y+h->y,h->radius)) {
                s->connected_strikes|=bit;s->attack_connected=1;
                if(s->move==MOVE_GRAB) {
                    s->captured=1;s->capture_age=0;s->damage_vx=0;
                    s->target.facing=-s->attack_facing;
                    combat_start_move(s,MOVE_GRAB_HOLD);
                    break;
                }
                else damage(s,h,(s->target.x-s->player.x)>=0?1:-1,s->player.y+h->y);
            }
        }
    }
    projectiles(s);
}
void combat_step(CombatScene *s,SceneInput in,int attack,int drop)
{CombatInput c={0};c.motion=in;c.attack=attack;c.drop=drop;combat_update(s,c);}

void combat_demo_begin(CombatScene *s,unsigned index)
{
    MarioMove m=(MarioMove)(index%MOVE_COUNT);combat_init_stage(s,STAGE_DREAMLAND);
    if((m>=MOVE_NAIR && m<=MOVE_DAIR) || m==MOVE_FIRE_AIR || m==MOVE_TORNADO_AIR) {
        s->player.state=s->target.state=SCENE_AIR;s->player.platform=s->target.platform=-1;
        s->player.y=1000;s->target.y=1000;s->player.jumps_used=1;
    }
    if(m==MOVE_THROW_F || m==MOVE_THROW_B || m==MOVE_GRAB_HOLD)s->captured=1;
    if(m==MOVE_LEDGE_START || m==MOVE_LEDGE_SLOW_START || m==MOVE_LEDGE || m==MOVE_LEDGE_SLOW) {
        s->player.x=-2498;s->player.y=-300;s->player.state=SCENE_CLIFF;
        s->player.cliff_side=-1;s->player.facing=1;s->player.state_ticks=20;s->target.x=-2100;
    }
    const MoveDesc *d=&mario_moves[m];
    if(m==MOVE_FIRE)s->target.x=1400;
    for(unsigned t=0;t<d->ticks;++t) {
        const MoveFrame *f=at(m,t);
        if(f->count) {
            const MoveHit *h=&mario_move_hits[f->first];s->target.x=s->player.x+h->x;
            s->target.y=s->player.y+h->y-180;if(s->target.y<0)s->target.y=0;break;
        }
    }
    combat_start_move(s,m);
}
