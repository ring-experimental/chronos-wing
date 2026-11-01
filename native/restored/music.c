// 플레이리스트. 메인메뉴 창. 게임이 실제 재생한 배경음을 저장 폴더의 PLAYLIST에 남기고, 그 곡들을 차례로 튼다.
// 게임 데이터에 곡 이름이 없어 번호로 표기한다. 번호는 snd/bgm%03d의 번호에 1을 더한 값이다.
#include "panels.h"
#include <stdio.h>
#include <stdlib.h>

void orig_f_74188(cpu_t *c);
void orig_f_7d3ec(cpu_t *c);

#define T_LABEL "\xc7\xc3\xb7\xb9\xc0\xcc\xb8\xae\xbd\xba\xc6\xae" // 플레이리스트
#define T_UNLOCKED "\xc7\xd8\xb1\xdd" // 해금
#define T_PLAYING "\xc0\xe7\xbb\xfd" // 재생

enum {
    TRACKS = 22,
    VISIBLE = 8,
    GOT_SCREEN = 0x2c,
    GOT_CURSOR = 0x1e0,
    FN_TEXT_WIDTH = 0x12d54c,
    FN_DRAW_TEXT = 0x12c1ec,
    FN_DRAW_TEXT_COLOR = 0x12c244,
    FN_SET_COLOR = 0x12d734,
    FN_DRAW_CURSOR = 0x11f514,
    FN_CURSOR_RESET = 0x11f4cc,
    FN_MUSIC_RESTART = 0x17d49c, // 환경설정이 배경음을 켤 때 쓰는 함수. 인자 (1, 1). 설정이 꺼져 있으면 틀지 않는다.
    KEY_CLEAR = -16,
    ROW_H = 14,
    NAME_X = 0x48,
    MARK_X = 0x82,
    BAR_X = 0xa0,
    BAR_W = 3,
};

static unsigned char unlocked[TRACKS];
static bool file_read;
static int loaded_track = -1; // 게임이 마지막으로 불러 둔 배경음

static int cursor, top_row;
static int playing = -1;      // 플레이리스트가 틀고 있는 곡
static uint32_t handles[TRACKS];
static uint32_t scratch;      // 줄 글을 담는 게스트 메모리

static void unlock_path(char *out, size_t cap) { snprintf(out, cap, "%s/PLAYLIST", save_dir); }

static void read_unlocked(void) {
    if (file_read) return;
    file_read = true;
    char path[1100];
    unlock_path(path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    fread(unlocked, 1, TRACKS, f); // 짧은 파일이면 나머지는 잠긴 곡이다.
    fclose(f);
}

static void unlock(int track) {
    read_unlocked();
    if (unlocked[track]) return;
    unlocked[track] = 1;
    char path[1100];
    unlock_path(path, sizeof path);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fwrite(unlocked, 1, TRACKS, f);
    fclose(f);
}

// 배경음 불러오기.
void f_74188(cpu_t *c) {
    int track = (int8_t)c->r[0];
    orig_f_74188(c);
    if (track >= 0 && track < TRACKS) loaded_track = track;
}

// 배경음 재생. 호출하는 곳은 배경음 쪽뿐이고 효과음은 0x17d2c0이 따로 튼다. 소리를 끈 설정이면 게임이 부르지 않는다.
void f_7d3ec(cpu_t *c) {
    uint32_t music = c->r[0];
    bool has_data = R32(music) != 0 && (int32_t)R32(music + 4) > 0;
    orig_f_7d3ec(c);
    if (has_data && loaded_track >= 0 && !audio_solo_active()) unlock(loaded_track);
}

static uint32_t track_handle(int track) {
    if (handles[track]) return handles[track];
    char path[1200];
    snprintf(path, sizeof path, "%s/snd/bgm%03d", game_dir, track);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc((size_t)size);
    if (data && fread(data, 1, (size_t)size, f) == (size_t)size) handles[track] = audio_load(data, (uint32_t)size);
    free(data);
    fclose(f);
    return handles[track];
}

static void play_track(int track) {
    uint32_t handle = track_handle(track);
    if (!handle) { playing = -1; return; }
    audio_solo_play(handle);
    playing = track;
}

static void scroll_to_cursor(void) {
    if (cursor < top_row) top_row = cursor;
    if (cursor >= top_row + VISIBLE) top_row = cursor - VISIBLE + 1;
}

static uint32_t guest_text(const char *s, int *len) {
    if (!scratch) scratch = heap_alloc(16);
    *len = (int)strlen(s);
    memcpy(cpu.m + scratch, s, (size_t)*len + 1);
    return scratch;
}

static void open_panel(void) {
    cpu_t *c = &cpu;
    read_unlocked();
    cursor = top_row = 0;
    playing = -1;
    CALL(FN_CURSOR_RESET, got(GOT_CURSOR));
}

static void draw_track(int track, int x, int y) {
    cpu_t *c = &cpu;
    uint32_t g = R32(got(GOT_SCREEN));
    char name[16];
    int len;
    if (unlocked[track]) snprintf(name, sizeof name, "BGM %02d", track + 1);
    else snprintf(name, sizeof name, "???");
    uint32_t addr = guest_text(name, &len);
    if (track == cursor) CALL(FN_SET_COLOR, 0xff, 0xff, 0xff);
    else if (unlocked[track]) CALL(FN_SET_COLOR, 0xc6, 0x9c, 0x6e);
    else CALL(FN_SET_COLOR, 0x70, 0x60, 0x50);
    CALL(FN_DRAW_TEXT, g, x + NAME_X, y + 1, addr, len, 1);
    if (track == cursor) {
        int width = (int)CALL(FN_TEXT_WIDTH, addr, len, 0);
        CALL(FN_DRAW_CURSOR, got(GOT_CURSOR), width, x + NAME_X, y);
    }
    if (track == playing) {
        uint32_t mark = menu_guest_text(T_PLAYING);
        CALL(FN_DRAW_TEXT_COLOR, g, x + MARK_X, y, mark, (int)strlen(T_PLAYING), menu_color(0xe8, 0xd6, 0xab), 0, 1);
    }
}

static void draw_panel(int x, int y) {
    cpu_t *c = &cpu;
    uint32_t g = R32(got(GOT_SCREEN));
    int top = (int)R32(got(GOT_SCREEN) + 0x4c) / 2 - 0x3e;
    menu_draw_frame(x, y);
    int count = 0, len;
    for (int i = 0; i < TRACKS; i++) count += unlocked[i] != 0;
    char head[16];
    snprintf(head, sizeof head, T_UNLOCKED " %d/%d", count, TRACKS);
    uint32_t addr = guest_text(head, &len);
    CALL(FN_DRAW_TEXT_COLOR, g, x + 0x58, top, addr, len, menu_color(0xe8, 0xd6, 0xab), 0, 1);
    for (int i = 0; i < VISIBLE; i++) draw_track(top_row + i, x, top + (i + 1) * ROW_H);
    int bar_y = top + ROW_H, bar_h = VISIBLE * ROW_H, thumb = bar_h * VISIBLE / TRACKS;
    CALL(FN_FILL_RECT, g, x + BAR_X, bar_y, BAR_W, bar_h, 0x40, 0x30, 0x20);
    CALL(FN_FILL_RECT, g, x + BAR_X, bar_y + (bar_h - thumb) * top_row / (TRACKS - VISIBLE), BAR_W, thumb, 0xe8, 0xd6, 0xab);
}

static void press_ok(void) {
    if (cursor == playing) {
        audio_solo_stop();
        playing = -1;
    } else if (unlocked[cursor]) play_track(cursor);
}

static void move_cursor(int step) {
    cursor = (cursor + step + TRACKS) % TRACKS;
    scroll_to_cursor();
}

static void move_page(int step) {
    cursor += step * VISIBLE;
    if (cursor < 0) cursor = 0;
    if (cursor >= TRACKS) cursor = TRACKS - 1;
    scroll_to_cursor();
}

static void panel_key(int key) {
    switch (key) {
    case KEY_CLEAR: menu_leave(); break;
    case -5: case 53: press_ok(); break;
    case -4: case 54: move_page(1); break;
    case -3: case 52: move_page(-1); break;
    case -2: case 56: move_cursor(1); break;
    case -1: case 50: move_cursor(-1); break;
    }
}

// 곡이 끝나면 다음 해금 곡으로 넘어간다. 한 곡만 열렸으면 그 곡을 다시 튼다.
static void tick_panel(void) {
    if (playing < 0 || audio_solo_playing()) return;
    int ended = playing, next = ended;
    for (int i = 1; i <= TRACKS; i++) {
        next = (ended + i) % TRACKS;
        if (unlocked[next]) break;
    }
    play_track(next);
    if (cursor == ended) { cursor = next; scroll_to_cursor(); }
}

static void close_panel(void) {
    cpu_t *c = &cpu;
    playing = -1;
    if (!audio_solo_active()) return; // 한 곡도 틀지 않았으면 타이틀 음악이 그대로 돌고 있다.
    audio_solo_end();
    CALL(FN_MUSIC_RESTART, 1, 1);
}

const menu_panel music_panel = {T_LABEL, open_panel, draw_panel, panel_key, tick_panel, close_panel};
