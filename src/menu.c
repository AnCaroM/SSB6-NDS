/* Local front-end flow: title, mode select and controls page. */
#include "menu.h"

const MenuControl menu_controls[]={
    {"Cruceta","mover / correr"},{"A","salto / doble"},
    {"X / dir+X","golpes y aereos"},{"Y+X+dir","smash"},
    {"L, luego X","agarrar / lanzar"},{"B","bola de fuego"},
    {"Arriba+B","recuperacion"},{"Abajo+B","tornado"},
    {"Abajo","bajar / soltar"},{"R","escudo / rodar"},
    {"Tocar abajo","mapa completo"},
    {"START","pausa"},{"SELECT","TU <-> CPU vs CPU"},
    {"START+SELECT","reiniciar"}
};
const unsigned menu_control_count=sizeof menu_controls/sizeof *menu_controls;

void menu_input_arm(MenuInputGate *gate,unsigned held)
{gate->blocked=held;}

void menu_input_filter(MenuInputGate *gate,unsigned *held,unsigned *down)
{
    unsigned blocked=gate->blocked;
    gate->blocked&=*held;
    *held&=~blocked;*down&=~blocked;
}

void menu_init(Menu *m)
{
    m->screen=MENU_TITLE;m->cursor=MENU_OPTION_TEST;m->ticks=0;m->dirty=1;
    m->pick[0]=0;m->pick[1]=1;m->side=0;m->pending=MENU_STAY;  /* Mario vs Link */
}

static MenuAction fighters(Menu *m,MenuAction mode)
{m->screen=MENU_FIGHTERS;m->pending=mode;m->side=0;m->dirty=1;return MENU_STAY;}

static MenuAction choose(Menu *m)
{
    if(m->cursor==MENU_OPTION_TEST)return fighters(m,MENU_PLAY_TEST);
    if(m->cursor==MENU_OPTION_DEMO)return fighters(m,MENU_PLAY_DEMO);
    if(m->cursor==MENU_OPTION_VIEWER)return MENU_VIEW_FIGHTER;
    m->screen=MENU_CONTROLS;m->dirty=1;return MENU_STAY;
}

MenuAction menu_step(Menu *m,unsigned down,int touch_row)
{
    ++m->ticks;
    if(m->screen==MENU_TITLE) {
        if(down&(MENU_KEY_A|MENU_KEY_START) || touch_row>=0) {m->screen=MENU_SELECT;m->dirty=1;}
        return MENU_STAY;
    }
    if(m->screen==MENU_FIGHTERS) {
        if(down&MENU_KEY_B) {m->screen=MENU_SELECT;m->dirty=1;return MENU_STAY;}
        if(touch_row>=MENU_START_ROW && touch_row<=MENU_START_ROW+1)return m->pending;
        for(unsigned i=0;i<2;++i)if(touch_row>=MENU_PICK_ROW+(int)(i*MENU_PICK_SPACING) &&
                                    touch_row<=MENU_PICK_ROW+(int)(i*MENU_PICK_SPACING)+1) {
            m->side=i;m->pick[i]=(m->pick[i]+1)%MENU_FIGHTER_COUNT;m->dirty=1;
        }
        if(down&(MENU_KEY_UP|MENU_KEY_DOWN)) {m->side^=1;m->dirty=1;}
        if(down&MENU_KEY_RIGHT) {m->pick[m->side]=(m->pick[m->side]+1)%MENU_FIGHTER_COUNT;m->dirty=1;}
        if(down&MENU_KEY_LEFT) {m->pick[m->side]=(m->pick[m->side]+MENU_FIGHTER_COUNT-1)%MENU_FIGHTER_COUNT;m->dirty=1;}
        if(down&(MENU_KEY_A|MENU_KEY_START))return m->pending;
        return MENU_STAY;
    }
    if(m->screen==MENU_CONTROLS) {
        if(down&(MENU_KEY_A|MENU_KEY_B|MENU_KEY_START) || touch_row>=0) {m->screen=MENU_SELECT;m->dirty=1;}
        return MENU_STAY;
    }
    if(down&MENU_KEY_B) {m->screen=MENU_TITLE;m->dirty=1;return MENU_STAY;}
    if(touch_row>=MENU_FIRST_ROW) {
        unsigned option=(unsigned)(touch_row-MENU_FIRST_ROW)/MENU_ROW_SPACING;
        if(option<MENU_OPTION_COUNT && (unsigned)(touch_row-MENU_FIRST_ROW)%MENU_ROW_SPACING<2) {
            m->cursor=option;m->dirty=1;return choose(m);
        }
    }
    if(down&MENU_KEY_UP) {m->cursor=(m->cursor+MENU_OPTION_COUNT-1)%MENU_OPTION_COUNT;m->dirty=1;}
    if(down&MENU_KEY_DOWN) {m->cursor=(m->cursor+1)%MENU_OPTION_COUNT;m->dirty=1;}
    if(down&(MENU_KEY_A|MENU_KEY_START))return choose(m);
    return MENU_STAY;
}
