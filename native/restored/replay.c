// 스토리 다시보기. 장면은 메인 퀘스트(DAT/QST 0~34)를 시작하게 하는 스크립트 묶음이다.
// 일반 플레이에서 스크립트가 시작될 때 맵, 메인 번호, 위치, 스토리 값(+0x6b4, +0x7b4), 조건 결과를 잡아 두고,
// 그 묶음이 퀘스트를 시작하면 저장 폴더의 REPLAY에 남긴다. 다시 볼 때는 값을 되돌리고 원본 스크립트 엔진이 같은 분기를 고르게 한다.
// 재생 중에는 host_save_sandbox로 게임의 파일 쓰기를 버리고 끝나면 메뉴로 나간다. 다음 일반 시작이 저장 파일을 다시 읽으므로 메모리 변화도 남지 않는다.
#include "panels.h"
#include <stdio.h>

void orig_f_19ce4(cpu_t *c);
void orig_f_19cac(cpu_t *c);
void orig_f_19a28(cpu_t *c);
void orig_f_1a054(cpu_t *c);
void orig_f_1a2dc(cpu_t *c);
void orig_f_1b4f8(cpu_t *c);
void orig_f_1eac4(cpu_t *c);
void orig_f_577fc(cpu_t *c);

#define T_LABEL "\xbd\xba\xc5\xe4\xb8\xae\x20\xb4\xd9\xbd\xc3\xba\xb8\xb1\xe2" // 스토리 다시보기
#define T_CONTINUE "\xc0\xcc\xbe\xee\xba\xb8\xb1\xe2" // 이어보기
#define T_LOCKED "\x3f\x3f\x3f"
#define T_EXIT_HINT "\xc3\xeb\xbc\xd2\xc5\xb0\x3a\x20\xb3\xaa\xb0\xa1\xb1\xe2" // 취소키: 나가기

enum {
    SCENE_COUNT = 36,
    QUEST_LAST = 34,
    KEY_FINAL = 35,
    QUEST_RUNNING = 1,
    QUEST_DONE = 2,
    UNLOCK_SEEN = 1,
    UNLOCK_ESTIMATED = 2,
    SAVE_MAIN_BYTES = 1000,
    SAVE_QUESTS = 304, // MAINGAMEDATA를 복호화한 내용에서 +0x7b4 표의 위치

    GOT_SCREEN = 0x2c,
    GOT_CURSOR = 0x1e0,
    GOT_WORLD = 0x13c,
    GOT_FIELD = 0xab8,
    FIELD_CONTROL = 0x44, // 0x1443c0. 0이면 스크립트가 조작을 잠근 상태다.
    GOT_LOADING = 0xa48, // 0x13f1a4가 켜고 0x13f49c가 끈다.
    SCRIPT = 0x1a2618,
    SCRIPT_RUNNING = SCRIPT + 0x328,
    SCRIPT_SKIPPING = SCRIPT + 0x331,
    SCRIPT_WARPING = SCRIPT + 0x6a0,
    SCRIPT_RELOAD = SCRIPT + 0x6a1,
    SCRIPT_TO_FIELD = SCRIPT + 0x6a2,
    SCRIPT_FLAGS = SCRIPT + 0x6b4,
    SCRIPT_QUESTS = SCRIPT + 0x7b4,
    WORLD_MAP = 0x2d4,
    WORLD_WARP_DIR = 0x2d8,
    WORLD_WARP_TIME = 0x2d9,
    PLAYER_X = 8,
    PLAYER_Y = 0xc,
    STAGE_MENU = 5,
    STAGE_MENU_LOAD = 3,
    MENU_STORY = 0,
    MAP_SPECIAL = 0xfd, // 0xfd 재시작, 0xfe 이전 맵, 0xff 저장 위치

    FN_MENU_STATE = 0x1453bc,
    FN_SCRIPT_RUN = 0x119ce4,
    FN_SCRIPT_END = 0x119e30,
    FN_SCRIPT_OP = 0x11a2dc,
    FN_READER_SKIP = 0x1365b4,
    FN_FACING = 0x103be0,
    FN_DAYTIME = 0x1759d8,
    FN_SET_DAYTIME = 0x175930,
    FN_ACTOR_BY_ID = 0x15a984,
    FN_SET_ACTION = 0x103bd4,
    FN_ACTOR_BY_NPC = 0x15a9e0,
    OP_ACTOR_SHOW = 0x0e,
    OP_ACTOR_HIDE = 0x0f,
    OP_CONTROL_LOCK = 0x45,
    FN_PLAYER = 0x103e7c,
    FN_SET_WARP_POS = 0x175820,
    FN_GAME_TEARDOWN = 0x140d14,
    FN_GAME_MENU_FREE = 0x1824d8,
    FN_STAGE_CHANGE = 0x13f1a4,
    FN_DRAW_TEXT = 0x12c1ec,
    FN_DRAW_TEXT_COLOR = 0x12c244,
    FN_SET_COLOR = 0x12d734,
    FN_TEXT_WIDTH = 0x12d54c,
    FN_DRAW_CURSOR = 0x11f514,
    FN_CURSOR_RESET = 0x11f4cc,

    TAPE_MAX = 95,
    WARP_UNKNOWN = 0xff,
    END_HOLD_MS = 150,

    ROW_H = 14,
    VISIBLE_ROWS = 10,
    ROW_TOP = 8,
    TEXT_X = 16,
    VALUE_X = 0x82,
    KEY_CLEAR = -16,
    KEY_LSK = -6,
    KEY_RSK = -7,
};

// 줄 이름은 DAT/QST의 퀘스트 제목이다. 마지막 줄만 [34,2](최종장 완료)라 제목이 없어 따로 쓴다.
static const char *const TITLES[SCENE_COUNT] = {
    "\xbc\xad\xc0\xe5\x20\x2d\x20\xb0\xf8\xb8\xed\x20\x31", // 서장 - 공명 1
    "\xbc\xad\xc0\xe5\x20\x2d\x20\xb0\xf8\xb8\xed\x20\x32", // 서장 - 공명 2
    "\xbc\xad\xc0\xe5\x20\x2d\x20\xb0\xf8\xb8\xed\x20\x33", // 서장 - 공명 3
    "\xbc\xad\xc0\xe5\x20\x2d\x20\xb0\xf8\xb8\xed\x20\x34", // 서장 - 공명 4
    "\x31\xc0\xe5\x20\x2d\x20\xba\xf1\xb0\xf8\xc1\xa4\x20\xbf\xcd\xc0\xcf\xb5\xe5\xc0\xae\x20\x31", // 1장 - 비공정 와일드윙 1
    "\x31\xc0\xe5\x20\x2d\x20\xba\xf1\xb0\xf8\xc1\xa4\x20\xbf\xcd\xc0\xcf\xb5\xe5\xc0\xae\x20\x32", // 1장 - 비공정 와일드윙 2
    "\x31\xc0\xe5\x20\x2d\x20\xba\xf1\xb0\xf8\xc1\xa4\x20\xbf\xcd\xc0\xcf\xb5\xe5\xc0\xae\x20\x33", // 1장 - 비공정 와일드윙 3
    "\x31\xc0\xe5\x20\x2d\x20\xba\xf1\xb0\xf8\xc1\xa4\x20\xbf\xcd\xc0\xcf\xb5\xe5\xc0\xae\x20\x34", // 1장 - 비공정 와일드윙 4
    "\x31\xc0\xe5\x20\x2d\x20\xba\xf1\xb0\xf8\xc1\xa4\x20\xbf\xcd\xc0\xcf\xb5\xe5\xc0\xae\x20\x35", // 1장 - 비공정 와일드윙 5
    "\x31\xc0\xe5\x20\x2d\x20\xba\xf1\xb0\xf8\xc1\xa4\x20\xbf\xcd\xc0\xcf\xb5\xe5\xc0\xae\x20\x36", // 1장 - 비공정 와일드윙 6
    "\x31\xc0\xe5\x20\x2d\x20\xba\xf1\xb0\xf8\xc1\xa4\x20\xbf\xcd\xc0\xcf\xb5\xe5\xc0\xae\x20\x37", // 1장 - 비공정 와일드윙 7
    "\x32\xc0\xe5\x20\x2d\x20\xb5\xc7\xc3\xa3\xc0\xba\x20\xc4\xda\xbe\xee\x20\x31", // 2장 - 되찾은 코어 1
    "\x32\xc0\xe5\x20\x2d\x20\xb5\xc7\xc3\xa3\xc0\xba\x20\xc4\xda\xbe\xee\x20\x32", // 2장 - 되찾은 코어 2
    "\x32\xc0\xe5\x20\x2d\x20\xb5\xc7\xc3\xa3\xc0\xba\x20\xc4\xda\xbe\xee\x20\x33", // 2장 - 되찾은 코어 3
    "\x32\xc0\xe5\x20\x2d\x20\xb5\xc7\xc3\xa3\xc0\xba\x20\xc4\xda\xbe\xee\x20\x34", // 2장 - 되찾은 코어 4
    "\x32\xc0\xe5\x20\x2d\x20\xb5\xc7\xc3\xa3\xc0\xba\x20\xc4\xda\xbe\xee\x20\x35", // 2장 - 되찾은 코어 5
    "\x33\xc0\xe5\x20\x2d\x20\xbf\xeb\xb1\xe2\xbb\xe7\x20\xb0\xa2\xbc\xba\x20\x31", // 3장 - 용기사 각성 1
    "\x33\xc0\xe5\x20\x2d\x20\xbf\xeb\xb1\xe2\xbb\xe7\x20\xb0\xa2\xbc\xba\x20\x32", // 3장 - 용기사 각성 2
    "\x33\xc0\xe5\x20\x2d\x20\xbf\xeb\xb1\xe2\xbb\xe7\x20\xb0\xa2\xbc\xba\x20\x33", // 3장 - 용기사 각성 3
    "\x33\xc0\xe5\x20\x2d\x20\xbf\xeb\xb1\xe2\xbb\xe7\x20\xb0\xa2\xbc\xba\x20\x34", // 3장 - 용기사 각성 4
    "\x33\xc0\xe5\x20\x2d\x20\xbf\xeb\xb1\xe2\xbb\xe7\x20\xb0\xa2\xbc\xba\x20\x35", // 3장 - 용기사 각성 5
    "\x33\xc0\xe5\x20\x2d\x20\xbf\xeb\xb1\xe2\xbb\xe7\x20\xb0\xa2\xbc\xba\x20\x36", // 3장 - 용기사 각성 6
    "\x34\xc0\xe5\x20\x2d\x20\xb4\xeb\xb8\xb6\xb5\xb5\xbb\xe7\xc0\xc7\x20\xb8\xb8\xb0\xa1\x20\x31", // 4장 - 대마도사의 만가 1
    "\x34\xc0\xe5\x20\x2d\x20\xb4\xeb\xb8\xb6\xb5\xb5\xbb\xe7\xc0\xc7\x20\xb8\xb8\xb0\xa1\x20\x32", // 4장 - 대마도사의 만가 2
    "\x34\xc0\xe5\x20\x2d\x20\xb4\xeb\xb8\xb6\xb5\xb5\xbb\xe7\xc0\xc7\x20\xb8\xb8\xb0\xa1\x20\x33", // 4장 - 대마도사의 만가 3
    "\x34\xc0\xe5\x20\x2d\x20\xb4\xeb\xb8\xb6\xb5\xb5\xbb\xe7\xc0\xc7\x20\xb8\xb8\xb0\xa1\x20\x34", // 4장 - 대마도사의 만가 4
    "\x35\xc0\xe5\x20\x2d\x20\xb8\xda\xc1\xf8\x20\xbd\xc5\xbc\xbc\xb0\xe8\x20\x31", // 5장 - 멋진 신세계 1
    "\x35\xc0\xe5\x20\x2d\x20\xb8\xda\xc1\xf8\x20\xbd\xc5\xbc\xbc\xb0\xe8\x20\x32", // 5장 - 멋진 신세계 2
    "\x35\xc0\xe5\x20\x2d\x20\xb8\xda\xc1\xf8\x20\xbd\xc5\xbc\xbc\xb0\xe8\x20\x33", // 5장 - 멋진 신세계 3
    "\x35\xc0\xe5\x20\x2d\x20\xb8\xda\xc1\xf8\x20\xbd\xc5\xbc\xbc\xb0\xe8\x20\x34", // 5장 - 멋진 신세계 4
    "\x35\xc0\xe5\x20\x2d\x20\xb8\xda\xc1\xf8\x20\xbd\xc5\xbc\xbc\xb0\xe8\x20\x35", // 5장 - 멋진 신세계 5
    "\x36\xc0\xe5\x20\x2d\x20\xb0\xe1\xc0\xfc\x21\x20\xbf\xcd\xc0\xcf\xb5\xe5\xc0\xae\x20\x31", // 6장 - 결전! 와일드윙 1
    "\x36\xc0\xe5\x20\x2d\x20\xb0\xe1\xc0\xfc\x21\x20\xbf\xcd\xc0\xcf\xb5\xe5\xc0\xae\x20\x32", // 6장 - 결전! 와일드윙 2
    "\x36\xc0\xe5\x20\x2d\x20\xb0\xe1\xc0\xfc\x21\x20\xbf\xcd\xc0\xcf\xb5\xe5\xc0\xae\x20\x33", // 6장 - 결전! 와일드윙 3
    "\xc3\xd6\xc1\xbe\xc0\xe5\x20\x2d\x20\xbf\xeb\xb1\xe2\xbb\xe7\xc0\xc7\x20\xb0\xe1\xc0\xc7", // 최종장 - 용기사의 결의
    "\xc3\xd6\xc1\xbe\xc0\xe5\x20\xbf\xcf\xb7\xe1", // 최종장 완료
};

// 파일 형식이 바뀌면 MAGIC을 올린다. 정수는 만든 기기의 바이트 순서지만 지원 대상은 모두 리틀 엔디언이다.
typedef struct {
    int32_t map, x, y;
    uint8_t unlocked, main, dir, time;
    uint8_t last_warp;                // 마지막 대사가 나온 때까지의 맵 이동 수. 그 뒤의 이동은 장면 밖이다.
    uint8_t tape_len;
    uint8_t facing;                   // 주인공 방향
    uint8_t tape[TAPE_MAX];           // 조건 판정 순서대로 (조건 번호 << 1) | 결과
    uint8_t flags[256], quests[256];
} scene_t;

static const char MAGIC[8] = "CWRPLY2\n";
static scene_t scenes[SCENE_COUNT];
static bool continue_on, file_loaded;

static scene_t capture;
static bool capturing, in_transit;
static int capture_warps, capture_key;
static bool capture_skipped;

typedef enum { REPLAY_OFF, REPLAY_LOADING, REPLAY_WAIT_TRIGGER, REPLAY_RUNNING, REPLAY_LEAVING } replay_state;
static replay_state state;
static int current, warps_seen, tape_pos;
static bool tape_live, apply_on_load, return_to_list, leave_now;
static uint64_t idle_since;
static int saved_daytime;
static uint8_t saved_warp_time;

static int selected, top_row; // 0은 이어보기 줄, 1부터 장면이다.

static void file_path(char *out, size_t cap) { snprintf(out, cap, "%s/REPLAY", save_dir); }

static void load_file(void) {
    if (file_loaded) return;
    file_loaded = true;
    char path[1100];
    file_path(path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    char magic[8];
    uint8_t on = 0;
    if (fread(magic, 1, 8, f) == 8 && !memcmp(magic, MAGIC, 8) && fread(&on, 1, 1, f) == 1) {
        continue_on = on != 0;
        if (fread(scenes, sizeof scenes, 1, f) != 1) memset(scenes, 0, sizeof scenes);
    }
    fclose(f);
}

// 강제 종료에도 이전 기록이 깨지지 않도록 임시 파일을 쓰고 바꿔 넣는다.
static void save_file(void) {
    char path[1100], tmp[1110];
    file_path(path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return;
    uint8_t on = continue_on;
    bool ok = fwrite(MAGIC, 1, 8, f) == 8 && fwrite(&on, 1, 1, f) == 1 && fwrite(scenes, sizeof scenes, 1, f) == 1;
    ok = fclose(f) == 0 && ok;
#ifdef _WIN32
    if (ok) remove(path); // Windows의 rename은 있는 파일을 덮지 않는다.
#endif
    if (ok) rename(tmp, path);
    else remove(tmp);
}

static bool replaying(void) { return state != REPLAY_OFF; }
static bool script_running(void) { cpu_t *c = &cpu; return R8(SCRIPT_RUNNING) != 0; }
static bool game_ready(void) { cpu_t *c = &cpu; return !R8(got(GOT_LOADING)); }

// 기존 세이브 소급 해금. 이 기능 전에 본 장면은 흐름 기록이 없어 세이브의 퀘스트 값으로 해금하고 정적 분석으로 고른 시작점을 넣는다.
// 일반 플레이에서 그 장면을 다시 보면 실제 기록으로 바뀐다.
typedef struct { short map, main, x, y; uint8_t quest_n, flag_n; uint8_t quests[4][2], flags[4][2]; } start_t;
static const start_t STARTS[SCENE_COUNT] = {
    {9, 10, 8, 8, 0, 0, {{0}}, {{0}}},
    {9, 9, 184, 136, 1, 0, {{0,2}}, {{0}}},
    {18, 4, 232, 280, 0, 0, {{0}}, {{0}}},
    {10, 3, 120, 136, 2, 0, {{1,2},{2,1}}, {{0}}},
    {8, 0, 136, 136, 0, 1, {{0}}, {{5,1}}},
    {22, 8, 296, 232, 0, 0, {{0}}, {{0}}},
    {31, 0, 136, 136, 0, 0, {{0}}, {{0}}},
    {38, 2, 232, 392, 0, 0, {{0}}, {{0}}},
    {209, 0, 136, 136, 0, 0, {{0}}, {{0}}},
    {1, 0, 136, 136, 0, 0, {{0}}, {{0}}},
    {1, 0, 136, 136, 0, 1, {{0}}, {{22,1}}},
    {209, 0, 136, 136, 1, 0, {{10,1}}, {{0}}},
    {211, 0, 136, 136, 0, 1, {{0}}, {{24,1}}},
    {21, 0, 136, 136, 1, 0, {{12,1}}, {{0}}},
    {67, 2, 72, 168, 0, 0, {{0}}, {{0}}},
    {82, 0, 136, 136, 0, 0, {{0}}, {{0}}},
    {21, 0, 136, 136, 1, 0, {{14,2}}, {{0}}},
    {214, 0, 136, 136, 0, 0, {{0}}, {{0}}},
    {88, 0, 136, 136, 1, 1, {{17,1}}, {{63,1}}},
    {214, 0, 136, 136, 1, 0, {{18,1}}, {{0}}},
    {225, 0, 136, 136, 0, 0, {{0}}, {{0}}},
    {215, 0, 136, 136, 0, 0, {{0}}, {{0}}},
    {180, 0, 136, 136, 0, 0, {{0}}, {{0}}},
    {216, 0, 136, 136, 0, 0, {{0}}, {{0}}},
    {82, 0, 136, 136, 1, 0, {{23,1}}, {{0}}},
    {57, 0, 136, 136, 0, 0, {{0}}, {{0}}},
    {204, 0, 136, 136, 1, 0, {{25,1}}, {{0}}},
    {103, 2, 168, 216, 1, 0, {{26,1}}, {{0}}},
    {206, 0, 136, 136, 1, 0, {{115,2}}, {{0}}},
    {215, 0, 136, 136, 0, 1, {{0}}, {{34,1}}},
    {32, 13, 184, 232, 0, 1, {{0}}, {{36,1}}},
    {38, 2, 232, 392, 1, 0, {{30,1}}, {{0}}},
    {114, 2, 504, 200, 0, 1, {{0}}, {{39,1}}},
    {224, 0, 136, 136, 0, 0, {{0}}, {{0}}},
    {208, 0, 136, 136, 0, 1, {{0}}, {{40,1}}},
    {161, 0, 136, 136, 0, 1, {{0}}, {{42,1}}},
};

static void fill_estimated(int key) {
    const start_t *st = &STARTS[key];
    scene_t *s = &scenes[key];
    memset(s, 0, sizeof *s);
    s->map = st->map;
    s->x = st->x;
    s->y = st->y;
    s->unlocked = UNLOCK_ESTIMATED;
    s->main = (uint8_t)st->main;
    s->dir = 1;
    s->time = 0xff;
    s->last_warp = WARP_UNKNOWN;
    int done = key < QUEST_LAST + 1 ? key : QUEST_LAST + 1;
    for (int q = 0; q < done; q++) s->quests[q] = QUEST_DONE;
    if (key > 0) s->quests[key - 1] = QUEST_RUNNING;
    for (int i = 0; i < st->quest_n; i++) s->quests[st->quests[i][0]] = st->quests[i][1];
    for (int i = 0; i < st->flag_n; i++) s->flags[st->flags[i][0]] = st->flags[i][1];
}

// 턴제 모드 시험(turntest.c): 장면의 시작 상태. 실제로 본 장면은 기록을, 아니면 정적 분석 값을 준다.
bool replay_scene_start(int key, int *map, int *x, int *y, int *main, int *dir, int *time, uint8_t flags[256], uint8_t quests[256]) {
    if (key < 0 || key >= SCENE_COUNT) return false;
    load_file();
    scene_t saved = scenes[key];
    if (!saved.unlocked) fill_estimated(key);
    const scene_t *s = &scenes[key];
    *map = s->map; *x = s->x; *y = s->y; *main = s->main; *dir = s->dir; *time = s->time;
    memcpy(flags, s->flags, 256);
    memcpy(quests, s->quests, 256);
    scenes[key] = saved;
    return true;
}

static void unlock_from_save(void) {
    static uint8_t data[SAVE_MAIN_BYTES];
    if (savefile_read_plain("MAINGAMEDATA", data, sizeof data) != SAVE_MAIN_BYTES) return;
    const uint8_t *quests = data + SAVE_QUESTS;
    bool changed = false;
    for (int key = 0; key < SCENE_COUNT; key++) {
        bool seen = key == KEY_FINAL ? quests[QUEST_LAST] == QUEST_DONE : quests[key] != 0;
        if (!seen || scenes[key].unlocked) continue;
        fill_estimated(key);
        changed = true;
    }
    if (changed) save_file();
}

// 일반 플레이 기록

static void begin_capture(int main) {
    cpu_t *c = &cpu;
    uint32_t player = CALL(FN_PLAYER), world = got(GOT_WORLD);
    capture.map = (int32_t)R32(world + WORLD_MAP);
    capture.x = (int32_t)R32(player + PLAYER_X);
    capture.y = (int32_t)R32(player + PLAYER_Y);
    capture.main = (uint8_t)main;
    capture.dir = R8(world + WORLD_WARP_DIR);
    capture.time = R8(world + WORLD_WARP_TIME);
    capture.unlocked = 1;
    capture.last_warp = 0;
    capture.tape_len = 0;
    capture.facing = (uint8_t)CALL(FN_FACING, player);
    memcpy(capture.flags, c->m + SCRIPT_FLAGS, sizeof capture.flags);
    memcpy(capture.quests, c->m + SCRIPT_QUESTS, sizeof capture.quests);
    capturing = true;
    capture_warps = 0;
    capture_key = -1;
    capture_skipped = false;
}

static void finish_capture(void) {
    if (!capturing) return;
    capturing = false;
    if (capture_key < 0 || capture_skipped) return; // 건너뛴 대사는 세지 못하니 끝을 정하지 않는다.
    scenes[capture_key].last_warp = capture.last_warp;
    save_file();
}

static void mark_scene(int key) {
    load_file();
    if (scenes[key].unlocked == UNLOCK_SEEN) return; // 처음 본 흐름을 남긴다. 소급 해금한 줄은 실제 흐름으로 바꾼다.
    scenes[key] = capture;
    scenes[key].last_warp = WARP_UNKNOWN; // 끝을 모르면 재생이 스크립트 끝까지 간다.
    capture_key = key;
    save_file(); // 묶음이 끝나기 전에 꺼져도 해금은 남는다.
}

// 대사가 시작될 때 dialog.c가 부른다.
void replay_note_talk(void) {
    if (capturing) capture.last_warp = (uint8_t)capture_warps;
}

// 스크립트가 끝날 때 dialog.c가 부른다. 맵 이동 중의 정리는 끝이 아니다.
void replay_script_ended(void) {
    if (!in_transit) finish_capture();
}

// 원본 함수 감싸기

// 메인 스크립트 시작. 맵 입장(0x119cac)과 칸 판정(0x119d00)이 모두 여기로 온다.
void f_19ce4(cpu_t *c) {
    int main = (int)c->r[0];
    if (main >= 0) {
        if (replaying()) {
            if (!tape_live && main == scenes[current].main) { tape_live = true; tape_pos = 0; }
        } else if (!in_transit) begin_capture(main);
    }
    c->r[0] = (uint32_t)main;
    orig_f_19ce4(c);
}

// 스크립트 명령. 재생 중에는 아이템, 돈, 경험치, HP, MP를 바꾸는 명령과 NPC 기능 창을 인자만 넘기고 건너뛴다.
// 어차피 저장되지 않고, 지금 세이브에 없는 아이템을 빼는 명령(0x13)은 원본이 빈 포인터를 읽어 멈춘다.
// 캐릭터 그림에는 방향에 따라 없는 동작이 있다(S053의 동작 8, 방향 1). 재생은 위치와 시점이 처음과 조금 다를 수 있으므로
// 명령 0x01이 그런 동작을 켜면 원본이 빈 칸을 읽어 멈춘다. 그때는 그 동작만 건너뛴다.
static bool action_missing(uint32_t reader) {
    cpu_t *c = &cpu;
    uint32_t r = handle_data(reader), bytes = handle_data(R32(r)) + R32(r + 8);
    int id = (int8_t)R8(bytes), action = (int8_t)R8(bytes + 1);
    if (action < 0) return false;
    uint32_t actor = CALL(FN_ACTOR_BY_ID, id);
    if (!actor || (R8(actor + 0xaa) == 0xff && R8(actor + 0xab) == 0xff) || R8(actor + 0xa6) != 1) return false;
    int dir = (int8_t)CALL(FN_FACING, actor);
    uint32_t table = R32(actor + 0x7c);
    if (dir < 0 || !table) return false;
    return !R32(handle_data(table) + action * 0x48 + (dir == 3 ? 2 : dir) * 4 + 8);
}

void f_1a2dc(cpu_t *c) {
    int skip = 0;
    if (replaying()) switch (c->r[0]) {
    case 0x01: { uint32_t op = c->r[0], reader = c->r[1]; if (action_missing(reader)) skip = 2; c->r[0] = op; c->r[1] = reader; break; }
    case 0x10: case 0x11: case 0x18: case 0x1c: case 0x1d: skip = 1; break;
    case 0x12: case 0x13: case 0x15: case 0x19: case 0x20: skip = 4; break;
    case 0x21: skip = 5; break;
    case 0x47: case 0x48: case 0x49: skip = 1; break;
    case 0x40: case 0x41: case 0x43: case 0x44: case 0x46: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e:
    case 0x50: case 0x51: case 0x53: case 0x54: case 0x55: case 0x56: c->r[0] = 1; return; // NPC 상점과 창고 창. 장면 내용이 아니다.
    }
    if (!skip) { orig_f_1a2dc(c); return; }
    uint32_t reader = c->r[1];
    CALL(FN_READER_SKIP, reader, skip);
    c->r[0] = 1;
}

// 조건 판정(스크립트 조건 0~9). 인자는 원본이 읽어야 하므로 늘 부르고, 재생 중에는 기록된 결과로 바꾼다.
void f_1a054(cpu_t *c) {
    int kind = (int)c->r[0];
    orig_f_1a054(c);
    int result = (c->r[0] & 0xff) != 0;
    if (replaying()) {
        const scene_t *s = &scenes[current];
        if (tape_live && tape_pos < s->tape_len && (s->tape[tape_pos] >> 1) == kind) c->r[0] = s->tape[tape_pos++] & 1;
    } else if (capturing && capture.tape_len < TAPE_MAX) {
        capture.tape[capture.tape_len++] = (uint8_t)(kind << 1 | result);
    }
}

// 스토리 값 쓰기. 메인 퀘스트 시작과 최종장 완료가 장면 해금이다.
void f_1eac4(cpu_t *c) {
    int quest = (int)(c->r[0] & 0xff), value = (int8_t)c->r[1];
    orig_f_1eac4(c);
    if (replaying() || !capturing) return;
    if (quest <= QUEST_LAST && value == QUEST_RUNNING) mark_scene(quest);
    else if (quest == QUEST_LAST && value == QUEST_DONE) mark_scene(KEY_FINAL);
}

// 맵 이동. 재생 중에 장면 밖으로 나가는 이동은 하지 않고 끝낸다.
void f_1b4f8(cpu_t *c) {
    int map = (int16_t)c->r[0];
    if (map < 0) map += 0x100;
    if (replaying()) {
        if (state == REPLAY_RUNNING) {
            warps_seen++;
            if (map >= MAP_SPECIAL || (scenes[current].last_warp != WARP_UNKNOWN && warps_seen > scenes[current].last_warp)) {
                leave_now = true;
                return;
            }
        }
    } else {
        in_transit = true;
        if (capturing) {
            capture_warps++;
            if (R8(SCRIPT_SKIPPING)) capture_skipped = true;
        }
    }
    orig_f_1b4f8(c);
}

// 스크립트와 맵을 읽는 함수. 게임을 처음 시작할 때도 장면 사이를 옮길 때도 맵 자리를 여기서 정한다.
void f_19a28(cpu_t *c) {
    if (replaying() && apply_on_load) {
        const scene_t *s = &scenes[current];
        uint32_t world = got(GOT_WORLD);
        W32(world + WORLD_MAP, (uint32_t)s->map);
        W8(world + WORLD_WARP_DIR, s->dir);
        W8(world + WORLD_WARP_TIME, s->time);
        memcpy(c->m + SCRIPT_FLAGS, s->flags, sizeof s->flags);
        memcpy(c->m + SCRIPT_QUESTS, s->quests, sizeof s->quests);
    }
    orig_f_19a28(c);
    if (replaying() && apply_on_load) {
        const scene_t *s = &scenes[current];
        uint32_t player = CALL(FN_PLAYER);
        W32(player + PLAYER_X, (uint32_t)s->x);
        W32(player + PLAYER_Y, (uint32_t)s->y);
        // 불러오기 직후의 방향은 -1이다. 그대로면 명령 0x0f의 대역 방향이 정해지지 않아 원본이 그림 표 밖을 읽어 멈춘다.
        CALL(FN_SET_ACTION, player, 0, (int8_t)s->facing < 0 ? 0 : s->facing);
        apply_on_load = false;
    }
}

// 맵 입장 스크립트 시작. 메인 0이 아닌 장면은 입장 스크립트가 끝난 뒤 tick에서 시작한다.
void f_19cac(cpu_t *c) {
    orig_f_19cac(c);
    if (replaying()) {
        if (state == REPLAY_LOADING) {
            state = scenes[current].main ? REPLAY_WAIT_TRIGGER : REPLAY_RUNNING;
            idle_since = 0;
        }
        return;
    }
    in_transit = false;
    if (!script_running()) finish_capture();
}

// 재생 안내. 원본 필드 그리기의 마지막 단계다.
void f_577fc(cpu_t *c) {
    orig_f_577fc(c);
    if (state != REPLAY_RUNNING && state != REPLAY_WAIT_TRIGGER) return;
    uint32_t g = R32(got(GOT_SCREEN));
    int width = (int)R32(got(GOT_SCREEN) + 0x48);
    uint32_t text = menu_guest_text(T_EXIT_HINT);
    CALL(FN_DRAW_TEXT_COLOR, g, width / 2, 2, text, (uint32_t)strlen(T_EXIT_HINT), menu_color(0xff, 0xff, 0xff), menu_color(0, 0, 0), 1);
}

// 재생 흐름

static void start_scene(int key) {
    current = key;
    warps_seen = 0;
    tape_live = false;
    tape_pos = 0;
    apply_on_load = true;
    leave_now = false;
    idle_since = 0;
    state = REPLAY_LOADING;
}

static int next_unlocked(int key) {
    for (int i = key + 1; i < SCENE_COUNT; i++)
        if (scenes[i].unlocked) return i;
    return -1;
}

static void restore_scene_state(void);

// 메뉴를 거치지 않는다. 원본 맵 이동으로 옮기면 맵과 스크립트를 새로 읽는다.
static void go_to_next_scene(int key) {
    cpu_t *c = &cpu;
    restore_scene_state();
    start_scene(key);
    const scene_t *s = &scenes[key];
    uint32_t args[5] = {(uint32_t)s->map, (uint32_t)(s->x >> 4), (uint32_t)(s->y >> 4), s->dir, s->time};
    guest_call(0x11b4f8, args, 5);
}

// 스크립트 명령을 원본 처리기로 하나 실행한다. 명령이 인자를 읽는 곳이 읽기 객체라 임시로 만든다.
static void run_script_op(int op, int value) {
    cpu_t *c = &cpu;
    uint32_t data = handle_alloc(4);
    W8(handle_data(data), value);
    uint32_t reader = handle_alloc(16), r = handle_data(reader); // 0x136444가 만드는 것과 같은 모양이다.
    W32(r, data);
    W32(r + 4, 1);
    W32(r + 8, 0);
    W32(r + 12, 1);
    CALL(FN_SCRIPT_OP, op, reader);
    handle_free(reader);
    handle_free(data);
}

// 장면을 중간에 끊으면 주인공 숨김과 조작 잠금이 풀리지 않고 남는다. 이 값들은 불러오기로 초기화되지 않는다.
// 스크립트들이 장면 끝에서 하는 그대로 되돌린다.
static void restore_scene_state(void) {
    cpu_t *c = &cpu;
    CALL(FN_SCRIPT_END);
    W8(SCRIPT_SKIPPING, 0);
    run_script_op(OP_ACTOR_SHOW, 1);
    if (CALL(FN_ACTOR_BY_NPC, 0)) run_script_op(OP_ACTOR_HIDE, 0); // 대역이 없는 맵에서 원본은 빈 포인터를 읽는다.
    run_script_op(OP_CONTROL_LOCK, 1);
    CALL(FN_SCRIPT_END);
    // 주인공 동작이 -1로 남으면 그려지지 않는다.
    uint32_t player = CALL(FN_PLAYER);
    int facing = (int8_t)CALL(FN_FACING, player);
    CALL(FN_SET_ACTION, player, 0, facing < 0 ? 0 : facing);
}

static void leave_to_menu(void) {
    cpu_t *c = &cpu;
    restore_scene_state();
    state = REPLAY_LEAVING;
    return_to_list = true;
    CALL(FN_GAME_TEARDOWN);
    CALL(FN_GAME_MENU_FREE);
    CALL(FN_STAGE_CHANGE, STAGE_MENU, STAGE_MENU_LOAD);
}

// 메뉴로 돌아왔는지 devmode.c의 상태 전환에서 묻는다. 참이면 다시보기 창을 연다.
bool replay_take_return(void) {
    if (!return_to_list) return false;
    return_to_list = false;
    state = REPLAY_OFF;
    host_save_sandbox = false;
    // 시간대는 불러오기로 초기화되지 않아 장면의 값이 남으면 다음 게임의 NPC 구성이 바뀐다.
    cpu_t *c = &cpu;
    CALL(FN_SET_DAYTIME, saved_daytime);
    W8(got(GOT_WORLD) + WORLD_WARP_TIME, saved_warp_time);
    return true;
}

// 주 루프에서 게임이 돌 때마다 부른다.
void replay_tick(void) {
    cpu_t *c = &cpu;
    if (state == REPLAY_OFF || state == REPLAY_LEAVING || !game_ready()) { idle_since = 0; return; }
    bool busy = script_running() || R8(SCRIPT_WARPING) || R8(SCRIPT_RELOAD) || R8(SCRIPT_TO_FIELD);
    if (state == REPLAY_LOADING) return;
    if (state == REPLAY_WAIT_TRIGGER) {
        if (busy) return;
        state = REPLAY_RUNNING;
        CALL(FN_SCRIPT_RUN, scenes[current].main);
        return;
    }
    if (!leave_now) {
        if (busy) { idle_since = 0; return; }
        uint64_t now = now_ms();
        if (!idle_since) { idle_since = now; return; }
        if (now - idle_since < END_HOLD_MS) return;
    }
    int next = continue_on ? next_unlocked(current) : -1;
    if (next >= 0) go_to_next_scene(next);
    else leave_to_menu();
}

// 재생 중의 키. 나가기와 게임 메뉴 키만 가져가고 나머지는 대사 진행에 쓰게 둔다.
bool replay_key(int type, int code) {
    if (state == REPLAY_OFF) return false;
    if (code == KEY_LSK || code == KEY_RSK) return true;
    if (code != KEY_CLEAR) {
        // 조작 잠금(명령 0x45) 전에 장면이 시작되면 키가 주인공 동작으로 간다. 주인공을 숨긴 상태(명령 0x0f)면 원본이 빈 동작 표를 읽어 멈춘다.
        cpu_t *c = &cpu;
        int script = (int)R32(SCRIPT + 0x32c);
        bool talking = script == 0x32 || script == 0x35 || script == 0x37 || script == 0x38;
        return R8(got(GOT_FIELD) + FIELD_CONTROL) && !talking;
    }
    if (type == 2 && game_ready() && (state == REPLAY_RUNNING || state == REPLAY_WAIT_TRIGGER)) {
        leave_to_menu();
    }
    return true;
}

// 목록 창

static int row_count(void) { return SCENE_COUNT + 1; }

static void clamp_view(void) {
    if (selected < top_row) top_row = selected;
    if (selected >= top_row + VISIBLE_ROWS) top_row = selected - VISIBLE_ROWS + 1;
}

static void open_panel(void) {
    load_file();
    unlock_from_save();
    cpu_t *c = &cpu;
    load_file();
    selected = 0;
    top_row = 0;
    CALL(FN_CURSOR_RESET, got(GOT_CURSOR));
}

static void draw_row(int row, int x, int y, bool is_selected) {
    cpu_t *c = &cpu;
    uint32_t g = R32(got(GOT_SCREEN));
    const char *label = row == 0 ? T_CONTINUE : scenes[row - 1].unlocked ? TITLES[row - 1] : T_LOCKED;
    bool dim = row > 0 && !scenes[row - 1].unlocked;
    uint32_t text = menu_guest_text(label);
    int len = (int)strlen(label);
    if (is_selected) CALL(FN_SET_COLOR, 0xff, 0xff, 0xff);
    else if (dim) CALL(FN_SET_COLOR, 0x70, 0x70, 0x70);
    else CALL(FN_SET_COLOR, 0xe8, 0xd6, 0xab);
    CALL(FN_DRAW_TEXT, g, x + TEXT_X, y + 1, text, len, 0);
    if (row == 0) {
        const char *value = continue_on ? "ON" : "OFF";
        CALL(FN_DRAW_TEXT, g, x + VALUE_X, y + 1, menu_guest_text(value), (uint32_t)strlen(value), 1);
    }
    if (is_selected) {
        int width = (int)CALL(FN_TEXT_WIDTH, text, len, 0);
        CALL(FN_DRAW_CURSOR, got(GOT_CURSOR), width, x + TEXT_X + width / 2, y);
    }
}

static void draw_panel(int x, int y) {
    menu_draw_frame(x, y);
    for (int i = 0; i < VISIBLE_ROWS && top_row + i < row_count(); i++)
        draw_row(top_row + i, x, y + ROW_TOP + i * ROW_H, top_row + i == selected);
    // 스크롤 막대
    cpu_t *c = &cpu;
    uint32_t g = R32(got(GOT_SCREEN));
    int track = VISIBLE_ROWS * ROW_H, bar = track * VISIBLE_ROWS / row_count(), bar_x = x + 0xb0 - 8, bar_y = y + ROW_TOP;
    CALL(FN_FILL_RECT, g, bar_x, bar_y, 3, track, 0x30, 0x30, 0x30);
    CALL(FN_FILL_RECT, g, bar_x, bar_y + (track - bar) * top_row / (row_count() - VISIBLE_ROWS), 3, bar, 0xc6, 0x9c, 0x6e);
}

static void start_from_list(int key) {
    cpu_t *c = &cpu;
    current = key;
    saved_daytime = (int8_t)CALL(FN_DAYTIME);
    saved_warp_time = R8(got(GOT_WORLD) + WORLD_WARP_TIME);
    host_save_sandbox = true;
    start_scene(key);
    in_transit = false;
    capturing = false;
    CALL(FN_MENU_STATE, MENU_STORY);
}

static void panel_key(int key) {
    int n = row_count();
    switch (key) {
    case KEY_CLEAR: menu_leave(); break;
    case -2: case 56: selected = (selected + 1) % n; clamp_view(); break;
    case -1: case 50: selected = (selected + n - 1) % n; clamp_view(); break;
    case -3: case -4: case 52: case 54:
        if (selected == 0) { continue_on = !continue_on; save_file(); }
        break;
    case -5: case 53:
        if (selected == 0) { continue_on = !continue_on; save_file(); }
        else if (scenes[selected - 1].unlocked) start_from_list(selected - 1);
        break;
    }
}

const menu_panel replay_panel = {T_LABEL, open_panel, draw_panel, panel_key, NULL, NULL};
