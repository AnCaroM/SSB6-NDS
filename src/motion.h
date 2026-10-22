#ifndef SSB_MOTION_H
#define SSB_MOTION_H

/* Research subset; units are those of the N64 game, not screen pixels. */
float motion_ground_clamp(float speed, float limit);
float motion_ground_friction(float speed, float friction);
float motion_walk_speed(float speed, int stick_x, float multiplier, float friction);
float motion_gravity(float speed_y, float gravity, float terminal_speed);
float motion_air_accel(float speed, int stick_x, int deadzone, float accel, float limit);
float motion_air_friction(float speed, float friction);
int motion_button_jump_y(int stick_x, int short_hop);

#endif
