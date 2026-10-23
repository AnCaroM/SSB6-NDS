#ifndef SSB_COMBAT_H
#define SSB_COMBAT_H
#include "motion_scene.h"
#include "moves.h"
typedef struct { SceneInput motion; int attack,special,grab,smash,up,down,variant,drop; } CombatInput;
typedef struct { float x,y,vx,vy; unsigned life; int facing; } Fireball;

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
} CombatScene;

void combat_init(CombatScene *scene);
void combat_init_stage(CombatScene *scene, StageId stage);
void combat_step(CombatScene *scene, SceneInput input, int attack_held, int drop);
float combat_jab_knockback(unsigned percent_before);
int combat_attack_active(const CombatScene *scene);
void combat_update(CombatScene *scene,CombatInput input);
int combat_start_move(CombatScene *scene,MarioMove move);
unsigned combat_player_clip(const CombatScene *scene);
float combat_knockback(unsigned percent,const MoveHit *hit);
void combat_demo_begin(CombatScene *scene,unsigned index);
/* In duel mode both controllers/physics run before either contact pass. */
void combat_resolve(CombatScene *scene);
#endif
