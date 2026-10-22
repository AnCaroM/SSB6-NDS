/* Two instances share one mesh, texture set and offline-sampled pose bank. */
#include <nds.h>
#include <stdio.h>
#include "versus.h"
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
    consoleDemoInit();lcdMainOnTop();videoSetMode(MODE_0_3D);glInit();
    if(!mario_render_init() || !dreamland_render_init()) {
        iprintf("Error al cargar recursos\n");while(pmMainLoop())swiWaitForVBlank();return 1;
    }
    glEnable(GL_ANTIALIAS);glClearColor(14,24,30,31);glClearPolyID(63);
    glClearDepth(0x7fff);glViewport(0,0,255,191);
    static Versus match;versus_init(&match);
    FightCamera camera;camera_reset(&camera);
    unsigned clips[2]={0},ages[2]={0};int paused=0,watch=SCENE_AUTOPLAY,map_view=0;
    unsigned work_us=0,peak_us=0,sim_us=0,over_budget=0,period_us=0;
    /* Calico reserves timers2/3. Timer0 is a free-running profiling counter;
     * 16-bit wrap is safe for intervals below ~125 ms at bus clock /64. */
    timerStart(0,ClockDivider_64,0,NULL);u16 previous=timerTick(0);
    iprintf("SSB NDS - Prueba 10\nMario vs CPU - 3 vidas\nA salto X golpe Y+X smash\nL agarre / B especial\nArriba+B subir Abajo+B giro\nR camara START pausa\nSELECT CPU vs CPU\nSTART+SELECT reiniciar\n\n");
    while(pmMainLoop()) {
        u16 start=timerTick(0);
        period_us=(unsigned)((u64)(u16)(start-previous)*1000000/(BUS_CLOCK/64));previous=start;
        scanKeys();unsigned held=keysHeld(),down=keysDown();
        int reset=(held&KEY_START) && (held&KEY_SELECT) && (down&(KEY_START|KEY_SELECT));
        if(reset) {versus_init(&match);camera_reset(&camera);paused=0;peak_us=over_budget=0;ages[0]=ages[1]=0;}
        else {
            if(down&KEY_START)paused=!paused;
            if(down&KEY_SELECT) {watch=!watch;versus_init(&match);camera_reset(&camera);ages[0]=ages[1]=0;}
        }
        if(down&KEY_R)map_view=!map_view;
        CombatInput input[2]={0};
        input[0].motion.stick_x=((held&KEY_RIGHT)?80:0)-((held&KEY_LEFT)?80:0);
        input[0].motion.jump_held=!!(held&KEY_A);input[0].attack=!!(held&KEY_X);
        input[0].special=!!(held&KEY_B);input[0].grab=!!(held&KEY_L);
        input[0].smash=!!(held&KEY_Y);input[0].variant=input[0].grab;
        input[0].up=!!(held&KEY_UP);input[0].down=!!(held&KEY_DOWN);
        input[0].drop=!!(down&KEY_DOWN) && !input[0].attack && !input[0].special;
        u16 sim_start=timerTick(0);
        if(!paused) {
            if(watch)input[0]=versus_cpu(&match,0);
            input[1]=versus_cpu(&match,1);
            unsigned before=match.fighter[0].player.tick;
            versus_step(&match,input);
            if(match.fighter[0].player.tick!=before)for(unsigned i=0;i<2;++i) {
                CombatScene *s=&match.fighter[i];unsigned next=clip_for(&s->player);
                if(s->attack_frame>=0)next=combat_player_clip(s);
                else if(s->dash_ticks)next=s->run_age<mario_clips[mario_dash_clip].ticks?mario_dash_clip:mario_run_clip;
                else if(s->brake_ticks)next=mario_brake_clip;
                if(versus_captured(&match,i))next=mario_capture_clip;
                ages[i]=next==clips[i]?ages[i]+1:0;clips[i]=next;
            }
        }
        sim_us=(unsigned)((u64)(u16)(timerTick(0)-sim_start)*1000000/(BUS_CLOCK/64));
        CameraFighter interests[2];
        for(unsigned i=0;i<2;++i) {
            MotionScene *s=&match.fighter[i].player;
            interests[i]=(CameraFighter){(int32_t)(s->x*16),(int32_t)(s->y*16),s->facing};
        }
        if(!paused || !camera.ready)camera_step(&camera,interests,2);
        float height=(map_view?5400.0f:camera.height/16.0f)/WORLD_UNIT;
        float half_width=height*(2.0f/3.0f),bottom=-height*.5f;
        float cx=map_view?0:camera.x/16.0f,cy=map_view?1400:camera.y/16.0f;
        glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(-half_width,half_width,bottom,height+bottom,.1,20);
        glMatrixMode(GL_MODELVIEW);glLoadIdentity();
        dreamland_render_background(-half_width,half_width,bottom,height+bottom);
        glTranslatef32(floattof32(-cx/WORLD_UNIT),floattof32(-cy/WORLD_UNIT),floattof32(-6));
        dreamland_render_draw();
        for(unsigned i=0;i<2;++i) {
            CombatScene *s=&match.fighter[i];unsigned tint=i?RGB15(12,24,31):RGB15(31,31,31);
            if(match.stun[i] || match.freeze)tint=RGB15(31,12,12);
            if(!match.invulnerable[i] || (match.tick&4))
                actor(&s->player,s->player.facing,clips[i],s->attack_frame>=0?(unsigned)s->attack_frame:ages[i],tint);
            for(unsigned j=0;j<4;++j)if(s->fireballs[j].life)
                mario_render_fireball(s->fireballs[j].x,s->fireballs[j].y,WORLD_UNIT,140-s->fireballs[j].life);
        }
        glFlush(0);
        if((match.tick&15)==0 || paused || match.winner>=0) {
            iprintf("\x1b[9;0H%s %s                   \n",paused?"PAUSA":"JUEGO",watch?"CPU vs CPU":"TU vs CPU");
            iprintf("TU : %3u%%  Vidas:%u          \n",match.percent[0],match.stocks[0]);
            iprintf("CPU: %3u%%  Vidas:%u          \n",match.percent[1],match.stocks[1]);
            iprintf("%s                     \n",match.winner<0?"Combate":match.winner==0?"GANASTE":match.winner==1?"GANA CPU":"EMPATE");
            iprintf("Sim:%uus Trabajo:%uus      \n",sim_us,work_us);
            iprintf("Pico:%uus >16.7ms:%u       \n",peak_us,over_budget);
            iprintf("Periodo:%uus              \n",period_us);
        }
        work_us=(unsigned)((u64)(u16)(timerTick(0)-start)*1000000/(BUS_CLOCK/64));
        if(work_us>peak_us)peak_us=work_us;
        if(work_us>16667)++over_budget;
        swiWaitForVBlank();
    }
    return 0;
}
