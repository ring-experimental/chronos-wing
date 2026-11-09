// 턴제 모드의 짐가방(사용자 결정 2026-11-09): 칸 수 제한을 원작 자료형이 견디는 만큼(127칸) 늘리고, 소지품 창을 원작 해상도(240×320)에 꽉 채운다.
// - 칸 수: 원작은 커서(가방 +4)·표시(+5)·남은 칸(0x13ad90)·개수(0x13ac9c)를 부호 있는 1바이트로 다룬다. -1이 "커서 없음"이므로 127칸이 한계다.
// - 창: 소지품 창 그리기 0x124c70(x, y, 포커스)을 감싸 창 왼쪽 위를 240×320 영역의 왼쪽 위로 주고, 그리는 동안만
//   창틀 0x172710(176×160 → 240×320, 구획은 직접)과 아이템 정보 0x139804(창 아래쪽으로)를 바꾼다. 허리주머니·제목·팝업은 원작 그대로다.
// - 격자: bag.c의 0x13aad8이 턴제에서는 10열, 아래 정보 칸까지 보이는 줄 수로 그린다. 키 0x13abc0을 감싸 위·아래 이동을 10칸으로 한다.
// - 저장: 원작 BAGSAVE(9000바이트, 36칸)는 본편과 같이 쓰고, 127칸 전체는 TURNBAG에 따로 쓴다(bag.c).
// 근거는 work/turn-plan.md "짐가방 늘리기".
#include "restored.h"
#include "turn.h"
#include <stdio.h>

void orig_f_24c70(cpu_t *c);
void orig_f_72710(cpu_t *c);
void orig_f_39804(cpu_t *c);
void orig_f_3abc0(cpu_t *c);

enum {
    WIN_W = 240, WIN_H = 320, OLD_W = 0xb0, OLD_H = 0xa0,
    INFO_TOP_FROM_BOTTOM = 0xa0 - 0x76, // 정보 칸 구분선(원작 창 160의 0x76)을 창 아래에서 같은 거리에
    INFO_TEXT = 7,                      // 구분선 아래 정보 글(원작 0x7d - 0x76)
    FN_DARK_BOX = 0x1071a8,
    GRID_TOP = 0x38,       // 원작 격자 위치(창 위에서)
    GOT_SCREEN = 0x2c, SCREEN_W = 0x48,
    KEY_UP = -1, KEY_DOWN = -2,
    FN_CURSOR_ON = 0x11f244, FN_LIST_EMPTY = 0x13f13c, FN_BAG_COUNT = 0x13ac9c, FN_POUCH_COUNT = 0x13b3cc,
    BAG_CURSOR = 4, BAG_LIST = 0x10,
};

static bool drawing;
static int win_y;

bool turn_bag_wide(void) { return turn_mode_on(); }
int turn_bag_cols(void) { return turn_bag_wide() ? TURN_BAG_COLS : 6; }
// 격자에 보이는 줄 수: 격자 위치부터 정보 칸 위까지.
int turn_bag_rows(void) { return turn_bag_wide() ? (WIN_H - INFO_TOP_FROM_BOTTOM - 3 - GRID_TOP) / 20 : 3; }

void f_24c70(cpu_t *c) {
    if (!turn_bag_wide()) { orig_f_24c70(c); return; }
    int w = (int)R32(got(GOT_SCREEN) + SCREEN_W), x = (w - WIN_W) / 2;
    uint32_t focus = c->r[2];
    win_y = 0;
    // 원작 창 바탕(0x172ac4)은 일정 높이까지만 칠하므로 바탕을 먼저 칠한다. CALL이 r0~r3을 바꾸므로 인자는 뒤에 넣는다.
    CALL(FN_FILL_RECT, R32(got(GOT_SCREEN)), (uint32_t)x, 0, WIN_W, WIN_H, 0x0c, 0x1a, 0x38);
    c->r[0] = (uint32_t)x;
    c->r[1] = (uint32_t)win_y;
    c->r[2] = focus;
    drawing = true;
    orig_f_24c70(c);
    drawing = false;
}

// 창틀(종류, 모양, x, y, 폭, 높이). 소지품 창은 종류 4로, 허리주머니·짐가방 글줄·격자·정보 칸을 원래 높이(160)에 맞춘 자리에 그린다.
// 큰 창에서는 테두리만 원작(종류 0)으로 그리고 같은 칠하기(0x106998)와 어두운 칸(0x1071a8)으로 격자와 정보 칸을 아래로 늘려 그린다.
void f_72710(cpu_t *c) {
    if (!(drawing && (int8_t)c->r[0] == 4 && (int)c->r[3] == win_y && R32(c->r[13]) == OLD_W && R32(c->r[13] + 4) == OLD_H)) { orig_f_72710(c); return; }
    int x = (int)c->r[2], y = (int)c->r[3], w = WIN_W, h = WIN_H, info = h - INFO_TOP_FROM_BOTTOM;
    c->r[0] = 0;
    W32(c->r[13], WIN_W);
    W32(c->r[13] + 4, WIN_H);
    orig_f_72710(c);
    uint32_t sc = R32(got(GOT_SCREEN));
    CALL(FN_DARK_BOX, sc, x + 4, y + 0xf, w - 8, 0x16, 0, 0, 0);          // 허리주머니
    CALL(FN_DARK_BOX, sc, x + 5, y + 0x10, w - 10, 0x14, 0, 0, 0);
    CALL(FN_FILL_RECT, sc, x + 1, y + 0x25, w - 4, 1, 0x30, 0x5f, 0xae);   // 짐가방 글줄
    CALL(FN_FILL_RECT, sc, x + 2, y + 0x26, w - 5, 0xd, 0x1e, 0x3c, 0x73);
    CALL(FN_DARK_BOX, sc, x + 4, y + 0x33, w - 8, info - 0x33 - 1, 0, 0, 0); // 격자
    CALL(FN_DARK_BOX, sc, x + 5, y + 0x34, w - 10, info - 0x34 - 3, 0, 0, 0);
    CALL(FN_FILL_RECT, sc, x + 1, y + info, w - 4, 1, 0x30, 0x5f, 0xae);    // 정보 칸
    CALL(FN_FILL_RECT, sc, x + 2, y + info + 1, w - 5, 2, 0x1e, 0x3c, 0x73);
    CALL(FN_DARK_BOX, sc, x + 4, y + info + 3, w - 8, 0x23, 0, 0, 0);
    CALL(FN_DARK_BOX, sc, x + 5, y + info + 4, w - 10, 0x22, 0, 0, 0);
}

// 아이템 정보(아이템, x, y, ...): 격자 아래로 옮긴다.
void f_39804(cpu_t *c) {
    if (drawing) c->r[2] = (uint32_t)(win_y + WIN_H - INFO_TOP_FROM_BOTTOM + INFO_TEXT);
    orig_f_39804(c);
}

// 짐가방 키(가방, 키). 위·아래만 10열에 맞춰 직접 하고 나머지는 원작(왼쪽·오른쪽은 개수로 돈다, 확인은 행동 팝업).
void f_3abc0(cpu_t *c) {
    int key = (int)c->r[1];
    bool up = key == KEY_UP || key == '2', down = key == KEY_DOWN || key == '8';
    if (!turn_bag_wide() || (!up && !down)) { orig_f_3abc0(c); return; }
    uint32_t bag = c->r[0];
    if (!(CALL(FN_CURSOR_ON, bag + BAG_CURSOR) & 0xff)) { c->r[0] = 0xffffffffu; return; }
    if (CALL(FN_LIST_EMPTY, bag + BAG_LIST) & 0xff) { c->r[0] = 2; return; }
    int count = (int8_t)CALL(FN_BAG_COUNT), cur = (int8_t)R8(bag + BAG_CURSOR), cols = TURN_BAG_COLS;
    c->r[0] = 0;
    if (up) {
        if (cur - cols >= 0) W8(bag + BAG_CURSOR, (uint8_t)(cur - cols));
        else if ((int8_t)CALL(FN_POUCH_COUNT) > 0) c->r[0] = 3; // 맨 윗줄에서 위: 허리주머니로(원작과 같다)
    } else if (cur + cols < count) W8(bag + BAG_CURSOR, (uint8_t)(cur + cols));
}
