// 게임 화면의 키 안내를 패드 버튼으로 바꿔 그린다. 패드로 조작 중이 아니면 원본 그대로다.
// 필드 오른쪽 아래의 소지품(*)/미니맵(#) 그림은 0x1444cc가 두 장으로 그린다. 그 그림 대신 버튼 이름 두 줄을 오른쪽에 맞춰 쓴다.
// 컷신의 점멸하는 "# SKIP" 글은 0x11c6cc가 0x12c50c로 그린다. 그 글만 대사 기록, 자동 진행, 스킵 안내로 바꾼다.
#include "restored.h"
#include "../host/gamepad.h"
#include <string.h>

void orig_f_444cc(cpu_t *c);
void orig_f_2c50c(cpu_t *c);

enum {
    GOT_SCREEN = 0x2c,
    GOT_HUD = 0xab8,
    HUD_SOFT_KEY_FRAME = 0x45, // 소지품 그림. 0은 기본, 1은 누른 그림. 음수면 그리지 않는다.
    HUD_MAP_KEY_FRAME = 0x46,  // 미니맵(#) 그림. 음수면 그리지 않는다.
    FN_DRAW_TEXT_COLOR = 0x12c244,
    FN_TEXT_WIDTH = 0x12d54c,
    STR_SKIP = 0x18f8e0, // "r# SKIP". 앞 글자는 글 모양 표시다.
    TEXT_MAX = 100,
    TEXT_WHITE = 0xffffff,
    OUTLINE = 1,
    LINE_H = 9,
    RIGHT_MARGIN = 2,
    ITEM_DY = -9, // 원본 그림은 그리는 y를 기준으로 위아래 8줄쯤에 걸친다.
    MAP_DY = 0,
    HINT_CAP = 128,
};

static uint32_t guest_text(int slot) {
    static uint32_t buf[3];
    if (!buf[slot]) buf[slot] = heap_alloc(HINT_CAP);
    return buf[slot];
}

static void draw_right(uint32_t g, int right, int y, int slot, const char *euc) {
    uint32_t s = guest_text(slot);
    cpu_t *c = &cpu;
    memcpy(c->m + s, euc, strlen(euc) + 1);
    int w = (int)(CALL(FN_TEXT_WIDTH, s, TEXT_MAX, 0) & 0xff);
    CALL(FN_DRAW_TEXT_COLOR, g, right - w, y, s, TEXT_MAX, TEXT_WHITE, 0, OUTLINE);
}

void f_444cc(cpu_t *c) {
    uint32_t hud = got(GOT_HUD);
    int frame = (int8_t)R8(hud + HUD_SOFT_KEY_FRAME);
    char item[HINT_CAP], map[HINT_CAP];
    if ((frame != 0 && frame != 1) || !gamepad_hint(GAMEPAD_HINT_ITEM, item, sizeof item) || !gamepad_hint(GAMEPAD_HINT_MAP, map, sizeof map)) {
        orig_f_444cc(c);
        return;
    }
    int width = (int)c->r[0], y = (int)c->r[1];
    uint8_t map_frame = R8(hud + HUD_MAP_KEY_FRAME);
    W8(hud + HUD_SOFT_KEY_FRAME, 0xff);
    W8(hud + HUD_MAP_KEY_FRAME, 0xff);
    orig_f_444cc(c);
    W8(hud + HUD_SOFT_KEY_FRAME, (uint8_t)frame);
    W8(hud + HUD_MAP_KEY_FRAME, map_frame);
    uint32_t g = R32(got(GOT_SCREEN));
    if (item[0]) draw_right(g, width - RIGHT_MARGIN, y + ITEM_DY, 0, item);
    if (map[0]) draw_right(g, width - RIGHT_MARGIN, y + MAP_DY, 1, map);
}

void f_2c50c(cpu_t *c) {
    char hint[HINT_CAP - 1];
    uint32_t sp = c->SP;
    if (R32(sp) != STR_SKIP || !gamepad_hint(GAMEPAD_HINT_SKIP, hint, sizeof hint)) { orig_f_2c50c(c); return; }
    uint32_t s = guest_text(2);
    c->m[s] = c->m[STR_SKIP];
    memcpy(c->m + s + 1, hint, strlen(hint) + 1);
    W32(sp, s);
    orig_f_2c50c(c);
    W32(sp, STR_SKIP);
}
