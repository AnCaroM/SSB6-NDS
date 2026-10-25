#pragma once
#include "fighter_model.h"
/* Generic bone renderer for converted fighters. Textures are uploaded once
 * per model; draws use matrix-stack slots 1..slot_count and leave the current
 * matrix changed, so callers push before and pop after (one level only). */
#define FIGHTER_MAX_TEXTURES 32
typedef struct { const FighterModel *model; int textures[FIGHTER_MAX_TEXTURES]; } FighterRender;
typedef struct { unsigned clip,tick,weight; } FighterBlend; /* weight 0..256 toward current */
int fighter_render_init(FighterRender *render,const FighterModel *model);
/* FIGHTER_DRAW_ROOT_MOTION also draws TransN movement (clip root 2) that
 * gameplay applies to the fighter position; the viewer uses it. */
#define FIGHTER_DRAW_ROOT_MOTION 1
void fighter_render_draw(const FighterRender *render,unsigned clip,unsigned age,unsigned tint,
                         const FighterBlend *from,unsigned flags);
/* One model part on its own (e.g. Link's thrown boomerang): its display-list
 * local vertices under the current matrix, in model units/divisor. */
void fighter_render_draw_part(const FighterRender *render,unsigned part,unsigned tint);
