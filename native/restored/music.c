// 플레이리스트. 메인메뉴 창.
#include "panels.h"

#define T_LABEL "\xc7\xc3\xb7\xb9\xc0\xcc\xb8\xae\xbd\xba\xc6\xae" // 플레이리스트

static void open_panel(void) {}
static void draw_panel(int x, int y) { menu_draw_frame(x, y); }
static void panel_key(int key) { if (key == -16) menu_leave(); }

const menu_panel music_panel = {T_LABEL, open_panel, draw_panel, panel_key, NULL, NULL};
