// 턴제 모드 켜고 끄기와 전용 저장 폴더(사용자 결정: 스토리모드 엔딩 뒤 메인메뉴에서 열리고 본편 세이브와 연계하지 않는다).
// - 메인메뉴 "턴제 모드"(devmode.c 항목 16)는 창(이어하기 / 처음부터)을 연다. 고르면 턴제를 켜고 저장 폴더를 save_dir/turn으로 바꾼 뒤 원작 스토리모드(상태 0)로 넘긴다.
//   처음부터는 한 번 더 누르면 기존 턴제 폴더를 지우지 않고 save_dir/turn-backup-날짜로 옮긴 뒤 새로 시작한다.
//   원작 스토리모드는 세이브가 있으면 이어 하고 없으면 새로 시작한다. 세이브 파일 경로는 열 때마다 save_dir로 정해진다(host/api.c).
// - 메인메뉴에서 다른 항목을 고르거나 좌우로 움직이면(0x1453bc, 0x14609c) 본편 폴더와 본편 모드로 되돌린다. 메인메뉴 상태 함수는 메뉴 안에서만 불린다.
// - 해금: 스토리 다시보기의 최종장(퀘스트 34 완료) 기록. 본편 세이브의 퀘스트 값으로도 소급한다(replay.c).
// - 시험: 환경 변수 CHRONOS_TURN이면 처음부터 켜고 폴더는 바꾸지 않는다(--save 그대로).
#include "panels.h"
#include "turn.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

bool replay_final_seen(void);
void turn_set_enabled(bool on);
void turn_reset_state(void);

static char base_dir[1024];
static bool switched;

bool turn_env_forced(void) { return getenv("CHRONOS_TURN") != NULL; }

bool turn_unlocked(void) { return turn_env_forced() || replay_final_seen(); }

// 메인메뉴에서 턴제 모드를 골랐다.
void turn_begin(void) {
    if (turn_env_forced()) return;
    if (!switched) {
        snprintf(base_dir, sizeof base_dir, "%s", save_dir);
        snprintf(save_dir, sizeof save_dir, "%s/turn", base_dir);
        mkdir(save_dir, 0755);
        switched = true;
    }
    turn_reset_state();
    turn_set_enabled(true);
    fprintf(stderr, "[turn] 턴제 모드 시작, 저장 폴더 %s\n", save_dir);
}

// 메인메뉴에서 턴제 모드가 아닌 동작을 했다. 턴제 게임에서 돌아왔으면 본편으로 되돌린다.
void turn_back_to_main(void) {
    if (!switched) return;
    snprintf(save_dir, sizeof save_dir, "%s", base_dir);
    switched = false;
    turn_set_enabled(false);
    turn_reset_state();
    fprintf(stderr, "[turn] 본편으로 돌아옴, 저장 폴더 %s\n", save_dir);
}

// ---- 메인메뉴 창 ----

#define T_CONTINUE "\xc0\xcc\xbe\xee\xc7\xcf\xb1\xe2"
#define T_NEW "\xc3\xb3\xc0\xbd\xba\xce\xc5\xcd"
#define T_NO_SAVE "\xc5\xcf\xc1\xa6\x20\xbc\xbc\xc0\xcc\xba\xea\xb0\xa1\x20\xbe\xf8\xbd\xc0\xb4\xcf\xb4\xd9"
#define T_CONFIRM "\xc7\xd1\x20\xb9\xf8\x20\xb4\xf5\x20\xb4\xa9\xb8\xa3\xb8\xe9\x20\xb1\xe2\xc1\xb8\x20\xc5\xcf\xc1\xa6\x20\xbc\xbc\xc0\xcc\xba\xea\xb8\xa6"
#define T_CONFIRM2 "\xb9\xe9\xbe\xf7\xc7\xcf\xb0\xed\x20\xbb\xf5\xb7\xce\x20\xbd\xc3\xc0\xdb\xc7\xd5\xb4\xcf\xb4\xd9"

enum {
    GOT_SCREEN = 0x2c, GOT_CURSOR = 0x1e0,
    FN_MENU_STATE = 0x1453bc, MENU_STORY = 0,
    FN_DRAW_TEXT = 0x12c1ec, FN_SET_COLOR = 0x12d734, FN_TEXT_WIDTH = 0x12d54c,
    FN_DRAW_CURSOR = 0x11f514, FN_CURSOR_RESET = 0x11f4cc,
    ROW_H = 14, ROW_TOP = 8, TEXT_X = 16, NOTE_Y = 0x60,
    KEY_CLEAR = -16,
};

static int sel;
static bool confirm, has_save;
bool turn_starting; // 창에서 시작할 때 devmode.c의 상태 0 감싸기가 본편으로 되돌리지 않게 한다

static bool turn_save_exists(void) {
    char p[1100];
    snprintf(p, sizeof p, "%s/turn/HERODATA", switched ? base_dir : save_dir);
    FILE *f = fopen(p, "rb");
    if (f) fclose(f);
    return f != NULL;
}

static void panel_open(void) {
    cpu_t *c = &cpu;
    has_save = turn_save_exists();
    sel = has_save ? 0 : 1;
    confirm = false;
    CALL(FN_CURSOR_RESET, got(GOT_CURSOR));
}

static void draw_text(int x, int y, const char *t, int r, int g, int b, bool cursor) {
    cpu_t *c = &cpu;
    uint32_t text = menu_guest_text(t);
    int len = (int)strlen(t);
    CALL(FN_SET_COLOR, r, g, b);
    CALL(FN_DRAW_TEXT, R32(got(GOT_SCREEN)), x, y + 1, text, len, 0);
    if (cursor) {
        int w = (int)CALL(FN_TEXT_WIDTH, text, len, 0);
        CALL(FN_DRAW_CURSOR, got(GOT_CURSOR), w, x + w / 2, y);
    }
}

static void panel_draw(int x, int y) {
    menu_draw_frame(x, y);
    const char *rows[2] = {T_CONTINUE, T_NEW};
    for (int i = 0; i < 2; i++) {
        bool dim = i == 0 && !has_save;
        int r = i == sel ? 0xff : dim ? 0x70 : 0xe8, g = i == sel ? 0xff : dim ? 0x70 : 0xd6, b = i == sel ? 0xff : dim ? 0x70 : 0xab;
        draw_text(x + TEXT_X, y + ROW_TOP + i * ROW_H, rows[i], r, g, b, i == sel);
    }
    if (confirm) {
        draw_text(x + 8, y + NOTE_Y, T_CONFIRM, 0xff, 0xa0, 0x40, false);
        draw_text(x + 8, y + NOTE_Y + ROW_H, T_CONFIRM2, 0xff, 0xa0, 0x40, false);
    } else if (!has_save) draw_text(x + 8, y + NOTE_Y, T_NO_SAVE, 0xa0, 0xa0, 0xa0, false);
}

// 기존 턴제 폴더를 날짜 붙은 백업으로 옮긴다(지우지 않는다).
static bool backup_turn_dir(void) {
    char from[1100], to[1200];
    const char *base = switched ? base_dir : save_dir;
    snprintf(from, sizeof from, "%s/turn", base);
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(to, sizeof to, "%s/turn-backup-%04d%02d%02d-%02d%02d%02d", base, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
    bool ok = rename(from, to) == 0;
    fprintf(stderr, "[turn] 처음부터: %s → %s %s\n", from, to, ok ? "옮김" : "실패");
    return ok;
}

static void start(void) {
    cpu_t *c = &cpu;
    turn_begin();
    turn_starting = true;
    CALL(FN_MENU_STATE, MENU_STORY);
    turn_starting = false;
}

static void panel_key(int key) {
    switch (key) {
    case KEY_CLEAR: confirm = false; menu_leave(); break;
    case -1: case -2: case 50: case 56:
        if (has_save) sel ^= 1;
        confirm = false;
        break;
    case -5: case 53:
        if (sel == 0) { if (has_save) start(); break; }
        if (has_save && !confirm) { confirm = true; break; }
        if (has_save && !backup_turn_dir()) { confirm = false; break; }
        start();
        break;
    }
}

const menu_panel turn_panel = {"", panel_open, panel_draw, panel_key, NULL, NULL};
