// 타이틀 화면. 원본 0x183694를 옮겼다. 폭 240에서는 원본과 픽셀이 같다.
#include "restored.h"

enum {
    FN_DRAW_SPRITE = 0x1086f8,
    FN_DRAW_TEXT = 0x12c1ec,
    FN_DRAW_TEXT_COLOR = 0x12c244,
    FN_SET_TEXT_STYLE = 0x12d734,
    FN_GET_SPRITE = 0x173d68,
    // 아래 두 함수의 이름은 호출 위치로 붙인 임시 이름이다.
    FN_MENU_RESET = 0x17c734,
    FN_MENU_ACTIVE = 0x17c9a4,
    FN_LOGO_DRAW = 0x17ead0,
    FN_LOGO_STEP = 0x17f520,
    FN_SKY_BACK = 0x18396c,
    FN_SKY_FRONT = 0x183a8c,
    FN_TEXT_COLOR = 0x18d06c,
    SKY_TILE_W = 320,
    WHITE = 0xffffff,
};

static uint32_t outline_color(void) {
    cpu_t *c = &cpu;
    return CALL(FN_TEXT_COLOR, 4, 90, 120, R32(R32(got(0x28)) + 88));
}

static void step_scroll(uint32_t scroll, int speed) {
    cpu_t *c = &cpu;
    int a = (int)R32(scroll) - speed, b = (int)R32(scroll + 4) - speed;
    W32(scroll, a < -SKY_TILE_W ? 0 : a);
    W32(scroll + 4, b < 0 ? SKY_TILE_W : b);
}

void f_83694(cpu_t *c) {
    uint32_t screen = got(0x2c), back = got(0x1858), front = got(0x185c);
    uint32_t g = R32(screen);
    int w = (int)R32(screen + 72), h = (int)R32(screen + 76), cx = w >> 1, cy = h >> 1;
    int left = cx - SKY_TILE_W / 2, top = cy - SKY_TILE_W / 2;

    CALL(FN_FILL_RECT, g, 0, 0, w, h, 0, 0, 0);

    // 원본은 타일 두 장만 그려 폭이 240을 넘으면 틈이 생긴다. 남는 폭을 같은 간격으로 채운다.
    int back_x0 = left + (int)R32(back), back_x1 = left + (int)R32(back + 4);
    int front_x0 = left + (int)R32(front), front_x1 = left + (int)R32(front + 4);
    for (int x = back_x0 - SKY_TILE_W; x + SKY_TILE_W > 0; x -= SKY_TILE_W) CALL(FN_SKY_BACK, x, top);
    for (int x = back_x1 + SKY_TILE_W; x < w; x += SKY_TILE_W) CALL(FN_SKY_BACK, x, top);

    uint32_t front_y = CALL(FN_SKY_BACK, back_x0, top);
    CALL(FN_SKY_FRONT, front_x0, front_y);
    front_y = CALL(FN_SKY_BACK, back_x1, top);
    CALL(FN_SKY_FRONT, front_x1, front_y);

    for (int x = front_x0 - SKY_TILE_W; x + SKY_TILE_W > 0; x -= SKY_TILE_W) CALL(FN_SKY_FRONT, x, front_y);
    for (int x = front_x1 + SKY_TILE_W; x < w; x += SKY_TILE_W) CALL(FN_SKY_FRONT, x, front_y);

    uint32_t logo = got(0x1624);
    CALL(FN_LOGO_DRAW, g, logo, 0, 0, logo + 106, cx, cy, 0, 0, 0);
    CALL(FN_LOGO_STEP, logo, 0, 0);

    uint32_t subtitle = got(0x1860);
    uint32_t color = outline_color();
    CALL(FN_DRAW_TEXT_COLOR, g, cx, cy + 25, subtitle, 100, WHITE, color, 1);
    color = outline_color();
    CALL(FN_DRAW_TEXT_COLOR, g, cx, cy + 24, subtitle, 100, WHITE, color, 1);

    uint32_t s = CALL(FN_GET_SPRITE, R32(got(0x1d8)), 1);
    CALL(FN_DRAW_SPRITE, g, w - 1, 1, R32(s), R32(s + 4), R32(s + 8), R32(s + 12), R32(s + 16), R32(s + 20),
         R32(s + 24), R32(s + 28), R32(s + 32), R32(s + 36), 33);

    CALL(FN_SET_TEXT_STYLE, 18, 41, 83);
    CALL(FN_DRAW_TEXT, g, cx, 2, got(0x1864), 100, 1);
    color = outline_color();
    CALL(FN_DRAW_TEXT_COLOR, g, cx, h - 10, got(0x1868), 100, WHITE, color, 1);

    // 깜빡이는 안내 문구. 11프레임 중 5프레임만 그린다.
    if ((CALL(FN_MENU_ACTIVE, 5) & 0xff) == 0 && R8(got(0xb94)) != 0) {
        uint32_t blink = got(0x1854);
        if ((int8_t)R8(blink) <= 4) {
            color = outline_color();
            CALL(FN_DRAW_TEXT_COLOR, g, cx, cy + ((h - cy - 45) >> 1) + 28, got(0x186c), 100, WHITE, color, 1);
        }
        uint8_t next = R8(blink) + 1;
        W8(blink, (int8_t)next > 10 ? 0 : next);
    }

    CALL(FN_MENU_RESET);

    step_scroll(back, 1);
    step_scroll(front, 3);
}
