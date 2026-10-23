/* Local two-player scheduler and deliberately simple CPU, not the N64 AI. */
#include "versus.h"
#include "motion.h"
#include <string.h>

static int grounded(const MotionScene *p)
{return p->state==SCENE_WAIT || p->state==SCENE_WALK;}
static void sound_event(Versus *v,FightSound sound,unsigned fighter)
{if(v->sound_count<8)v->sounds[v->sound_count++]=(FightSoundEvent){sound,fighter};}
static void fighter_reset(Versus *v,unsigned i,int respawn)
{
    CombatScene *s=&v->fighter[i];combat_init_stage(s,STAGE_DREAMLAND);
    s->duel_mode=1;s->player.x=i?650:-650;s->player.facing=i?-1:1;
    s->facing=s->attack_facing=s->player.facing;
    if(respawn) {
        s->player.y=1800;s->player.state=SCENE_AIR;s->player.platform=-1;
        s->player.jumps_used=1;v->invulnerable[i]=90;
    }
    v->percent[i]=v->stun[i]=0;v->launch[i]=v->launch_y[i]=0;
    v->reaction[i]=(DamageReaction){0};
}
void versus_init(Versus *v)
{
    memset(v,0,sizeof *v);v->winner=-1;v->stocks[0]=v->stocks[1]=3;
    fighter_reset(v,0,0);fighter_reset(v,1,0);
}
int versus_captured(const Versus *v,unsigned i)
{return v->fighter[1-i].captured;}
static void target_snapshot(Versus *v,unsigned i)
{
    CombatScene *s=&v->fighter[i];unsigned j=1-i;
    s->target=v->fighter[j].player;s->percent=v->percent[j];
    s->hitstun=v->stun[j];s->damage_vx=v->launch[j];
    s->target_immune=v->invulnerable[j]!=0;s->hitstop=0;
}
void versus_step(Versus *v,const CombatInput input[2])
{
    v->sound_count=0;
    if(v->winner>=0)return;
    ++v->tick;
    if(v->freeze) {--v->freeze;return;}
    unsigned before_hits[2],before_respawn[2];int was_captured[2];
    int order=v->fighter[0].player.x<=v->fighter[1].player.x?1:-1;
    for(unsigned i=0;i<2;++i) {
        target_snapshot(v,i);before_hits[i]=v->fighter[i].hits;
        before_respawn[i]=v->fighter[i].player.respawns;
        was_captured[i]=versus_captured(v,i);
    }
    for(unsigned i=0;i<2;++i) {
        CombatScene *s=&v->fighter[i];
        if(v->invulnerable[i])--v->invulnerable[i];
        if(was_captured[i]) {++s->tick;s->attack_frame=-1;continue;}
        CombatInput in=input[i];
        if(v->stun[i]) {
            --v->stun[i];in=(CombatInput){0};s->attack_frame=-1;
            s->player.cliff_cooldown=2;
        }
        s->player.hit_vx=v->launch[i];s->player.hit_vy=v->launch_y[i];
        unsigned lands=s->player.landings,jumps=s->player.air_jumps;
        SceneState old_state=s->player.state;
        int old_attack=s->attack_frame;
        combat_update(s,in);
        if(v->stun[i])++v->reaction[i].age;
        /* Decay the external vector independently of ordinary gravity/drift.
         * Source air decay 1.7; ground damage friction = traction6 * .25.
         * Flat default surface material; no per-frame sqrt. */
        if(grounded(&s->player) || s->player.state==SCENE_LAND) {
            v->launch[i]=motion_ground_friction(s->player.hit_vx,1.5f);
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
            sound_event(v,SOUND_LAND,i);
            if(v->stun[i]) {v->stun[i]=6;v->reaction[i].pose=3;v->reaction[i].age=0;}
        }
        if((old_state==SCENE_KNEE && s->player.state==SCENE_AIR) || s->player.air_jumps!=jumps)
            sound_event(v,SOUND_JUMP,i);
        if(old_attack<0 && s->attack_frame>=0)sound_event(v,SOUND_SWING,i);
    }
    int respawned=0;
    for(unsigned i=0;i<2;++i) {
        MotionScene *p=&v->fighter[i].player;
        /* Local horizontal/top blast limits supplement the scene bottom. */
        if(p->respawns!=before_respawn[i] || p->x>6000 || p->x< -6000 || p->y>6000) {
            --v->stocks[i];fighter_reset(v,i,1);v->fighter[1-i].captured=0;respawned=1;
            sound_event(v,SOUND_KO,i);
        }
    }
    if(!v->stocks[0] || !v->stocks[1]) {
        v->winner=!v->stocks[0] && !v->stocks[1]?2:(!v->stocks[0]?1:0);return;
    }
    if(respawned)return;
    MotionScene *a=&v->fighter[0].player,*b=&v->fighter[1].player;
    if(!v->fighter[0].captured && !v->fighter[1].captured && grounded(a) && grounded(b) &&
       a->y-b->y<200 && b->y-a->y<200 && (b->x-a->x)*order<220) {
        /* Reduced grounded body push prevents the CPU from walking through
         * its opponent and turning back and forth forever at close range. */
        float middle=(a->x+b->x)*.5f;a->x=middle-order*110;b->x=middle+order*110;
    }
    /* Both contact passes see post-physics snapshots, before either damage
     * result is committed. This permits trades without player-order bias. */
    for(unsigned i=0;i<2;++i)target_snapshot(v,i);
    for(unsigned i=0;i<2;++i) {
        int spawned=v->fighter[i].spawned;
        combat_resolve(&v->fighter[i]);
        if(!spawned && v->fighter[i].spawned)sound_event(v,SOUND_FIRE,i);
    }
    if(v->fighter[0].captured && v->fighter[1].captured)
        v->fighter[(v->tick&1)?0:1].captured=0;
    for(unsigned i=0;i<2;++i) {
        CombatScene *s=&v->fighter[i];unsigned j=1-i;
        if(s->hits!=before_hits[i]) {
            v->percent[j]=s->percent;v->stun[j]=s->hitstun;v->launch[j]=s->damage_vx;
            v->launch_y[j]=s->damage_vy;
            v->reaction[j]=(DamageReaction){s->damage_pose,0,
                1.7f*s->damage_vx/s->damage_kb,1.7f*s->damage_vy/s->damage_kb};
            v->fighter[j].player=s->target;
            v->fighter[j].attack_frame=-1;v->fighter[j].dash_ticks=0;
            v->fighter[j].captured=0;
            v->fighter[j].brake_ticks=0;
            sound_event(v,s->damage_amount>=10?SOUND_HEAVY:SOUND_HIT,j);
            if(s->hitstop>v->freeze)v->freeze=s->hitstop;
        } else if(s->captured) {
            v->fighter[j].player=s->target;
            v->fighter[j].attack_frame=-1;
            v->stun[j]=0;v->launch[j]=v->launch_y[j]=0;
            v->fighter[j].player.hit_vx=v->fighter[j].player.hit_vy=0;
        }
    }
}
CombatInput versus_cpu(const Versus *v,unsigned who)
{
    CombatInput in={0};const CombatScene *s=&v->fighter[who];
    const MotionScene *p=&s->player,*other=&v->fighter[1-who].player;
    if(v->winner>=0 || v->freeze || v->stun[who] || versus_captured(v,who))return in;
    float dx=other->x-p->x,dy=other->y-p->y,dist=dx<0?-dx:dx;
    int dir=dx<0?-1:1;
    /* Bounded decision pulses: no allocations, path search or RNG. */
    unsigned beat=(v->tick/6+who*3)%16;
    int pulse=v->tick%6==0;
    if(p->state==SCENE_CLIFF) {in.motion.stick_x=-p->cliff_side*80;return in;}
    if((p->x>2050 || p->x< -2050) && p->y<150) {
        in.motion.stick_x=p->x>0?-80:80;
        if(p->vy<0 && p->y<0 && p->jumps_used<2)in.motion.jump_held=pulse;
        else if(p->y< -450 && !p->special_used && p->jumps_used>=2) {
            in.up=1;in.special=pulse;
        }
        return in;
    }
    if(s->captured) {in.attack=pulse;return in;}
    if(s->attack_frame>=0) {
        if(s->move==MOVE_JAB1 || s->move==MOVE_JAB2)in.attack=pulse;
        return in;
    }
    if(grounded(p) && p->facing!=dir) {in.motion.stick_x=dir*80;return in;}
    if(dist>260)in.motion.stick_x=dir*80;
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
