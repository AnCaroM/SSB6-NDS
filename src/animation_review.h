#ifndef ANIMATION_REVIEW_H
#define ANIMATION_REVIEW_H
#include "versus.h"
/* Separate diagnostic ROM, never part of the normal game input flow. */
int animation_review_step(Versus *match,unsigned wall_frame);
#endif
