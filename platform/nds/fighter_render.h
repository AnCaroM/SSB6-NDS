#pragma once
#include "fighter_model.h"
/* Generic bone renderer for converted fighters. load/unload upload a model's
 * textures and build its display lists, so only the fighters in use occupy
 * VRAM; draws use matrix-stack slots 1..slot_count and leave the current
 * matrix changed, so callers push before and pop after (one level only). */
#define FIGHTER_MAX_TEXTURES 32
#define FIGHTER_LIST_TINTS 3   /* vertex-colour tints with prebuilt lists */
#define FIGHTER_BUCKETS 33     /* model parts 0..31 + untagged triangles */
/* part_set/part_clear adjust the visible-part mask (e.g. Link holding a bomb
 * keeps the shield on his back); callers set them around a draw. */
typedef struct {
    const FighterModel *model; int textures[FIGHTER_MAX_TEXTURES]; uint32_t part_set,part_clear;
    uint32_t texture_params[FIGHTER_MAX_TEXTURES];
    uint32_t *lists[FIGHTER_LIST_TINTS][FIGHTER_BUCKETS]; unsigned tints[FIGHTER_LIST_TINTS],tint_count; int loaded;
} FighterRender;
typedef struct { unsigned clip,tick,weight; } FighterBlend; /* weight 0..256 toward current */
/* Upload textures and build display lists for the given tints (others draw
 * through the slower immediate path). unload frees both. */
int fighter_render_load(FighterRender *render,const FighterModel *model,const unsigned *tints,unsigned tint_count);
void fighter_render_unload(FighterRender *render);
/* FIGHTER_DRAW_ROOT_MOTION also draws TransN movement (clip root 2) that
 * gameplay applies to the fighter position; the viewer uses it. */
#define FIGHTER_DRAW_ROOT_MOTION 1
void fighter_render_draw(const FighterRender *render,unsigned clip,unsigned age,unsigned tint,
                         const FighterBlend *from,unsigned flags);
/* One model part on its own (e.g. Link's thrown boomerang): its display-list
 * local vertices, centred on their average, under the current matrix. */
void fighter_render_draw_part(const FighterRender *render,unsigned part,unsigned tint);
