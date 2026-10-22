#ifndef SSB_RECOVERY_DEMO_H
#define SSB_RECOVERY_DEMO_H
#include "motion_scene.h"
void recovery_demo_begin(MotionScene *s,int side);
SceneInput recovery_demo_input(const MotionScene *s,unsigned frame,int side);
#define RECOVERY_SPECIAL_FRAME 100
SceneInput recovery_demo_special_input(const MotionScene *s,unsigned frame,int side);
#endif
