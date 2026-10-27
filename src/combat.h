#ifndef SSB_COMBAT_H
#define SSB_COMBAT_H
#include "motion_scene.h"
#include "moves.h"
#include "fighter.h"
typedef struct { SceneInput motion; int attack,special,grab,smash,up,down,variant,drop,shield; } CombatInput;
typedef struct { float x,y,vx,vy; unsigned life; int facing; } Fireball;
/* Link's boomerang (wplinkboomerang.c, reduced): unit direction and speed. */
typedef struct { float x,y,dx,dy,speed,turn; unsigned life,homing,age; int active,returning,hit,caught; } Boomerang;
/* Link's bomb item (itlinkbomb.c, reduced). */
typedef enum { BOMB_NONE, BOMB_HELD, BOMB_FLYING, BOMB_RESTING, BOMB_EXPLODING } BombState;
typedef struct { float x,y,vx,vy; unsigned life,age,landings; BombState state; int hit; } Bomb;

/* Reduced training integration, with an inert target. Collision volumes are
 * proxies, not the original bone-linked hitboxes or full damage state machine. */
typedef struct {
    MotionScene player, target;
    unsigned tick, percent, hits, knockouts, hitstop, hitstun;
    int facing, attack_frame, attack_was_held, attack_connected;
    float damage_vx;
    float damage_vy,damage_kb;
    unsigned damage_pose,damage_amount,damage_sound;
    MarioMove move;
    int attack_facing,special_was_held,grab_was_held,queued_combo,captured;
    unsigned capture_age,dash_ticks,tap_age,run_age,brake_ticks;
    int last_stick,last_tap,tornado_charge,spawned;
    uint64_t connected_strikes;
    Fireball fireballs[4];
    int duel_mode,target_immune;
    const Fighter *fighter;   /* moves, attributes and sounds of the attacker */
    Boomerang boomerang; Bomb bomb;
    int aim,boomerang_smash,boomerang_empty; /* aim: +1 up, -1 down at boomerang spawn */
    unsigned item_throw;                      /* ticks into a held-bomb throw, 0 none */
    float throw_vx,throw_vy;
    /* Shield, rolls, techs and knockdowns (fighter.h FighterActionId; reduced
     * from ftcommonguard/escape/passive/down/shieldbreak/furafura). */
    unsigned action,action_tick,action_starts;
    int action_facing,shield_release;
    int shield_health;                        /* 55 full (ftmanager), 30 after a break */
    unsigned shield_heal_wait,shield_decay_wait,setoff_ticks,down_wait,down_attack_buffer,breakout_wait;
    int shield_was_held,shield_age,own_percent,mash_stick; /* shield_age: ticks since the last press */
    float target_shield,target_shield_x,target_shield_y;   /* target's shield (snapshot), radius 0 = none */
    unsigned shield_hits,shield_hit_damage,shield_hit_sound;int shield_hit_dir;
} CombatScene;

void combat_init(CombatScene *scene);
void combat_init_stage(CombatScene *scene, StageId stage);
/* Choose the character (default Mario); resets nothing else. */
void combat_set_fighter(CombatScene *scene,const Fighter *fighter);
void combat_step(CombatScene *scene, SceneInput input, int attack_held, int drop);
float combat_jab_knockback(unsigned percent_before);
int combat_attack_active(const CombatScene *scene);
void combat_update(CombatScene *scene,CombatInput input);
int combat_start_move(CombatScene *scene,MarioMove move);
unsigned combat_player_clip(const CombatScene *scene);
float combat_knockback(unsigned percent,const MoveHit *hit);
float combat_knockback_weight(unsigned percent,const MoveHit *hit,float weight);
void combat_demo_begin(CombatScene *scene,unsigned index);
/* In duel mode both controllers/physics run before either contact pass. */
void combat_resolve(CombatScene *scene);
/* Shield button edge/age (tech buffer); the duel calls it before input lock. */
void combat_note_shield(CombatScene *scene,int held);
/* Begin a status from outside (landing while damaged: tech or knockdown). */
void combat_action_start(CombatScene *scene,unsigned action);
/* Clip and age of the current action (0 when none). */
unsigned combat_action_clip(const CombatScene *scene,unsigned *age);
float combat_shield_radius(const CombatScene *scene); /* 0 unless the shield is up */
int combat_intangible(const CombatScene *scene);      /* SetHitStatusAll(3) window */
/* Victim side of a blocked hit: shield stun/push-back or shield break. */
void combat_shield_hit(CombatScene *scene,unsigned damage,int dir);
#endif
