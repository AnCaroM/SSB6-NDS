/* FTAttributes (US) from ssb-decomp-re 572a747d8171137718fad472c5d7473dc7309ffd:
 * relocData/203_MarioMain.c dMarioMain_attr, 225_LinkMain.c dLinkMain_attr,
 * 229_KirbyMain.c dKirbyMain_attr.
 * tests/test_fighter_attr.py compares these values with the reference. */
#include "fighter.h"

const FighterAttributes fighter_attr_mario={
    .walk_speed_mul=.3f,.traction=1.5f,.run_speed=44,.jump_vel_x=.35f,.jump_height_mul=.7f,
    .jump_height_base=26,.jumpaerial_vel_x=.35f,.jumpaerial_height=.9f,.air_accel=.025f,
    .air_speed_max_x=30,.air_friction=.2f,.gravity=2.4f,.tvel_base=44,.weight=1,.kneebend_ticks=3,.jumps_max=2,
    .dash_velocity=50,.fall_special_speed=18,.shield_size=260,.shield_break_vel_y=70};

const FighterAttributes fighter_attr_link={
    .walk_speed_mul=.37f,.traction=2,.run_speed=42,.jump_vel_x=.35f,.jump_height_mul=.7f,
    .jump_height_base=36,.jumpaerial_vel_x=.35f,.jumpaerial_height=.9f,.air_accel=.04f,
    .air_speed_max_x=31,.air_friction=0,.gravity=3.4f,.tvel_base=64,.weight=.96f,.kneebend_ticks=7,.jumps_max=2,
    /* Mario's local dash value scaled by the run-speed ratio 42/44. */
    .dash_velocity=48,.fall_special_speed=18.6f, /* 31 x FALLSPECIAL_DRIFT .6 */
    .shield_size=280,.shield_break_vel_y=90};

const FighterAttributes fighter_attr_kirby={
    .walk_speed_mul=.35f,.traction=1.4f,.run_speed=48,.jump_vel_x=.45f,.jump_height_mul=.6f,
    .jump_height_base=30,.jumpaerial_vel_x=.45f,.jumpaerial_height=.8f,.air_accel=.04f,
    .air_speed_max_x=28,.air_friction=.5f,.gravity=2.4f,.tvel_base=48,.weight=1.19f,.kneebend_ticks=3,.jumps_max=6,
    /* Mario's local dash value scaled by the run-speed ratio 48/44. */
    .dash_velocity=54.5f,.fall_special_speed=16.8f, /* 28 x FALLSPECIAL_DRIFT .6 */
    .shield_size=270,.shield_break_vel_y=90,
    .multi_jump_vel={60,52,47,40}}; /* ftcommonjumpaerial.c dFTKirbyJumpAerialFVelocities */
