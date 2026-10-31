// 월드맵 그리기. 원본 0x1805f8, 0x180674를 옮겼다. 폭이 지도 이하이면 원본과 같다.
#include "restored.h"

enum {
    FN_MARK_RECT = 0x106958, // 이름은 호출 위치로 붙인 임시 이름이다.
    FN_DRAW_IMAGE = 0x108724,
    FN_MAP_OFFSET_X = 0x17c41c,
    FN_MAP_OFFSET_Y = 0x17c448,
    FN_MODE = 0x17fc58,
    FN_ENTER_EFFECT = 0x180978,
    FN_LEAVE_EFFECT = 0x180718,
    FN_MAP_LABELS = 0x180bf4,
    FN_MAP_POINTS = 0x180e40,
    FN_MAP_SHIP = 0x181138,
    FN_MAP_HUD = 0x1815c4,
    FN_MAP_INPUT = 0x181b40,
    FN_MAP_FIRST_DRAW = 0x180674,
    MODE_ENTER = 1,
    MODE_LEAVE = 13,
    MAP_CAM_X = 0x144, // 지명, 배 아이콘도 이 값을 빼서 화면 좌표를 만든다.
    MAP_CAM_Y = 0x148,
};

// 화면이 지도보다 넓으면 지도 양옆 폭을 돌려준다. 그렇지 않으면 0이다.
static int map_margin(int w) {
    cpu_t *c = &cpu;
    int map_w = (int)R32(got(0x1688) + 8);
    return map_w > 0 && w > map_w ? (w - map_w) / 2 : 0;
}

static void fill_margins(uint32_t g, int w, int h, int left) {
    cpu_t *c = &cpu;
    int map_w = (int)R32(got(0x1688) + 8);
    CALL(FN_FILL_RECT, g, 0, 0, left, h, 0, 0, 0);
    CALL(FN_FILL_RECT, g, left + map_w, 0, w - left - map_w, h, 0, 0, 0);
}

void f_80674(cpu_t *c) {
    uint32_t g = c->r[0], screen = got(0x2c), image = got(0x1688), map = got(0x18c);
    int w = (int)R32(screen + 72), h = (int)R32(screen + 76);
    int left = map_margin(w), draw_w = left ? (int)R32(image + 8) : w;

    CALL(FN_MARK_RECT, g, 0, 0, 2, 2);
    CALL(FN_MARK_RECT, g, w - 2, h - 2, 2, 2);

    // 지도는 원본 소스 좌표에서 반복되어 폭이 지도보다 넓으면 같은 대륙이 두 번 보인다.
    // 카메라를 지도가 가운데 오는 값으로 고정해 한 장만 그리고 양옆은 비운다.
    if (left) {
        W32(map + MAP_CAM_X, -left);
        fill_margins(g, w, h, left);
    }

    uint32_t x = CALL(FN_MAP_OFFSET_X), y = CALL(FN_MAP_OFFSET_Y);
    int cam_x = (int)R32(map + MAP_CAM_X) + left;
    CALL(FN_DRAW_IMAGE, g, x + left, y, draw_w, h, R32(image), R32(image + 4), R32(image + 8), R32(image + 12),
         R32(image + 16), R32(image + 20), R32(image + 24), R32(image + 28), R32(image + 32), R32(image + 36),
         cam_x, R32(map + MAP_CAM_Y), 0, 0);
}

void f_805f8(cpu_t *c) {
    uint32_t screen = got(0x2c), g = R32(screen);
    int mode = CALL(FN_MODE) & 0xff;

    if (mode == MODE_LEAVE || mode == MODE_ENTER) {
        CALL(mode == MODE_LEAVE ? FN_LEAVE_EFFECT : FN_ENTER_EFFECT);
        // 줌과 모자이크는 화면 전체에 그려 지도가 여백으로 번진다. 효과가 끝난 뒤 다시 지운다.
        int w = (int)R32(screen + 72), left = map_margin(w);
        if (left) fill_margins(g, w, (int)R32(screen + 76), left);
    } else {
        CALL(FN_MAP_INPUT, (uint32_t)(int32_t)(int8_t)R8(got(0x1690) + 4));
        CALL(FN_MAP_FIRST_DRAW, g);
        CALL(FN_MAP_LABELS, g);
        CALL(FN_MAP_POINTS, g);
        CALL(FN_MAP_SHIP, g);
        CALL(FN_MAP_HUD, g);
    }
    c->r[0] = 0;
}
