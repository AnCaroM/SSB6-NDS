#include "menu.h"
#include "motion_scene.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
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
    assert(menu_step(&m,MENU_KEY_A,-1)==MENU_STAY && m.screen==MENU_FIGHTERS);
    assert(menu_step(&m,MENU_KEY_A,-1)==MENU_PLAY_TEST);
    menu_init(&m);menu_step(&m,MENU_KEY_START,-1);menu_step(&m,MENU_KEY_DOWN,-1);
    assert(menu_step(&m,MENU_KEY_START,-1)==MENU_STAY);
    assert(menu_step(&m,MENU_KEY_START,-1)==MENU_PLAY_DEMO);
    /* Character screen: Mario vs Link by default; arrows and touch change it. */
    menu_init(&m);menu_step(&m,MENU_KEY_START,-1);menu_step(&m,MENU_KEY_A,-1);
    assert(m.screen==MENU_FIGHTERS && m.pick[0]==0 && m.pick[1]==1);
    menu_step(&m,MENU_KEY_RIGHT,-1);assert(m.pick[0]==1);
    menu_step(&m,MENU_KEY_DOWN,-1);menu_step(&m,MENU_KEY_LEFT,-1);assert(m.side==1 && m.pick[1]==0);
    menu_step(&m,0,MENU_PICK_ROW+MENU_PICK_SPACING);assert(m.pick[1]==1);
    assert(menu_step(&m,MENU_KEY_B,-1)==MENU_STAY && m.screen==MENU_SELECT);
    /* Third option: explore the stage with one chosen fighter (single row). */
    menu_init(&m);menu_step(&m,MENU_KEY_START,-1);menu_step(&m,MENU_KEY_DOWN,-1);menu_step(&m,MENU_KEY_DOWN,-1);
    assert(m.cursor==MENU_OPTION_EXPLORE && menu_step(&m,MENU_KEY_A,-1)==MENU_STAY);
    assert(m.screen==MENU_FIGHTERS && m.pending==MENU_PLAY_EXPLORE && m.side==0);
    menu_step(&m,MENU_KEY_DOWN,-1);assert(m.side==0);                 /* no second side */
    menu_step(&m,MENU_KEY_RIGHT,-1);assert(m.pick[0]==1);
    menu_step(&m,0,MENU_PICK_ROW+MENU_PICK_SPACING);assert(m.pick[1]==1); /* second row inert */
    assert(menu_step(&m,MENU_KEY_A,-1)==MENU_PLAY_EXPLORE);
    m.screen=MENU_SELECT;
    assert(menu_step(&m,0,MENU_FIRST_ROW+2*MENU_ROW_SPACING)==MENU_STAY && m.pending==MENU_PLAY_EXPLORE);
    /* Controls page opens from the list and returns to it. */
    menu_init(&m);menu_step(&m,MENU_KEY_START,-1);m.cursor=MENU_OPTION_CONTROLS;
    assert(menu_step(&m,MENU_KEY_A,-1)==MENU_STAY && m.screen==MENU_CONTROLS);
    assert(menu_step(&m,MENU_KEY_B,-1)==MENU_STAY && m.screen==MENU_SELECT);
    /* B from the list goes back to the title. */
    assert(menu_step(&m,MENU_KEY_B,-1)==MENU_STAY && m.screen==MENU_TITLE);
    /* Touch: title row and description row select; spacer row does not. */
    menu_init(&m);menu_step(&m,0,5);assert(m.screen==MENU_SELECT);
    assert(menu_step(&m,0,MENU_FIRST_ROW+MENU_ROW_SPACING+1)==MENU_STAY && m.pending==MENU_PLAY_DEMO);
    m.screen=MENU_SELECT;
    menu_init(&m);menu_step(&m,MENU_KEY_A,-1);
    assert(menu_step(&m,0,MENU_FIRST_ROW+2)==MENU_STAY && m.cursor==MENU_OPTION_TEST);
    assert(menu_step(&m,0,MENU_FIRST_ROW)==MENU_STAY && m.screen==MENU_FIGHTERS);
    assert(menu_step(&m,0,MENU_START_ROW)==MENU_PLAY_TEST);m.screen=MENU_SELECT;
    assert(menu_step(&m,0,MENU_FIRST_ROW+MENU_ROW_SPACING*MENU_OPTION_COUNT)==MENU_STAY);
    assert(menu_step(&m,0,2)==MENU_STAY && m.screen==MENU_SELECT); /* Above list. */
    /* Confirming with A cannot become a jump, including the reset frame. */
    enum {BUTTON_A=1u<<12,BUTTON_OTHER=1u<<13};
    MenuInputGate gate={0};MotionScene fighter;scene_init(&fighter);
    menu_input_arm(&gate,BUTTON_A);
    for(unsigned t=0;t<30;++t) {
        unsigned held=BUTTON_A,down=t==0?BUTTON_A:0;
        menu_input_filter(&gate,&held,&down);assert(!(held|down));
        scene_step(&fighter,(SceneInput){0,!!(held&BUTTON_A)});
        assert(fighter.state==SCENE_WAIT);
    }
    unsigned held=BUTTON_A|BUTTON_OTHER,down=BUTTON_OTHER;
    menu_input_filter(&gate,&held,&down);assert(held==BUTTON_OTHER && down==BUTTON_OTHER);
    held=down=0;menu_input_filter(&gate,&held,&down);
    held=down=BUTTON_A;menu_input_filter(&gate,&held,&down);
    assert(held==BUTTON_A && down==BUTTON_A);
    scene_step(&fighter,(SceneInput){0,!!(held&BUTTON_A)});assert(fighter.state==SCENE_KNEE);
    /* Validate the actual strings/columns rendered on the 32-column console. */
    assert(menu_control_count>0 && 5+menu_control_count<=19);
    for(unsigned i=0;i<menu_control_count;++i) {
        assert(strlen(menu_controls[i].button)+MENU_CONTROL_LABEL_COLUMN<=MENU_CONTROL_ACTION_COLUMN);
        assert(strlen(menu_controls[i].action)+MENU_CONTROL_ACTION_COLUMN<=MENU_CONSOLE_COLUMNS);
    }
    puts("PASS menu: title, modes, controls, touch, release-before-gameplay and 32-column text bounds");
    return 0;
}
