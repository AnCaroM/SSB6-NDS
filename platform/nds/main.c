#include <nds.h>
#include <stdio.h>
#include "motion_scene.h"

static void rect(int x, int y, int width, int height, unsigned short color)
{
    for (int row = y; row < y + height; ++row)
        if (row >= 0 && row < 192)
            for (int col = x; col < x + width; ++col)
                if (col >= 0 && col < 256) VRAM_A[row * 256 + col] = color;
}

int main(void)
{
    consoleDemoInit();
    lcdMainOnTop();
    videoSetMode(MODE_FB0);
    vramSetBankA(VRAM_A_LCD);
    rect(0, 0, 256, 192, RGB15(2, 3, 6));
    iprintf("SSB NDS - Motion research\n");
    iprintf("D-pad: move   A: jump\n");
    iprintf("B held: slow stick\n");
    iprintf("START: pause  SELECT: step\n");
    iprintf("Y: reset\n\n");
    iprintf("Provisional scene\nNo original models/animations\n");

    MotionScene scene;
    scene_init(&scene);
    int paused = 0, old_x = 124, old_y = 144;
    while (pmMainLoop()) {
        swiWaitForVBlank();
        scanKeys();
        unsigned held = keysHeld(), down = keysDown();
        if (down & KEY_START) paused = !paused;
        if (down & KEY_Y) scene_init(&scene);
        int stick = (held & KEY_B) ? 24 : 80;
        SceneInput input = {
            ((held & KEY_RIGHT) ? stick : 0) - ((held & KEY_LEFT) ? stick : 0),
            !!(held & KEY_A)
        };
        if (!paused || (down & KEY_SELECT)) scene_step(&scene, input);
        int x = 124 + (int)(scene.x * 0.08f);
        int y = 144 - (int)(scene.y * 0.08f);
        rect(old_x, old_y, 8, 16, RGB15(2, 3, 6));
        rect(56, 160, 145, 3, RGB15(8, 16, 12));
        rect(x, y, 8, 16, RGB15(31, 8, 5));
        old_x = x;
        old_y = y;
        iprintf("\x1b[10;0HTick: %lu  %s       \n", (unsigned long)scene.tick, paused ? "PAUSE" : "RUN");
        iprintf("State: %-6s age:%3u\n", scene_state_name(scene.state), scene.state_ticks);
        iprintf("x:%7d y:%7d      \n", (int)scene.x, (int)scene.y);
        iprintf("vx:%6d vy:%6d     \n", (int)(scene.vx * 100), (int)(scene.vy * 100));
        iprintf("Short:%d Land:%u KO:%u    \n", scene.short_hop, scene.landings, scene.respawns);
    }
    return 0;
}
