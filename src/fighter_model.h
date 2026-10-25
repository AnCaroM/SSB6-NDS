#ifndef SSB_FIGHTER_MODEL_H
#define SSB_FIGHTER_MODEL_H
#include <stdint.h>
/* Generic converted fighter for the bone renderer (tools/convert_fighter.py).
 * Draw vertices index joint-local positions; clips store per-tick joint
 * channels (see skeleton_sample.h). Data is generated from the user ROM. */
typedef struct { uint16_t color; int16_t u,v; } FighterVertex;
typedef struct { const uint16_t *pixels; uint16_t width,height; uint32_t param; } FighterTexture;
typedef struct { int16_t x,y,z,slot; } FighterLocalVertex; /* slot: matrix-stack slot of its joint */
/* parts: (tick, visible-part mask) pairs from the motion script's model-part
 * commands; NULL keeps the model's base mask. root: 0 no inserted joint,
 * 1 drawn TransN/XRotN, 2 TransN the game applies as movement. */
typedef struct { const int16_t *frames; const uint16_t *channels; const int16_t *base; const uint32_t *parts;
                 uint16_t ticks,stored,varying,loop,part_changes,root; } FighterClip;
/* Joint evaluation order: restore = matrix-stack slot of the parent (-1: keep
 * the current matrix, i.e. parent was evaluated just before), store = slot to
 * keep this joint for children/vertices (-1: not needed), skip = no geometry
 * and no children. Slots are absolute; slot 0 is left to the caller's push. */
typedef struct { int8_t restore,store,skip,pad; } FighterJoint;
typedef struct {
    const char *name;
    const FighterVertex *vertices; unsigned vertex_count;
    const int16_t *triangle_textures;
    const FighterTexture *textures; unsigned texture_count;
    const uint16_t *local_map; const FighterLocalVertex *locals;
    const FighterClip *clips; unsigned clip_count;
    const FighterJoint *joints; const int8_t *parents; unsigned joint_count,slot_count;
    int32_t scale,offset[3]; /* 4.12: skeleton units (model/divisor) -> GL units (1/1024 world) */
    const uint8_t *triangle_parts; uint32_t base_parts; /* part bit per triangle, 255 = always */
    int32_t translate; /* channel translation (model units*8) -> f32 skeleton units */
} FighterModel;
#endif
