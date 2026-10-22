#include "motion.h"
#include "reference.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static void equal(float actual, float expected, const char *name)
{
    ++checks;
    /* Signed zero is compared too; no tolerance hides arithmetic changes. */
    if (memcmp(&actual, &expected, sizeof(float))) {
        fprintf(stderr, "%s mismatch: %.9g / %.9g\n", name, actual, expected);
        exit(1);
    }
}

int main(void)
{
    const float speeds[] = {-100.0f, -44.0f, -30.0f, -1.5f, -0.2f,
                            -0.0f, 0.0f, 0.2f, 1.5f, 30.0f, 44.0f, 100.0f};
    const float frictions[] = {0.0f, 0.2f, 1.5f, 6.0f};
    for (unsigned i = 0; i < sizeof speeds / sizeof *speeds; ++i) {
        float v = speeds[i];
        FTStruct fp = {0};
        fp.physics.vel_ground.x = v;
        ftPhysicsClampGroundVel(&fp, 30.0f);
        equal(motion_ground_clamp(v, 30.0f), fp.physics.vel_ground.x, "ground clamp");
        fp.physics.vel_air.y = v;
        ftPhysicsApplyGravityClampTVel(&fp, 2.4f, 44.0f);
        equal(motion_gravity(v, 2.4f, 44.0f), fp.physics.vel_air.y, "gravity");
        for (unsigned f = 0; f < sizeof frictions / sizeof *frictions; ++f) {
            float friction = frictions[f];
            fp.physics.vel_ground.x = v;
            ftPhysicsSetGroundVelFriction(&fp, friction);
            equal(motion_ground_friction(v, friction), fp.physics.vel_ground.x, "ground friction");
            FTAttributes attr = {.air_friction = friction};
            fp.physics.vel_air.x = v;
            ftPhysicsApplyAirVelXFriction(&fp, &attr);
            equal(motion_air_friction(v, friction), fp.physics.vel_air.x, "air friction");
            for (int stick = -80; stick <= 80; ++stick) {
                fp.input.pl.stick_range.x = stick;
                fp.physics.vel_ground.x = v;
                ftPhysicsSetGroundVelAbsStickRange(&fp, 0.3f, friction);
                equal(motion_walk_speed(v, stick, 0.3f, friction), fp.physics.vel_ground.x, "walk speed");
                fp.physics.vel_air.x = v;
                ftPhysicsClampAirVelXStickRange(&fp, 8, 0.025f, 30.0f);
                equal(motion_air_accel(v, stick, 8, 0.025f, 30.0f), fp.physics.vel_air.x, "air accel");
            }
        }
    }
    for (int stick = -80; stick <= 80; ++stick) {
        for (int short_hop = 0; short_hop <= 1; ++short_hop) {
            int x, y;
            ftCommonJumpGetJumpForceButton(stick, &x, &y, short_hop);
            equal((float)motion_button_jump_y(stick, short_hop), (float)y, "jump force");
        }
    }
    /* Multi-tick check catches accumulation differences at terminal velocity. */
    FTStruct fp = {0};
    float vy = 79.9f;
    fp.physics.vel_air.y = vy;
    for (int tick = 0; tick < 600; ++tick) {
        ftPhysicsApplyGravityClampTVel(&fp, 2.4f, 44.0f);
        vy = motion_gravity(vy, 2.4f, 44.0f);
        equal(vy, fp.physics.vel_air.y, "gravity sequence");
    }
    printf("PASS: %u comparisons against upstream C functions\n", checks);
    return 0;
}
