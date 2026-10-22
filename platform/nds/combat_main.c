/* Two instances share one mesh, texture set and offline-sampled pose bank. */
#include <nds.h>
#include <stdio.h>
#include "combat.h"
#include "mario_mesh.h"
#include "mario_render.h"
#include "stage_render.h"
#include "recovery_demo.h"
#ifdef ORIGINAL_DREAMLAND
#include "dreamland_render.h"
#include "camera.h"
#define WORLD_UNIT 1024.0f
#define CURRENT_STAGE STAGE_DREAMLAND
#else
#define WORLD_UNIT 400.0f
#define CURRENT_STAGE STAGE_GARDEN
#endif

#ifndef SCENE_AUTOPLAY
#define SCENE_AUTOPLAY 0
#endif

static unsigned clip_for(const MotionScene *s)
{
    if(s->state==SCENE_SPECIAL_HI)return 10;
    if(s->state==SCENE_SPECIAL_FALL)return 11;
    if(s->state==SCENE_CLIFF)return s->state_ticks<SCENE_CATCH_TICKS?9:6;
    if (s->state == SCENE_WALK) return 1;
    if (s->state == SCENE_KNEE) return 2;
    if(s->state==SCENE_AIR && s->air_jump) {
        unsigned clip=s->air_jump==1?7:8;
        return s->air_jump_ticks<mario_clips[clip].ticks?clip:3;
    }
    if (s->state == SCENE_AIR) return s->vy > 0 ? 2 : 3;
    return s->state == SCENE_LAND ? 4 : 0;
}

static void actor(const MotionScene *s, int facing, unsigned clip, unsigned age, unsigned tint)
{
    glPushMatrix();
    float x=s->x,y=s->y;
    if(clip==7 || clip==8)age=s->air_jump_ticks;
    if(clip==9)age=s->state_ticks;
    if(clip==10 || clip==11)age=s->state_ticks;
    if(s->state==SCENE_CLIFF) {x=s->cliff_side*2318;y=0;facing=-s->cliff_side;}
    glTranslatef32(floattof32(x/WORLD_UNIT),floattof32((y+MARIO_CENTER_Y)/WORLD_UNIT),0);
    if(s->state==SCENE_SPECIAL_HI)glRotateZi(s->special_tilt*32768/360);
    glRotateYi(facing > 0 ? 8192 : -8192);
    int size = floattof32(MARIO_RESTORE_SCALE/WORLD_UNIT);
    glScalef32(size,size,size);
    mario_render_draw_pose_tint(clip,age,tint);
    glPopMatrix(1);
}

int main(void)
{
    consoleDemoInit(); lcdMainOnTop(); videoSetMode(MODE_0_3D); glInit();
    if (!mario_render_init()) { iprintf("Error de texturas\n"); while(pmMainLoop()) swiWaitForVBlank(); return 1; }
#ifdef ORIGINAL_DREAMLAND
    if(!dreamland_render_init()) { iprintf("Error al cargar Dream Land\n"); while(pmMainLoop())swiWaitForVBlank();return 1; }
    int map_view=0;
    FightCamera camera; camera_reset(&camera);
#endif
    glEnable(GL_ANTIALIAS); glClearColor(14,24,30,31); glClearPolyID(63);
    glClearDepth(0x7fff); glViewport(0,0,255,191);
    CombatScene battle; combat_init_stage(&battle,CURRENT_STAGE);
    unsigned clip=0, age=0, target_clip=0, target_age=0, phase=0,demo_index=0;
    int automatic=SCENE_AUTOPLAY, paused=0;
#ifndef ORIGINAL_DREAMLAND
    float camera_top=1550;
#endif
    u64 previous_start=0;
    unsigned work_us=0, peak_us=0, period_us=0;
#ifdef ORIGINAL_DREAMLAND
    iprintf("SSB NDS - Prueba 09\nMario: animaciones revisadas\nR: automatica/mapa\n");
#else
    iprintf("SSB NDS - Prueba 02\nMario vs blanco de prueba\n\n");
#endif
    iprintf("A salto / doble salto\nX golpe + direccion\nY+X smash   L agarre\nB fuego / arriba+B especial\nAbajo+B Tornado\nSTART pausa SELECT demo\nSTART+SELECT reiniciar\nColisiones simplificadas\n");
    while(pmMainLoop()) {
        u64 frame_start=tickGetCount();
        if(previous_start) period_us=(unsigned)((frame_start-previous_start)*1000000/TICK_FREQ);
        previous_start=frame_start;
        scanKeys(); unsigned held=keysHeld(), down=keysDown();
        int reset=(held&KEY_START) && (held&KEY_SELECT) && (down&(KEY_START|KEY_SELECT));
        if(!reset && (down & KEY_START)) paused=!paused;
        if(!reset && (down & KEY_SELECT)) { automatic=!automatic; combat_init_stage(&battle,CURRENT_STAGE); phase=demo_index=0; age=target_age=0; }
        if(reset) { combat_init_stage(&battle,CURRENT_STAGE); phase=demo_index=0;age=target_age=0;paused=0; }
#ifdef ORIGINAL_DREAMLAND
        if(down & KEY_R)map_view=!map_view;
        if(reset || (down & KEY_SELECT))camera_reset(&camera);
#endif
        if(held & (KEY_LEFT|KEY_RIGHT|KEY_UP|KEY_DOWN|KEY_A|KEY_B|KEY_X|KEY_Y|KEY_L)) automatic=0;
        int stick=80;
        SceneInput input={((held & KEY_RIGHT)?stick:0)-((held & KEY_LEFT)?stick:0),!!(held & KEY_A)};
        int attack=!!(held & KEY_X);
        int drop=!!(down & KEY_DOWN);
        CombatInput command={0};command.motion=input;command.attack=attack;
        command.special=!!(held&KEY_B);command.grab=!!(held&KEY_L);
        command.smash=!!(held&KEY_Y);command.variant=command.grab;
        command.up=!!(held&KEY_UP);command.down=!!(held&KEY_DOWN);
        command.drop=drop && !command.attack && !command.special;
        if(!paused) {
            if(automatic) {
#ifdef ORIGINAL_DREAMLAND
                if(!phase)combat_demo_begin(&battle,mario_move_demo_order[demo_index++%MOVE_COUNT]);
                command=(CombatInput){0};
                if(battle.captured && battle.move==MOVE_GRAB_HOLD && phase==45)command.attack=1;
                if(battle.attack_frame>=0 && (battle.move==MOVE_TORNADO || battle.move==MOVE_TORNADO_AIR))command.special=phase%6==0;
                phase=(phase+1)%240;
#else
                if(!phase) combat_init_stage(&battle,CURRENT_STAGE);
                input=(SceneInput){0,0}; attack=phase==10;
                command=(CombatInput){0};command.motion=input;command.attack=attack;
                phase=(phase+1)%150;
#endif
            }
            unsigned old_tick=battle.player.tick;
            combat_update(&battle,command);
            if(battle.player.tick != old_tick) {
                unsigned next=battle.attack_frame >= 0 ? combat_player_clip(&battle) : clip_for(&battle.player);
                if(battle.attack_frame<0 && battle.dash_ticks)
                    next=battle.run_age<mario_clips[mario_dash_clip].ticks?mario_dash_clip:mario_run_clip;
                else if(battle.attack_frame<0 && battle.brake_ticks)next=mario_brake_clip;
                age=next==clip ? age+1 : 0; clip=next;
                next=battle.captured?mario_capture_clip:clip_for(&battle.target);
                target_age=next==target_clip ? target_age+1 : 0; target_clip=next;
            }
        }
#ifdef ORIGINAL_DREAMLAND
        CameraFighter interests[]={
            {(int32_t)(battle.player.x*16),(int32_t)(battle.player.y*16),battle.facing},
            {(int32_t)(battle.target.x*16),(int32_t)(battle.target.y*16),-1}
        };
        if(!paused || !camera.ready)camera_step(&camera,interests,2);
        float view_height=(map_view?5400.0f:camera.height/16.0f)/WORLD_UNIT;
        float half_width=view_height*(2.0f/3.0f);
        float center=map_view?0:camera.x/16.0f;
        float center_y=map_view?1400:camera.y/16.0f;
        float bottom=-view_height*.5f;
#else
        float highest=battle.player.y > battle.target.y ? battle.player.y : battle.target.y;
        float target_top=highest+500;
        if(target_top<1550)target_top=1550;
        if(target_top>camera_top)camera_top=target_top;
        else camera_top+=(target_top-camera_top)*.08f;
        float center=(battle.player.x+battle.target.x)*.5f;
        if(center>600)center=600;
        if(center<-600)center=-600;
        float bottom=-350;
        float half_width=(camera_top-bottom)*(2.0f/3.0f)/WORLD_UNIT;
        float spread=battle.player.x-battle.target.x;
        if(spread<0)spread=-spread;
        float need=(spread*.5f+450)/WORLD_UNIT;
        if(need>half_width)half_width=need;
        /* Expand both axes by the same factor to preserve pixel proportions. */
        float view_height=half_width*1.5f;
#endif
        glMatrixMode(GL_PROJECTION);glLoadIdentity();
#ifdef ORIGINAL_DREAMLAND
        glOrtho(-half_width,half_width,bottom,view_height+bottom,.1,20);
#else
        glOrtho(-half_width,half_width,bottom/WORLD_UNIT,view_height+bottom/WORLD_UNIT,.1,20);
#endif
        glMatrixMode(GL_MODELVIEW);glLoadIdentity();
#ifdef ORIGINAL_DREAMLAND
        dreamland_render_background(-half_width,half_width,bottom,view_height+bottom);
        glTranslatef32(floattof32(-center/WORLD_UNIT),floattof32(-center_y/WORLD_UNIT),floattof32(-6));
#else
        glTranslatef32(floattof32(-center/WORLD_UNIT),0,floattof32(-6));
#endif
#ifdef ORIGINAL_DREAMLAND
        dreamland_render_draw();
#else
        stage_render_draw();
#endif
        actor(&battle.player,battle.facing,clip,battle.attack_frame>=0?(unsigned)battle.attack_frame:age,RGB15(31,31,31));
        actor(&battle.target,battle.captured?-battle.attack_facing:-1,target_clip,target_age,battle.hitstop?RGB15(31,12,12):RGB15(12,24,31));
        for(unsigned i=0;i<4;++i)if(battle.fireballs[i].life)
            mario_render_fireball(battle.fireballs[i].x,battle.fireballs[i].y,WORLD_UNIT,140-battle.fireballs[i].life);
        glFlush(0);
        if((battle.tick & 7)==0 || paused) {
            iprintf("\x1b[12;0H%s %s                     \n",paused?"PAUSA":"RUN",automatic?"AUTO":"MANUAL");
            iprintf("Blanco:%u%% Hits:%u KO:%u     \n",battle.percent,battle.hits,battle.knockouts);
            iprintf("%-16s F:%d        \n",battle.attack_frame>=0?mario_moves[battle.move].name:"Libre",battle.attack_frame);
            iprintf("%s J:%u/2 A:%u B:%u       \n",scene_state_name(battle.player.state),battle.player.jumps_used,battle.player.air_jumps,battle.player.cliff_grabs);
#ifdef ORIGINAL_DREAMLAND
            iprintf("Cam:%s Alto:%d        \n",map_view?"MAPA":"AUTO",(int)(view_height*WORLD_UNIT));
#else
            iprintf("Modelo/poses compartidos\n");
#endif
            iprintf("Trabajo:%uus Pico:%u       \n",work_us,peak_us);
            iprintf("Periodo:%uus               \n",period_us);
        }
        work_us=(unsigned)((tickGetCount()-frame_start)*1000000/TICK_FREQ);
        if(work_us>peak_us)peak_us=work_us;
        swiWaitForVBlank();
    }
    return 0;
}
