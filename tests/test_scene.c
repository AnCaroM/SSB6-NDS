#include "motion_scene.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef NDEBUG
#error Scene tests require assertions enabled
#endif

static float jump_peak(int short_hop)
{
    MotionScene s;
    scene_init(&s);
    scene_step(&s, (SceneInput){0, 1});
    assert(s.state == SCENE_KNEE && s.y == 0.0f);
    scene_step(&s, (SceneInput){0, !short_hop});
    assert(s.state == SCENE_KNEE && s.y == 0.0f);
    scene_step(&s, (SceneInput){0, !short_hop});
    assert(s.state == SCENE_KNEE && s.y == 0.0f);
    scene_step(&s, (SceneInput){0, !short_hop});
    assert(s.state == SCENE_AIR && s.y > 0.0f);
    float peak = s.y;
    for (int t = 0; t < 240; ++t) {
        scene_step(&s, (SceneInput){0, !short_hop});
        if (s.y > peak) peak = s.y;
        assert(s.y >= 0.0f); /* Flat floor catches the vertical jump. */
    }
    assert(s.state == SCENE_WAIT && s.landings == 1 && s.respawns == 0);
    assert(s.vy == 0.0f && s.y == 0.0f);
    /* A continuously held button did not retrigger on landing. */
    return peak;
}

int main(void)
{
    assert(jump_peak(1) < jump_peak(0));
    MotionScene s;
    scene_init(&s);
    for (int t = 0; t < 120; ++t) scene_step(&s, (SceneInput){0, 0});
    assert(s.tick == 120 && s.state == SCENE_WAIT && s.y == 0.0f && s.x == 0.0f);
    for (int t = 0; t < 5; ++t) scene_step(&s, (SceneInput){80, 0});
    assert(s.state == SCENE_WALK && s.x > 0.0f && s.y == 0.0f);
    for (int t = 0; t < 10; ++t) scene_step(&s, (SceneInput){0, 0});
    assert(s.state == SCENE_WAIT && s.vx == 0.0f);

    /* Walking over a ledge must fall, not be snapped back onto the floor. */
    scene_init(&s);
    for (int t = 0; t < 45; ++t) scene_step(&s, (SceneInput){80, 0});
    assert(s.state == SCENE_AIR && s.y < 0.0f);
    uint32_t previous_tick = s.tick;
    for (int t = 0; t < 120; ++t) scene_step(&s, (SceneInput){0, 0});
    assert(s.respawns == 1 && s.state == SCENE_WAIT && s.y == 0.0f);
    assert(s.tick == previous_tick + 120);

    /* Deterministic replay, including KO/reset and press/release history. */
    MotionScene replay, clamped;
    scene_init(&s);
    scene_init(&replay);
    for (int t = 0; t < 1200; ++t) {
        SceneInput input = {((t / 120) % 3 - 1) * 80, t % 200 < 20};
        scene_step(&s, input);
        scene_step(&replay, input);
        assert(memcmp(&s, &replay, sizeof s) == 0);
        assert(isfinite(s.x) && isfinite(s.y) && isfinite(s.vx) && isfinite(s.vy));
        assert(s.tick == (uint32_t)t + 1);
    }
    scene_init(&s);
    scene_init(&clamped);
    for (int t = 0; t < 90; ++t) {
        scene_step(&s, (SceneInput){500, t < 10});
        scene_step(&clamped, (SceneInput){80, t < 10});
        assert(memcmp(&s, &clamped, sizeof s) == 0);
    }
    puts("PASS: jump timing, short hop, held input, floor, ledge, respawn, replay and input clamp");
    return 0;
}
