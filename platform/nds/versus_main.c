/* 1v1 of converted fighters (Mario, Link) on the generic bone renderer. */
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include "versus.h"
#include "menu.h"
#include "mario_model.h"
#include "stage_render.h"
#include "recovery_demo.h"
#include "fight_audio.h"
#include "fighter_render.h"
#include "link_model.h"
#ifdef ANIMATION_REVIEW
#include "animation_review.h"
#endif
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

/* libnds console SGR colours; unsupported codes are ignored by the console. */
#define WHITE "\x1b[37m"
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define CYAN "\x1b[36m"
#define EMBLEM_SEGMENTS 24

/* Selectable characters: game data, converted model and its GPU textures. */
static FighterRender renders[MENU_FIGHTER_COUNT];
static const Fighter *const fighters[MENU_FIGHTER_COUNT]={&fighter_mario,&fighter_link};
static const FighterModel *const models[MENU_FIGHTER_COUNT]={&mario_model,&link_model};
static const FighterRender *render_of(const Fighter *f) {return &renders[f==&fighter_link];}
static unsigned up_clip(const Fighter *f) {return f->moves[f->kind==FIGHTER_MARIO?MOVE_UP:MOVE_UP_AIR].clip;}

static unsigned clip_for(const Fighter *f,const MotionScene *s)
{
    const FighterClipMap *c=&f->clips;
    if(s->state==SCENE_SPECIAL_HI)return up_clip(f);
    if(s->state==SCENE_SPECIAL_FALL)return c->special_fall;
    if(s->state==SCENE_CLIFF)return s->state_ticks<SCENE_CATCH_TICKS?c->cliff_catch:c->cliff_wait;
    if (s->state == SCENE_WALK) return c->walk;
    if (s->state == SCENE_SQUAT) return s->state_ticks<render_of(f)->model->clips[c->squat].ticks?c->squat:c->squat_wait;
    if (s->state == SCENE_KNEE) return c->jump;
    if(s->state==SCENE_AIR && s->air_jump) {
        unsigned clip=s->air_jump==1?c->jump_aerial_f:c->jump_aerial_b;
        return s->air_jump_ticks<render_of(f)->model->clips[clip].ticks?clip:c->fall;
    }
    if (s->state == SCENE_AIR) return s->vy > 0 ? c->jump : c->fall;
    return s->state == SCENE_LAND ? c->land : c->wait;
}

static void actor(const Fighter *f,const MotionScene *s,int facing,unsigned clip,unsigned age,unsigned tint,const FighterBlend *blend)
{
    const FighterClipMap *c=&f->clips;
    glPushMatrix();
    float x=s->x,y=s->y;
    /* Clips driven by scene timers rather than the per-clip age. */
    if(clip==c->jump_aerial_f || clip==c->jump_aerial_b)age=s->air_jump_ticks;
    if(clip==c->cliff_catch || clip==c->squat)age=s->state_ticks;
    if(clip==up_clip(f) || clip==c->special_fall)age=s->state_ticks;
    if(s->state==SCENE_CLIFF) {x=s->cliff_side*2318;y=0;facing=-s->cliff_side;}
    glTranslatef32(floattof32(x/WORLD_UNIT),floattof32(y/WORLD_UNIT),0);
    if(s->state==SCENE_SPECIAL_HI)glRotateZi(s->special_tilt*32768/360);
    glRotateYi(facing > 0 ? 8192 : -8192);
    fighter_render_draw(render_of(f),clip,age,tint,blend,0);
    glPopMatrix(1);
}

/* Link's thrown boomerang: the in-hand boomerang part, spinning in the view plane. */
static void draw_boomerang(const Boomerang *b)
{
    glPushMatrix();glTranslatef32(floattof32(b->x/WORLD_UNIT),floattof32(b->y/WORLD_UNIT),floattof32(.2f));
    glRotateZi((int)(b->age*3600)&32767);glRotateYi(8192);
    fighter_render_draw_part(&renders[1],LINK_PART_11_1,0x7fff);
    glPopMatrix(1);
}

/* Unit circle (24 segments), tabulated once at start-up by emblem_init(). */
static v16 ring[EMBLEM_SEGMENTS+1][2];

/* Bomb: provisional untextured stand-in (navy body, fuse spark), bloating in
 * the last 96 fuse ticks with the original 1.0..1.8 scale cycle. */
static void draw_bomb(const Bomb *m)
{
    static const int bloat[11]={4096,4915,5734,6554,7373,6554,5734,4915,4096,3277,3277};
    glPushMatrix();glTranslatef32(floattof32(m->x/WORLD_UNIT),floattof32(m->y/WORLD_UNIT),floattof32(.3f));
    glBindTexture(0,0);
    if(m->state==BOMB_EXPLODING) {
        float radius=(m->age<2?300:m->age<4?230:150)/WORLD_UNIT;
        glScalef32(floattof32(radius),floattof32(radius),inttof32(1));
        glPolyFmt(POLY_ALPHA(16)|POLY_CULL_NONE|POLY_ID(58));glColor(RGB15(31,22,6));
    } else {
        int k=m->life<96?bloat[(96-m->life)/4%11]:4096;
        glScalef32(floattof32(70.0f/WORLD_UNIT)*k/4096,floattof32(70.0f/WORLD_UNIT)*k/4096,inttof32(1));
        glPolyFmt(POLY_ALPHA(31)|POLY_CULL_NONE|POLY_ID(57));
        glColor(m->life<96 && (m->life&4)?RGB15(20,6,8):RGB15(4,6,16));
    }
    glBegin(GL_TRIANGLE);
    for(unsigned i=0;i<EMBLEM_SEGMENTS;++i) {glVertex3v16(0,0,0);glVertex3v16(ring[i][0],ring[i][1],0);glVertex3v16(ring[i+1][0],ring[i+1][1],0);}
    glEnd();
    if(m->state!=BOMB_EXPLODING) {
        glColor(RGB15(31,28,8));glBegin(GL_QUADS);
        glVertex3v16(800,4000,10);glVertex3v16(1800,4000,10);glVertex3v16(1800,5200,10);glVertex3v16(800,5200,10);glEnd();
    }
    glPopMatrix(1);
}

/* Mario's fireball: MarioSpecial3 sprite on one textured quad. */
static int fireball_texture;
static void draw_fireball(float x,float y,unsigned age)
{
    glPushMatrix();glTranslatef32(floattof32(x/WORLD_UNIT),floattof32(y/WORLD_UNIT),0);
    glRotateZi((age*1820)&32767);
    glBindTexture(0,fireball_texture);glColor(RGB15(31,31,31));
    glPolyFmt(POLY_ALPHA(31)|POLY_CULL_NONE|POLY_ID(4));
    int r=floattov16(100/WORLD_UNIT);
    glBegin(GL_QUADS);
    glTexCoord2t16(0,16<<4);glVertex3v16(-r,-r,0);
    glTexCoord2t16(16<<4,16<<4);glVertex3v16(r,-r,0);
    glTexCoord2t16(16<<4,0);glVertex3v16(r,r,0);
    glTexCoord2t16(0,0);glVertex3v16(-r,r,0);glEnd();glPopMatrix(1);
}

/* Title emblem: 24 ring quads + 2 bars, untextured and translucent. The unit
 * circle is tabulated once at start-up, so the menu does no per-frame trig. */

static void emblem_init(void)
{
    for(unsigned i=0;i<=EMBLEM_SEGMENTS;++i) {
        int angle=(int)(i*32768/EMBLEM_SEGMENTS);
        ring[i][0]=cosLerp(angle);ring[i][1]=sinLerp(angle);
    }
}

static void emblem_quad(v16 x0,v16 y0,v16 x1,v16 y1)
{
    glVertex3v16(x0,y0,0);glVertex3v16(x1,y0,0);glVertex3v16(x1,y1,0);glVertex3v16(x0,y1,0);
}

static void emblem_draw(float x,float y,float radius)
{
    glPushMatrix();
    glTranslatef32(floattof32(x),floattof32(y),floattof32(-12));
    glScalef32(floattof32(radius),floattof32(radius),inttof32(1));
    glBindTexture(0,0);
    /* One polygon ID: overlapping translucent pieces do not blend twice. */
    glPolyFmt(POLY_ALPHA(18)|POLY_CULL_NONE|POLY_ID(60));
    glColor(RGB15(31,31,31));
    glBegin(GL_QUADS);
    for(unsigned i=0;i<EMBLEM_SEGMENTS;++i) {
        const v16 *a=ring[i],*b=ring[i+1];
        /* Inner radius 0.8 = 3277/4096. */
        glVertex3v16(a[0]*3277/4096,a[1]*3277/4096,0);glVertex3v16(a[0],a[1],0);
        glVertex3v16(b[0],b[1],0);glVertex3v16(b[0]*3277/4096,b[1]*3277/4096,0);
    }
    emblem_quad(floattov16(-.36f),floattov16(-.85f),floattov16(-.24f),floattov16(.85f));
    emblem_quad(floattov16(-.85f),floattov16(-.31f),floattov16(.85f),floattov16(-.19f));
    glEnd();
    glPopMatrix(1);
}

static void at(int row,int col,const char *color,const char *text)
{
    iprintf("\x1b[%d;%dH%s%s",row,col,color,text);
}

static void draw_title(int prompt)
{
    if(prompt<0) {
        iprintf("\x1b[2J");
        at(2,1,CYAN,"+----------------------------+");
        for(int row=3;row<8;++row) {at(row,1,CYAN,"|");at(row,30,CYAN,"|");}
        at(8,1,CYAN,"+----------------------------+");
        at(4,5,YELLOW,"SUPER  SMASH  BROS.");
        at(6,10,RED,"N   D   S");
        at(10,2,WHITE,"Port experimental de SSB64");
        at(11,2,WHITE,"para Nintendo DS / DS Lite");
        at(21,1,GREEN,"Prueba 18 - mapa: Dream Land");
        at(22,1,GREEN,"No es el juego completo");
        prompt=1;
    }
    at(16,9,YELLOW,prompt?"PULSA  START":"            ");
}

static void draw_select(const Menu *m)
{
    static const char *const names[MENU_OPTION_COUNT]={"MODO DE PRUEBA","DEMO CPU vs CPU","VISOR DE LINK","CONTROLES"};
    static const char *const notes[MENU_OPTION_COUNT]={"Mario vs CPU - 3 vidas","La CPU pelea contra si misma","Animaciones originales","Botones y acciones"};
    iprintf("\x1b[2J");
    at(1,1,CYAN,"+----------------------------+");
    at(2,1,CYAN,"|");at(2,6,YELLOW,"SELECCIONA UN MODO");at(2,30,CYAN,"|");
    at(3,1,CYAN,"+----------------------------+");
    at(5,2,WHITE,"Mapa: Dream Land (original)");
    for(unsigned i=0;i<MENU_OPTION_COUNT;++i) {
        int row=MENU_FIRST_ROW+(int)i*MENU_ROW_SPACING,chosen=i==m->cursor;
        at(row,2,chosen?YELLOW:WHITE,chosen?"> ":"  ");
        iprintf("%s",names[i]);
        at(row+1,4,chosen?CYAN:BLUE,notes[i]);
    }
    at(20,1,WHITE,"ARRIBA/ABAJO elegir  A aceptar");
    at(21,1,WHITE,"B volver   o toca una opcion");
}

static void draw_controls(void)
{
    iprintf("\x1b[2J");
    at(1,1,CYAN,"+----------------------------+");
    at(2,1,CYAN,"|");at(2,11,YELLOW,"CONTROLES");at(2,30,CYAN,"|");
    at(3,1,CYAN,"+----------------------------+");
    for(unsigned i=0;i<menu_control_count;++i) {
        at(5+(int)i,MENU_CONTROL_LABEL_COLUMN,YELLOW,menu_controls[i].button);
        at(5+(int)i,MENU_CONTROL_ACTION_COLUMN,WHITE,menu_controls[i].action);
    }
    at(19,1,GREEN,"En pausa: B vuelve al menu");
    at(22,1,WHITE,"A, B o toca para volver");
}

static void draw_fighters(const Menu *m)
{
    iprintf("\x1b[2J");
    at(1,1,CYAN,"+----------------------------+");
    at(2,1,CYAN,"|");at(2,5,YELLOW,"ELIGE LOS PERSONAJES");at(2,30,CYAN,"|");
    at(3,1,CYAN,"+----------------------------+");
    at(5,2,WHITE,m->pending==MENU_PLAY_DEMO?"Demo: CPU contra CPU":"Modo de prueba: tu vs CPU");
    for(unsigned i=0;i<2;++i) {
        int row=MENU_PICK_ROW+(int)(i*MENU_PICK_SPACING),chosen=i==m->side;
        at(row,2,chosen?YELLOW:WHITE,chosen?"> ":"  ");
        iprintf("%s%-6s",i?BLUE:RED,i?"CPU":(m->pending==MENU_PLAY_DEMO?"CPU 1":"TU"));
        iprintf("%s  <  %s%-6s%s  >",WHITE,YELLOW,fighters[m->pick[i]]->name,WHITE);
    }
    at(MENU_START_ROW,9,GREEN,"A / toca: JUGAR");
    at(20,1,WHITE,"IZQ/DER personaje ARR/ABA lado");
    at(21,1,WHITE,"B volver   o toca un nombre");
}

static void draw_hud_frame(const Versus *match,int watch)
{
    iprintf("\x1b[2J");
    at(0,0,CYAN,"================================");
    at(1,1,YELLOW,watch?"DEMO CPU vs CPU":"MODO DE PRUEBA");
    at(1,20,WHITE,"Dream Land");
    at(2,0,CYAN,"================================");
    at(4,2,RED,watch?"CPU 1":"TU");at(4,18,BLUE,watch?"CPU 2":"CPU");
    at(4,7,WHITE,match->kinds[0]->name);at(4,23,WHITE,match->kinds[1]->name);
    at(11,1,WHITE,"X golpe  A salto  B especial");
    at(12,1,WHITE,"L agarre Y+X smash R camara");
    at(13,1,WHITE,"START pausa  SELECT modo");
    at(15,0,CYAN,"--------------------------------");
}

/* Fighter viewer: converted Link clips on the generic bone renderer. */
typedef struct {unsigned clip,age,slow;int paused,facing,dirty;} Viewer;

static void draw_viewer_frame(void)
{
    iprintf("\x1b[2J");
    at(0,0,CYAN,"================================");
    at(1,1,YELLOW,"VISOR DE LINK");at(1,20,WHITE,"Dream Land");
    at(2,0,CYAN,"================================");
    at(10,1,WHITE,"IZQ/DER  animacion");
    at(11,1,WHITE,"A pausa  R paso   Y lento");
    at(12,1,WHITE,"X girar  B volver al menu");
    at(14,0,CYAN,"--------------------------------");
    at(16,1,GREEN,"Modelo y animaciones de la ROM");
    at(17,1,GREEN,"Sin combate todavia");
}

static void draw_viewer(const Viewer *v)
{
    const FighterClip *c=&link_model.clips[v->clip];unsigned ticks=c->ticks;
    unsigned tick=c->loop?v->age%ticks:(v->age<ticks?v->age:ticks-1);
    iprintf("\x1b[4;1H%s%2u/%u %s%-18s",WHITE,v->clip+1,link_model.clip_count,YELLOW,link_clip_names[v->clip]);
    iprintf("\x1b[6;1H%sTic %3u/%-3u %s",WHITE,tick,ticks,c->loop?"bucle ":"      ");
    iprintf("\x1b[7;1H%s%-8s%-10s",YELLOW,v->paused?"PAUSA":"",v->slow?"LENTO x1/3":"");
}

static void viewer_step(Viewer *v,unsigned down)
{
    unsigned n=link_model.clip_count;
    if(down&KEY_RIGHT) {v->clip=(v->clip+1)%n;v->age=0;v->dirty=1;}
    if(down&KEY_LEFT) {v->clip=(v->clip+n-1)%n;v->age=0;v->dirty=1;}
    if(down&KEY_A) {v->paused=!v->paused;v->dirty=1;}
    if(down&KEY_Y) {v->slow=!v->slow;v->dirty=1;}
    if(down&KEY_X)v->facing=-v->facing;
    static unsigned clock;
    if(v->paused) {if(down&KEY_R) {++v->age;v->dirty=1;}}
    else if(!v->slow || ++clock%3==0) {++v->age;if((v->age&3)==0)v->dirty=1;}
}

static const char *percent_color(unsigned percent)
{return percent>=100?RED:percent>=50?YELLOW:WHITE;}

static void draw_hud(const Versus *match,int paused,int watch)
{
    for(unsigned i=0;i<2;++i) {
        int col=i?18:2;
        iprintf("\x1b[6;%dH%s%3u%%",col,percent_color(match->percent[i]),match->percent[i]);
        iprintf("\x1b[8;%dH%s",col,GREEN);
        for(unsigned s=0;s<3;++s)iprintf(s<match->stocks[i]?"o ":"  ");
    }
    if(match->winner>=0) {
        const char *who=match->winner==2?"EMPATE":match->winner==0?(watch?"GANA CPU 1":"GANASTE!"):(watch?"GANA CPU 2":"GANA CPU");
        iprintf("\x1b[9;1H%s%-11s%sA revancha B menu",YELLOW,who,WHITE);
    } else if(paused)iprintf("\x1b[9;1H%sPAUSA %s  B volver al menu  ",YELLOW,WHITE);
    else iprintf("\x1b[9;1H%s%-30s",WHITE,"");
}

int main(void)
{
    consoleDemoInit();lcdMainOnTop();videoSetMode(MODE_0_3D);glInit();
    /* Textures: fighters in bank A, Dream Land in B (+A), palettes in E. */
    vramSetBankA(VRAM_A_TEXTURE);glEnable(GL_TEXTURE_2D);
    int ready=dreamland_render_init() && glGenTextures(1,&fireball_texture);
    for(unsigned i=0;i<MENU_FIGHTER_COUNT && ready;++i)ready=fighter_render_init(&renders[i],models[i]);
    if(ready) {
        glBindTexture(0,fireball_texture);
        ready=glTexImage2D(0,0,GL_RGBA,TEXTURE_SIZE_16,TEXTURE_SIZE_16,0,0,mario_fireball_pixels);
    }
    if(!ready) {
        iprintf("Error al cargar recursos\n");while(pmMainLoop())swiWaitForVBlank();return 1;
    }
    emblem_init();fight_audio_init();
    glEnable(GL_ANTIALIAS);glEnable(GL_BLEND);glClearColor(14,24,30,31);glClearPolyID(63);
    glClearDepth(0x7fff);glViewport(0,0,255,191);
    static Versus match;versus_init(&match);
    FightCamera camera;camera_reset(&camera);
    Menu menu;menu_init(&menu);
    MenuInputGate input_gate={0};
    int in_menu=!SCENE_AUTOPLAY,fade=16,prompt=-1,hud_dirty=1,viewing=0;
    Viewer viewer={0,0,0,0,1,1};
    unsigned clips[2]={0},ages[2]={0};int paused=0,watch=SCENE_AUTOPLAY,map_view=0;
    unsigned rendered_age[2]={0},blend_left[2]={0};
    int damage_pose[2]={-1,-1};FighterBlend blends[2]={{0}};
#ifdef ANIMATION_REVIEW
    unsigned review_clock=0;
#endif
    unsigned work_us=0,peak_us=0,sim_us=0,over_budget=0,period_us=0;
    /* Calico reserves timers2/3. Timer0 is a free-running profiling counter;
     * 16-bit wrap is safe for intervals below ~125 ms at bus clock /64. */
    timerStart(0,ClockDivider_64,0,NULL);u16 previous=timerTick(0);
    while(pmMainLoop()) {
        u16 start=timerTick(0);
        period_us=(unsigned)((u64)(u16)(start-previous)*1000000/(BUS_CLOCK/64));previous=start;
        scanKeys();unsigned physical_held=keysHeld(),held=physical_held,down=keysDown();
        menu_input_filter(&input_gate,&held,&down);
        fight_audio_tick();
        /* Short fade from black on boot and on every screen change. */
        setBrightness(3,-fade);if(fade)--fade;
        if(in_menu) {
            int touch_row=-1;
            if(down&KEY_TOUCH) {touchPosition touch;touchRead(&touch);touch_row=touch.py/8;}
            unsigned keys=((down&KEY_UP)?MENU_KEY_UP:0)|((down&KEY_DOWN)?MENU_KEY_DOWN:0)|
                ((down&KEY_A)?MENU_KEY_A:0)|((down&KEY_B)?MENU_KEY_B:0)|((down&KEY_START)?MENU_KEY_START:0)|
                ((down&KEY_LEFT)?MENU_KEY_LEFT:0)|((down&KEY_RIGHT)?MENU_KEY_RIGHT:0);
            MenuAction action=menu_step(&menu,keys,touch_row);
            if(action==MENU_VIEW_FIGHTER) {
                in_menu=0;viewing=1;fade=16;viewer=(Viewer){0,0,0,0,1,1};
                menu_input_arm(&input_gate,physical_held);draw_viewer_frame();
            } else if(action!=MENU_STAY) {
                in_menu=0;watch=action==MENU_PLAY_DEMO;fade=16;hud_dirty=1;
                versus_init_fighters(&match,fighters[menu.pick[0]],fighters[menu.pick[1]]);camera_reset(&camera);
                menu_input_arm(&input_gate,physical_held);
                paused=map_view=0;peak_us=over_budget=0;ages[0]=ages[1]=clips[0]=clips[1]=0;
            } else if(menu.dirty) {
                menu.dirty=0;
                if(menu.screen==MENU_TITLE)draw_title(prompt=-1);
                else if(menu.screen==MENU_SELECT)draw_select(&menu);
                else if(menu.screen==MENU_FIGHTERS)draw_fighters(&menu);
                else draw_controls();
            }
            if(in_menu && menu.screen==MENU_TITLE && ((menu.ticks>>5)&1)!=(unsigned)prompt)
                draw_title(prompt=(int)((menu.ticks>>5)&1));
            /* Title: Mario and Link idle on the match stage (spawn positions). */
            float height=3400.0f/WORLD_UNIT,half_width=height*(2.0f/3.0f),bottom=-height*.5f;
            float cx=300.0f*sinLerp((int)(menu.ticks*32))/4096.0f,cy=1000.0f;
            glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(-half_width,half_width,bottom,height+bottom,.1,20);
            glMatrixMode(GL_MODELVIEW);glLoadIdentity();
            dreamland_render_background(-half_width,half_width,bottom,height+bottom);
            emblem_draw(0,height*.17f,height*.25f);
            glTranslatef32(floattof32(-cx/WORLD_UNIT),floattof32(-cy/WORLD_UNIT),floattof32(-6));
            dreamland_render_draw();
            for(unsigned i=0;i<2;++i)
                actor(fighters[i],&match.fighter[i].player,match.fighter[i].player.facing,fighters[i]->clips.wait,menu.ticks,0x7fff,NULL);
            glFlush(0);
            swiWaitForVBlank();
            continue;
        }
        if(viewing) {
            if(down&KEY_B) {
                viewing=0;in_menu=1;menu_init(&menu);menu.screen=MENU_SELECT;menu.cursor=MENU_OPTION_VIEWER;fade=16;
                swiWaitForVBlank();continue;
            }
            viewer_step(&viewer,down);
            if(viewer.dirty) {viewer.dirty=0;draw_viewer(&viewer);}
            float height=2600.0f/WORLD_UNIT,half_width=height*(2.0f/3.0f),bottom=-height*.5f;
            glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(-half_width,half_width,bottom,height+bottom,.1,20);
            glMatrixMode(GL_MODELVIEW);glLoadIdentity();
            dreamland_render_background(-half_width,half_width,bottom,height+bottom);
            /* Ledge clips are authored relative to the ledge (and their climbs
             * move by TransN): show them hanging from Dream Land's right edge. */
            int ledge=!strncmp(link_clip_names[viewer.clip],"Cliff",5);
            float x=ledge?2318.0f:0,cx=ledge?1900.0f:0;
            glTranslatef32(floattof32(-cx/WORLD_UNIT),floattof32(-700.0f/WORLD_UNIT),floattof32(-6));
            dreamland_render_draw();
            glPushMatrix();glTranslatef32(floattof32(x/WORLD_UNIT),0,0);
            glRotateYi((ledge?-1:viewer.facing)>0?8192:-8192);
            fighter_render_draw(&renders[1],viewer.clip,viewer.age,0x7fff,NULL,ledge?FIGHTER_DRAW_ROOT_MOTION:0);
            glPopMatrix(1);
            glFlush(0);swiWaitForVBlank();
            continue;
        }
        int reset=(held&KEY_START) && (held&KEY_SELECT) && (down&(KEY_START|KEY_SELECT));
        int to_menu=(paused || match.winner>=0) && (down&KEY_B);
        if(to_menu) {fight_audio_stop();in_menu=1;menu_init(&menu);menu.screen=MENU_SELECT;menu.cursor=watch?MENU_OPTION_DEMO:MENU_OPTION_TEST;fade=16;
            versus_init_fighters(&match,fighters[0],fighters[1]);swiWaitForVBlank();continue;}
        if(reset || (match.winner>=0 && (down&KEY_A))) {
            fight_audio_stop();versus_init_fighters(&match,match.kinds[0],match.kinds[1]);camera_reset(&camera);paused=0;peak_us=over_budget=0;
            ages[0]=ages[1]=0;hud_dirty=1;
            menu_input_arm(&input_gate,physical_held);held=physical_held;
            menu_input_filter(&input_gate,&held,&down);
        }
        else {
            if(down&KEY_START) {paused=!paused;if(paused)fight_audio_stop();}
            if(down&KEY_SELECT) {
                fight_audio_stop();watch=!watch;versus_init_fighters(&match,match.kinds[0],match.kinds[1]);camera_reset(&camera);
                ages[0]=ages[1]=0;hud_dirty=1;
                menu_input_arm(&input_gate,physical_held);held=physical_held;
                menu_input_filter(&input_gate,&held,&down);
            }
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
#ifdef ANIMATION_REVIEW
            int advances=animation_review_step(&match,review_clock++);
#else
            if(watch)input[0]=versus_cpu(&match,0);
            input[1]=versus_cpu(&match,1);
            int advances=!match.freeze && match.winner<0;
            versus_step(&match,input);
#endif
            fight_audio_events(&match);
            if(advances)for(unsigned i=0;i<2;++i) {
                CombatScene *s=&match.fighter[i];const Fighter *f=s->fighter;const FighterClipMap *c=&f->clips;
                unsigned next=clip_for(f,&s->player);
                /* SquatRv plays once when down is released (until it ends or another state starts). */
                if(next==c->wait && (clips[i]==c->squat || clips[i]==c->squat_wait ||
                   (clips[i]==c->squat_rv && ages[i]+1<render_of(f)->model->clips[c->squat_rv].ticks)))next=c->squat_rv;
                if(s->attack_frame>=0)next=combat_player_clip(s);
                if(s->item_throw)next=s->player.state==SCENE_AIR?c->item_throw_air:c->item_throw;
                else if(s->dash_ticks)next=s->run_age<render_of(f)->model->clips[c->dash].ticks?c->dash:c->run;
                else if(s->brake_ticks)next=c->brake;
                if(versus_captured(&match,i))next=c->capture;
                int pose=versus_damage_pose(&match,i);
                if(pose>=0)next=pose==17?c->damage_fall:c->damage[pose];
                if(blend_left[i])--blend_left[i];
                if(damage_pose[i]>=0 && (pose<0 || pose==17) && next!=clips[i] &&
                   s->attack_frame<0 && !versus_captured(&match,i)) {
                    blends[i]=(FighterBlend){clips[i],rendered_age[i],0};blend_left[i]=2;
                }
                if(pose>=0 && pose!=17 && pose!=damage_pose[i])blend_left[i]=0;
                if(s->attack_frame>=0)blend_left[i]=0;
                blends[i].weight=256-blend_left[i]*256/3;
                damage_pose[i]=pose;
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
            CombatScene *s=&match.fighter[i];const Fighter *f=s->fighter;
            /* Same character on both sides: tint the CPU blue, as before. */
            unsigned tint=i && match.kinds[0]==match.kinds[1]?RGB15(12,24,31):0x7fff;
            if(match.stun[i] && match.freeze)tint=RGB15(31,12,12);
            unsigned age=s->attack_frame>=0?(unsigned)s->attack_frame:ages[i];
            if(damage_pose[i]>=0) {
                age=match.reaction[i].age;
                if(damage_pose[i]==17)age-=f->damage_ticks[match.reaction[i].pose];
                unsigned ticks=render_of(f)->model->clips[clips[i]].ticks;
                if(age>=ticks)age=ticks-1;
            }
            rendered_age[i]=age;
            /* Holding a bomb: shield on the back (itmain pickup sets base parts 21/19). */
            FighterRender *r=&renders[f==&fighter_link];
            if(f==&fighter_link && s->bomb.state==BOMB_HELD) {r->part_set=1u<<LINK_PART_21_0;r->part_clear=1u<<LINK_PART_19_0;}
            if(!match.invulnerable[i] || (match.tick&4))
                actor(f,&s->player,s->player.facing,clips[i],age,tint,blend_left[i]?&blends[i]:NULL);
            r->part_set=r->part_clear=0;
            for(unsigned j=0;j<4;++j)if(s->fireballs[j].life)
                draw_fireball(s->fireballs[j].x,s->fireballs[j].y,140-s->fireballs[j].life);
            if(s->boomerang.active)draw_boomerang(&s->boomerang);
            if(s->bomb.state!=BOMB_NONE)draw_bomb(&s->bomb);
        }
        glFlush(0);
        if(hud_dirty) {draw_hud_frame(&match,watch);hud_dirty=0;}
        if((match.tick&15)==0 || paused || match.winner>=0) {
            draw_hud(&match,paused,watch);
            iprintf("\x1b[17;1H%sSim:%uus Trabajo:%uus      \n",WHITE,sim_us,work_us);
            iprintf(" Pico:%uus >16.7ms:%u       \n",peak_us,over_budget);
            iprintf(" Periodo:%uus              \n",period_us);
        }
#ifdef ANIMATION_REVIEW
        iprintf("\x1b[21;1HRevision %-8s t:%3u       ",match.fighter[0].fighter->moves[match.fighter[0].move].name,match.tick);
        iprintf("\x1b[22;1HReaccion:%2d edad:%3u      ",versus_damage_pose(&match,1),match.reaction[1].age);
#endif
        work_us=(unsigned)((u64)(u16)(timerTick(0)-start)*1000000/(BUS_CLOCK/64));
        if(work_us>peak_us)peak_us=work_us;
        if(work_us>16667)++over_budget;
        swiWaitForVBlank();
    }
    return 0;
}
