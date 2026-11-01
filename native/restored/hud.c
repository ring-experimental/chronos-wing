// 필드 상태창과 미니맵을 화면 위쪽에 그린다. 터치 패드가 하단을 가리기 때문이다.
// 원본 0x141d00을 감싸고, 개발자모드에서 상단을 골랐을 때만 y 좌표를 바꿔 같은 순서로 그린다.
// 문양 그림은 아래쪽 경계가 잘려 있어(원본은 화면 아래에 붙는다) 세로로 뒤집어 그 경계를 화면 위에 붙인다.
// 그림 오른쪽에 바 틀의 왼쪽 끝이 함께 그려져 있어 바와 글 줄도 세로로 맞바꾼다. HP가 위인 것은 유지한다.
// 바의 틀, 끝 그림, 채움도 세로로 뒤집어 사선을 문양의 바 끝과 맞춘다(좌상에서 우하로).
#include "restored.h"

void orig_f_41d00(cpu_t *c);
void orig_f_44284(cpu_t *c);
bool devmode_hud_top(void);

enum {
    GOT_SCREEN = 0x2c,
    GOT_HUD = 0xab8,
    GOT_API = 0x88,
    GOT_EXP_FORMAT = 0xae4,
    GOT_EXP_LABEL = 0xae8,
    GOT_EXP_COLON = 0xaec,
    GOT_MAP = 0x13c,
    API_SPRINTF = 4,
    FN_CALL_R3 = 0x18d06c,
    FN_DIV = 0x1885f0,
    FN_PLAYER = 0x147434,
    FN_HIDDEN = 0x141cbc,
    FN_SOFT_KEYS = 0x1444cc,
    FN_NOTICES = 0x144c48,
    GOT_HUD_SPRITES = 0xaf4,
    FN_GET_SPRITE = 0x173d68,
    FN_DRAW_SPRITE_REGION = 0x1087e4,
    SPRITE_EMBLEM = 5,
    SPRITE_BAR_END = 6,
    SPRITE_H = 4,
    SPRITE_W = 8,
    SPRITE_FLIP_V = 4,
    SPRITE_OPAQUE = 0xff,
    ANCHOR_TOP_LEFT = 9,
    BAR_X = 0x1a,
    BAR_FILL_ROWS = 4,
    BAR_END_FLIP_Y = 1, // 끝 그림은 틀 8줄 중 위 7줄이라 뒤집으면 한 줄 내려야 한다.
    UPPER_FRAME_Y = 3,
    LOWER_FRAME_Y = 12,
    BAR_INSET = 2, // 틀 위에서 채움까지
    TEXT_Y = 21,
    POTION_Y = 22,
    TRAIL_HP_Y = 0x10, // 0x1441f8이 인자에 더하는 값
    FN_CUR_EXP = 0x12ec04,
    FN_NEXT_EXP = 0x12ebfc,
    FN_PERCENT = 0x1292c4,
    FN_PERCENT_TEXT = 0x12c17c,
    FN_DRAW_TEXT_COLOR = 0x12c244,
    FN_TEXT_WIDTH = 0x12d54c,
    FN_POTION = 0x142068,
    FN_HP = 0x12ec34,
    FN_MAX_HP = 0x12ec58,
    FN_MP = 0x12eca0,
    FN_MAX_MP = 0x12ecc4,
    FN_BAR = 0x142464,
    FN_BAR_TRAIL = 0x1441f8,
    FN_MAP_BLINK = 0x1684ac,
    FN_MINIMAP = 0x178a50,
    SCREEN_W = 0x48,
    SCREEN_H = 0x4c,
    HUD_HEIGHT = 35,
    HUD_SLIDE_STEP = 7,
    HUD_SLIDE_MAX = 5,
    HUD_MAP_MODE = 2,
    HUD_MAP_ON = 3,
    HUD_VISIBLE = 0x44,
    HUD_SLIDE = 0x49,
    HUD_CAPTION = 0x4b,
    PLAYER_HP_POTIONS = 0x120,
    PLAYER_MP_POTIONS = 0x121,
    MAP_W = 0x140,
    MAP_H = 0x144,
    MAP_GRID_ROWS = 0x135,
    MAP_FADE = 0x164,
    MAP_CAPTION = 0x279,
    MAP_Y = 50, // 위쪽 패드 줄(y 30까지)과 대상 정보(y 49까지) 아래.
    GRID_BOTTOM_MARGIN = 0x19,
    TEXT_WHITE = 0xffffff,
};

// 0x178a50의 격자 지도는 y를 화면 높이에서 직접 계산한다.
static int minimap_grid_height(uint32_t map) {
    cpu_t *c = &cpu;
    int rows = (int8_t)R8(map + MAP_GRID_ROWS);
    return rows * 12 - (rows - (rows >> 31) >> 1) * 9;
}

#define SPRITE_ARGS(s) R32(s), R32((s) + 4), R32((s) + 8), R32((s) + 12), R32((s) + 16), R32((s) + 20), R32((s) + 24), R32((s) + 28), R32((s) + 32), R32((s) + 36)

static void draw_sprite_flipped(uint32_t g, int x, int y, uint32_t s) {
    cpu_t *c = &cpu;
    CALL(FN_DRAW_SPRITE_REGION, g, x, y, R32(s + SPRITE_W), R32(s + SPRITE_H), SPRITE_ARGS(s),
         0, 0, SPRITE_FLIP_V, 0, SPRITE_OPAQUE, ANCHOR_TOP_LEFT);
}

// 바 하나의 틀. 0x142204의 사각형 줄을 세로로 뒤집어 표로 옮겼다.
static void draw_bar_frame(uint32_t g, int y, int width) {
    enum { DARK = 0x13, DARK_B = 0x15, LIGHT = 0xff, INNER = 0x1f };
    static const struct { signed char extra; unsigned char color; } ROWS[] = {
        {0, DARK}, {0, LIGHT}, {0, INNER}, {1, INNER}, {2, INNER}, {3, INNER}, {0, LIGHT}, {0, DARK},
    };
    for (int i = 0; i < (int)(sizeof ROWS / sizeof ROWS[0]); i++) {
        int color = ROWS[i].color, blue = color == DARK ? DARK_B : color;
        CALL(FN_FILL_RECT, g, BAR_X, y + i, width + ROWS[i].extra, 1, color, color, blue);
    }
}

// 0x142204의 세로 대칭. 뒤집힌 문양의 바 끝이 긴 쪽이 위로 오므로 위 바가 원본 아래 바의 모양을 쓴다.
static void draw_frame(uint32_t g, int y) {
    cpu_t *c = &cpu;
    uint32_t sprites = R32(got(GOT_HUD_SPRITES));
    draw_sprite_flipped(g, 0, y, CALL(FN_GET_SPRITE, sprites, SPRITE_EMBLEM));
    draw_bar_frame(g, y + UPPER_FRAME_Y, 0x40);
    draw_sprite_flipped(g, 0x5a, y + UPPER_FRAME_Y + BAR_END_FLIP_Y, CALL(FN_GET_SPRITE, sprites, SPRITE_BAR_END));
    draw_bar_frame(g, y + LOWER_FRAME_Y, 0x49);
    draw_sprite_flipped(g, 99, y + LOWER_FRAME_Y + BAR_END_FLIP_Y, CALL(FN_GET_SPRITE, sprites, SPRITE_BAR_END));
}

static bool flip_bar_fill;

// 바 채움 네 줄. 원본은 아래 줄로 갈수록 왼쪽으로 밀리고 색은 0, 1, 2, 0번이다.
void f_44284(cpu_t *c) {
    if (!flip_bar_fill) { orig_f_44284(c); return; }
    static const unsigned char COLORS[BAR_FILL_ROWS] = {0, 2, 1, 0};
    uint32_t colors = c->r[0], width = c->r[3], g = R32(got(GOT_SCREEN));
    int x = (int)c->r[1], y = (int)c->r[2];
    for (int i = 0; i < BAR_FILL_ROWS; i++) {
        uint32_t rgb = colors + COLORS[i] * 12;
        CALL(FN_FILL_RECT, g, x - (BAR_FILL_ROWS - 1 - i), y + i, width, 1, R32(rgb), R32(rgb + 4), R32(rgb + 8));
    }
}

static void draw_minimap(uint32_t screen, uint32_t hud, uint32_t map, int width) {
    cpu_t *c = &cpu;
    uint32_t g = R32(screen);
    int fade = (int8_t)R8(map + MAP_FADE);
    if (!R8(hud + HUD_MAP_ON) && fade != -1 && fade != -2) return;

    int caption = (int8_t)R8(hud + HUD_CAPTION);
    int map_w = (int)R32(map + MAP_W);
    uint32_t blink = CALL(FN_MAP_BLINK, 6) & 0xff;
    uint32_t real_height = R32(screen + SCREEN_H);
    W32(screen + SCREEN_H, MAP_Y + minimap_grid_height(map) + GRID_BOTTOM_MARGIN);
    CALL(FN_MINIMAP, g, (uint32_t)(int8_t)R8(hud + HUD_MAP_MODE), width - map_w - 1, MAP_Y, blink);
    W32(screen + SCREEN_H, real_height);

    if (caption <= 0) return;
    uint32_t text = map + MAP_CAPTION;
    int text_w = (int)CALL(FN_TEXT_WIDTH, text, 0x1b, 1);
    if (text_w < map_w) text_w = map_w;
    int x = width - (text_w >> 1) - 1, slide = 0;
    if (caption < HUD_SLIDE_MAX) slide = (int)CALL(FN_DIV, width - x, 5) * (HUD_SLIDE_MAX - caption);
    CALL(FN_DRAW_TEXT_COLOR, g, x + slide, MAP_Y + (int)R32(map + MAP_H) + 1, text, 100, TEXT_WHITE, 0, 1);
    W8(hud + HUD_CAPTION, caption - 1);
}

void f_41d00(cpu_t *c) {
    if (!devmode_hud_top()) { orig_f_41d00(c); return; }
    if (CALL(FN_HIDDEN) & 0xff) return;

    static uint32_t percent, percent_text, exp_text;
    if (!percent) {
        percent = heap_alloc(12);
        percent_text = heap_alloc(100);
        exp_text = heap_alloc(100);
    }
    uint32_t screen = got(GOT_SCREEN), hud = got(GOT_HUD), player = CALL(FN_PLAYER);
    uint32_t g = R32(screen);
    int width = (int)R32(screen + SCREEN_W), height = (int)R32(screen + SCREEN_H);
    int slide = (int8_t)R8(hud + HUD_SLIDE);
    int y = slide * HUD_SLIDE_STEP - HUD_HEIGHT;

    CALL(FN_SOFT_KEYS, width, height + 1 - slide * 2);
    CALL(FN_NOTICES);
    draw_frame(g, y);

    int cur_exp = (int)CALL(FN_CUR_EXP, player), next_exp = (int)CALL(FN_NEXT_EXP, player);
    CALL(FN_PERCENT, percent, CALL(FN_DIV, cur_exp * 100, next_exp));
    CALL(FN_PERCENT_TEXT, percent, (uint32_t)(int8_t)R8(percent + 10), percent_text);
    CALL(FN_CALL_R3, exp_text, got(GOT_EXP_FORMAT), percent_text, R32(R32(got(GOT_API)) + API_SPRINTF));
    CALL(FN_DRAW_TEXT_COLOR, g, 0x1c, y + TEXT_Y, got(GOT_EXP_LABEL), 100, TEXT_WHITE, 0, 0);
    CALL(FN_DRAW_TEXT_COLOR, g, 0x20, y + TEXT_Y, got(GOT_EXP_COLON), 100, TEXT_WHITE, 0, 0);
    CALL(FN_DRAW_TEXT_COLOR, g, 0x24, y + TEXT_Y, exp_text, 100, TEXT_WHITE, 0, 0);

    CALL(FN_POTION, 0, 0x3a, y + POTION_Y, (uint32_t)(int8_t)R8(player + PLAYER_HP_POTIONS), 1);
    CALL(FN_POTION, 1, 0x42, y + POTION_Y, (uint32_t)(int8_t)R8(player + PLAYER_MP_POTIONS), 1);
    flip_bar_fill = true;
    uint32_t hp = CALL(FN_HP, player), max_hp = CALL(FN_MAX_HP, player);
    CALL(FN_BAR, 0, 10, y + UPPER_FRAME_Y + BAR_INSET, 0x53, 4, hp, max_hp);
    uint32_t mp = CALL(FN_MP, player), max_mp = CALL(FN_MAX_MP, player);
    CALL(FN_BAR, 1, 0x13, y + LOWER_FRAME_Y + BAR_INSET, 0x53, 4, mp, max_mp);
    CALL(FN_BAR_TRAIL, 0, y + UPPER_FRAME_Y + BAR_INSET - TRAIL_HP_Y);
    flip_bar_fill = false;

    draw_minimap(screen, hud, got(GOT_MAP), width);

    if (R8(hud + HUD_VISIBLE)) {
        if (slide < HUD_SLIDE_MAX) W8(hud + HUD_SLIDE, slide + 1);
    } else if (slide > 0) W8(hud + HUD_SLIDE, slide - 1);
}
