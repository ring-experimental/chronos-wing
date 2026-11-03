// 게임패드 입력. 게임에는 터치 패드와 같은 키 코드로 넘기고, 설정 창에서는 고정 배치로 메뉴를 움직인다.
// 맵핑을 잘못 바꿔도 설정 창을 다룰 수 있도록 메뉴 조작은 맵핑을 따르지 않는다.
#include "host.h"
#include "ui.h"
#include "gamepad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ime_utf8_to_euckr(const char *utf8, char *out, int cap);
bool ime_active(void);
bool ime_event(const SDL_Event *e);

enum { TRIGGER_L = SDL_GAMEPAD_BUTTON_COUNT, TRIGGER_R, INPUT_COUNT, NO_INPUT = -1 };
enum { ACT_KEY, ACT_SETTINGS, ACT_AUTO, ACT_LOG };
enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT, DIR_COUNT }; // ACTIONS의 앞 네 줄과 순서가 같다.
enum {
    A_UP, A_DOWN, A_LEFT, A_RIGHT, A_OK, A_MENU, A_SKILL7, A_SKILL9, A_ITEM, A_MAP,
    A_LSK, A_SAVE, A_SLOT1, A_SLOT3, A_SLOT0, A_SETTINGS, A_LOG, A_AUTO, ACTION_COUNT,
};
enum {
    AXIS_ON = 16000, AXIS_OFF = 10000, MAX_PADS = 8,
    NAV_DELAY_MS = 400, NAV_REPEAT_MS = 120,
    KEY_CLEAR = -16, TEXT_CAP = 1024,
};

static const struct { const char *id, *name; int kind, code, def; } ACTIONS[ACTION_COUNT] = {
    [A_UP] = {"up", "위", ACT_KEY, -1, SDL_GAMEPAD_BUTTON_DPAD_UP},
    [A_DOWN] = {"down", "아래", ACT_KEY, -2, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    [A_LEFT] = {"left", "왼쪽", ACT_KEY, -3, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    [A_RIGHT] = {"right", "오른쪽", ACT_KEY, -4, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
    [A_OK] = {"ok", "확인, 스킬 5", ACT_KEY, -5, SDL_GAMEPAD_BUTTON_SOUTH},
    [A_MENU] = {"menu", "메뉴, 취소", ACT_KEY, KEY_CLEAR, SDL_GAMEPAD_BUTTON_EAST},
    [A_SKILL7] = {"skill7", "스킬 7", ACT_KEY, '7', SDL_GAMEPAD_BUTTON_WEST},
    [A_SKILL9] = {"skill9", "스킬 9", ACT_KEY, '9', SDL_GAMEPAD_BUTTON_NORTH},
    [A_ITEM] = {"item", "소지품", ACT_KEY, '*', SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    [A_MAP] = {"map", "미니맵, 스킵", ACT_KEY, '#', SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
    [A_LSK] = {"lsk", "시스템 메뉴", ACT_KEY, -6, SDL_GAMEPAD_BUTTON_START},
    // L3는 이동 중에 스틱을 누르다 잘못 들어가기 쉬워 저장은 R3에 둔다. 예전 설정의 "rsk" 줄은 읽지 않는다.
    [A_SAVE] = {"save", "저장", ACT_KEY, -7, SDL_GAMEPAD_BUTTON_RIGHT_STICK},
    [A_SLOT1] = {"slot1", "단축키 1", ACT_KEY, '1', NO_INPUT},
    [A_SLOT3] = {"slot3", "단축키 3", ACT_KEY, '3', NO_INPUT},
    [A_SLOT0] = {"slot0", "단축키 0", ACT_KEY, '0', NO_INPUT},
    [A_SETTINGS] = {"settings", "설정 창", ACT_SETTINGS, 0, SDL_GAMEPAD_BUTTON_BACK},
    [A_LOG] = {"log", "대사 기록", ACT_LOG, 0, TRIGGER_L},
    [A_AUTO] = {"auto", "자동 진행", ACT_AUTO, 0, TRIGGER_R},
};

typedef enum { SOURCE_KEYBOARD, SOURCE_TOUCH, SOURCE_PAD } source_t;

static int binds[ACTION_COUNT];
static SDL_Gamepad *pads[MAX_PADS];
static SDL_GamepadType pad_type = SDL_GAMEPAD_TYPE_STANDARD;
static source_t source;
static int generation;
static bool input_down[INPUT_COUNT], stick_held[DIR_COUNT], stick_sent[DIR_COUNT], trigger_held[2];
static int action_count[ACTION_COUNT];
static int nav_dir = -1;
static uint64_t nav_next;

static void replace_all(char *s, const char *from, const char *to);
static const char *skill_label(int action, const char *keyboard);

static source_t default_source(void) {
#ifdef __ANDROID__
    return SOURCE_TOUCH;
#else
    return SOURCE_KEYBOARD;
#endif
}

void gamepad_init(void) {
    source = default_source();
    gamepad_bind_reset();
}

void gamepad_bind_reset(void) {
    for (int i = 0; i < ACTION_COUNT; i++) binds[i] = ACTIONS[i].def;
    generation++;
}

void gamepad_bind(int action, int input) {
    if (action < 0 || action >= ACTION_COUNT) return;
    gamepad_release_all();
    for (int i = 0; i < ACTION_COUNT; i++) if (i != action && input != NO_INPUT && binds[i] == input) binds[i] = binds[action];
    binds[action] = input;
    generation++;
}

static int pad_count(void) {
    int n = 0;
    for (int i = 0; i < MAX_PADS; i++) if (pads[i]) n++;
    return n;
}

bool gamepad_connected(void) { return pad_count() > 0; }
int gamepad_generation(void) { return generation; }
int gamepad_action_count(void) { return ACTION_COUNT; }
const char *gamepad_action_name(int a) { return ACTIONS[a].name; }
const char *gamepad_config_id(int a) { return ACTIONS[a].id; }

static bool type_is_playstation(void) {
    return pad_type == SDL_GAMEPAD_TYPE_PS3 || pad_type == SDL_GAMEPAD_TYPE_PS4 || pad_type == SDL_GAMEPAD_TYPE_PS5;
}

static bool type_is_nintendo(void) {
    return pad_type >= SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO && pad_type <= SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR;
}

static const char *input_label(int input) {
    int family = type_is_playstation() ? 1 : type_is_nintendo() ? 2 : 0;
    switch (input) {
    case NO_INPUT: return "없음";
    case TRIGGER_L: { static const char *n[] = {"LT", "L2", "ZL"}; return n[family]; }
    case TRIGGER_R: { static const char *n[] = {"RT", "R2", "ZR"}; return n[family]; }
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: { static const char *n[] = {"LB", "L1", "L"}; return n[family]; }
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: { static const char *n[] = {"RB", "R1", "R"}; return n[family]; }
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: { static const char *n[] = {"LS", "L3", "LS"}; return n[family]; }
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: { static const char *n[] = {"RS", "R3", "RS"}; return n[family]; }
    case SDL_GAMEPAD_BUTTON_BACK: { static const char *n[] = {"BACK", "SHARE", "-"}; return n[family]; }
    case SDL_GAMEPAD_BUTTON_START: { static const char *n[] = {"START", "OPTIONS", "+"}; return n[family]; }
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return "D패드 위";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "D패드 아래";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "D패드 왼쪽";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "D패드 오른쪽";
    case SDL_GAMEPAD_BUTTON_MISC1: return "추가 버튼";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1: return "P1";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1: return "P2";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2: return "P3";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2: return "P4";
    case SDL_GAMEPAD_BUTTON_TOUCHPAD: return "터치패드";
    case SDL_GAMEPAD_BUTTON_SOUTH: case SDL_GAMEPAD_BUTTON_EAST: case SDL_GAMEPAD_BUTTON_WEST: case SDL_GAMEPAD_BUTTON_NORTH:
        switch (SDL_GetGamepadButtonLabelForType(pad_type, (SDL_GamepadButton)input)) {
        case SDL_GAMEPAD_BUTTON_LABEL_A: return "A";
        case SDL_GAMEPAD_BUTTON_LABEL_B: return "B";
        case SDL_GAMEPAD_BUTTON_LABEL_X: return "X";
        case SDL_GAMEPAD_BUTTON_LABEL_Y: return "Y";
        case SDL_GAMEPAD_BUTTON_LABEL_CROSS: return "X";
        case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE: return "○";
        case SDL_GAMEPAD_BUTTON_LABEL_SQUARE: return "□";
        case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE: return "△";
        default: return "?";
        }
    default: return "?";
    }
}

const char *gamepad_action_key(int a) { return input_label(binds[a]); }

const char *gamepad_config_value(int a) {
    int b = binds[a];
    if (b == NO_INPUT) return "none";
    if (b == TRIGGER_L) return "lefttrigger";
    if (b == TRIGGER_R) return "righttrigger";
    return SDL_GetGamepadStringForButton((SDL_GamepadButton)b);
}

void gamepad_config_read(const char *id, const char *value) {
    for (int a = 0; a < ACTION_COUNT; a++) {
        if (strcmp(ACTIONS[a].id, id)) continue;
        int input = !strcmp(value, "none") ? NO_INPUT : !strcmp(value, "lefttrigger") ? TRIGGER_L : !strcmp(value, "righttrigger") ? TRIGGER_R : (int)SDL_GetGamepadButtonFromString(value);
        if (input == SDL_GAMEPAD_BUTTON_INVALID || input == SDL_GAMEPAD_BUTTON_GUIDE) return;
        gamepad_bind(a, input);
        return;
    }
}

static void fire(int action, bool down) {
    switch (ACTIONS[action].kind) {
    case ACT_SETTINGS: if (down) ui_toggle_settings(); break;
    case ACT_AUTO: if (down) ui_toggle_auto(); break;
    case ACT_LOG: if (down) dialog_log_toggle(); break;
    default:
        // 입력 칸에서는 키보드 Backspace처럼 글자를 먼저 지운다.
        if (ACTIONS[action].code == KEY_CLEAR && ime_active()) {
            SDL_Event b;
            SDL_zero(b);
            b.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
            b.key.scancode = SDL_SCANCODE_BACKSPACE;
            b.key.down = down;
            ime_event(&b);
        } else host_key(down ? 2 : 3, ACTIONS[action].code);
    }
}

static void action_set(int action, bool down) {
    if (down) { if (action_count[action]++ == 0) fire(action, true); }
    else if (action_count[action] > 0 && --action_count[action] == 0) fire(action, false);
}

void gamepad_release_all(void) {
    for (int i = 0; i < INPUT_COUNT; i++) input_down[i] = false;
    for (int i = 0; i < DIR_COUNT; i++) stick_sent[i] = false;
    for (int i = 0; i < ACTION_COUNT; i++) if (action_count[i] > 0) { action_count[i] = 0; fire(i, false); }
    nav_dir = -1;
}

static void nav_press(int dir) {
    ui_pad_nav(dir == DIR_UP ? UI_NAV_UP : dir == DIR_DOWN ? UI_NAV_DOWN : dir == DIR_LEFT ? UI_NAV_LEFT : UI_NAV_RIGHT);
    nav_dir = dir;
    nav_next = SDL_GetTicks() + NAV_DELAY_MS;
}

static void note_pad_used(SDL_JoystickID which) {
    SDL_Gamepad *p = SDL_GetGamepadFromID(which);
    SDL_GamepadType type = p ? SDL_GetGamepadType(p) : pad_type;
    if (type != pad_type) { pad_type = type; generation++; }
    source = SOURCE_PAD;
}

static int dpad_dir(int input) {
    switch (input) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return DIR_UP;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return DIR_DOWN;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return DIR_LEFT;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return DIR_RIGHT;
    default: return -1;
    }
}

static void input_set(int input, bool down) {
    if (down && ui_pad_capture(input)) return;
    if (ui_modal()) {
        int dir = dpad_dir(input);
        if (!down) { if (dir >= 0 && dir == nav_dir) nav_dir = -1; return; }
        if (dir >= 0) nav_press(dir);
        else if (input == SDL_GAMEPAD_BUTTON_SOUTH) ui_pad_nav(UI_NAV_OK);
        else if (input == SDL_GAMEPAD_BUTTON_EAST) ui_pad_nav(UI_NAV_BACK);
        else if (input == binds[A_SETTINGS]) ui_pad_nav(UI_NAV_BACK);
        return;
    }
    if (savefile_dialog_open()) return;
    if (down == input_down[input]) return;
    input_down[input] = down;
    for (int a = 0; a < ACTION_COUNT; a++) if (binds[a] == input) { action_set(a, down); break; }
}

static void stick_set(int dir, bool held) {
    if (stick_held[dir] == held) return;
    stick_held[dir] = held;
    if (held) {
        if (ui_pad_capturing()) return;
        if (ui_modal()) nav_press(dir);
        else if (!savefile_dialog_open()) { stick_sent[dir] = true; action_set(A_UP + dir, true); }
    } else {
        if (dir == nav_dir) nav_dir = -1;
        if (stick_sent[dir]) { stick_sent[dir] = false; action_set(A_UP + dir, false); }
    }
}

static bool past_threshold(bool held, int value) { return value >= (held ? AXIS_OFF : AXIS_ON); }

static void axis_event(const SDL_GamepadAxisEvent *e) {
    int v = e->value;
    switch (e->axis) {
    case SDL_GAMEPAD_AXIS_LEFTX:
        stick_set(DIR_LEFT, past_threshold(stick_held[DIR_LEFT], -v));
        stick_set(DIR_RIGHT, past_threshold(stick_held[DIR_RIGHT], v));
        break;
    case SDL_GAMEPAD_AXIS_LEFTY:
        stick_set(DIR_UP, past_threshold(stick_held[DIR_UP], -v));
        stick_set(DIR_DOWN, past_threshold(stick_held[DIR_DOWN], v));
        break;
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: {
        int t = e->axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
        bool held = past_threshold(trigger_held[t], v);
        if (held == trigger_held[t]) break;
        trigger_held[t] = held;
        if (held) note_pad_used(e->which);
        input_set(t ? TRIGGER_R : TRIGGER_L, held);
        break;
    }
    default: break;
    }
    if (e->axis == SDL_GAMEPAD_AXIS_LEFTX || e->axis == SDL_GAMEPAD_AXIS_LEFTY)
        if (v >= AXIS_ON || v <= -AXIS_ON) note_pad_used(e->which);
}

static void pad_removed(SDL_JoystickID which) {
    SDL_Gamepad *p = SDL_GetGamepadFromID(which);
    for (int i = 0; i < MAX_PADS; i++) if (pads[i] == p) { SDL_CloseGamepad(p); pads[i] = NULL; }
    gamepad_release_all();
    for (int i = 0; i < DIR_COUNT; i++) stick_held[i] = false;
    trigger_held[0] = trigger_held[1] = false;
    if (!gamepad_connected() && source == SOURCE_PAD) source = default_source();
    generation++;
}

static void pad_added(SDL_JoystickID which) {
    for (int i = 0; i < MAX_PADS; i++) if (!pads[i]) {
        pads[i] = SDL_OpenGamepad(which);
        if (!pads[i]) { SDL_Log("게임패드 열기 실패: %s", SDL_GetError()); return; }
        pad_type = SDL_GetGamepadType(pads[i]);
        SDL_Log("게임패드 연결: %s", SDL_GetGamepadName(pads[i]));
        generation++;
        return;
    }
}

bool gamepad_event(const SDL_Event *e) {
    switch (e->type) {
    case SDL_EVENT_GAMEPAD_ADDED: pad_added(e->gdevice.which); return true;
    case SDL_EVENT_GAMEPAD_REMOVED: pad_removed(e->gdevice.which); return true;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN: case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        bool down = e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        if (e->gbutton.button == SDL_GAMEPAD_BUTTON_GUIDE) return true; // 시스템이 쓰는 버튼이다.
        if (down) note_pad_used(e->gbutton.which);
        input_set(e->gbutton.button, down);
        return true;
    }
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: axis_event(&e->gaxis); return true;
    case SDL_EVENT_KEY_DOWN: // Android BACK과 음량 키는 키보드로 치지 않는다.
        if (e->key.scancode != SDL_SCANCODE_AC_BACK && e->key.scancode != SDL_SCANCODE_VOLUMEUP && e->key.scancode != SDL_SCANCODE_VOLUMEDOWN) source = SOURCE_KEYBOARD;
        return false;
    case SDL_EVENT_FINGER_DOWN:
        if (e->tfinger.touchID != SDL_MOUSE_TOUCHID) source = SOURCE_TOUCH;
        return false;
    default: return false;
    }
}

// 게임 글꼴에 없는 PS 기호는 게임 글에 쓰지 않는 ASCII 자리에 그려 넣어 쓴다. 비트맵은 work/tools/hnf2ttf.py와 같다.
enum { GLYPH_SQUARE = '`', GLYPH_TRIANGLE = '^', ENG_HEADER = 6, ENG_FIRST = 0x20, ENG_H = 8, MASK_H = 10 };
static const uint8_t SQUARE_ROWS[ENG_H] = {0, 0, 0xf8, 0x88, 0x88, 0x88, 0xf8, 0};
static const uint8_t TRIANGLE_ROWS[ENG_H] = {0, 0, 0x20, 0x50, 0x50, 0x88, 0xf8, 0};

// UTF-8 표시 글을 게임 글(EUC-KR)로 바꾼다.
static int game_text(const char *utf8, char *out, int cap) {
    char t[TEXT_CAP];
    snprintf(t, sizeof t, "%s", utf8);
    char square[] = {GLYPH_SQUARE, 0}, triangle[] = {GLYPH_TRIANGLE, 0};
    replace_all(t, "□", square);
    replace_all(t, "△", triangle);
    replace_all(t, "○", "O");
    return ime_utf8_to_euckr(t, out, cap);
}

// 본체 글꼴(8줄)과 외곽선 마스크(10줄, 본체를 한 칸씩 민 자리의 둘레)에 기호를 넣는다.
static void put_glyph(uint8_t *font, size_t size, int height, int ch, const uint8_t *rows) {
    size_t at = ENG_HEADER + (size_t)(ch - ENG_FIRST) * height;
    if (at + height > size) return;
    if (height == ENG_H) { memcpy(font + at, rows, ENG_H); return; }
    uint8_t shifted[MASK_H] = {0};
    for (int y = 0; y < ENG_H; y++) shifted[y + 1] = rows[y] >> 1;
    for (int y = 0; y < MASK_H; y++) {
        uint8_t around = 0;
        for (int d = -1; d <= 1; d++) if (y + d >= 0 && y + d < MASK_H) around |= (uint8_t)(shifted[y + d] | shifted[y + d] << 1 | shifted[y + d] >> 1);
        font[at + y] = around & (uint8_t)~shifted[y];
    }
}

// 스킬 표. DAT/SKL을 불러온 뒤에는 다시 읽지 않으므로 커맨드 글을 게임 메모리에서 바로 고친다. 이 글은 표시에만 쓰인다.
enum { SKILL_NAMES = 0x1abcba, SKILL_COUNT = 39, SKILL_STRIDE = 0x130, SKILL_CMD = 0xb8, SKILL_CMD_SIZE = 27, SKILL_CMDS = 4 };
static char skill_original[SKILL_COUNT][SKILL_CMDS][SKILL_CMD_SIZE];
static char skill_shown[SKILL_COUNT][SKILL_CMDS][SKILL_CMD_SIZE];
static bool skill_ready, skill_failed;
static int skill_generation = -1;
static source_t skill_source;

static bool skill_load(void) {
    char path[1200];
    snprintf(path, sizeof path, "%s/DAT/SKL", game_dir);
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    uint8_t d[8192];
    size_t n = fread(d, 1, sizeof d, f), p = 2;
    fclose(f);
    if (n < 2 || d[0] != SKILL_COUNT) return false;
    for (int r = 0; r < SKILL_COUNT; r++) {
        // [분류][이름 길이][이름][8바이트][설명 길이][설명][4바이트][커맨드 4개][5바이트]
        if (p + 2 > n) return false;
        p += 2 + d[p + 1] + 8;
        if (p >= n) return false;
        p += 1 + d[p] + 4;
        for (int k = 0; k < SKILL_CMDS; k++) {
            if (p >= n || d[p] >= SKILL_CMD_SIZE || p + 1 + d[p] > n) return false;
            memcpy(skill_original[r][k], d + p + 1, d[p]);
            p += 1 + d[p];
        }
        p += 5;
    }
    return p == n;
}

static void skill_rebuild(void) {
    char five[32], seven[32], nine[32];
    game_text(skill_label(A_OK, "W"), five, sizeof five);
    game_text(skill_label(A_SKILL7, "A"), seven, sizeof seven);
    game_text(skill_label(A_SKILL9, "D"), nine, sizeof nine);
    memset(skill_shown, 0, sizeof skill_shown);
    for (int r = 0; r < SKILL_COUNT; r++) for (int k = 0; k < SKILL_CMDS; k++) {
        const char *o = skill_original[r][k], *key = o[0] == '5' ? five : o[0] == '7' ? seven : o[0] == '9' ? nine : NULL;
        if (key && source != SOURCE_TOUCH && (!o[1] || o[1] == ' ')) snprintf(skill_shown[r][k], SKILL_CMD_SIZE, "%s%s", key, o + 1);
        else memcpy(skill_shown[r][k], o, SKILL_CMD_SIZE);
    }
}

static void skill_sync(void) {
    if (skill_failed) return;
    if (!skill_ready) {
        if (!skill_load()) { skill_failed = true; SDL_Log("스킬 표 형식이 달라 커맨드 글을 바꾸지 않습니다"); return; }
        skill_ready = true;
    }
    if (!cpu.m[SKILL_NAMES]) return; // 아직 불러오지 않았다.
    if (skill_generation != generation || skill_source != source) {
        skill_generation = generation;
        skill_source = source;
        skill_rebuild();
    }
    for (int r = 0; r < SKILL_COUNT; r++) for (int k = 0; k < SKILL_CMDS; k++) {
        uint8_t *field = cpu.m + SKILL_NAMES + r * SKILL_STRIDE + SKILL_CMD + k * SKILL_CMD_SIZE;
        if (memcmp(field, skill_shown[r][k], SKILL_CMD_SIZE)) memcpy(field, skill_shown[r][k], SKILL_CMD_SIZE);
    }
}

void gamepad_tick(void) {
    skill_sync();
    if (nav_dir < 0) return;
    if (!ui_modal()) { nav_dir = -1; return; }
    uint64_t now = SDL_GetTicks();
    if (now < nav_next) return;
    int dir = nav_dir;
    nav_press(dir);
    nav_next = now + NAV_REPEAT_MS;
}

// 조작 안내 글. 원본 형식은 [u8 개수][u16BE 길이][EUC-KR 글]의 반복이고 `|`, `q`는 색, `;`는 줄바꿈이다.
enum { HELP_BASIC = 11, HELP_SKILL = 12 };
#define EUC_KEY "\xc5\xb0" // 키

static const uint8_t *out_data;

static const char *skill_label(int action, const char *keyboard) {
    return source == SOURCE_PAD ? input_label(binds[action]) : keyboard;
}

static void basic_controls(char *out) {
    char t[TEXT_CAP], move[128];
    if (source == SOURCE_KEYBOARD) {
        snprintf(t, sizeof t, "|이동 q: 방향키;|조사/대화 q: SPACE;|스킬 q: W, A, D 조합;|소지품 사용 q: Z;|미니맵 모드 변환 q: C;|메뉴 q: BACKSPACE");
    } else {
        bool dpad = binds[A_UP] == SDL_GAMEPAD_BUTTON_DPAD_UP && binds[A_DOWN] == SDL_GAMEPAD_BUTTON_DPAD_DOWN && binds[A_LEFT] == SDL_GAMEPAD_BUTTON_DPAD_LEFT && binds[A_RIGHT] == SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
        if (dpad) snprintf(move, sizeof move, "D패드, 왼쪽 스틱");
        else snprintf(move, sizeof move, "왼쪽 스틱;  %s, %s, %s, %s", input_label(binds[A_UP]), input_label(binds[A_DOWN]), input_label(binds[A_LEFT]), input_label(binds[A_RIGHT]));
        snprintf(t, sizeof t, "|이동 q: %s;|조사/대화 q: %s;|스킬 q: %s, %s, %s 조합;|소지품 사용 q: %s;|미니맵 모드 변환 q: %s;|메뉴 q: %s",
            move, input_label(binds[A_OK]), input_label(binds[A_OK]), input_label(binds[A_SKILL7]), input_label(binds[A_SKILL9]),
            input_label(binds[A_ITEM]), input_label(binds[A_MAP]), input_label(binds[A_MENU]));
    }
    game_text(t, out, TEXT_CAP);
}

// 글 안의 from을 모두 to로 바꾼다.
static void replace_all(char *s, const char *from, const char *to) {
    char o[TEXT_CAP];
    int n = 0;
    size_t fl = strlen(from), tl = strlen(to);
    for (const char *p = s; *p && n < TEXT_CAP - 1;) {
        if (!strncmp(p, from, fl)) {
            if (n + (int)tl >= TEXT_CAP - 1) break;
            memcpy(o + n, to, tl);
            n += (int)tl;
            p += fl;
        } else o[n++] = *p++;
    }
    o[n] = 0;
    memcpy(s, o, (size_t)n + 1);
}

static void replace_key(char *s, const char *key, const char *utf8_label) {
    char label[64], from[8];
    int n = game_text(utf8_label, label, sizeof label - 3);
    memcpy(label + n, EUC_KEY, 3);
    snprintf(from, sizeof from, "%s" EUC_KEY, key);
    replace_all(s, from, label);
}

const uint8_t *gamepad_resource(const char *path, size_t *size) {
    size_t pl = strlen(path);
    bool help = pl >= 9 && !strcmp(path + pl - 9, "/tdf/HELP");
    bool tip = pl >= 8 && !strcmp(path + pl - 8, "/tdf/TIP");
    bool body_font = pl >= 11 && !strcmp(path + pl - 11, "/ft/eng5x14");
    bool mask_font = pl >= 11 && !strcmp(path + pl - 11, "/ft/eng7x10");
    if (body_font || mask_font) {
        // 글꼴은 처음에 한 번 읽히므로 장치와 상관없이 언제나 넣는다.
        FILE *ff = fopen(path, "rb");
        if (!ff) return NULL;
        fseek(ff, 0, SEEK_END);
        long fl = ftell(ff);
        fseek(ff, 0, SEEK_SET);
        uint8_t *font = fl > 0 ? malloc((size_t)fl) : NULL;
        if (!font || fread(font, 1, (size_t)fl, ff) != (size_t)fl) { free(font); fclose(ff); return NULL; }
        fclose(ff);
        int height = body_font ? ENG_H : MASK_H;
        put_glyph(font, (size_t)fl, height, GLYPH_SQUARE, SQUARE_ROWS);
        put_glyph(font, (size_t)fl, height, GLYPH_TRIANGLE, TRIANGLE_ROWS);
        free((void *)out_data);
        out_data = font;
        *size = (size_t)fl;
        return out_data;
    }
    if ((!help && !tip) || source == SOURCE_TOUCH) return NULL;

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *in = malloc((size_t)len);
    bool ok = in && fread(in, 1, (size_t)len, f) == (size_t)len;
    fclose(f);
    if (!ok || len < 1) { free(in); return NULL; }

    uint8_t *out = malloc((size_t)len + 2 * TEXT_CAP);
    if (!out) { free(in); return NULL; }
    const char *five = skill_label(A_OK, "W"), *seven = skill_label(A_SKILL7, "A"), *nine = skill_label(A_SKILL9, "D");
    int count = in[0];
    size_t ip = 1, op = 1;
    out[0] = in[0];
    for (int i = 0; i < count; i++) {
        if (ip + 2 > (size_t)len) goto bad;
        size_t l = (size_t)in[ip] << 8 | in[ip + 1];
        if (ip + 2 + l > (size_t)len || l >= TEXT_CAP) goto bad;
        char t[TEXT_CAP];
        memcpy(t, in + ip + 2, l);
        t[l] = 0;
        ip += 2 + l;
        if (help && i == HELP_BASIC) basic_controls(t);
        else if (help && i == HELP_SKILL) {
            char a[64], b[64], c[64], euc[TEXT_CAP];
            game_text(five, a, sizeof a);
            game_text(seven, b, sizeof b);
            game_text(nine, c, sizeof c);
            snprintf(euc, sizeof euc, "%s, %s, %s " EUC_KEY, a, b, c);
            replace_all(t, "5, 7, 9 " EUC_KEY, euc);
        } else if (tip) {
            replace_key(t, "5", five);
            replace_key(t, "7", seven);
        }
        size_t nl = strlen(t);
        if (op + 2 + nl > (size_t)len + 2 * TEXT_CAP) goto bad;
        out[op++] = (uint8_t)(nl >> 8);
        out[op++] = (uint8_t)nl;
        memcpy(out + op, t, nl);
        op += nl;
    }
    free(in);
    free((void *)out_data);
    out_data = out;
    *size = op;
    return out_data;
bad:
    SDL_Log("조작 안내 글 형식이 달라 원본을 씁니다: %s", path);
    free(in);
    free(out);
    return NULL;
}

// 게임 화면의 키 안내(필드 오른쪽 아래 소지품/미니맵, 컷신의 스킵). 패드로 조작 중이 아니면 false로 원본을 그대로 둔다.
bool gamepad_hint(int kind, char *out, int cap) {
    if (source != SOURCE_PAD) return false;
    char t[TEXT_CAP] = "";
    switch (kind) {
    case GAMEPAD_HINT_ITEM: case GAMEPAD_HINT_MAP: {
        int b = binds[kind == GAMEPAD_HINT_ITEM ? A_ITEM : A_MAP];
        if (b != NO_INPUT) snprintf(t, sizeof t, "%s %s", input_label(b), kind == GAMEPAD_HINT_ITEM ? "ITEM" : "MAP");
        break;
    }
    case GAMEPAD_HINT_SKIP: {
        static const struct { int action; const char *word; } PARTS[] = {{A_LOG, "LOG"}, {A_AUTO, "AUTO"}, {A_MAP, "SKIP"}};
        for (size_t i = 0; i < sizeof PARTS / sizeof PARTS[0]; i++) {
            if (binds[PARTS[i].action] == NO_INPUT) continue;
            size_t n = strlen(t);
            snprintf(t + n, sizeof t - n, "%s%s %s", n ? "  " : "", input_label(binds[PARTS[i].action]), PARTS[i].word);
        }
        break;
    }
    default: return false;
    }
    game_text(t, out, cap);
    return true;
}

// 시험용 가상 패드. vpad xbox|ps, vbtn <이름> [up], vaxis <이름> <값>, vdetach. 이름은 SDL 매핑 이름이다.
static SDL_JoystickID virtual_pad;

bool gamepad_ctrl(const char *line) {
    char a[32], b[32];
    if (!strncmp(line, "vpad ", 5)) {
        SDL_VirtualJoystickDesc d;
        SDL_INIT_INTERFACE(&d);
        bool ps = !strcmp(line + 5, "ps");
        d.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        d.naxes = SDL_GAMEPAD_AXIS_COUNT;
        d.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        d.vendor_id = ps ? 0x054c : 0x045e;
        d.product_id = ps ? 0x0ce6 : 0x028e;
        d.name = ps ? "virtual ps" : "virtual xbox";
        virtual_pad = SDL_AttachVirtualJoystick(&d);
        if (!virtual_pad) SDL_Log("가상 패드 실패: %s", SDL_GetError());
        return true;
    }
    if (!strcmp(line, "vdetach")) { if (virtual_pad) SDL_DetachVirtualJoystick(virtual_pad); virtual_pad = 0; return true; }
    SDL_Joystick *j = virtual_pad ? SDL_GetJoystickFromID(virtual_pad) : NULL;
    if (sscanf(line, "vbtn %31s %31s", a, b) >= 1 && !strncmp(line, "vbtn ", 5)) {
        bool up = sscanf(line, "vbtn %31s %31s", a, b) == 2;
        if (j) SDL_SetJoystickVirtualButton(j, SDL_GetGamepadButtonFromString(a), !up);
        return true;
    }
    int v;
    if (sscanf(line, "vaxis %31s %d", a, &v) == 2) {
        if (j) SDL_SetJoystickVirtualAxis(j, SDL_GetGamepadAxisFromString(a), (Sint16)v);
        return true;
    }
    return false;
}
