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
static int link(const CombatScene *s) {return s->fighter->kind==FIGHTER_LINK;}
static int kirby(const CombatScene *s) {return s->fighter->kind==FIGHTER_KIRBY;}
static int tornado(const CombatScene *s,MarioMove m) {return mario(s) && (m==MOVE_TORNADO || m==MOVE_TORNADO_AIR);}
static int up_special(MarioMove m) {return m==MOVE_UP || m==MOVE_UP_AIR;}
static const MoveFrame *at(const CombatScene *s,MarioMove m,unsigned t)
{const MoveDesc *d=&s->fighter->moves[m];if(t>=d->ticks)t=d->ticks-1;return &s->fighter->frames[d->first+t];}

/* ---- Kirby (ftkirby.h, US) ---- */
#define KIRBY_STONE_DURATION 160        /* FTKIRBY_STONE_DURATION_MAX */
#define KIRBY_STONE_MIN 18              /* FTKIRBY_STONE_DURATION_MIN (US) */
#define KIRBY_STONE_RESIST 38           /* FTKIRBY_STONE_HEALTH_MAX (US) */
#define KIRBY_STONE_FALL_VEL -140.0f
#define KIRBY_INHALE_RELEASE_LAG 40     /* FTKIRBY_VACUUM_RELEASE_LAG */
#define KIRBY_INHALE_BREAKOUT 500       /* ftCommonCaptureWaitKirbySetStatus */
#define KIRBY_SPIT_DAMAGE 10            /* FTKIRBY_VACUUM_THROW_DAMAGE */
#define KIRBY_SPIT_VEL 120.0f           /* FTKIRBY_VACUUM_THROW_VEL_BASE */
#define KIRBY_STAR_DECEL 4.0f           /* FTCOMMON_THROWNKIRBYSTAR_DECELERATE */
#define KIRBY_BEAM_LIFE 20              /* WPFINALCUTTER_LIFETIME */
#define KIRBY_BEAM_VEL 100.0f           /* WPFINALCUTTER_VEL */
#define KIRBY_BEAM_OFF_X 200.0f         /* FTKIRBY_FINALCUTTER_OFF_X */

/* ---- Shield, rolls, techs and knockdowns (ftcommon.h constants) ---- */
#define SHIELD_HEALTH_MAX 55      /* ftmanager.c spawn value */
#define SHIELD_HEALTH_BROKEN 30
#define SHIELD_HEAL_WAIT 10       /* ftMainProcParams: +1 every 10 ticks while down */
#define SHIELD_DECAY_WAIT 16      /* FTCOMMON_GUARD_DECAY_INT */
#define SHIELD_SETOFF_MUL 1.62f   /* FTCOMMON_GUARD_SETOFF_MUL (US) */
#define SHIELD_SETOFF_ADD 4
#define SHIELD_SETOFF_VEL 2       /* FTCOMMON_GUARD_VEL_MUL */
#define ESCAPE_STICK_MIN 56       /* FTCOMMON_ESCAPE_STICK_RANGE_MIN */
#define ESCAPE_BUFFER 4
#define PASSIVE_BUFFER 20         /* FTCOMMON_PASSIVE_BUFFER_TICS_MAX */
#define DOWN_STAND_WAIT 180
#define DOWN_ATTACK_BUFFER 60
#define DOWN_STICK_MIN 20
#define FURAFURA_WAIT 400
#define FURAFURA_MIN 90           /* US */
static int has_actions(const CombatScene *s) {return s->fighter->actions!=NULL;}
static const FighterAction *act(const CombatScene *s) {return &s->fighter->actions[s->action];}
static int shielding(const CombatScene *s)
{return (s->action==ACTION_GUARD_ON || s->action==ACTION_GUARD) && s->shield_health>0;}
static int down_face(unsigned a) /* 1 = the face-down (D) variant of a knockdown status */
{return a==ACTION_DOWN_BOUNCE_D || a==ACTION_DOWN_WAIT_D || a==ACTION_SHIELD_BREAK_DOWN_D;}
static void landed(CombatScene *s)
{s->player.state=SCENE_WAIT;s->player.state_ticks=0;s->player.vy=0;s->player.hit_vy=0;}
static void action_set(CombatScene *s,unsigned a)
{s->action=a;s->action_tick=0;++s->action_starts;s->action_facing=s->player.facing;}
static void shield_break(CombatScene *s)
{
    /* ftCommonShieldBreakFlySetStatus: straight up at attr shield_break_vel_y. */
    s->shield_health=SHIELD_HEALTH_BROKEN;action_set(s,ACTION_SHIELD_BREAK_FLY);
    s->player.state=SCENE_AIR;s->player.platform=-1;s->player.vx=0;
    s->player.vy=s->fighter->attr->shield_break_vel_y;s->player.hit_vx=s->player.hit_vy=0;
    s->player.jumps_used=1;s->player.air_jump=0;
}
void combat_action_start(CombatScene *s,unsigned a)
{
    if(!has_actions(s) || a>=ACTION_COUNT)return;
    action_set(s,a);s->attack_frame=-1;s->dash_ticks=0;s->run_age=0;s->brake_ticks=0;s->item_throw=0;
    switch(a) {
    case ACTION_GUARD_ON:s->shield_decay_wait=SHIELD_DECAY_WAIT;s->setoff_ticks=0;s->shield_release=0;break;
    case ACTION_DOWN_WAIT_D:case ACTION_DOWN_WAIT_U:s->down_wait=DOWN_STAND_WAIT;landed(s);break;
    case ACTION_DOWN_BOUNCE_D:case ACTION_DOWN_BOUNCE_U:s->down_attack_buffer=0;landed(s);break;
    case ACTION_SHIELD_BREAK_FLY:shield_break(s);break;
    case ACTION_FURAFURA:
        /* ftCommonFuraFuraSetStatus: 400 - percent (floor 0) + 90, shield 30. */
        s->breakout_wait=(s->own_percent<FURAFURA_WAIT?FURAFURA_WAIT-s->own_percent:0)+FURAFURA_MIN;
        s->shield_health=SHIELD_HEALTH_BROKEN;break;
    case ACTION_PASSIVE:case ACTION_PASSIVE_F:case ACTION_PASSIVE_B:
    case ACTION_SHIELD_BREAK_DOWN_D:case ACTION_SHIELD_BREAK_DOWN_U:landed(s);break;
    default:break;
    }
}
void combat_note_shield(CombatScene *s,int held)
{
    int pressed=held && !s->shield_was_held;s->shield_was_held=held;
    if(pressed)s->shield_age=0;else if(s->shield_age<100)++s->shield_age;
}
unsigned combat_action_clip(const CombatScene *s,unsigned *age)
{
    if(!s->action || !has_actions(s)) {*age=0;return 0;}
    *age=s->action_tick;return act(s)->clip; /* the renderer clamps or loops */
}
float combat_shield_radius(const CombatScene *s)
{
    /* ftCommonGuardUpdateShieldCollision: 30 x ((.65 health/55 + .35) size/30). */
    if(!has_actions(s) || !shielding(s))return 0;
    return (.65f*s->shield_health/SHIELD_HEALTH_MAX+.35f)*s->fighter->attr->shield_size;
}
int combat_intangible(const CombatScene *s)
{
    if(!s->action || !has_actions(s))return 0;
    const FighterAction *a=act(s);
    return s->action_tick>=a->intangible_from && s->action_tick<a->intangible_to;
}
void combat_shield_hit(CombatScene *s,unsigned damage,int dir)
{
    if(!has_actions(s))return;
    s->shield_health-=(int)damage;
    if(s->shield_health<=0) {shield_break(s);return;}
    /* ftCommonGuardSetOffSetStatus: stun ticks and push-back away from the attacker. */
    s->setoff_ticks=(unsigned)(damage*SHIELD_SETOFF_MUL+SHIELD_SETOFF_ADD);
    s->player.vx=dir*(float)s->setoff_ticks*SHIELD_SETOFF_VEL;
    if(s->action==ACTION_GUARD_ON) {s->action=ACTION_GUARD;s->action_tick=act(s)->ticks;}
}
static int getup_attack(CombatScene *s)
{
    unsigned a=s->action;s->action=ACTION_NONE;
    return combat_start_move(s,down_face(a)?MOVE_GETUP_D:MOVE_GETUP_U);
}
/* One tick of the current action: transitions, timers and TransN movement.
 * Inputs: this tick's edges. Returns after the action's own tick advance. */
static void action_update(CombatScene *s,CombatInput in,int pressed,int special,int grab,int shield_pressed)
{
    const FighterAction *a=act(s);unsigned t=s->action_tick,starts=s->action_starts;
    int x=in.motion.stick_x,fwd=x*s->player.facing>=0?1:-1;
    int roll=(x>=ESCAPE_STICK_MIN || x<=-ESCAPE_STICK_MIN) && s->tap_age<ESCAPE_BUFFER?fwd:0;
    int down_roll=(x>=DOWN_STICK_MIN || x<=-DOWN_STICK_MIN)?fwd:0;
    int jump=in.motion.jump_held && !s->player.jump_was_held;
    int d=down_face(s->action),ended=t+1>=a->ticks;
    if(a->dx && t<a->ticks) {
        /* ftPhysicsApplyGroundVelTransN: the animation's TransN delta along the
         * facing the status began with (after flag1 flips lr mid-roll the
         * source negates the velocity again, so the direction never changes);
         * rolls stay on their platform. */
        s->player.x+=a->dx[t]*s->action_facing;s->player.vx=0;
        unsigned n;const StagePlatform *p=stage_platforms(s->player.stage,&n);
        if(s->player.platform>=0 && (unsigned)s->player.platform<n) {
            const StagePlatform *q=&p[s->player.platform];
            if(s->player.x<q->left)s->player.x=q->left;
            if(s->player.x>q->right)s->player.x=q->right;
        }
    }
    switch(s->action) {
    case ACTION_GUARD_ON:case ACTION_GUARD:
        if(!in.shield)s->shield_release=1;
        if(!s->setoff_ticks) {
            /* GuardKneeBend / Escape / Catch / GuardPass interrupts. */
            if(jump) {s->action=ACTION_NONE;break;}
            if(roll) {action_set(s,roll>0?ACTION_ESCAPE_F:ACTION_ESCAPE_B);break;}
            if(grab) {s->action=ACTION_NONE;combat_start_move(s,MOVE_GRAB);break;}
            if(in.drop && s->player.platform>0) {s->action=ACTION_NONE;scene_drop_through(&s->player);break;}
        }
        /* ftCommonGuardUpdateShieldVars: one point every 16 ticks; empty breaks. */
        if(s->shield_health>0 && --s->shield_decay_wait==0) {
            s->shield_decay_wait=SHIELD_DECAY_WAIT;
            if(--s->shield_health==0) {shield_break(s);break;}
        }
        if(s->setoff_ticks) {
            if(--s->setoff_ticks==0 && s->shield_release)action_set(s,ACTION_GUARD_OFF);
        } else if(s->action==ACTION_GUARD_ON) {
            if(ended)action_set(s,s->shield_release?ACTION_GUARD_OFF:ACTION_GUARD);
        } else if(s->shield_release)action_set(s,ACTION_GUARD_OFF);
        break;
    case ACTION_GUARD_OFF:
        if(ended)s->action=ACTION_NONE;
        break;
    case ACTION_DOWN_BOUNCE_D:case ACTION_DOWN_BOUNCE_U:
        if(pressed || special)s->down_attack_buffer=DOWN_ATTACK_BUFFER;
        else if(s->down_attack_buffer)--s->down_attack_buffer;
        if(ended) {
            if(s->down_attack_buffer)getup_attack(s);
            else if(down_roll)action_set(s,down_roll>0?(d?ACTION_DOWN_FORWARD_D:ACTION_DOWN_FORWARD_U):(d?ACTION_DOWN_BACK_D:ACTION_DOWN_BACK_U));
            else combat_action_start(s,d?ACTION_DOWN_WAIT_D:ACTION_DOWN_WAIT_U);
        }
        break;
    case ACTION_DOWN_WAIT_D:case ACTION_DOWN_WAIT_U:
        if(pressed || special)getup_attack(s);
        else if(down_roll)action_set(s,down_roll>0?(d?ACTION_DOWN_FORWARD_D:ACTION_DOWN_FORWARD_U):(d?ACTION_DOWN_BACK_D:ACTION_DOWN_BACK_U));
        else if(in.up || shield_pressed || --s->down_wait==0)action_set(s,d?ACTION_DOWN_STAND_D:ACTION_DOWN_STAND_U);
        break;
    case ACTION_SHIELD_BREAK_FLY:
        if(ended)action_set(s,ACTION_SHIELD_BREAK_FALL);
        break;
    case ACTION_SHIELD_BREAK_FALL:
        break; /* lands into ShieldBreakDown (combat_update) */
    case ACTION_SHIELD_BREAK_DOWN_D:case ACTION_SHIELD_BREAK_DOWN_U:
        if(ended)action_set(s,d?ACTION_SHIELD_BREAK_STAND_D:ACTION_SHIELD_BREAK_STAND_U);
        break;
    case ACTION_SHIELD_BREAK_STAND_D:case ACTION_SHIELD_BREAK_STAND_U:
        if(ended)combat_action_start(s,ACTION_FURAFURA);
        break;
    case ACTION_FURAFURA: {
        /* ftCommonCaptureTrappedUpdateBreakoutVars x4: button taps and stick
         * flips shorten the dizzy time; the shield stays at 30. */
        int stick=x>DOWN_STICK_MIN?1:x< -DOWN_STICK_MIN?-1:0;
        int mash=pressed || special || shield_pressed || jump || (stick && stick!=s->mash_stick);
        s->mash_stick=stick;s->shield_health=SHIELD_HEALTH_BROKEN;
        if(s->breakout_wait)--s->breakout_wait;
        if(mash)s->breakout_wait=s->breakout_wait>4?s->breakout_wait-4:0;
        if(!s->breakout_wait)s->action=ACTION_NONE;
        break;
    }
    case ACTION_DOWN_STAND_D:case ACTION_DOWN_STAND_U:
        if(ended || (jump && t>=a->turn_tick))s->action=ACTION_NONE;
        break;
    case ACTION_ESCAPE_F:case ACTION_ESCAPE_B:
        /* flag1 (RollF tick 20) flips lr; the model's yaw follows at the next
         * status, when the animation has turned the body, so the drawn facing
         * flips here at the end: a forward roll ends facing the other way. */
        if(ended) {
            if(a->turn_tick!=255)s->player.facing=-s->player.facing;
            s->action=ACTION_NONE;
        }
        break;
    default: /* techs and down rolls end into Wait */
        if(ended)s->action=ACTION_NONE;
        break;
    }
    if(s->action && s->action_starts==starts)++s->action_tick;
}

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
    s->shield_health=SHIELD_HEALTH_MAX;s->shield_heal_wait=SHIELD_HEAL_WAIT;s->shield_age=100;
}
void combat_set_fighter(CombatScene *s,const Fighter *f)
{s->fighter=f;s->player.attr=f->attr;s->player.kind=(uint8_t)f->kind;}
void combat_init(CombatScene *s) {combat_init_stage(s,STAGE_GARDEN);}

int combat_start_move(CombatScene *s,MarioMove m)
{
    if((unsigned)m>=MOVE_COUNT || s->hitstop)return 0;
    if(m>=MOVE_UP_FALL) {
        /* Kirby statuses entered by the combat logic itself, never by input. */
        if(!kirby(s))return 0;
    } else if(up_special(m) && kirby(s)) {
        if(!scene_special_hi_cutter(&s->player))return 0;
    } else if(up_special(m) && mario(s)) {
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
    /* ftKirbySpecialLwStartSetStatus / SpecialAirLwStart: velocities zeroed. */
    if(kirby(s) && (m==MOVE_TORNADO || m==MOVE_TORNADO_AIR)) {s->player.vx=0;s->player.vy=0;}
    if(kirby(s) && m==MOVE_INHALE)s->inhale_lag=KIRBY_INHALE_RELEASE_LAG;
    return 1;
}
int combat_stone(const CombatScene *s)
{return kirby(s) && s->attack_frame>=0 && (s->move==MOVE_STONE_HOLD || s->move==MOVE_STONE_FALL);}
unsigned combat_player_clip(const CombatScene *s)
{return s->attack_frame<0?0:s->fighter->moves[s->move].clip;}
int combat_attack_active(const CombatScene *s)
{return s->attack_frame>=0 && at(s,s->move,(unsigned)s->attack_frame)->count>0;}

static int overlap(const MotionScene *p,float x,float y,float radius)
{
    float yy=y;if(yy<p->y+80)yy=p->y+80;if(yy>p->y+330)yy=p->y+330;
    float dx=p->x-x,dy=yy-y,r=radius+70;return dx*dx+dy*dy<=r*r;
}
/* gmCollisionCheckFighterAttackShieldCollide: hit sphere against the shield
 * sphere at the target's YRotN. Checked before the hurt capsule. */
static int shield_blocks(const CombatScene *s,float x,float y,float radius)
{
    if(s->target_shield<=0)return 0;
    float dx=s->target.x+s->target_shield_x-x,dy=s->target.y+s->target_shield_y-y,r=radius+s->target_shield;
    return dx*dx+dy*dy<=r*r;
}
static void shield_hit(CombatScene *s,unsigned damage,int dir,unsigned sound)
{
    ++s->shield_hits;s->shield_hit_damage=damage;s->shield_hit_dir=dir;s->shield_hit_sound=sound;
    s->attack_connected=1;s->hitstop=damage/3+5;
}
static void damage(CombatScene *s,const MoveHit *hit,int dir,float impact_y)
{
    MoveHit leftover=*hit;const MoveHit *h=hit;
    if(s->target_resist>0) {
        /* ftMainCheckGetUpdateDamage: Kirby's stone absorbs the damage; only
         * what exceeds its HP lands, and then the stone breaks. */
        int left=s->target_resist-hit->damage;
        if(left>0) {
            s->target_resist=left;++s->resisted;s->resisted_damage=(unsigned)hit->damage;
            s->damage_sound=hit->sound;s->hitstop=hit->damage/3+5;return;
        }
        s->target_resist=0;leftover.damage=(int16_t)-left;h=&leftover;
    }
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
        if(b->life && !s->captured && !s->target_immune && shield_blocks(s,b->x,b->y,100)) {
            shield_hit(s,7,b->facing,SOUND_BURN_S);b->life=0;
        } else if(b->life && !s->captured && !s->target_immune && overlap(&s->target,b->x,b->y,100)) {
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
    if(!b->hit && !s->captured && !s->target_immune && shield_blocks(s,b->x,b->y,100)) {
        shield_hit(s,b->returning?8:9,b->dx>=0?1:-1,SOUND_KICK_L);b->hit=1;
        if(!b->returning) {b->speed-=5;boomerang_return(b,1.5f);}
    } else if(!b->hit && !s->captured && !s->target_immune && overlap(&s->target,b->x,b->y,100)) {
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
        if(!m->hit && !s->captured && !s->target_immune && shield_blocks(s,m->x,m->y,radius)) {
            shield_hit(s,5,s->target.x>=m->x?1:-1,SOUND_EXPLODE_L);m->hit=1;
        } else if(!m->hit && !s->captured && !s->target_immune && overlap(&s->target,m->x,m->y,radius)) {
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
    if(!s->captured && !s->target_immune && (shield_blocks(s,m->x,m->y,110) || overlap(&s->target,m->x,m->y,110))) {
        if(fast) {bomb_explode(m);return;}
        float speed=sqrtf(m->vx*m->vx+m->vy*m->vy);
        MoveHit h={0};h.damage=(int16_t)(2+(unsigned)(speed*.1f+.999f));h.angle=80;h.scale=20;h.base=60;h.sound=SOUND_KICK_M;
        h.cos_angle=711;h.sin_angle=4034;
        int dir=s->target.x>=m->x?1:-1;
        if(shield_blocks(s,m->x,m->y,110))shield_hit(s,(unsigned)h.damage,dir,SOUND_KICK_M);
        else damage(s,&h,dir,m->y);
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
/* ---- Kirby: Final Cutter beam, inhale spit/escape (reduced, see docs/kirby-prueba20.md) ---- */
static void beam_update(CombatScene *s)
{
    CutterBeam *b=&s->beam;
    if(!b->active)return;
    /* wpKirbyCutter: 100 per tick along the floor for 20 ticks, one hit. */
    b->x+=b->facing*KIRBY_BEAM_VEL;
    if(!--b->life || b->x>2318+200 || b->x< -2318-200) {b->active=0;return;}
    if(b->hit || s->captured || s->target_immune)return;
    if(shield_blocks(s,b->x,b->y+110,125)) {shield_hit(s,6,b->facing,SOUND_SLASH_M);b->hit=1;return;}
    if(overlap(&s->target,b->x,b->y+110,125)) {
        /* dKirbyMain_CutterWeaponAttributes: 6%, 361, scale 50, base 70, SlashM. */
        MoveHit h={0};h.damage=6;h.angle=361;h.scale=50;h.base=70;h.sound=SOUND_SLASH_M;
        damage(s,&h,b->facing,b->y+110);b->hit=1;
    }
}
static void kirby_spit(CombatScene *s)
{
    /* ftKirbySpecialNThrow: 10% then ThrownKirbyStar at 120, slowing by 4 per
     * tick. Reduced to a fixed launch (versus decays it by 1.7*v/kb = 4). */
    int dir=s->attack_facing;
    s->captured=0;s->inhaled=0;
    s->target.x=s->player.x+dir*150;s->target.y=s->player.y+40;
    s->target.state=SCENE_AIR;s->target.platform=-1;s->target.vx=s->target.vy=0;s->target.facing=-dir;
    s->target.jumps_used=1;
    s->damage_kb=KIRBY_SPIT_VEL*1.7f/KIRBY_STAR_DECEL;s->damage_vx=dir*KIRBY_SPIT_VEL;s->damage_vy=0;
    s->hitstun=(unsigned)(KIRBY_SPIT_VEL/KIRBY_STAR_DECEL);s->damage_pose=13;
    s->damage_amount=KIRBY_SPIT_DAMAGE;s->damage_sound=SOUND_NONE;
    s->percent+=KIRBY_SPIT_DAMAGE;if(s->percent>999)s->percent=999;
    ++s->hits;s->hitstop=0;
}
static void kirby_escape(CombatScene *s)
{
    /* ftCommonCaptureWaitKirby breakout: dFTCommonCaptureKirbyKnockbackCapture
     * (angle 80, scale 100, fixed weight 60) on the escapee. */
    MoveHit h={0};h.angle=80;h.scale=100;h.weight=60;h.cos_angle=711;h.sin_angle=4034;h.sound=SOUND_NONE;
    s->captured=0;s->inhaled=0;s->target.x=s->player.x+s->attack_facing*150;s->target.y=s->player.y;
    damage(s,&h,s->attack_facing,s->target.y+180);s->attack_frame=-1;
}
static void kirby_stone_end(CombatScene *s)
{s->stone=0;s->stone_resist=0;combat_start_move(s,MOVE_STONE_END);}

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
    if(!s->duel_mode)combat_note_shield(s,in.shield);
    int shield_pressed=s->shield_age==0,actions=has_actions(s);
    if(pressed && s->attack_frame>=0 && (s->move==MOVE_JAB1 || s->move==MOVE_JAB2))s->queued_combo=1;
    if(s->hitstop) {--s->hitstop;return;}
    if(s->hitstun)--s->hitstun;
    if(s->brake_ticks)--s->brake_ticks;
    if(s->tap_age<100)++s->tap_age;
    /* ftMainProcParams: the shield heals one point every 10 ticks while down. */
    if(actions && !shielding(s) && s->action!=ACTION_FURAFURA && s->shield_health<SHIELD_HEALTH_MAX &&
       --s->shield_heal_wait==0) {++s->shield_health;s->shield_heal_wait=SHIELD_HEAL_WAIT;}
    int dir=in.motion.stick_x>0?1:in.motion.stick_x<0?-1:0;
    if(s->dash_ticks && (!dir || dir!=s->last_stick || !ground(&s->player) || s->attack_frame>=0 || s->action)) {
        if(!dir && ground(&s->player) && s->attack_frame<0 && !s->action)s->brake_ticks=24;
        s->dash_ticks=0;s->run_age=0;
    }
    if(dir && dir!=s->last_stick && ground(&s->player)) {
        if(dir==s->last_tap && s->tap_age<=10 && s->attack_frame<0 && !s->action) {s->dash_ticks=1;s->run_age=0;}
        s->last_tap=dir;s->tap_age=0;
    }
    s->last_stick=dir;
    if(dir)s->brake_ticks=0;
    if(s->dash_ticks)++s->run_age;
    if(s->action && actions)action_update(s,in,pressed,special,grab,shield_pressed);
    if(s->player.state==SCENE_SPECIAL_HI && s->attack_frame<0 && !kirby(s)) {
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
            /* Kirby's ThrowF releases on landing (ThrowFLanding, not converted):
             * a throw that ends still holding lets go here. */
            else if((old==MOVE_THROW_F || old==MOVE_THROW_B) && s->captured)release_throw(s);
            else if(kirby(s))switch(old) {
            /* Kirby: SpecialHi -> SpecialAirHiFall while airborne; held poses;
             * inhale start -> loop; eat -> wait (looped while full). */
            case MOVE_UP:case MOVE_UP_AIR:
                if(s->player.state==SCENE_SPECIAL_HI)combat_start_move(s,MOVE_UP_FALL);
                break;
            case MOVE_UP_FALL:case MOVE_STONE_HOLD:case MOVE_STONE_FALL:
                if(old!=MOVE_UP_FALL || s->player.state==SCENE_SPECIAL_HI)s->attack_frame=s->fighter->moves[old].ticks-1;
                break;
            case MOVE_FIRE:case MOVE_FIRE_AIR:combat_start_move(s,MOVE_INHALE);s->inhale_lag=KIRBY_INHALE_RELEASE_LAG;break;
            case MOVE_INHALE:s->attack_frame=0;s->connected_strikes=0;break;
            case MOVE_INHALE_EAT:if(s->captured)combat_start_move(s,MOVE_INHALE_WAIT);break;
            case MOVE_INHALE_WAIT:if(s->captured)s->attack_frame=0;break;
            default:break;
            }
            if(tornado(s,old) && air(&s->player))s->tornado_charge=0;
        }
    }
    if(s->captured && s->move==MOVE_GRAB_HOLD) {
        ++s->capture_age;
        if(pressed || (dir && s->capture_age>8))
            combat_start_move(s,dir*s->attack_facing<0?MOVE_THROW_B:MOVE_THROW_F);
        else if(s->capture_age>90) {s->captured=0;s->attack_frame=-1;s->target.state=SCENE_AIR;}
    }
    if(kirby(s) && s->attack_frame>=0) {
        if(s->move==MOVE_INHALE) {
            /* ftKirbySpecialNLoopCheckContinueLoop: 40 ticks minimum, then while B held. */
            if(s->inhale_lag)--s->inhale_lag;
            else if(!in.special)combat_start_move(s,MOVE_INHALE_END);
        } else if(s->captured && s->inhaled) {
            if(--s->inhale_breakout<=0)kirby_escape(s);
            else if(s->move==MOVE_INHALE_WAIT) {
                if(dir)s->player.facing=s->attack_facing=dir; /* SpecialNTurn, reduced */
                if(pressed || special)combat_start_move(s,MOVE_INHALE_THROW); /* copy (B/down) spits too */
            }
        } else if(s->move==MOVE_TORNADO || s->move==MOVE_TORNADO_AIR) {
            /* flag1 of StoneStartGround/Air: the stone (resist 38 HP, 160 ticks). */
            if(at(s,s->move,(unsigned)s->attack_frame)->flags&2) {
                s->stone=KIRBY_STONE_DURATION;s->stone_age=0;s->stone_resist=KIRBY_STONE_RESIST;
                combat_start_move(s,air(&s->player)?MOVE_STONE_FALL:MOVE_STONE_HOLD);
            }
        } else if(s->move==MOVE_STONE_HOLD || s->move==MOVE_STONE_FALL) {
            /* ftKirbySpecialLwCheckRelease: B after the minimum, or time out. */
            ++s->stone_age;
            if(!s->stone_resist || !--s->stone || (special && s->stone_age>=KIRBY_STONE_MIN))kirby_stone_end(s);
        }
    }
    if(s->attack_frame<0 && !s->action) {
        int jump_pressed=in.motion.jump_held && !s->player.jump_was_held;
        if(actions && in.shield && s->shield_health>0 && ground(&s->player) && !s->item_throw && !s->captured && !jump_pressed) {
            /* Z held: GuardOn; tapped while dashing: forward roll (ftCommonEscapeCheckInterruptDash). */
            if(s->dash_ticks && shield_pressed)combat_action_start(s,ACTION_ESCAPE_F);
            else combat_action_start(s,ACTION_GUARD_ON);
        } else if(special && kirby(s)) {
            /* Up: Final Cutter; down: Stone; neutral: Inhale. */
            if(dir)s->player.facing=dir;
            if(in.up)combat_start_move(s,air(&s->player)?MOVE_UP_AIR:MOVE_UP);
            else if(in.down)combat_start_move(s,air(&s->player)?MOVE_TORNADO_AIR:MOVE_TORNADO);
            else combat_start_move(s,air(&s->player)?MOVE_FIRE_AIR:MOVE_FIRE);
        } else if(special && link(s) && s->item_throw) {
        } else if(special && link(s)) {
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
        } else if(pressed && link(s) && s->bomb.state==BOMB_HELD && !s->item_throw) {
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
    if(s->action) {motion.stick_x=0;motion.jump_held=0;in.drop=0;in.down=0;}
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
        if(kirby(s)) {
            /* Final Cutter: vertical speed = the clip's TransN delta (ApplyAirVelTransNYZ);
             * SpecialAirHiFall keeps the last one. Landing: ground TransN. */
            if(s->move==MOVE_UP || s->move==MOVE_UP_AIR)s->player.vy=f->dy;
            if(s->move==MOVE_UP_LAND) {s->player.x+=f->dx*s->attack_facing;s->player.vx=0;}
            if(s->move==MOVE_STONE_FALL) {s->player.vx=0;s->player.force_vy=1;s->player.forced_vy=KIRBY_STONE_FALL_VEL;}
            if(s->move==MOVE_STONE_HOLD || s->move==MOVE_STONE_END || s->move==MOVE_INHALE || s->move==MOVE_INHALE_EAT ||
               s->move==MOVE_INHALE_WAIT || s->move==MOVE_INHALE_THROW || s->move==MOVE_INHALE_END)s->player.vx=ground(&s->player)?0:s->player.vx;
        }
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
    s->player.force_vy=0;
    if(air(&s->player))s->player.facing=previous_facing;
    if(f)s->player.facing=s->attack_facing;
    s->facing=s->player.facing;
    if(s->player.respawns!=old_respawns) {
        s->attack_frame=-1;s->captured=0;f=0;s->action=ACTION_NONE;s->shield_health=SHIELD_HEALTH_MAX;
    }
    if(s->player.landings!=old_landings && (s->action==ACTION_SHIELD_BREAK_FLY || s->action==ACTION_SHIELD_BREAK_FALL))
        combat_action_start(s,ACTION_SHIELD_BREAK_DOWN_U);
    if(kirby(s) && s->player.landings!=old_landings && s->attack_frame>=0) {
        /* Final Cutter lands into SpecialHiLanding (shockwave); a falling stone keeps going on the ground. */
        if(s->move==MOVE_UP || s->move==MOVE_UP_AIR || s->move==MOVE_UP_FALL)combat_start_move(s,MOVE_UP_LAND);
        else if(s->move==MOVE_STONE_FALL)combat_start_move(s,MOVE_STONE_HOLD);
        f=at(s,s->move,(unsigned)s->attack_frame);
    }
    if(s->player.landings!=old_landings || s->player.cliff_grabs!=old_grabs) {
        s->tornado_charge=1;
        if(s->attack_frame>=0 && (s->player.cliff_grabs!=old_grabs ||
           (s->move>=MOVE_NAIR && s->move<=MOVE_DAIR) || (up_special(s->move) && s->player.landings!=old_landings) ||
           (kirby(s) && s->move==MOVE_UP_FALL))) {
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
    } else if(s->captured && s->inhaled) {
        s->target.x=s->player.x;s->target.y=s->player.y;s->target.vx=s->target.vy=0; /* inside Kirby */
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
    if(s->captured && s->inhaled) {
        s->target.x=s->player.x;s->target.y=s->player.y;s->target.vx=s->target.vy=0;
    } else if(s->captured && f) {
        s->target.x=s->player.x+s->attack_facing*f->grab_x;
        s->target.y=s->player.y+f->grab_y-180;
        s->target.vx=s->target.vy=0;
    }
    if(f) {
        if(mario(s) && (s->move==MOVE_FIRE || s->move==MOVE_FIRE_AIR) && !s->spawned && (f->flags&1)) {spawn(s,f);s->spawned=1;}
        if(link(s) && (s->move==MOVE_FIRE || s->move==MOVE_FIRE_AIR) && !s->spawned && (f->flags&1)) {
            if(!s->boomerang_empty && !s->boomerang.active)boomerang_spawn(s);
            s->spawned=1;
        }
        if(kirby(s) && s->move==MOVE_UP_LAND && !s->spawned && (f->flags&1)) {
            /* SpecialHiLanding flag0: beam at TopN + 200 forward. */
            CutterBeam *b=&s->beam;b->active=1;b->hit=0;b->life=KIRBY_BEAM_LIFE;b->facing=s->attack_facing;
            b->x=s->player.x+b->facing*KIRBY_BEAM_OFF_X;b->y=s->player.y;s->spawned=1;
        }
        if(kirby(s) && s->move==MOVE_INHALE_THROW && s->captured && s->inhaled && (f->flags&1 || s->attack_frame>=7))kirby_spit(s);
        if(link(s) && (s->move==MOVE_TORNADO || s->move==MOVE_TORNADO_AIR) && !s->spawned && (f->flags&1)) {
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
            /* Shields stop everything but grabs and inhale (ftMainSearchHitFighter order). */
            int catches=s->move==MOVE_GRAB || (kirby(s) && s->move==MOVE_INHALE);
            if(!catches && shield_blocks(s,s->player.x+h->x*s->attack_facing,s->player.y+h->y,h->radius)) {
                s->connected_strikes|=bit;
                shield_hit(s,(unsigned)h->damage,(s->target.x-s->player.x)>=0?1:-1,h->sound);
                continue;
            }
            if(overlap(&s->target,s->player.x+h->x*s->attack_facing,s->player.y+h->y,h->radius)) {
                s->connected_strikes|=bit;s->attack_connected=1;
                if(catches) {
                    s->captured=1;s->capture_age=0;s->damage_vx=0;
                    s->target.facing=-s->attack_facing;
                    if(s->move==MOVE_INHALE) {
                        /* SpecialNCatch -> Eat: the victim is held inside (CaptureWaitKirby). */
                        s->inhaled=1;s->inhale_breakout=KIRBY_INHALE_BREAKOUT;combat_start_move(s,MOVE_INHALE_EAT);
                    } else combat_start_move(s,MOVE_GRAB_HOLD);
                    break;
                }
                else damage(s,h,(s->target.x-s->player.x)>=0?1:-1,s->player.y+h->y);
            }
        }
    }
    projectiles(s);
    if(link(s)) {boomerang_update(s);bomb_update(s);}
    if(kirby(s))beam_update(s);
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
