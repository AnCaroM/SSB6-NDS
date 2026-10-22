#include "motion.h"
#include <stdio.h>

/* Isolated numeric experiment, not the fighter's complete update pipeline.
 * Launch impulse uses Mario's attributes. Ground, states and collision are
 * deliberately absent: the CSV shows only repeated vertical integration.
 */
int main(void)
{
    puts("tick,long_y,long_vy,short_y,short_vy");
    float long_y = 0.0f, short_y = 0.0f;
    float long_vy = motion_button_jump_y(0, 0) * 0.7f + 26.0f;
    float short_vy = motion_button_jump_y(0, 1) * 0.7f + 26.0f;
    for (int tick = 0; tick <= 90; ++tick) {
        printf("%d,%.6f,%.6f,%.6f,%.6f\n", tick, long_y, long_vy, short_y, short_vy);
        long_vy = motion_gravity(long_vy, 2.4f, 44.0f);
        short_vy = motion_gravity(short_vy, 2.4f, 44.0f);
        long_y += long_vy;
        short_y += short_vy;
    }
    return 0;
}
