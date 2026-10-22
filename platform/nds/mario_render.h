#pragma once
void mario_render_fireball(float x,float y,float unit,unsigned age);
/* Call after glInit, before drawing. Textures occupy VRAM bank A. */
int mario_render_init(void);
void mario_render_draw(void);
void mario_render_draw_pose(unsigned clip, unsigned frame);
void mario_render_draw_pose_tint(unsigned clip, unsigned frame, unsigned tint);
