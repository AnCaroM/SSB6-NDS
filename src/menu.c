/* Local front-end flow: title, mode select and controls page. */
#include "menu.h"

void menu_init(Menu *m)
{m->screen=MENU_TITLE;m->cursor=MENU_OPTION_TEST;m->ticks=0;m->dirty=1;}

static MenuAction choose(Menu *m)
{
    if(m->cursor==MENU_OPTION_TEST)return MENU_PLAY_TEST;
    if(m->cursor==MENU_OPTION_DEMO)return MENU_PLAY_DEMO;
    m->screen=MENU_CONTROLS;m->dirty=1;return MENU_STAY;
}

MenuAction menu_step(Menu *m,unsigned down,int touch_row)
{
    ++m->ticks;
    if(m->screen==MENU_TITLE) {
        if(down&(MENU_KEY_A|MENU_KEY_START) || touch_row>=0) {m->screen=MENU_SELECT;m->dirty=1;}
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
