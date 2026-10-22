#ifndef SSB_MENU_H
#define SSB_MENU_H
/* Front-end state only: no SDK calls, so the flow is host-testable. */
enum {MENU_KEY_UP=1,MENU_KEY_DOWN=2,MENU_KEY_A=4,MENU_KEY_B=8,MENU_KEY_START=16};
typedef enum {MENU_TITLE,MENU_SELECT,MENU_CONTROLS} MenuScreen;
typedef enum {MENU_OPTION_TEST,MENU_OPTION_DEMO,MENU_OPTION_CONTROLS,MENU_OPTION_COUNT} MenuOption;
typedef enum {MENU_STAY,MENU_PLAY_TEST,MENU_PLAY_DEMO} MenuAction;
/* Bottom console rows: each option is a title row plus a description row. */
#define MENU_FIRST_ROW 8
#define MENU_ROW_SPACING 3
typedef struct {MenuScreen screen;unsigned cursor,ticks;int dirty;} Menu;
void menu_init(Menu *m);
/* down: MENU_KEY_* pressed this frame; touch_row: console row or -1. */
MenuAction menu_step(Menu *m,unsigned down,int touch_row);
#endif
