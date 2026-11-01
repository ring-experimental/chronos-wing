// 스토리 다시보기. 메인메뉴 창.
#include "panels.h"

#define T_LABEL "\xbd\xba\xc5\xe4\xb8\xae\x20\xb4\xd9\xbd\xc3\xba\xb8\xb1\xe2" // 스토리 다시보기

static void open_panel(void) {}
static void draw_panel(int x, int y) { menu_draw_frame(x, y); }
static void panel_key(int key) { if (key == -16) menu_leave(); }

const menu_panel replay_panel = {T_LABEL, open_panel, draw_panel, panel_key, NULL, NULL};
