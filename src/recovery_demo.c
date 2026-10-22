#include "recovery_demo.h"

void recovery_demo_begin(MotionScene *s,int side)
{
    scene_init_stage(s,STAGE_DREAMLAND);
    s->x=side*1900;s->facing=side;
}

SceneInput recovery_demo_input(const MotionScene *s,unsigned frame,int side)
{
    /* Only initial placement is scripted. Leave the island, air jump, turn
     * toward the edge, catch and stand up through ordinary scene inputs. */
    if(s->state==SCENE_CLIFF)
        return (SceneInput){0,s->state_ticks==18};
    if(s->state==SCENE_CLIMB || s->cliff_grabs)return (SceneInput){0,0};
    return (SceneInput){frame<59?side*80:-side*80,frame==59};
}

SceneInput recovery_demo_special_input(const MotionScene *s,unsigned frame,int side)
{
    if(s->state==SCENE_CLIFF)return (SceneInput){0,s->state_ticks==18};
    if(s->state==SCENE_CLIMB || s->cliff_grabs ||
       (s->special_uses && (s->state==SCENE_WAIT || s->state==SCENE_LAND)))return (SceneInput){0,0};
    return (SceneInput){frame<70?side*80:-side*80,frame==70};
}
