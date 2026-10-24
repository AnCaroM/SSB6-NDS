#pragma once
#include "fighter_model.h"
/* Generic bone renderer for converted fighters. Textures are uploaded once
 * per model; draws use matrix-stack slots 1..slot_count and leave the current
 * matrix changed, so callers push before and pop after (one level only). */
#define FIGHTER_MAX_TEXTURES 32
typedef struct { const FighterModel *model; int textures[FIGHTER_MAX_TEXTURES]; } FighterRender;
typedef struct { unsigned clip,tick,weight; } FighterBlend; /* weight 0..256 toward current */
int fighter_render_init(FighterRender *render,const FighterModel *model);
void fighter_render_draw(const FighterRender *render,unsigned clip,unsigned age,unsigned tint,const FighterBlend *from);
