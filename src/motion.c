/* Adapted for local research from ssb-decomp-re, revision
 * 572a747d8171137718fad472c5d7473dc7309ffd.
 * See docs/motion-lab.md for function provenance and limits.
 */
#include "motion.h"
#include <math.h>
#include <stdlib.h>

float motion_ground_clamp(float speed, float limit)
{
    if (speed < -limit) return -limit;
    if (speed > limit) return limit;
    return speed;
}

float motion_ground_friction(float speed, float friction)
{
    if (speed < 0.0f) {
        speed += friction;
        if (speed > 0.0f) speed = 0.0f;
    } else {
        speed -= friction;
        if (speed < 0.0f) speed = 0.0f;
    }
    return speed;
}

float motion_walk_speed(float speed, int stick_x, float multiplier, float friction)
{
    float target = abs(stick_x) * multiplier;
    if (speed < target) speed = target;
    else {
        speed -= friction;
        if (speed < target) speed = target;
    }
    return speed;
}

float motion_gravity(float speed_y, float gravity, float terminal_speed)
{
    speed_y -= gravity;
    if (speed_y < -terminal_speed) speed_y = -terminal_speed;
    return speed_y;
}

float motion_air_accel(float speed, int stick_x, int deadzone, float accel, float limit)
{
    if (abs(stick_x) >= deadzone) {
        speed += stick_x * accel;
        if (speed < -limit) speed = -limit;
        else if (speed > limit) speed = limit;
    }
    return speed;
}

float motion_air_friction(float speed, float friction)
{
    if (speed < 0.0f) {
        speed += friction;
        if (speed >= 0.0f) speed = 0.0f;
    } else {
        speed -= friction;
        if (speed <= 0.0f) speed = 0.0f;
    }
    return speed;
}

int motion_button_jump_y(int stick_x, int short_hop)
{
    float x = (float)abs(stick_x);
    float unit = x / 80.0f;
    float root = sqrtf(1.0f - unit * unit);
    float minimum = short_hop ? 36.0f : 63.0f;
    float y = (short_hop ? 9.0f : 17.0f) * root + minimum;
    if (x * x + y * y > 80.0f * 80.0f)
        y = sqrtf(80.0f * 80.0f - x * x);
    if (y < minimum) y = minimum;
    if (y > 77.0f) y = 77.0f;
    return (int)y;
}
