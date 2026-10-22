#include "menu.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    Menu m;menu_init(&m);
    assert(m.screen==MENU_TITLE && m.dirty);
    assert(menu_step(&m,MENU_KEY_A,-1)==MENU_STAY && m.screen==MENU_SELECT); /* A also opens. */
    menu_init(&m);
    assert(menu_step(&m,0,-1)==MENU_STAY && m.screen==MENU_TITLE);
    assert(menu_step(&m,MENU_KEY_START,-1)==MENU_STAY && m.screen==MENU_SELECT);
    assert(m.cursor==MENU_OPTION_TEST);
    assert(menu_step(&m,MENU_KEY_UP,-1)==MENU_STAY && m.cursor==MENU_OPTION_CONTROLS); /* Wraps. */
    assert(menu_step(&m,MENU_KEY_DOWN,-1)==MENU_STAY && m.cursor==MENU_OPTION_TEST);
    assert(menu_step(&m,MENU_KEY_A,-1)==MENU_PLAY_TEST);
    menu_init(&m);menu_step(&m,MENU_KEY_START,-1);menu_step(&m,MENU_KEY_DOWN,-1);
    assert(menu_step(&m,MENU_KEY_START,-1)==MENU_PLAY_DEMO);
    /* Controls page opens from the list and returns to it. */
    menu_init(&m);menu_step(&m,MENU_KEY_START,-1);m.cursor=MENU_OPTION_CONTROLS;
    assert(menu_step(&m,MENU_KEY_A,-1)==MENU_STAY && m.screen==MENU_CONTROLS);
    assert(menu_step(&m,MENU_KEY_B,-1)==MENU_STAY && m.screen==MENU_SELECT);
    /* B from the list goes back to the title. */
    assert(menu_step(&m,MENU_KEY_B,-1)==MENU_STAY && m.screen==MENU_TITLE);
    /* Touch: title row and description row select; spacer row does not. */
    menu_init(&m);menu_step(&m,0,5);assert(m.screen==MENU_SELECT);
    assert(menu_step(&m,0,MENU_FIRST_ROW+MENU_ROW_SPACING+1)==MENU_PLAY_DEMO);
    menu_init(&m);menu_step(&m,MENU_KEY_A,-1);
    assert(menu_step(&m,0,MENU_FIRST_ROW+2)==MENU_STAY && m.cursor==MENU_OPTION_TEST);
    assert(menu_step(&m,0,MENU_FIRST_ROW)==MENU_PLAY_TEST);
    assert(menu_step(&m,0,MENU_FIRST_ROW+MENU_ROW_SPACING*MENU_OPTION_COUNT)==MENU_STAY);
    assert(menu_step(&m,0,2)==MENU_STAY && m.screen==MENU_SELECT); /* Above list. */
    puts("PASS menu: title, mode list, controls page, keys and touch");
    return 0;
}
