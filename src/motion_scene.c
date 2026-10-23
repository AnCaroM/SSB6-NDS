#include "motion_scene.h"
#include "motion.h"
#include "special_hi.h"
#include <string.h>

/* Mario's numeric attributes: relocData/203_MarioMain.c at revision
 * 572a747d8171137718fad472c5d7473dc7309ffd.
 * Stage solver, turning, landing duration and respawn are provisional.
 * No dash, fastfall, full combat or GObj scheduling. */

static void enter(MotionScene *s, SceneState state)
{
    s->state = state;
    s->state_ticks = 0;
}

void scene_init(MotionScene *s)
{
    scene_init_stage(s, STAGE_FLAT);
}

void scene_init_stage(MotionScene *s, StageId stage)
{
    memset(s, 0, sizeof *s);
    s->state = SCENE_WAIT;
    s->stage = stage;
    s->ignored_platform = -1;
    s->facing=1;
    s->landing_ticks=3;
}

int scene_special_hi(MotionScene *s,int stick_x)
{
    if(s->special_used || (s->state!=SCENE_WAIT && s->state!=SCENE_WALK && s->state!=SCENE_AIR))return 0;
    if(stick_x>80)stick_x=80;
    if(stick_x<-80)stick_x=-80;
    s->special_air=s->state==SCENE_AIR;
    if(stick_x>20 || stick_x<-20)s->facing=stick_x>0?1:-1;
    s->special_tilt=0;
    if(stick_x>50)s->special_tilt=-(stick_x-50)*3/5;
    if(stick_x<-50)s->special_tilt=-(stick_x+50)*3/5;
    s->special_used=1;++s->special_uses;s->air_jump=0;s->jumps_used=2;
    if(s->special_air) {s->vy=0;s->vx/=1.5f;}
    else s->vx=s->vy=0;
    s->platform=-1;enter(s,SCENE_SPECIAL_HI);return 1;
}

static void cliff_drop(MotionScene *s)
{
    s->cliff_cooldown=30;s->platform=-1;s->vx=0;s->vy=-2;
    enter(s,SCENE_AIR);
}

void scene_drop_through(MotionScene *s)
{
    if(s->state==SCENE_LAND && s->landing_ticks==25)return;
    if(s->state==SCENE_CLIFF) {cliff_drop(s);return;}
    if ((s->state==SCENE_WAIT || s->state==SCENE_WALK || s->state==SCENE_LAND) && s->platform > 0) {
        s->ignored_platform = s->platform;
        s->drop_ticks = 12;
        s->platform = -1;
        s->jumps_used=1;s->air_jump=0;
        s->vy = -2;
        enter(s, SCENE_AIR);
    }
}

const char *scene_state_name(SceneState state)
{
    static const char *names[] = {"WAIT", "WALK", "KNEE", "AIR", "LAND", "CLIFF", "CLIMB", "UP-B", "FREEFALL"};
    return (unsigned)state < sizeof names / sizeof *names ? names[state] : "INVALID";
}

void scene_step(MotionScene *s, SceneInput input)
{
    if (input.stick_x > 80) input.stick_x = 80;
    if (input.stick_x < -80) input.stick_x = -80;
    int pressed = input.jump_held && !s->jump_was_held;
    int released = !input.jump_held && s->jump_was_held;
    s->jump_was_held = !!input.jump_held;
    ++s->tick;
    if(s->cliff_cooldown)--s->cliff_cooldown;
    if (s->drop_ticks && --s->drop_ticks == 0) s->ignored_platform = -1;
    if(s->state==SCENE_CLIFF) {
        ++s->state_ticks;
        /* As in CliffWait, release the stick before choosing a ledge action. */
        if(input.stick_x>-20 && input.stick_x<20)s->cliff_ready=1;
        if(s->state_ticks>=1080)cliff_drop(s);
        else if(s->state_ticks>=SCENE_CATCH_TICKS && (pressed || s->cliff_ready)) {
            int toward=input.stick_x * -s->cliff_side;
            if(pressed || toward>=20)enter(s,SCENE_CLIMB);
            else if(toward<=-20)cliff_drop(s);
        }
        return;
    }
    if(s->state==SCENE_CLIMB) {
        /* Local twelve-tick stand-up path; original climb animation pending. */
        unsigned t=++s->state_ticks;
        s->x=s->cliff_side*(2318+180-360*(float)t/12);
        s->y=-300+300*(float)t/12;
        if(t>=12) {s->platform=0;s->vx=s->vy=0;s->jumps_used=0;s->air_jump=0;s->special_used=0;s->landing_ticks=3;enter(s,SCENE_LAND);}
        return;
    }
    if(s->state!=SCENE_AIR && s->state!=SCENE_SPECIAL_HI && s->state!=SCENE_SPECIAL_FALL && (input.stick_x>=8 || input.stick_x<=-8))
        s->facing=input.stick_x>0?1:-1;

    switch (s->state) {
    case SCENE_WAIT:
    case SCENE_WALK:
        if (pressed) {
            s->short_hop = 0;
            enter(s, SCENE_KNEE);
        } else {
            if (input.stick_x >= 8 || input.stick_x <= -8) {
                enter(s, SCENE_WALK);
                float speed = s->vx < 0.0f ? -s->vx : s->vx;
                speed = motion_walk_speed(speed, input.stick_x, 0.3f, 1.5f);
                s->vx = input.stick_x < 0 ? -speed : speed;
            } else {
                enter(s, SCENE_WAIT);
                s->vx = motion_ground_friction(s->vx, 6.0f);
            }
        }
        break;
    case SCENE_KNEE:
        ++s->state_ticks;
        if (released && s->state_ticks <= 3) s->short_hop = 1;
        if (s->state_ticks >= 3) {
            s->vy = motion_button_jump_y(input.stick_x, s->short_hop) * 0.7f + 26.0f;
            s->vx = input.stick_x * 0.35f;
            s->platform = -1;
            s->jumps_used=1;s->air_jump=0;
            enter(s, SCENE_AIR);
        }
        break;
    case SCENE_AIR:
        ++s->state_ticks;
        if(s->air_jump)++s->air_jump_ticks;
        if(pressed && s->jumps_used<2) {
            /* ftCommonJumpAerialSetStatus, Mario attrs: 80*.7+26, *.9;
             * horizontal impulse stick*.35, forward/back threshold -10. */
            s->air_jump=input.stick_x*s->facing>=-10?1:2;
            s->air_jump_ticks=0;
            s->vy=(80*.7f+26)*.9f;s->vx=input.stick_x*.35f;
            s->jumps_used=2;++s->air_jumps;
        }
        /* Preserve the prototype's immediate air steering/turning. Select
         * the original forward/back clip before this local facing update. */
        if(input.stick_x>=8 || input.stick_x<=-8)s->facing=input.stick_x>0?1:-1;
        break;
    case SCENE_LAND:
        /* Placeholder recovery; does not claim original landing animation timing. */
        if (++s->state_ticks >= s->landing_ticks) enter(s, SCENE_WAIT);
        break;
    case SCENE_SPECIAL_HI:
        ++s->state_ticks;
        if(s->state_ticks<=SPECIAL_HI_TICKS) {
            if(s->state_ticks<=6 && s->special_air) {
                s->vy=motion_gravity(s->vy,.5f,44);s->vx=motion_air_friction(s->vx,.2f);
            } else {
                special_hi_velocity(s->state_ticks,s->facing,s->special_tilt,&s->vx,&s->vy);
                if(s->special_air) {s->vx*=.95f;s->vy*=.95f;}
            }
        } else {
            enter(s,SCENE_SPECIAL_FALL);s->vx=motion_ground_clamp(s->vx,18);
        }
        break;
    case SCENE_SPECIAL_FALL:
        ++s->state_ticks;
        break;
    case SCENE_CLIFF:
    case SCENE_CLIMB:
        break; /* Handled before ordinary physics. */
    }

    if (s->state == SCENE_KNEE || s->state == SCENE_LAND)
        s->vx = motion_ground_friction(s->vx, 6.0f);
    if (s->state == SCENE_AIR) {
        s->vy = motion_gravity(s->vy, 2.4f, 44.0f);
        s->vx = motion_air_accel(s->vx, input.stick_x, 8, 0.025f, 30.0f);
        s->vx = motion_air_friction(s->vx, 0.2f);
    }
    if(s->state==SCENE_SPECIAL_FALL) {
        s->vy=motion_gravity(s->vy,2.4f,44);
        s->vx=motion_air_accel(s->vx,input.stick_x,8,.025f,18);
        s->vx=motion_air_friction(s->vx,.2f);
    }

    float previous_y = s->y, previous_x = s->x;
    float total_vx=s->vx+s->hit_vx,total_vy=s->vy+s->hit_vy;
    s->x += total_vx;
    s->y += total_vy;
    int aerial=s->state==SCENE_AIR || s->state==SCENE_SPECIAL_HI || s->state==SCENE_SPECIAL_FALL;
    if(s->stage==STAGE_DREAMLAND && aerial && !s->cliff_cooldown &&
       total_vy<0 && previous_y+360>=0 && s->y+360<=0) {
        float t=(previous_y+360)/(previous_y-s->y);
        float crossing=previous_x+(s->x-previous_x)*t;
        int side=crossing<0?-1:1;
        float outside=crossing*side-2318;
        if(outside>=0 && outside<=400 && s->facing==-side) {
            s->cliff_side=side;s->x=side*(2318+180);s->y=-300;
            s->vx=s->vy=0;s->platform=-1;s->cliff_ready=0;
            s->jumps_used=1;s->air_jump=0;
            s->special_used=0;
            ++s->cliff_grabs;enter(s,SCENE_CLIFF);return;
        }
    }
    int special_landing=s->state==SCENE_SPECIAL_HI && s->state_ticks>6 && s->vy<0;
    float incoming_x=total_vx,incoming_y=total_vy;
    stage_resolve_solid(s->stage,previous_x,previous_y,&s->x,&s->y,&total_vx,&total_vy);
    if(total_vx!=incoming_x) {s->vx=total_vx;s->hit_vx=0;}
    if(total_vy!=incoming_y) {s->vy=total_vy;s->hit_vy=0;}
    unsigned count;
    const StagePlatform *platforms = stage_platforms(s->stage, &count);
    int landing = -1, supported = 0;
    for (unsigned i = 0; i < count; ++i) {
        const StagePlatform *p = &platforms[i];
        int within = s->x >= p->left && s->x <= p->right;
        if (within && s->y == p->height) supported = 1;
        if (!aerial || (s->state==SCENE_SPECIAL_HI && !special_landing) || total_vy > 0 || (int)i == s->ignored_platform) continue;
        if (!within || previous_y < p->height || s->y > p->height) continue;
        float fraction = previous_y == s->y ? 0 : (previous_y-p->height)/(previous_y-s->y);
        float crossing_x = previous_x + (s->x-previous_x)*fraction;
        if (crossing_x < p->left || crossing_x > p->right) continue;
        if (landing < 0 || p->height > platforms[landing].height) landing = (int)i;
    }
    if (landing >= 0) {
        s->landing_ticks=(s->state==SCENE_SPECIAL_HI || s->state==SCENE_SPECIAL_FALL)?25:3;
        s->platform = landing;
        s->y = platforms[landing].height;
        s->vy = 0.0f;
        s->hit_vy=0;
        s->jumps_used=0;s->air_jump=0;
        s->special_used=0;
        ++s->landings;
        enter(s, SCENE_LAND);
    } else if (!aerial && !supported) {
        s->platform = -1;
        s->short_hop = 0;
        s->jumps_used=1;s->air_jump=0;
        enter(s, SCENE_AIR);
    }
    if (s->y < stage_blast_bottom(s->stage)) {
        uint32_t tick = s->tick;
        unsigned respawns = s->respawns + 1, landings = s->landings;
        int held = s->jump_was_held;
        StageId stage = s->stage;
        scene_init_stage(s, stage);
        s->tick = tick;
        s->landings = landings;
        s->respawns = respawns;
        s->jump_was_held = held;
    }
}
