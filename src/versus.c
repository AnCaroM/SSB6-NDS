/* Local two-player scheduler and deliberately simple CPU, not the N64 AI. */
#include "versus.h"
#include "motion.h"
#include <string.h>

static int grounded(const MotionScene *p)
{return p->state==SCENE_WAIT || p->state==SCENE_WALK || p->state==SCENE_SQUAT;}
/* Rolls and tech rolls ignore the body push (is_jostle_ignore). */
static int rolling(const CombatScene *s)
{return s->action && s->fighter->actions && s->fighter->actions[s->action].dx;}
static void sound_event(Versus *v,FightSound sound,unsigned fighter)
{if(v->sound_count<8)v->sounds[v->sound_count++]=(FightSoundEvent){sound,fighter};}
/* Sounds authored on the current tick of the move's original motion script. */
static void move_sounds(Versus *v,unsigned i)
{
    const CombatScene *s=&v->fighter[i];const MoveDesc *d=&s->fighter->moves[s->move];
    unsigned t=(unsigned)s->attack_frame;if(t>=d->ticks)return;
    for(unsigned k=0;k<2;++k) {
        unsigned sound=s->fighter->frames[d->first+t].sound[k];
        if(sound==SOUND_SMASH_RANDOM)sound=s->fighter->sounds.smash[(v->smash_voice++)%3];
        if(sound<SOUND_COUNT)sound_event(v,(FightSound)sound,i);
    }
}
static void fighter_reset(Versus *v,unsigned i,int respawn)
{
    CombatScene *s=&v->fighter[i];combat_init_stage(s,STAGE_DREAMLAND);
    combat_set_fighter(s,v->kinds[i]?v->kinds[i]:&fighter_mario);
    s->duel_mode=1;s->player.x=i?650:-650;s->player.facing=i?-1:1;
    s->facing=s->attack_facing=s->player.facing;
    if(respawn) {
        s->player.y=1800;s->player.state=SCENE_AIR;s->player.platform=-1;
        s->player.jumps_used=1;v->invulnerable[i]=90;
    }
    v->percent[i]=v->stun[i]=0;v->launch[i]=v->launch_y[i]=0;
    v->reaction[i]=(DamageReaction){0};
}
void versus_init(Versus *v) {versus_init_fighters(v,&fighter_mario,&fighter_mario);}
void versus_init_solo(Versus *v,const Fighter *a)
{
    versus_init_fighters(v,a,a);v->solo=1;v->fighter[0].player.x=0;
    /* The absent side is parked out of reach and never stepped. */
    v->fighter[1].player.x=1e6f;v->fighter[1].player.y=1e6f;v->fighter[1].player.state=SCENE_AIR;
}
void versus_init_fighters(Versus *v,const Fighter *a,const Fighter *b)
{
    memset(v,0,sizeof *v);v->winner=-1;v->stocks[0]=v->stocks[1]=3;
    v->kinds[0]=a;v->kinds[1]=b;
    fighter_reset(v,0,0);fighter_reset(v,1,0);
}
int versus_captured(const Versus *v,unsigned i)
{return v->fighter[1-i].captured;}
int versus_damage_pose(const Versus *v,unsigned i)
{
    const DamageReaction *r=&v->reaction[i];
    if(!r->active)return -1;
    if(v->stun[i] || r->age<v->fighter[i].fighter->damage_ticks[r->pose])return (int)r->pose;
    return v->fighter[i].player.state==SCENE_AIR && r->pose>=12?17:-1;
}
static void target_snapshot(Versus *v,unsigned i)
{
    CombatScene *s=&v->fighter[i];unsigned j=1-i;const CombatScene *o=&v->fighter[j];
    s->target=o->player;s->percent=v->percent[j];s->own_percent=v->percent[i];
    s->hitstun=v->stun[j];s->damage_vx=v->launch[j];
    /* Respawn immunity, or the intangible window of a roll/tech/stand-up. */
    s->target_immune=v->invulnerable[j]!=0 || combat_intangible(o);s->hitstop=0;
    s->target_shield=combat_shield_radius(o);
    s->target_resist=combat_stone(o)?o->stone_resist:0;
    s->target_shield_x=o->fighter->shield_x*o->player.facing;s->target_shield_y=o->fighter->shield_y;
}
/* Sounds of a status that began this tick (script tick 0 + status SetStatus). */
static void action_sounds(Versus *v,unsigned i)
{
    const CombatScene *s=&v->fighter[i];
    if(!s->action || !s->fighter->actions)return;
    const FighterAction *a=&s->fighter->actions[s->action];
    for(unsigned k=0;k<2;++k)if(a->sound[k]<SOUND_COUNT)sound_event(v,(FightSound)a->sound[k],i);
    switch(s->action) {
    case ACTION_GUARD_ON:sound_event(v,SOUND_GUARD_ON,i);break;
    case ACTION_GUARD_OFF:sound_event(v,SOUND_GUARD_OFF,i);break;
    case ACTION_SHIELD_BREAK_FLY:sound_event(v,SOUND_SHIELD_BREAK,i);break;
    case ACTION_DOWN_BOUNCE_D:case ACTION_DOWN_BOUNCE_U:case ACTION_SHIELD_BREAK_DOWN_D:case ACTION_SHIELD_BREAK_DOWN_U:
        sound_event(v,(FightSound)s->fighter->sounds.down_bounce,i);break;
    default:break;
    }
}
void versus_step(Versus *v,const CombatInput input[2])
{
    v->sound_count=0;
    if(v->winner>=0)return;
    ++v->tick;
    if(v->freeze) {--v->freeze;return;}
    unsigned before_hits[2],before_respawn[2],before_shield[2],before_starts[2],before_resisted[2];int was_captured[2],falling[2];
    int order=v->fighter[0].player.x<=v->fighter[1].player.x?1:-1;
    unsigned sides=v->solo?1:2;
    for(unsigned i=0;i<2;++i) {
        target_snapshot(v,i);before_hits[i]=v->fighter[i].hits;
        before_respawn[i]=v->fighter[i].player.respawns;
        was_captured[i]=versus_captured(v,i);
        before_shield[i]=v->fighter[i].shield_hits;before_starts[i]=v->fighter[i].action_starts;
        before_resisted[i]=v->fighter[i].resisted;
        /* Airborne damage (hitstun or tumble): landing techs or knocks down. */
        falling[i]=v->stun[i] || versus_damage_pose(v,i)==17;
        combat_note_shield(&v->fighter[i],input[i].shield); /* counted even while stunned (tech buffer) */
    }
    for(unsigned i=0;i<sides;++i) {
        CombatScene *s=&v->fighter[i];
        if(v->invulnerable[i])--v->invulnerable[i];
        if(was_captured[i]) {
            ++s->tick;s->attack_frame=-1;
            CombatScene *k=&v->fighter[1-i];
            if(k->inhaled) {
                /* ftCommonCaptureWaitKirby: each button tap or stick flip inside
                 * Kirby shortens the breakout by 6 (1 + 5x the mash). */
                const CombatInput *in=&input[i];int stick=in->motion.stick_x>20?1:in->motion.stick_x< -20?-1:0;
                int mash=(in->attack && !s->attack_was_held) || (in->special && !s->special_was_held) ||
                         (in->motion.jump_held && !s->player.jump_was_held) || (stick && stick!=s->last_stick);
                s->attack_was_held=in->attack;s->special_was_held=in->special;s->player.jump_was_held=in->motion.jump_held;
                s->last_stick=stick;
                if(mash)k->inhale_breakout-=6;
            }
            continue;
        }
        CombatInput in=input[i];
        if(v->stun[i]) {
            --v->stun[i];in=(CombatInput){0};s->attack_frame=-1;
            s->player.cliff_cooldown=2;
        }
        s->player.hit_vx=v->launch[i];s->player.hit_vy=v->launch_y[i];
        unsigned lands=s->player.landings,jumps=s->player.air_jumps;
        SceneState old_state=s->player.state;
        int old_attack=s->attack_frame;MarioMove old_move=s->move;
        combat_update(s,in);
        if(v->reaction[i].active) {
            ++v->reaction[i].age;
            if(!v->stun[i] && (s->attack_frame>=0 || s->player.air_jumps!=jumps ||
               s->player.state==SCENE_KNEE || in.motion.stick_x))v->reaction[i].active=0;
        }
        /* Decay the external vector independently of ordinary gravity/drift.
         * Source air decay 1.7; ground damage friction = traction6 * .25.
         * Flat default surface material; no per-frame sqrt. */
        if(grounded(&s->player) || s->player.state==SCENE_LAND) {
            v->launch[i]=motion_ground_friction(s->player.hit_vx,s->player.attr->traction);
            v->launch_y[i]=0;
        } else {
            float x=s->player.hit_vx,y=s->player.hit_vy;
            float dx=v->reaction[i].decay_x,dy=v->reaction[i].decay_y;
            v->launch[i]=(x>0 && x<=dx)||(x<0 && x>=dx)?0:x-dx;
            v->launch_y[i]=(y>0 && y<=dy)||(y<0 && y>=dy)?0:y-dy;
            if(x==0)v->launch[i]=0;
            if(y==0)v->launch_y[i]=0;
        }
        if(s->player.landings!=lands) {
            sound_event(v,(FightSound)s->fighter->sounds.land,i);
            /* Actual landing animation replaces airborne damage. */
            int tumble=versus_damage_pose(v,i)==17;
            v->reaction[i].active=0;
            if(falling[i] && s->fighter->actions) {
                /* ftCommonDamageAirCommonProcMap / DamageFallProcMap: Z within
                 * 20 ticks techs (sideways with the stick), else DownBounce. */
                int x=input[i].motion.stick_x;unsigned a;
                if(s->shield_age<20)a=x>=20 || x<=-20?(x*s->player.facing>=0?ACTION_PASSIVE_F:ACTION_PASSIVE_B):ACTION_PASSIVE;
                else a=tumble?ACTION_DOWN_BOUNCE_D:ACTION_DOWN_BOUNCE_U;
                v->stun[i]=0;combat_action_start(s,a);
            } else if(v->stun[i])v->stun[i]=6;
        }
        /* Mario motion scripts Jump/JumpAerial play these voices. */
        if(s->player.air_jumps!=jumps)sound_event(v,(FightSound)s->fighter->sounds.jump_air,i);
        else if(old_state==SCENE_KNEE && s->player.state==SCENE_AIR)sound_event(v,(FightSound)s->fighter->sounds.jump,i);
        if(s->attack_frame>=0 && (s->attack_frame!=old_attack || s->move!=old_move))move_sounds(v,i);
    }
    int respawned=0;
    for(unsigned i=0;i<sides;++i) {
        MotionScene *p=&v->fighter[i].player;
        /* Local horizontal/top blast limits supplement the scene bottom. */
        if(p->respawns!=before_respawn[i] || p->x>6000 || p->x< -6000 || p->y>6000) {
            if(!v->solo)--v->stocks[i]; /* exploring: endless lives */
            fighter_reset(v,i,1);v->fighter[1-i].captured=0;respawned=1;
            /* ftCommonDead: attr dead_fgm_ids (voice, slam) + DeadExplodeL. */
            sound_event(v,(FightSound)v->fighter[i].fighter->sounds.dead,i);
            sound_event(v,(FightSound)v->fighter[i].fighter->sounds.dead_slam,i);
            sound_event(v,SOUND_DEAD_EXPLODE,i);
        }
    }
    if(!v->stocks[0] || !v->stocks[1]) {
        v->winner=!v->stocks[0] && !v->stocks[1]?2:(!v->stocks[0]?1:0);return;
    }
    if(respawned)return;
    MotionScene *a=&v->fighter[0].player,*b=&v->fighter[1].player;
    if(!v->solo && !v->fighter[0].captured && !v->fighter[1].captured && grounded(a) && grounded(b) &&
       !rolling(&v->fighter[0]) && !rolling(&v->fighter[1]) &&
       a->y-b->y<200 && b->y-a->y<200 && (b->x-a->x)*order<220) {
        /* Reduced grounded body push prevents the CPU from walking through
         * its opponent and turning back and forth forever at close range. */
        float middle=(a->x+b->x)*.5f;a->x=middle-order*110;b->x=middle+order*110;
    }
    /* Both contact passes see post-physics snapshots, before either damage
     * result is committed. This permits trades without player-order bias.
     * Exploring alone still resolves side 0 (projectiles and items). */
    for(unsigned i=0;i<sides;++i)target_snapshot(v,i);
    for(unsigned i=0;i<sides;++i) {
        CombatScene *f=&v->fighter[i];int returning=f->boomerang.returning,bomb=f->bomb.state;
        f->boomerang.caught=0;
        combat_resolve(f);
        /* Weapon/item events: boomerang turn and catch, bomb explosion. */
        if(f->boomerang.active && f->boomerang.returning && !returning)sound_event(v,SOUND_BOOMERANG_RETURN,i);
        if(f->boomerang.caught)sound_event(v,SOUND_BOOMERANG_CATCH,i);
        if(f->bomb.state==BOMB_EXPLODING && bomb!=BOMB_EXPLODING)sound_event(v,SOUND_EXPLODE_L,i);
    }
    if(v->fighter[0].captured && v->fighter[1].captured)
        v->fighter[(v->tick&1)?0:1].captured=0;
    for(unsigned i=0;i<sides;++i) {
        CombatScene *s=&v->fighter[i];unsigned j=1-i;
        if(s->hits!=before_hits[i]) {
            v->percent[j]=s->percent;v->stun[j]=s->hitstun;v->launch[j]=s->damage_vx;
            v->launch_y[j]=s->damage_vy;
            /* Tick0 of these clips is a neutral setup pose. Show the first
             * authored recoil pose at contact, including during hitstop. */
            v->reaction[j]=(DamageReaction){s->damage_pose,1,1,
                1.7f*s->damage_vx/s->damage_kb,1.7f*s->damage_vy/s->damage_kb};
            v->fighter[j].player=s->target;
            v->fighter[j].attack_frame=-1;v->fighter[j].dash_ticks=0;
            v->fighter[j].captured=0;v->fighter[j].inhaled=0;v->fighter[j].action=ACTION_NONE;
            v->fighter[j].brake_ticks=0;v->fighter[j].stone=0;v->fighter[j].stone_resist=0;
            if(s->damage_sound!=SOUND_NONE)sound_event(v,s->damage_sound,j);
            /* ftCommonDamage compares hitstun with DAMAGEVOICE_MIN (80). */
            if(s->hitstun>=80)sound_event(v,(FightSound)v->fighter[j].fighter->sounds.damage,j);
            if(s->hitstop>v->freeze)v->freeze=s->hitstop;
        } else if(s->resisted!=before_resisted[i]) {
            /* Kirby's stone absorbed the hit: hitlag and sound only. */
            v->fighter[j].stone_resist=s->target_resist;
            if(s->damage_sound!=SOUND_NONE)sound_event(v,s->damage_sound,j);
            if(s->hitstop>v->freeze)v->freeze=s->hitstop;
        } else if(s->shield_hits!=before_shield[i]) {
            /* Blocked: hitlag for both, shield stun/push-back or a break. */
            combat_shield_hit(&v->fighter[j],s->shield_hit_damage,s->shield_hit_dir);
            if(s->shield_hit_sound<SOUND_COUNT)sound_event(v,(FightSound)s->shield_hit_sound,j);
            if(s->hitstop>v->freeze)v->freeze=s->hitstop;
        } else if(s->captured) {
            v->fighter[j].player=s->target;
            v->fighter[j].attack_frame=-1;v->fighter[j].action=ACTION_NONE;
            v->stun[j]=0;v->launch[j]=v->launch_y[j]=0;
            v->reaction[j].active=0;
            v->fighter[j].player.hit_vx=v->fighter[j].player.hit_vy=0;
        }
    }
    for(unsigned i=0;i<2;++i)if(v->fighter[i].action_starts!=before_starts[i])action_sounds(v,i);
}
CombatInput versus_cpu(const Versus *v,unsigned who)
{
    CombatInput in={0};const CombatScene *s=&v->fighter[who],*o=&v->fighter[1-who];
    const MotionScene *p=&s->player,*other=&o->player;
    float dx=other->x-p->x,dy=other->y-p->y,dist=dx<0?-dx:dx;
    int dir=dx<0?-1:1;
    /* Bounded decision pulses: no allocations, path search or RNG. */
    unsigned beat=(v->tick/6+who*3)%16;
    int pulse=v->tick%6==0;
    /* Tech: tap the shield just before landing from a launch (not always). */
    if(v->stun[who] && p->state==SCENE_AIR && p->vy<0 && p->y<320 && p->y>-50 && beat%3)in.shield=pulse;
    if(versus_captured(v,who) && o->inhaled) {in.attack=pulse;return in;} /* mash out of Kirby */
    if(v->winner>=0 || v->freeze || v->stun[who] || versus_captured(v,who))return in;
    if(s->action) {
        switch(s->action) {
        case ACTION_DOWN_BOUNCE_D:case ACTION_DOWN_BOUNCE_U:
            if(pulse && dist<350)in.attack=1; /* buffered get-up attack */
            break;
        case ACTION_DOWN_WAIT_D:case ACTION_DOWN_WAIT_U:
            if(pulse) {if(dist<350)in.motion.stick_x=-dir*80;else in.shield=1;}
            break;
        case ACTION_GUARD_ON:case ACTION_GUARD:
            /* Hold while the opponent attacks; roll away when crowded. */
            in.shield=o->attack_frame>=0 || beat%4!=0;
            if(pulse && dist<220 && beat==5)in.motion.stick_x=-dir*80;
            break;
        case ACTION_FURAFURA:in.attack=pulse;break;
        default:break;
        }
        return in;
    }
    /* Hanging: a neutral stick first (CliffWait arms the climb), then inward. */
    if(p->state==SCENE_CLIFF) {in.motion.stick_x=p->state_ticks<12?0:-p->cliff_side*80;return in;}
    if((p->x>2050 || p->x< -2050) && p->y<150) {
        in.motion.stick_x=p->x>0?-80:80;
        if(p->vy<0 && p->y<0 && p->jumps_used<p->attr->jumps_max)in.motion.jump_held=pulse;
        else if(p->y< -450 && !p->special_used && p->jumps_used>=p->attr->jumps_max) {
            in.up=1;in.special=pulse;
        }
        return in;
    }
    if(s->captured) {in.attack=pulse;return in;}
    if(s->attack_frame>=0) {
        if(s->move==MOVE_JAB1 || s->move==MOVE_JAB2)in.attack=pulse;
        return in;
    }
    /* Turn toward the opponent, unless almost straight above/below: walking
     * past each other every tick would otherwise flip both forever. */
    if(grounded(p) && p->facing!=dir && (dist>120 || (dy<200 && dy> -200))) {in.motion.stick_x=dir*80;return in;}
    /* Block an incoming attack most of the time (the shield decays, so not always). */
    if(grounded(p) && o->attack_frame>=0 && dist<450 && dy<200 && dy> -200 && beat%3!=0 && s->shield_health>10) {
        in.shield=1;return in;
    }
    if(dist>260)in.motion.stick_x=dir*80;
    /* Do not walk off the stage after an opponent hanging or recovering. */
    if(grounded(p) && p->x*dir>2100)in.motion.stick_x=0;
    if(dy>350 && grounded(p))in.motion.jump_held=pulse;
    if(dy< -450 && p->platform>0)in.drop=pulse;
    if(pulse && dist<380 && dy<350 && dy> -300) {
        /* Select a grounded direction before attacking; face the opponent. */
        in.motion.stick_x=dir*80;in.attack=1;
        if(beat%4==0)in.smash=1;
        else if(beat%4==1) {in.motion.stick_x=0;in.up=dy>100;}
        if(beat==7 && grounded(p)) {in.attack=0;in.grab=1;}
    } else if(pulse && dist>500 && dist<1500 && dy<250 && dy> -250 && beat==0)
        in.special=1;
    return in;
}
