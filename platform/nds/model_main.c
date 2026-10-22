/* ROM-derived mesh and sampled locomotion clip inspection. */
#include <nds.h>
#include <stdio.h>
#include "mario_mesh.h"
#include "mario_render.h"

int main(void)
{
    consoleDemoInit();
    lcdMainOnTop();
    videoSetMode(MODE_0_3D);
    glInit();
    if (!mario_render_init()) { iprintf("Error al cargar texturas\n"); while (pmMainLoop()) swiWaitForVBlank(); return 1; }
    glEnable(GL_ANTIALIAS);
    glClearColor(3, 5, 9, 31);
    glClearPolyID(63);
    glClearDepth(0x7fff);
    glViewport(0, 0, 255, 191);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(40, 256.0 / 192.0, 0.1, 20);

    int yaw = 0, pitch = 0;
    int distance = floattof32(-3.5f);
    unsigned frame = 0;
    unsigned clip = 0;
    int touch_x = -1;
    iprintf("SSB64 -> NDS: Mario\n\n");
    iprintf("Modelo y animaciones originales\n");
    iprintf("%u triangulos\n\n", mario_vertex_count / 3);
    iprintf("Izq/Der: girar\nA (X PC): acercar\nB (Z PC): alejar\nY (R PC): restablecer\n\n");
    iprintf("Texturas originales\nEspacio: cambiar animacion\n");
    iprintf("Arrastra aqui: girar\n");
    while (pmMainLoop()) {
        scanKeys();
        unsigned held = keysHeld(), down = keysDown();
        if (down & KEY_SELECT) { clip = (clip+1) % mario_clip_count; frame = 0; }
        if (held & KEY_TOUCH) {
            touchPosition touch;
            touchRead(&touch);
            if (touch_x >= 0) yaw += (touch.px - touch_x) * 90;
            touch_x = touch.px;
        } else touch_x = -1;
        if (held & KEY_LEFT) yaw += 160;
        if (held & KEY_RIGHT) yaw -= 160;
        if (held & KEY_UP) pitch += 100;
        if (held & KEY_DOWN) pitch -= 100;
        if ((held & KEY_A) && distance < -floattof32(2.0f)) distance += 64;
        if ((held & KEY_B) && distance > -floattof32(7.0f)) distance -= 64;
        if (down & KEY_Y) { yaw = 0; pitch = 0; distance = floattof32(-3.5f); }
        yaw %= 32768;
        if (pitch > 4096) pitch = 4096;
        if (pitch < -4096) pitch = -4096;
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef32(0, 0, distance);
        glRotateXi(pitch);
        glRotateYi(yaw);
        mario_render_draw_pose(clip, frame);
        glFlush(0);
        swiWaitForVBlank();
        if ((++frame & 15) == 0)
            iprintf("\x1b[17;0HFrame: %lu Clip:%u\nGiro: %d     ", (unsigned long)frame, clip, yaw);
    }
    return 0;
}
