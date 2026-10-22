/* Integration scene: ROM model and sampled clips + adapted movement helpers.
 * Reduced state scheduler, floor and animation selection are provisional. */
#include <nds.h>
#include <stdio.h>
#include "motion_scene.h"
#include "mario_mesh.h"
#include "mario_render.h"
#include "stage_render.h"

#ifndef SCENE_AUTOPLAY
#define SCENE_AUTOPLAY 0
#endif

static const char *clip_names[] = {"Reposo", "Caminar", "Salto", "Caida", "Aterrizar"};

static unsigned select_clip(const MotionScene *scene)
{
    if (scene->state == SCENE_WALK) return 1;
    if (scene->state == SCENE_KNEE) return 2;
    if (scene->state == SCENE_AIR) return scene->vy > 0 ? 2 : 3;
    if (scene->state == SCENE_LAND) return 4;
    return 0;
}

int main(void)
{
    consoleDemoInit();
    lcdMainOnTop();
    videoSetMode(MODE_0_3D);
    glInit();
    if (!mario_render_init()) { iprintf("Error de texturas\n"); while (pmMainLoop()) swiWaitForVBlank(); return 1; }
    glEnable(GL_ANTIALIAS);
    glClearColor(14, 24, 30, 31);
    glClearPolyID(63);
    glClearDepth(0x7fff);
    glViewport(0, 0, 255, 191);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    MotionScene scene;
    scene_init_stage(&scene, STAGE_GARDEN);
    unsigned clip = 0, age = 0, phase = 0, seen = 0, total_landings = 0;
    int facing = 1, paused = 0, automatic = SCENE_AUTOPLAY;
    float camera_top = 1550;
    iprintf("SSB NDS - Prueba 01\nJardin de entrenamiento\n\n");
    iprintf("Cruceta: mover\nA: saltar  B: andar lento\nAbajo: bajar plataforma\nY: reiniciar  START: pausa\nSELECT: automatico/manual\n\n");
    iprintf("Mario: recursos originales\nMapa y reglas provisionales\n");
    while (pmMainLoop()) {
        scanKeys();
        unsigned held = keysHeld(), down = keysDown();
        if (down & KEY_START) paused = !paused;
        if (down & KEY_SELECT) { automatic = !automatic; phase = 0; scene_init_stage(&scene, STAGE_GARDEN); }
        if (down & KEY_Y) { scene_init_stage(&scene, STAGE_GARDEN); phase = age = 0; }
        if (held & (KEY_LEFT | KEY_RIGHT | KEY_A | KEY_DOWN)) automatic = 0;
        int stick = held & KEY_B ? 24 : 80;
        SceneInput input = {((held & KEY_RIGHT) ? stick : 0) - ((held & KEY_LEFT) ? stick : 0), !!(held & KEY_A)};
        if (!paused) {
            if (automatic) {
                if (phase == 0) scene_init_stage(&scene, STAGE_GARDEN);
                /* Keep the jump vertical so the test catches the upper surface;
                 * then drop through it and exercise walking on the island. */
                input.stick_x = phase < 20 ? 24 : (phase >= 130 && phase < 155 ? 24 :
                               (phase >= 155 && phase < 180 ? -24 : 0));
                input.jump_held = phase >= 30 && phase < 40;
                if (phase == 90) scene_drop_through(&scene);
                phase = (phase + 1) % 200;
            }
            unsigned before = scene.landings;
            if (down & KEY_DOWN) scene_drop_through(&scene);
            scene_step(&scene, input);
            if (scene.landings > before) total_landings += scene.landings-before;
            if (input.stick_x) facing = input.stick_x > 0 ? 1 : -1;
            unsigned next = select_clip(&scene);
            age = next == clip ? age+1 : 0;
            clip = next;
            seen |= 1u << scene.state;
        }
        /* Zoom out on high jumps. In particular a jump from the upper platform
         * must keep Mario's head inside the 192-pixel display. */
        float target_top = scene.y + 500;
        if (target_top < 1550) target_top = 1550;
        if (target_top > camera_top) camera_top = target_top;
        else camera_top += (target_top-camera_top)*.08f;
        float half_width = (camera_top+350)*(2.0f/3.0f)/400;
        float center_x = scene.x > 650 ? (scene.x-650)*.4f :
                         (scene.x < -650 ? (scene.x+650)*.4f : 0);
        if (center_x > 600) center_x = 600;
        if (center_x < -600) center_x = -600;
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-half_width, half_width, -.875, camera_top/400, .1, 20);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef32(floattof32(-center_x/400), 0, floattof32(-6));
        stage_render_draw();
        glPushMatrix();
        glTranslatef32(floattof32(scene.x/400), floattof32((scene.y+MARIO_CENTER_Y)/400), 0);
        glRotateYi(facing > 0 ? 8192 : -8192);
        const int size = floattof32(MARIO_RESTORE_SCALE/400);
        glScalef32(size, size, size);
        mario_render_draw_pose(clip, age);
        glPopMatrix(1);
        glFlush(0);
        swiWaitForVBlank();
        if ((scene.tick & 7) == 0 || paused) {
            iprintf("\x1b[13;0HTick:%lu %s %s       \n", (unsigned long)scene.tick, paused ? "PAUSA" : "RUN", automatic ? "AUTO" : "MANUAL");
            iprintf("Estado:%-5s x:%d y:%d     \n", scene_state_name(scene.state), (int)scene.x, (int)scene.y);
            iprintf("Anim:%-9s f:%u        \n", clip_names[clip], age % mario_clips[clip].frames);
            iprintf("Suelo:%d Caidas:%u            \n", scene.platform, scene.respawns);
            iprintf("Aterrizajes:%u Vistos:%02x    \n", total_landings, seen);
        }
    }
    return 0;
}
