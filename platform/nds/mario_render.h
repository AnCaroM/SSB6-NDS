#pragma once
void mario_render_fireball(float x,float y,float unit,unsigned age);
/* Call after glInit, before drawing. Textures occupy VRAM bank A. */
int mario_render_init(void);
void mario_render_draw(void);
void mario_render_draw_pose(unsigned clip, unsigned frame);
void mario_render_draw_pose_tint(unsigned clip, unsigned frame, unsigned tint);
typedef struct {unsigned clip,tick,weight;} MarioPoseBlend;
/* Optional short visual recovery blend, weight0..256 toward current pose. */
void mario_render_draw_pose_mix(unsigned clip,unsigned frame,unsigned tint,const MarioPoseBlend *from);
