// 개발자모드. 메인메뉴의 빈 상태값 8을 환경설정과 같은 창으로 쓰고, 보상과 전투 함수에 배율을 건다.
// 원본 0x1453bc, 0x1454f4, 0x145690, 0x145a88, 0x14609c를 감싸고 0x145b5c는 옮겼다. 값은 저장 폴더의 DEVMODE에 남는다.
#include "panels.h"
#include <stdio.h>

void orig_f_453bc(cpu_t *c);
void orig_f_454f4(cpu_t *c);
void orig_f_45690(cpu_t *c);
void orig_f_45a88(cpu_t *c);
void orig_f_4609c(cpu_t *c);
void orig_f_3eac(cpu_t *c);
void orig_f_41bc(cpu_t *c);
void orig_f_3ebc(cpu_t *c);
void orig_f_2eca0(cpu_t *c);
void orig_f_2edac(cpu_t *c);
void orig_f_2ed70(cpu_t *c);
void orig_f_4ab54(cpu_t *c);
void orig_f_4ad54(cpu_t *c);
void orig_f_264c(cpu_t *c);

// 게임 글꼴이 읽는 EUC-KR이다.
#define BAE "\xb9\xe8" // 배
#define T_MENU "\xb0\xb3\xb9\xdf\xc0\xda\xb8\xf0\xb5\xe5" // 개발자모드
#define T_EXP "\xb0\xe6\xc7\xe8\xc4\xa1" // 경험치
#define T_GOLD "\xb0\xf1\xb5\xe5" // 골드
#define T_RARE "\xb7\xb9\xbe\xee\x20\xb5\xe5\xb6\xf8" // 레어 드랍
#define T_ATTACK "\xb0\xf8\xb0\xdd\xb7\xc2" // 공격력
#define T_SKILL "\xbd\xba\xc5\xb3\x20\xc6\xf7\xc0\xce\xc6\xae" // 스킬 포인트
#define T_TRAIT "\xc6\xaf\xbc\xba\x20\xc6\xf7\xc0\xce\xc6\xae" // 특성 포인트
#define T_GOD "\xb9\xab\xc0\xfb" // 무적
#define T_MP "\x4d\x50\x20\xb9\xab\xc7\xd1" // MP 무한
#define T_HUD "\xbb\xf3\xc5\xc2\xc3\xa2" // 상태창
#define T_BOTTOM "\xc7\xcf\xb4\xdc" // 하단
#define T_TOP "\xbb\xf3\xb4\xdc" // 상단

enum {
    GOT_SCREEN = 0x2c,
    GOT_API = 0x28,
    GOT_MENU_STATE = 0xb8c, // 첫 바이트가 상태, +4가 tdf/MENU 글 표
    GOT_MENU_ITEM = 0xb84,  // 첫 바이트가 고른 항목. 상태값과 같은 번호다.
    GOT_CURSOR = 0x1e0,
    FN_TEXT = 0x148558,
    FN_TEXT_LEN = 0x148564,
    FN_TEXT_WIDTH = 0x12d54c,
    FN_DRAW_TEXT = 0x12c1ec,
    FN_DRAW_TEXT_COLOR = 0x12c244,
    FN_SET_COLOR = 0x12d734,
    FN_CALL_R3 = 0x18d06c,
    FN_DRAW_CURSOR = 0x11f514,
    FN_CURSOR_RESET = 0x11f4cc,
    FN_CURSOR_TICK = 0x11f4d8,
    FN_ANIM_IDLE = 0x11f244,
    FN_PANEL = 0x172aa8,
    FN_LAYOUT = 0x140570,
    FN_LEAVING = 0x140908,
    FN_LEAVE = 0x140930,
    FN_BACK_TO_MENU = 0x1454ac,
    FN_EXPERT_OPEN = 0x148290,
    FN_PLAYER = 0x147434,
    FN_MAX_MP = 0x12ecc4,
    FN_ADD_SKILL_POINT = 0x12edac,
    FN_ADD_TRAIT_POINT = 0x12ed70,
    FN_RARE_TRAIT = 0x101dec,
    STATE_DEV = 8,
    ITEM_DEV = 8,
    ITEM_EXPERT = 1,
    ITEM_MUSIC = 14, // 원본 상태표(0~13) 밖의 번호다. 0x1453bc에 넘기기 전에 상태 8로 바꾼다.
    ITEM_REPLAY = 15,
    PANEL_W = 0xb0,
    PANEL_H = 0xa0,
    ROW_H = 14,
    LABEL_X = 0x3e,
    VALUE_X = 0x82,
    KEY_CLEAR = -16,
    PLAYER_EXP = 0xf4,
    PLAYER_LEVEL = 0x101,
    PLAYER_MP = 0x114,
    PLAYER_NP = 0x218,
    MAX_LEVEL = 99,
    MAX_NP = 1999999999, // 0x1041bc는 이 값을 넘는 증가를 통째로 버린다.
    MAX_POINTS = 120,    // 포인트는 부호 있는 바이트다.
    DROP_SLOTS = 5,
    DROP_SLOT_SIZE = 12,
    DROP_CHANCE = 0x144,
    MAX_DAMAGE = 1000000000,
    RET_HIT_CRITICAL = 0x135abc,
    RET_HIT_NUMBER = 0x135acc,
    RET_DOT_NUMBER = 0x14ff96,
    RARE_TRAIT_UNIT = 700, // 0x14ab54가 특성 값에 곱하는 수. 판정 범위는 0~32767이다.
};

enum { ROW_EXP, ROW_GOLD, ROW_RARE, ROW_ATTACK, ROW_SKILL, ROW_TRAIT, ROW_GOD, ROW_MP, ROW_HUD, ROW_COUNT };
static const char *const LABELS[ROW_COUNT] = {T_EXP, T_GOLD, T_RARE, T_ATTACK, T_SKILL, T_TRAIT, T_GOD, T_MP, T_HUD};
static const int RATES[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 50, 100};
#define RATE_COUNT ((int)(sizeof RATES / sizeof RATES[0]))

static signed char values[ROW_COUNT] = {[ROW_HUD] = 1}; // 배율 줄은 RATES의 번호, 토글 줄은 0 또는 1이다. 상태창은 1이 상단이다.
static int selected_row;
static const menu_panel *active; // 상태 8에서 열린 창. NULL이면 개발자모드다.
static bool loaded;

static bool is_toggle(int row) { return row >= ROW_GOD; }
static int value_count(int row) { return is_toggle(row) ? 2 : RATE_COUNT; }

static void config_path(char *out, size_t cap) { snprintf(out, cap, "%s/DEVMODE", save_dir); }

static void load_values(void) {
    if (loaded) return;
    loaded = true;
    char path[1100];
    config_path(path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    signed char saved[ROW_COUNT];
    int n = (int)fread(saved, 1, ROW_COUNT, f); // 줄이 늘기 전 파일은 짧다. 없는 줄은 기본값을 쓴다.
    for (int i = 0; i < n; i++)
        if (saved[i] >= 0 && saved[i] < value_count(i)) values[i] = saved[i];
    fclose(f);
}

static void save_values(void) {
    char path[1100];
    config_path(path, sizeof path);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fwrite(values, 1, ROW_COUNT, f);
    fclose(f);
}

static int rate(int row) { load_values(); return RATES[values[row]]; }
static bool enabled(int row) { load_values(); return values[row] != 0; }
bool devmode_hud_top(void) { return enabled(ROW_HUD); }

// 글은 게스트 메모리에 있어야 그리기 함수에 넘길 수 있다. 한 번 만들고 계속 쓴다.
uint32_t menu_guest_text(const char *s) {
    enum { MAX_TEXTS = 64 };
    static struct { const char *s; uint32_t addr; } cache[MAX_TEXTS];
    for (int i = 0; i < MAX_TEXTS; i++) {
        if (cache[i].s == s) return cache[i].addr;
        if (!cache[i].s) {
            uint32_t n = (uint32_t)strlen(s) + 1, addr = heap_alloc(n);
            memcpy(cpu.m + addr, s, n);
            cache[i].s = s;
            cache[i].addr = addr;
            return addr;
        }
    }
    return 0;
}

uint32_t menu_color(int r, int g, int b) {
    cpu_t *c = &cpu;
    return CALL(FN_CALL_R3, r, g, b, R32(R32(got(GOT_API)) + 0x58));
}

static const char *value_text(int row) {
    static const char *const RATE_TEXT[RATE_COUNT] = {"1" BAE, "2" BAE, "3" BAE, "4" BAE, "5" BAE, "6" BAE, "7" BAE, "8" BAE, "9" BAE, "10" BAE, "20" BAE, "50" BAE, "100" BAE};
    if (row == ROW_HUD) return values[row] ? T_TOP : T_BOTTOM;
    if (is_toggle(row)) return values[row] ? "ON" : "OFF";
    return RATE_TEXT[values[row]];
}

// 0x147c10과 같은 줄 그리기.
static void draw_row(int row, int x, int y) {
    cpu_t *c = &cpu;
    uint32_t g = R32(got(GOT_SCREEN));
    const char *label = LABELS[row], *value = value_text(row);
    uint32_t label_addr = menu_guest_text(label), value_addr = menu_guest_text(value);
    int label_len = (int)strlen(label), value_len = (int)strlen(value);
    uint32_t label_color = menu_color(0xe8, 0xd6, 0xab);
    CALL(FN_DRAW_TEXT_COLOR, g, x + LABEL_X, y, label_addr, label_len, label_color, 0, 1);
    if (row == selected_row) CALL(FN_SET_COLOR, 0xff, 0xff, 0xff);
    else CALL(FN_SET_COLOR, 0xc6, 0x9c, 0x6e);
    CALL(FN_DRAW_TEXT, g, x + VALUE_X, y + 1, value_addr, value_len, 1);
    if (row == selected_row) {
        int width = (int)CALL(FN_TEXT_WIDTH, value_addr, value_len, 0);
        CALL(FN_DRAW_CURSOR, got(GOT_CURSOR), width, x + VALUE_X, y);
    }
}

void menu_draw_frame(int x, int y) {
    cpu_t *c = &cpu;
    CALL(FN_PANEL, x, y, PANEL_W, PANEL_H);
}

void menu_leave(void) {
    cpu_t *c = &cpu;
    CALL(FN_LEAVE, 1);
}

static void draw_panel(int x, int y) {
    cpu_t *c = &cpu;
    int top = (int)R32(got(GOT_SCREEN) + 0x4c) / 2 - 0x3e;
    menu_draw_frame(x, y);
    for (int i = 0; i < ROW_COUNT; i++) draw_row(i, x, top + i * ROW_H);
}

static void change_value(int step) {
    int n = value_count(selected_row);
    values[selected_row] = (signed char)((values[selected_row] + step + n) % n);
}

// 키 배치는 환경설정(0x147d7c)과 같다.
static void panel_key(int key) {
    cpu_t *c = &cpu;
    switch (key) {
    case KEY_CLEAR: CALL(FN_LEAVE, 1); break;
    case -5: case -4: case 53: case 54: change_value(1); break;
    case -3: case 52: change_value(-1); break;
    case -2: case 56: selected_row = (selected_row + 1) % ROW_COUNT; break;
    case -1: case 50: selected_row = (selected_row + ROW_COUNT - 1) % ROW_COUNT; break;
    }
}

static int menu_state(void) {
    cpu_t *c = &cpu;
    return (int8_t)R8(got(GOT_MENU_STATE));
}

bool replay_take_return(void);

void f_453bc(cpu_t *c) {
    int8_t next = (int8_t)c->r[0];
    // 스토리 다시보기가 끝나 메뉴로 돌아오면 목록을 다시 연다.
    if ((next == 9 || next == 11) && replay_take_return()) {
        next = ITEM_REPLAY;
        W8(got(GOT_MENU_ITEM), ITEM_REPLAY);
    }
    active = next == ITEM_MUSIC ? &music_panel : next == ITEM_REPLAY ? &replay_panel : NULL;
    if (active) c->r[0] = STATE_DEV;
    orig_f_453bc(c);
    if (menu_state() != STATE_DEV) return;
    if (active) { active->open(); return; }
    load_values();
    selected_row = 0;
    CALL(FN_CURSOR_RESET, got(GOT_CURSOR));
}

void f_454f4(cpu_t *c) {
    orig_f_454f4(c);
    if (menu_state() != STATE_DEV) return;
    uint32_t layout = CALL(FN_LAYOUT);
    int x = (int)R32(layout + 0x60), y = (int)R32(layout + 0x64);
    if (active) active->draw(x, y);
    else draw_panel(x, y);
}

void f_45690(cpu_t *c) {
    if (menu_state() != STATE_DEV) { orig_f_45690(c); return; }
    if (CALL(FN_LEAVING) & 0xff) {
        if (active) { if (active->close) active->close(); }
        else save_values();
        CALL(FN_BACK_TO_MENU);
        return;
    }
    orig_f_45690(c);
    CALL(FN_CURSOR_TICK, got(GOT_CURSOR));
    if (active && active->tick) active->tick();
}

void f_45a88(cpu_t *c) {
    if (menu_state() != STATE_DEV) { orig_f_45a88(c); return; }
    int key = (int)c->r[0];
    if (!(CALL(FN_ANIM_IDLE, got(GOT_MENU_ITEM)) & 0xff)) return;
    if (active) active->key(key);
    else panel_key(key);
}

// 메인메뉴 좌우 이동. 플레이리스트와 스토리 다시보기를 네트워크 다음에, 개발자모드를 환경설정 다음에 끼운다. 열리지 않은 EXPERT는 건너뛴다(원본과 같다).
void f_4609c(cpu_t *c) {
    static const signed char ORDER[] = {0, 1, 2, ITEM_MUSIC, ITEM_REPLAY, 3, 4, ITEM_DEV, 5, 6, 7};
    enum { COUNT = sizeof ORDER };
    int key = (int)c->r[0], step = 0;
    if (key == -4 || key == -2 || key == 54 || key == 56) step = 1;
    else if (key == -3 || key == -1 || key == 50 || key == 52) step = -1;
    if (!step) { orig_f_4609c(c); return; }
    uint32_t item = got(GOT_MENU_ITEM);
    int current = (int8_t)R8(item), pos = 0;
    for (int i = 0; i < COUNT; i++)
        if (ORDER[i] == current) pos = i;
    bool expert_open = (CALL(FN_EXPERT_OPEN) & 0xff) != 0;
    do pos = (pos + step + COUNT) % COUNT;
    while (ORDER[pos] == ITEM_EXPERT && !expert_open);
    W8(item, ORDER[pos]);
}

// 메인메뉴 항목 이름 그리기. tdf/MENU의 8번은 캐릭터 복구라 추가한 항목의 글은 따로 넘긴다.
void f_45b5c(cpu_t *c) {
    uint32_t screen = got(GOT_SCREEN), table = got(GOT_MENU_STATE) + 4;
    int height = (int)R32(screen + 0x4c), half = height >> 1;
    int x = (int)R32(screen + 0x48) >> 1, y = half + ((height - half - 0x2d) >> 1) + 0x1c;
    int item = (int8_t)R8(got(GOT_MENU_ITEM));
    uint32_t text, len;
    const char *added = item == ITEM_DEV ? T_MENU : item == ITEM_MUSIC ? music_panel.label : item == ITEM_REPLAY ? replay_panel.label : NULL;
    if (added) {
        text = menu_guest_text(added);
        len = (uint32_t)strlen(added);
    } else {
        text = CALL(FN_TEXT, table, item);
        len = CALL(FN_TEXT_LEN, table, item);
    }
    uint32_t width = CALL(FN_TEXT_WIDTH, text, len, 1);
    CALL(FN_DRAW_CURSOR, got(GOT_CURSOR), width, x, y);
    uint32_t outline = menu_color(4, 0x5a, 0x78);
    CALL(FN_DRAW_TEXT_COLOR, R32(screen), x, y, text, len, 0xffffff, outline, 1);
}

static int exp_needed(int level) { return level * (level * level - level + 80); } // 0x12eb28

// 만렙까지 남은 경험치. 원본 레벨업 반복은 만렙을 검사하지 않아 배율이 크면 레벨이 넘친다.
static int64_t exp_to_max_level(uint32_t player) {
    cpu_t *c = &cpu;
    int level = (int8_t)R8(player + PLAYER_LEVEL);
    int64_t left = -(int64_t)(int32_t)R32(player + PLAYER_EXP);
    for (int l = level; l < MAX_LEVEL; l++) left += exp_needed(l);
    return left > 0 ? left : 0;
}

void f_3eac(cpu_t *c) {
    int amount = (int)c->r[0], mul = rate(ROW_EXP);
    if (mul <= 1 || amount <= 0) { orig_f_3eac(c); return; }
    uint32_t player = CALL(FN_PLAYER);
    int level = (int8_t)R8(player + PLAYER_LEVEL);
    int64_t scaled = (int64_t)amount * mul, limit = exp_to_max_level(player);
    c->r[0] = (uint32_t)(scaled < limit ? scaled : limit);
    orig_f_3eac(c);
    // 원본은 한 번에 여러 레벨이 올라도 포인트를 한 번만 준다.
    int gained = (int8_t)R8(player + PLAYER_LEVEL) - level;
    for (int i = 1; i < gained; i++) {
        CALL(FN_ADD_SKILL_POINT, 0);
        CALL(FN_ADD_TRAIT_POINT, 0);
    }
}

void f_41bc(cpu_t *c) {
    int amount = (int)c->r[0], mul = rate(ROW_GOLD);
    if (mul > 1 && amount > 0) {
        int current = (int)R32(CALL(FN_PLAYER) + PLAYER_NP);
        int64_t scaled = (int64_t)amount * mul, limit = (int64_t)MAX_NP - current;
        if (scaled > limit) scaled = limit;
        if (scaled <= 0) scaled = amount; // 이미 상한이면 원본이 그대로 버린다.
        c->r[0] = (uint32_t)scaled;
    } else c->r[0] = (uint32_t)amount;
    orig_f_41bc(c);
}

void f_3ebc(cpu_t *c) {
    if ((int)c->r[0] < 0 && enabled(ROW_GOD)) return;
    orig_f_3ebc(c);
}

// 스킬 사용 판정과 HUD가 모두 이 함수로 현재 MP를 읽는다.
void f_2eca0(cpu_t *c) {
    uint32_t player = c->r[0];
    if (enabled(ROW_MP)) {
        W32(player + PLAYER_MP, CALL(FN_MAX_MP, player));
        c->r[0] = player;
    }
    orig_f_2eca0(c);
}

// 인자 0이 포인트 지급, 0이 아니면 사용이다.
static void add_points(cpu_t *c, void (*orig)(cpu_t *), int row) {
    int mul = (int8_t)c->r[0] == 0 ? rate(row) : 1;
    uint32_t arg0 = c->r[0];
    orig(c);
    for (int i = 1; i < mul && (int)c->r[0] < MAX_POINTS; i++) {
        c->r[0] = arg0;
        orig(c);
    }
}

void f_2edac(cpu_t *c) { add_points(c, orig_f_2edac, ROW_SKILL); }
void f_2ed70(cpu_t *c) { add_points(c, orig_f_2ed70, ROW_TRAIT); }

// 칸마다 `판정값 <= 확률 + 특성*700`이다. 특성까지 포함한 최종 확률에 배율을 건다.
void f_4ab54(cpu_t *c) {
    int mul = rate(ROW_RARE);
    if (mul <= 1) { orig_f_4ab54(c); return; }
    uint32_t monster = c->r[0], arg1 = c->r[1];
    int bonus = (int)CALL(FN_RARE_TRAIT) * RARE_TRAIT_UNIT, saved[DROP_SLOTS];
    for (int i = 0; i < DROP_SLOTS; i++) {
        uint32_t chance = monster + DROP_CHANCE + i * DROP_SLOT_SIZE;
        saved[i] = (int)R32(chance);
        W32(chance, (saved[i] + bonus) * mul - bonus);
    }
    c->r[0] = monster;
    c->r[1] = arg1;
    orig_f_4ab54(c);
    for (int i = 0; i < DROP_SLOTS; i++) W32(monster + DROP_CHANCE + i * DROP_SLOT_SIZE, saved[i]);
}

static int scale_damage(int damage, int mul) {
    int64_t scaled = (int64_t)damage * mul;
    return scaled > MAX_DAMAGE ? MAX_DAMAGE : scaled < -MAX_DAMAGE ? -MAX_DAMAGE : (int)scaled;
}

// 몬스터 HP 변화. 음수가 피해다.
void f_4ad54(cpu_t *c) {
    int delta = (int)c->r[1], mul = rate(ROW_ATTACK);
    if (mul > 1 && delta < 0) c->r[1] = (uint32_t)scale_damage(delta, mul);
    orig_f_4ad54(c);
}

// 피해 숫자 표시. 타격(0x135a04)과 지속 피해(0x14fef0)는 HP와 따로 값을 넘기므로 복귀 주소로 골라 같은 배율을 건다.
void f_264c(cpu_t *c) {
    int mul = rate(ROW_ATTACK);
    if (mul > 1) {
        uint32_t from = c->LR & ~1u;
        if (from == RET_HIT_CRITICAL) c->r[2] = (uint32_t)scale_damage((int)c->r[2], mul);
        else if (from == RET_HIT_NUMBER || from == RET_DOT_NUMBER) c->r[3] = (uint32_t)scale_damage((int)c->r[3], mul);
    }
    orig_f_264c(c);
}
