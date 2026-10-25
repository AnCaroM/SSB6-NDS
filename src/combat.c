/* Original offline animation/event data; local reduced combat integration.
 * The passive target uses a 2D hurt capsule, not the full N64 damage engine. */
#include "combat.h"
#include "motion.h"
#include <string.h>
#include <math.h>
#include "damage_angles.h"
static int air(const MotionScene *p) {return p->state==SCENE_AIR;}
static int ground(const MotionScene *p) {return p->state==SCENE_WAIT || p->state==SCENE_WALK || p->state==SCENE_SQUAT;}
static int hurt_air(const MotionScene *p)
{return p->state==SCENE_AIR || p->state==SCENE_SPECIAL_HI || p->state==SCENE_SPECIAL_FALL || p->state==SCENE_CLIFF || p->state==SCENE_CLIMB;}
static int mario(const CombatScene *s) {return s->fighter->kind==FIGHTER_MARIO;}
static int tornado(const CombatScene *s,MarioMove m) {return mario(s) && (m==MOVE_TORNADO || m==MOVE_TORNADO_AIR);}
static int up_special(MarioMove m) {return m==MOVE_UP || m==MOVE_UP_AIR;}
static const MoveFrame *at(const CombatScene *s,MarioMove m,unsigned t)
{const MoveDesc *d=&s->fighter->moves[m];if(t>=d->ticks)t=d->ticks-1;return &s->fighter->frames[d->first+t];}

/* ftParamGetCommonKnockback with the defender's FTAttributes weight. */
float combat_knockback_weight(unsigned p,const MoveHit *h,float weight)
{
    float n=h->weight?1+h->weight*.5f:(p+h->damage)*.1f+(p+h->damage)*h->damage*.05f;
    float kb=(n*weight*1.4f+18)*h->scale*.01f+h->base;return kb>2500?2500:kb;
}
float combat_knockback(unsigned p,const MoveHit *h) {return combat_knockback_weight(p,h,1);}
float combat_jab_knockback(unsigned p)
{MoveHit h={0};h.damage=2;h.scale=50;h.base=8;return combat_knockback(p,&h);}
void combat_init_stage(CombatScene *s,StageId stage)
{
    memset(s,0,sizeof *s);scene_init_stage(&s->player,stage);scene_init_stage(&s->target,stage);
    s->player.x=-200;s->target.x=180;s->facing=s->attack_facing=1;s->target.facing=-1;
    s->attack_frame=-1;s->tornado_charge=1;s->tap_age=100;
    s->fighter=&fighter_mario;
}
void combat_set_fighter(CombatScene *s,const Fighter *f)
{s->fighter=f;s->player.attr=f->attr;s->player.kind=(uint8_t)f->kind;}
void combat_init(CombatScene *s) {combat_init_stage(s,STAGE_GARDEN);}

int combat_start_move(CombatScene *s,MarioMove m)
{
    if((unsigned)m>=MOVE_COUNT || s->hitstop)return 0;
    if(up_special(m) && mario(s)) {
        if(!scene_special_hi(&s->player,s->player.facing*80))return 0;
    } else if(m==MOVE_UP_AIR) {
        /* Link SpecialAirHi: rise, reduced gravity until flag1, then freefall. */
        if(!scene_special_hi_rise(&s->player,s->fighter->moves[m].ticks))return 0;
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
    if(m==MOVE_DASH)s->player.vx=s->attack_facing*s->fighter->attr->run_speed;
    if(tornado(s,m)) {
        s->player.vx=motion_ground_clamp(s->player.vx,17);
        if(air(&s->player))s->player.vy=-7;
    }
    return 1;
}
unsigned combat_player_clip(const CombatScene *s)
{return s->attack_frame<0?0:s->fighter->moves[s->move].clip;}
int combat_attack_active(const CombatScene *s)
{return s->attack_frame>=0 && at(s,s->move,(unsigned)s->attack_frame)->count>0;}

static int overlap(const MotionScene *p,float x,float y,float radius)
{
    float yy=y;if(yy<p->y+80)yy=p->y+80;if(yy>p->y+330)yy=p->y+330;
    float dx=p->x-x,dy=yy-y,r=radius+70;return dx*dx+dy*dy<=r*r;
}
static void damage(CombatScene *s,const MoveHit *h,int dir,float impact_y)
{
    float kb=combat_knockback_weight(s->percent,h,s->target.attr->weight),cs=h->cos_angle/4096.0f,sn=h->sin_angle/4096.0f;
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
    /* FTThrowHitDesc of the fighter's ThrowF/ThrowB script. */
    MoveHit h=s->fighter->throws[s->move==MOVE_THROW_B];
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
/* ---- Link: boomerang and bomb (reduced from wplinkboomerang.c / itlinkbomb.c,
 * values in docs/link-prueba18.md). No map collision for the boomerang, no
 * pickup/reflection of either, and the explosion does not hurt Link. ---- */
#define BOOMERANG_X 150
#define BOOMERANG_Y 290
#define BOMB_LIFE 300
static void boomerang_spawn(CombatScene *s)
{
    Boomerang *b=&s->boomerang;memset(b,0,sizeof *b);
    b->active=1;b->x=s->player.x+BOOMERANG_X*s->attack_facing;b->y=s->player.y+BOOMERANG_Y;
    b->speed=s->boomerang_smash?114:85;b->life=s->boomerang_smash?190:160;
    /* Stick up/down at spawn: the +-30 degree clamp (digital pad). */
    b->dx=s->aim?.8660254f*s->attack_facing:(float)s->attack_facing;b->dy=s->aim?.5f*s->aim:0;
}
static void boomerang_return(Boomerang *b,float turn)
{b->returning=1;b->dx=-b->dx;b->dy=-b->dy;b->homing=140;b->turn=turn;}
static void boomerang_update(CombatScene *s)
{
    Boomerang *b=&s->boomerang;
    if(!b->active)return;
    ++b->age;
    if(!--b->life) {b->active=0;return;}
    if(!b->returning) {
        b->speed-=1.4f;
        if(b->speed<=10) {b->speed=10;boomerang_return(b,.75f);}
    } else {
        b->speed+=1;if(b->speed>90)b->speed=90;
        if(b->homing) {
            --b->homing;
            /* Turn toward Link's chest by at most `turn` degrees: rotation by a
             * fixed small angle (precomputed cos/sin, no per-tick trig). */
            float tx=s->player.x-b->x,ty=s->player.y+BOOMERANG_Y-b->y;
            float c=b->turn>1?.99965732f:.99991433f,sn=b->turn>1?.02617695f:.01308960f;
            if(b->dx*ty-b->dy*tx<0)sn=-sn;
            float dx=c*b->dx-sn*b->dy,dy=sn*b->dx+c*b->dy;b->dx=dx;b->dy=dy;
        }
    }
    b->x+=b->dx*b->speed;b->y+=b->dy*b->speed;
    float cx=s->player.x-b->x,cy=s->player.y+BOOMERANG_Y-b->y;
    if(b->returning && cx*cx+cy*cy<180*180) {b->active=0;b->caught=1;return;}
    if(!b->hit && !s->captured && !s->target_immune && overlap(&s->target,b->x,b->y,100)) {
        MoveHit h={0};h.damage=b->returning?8:9;h.angle=70;h.scale=30;h.base=55;h.sound=SOUND_KICK_L;
        h.cos_angle=1401;h.sin_angle=3849;
        damage(s,&h,b->dx>=0?1:-1,b->y);b->hit=1;
        if(!b->returning) {b->speed-=5;boomerang_return(b,1.5f);}
    }
}
static void bomb_explode(Bomb *m) {m->state=BOMB_EXPLODING;m->age=0;m->hit=0;m->vx=m->vy=0;}
static void bomb_update(CombatScene *s)
{
    Bomb *m=&s->bomb;
    if(m->state==BOMB_NONE)return;
    if(m->state==BOMB_EXPLODING) {
        /* Radius 300/230/150 over ticks 0-5, then gone. */
        float radius=m->age<2?300:m->age<4?230:150;
        if(!m->hit && !s->captured && !s->target_immune && overlap(&s->target,m->x,m->y,radius)) {
            MoveHit h={0};h.damage=5;h.angle=361;h.scale=20;h.base=60;h.sound=SOUND_EXPLODE_L;
            damage(s,&h,s->target.x>=m->x?1:-1,m->y);m->hit=1;
        }
        if(++m->age>=6)m->state=BOMB_NONE;
        return;
    }
    if(!--m->life) {bomb_explode(m);return;}
    if(m->state==BOMB_HELD) {
        const MoveFrame *f=s->attack_frame>=0?at(s,s->move,(unsigned)s->attack_frame):0;
        m->x=s->player.x+(f?f->spawn_x:70)*s->player.facing;m->y=s->player.y+(f?f->spawn_y:170);
        return;
    }
    if(m->state==BOMB_RESTING)return;
    float old_y=m->y;m->vy-=1.2f;
    float v2=m->vx*m->vx+m->vy*m->vy;
    if(v2>100*100) {float k=100/sqrtf(v2);m->vx*=k;m->vy*=k;}
    m->x+=m->vx;m->y+=m->vy;
    int fast=m->vx>36 || m->vx< -36 || m->vy>25 || m->vy< -25;
    unsigned n;const StagePlatform *p=stage_platforms(s->player.stage,&n);
    for(unsigned i=0;i<n;++i)if(m->vy<0 && m->x>=p[i].left && m->x<=p[i].right && old_y>=p[i].height && m->y<=p[i].height) {
        if(fast) {m->y=p[i].height;bomb_explode(m);return;}
        m->y=p[i].height;
        if(++m->landings>=2) {m->state=BOMB_RESTING;m->vx=m->vy=0;}
        else {float speed=sqrtf(m->vx*m->vx+m->vy*m->vy);m->vy=(-m->vy+speed)*.15f;m->vx*=.15f;}
        break;
    }
    if(m->y<stage_blast_bottom(s->player.stage)) {m->state=BOMB_NONE;return;}
    if(!s->captured && !s->target_immune && overlap(&s->target,m->x,m->y,110)) {
        if(fast) {bomb_explode(m);return;}
        float speed=sqrtf(m->vx*m->vx+m->vy*m->vy);
        MoveHit h={0};h.damage=(int16_t)(2+(unsigned)(speed*.1f+.999f));h.angle=80;h.scale=20;h.base=60;h.sound=SOUND_KICK_M;
        h.cos_angle=711;h.sin_angle=4034;
        int dir=s->target.x>=m->x?1:-1;damage(s,&h,dir,m->y);
        m->vx=-8.0f*dir;m->vy=20;
    }
}
/* LightThrow (ftcommondata.c item throw table x Link 100% x bomb .6):
 * ground F 36@15, Hi 39@90, Lw 39@-70, F4 66@8, Hi4/Lw4 66; air F 45@8,
 * Hi 48@90, Lw 45@-90, F4 72@7, Hi4 72@90, Lw4 84@-90. Released at tick 8. */
static void bomb_throw_start(CombatScene *s,CombatInput in,int dir,int smash)
{
    int a=air(&s->player);float vx,vy;
    if(dir)s->player.facing=dir;
    if(in.up) {vx=0;vy=a?(smash?72:48):(smash?66:39);}
    else if(in.down) {vx=a?0:(smash?22.6f:13.3f);vy=a?(smash?-84:-45):(smash?-62:-36.6f);}
    else if(a) {vx=smash?71.5f:44.6f;vy=smash?8.8f:6.3f;}
    else {vx=smash?65.4f:34.8f;vy=smash?9.2f:9.3f;}
    s->throw_vx=vx*s->player.facing;s->throw_vy=vy;s->item_throw=1;
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
    s->aim=in.up?1:in.down?-1:0;
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
        s->move=mario(s)?MOVE_UP:MOVE_UP_AIR;s->attack_frame=(int)s->player.state_ticks;
        s->attack_facing=s->player.facing;s->connected_strikes=0;s->spawned=0;
    }
    if(s->attack_frame>=0) {
        if(s->queued_combo && ((s->move==MOVE_JAB1 && s->attack_frame>=10) ||
            (s->move==MOVE_JAB2 && s->attack_frame>=8)))
            combat_start_move(s,s->move==MOVE_JAB1?MOVE_JAB2:MOVE_JAB3);
        else if(++s->attack_frame>=s->fighter->moves[s->move].ticks) {
            MarioMove old=s->move;s->attack_frame=-1;
            if(old==MOVE_LEDGE_START || old==MOVE_LEDGE_SLOW_START)
                combat_start_move(s,old==MOVE_LEDGE_START?MOVE_LEDGE:MOVE_LEDGE_SLOW);
            else if(old==MOVE_GRAB && s->captured)combat_start_move(s,MOVE_GRAB_HOLD);
            else if(old==MOVE_GRAB_HOLD && s->captured)s->attack_frame=s->fighter->moves[old].ticks-1;
            if(tornado(s,old) && air(&s->player))s->tornado_charge=0;
        }
    }
    if(s->captured && s->move==MOVE_GRAB_HOLD) {
        ++s->capture_age;
        if(pressed || (dir && s->capture_age>8))
            combat_start_move(s,dir*s->attack_facing<0?MOVE_THROW_B:MOVE_THROW_F);
        else if(s->capture_age>90) {s->captured=0;s->attack_frame=-1;s->target.state=SCENE_AIR;}
    }
    if(s->attack_frame<0) {
        if(special && !mario(s) && s->item_throw) {
        } else if(special && !mario(s)) {
            if(dir)s->player.facing=dir;
            if(in.up)combat_start_move(s,air(&s->player)?MOVE_UP_AIR:MOVE_UP);
            else if(in.down) {
                /* Down-B: pull a bomb, or throw the held one as a smash throw. */
                if(s->bomb.state==BOMB_HELD)bomb_throw_start(s,in,0,1);
                else if(s->bomb.state==BOMB_NONE)combat_start_move(s,air(&s->player)?MOVE_TORNADO_AIR:MOVE_TORNADO);
            } else {
                /* Neutral B: boomerang; SpecialNEmpty if one is already out. */
                s->boomerang_smash=in.smash;s->boomerang_empty=s->boomerang.active;
                combat_start_move(s,air(&s->player)?MOVE_FIRE_AIR:MOVE_FIRE);
            }
        } else if(pressed && !mario(s) && s->bomb.state==BOMB_HELD && !s->item_throw) {
            bomb_throw_start(s,in,dir,in.smash);
        } else if(special) {
            MarioMove m=in.up?MOVE_UP:in.down?(air(&s->player)?MOVE_TORNADO_AIR:MOVE_TORNADO):
                (air(&s->player)?MOVE_FIRE_AIR:MOVE_FIRE);
            if(dir && m!=MOVE_UP)s->player.facing=dir;
            if(m==MOVE_UP) {
                if(scene_special_hi(&s->player,in.motion.stick_x)) {
                    s->move=m;s->attack_frame=0;s->attack_facing=s->player.facing;s->connected_strikes=0;
                }
            } else combat_start_move(s,m);
        } else if(pressed && !s->item_throw) {
            MarioMove m=select_normal(s,in);
            if(!air(&s->player) && dir)s->player.facing=dir;
            combat_start_move(s,m);
        } else if(grab && !in.smash && !s->item_throw && s->bomb.state!=BOMB_HELD)combat_start_move(s,MOVE_GRAB);
    }
    if(s->item_throw) {
        /* Release on tick 8 (LightThrow scripts), short recovery afterwards. */
        if(s->item_throw==8 && s->bomb.state==BOMB_HELD) {
            s->bomb.state=BOMB_FLYING;s->bomb.vx=s->throw_vx;s->bomb.vy=s->throw_vy;s->bomb.landings=0;
        }
        if(++s->item_throw>20)s->item_throw=0;
        if(!air(&s->player))in.motion.stick_x=0;
        in.motion.jump_held=0;
    }
    const MoveFrame *f=s->attack_frame>=0?at(s,s->move,(unsigned)s->attack_frame):0;
    unsigned old_respawns=s->player.respawns,old_landings=s->player.landings,old_grabs=s->player.cliff_grabs;
    SceneInput motion=in.motion;
    if(f) {
        motion.jump_held=0;if(!air(&s->player))motion.stick_x=0;
        if(tornado(s,s->move)) {
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
        s->player.vx=dir*s->fighter->attr->dash_velocity;motion.stick_x=0;s->player.facing=dir;
    }
    /* Preserve aerial facing while steering; back-air remains reachable when
     * the direction is pressed before X, rather than only on the same tick. */
    int previous_facing=s->player.facing;
    if(in.drop && !f)scene_drop_through(&s->player);
    s->player.squat_held=in.down && !f;
    scene_step(&s->player,motion);
    if(air(&s->player))s->player.facing=previous_facing;
    if(f)s->player.facing=s->attack_facing;
    s->facing=s->player.facing;
    if(s->player.respawns!=old_respawns) {s->attack_frame=-1;s->captured=0;f=0;}
    if(s->player.landings!=old_landings || s->player.cliff_grabs!=old_grabs) {
        s->tornado_charge=1;
        if(s->attack_frame>=0 && (s->player.cliff_grabs!=old_grabs ||
           (s->move>=MOVE_NAIR && s->move<=MOVE_DAIR) || (up_special(s->move) && s->player.landings!=old_landings))) {
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
    const MoveFrame *f=s->attack_frame>=0?at(s,s->move,(unsigned)s->attack_frame):0;
    if(s->captured && f) {
        s->target.x=s->player.x+s->attack_facing*f->grab_x;
        s->target.y=s->player.y+f->grab_y-180;
        s->target.vx=s->target.vy=0;
    }
    if(f) {
        if(mario(s) && (s->move==MOVE_FIRE || s->move==MOVE_FIRE_AIR) && !s->spawned && (f->flags&1)) {spawn(s,f);s->spawned=1;}
        if(!mario(s) && (s->move==MOVE_FIRE || s->move==MOVE_FIRE_AIR) && !s->spawned && (f->flags&1)) {
            if(!s->boomerang_empty && !s->boomerang.active)boomerang_spawn(s);
            s->spawned=1;
        }
        if(!mario(s) && (s->move==MOVE_TORNADO || s->move==MOVE_TORNADO_AIR) && !s->spawned && (f->flags&1)) {
            if(s->bomb.state==BOMB_NONE) {
                Bomb *m=&s->bomb;memset(m,0,sizeof *m);m->state=BOMB_HELD;m->life=BOMB_LIFE;
            }
            s->spawned=1;
        }
        if((s->move==MOVE_THROW_F || s->move==MOVE_THROW_B) && s->captured && (f->flags&4))release_throw(s);
        for(unsigned i=0;i<f->count && !s->captured && !s->target_immune;++i) {
            const MoveHit *h=&s->fighter->hits[f->first+i];uint64_t bit=UINT64_C(1)<<h->strike;
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
    if(!mario(s)) {boomerang_update(s);bomb_update(s);}
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
    const MoveDesc *d=&s->fighter->moves[m];
    if(m==MOVE_FIRE)s->target.x=1400;
    for(unsigned t=0;t<d->ticks;++t) {
        const MoveFrame *f=at(s,m,t);
        if(f->count) {
            const MoveHit *h=&s->fighter->hits[f->first];s->target.x=s->player.x+h->x;
            s->target.y=s->player.y+h->y-180;if(s->target.y<0)s->target.y=0;break;
        }
    }
    combat_start_move(s,m);
}
