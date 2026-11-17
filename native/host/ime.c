// 입력기(표 4)와 TextComponent(표 9). 입력 칸(0x156ffc, 0x104eec)은 원본이 폰 키패드로 글자를 만들지만, 플랫폼 입력기(SDL 텍스트 입력)의 글자를 받아 칸에 직접 넣는다.
// 칸은 하나다: +4 글자(EUC-KR), +0x84 커서 뒤 글자 사본, +0x104 커서, +0x106 최대 바이트, +0x108 조합 폭(0x104ae4, 0x104eec). 그리기는 게임이 한다(0x104d3e).
#include "host.h"
#include <SDL3/SDL.h>
#include <string.h>

enum {
    FIELD_OPEN = 0x20d758 + 0x1cf, // 0x156ffc가 켜고 0x157078이 끈다
    FIELD = 0x20d638, F_TEXT = 4, F_TAIL = 0x84, F_POS = 0x104, F_MAX = 0x106, F_COMPOSE_W = 0x108, TEXT_CAP = 0x80,
    NAME_FIELD_MAX = 14, NAME_BYTES = 10, MODE_HANGUL = 3, COMPOSE_MAX = 32, FIELD_MAX = 8,
    KEY_OK = -5, KEY_CLEAR = -16, KEY_UP = -1, KEY_DOWN = -2, KEY_LEFT = -3, KEY_RIGHT = -4,
};

// 유니코드 음절(AC00~) 순서로 KS X 1001 수록 여부. 수록 음절은 유니코드 순으로 EUC-KR B0A1부터 놓인다.
static const uint8_t KS_SYLLABLES[] = {
    147,7,255,62,17,176,3,19,1,40,16,17,0,0,147,5,123,30,17,176,3,151,1,59,18,17,160,0,147,149,107,48,
    81,176,2,17,1,50,48,17,176,2,17,1,10,48,121,184,6,19,1,48,16,0,128,0,19,1,11,16,17,0,0,147,
    3,43,16,0,0,0,147,5,107,116,81,176,35,19,1,59,48,16,0,0,0,0,0,112,17,176,3,19,0,41,16,17,
    128,33,1,0,0,48,21,176,14,3,1,48,48,0,0,2,17,1,35,16,0,0,0,19,129,107,16,16,0,3,19,1,
    19,16,17,48,0,1,0,0,48,85,184,34,0,0,0,48,17,176,2,151,7,251,58,17,176,3,19,1,33,0,0,0,
    0,27,13,59,56,17,176,3,19,1,51,17,1,0,0,19,5,43,28,17,0,1,0,0,0,16,17,176,0,19,1,42,
    48,25,176,2,1,0,16,16,0,0,0,17,1,3,48,16,48,2,19,7,107,20,17,0,0,19,5,43,116,249,184,143,
    19,1,59,16,0,0,0,0,0,0,112,217,176,74,19,1,59,16,17,0,3,17,0,0,48,89,177,42,17,1,0,16,
    0,0,1,17,1,11,16,0,0,0,19,1,43,16,0,0,1,1,0,32,16,17,160,2,17,1,33,48,89,176,2,1,
    0,0,48,25,176,7,19,1,59,56,17,176,3,0,0,0,0,0,0,0,19,13,59,56,17,176,3,1,0,16,0,0,
    0,0,19,1,32,16,16,0,0,1,0,0,16,1,0,0,0,0,0,48,17,24,2,0,0,0,16,0,0,0,17,1,
    35,0,0,0,0,147,1,11,16,17,48,0,17,1,43,48,17,176,199,19,1,59,48,1,128,2,0,0,0,48,17,176,
    131,19,1,43,48,17,176,3,17,0,10,48,17,176,2,17,0,32,0,0,0,1,17,1,43,16,17,160,2,19,1,43,
    16,0,0,1,1,0,0,48,17,144,2,19,1,43,48,17,176,102,0,0,0,48,17,176,2,211,7,107,58,17,176,7,
    3,1,32,0,0,0,0,19,5,107,56,17,176,3,19,1,184,16,0,0,0,27,5,43,16,1,0,3,0,0,0,16,
    17,160,2,17,1,10,112,121,176,162,17,1,10,16,0,0,0,17,1,0,16,17,144,0,17,1,9,0,0,0,0,147,
    5,187,242,249,176,34,19,1,59,50,1,32,0,0,0,0,48,89,176,6,147,1,59,48,17,160,35,17,0,0,112,17,
    176,2,17,0,16,16,0,0,1,19,1,3,16,1,0,0,147,7,43,22,16,0,1,1,0,0,48,17,0,2,17,1,
    41,48,17,176,0,0,0,0,48,81,176,14,19,5,59,56,17,176,3,3,0,1,0,0,0,0,147,1,57,16,0,0,
    2,3,0,59,0,0,0,0,19,1,35,0,0,0,0,0,0,0,16,0,0,0,1,0,32,48,17,144,2,0,0,0,
    0,0,0,0,0,0,0,16,0,0,2,17,1,3,0,0,0,0,19,1,43,176,121,176,35,19,1,59,48,17,176,2,
    17,1,33,240,217,176,67,19,1,59,48,17,176,3,17,1,32,112,81,176,34,19,1,32,16,17,144,1,17,1,11,48,
    17,176,2,147,1,171,22,0,0,1,19,1,33,48,17,176,2,3,1,41,48,49,176,2,0,0,0,48,25,184,66,27,
    1,51,56,17,48,3,0,0,32,0,0,0,0,19,5,51,16,17,0,0,0,0,0,0,1,0,0,147,5,35,48,1,
    0,1,1,0,16,16,17,48,0,1,0,0,48,17,48,2,1,0,16,16,0,0,0,17,0,0,0,0,0,2,19,133,
    3,16,17,16,0,19,1,43,48,119,184,99,19,1,59,48,145,176,162,17,1,2,48,123,240,87,19,1,43,112,209,240,
    227,17,1,27,48,113,185,10,19,1,59,48,1,144,2,19,1,43,48,17,176,2,19,7,43,48,17,48,3,19,1,35,
    48,17,176,2,19,1,171,48,17,180,254,17,1,9,48,113,184,71,211,5,123,48,17,176,3,83,1,33,16,17,0,0,
    19,5,107,48,17,176,2,17,1,51,16,0,0,0,19,5,235,56,16,160,2,1,0,48,16,17,176,2,19,0,32,48,
    113,176,2,1,0,16,16,0,0,0,19,1,11,16,17,16,0,19,1,43,0,0,0,0,147,5,107,54,149,176,3,19,
    1,59,16,1,0,2,0,0,0,48,17,176,3,1,0,32,16,0,0,1,0,0,0,48,17,176,10,3,1,16,16,0,
    0,1,17,1,3,0,0,0,2,19,1,35,16,0,0,3,0,0,0,16,0,0,0,1,0,0,16,0,144,2,0,0,
    0,48,17,48,134,83,1,123,48,17,176,3,81,1,33,0,0,0,0,19,1,59,48,17,176,2,17,0,16,16,1,0,
    2,19,1,43,16,17,0,2,0,0,0,16,17,176,2,1,0,1,48,17,176,2,1,0,16,16,1,0,0,17,1,43,
    16,17,16,2,19,1,43,0,0,0,0,147,3,43,48,17,176,2,19,1,59,48,0,0,2,0,0,0,48,25,176,3,
    19,1,43,16,17,176,3,1,0,0,48,17,176,2,19,1,33,16,0,0,2,1,1,0,16,0,0,0,19,1,43,16,
    17,0,2,1,0,32,48,17,176,2,17,1,1,48,17,48,2,0,0,0,48,17,176,2,19,3,59,48,17,176,3,1,
    0,32,0,0,0,0,19,5,59,48,17,176,2,17,0,16,16,1,0,0,19,1,43,20,1,0,0,1,0,0,16,1,
    128,2,1,0,0,48,17,176,2,1,0,16,16,0,0,0,19,1,35,16,17,16,2,147,5,11,16,17,48,0,19,1,
    43,112,81,176,35,19,1,59,48,0,0,0,0,0,0,48,17,176,3,19,1,43,16,17,48,3,1,1,10,48,17,176,
    2,1,0,32,0,0,0,0,17,0,0,16,17,160,0,147,5,43,16,0,0,2,0,0,0,16,17,144,0,17,1,41,
    16,17,176,0,0,0,0,48,17,176,2,19,33,43,48,17,176,3,1,0,32,0,0,0,0,19,5,43,48,17,176,2,
    19,1,59,16,17,32,0,19,33,43,50,17,128,2,19,0,40,48,17,160,2,17,1,10,48,17,146,2,17,1,33,48,
    17,0,2,19,1,43,48,17,144,2,211,3,43,18,17,48,2,19,1,43,0,
};

// 글자 하나를 EUC-KR로. 게임 글꼴에 없는 글자는 0을 돌려준다. 영문 소문자는 원본 칸처럼 대문자로 넣는다(0x104eec).
static int to_euckr(uint32_t u, char *o) {
    if (u >= 'a' && u <= 'z') u -= 0x20;
    if (u >= 0x20 && u < 0x7f) { o[0] = (char)u; return 1; }
    if (u >= 0x3131 && u <= 0x3163) { o[0] = (char)0xa4; o[1] = (char)(0xa1 + u - 0x3131); return 2; }
    if (u < 0xac00 || u > 0xd7a3) return 0;
    int idx = (int)u - 0xac00, rank = 0;
    if (!(KS_SYLLABLES[idx / 8] >> (idx % 8) & 1)) return 0;
    for (int i = 0; i < idx / 8; i++) rank += __builtin_popcount(KS_SYLLABLES[i]);
    rank += __builtin_popcount(KS_SYLLABLES[idx / 8] & ((1 << (idx % 8)) - 1));
    o[0] = (char)(0xb0 + rank / 94);
    o[1] = (char)(0xa1 + rank % 94);
    return 2;
}

static char *field_text(void) { return (char *)cpu.m + FIELD + F_TEXT; }

static uint32_t utf8_next(const char **p);

// UTF-8 글을 EUC-KR로 바꾼다. ASCII는 대소문자를 그대로 두고, 게임 글꼴에 없는 글자는 버린다. 바꾼 바이트 수를 돌려준다.
int ime_utf8_to_euckr(const char *utf8, char *out, int cap) {
    int n = 0;
    for (uint32_t u; (u = utf8_next(&utf8)) != 0;) {
        char o[2];
        int l = u < 0x80 ? (o[0] = (char)u, 1) : to_euckr(u, o);
        if (n + l >= cap) break;
        memcpy(out + n, o, (size_t)l);
        n += l;
    }
    out[n] = 0;
    return n;
}

// 게임 글꼴의 대략 폭. 커서 위치에만 쓴다.
int ime_text_width(const char *s, int n) {
    int w = 0;
    for (int i = 0; i < n && s[i]; i++) {
        if ((uint8_t)s[i] & 0x80) { w += 11; i++; }
        else w += s[i] == ' ' ? 4 : 6;
    }
    return w;
}

static bool composing;
static bool text_on;

// 커서 앞 글자 + extra + 커서 뒤 글자로 칸을 다시 쓴다. 커서는 그대로다.
static void field_show(const char *extra, int n) {
    cpu_t *c = &cpu;
    int pos = (int16_t)R16_(c, FIELD + F_POS);
    char *t = field_text();
    const char *tail = (const char *)c->m + FIELD + F_TAIL;
    int room = TEXT_CAP - 1 - pos, tail_n = (int)strnlen(tail, TEXT_CAP);
    if (n + tail_n > room) n = room > tail_n ? room - tail_n : 0;
    memset(t + pos, 0, (size_t)(TEXT_CAP - pos));
    memcpy(t + pos, extra, (size_t)n);
    memcpy(t + pos + n, tail, (size_t)(tail_n < room - n ? tail_n : room - n));
    W8_(c, FIELD + F_COMPOSE_W, (uint32_t)ime_text_width(extra, n));
}

static bool field_commit(const char *s, int n) {
    cpu_t *c = &cpu;
    int pos = (int16_t)R16_(c, FIELD + F_POS), max = (int16_t)R16_(c, FIELD + F_MAX);
    int tail_n = (int)strnlen((const char *)c->m + FIELD + F_TAIL, TEXT_CAP);
    if (max == NAME_FIELD_MAX) max = NAME_BYTES; // 이름 칸(닉네임, 받는 사람, 검색)은 패킷이 10바이트라 넘치면 글자가 잘린다
    if (pos + tail_n + n > max) return false;
    memcpy(field_text() + pos, s, (size_t)n);
    W16_(c, FIELD + F_POS, (uint32_t)(pos + n));
    field_show("", 0);
    return true;
}

static uint32_t utf8_next(const char **p) {
    return SDL_StepUTF8(p, NULL);
}

// 입력기가 U+1100대 첫가끝 자모로 줄 수 있어 음절이나 호환 자모로 합친다. 다음 글자를 돌려주고 p를 옮긴다.
static uint32_t next_char(const char **p) {
    static const uint16_t CHO_COMPAT[19] = {0x3131, 0x3132, 0x3134, 0x3137, 0x3138, 0x3139, 0x3141, 0x3142, 0x3143, 0x3145, 0x3146, 0x3147, 0x3148, 0x3149, 0x314a, 0x314b, 0x314c, 0x314d, 0x314e};
    uint32_t u = utf8_next(p);
    if (u >= 0x1161 && u <= 0x1175) return 0x314f + u - 0x1161;
    if (u < 0x1100 || u > 0x1112) return u;
    const char *q = *p;
    uint32_t v = utf8_next(&q);
    if (v < 0x1161 || v > 0x1175) return CHO_COMPAT[u - 0x1100];
    *p = q;
    uint32_t syllable = 0xac00 + ((u - 0x1100) * 21 + v - 0x1161) * 28;
    uint32_t t = utf8_next(&q);
    if (t >= 0x11a8 && t <= 0x11c2) { *p = q; syllable += t - 0x11a7; }
    return syllable;
}

// 칸에 못 넣은 글자 수. 입력기는 자기가 보낸 글자를 지운다고 믿고 Backspace를 보내므로, 이만큼은 칸을 건드리지 않는다.
static int dropped;

void ime_text(const char *utf8) {
    composing = false;
    field_show("", 0);
    for (uint32_t u; (u = next_char(&utf8)) != 0;) {
        char o[2];
        int n = to_euckr(u, o);
        if (!n || !field_commit(o, n)) dropped++;
    }
}

// 조합 중인 글자는 칸에 보이기만 하고 커서를 옮기지 않는다.
void ime_compose(const char *utf8) {
    char o[COMPOSE_MAX];
    int n = 0;
    for (uint32_t u; (u = next_char(&utf8)) != 0 && n + 2 <= COMPOSE_MAX;) n += to_euckr(u, o + n);
    composing = n > 0;
    field_show(o, n);
}

// 게임의 키 입력은 틱마다 한 칸(0x1593b0)이라 Backspace 뒤에 바로 오는 글자보다 늦게 처리되고 연속이면 사라진다.
// 입력기는 조합을 Backspace + 새 글자로 보내므로(Android, macOS의 글자 교체) 칸에서 바로 지운다. 칸이 비었으면 게임의 취소로 넘긴다.
static bool field_backspace(void) {
    cpu_t *c = &cpu;
    if (dropped) { dropped--; return true; }
    int pos = (int16_t)R16_(c, FIELD + F_POS);
    if (!pos) return field_text()[0] != 0;
    const char *t = field_text();
    pos -= pos >= 2 && (uint8_t)t[pos - 1] & 0x80 ? 2 : 1;
    W16_(c, FIELD + F_POS, (uint32_t)pos);
    field_show("", 0);
    return true;
}

// 칸의 화면 위치를 입력기 후보 창에 알린다.
static void set_area(void) {
    SDL_Window *win = host_window();
    SDL_Rect area;
    int w, h;
    SDL_GetWindowSize(win, &w, &h);
    area.x = w / 4; area.w = w / 2; area.y = h / 2; area.h = h / 16;
    SDL_SetTextInputArea(win, &area, 0);
}

bool ime_active(void) { return cpu.m[FIELD_OPEN] != 0; }

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// 웹: SDL은 폰의 화면 키보드를 띄우지 못하고 한글 조합도 주지 않는다. 페이지가 진짜 입력 칸을 띄워 브라우저의 입력기를 쓰고(web/app.js),
// 그 칸의 글 전체를 바뀔 때마다 넘겨 게임 칸을 같게 맞춘다. 조합 중인 글자도 그 안에 들어 있다.
// 게임 칸에 들어간 글자 수를 돌려준다. 페이지는 넘친 글자를 제 칸에서 지운다.
EMSCRIPTEN_KEEPALIVE int web_ime_set(const char *utf8) {
    cpu_t *c = &cpu;
    if (!ime_active()) return 0;
    W16_(c, FIELD + F_POS, 0);
    c->m[FIELD + F_TAIL] = 0;
    composing = false;
    field_show("", 0);
    int kept = 0;
    for (uint32_t u; (u = next_char(&utf8)) != 0;) {
        char o[2];
        int n = to_euckr(u, o);
        if (!n || !field_commit(o, n)) break;
        kept++;
    }
    return kept;
}

// 지금 게임 칸에 있는 글(EUC-KR). 칸이 열릴 때 페이지가 읽어 제 칸의 처음 값으로 쓴다.
EMSCRIPTEN_KEEPALIVE const char *web_ime_current(void) { return field_text(); }

// 페이지의 칸에서 Enter(완료)를 누르면 게임에 OK를 보낸다.
EMSCRIPTEN_KEEPALIVE void web_ime_ok(void) {
    if (!ime_active()) return;
    host_key(2, KEY_OK);
    host_key(3, KEY_OK);
}
#endif

// 칸이 열리고 닫힐 때 플랫폼 입력기(Android는 화면 키보드)를 켜고 끈다.
void ime_poll(void) {
    bool on = ime_active();
    if (on == text_on) return;
    text_on = on;
    composing = false;
    dropped = 0;
#ifdef __EMSCRIPTEN__
    EM_ASM({ Module.imeShow($0); }, on);
    return;
#endif
    SDL_Window *win = host_window();
    if (on) { set_area(); SDL_StartTextInput(win); }
    else SDL_StopTextInput(win);
}

// 칸이 열려 있는 동안 키보드는 글자 입력이다. 게임으로는 조작 키만 보낸다. 처리했으면 true.
bool ime_event(const SDL_Event *e) {
    if (!ime_active()) return false;
    switch (e->type) {
    case SDL_EVENT_TEXT_INPUT: ime_text(e->text.text); return true;
    case SDL_EVENT_TEXT_EDITING: ime_compose(e->edit.text); return true;
    case SDL_EVENT_FINGER_UP: // 화면 키보드를 내렸으면 화면을 눌러 다시 연다.
        if (SDL_HasScreenKeyboardSupport() && !SDL_ScreenKeyboardShown(host_window())) SDL_StartTextInput(host_window());
        return false;
    case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP: {
        int k = 0;
        switch (e->key.scancode) {
        case SDL_SCANCODE_BACKSPACE: case SDL_SCANCODE_AC_BACK: k = KEY_CLEAR; break;
        case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: k = KEY_OK; break;
        case SDL_SCANCODE_UP: k = KEY_UP; break;
        case SDL_SCANCODE_DOWN: k = KEY_DOWN; break;
        case SDL_SCANCODE_LEFT: k = KEY_LEFT; break;
        case SDL_SCANCODE_RIGHT: k = KEY_RIGHT; break;
        case SDL_SCANCODE_F11: return false;
        default: return true;
        }
        if (composing) return true; // 입력기가 쓰는 키다
        if (k == KEY_CLEAR && e->type == SDL_EVENT_KEY_DOWN && field_backspace()) return true;
        if (k == KEY_CLEAR && e->type == SDL_EVENT_KEY_UP) { if (field_text()[0]) return true; }
        if (e->type == SDL_EVENT_KEY_DOWN) host_key(2, k);
        else if (!e->key.repeat) host_key(3, k);
        return true;
    }
    }
    return false;
}

// 글자는 ime_text가 넣으므로 게임이 묻는 키 입력에는 돌려줄 글자가 없다. 표시 모드는 고정한다.
void ime_api(cpu_t *c, uint32_t i) {
    switch (i) {
    case 0:
        memset(c->m + c->r[2], 0, 5);
        memset(c->m + arg(c, 4), 0, 5);
        W32_(c, c->r[3], 0);
        W32_(c, arg(c, 5), 0);
        c->r[0] = 0;
        break;
    case 2: c->r[0] = MODE_HANGUL; break;
    default: c->r[0] = 0;
    }
}

// TextComponent 위치만 기억한다. 그리기는 게임이 한다(0x104d3e).
static struct { int x, y, w, h; bool used; } fields[FIELD_MAX];

void uic_api(cpu_t *c, uint32_t i) {
    uint32_t *r = c->r;
    switch (i) {
    case 2: // create: 0은 게임이 빈 값으로 볼 수 있어 1부터 준다.
        r[0] = 0;
        for (int k = 0; k < FIELD_MAX; k++)
            if (!fields[k].used) { fields[k].used = true; r[0] = (uint32_t)k + 1; break; }
        break;
    case 3:
        if (r[0] >= 1 && r[0] <= FIELD_MAX) fields[r[0] - 1].used = false;
        r[0] = 0;
        break;
    case 9: // configure(handle, x, y, w, h, ...)
        if (r[0] >= 1 && r[0] <= FIELD_MAX) {
            fields[r[0] - 1].x = (int)r[1]; fields[r[0] - 1].y = (int)r[2]; fields[r[0] - 1].w = (int)r[3]; fields[r[0] - 1].h = (int)arg(c, 4);
        }
        r[0] = 0;
        break;
    case 10: { // MC_uicGetGeometry(handle, &x, &y, &w, &h)
        int k = r[0] >= 1 && r[0] <= FIELD_MAX ? (int)r[0] - 1 : -1;
        W32_(c, r[1], k < 0 ? 0 : (uint32_t)fields[k].x);
        W32_(c, r[2], k < 0 ? 0 : (uint32_t)fields[k].y);
        W32_(c, r[3], k < 0 ? 0 : (uint32_t)fields[k].w);
        W32_(c, arg(c, 4), k < 0 ? 0 : (uint32_t)fields[k].h);
        r[0] = 0;
        break;
    }
    default: r[0] = 0;
    }
}
