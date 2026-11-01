// 스크립트 대사의 자동 진행과 대사 기록.
// 원본 0x11bde0(대사 시작)과 0x119e30(스크립트 끝)을 감싸 기록을 모은다. 기록 창은 게임의 창틀과 글꼴로 그린다.
// 창이 열린 동안 게임은 멈추므로 열 때의 화면을 복사해 두고 그 위에 다시 그린다.
#include "restored.h"
#include <stdlib.h>

void orig_f_1bde0(cpu_t *c);
void orig_f_19e30(cpu_t *c);
bool game_in_dialog(void);
void host_key(int type, int code);
uint32_t gfx_screen_handle(void);

#define T_TITLE "\xb4\xeb\xbb\xe7\x20\xb1\xe2\xb7\xcf" // 대사 기록

enum {
    GOT_SCREEN = 0x2c,
    SCREEN_W = 0x48,
    SCREEN_H = 0x4c,
    FN_PANEL = 0x172aa8,
    FN_WRAP = 0x12d3c8,
    FN_DRAW_LINE = 0x12c50c,
    FN_SET_COLOR = 0x12d734,
    TEXT_FONT = 1,

    SCRIPT = 0x1a2618,
    SCRIPT_STATE = SCRIPT + 0x32c,
    SCRIPT_SLOT = SCRIPT + 0x330,
    SCRIPT_SKIPPING = SCRIPT + 0x331,
    SCRIPT_CONTINUES = SCRIPT + 0x699, // 맵 이동을 거쳐 이어지는 스크립트
    STATE_TALK = 0x32,
    STATE_NARRATION = 0x35,
    SLOT_COUNT = 4,
    SLOT_SIZE = 0xb4,
    SLOT_OPEN = 0x340,
    SLOT_TEXT = 0x344,
    SLOT_LEN = 0x348,
    SLOT_PORTRAIT = 0x35a,
    SLOT_NAME = 0x350,
    SLOT_OPENING = 0x364,
    SLOT_CLOSING = 0x365,
    SLOT_DONE = 0x367,
    SLOT_NEXT_PAGE = 0x368,
    SLOT_PAGE_START = 0x36c,
    SLOT_PAGE_END = 0x370,
    SLOT_SHOWN = 0x374,

    AUTO_BASE_MS = 900,
    AUTO_PER_BYTE_MS = 35,
    AUTO_MAX_MS = 5000,
    KEY_OK = -5,

    LINE_BYTES = 80,
    MAX_LINES = 2000,
    PANEL_W = 224,
    PANEL_MARGIN = 8,
    PAD_X = 10,
    PANEL_BORDER = 2,
    TITLE_H = 24,
    ROW_H = 14,
    BAR_W = 3,
    NAME_BYTES = 32,
    PAGE_KEYS_ROWS = 5,
};

typedef enum { LINE_TEXT, LINE_NAME, LINE_GAP } line_kind;
typedef struct { char text[LINE_BYTES]; unsigned char len, kind; } log_line;

static log_line *lines;
static int line_count, top_line;
static bool story_ended = true, log_open, auto_on;
static uint64_t auto_since;
static uint16_t *snapshot;
static uint32_t text_buffer;

static void add_line(line_kind kind, const uint8_t *text, int len) {
    if (!lines) lines = calloc(MAX_LINES, sizeof *lines);
    if (!lines) return;
    if (line_count == MAX_LINES) {
        memmove(lines, lines + 1, (MAX_LINES - 1) * sizeof *lines);
        line_count--;
    }
    if (len > LINE_BYTES) len = LINE_BYTES;
    log_line *l = &lines[line_count++];
    l->kind = kind;
    l->len = (unsigned char)len;
    if (len) memcpy(l->text, text, len);
}

static int text_width(void) { return PANEL_W - PAD_X * 2 - BAR_W - 2; }

// 색 표시 글자(0x12d600의 표에서 줄바꿈을 뺀 것)는 다음 표시 글자까지 이어진다.
static bool is_color_mark(uint8_t ch) { return ch && strchr("&|${}opqrst", ch); }

static void add_text(uint32_t text, int len) {
    cpu_t *c = &cpu;
    int start = 0;
    uint8_t mark = 0;
    while (start < len) {
        int n = (int)CALL(FN_WRAP, text, len, start, text_width(), 1, TEXT_FONT);
        if (n <= 0) break;
        int shown = n;
        while (shown > 0 && (R8(text + start + shown - 1) == ';' || R8(text + start + shown - 1) == 0)) shown--;
        if (shown) {
            // 줄을 나눠 그리므로 앞 줄에서 시작한 색을 줄 머리에 다시 붙인다.
            uint8_t line[LINE_BYTES];
            int used = 0;
            if (mark) line[used++] = mark;
            if (shown > LINE_BYTES - used) shown = LINE_BYTES - used;
            memcpy(line + used, c->m + text + start, shown);
            add_line(LINE_TEXT, line, used + shown);
        }
        for (int i = start; i < start + n; i++) {
            uint8_t ch = R8(text + i);
            if (ch & 0x80) i++; // 한글 두 번째 바이트는 표시 글자가 아니다.
            else if (is_color_mark(ch)) mark = ch;
        }
        start += n;
    }
}

// 인자는 0x11bde0과 같다(글 번호, 칸, 내레이션 여부).
void f_1bde0(cpu_t *c) {
    uint32_t index = c->r[1] & 0xff, narration = c->r[2] & 0xff;
    orig_f_1bde0(c);
    if (story_ended && !R8(SCRIPT_CONTINUES)) line_count = 0;
    story_ended = false;
    uint32_t slot = SCRIPT + index * SLOT_SIZE, handle = R32(slot + SLOT_TEXT), name = R32(slot + SLOT_NAME);
    if (!handle) return;
    if (line_count) add_line(LINE_GAP, NULL, 0);
    if (!narration && (int8_t)R8(slot + SLOT_PORTRAIT) != -1 && name) {
        uint32_t text = R32(name) + 8;
        add_line(LINE_NAME, c->m + text, (int)strnlen((const char *)c->m + text, NAME_BYTES));
    }
    add_text(R32(handle) + 8, (int)R32(slot + SLOT_LEN));
}

void f_19e30(cpu_t *c) {
    orig_f_19e30(c);
    story_ended = true;
}

// 글이 다 나왔고 OK를 기다리는 중이면 그 쪽 글 길이를 돌려준다. 아니면 -1이다.
static int waiting_page_bytes(void) {
    cpu_t *c = &cpu;
    int state = (int)R32(SCRIPT_STATE), index = (int8_t)R8(SCRIPT_SLOT);
    if ((state != STATE_TALK && state != STATE_NARRATION) || index < 0 || index >= SLOT_COUNT || R8(SCRIPT_SKIPPING)) return -1;
    uint32_t slot = SCRIPT + (uint32_t)index * SLOT_SIZE;
    if (!R8(slot + SLOT_OPEN) || !R32(slot + SLOT_TEXT) || R8(slot + SLOT_OPENING) || R8(slot + SLOT_CLOSING)) return -1;
    if (R8(slot + SLOT_DONE) || R8(slot + SLOT_NEXT_PAGE)) return -1;
    int start = (int)R32(slot + SLOT_PAGE_START), end = (int)R32(slot + SLOT_PAGE_END);
    if (end <= start || (int)R32(slot + SLOT_SHOWN) < end) return -1;
    return end - start;
}

void dialog_set_auto(bool on) { auto_on = on; auto_since = 0; }
bool dialog_auto(void) { return auto_on; }

// 주 루프에서 게임이 돌 때마다 부른다.
void dialog_auto_tick(void) {
    int bytes = auto_on && game_in_dialog() ? waiting_page_bytes() : -1;
    if (bytes < 0) { auto_since = 0; return; }
    uint64_t now = now_ms();
    if (!auto_since) { auto_since = now; return; }
    int delay = AUTO_BASE_MS + bytes * AUTO_PER_BYTE_MS;
    if (delay > AUTO_MAX_MS) delay = AUTO_MAX_MS;
    if (now - auto_since < (uint64_t)delay) return;
    auto_since = 0;
    host_key(2, KEY_OK);
    host_key(3, KEY_OK);
}

static uint8_t *screen_pixels(int *w, int *h, int *bpl) {
    cpu_t *c = &cpu;
    uint32_t f = handle_data(gfx_screen_handle());
    *w = (int)R32(f);
    *h = (int)R32(f + 4);
    *bpl = (int)R32(f + 8);
    return c->m + handle_data(R32(f + 16));
}

static int panel_h(void) { cpu_t *c = &cpu; return (int)R32(got(GOT_SCREEN) + SCREEN_H) - PANEL_MARGIN * 2; }
static int visible_rows(void) { return (panel_h() - TITLE_H - PAD_X) / ROW_H; }

static void clamp_scroll(void) {
    int max_top = line_count - visible_rows();
    if (top_line > max_top) top_line = max_top;
    if (top_line < 0) top_line = 0;
}

static void draw_text(int x, int y, int width, const char *text, int len, uint32_t color, int center) {
    cpu_t *c = &cpu;
    if (!text_buffer) text_buffer = heap_alloc(LINE_BYTES + 1);
    memcpy(c->m + text_buffer, text, len);
    W8(text_buffer + len, 0);
    CALL(FN_DRAW_LINE, R32(got(GOT_SCREEN)), x, y, width, text_buffer, 0, 0, len, color, 0, center);
}

static void draw_log(void) {
    cpu_t *c = &cpu;
    enum { GOLD = 0xe8d6ab };
    const uint32_t KEEP_COLOR = 0xffffffffu; // 0x12c50c는 -1이면 지금 색을 그대로 쓰고 색 표시를 따른다.
    int w, h, bpl;
    uint8_t *px = screen_pixels(&w, &h, &bpl);
    memcpy(px, snapshot, (size_t)h * bpl);
    uint32_t g = R32(got(GOT_SCREEN));
    int width = (int)R32(got(GOT_SCREEN) + SCREEN_W), x = (width - PANEL_W) / 2, height = panel_h(), rows = visible_rows();
    CALL(FN_PANEL, x, PANEL_MARGIN, PANEL_W, height);
    // 창틀의 바탕 무늬는 메뉴 높이까지만 있어 긴 창은 뒤가 비친다. 틀 안을 칠한다.
    CALL(FN_FILL_RECT, g, x + PANEL_BORDER, PANEL_MARGIN + PANEL_BORDER, PANEL_W - PANEL_BORDER * 2, height - PANEL_BORDER * 2, 0x10, 0x18, 0x30);
    CALL(FN_FILL_RECT, g, x + PAD_X, PANEL_MARGIN + TITLE_H - 4, PANEL_W - PAD_X * 2, 1, 0x30, 0x5f, 0xae);
    draw_text(width / 2, PANEL_MARGIN + 7, PANEL_W, T_TITLE, (int)strlen(T_TITLE), GOLD, 1);
    int top = PANEL_MARGIN + TITLE_H;
    for (int i = 0; i < rows && top_line + i < line_count; i++) {
        const log_line *l = &lines[top_line + i];
        if (l->kind == LINE_GAP) continue;
        if (l->kind == LINE_TEXT) CALL(FN_SET_COLOR, 0xff, 0xff, 0xff);
        draw_text(x + PAD_X, top + i * ROW_H, text_width(), l->text, l->len, l->kind == LINE_NAME ? GOLD : KEEP_COLOR, 0);
    }
    if (line_count > rows) {
        int track = rows * ROW_H, bar = track * rows / line_count, bar_x = x + PANEL_W - PAD_X - BAR_W + 2;
        if (bar < ROW_H) bar = ROW_H;
        CALL(FN_FILL_RECT, g, bar_x, top, BAR_W, track, 0x30, 0x30, 0x30);
        CALL(FN_FILL_RECT, g, bar_x, top + (track - bar) * top_line / (line_count - rows), BAR_W, bar, 0xc6, 0x9c, 0x6e);
    }
    video_present(gfx_screen_handle());
}

bool dialog_log_is_open(void) { return log_open; }

void dialog_log_close(void) {
    if (!log_open) return;
    int w, h, bpl;
    uint8_t *px = screen_pixels(&w, &h, &bpl);
    memcpy(px, snapshot, (size_t)h * bpl);
    video_present(gfx_screen_handle());
    log_open = false;
}

void dialog_log_toggle(void) {
    if (log_open) { dialog_log_close(); return; }
    if (!game_in_dialog() || !gfx_screen_handle()) return;
    int w, h, bpl;
    uint8_t *px = screen_pixels(&w, &h, &bpl);
    uint16_t *copy = realloc(snapshot, (size_t)h * bpl);
    if (!copy) return;
    snapshot = copy;
    memcpy(snapshot, px, (size_t)h * bpl);
    log_open = true;
    top_line = line_count;
    clamp_scroll();
    draw_log();
}

void dialog_log_scroll(int rows) {
    if (!log_open) return;
    int before = top_line;
    top_line += rows;
    clamp_scroll();
    if (top_line != before) draw_log();
}

// 창이 열려 있으면 키를 모두 가져간다.
bool dialog_log_key(int type, int code) {
    if (!log_open) return false;
    if (type != 2) return true;
    switch (code) {
    case -1: case 50: dialog_log_scroll(-1); break;
    case -2: case 56: dialog_log_scroll(1); break;
    case -3: case 52: dialog_log_scroll(-PAGE_KEYS_ROWS); break;
    case -4: case 54: dialog_log_scroll(PAGE_KEYS_ROWS); break;
    case KEY_OK: case 53: case -16: dialog_log_close(); break;
    }
    return true;
}
