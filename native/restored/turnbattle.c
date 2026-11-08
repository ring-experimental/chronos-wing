// 턴제 전투 시제품. 필드 몬스터에 닿거나(0x148bfc 안의 0x148ba4), 몬스터가 공격하거나(0x14de84), 주인공이 몬스터를 치면(0x135a04)
// 실시간 전투 대신 사이드뷰 턴제 전투를 연다. 전투 중에는 게임 타이머를 돌리지 않고(main.c) 열 때의 화면 위에 그린다.
// 이기면 원본 피해 함수(0x14ad54)로 HP를 0으로 만들어 원래 죽음, 보상, 도감, 사냥 카운트 경로를 탄다.
// 근거 주소와 정한 값은 work/turn-plan.md "조사 결과", "시제품".
#include "restored.h"
#include "turn.h"
#include <stdio.h>
#include <stdlib.h>

void orig_f_48bfc(cpu_t *c);
void orig_f_4de84(cpu_t *c);
void orig_f_35a04(cpu_t *c);
uint32_t gfx_screen_handle(void);

#define T_ATTACK "\xb0\xf8\xb0\xdd"                                                         // 공격
#define T_SKILL "\xb0\xad\xc5\xb8"                                                          // 강타
#define T_GUARD "\xb9\xe6\xbe\xee"                                                          // 방어
#define T_ESCAPE "\xb5\xb5\xb8\xc1"                                                         // 도망
#define T_FLED "\xb9\xab\xbb\xe7\xc8\xf7\x20\xb5\xb5\xb8\xc1\xc3\xc6\xb4\xd9\x21"             // 무사히 도망쳤다!
#define T_NOFLEE "\xb5\xb5\xb8\xc1\xc4\xa5\x20\xbc\xf6\x20\xbe\xf8\xbe\xfa\xb4\xd9\x21"       // 도망칠 수 없었다!
#define T_WIN "\xbd\xc2\xb8\xae\x21"                                                        // 승리!
#define T_LOSE "\xc0\xfc\xb8\xea\xc7\xdf\xb4\xd9\x2e\x2e\x2e"                               // 전멸했다...
#define T_NOMP "\x4d\x50\xb0\xa1\x20\xba\xce\xc1\xb7\xc7\xcf\xb4\xd9"                       // MP가 부족하다
#define T_FIRST "\xbc\xb1\xc1\xa6\x20\xb0\xf8\xb0\xdd\x21"                                  // 선제 공격!
#define T_AMBUSH "\xb1\xe2\xbd\xc0\xc0\xbb\x20\xb9\xde\xbe\xd2\xb4\xd9\x21"                 // 기습을 받았다!
#define T_HERO "\xc4\xab\xc0\xcc"                                                           // 카이
#define T_TARGET "\xb4\xeb\xbb\xf3\xc0\xbb\x20\xb0\xed\xb8\xa3\xbc\xbc\xbf\xe4"             // 대상을 고르세요
#define T_GUARDING "\xb9\xe6\xbe\xee\x20\xc0\xda\xbc\xbc\xb8\xa6\x20\xc3\xeb\xc7\xdf\xb4\xd9" // 방어 자세를 취했다
#define T_BOSS_FLED "\xb4\xde\xbe\xc6\xb3\xb5\xb4\xd9\x21"                                      // 달아났다!
#define T_STUNNED "\xb1\xe2\xc0\xfd\xc7\xdf\xb4\xd9\x21" // 기절했다!
#define T_STUN_SKIP "\xb1\xe2\xc0\xfd\xc7\xd8\xbc\xad\x20\xbf\xf2\xc1\xf7\xc0\xcc\xc1\xf6\x20\xb8\xf8\xc7\xd1\xb4\xd9" // 기절해서 움직이지 못한다
#define T_GLOW "\xb8\xf6\xc0\xcc\x20\xba\xfb\xb3\xaa\xb1\xe2\x20\xbd\xc3\xc0\xdb\xc7\xdf\xb4\xd9\x21" // 몸이 빛나기 시작했다!
#define T_EXPLODE "\xc6\xf8\xb9\xdf\xc7\xdf\xb4\xd9\x21" // 폭발했다!
#define T_STUN_TAG "\xb1\xe2\xc0\xfd" // 기절
#define T_LOOT "\xc8\xb9\xb5\xe6" // 획득
#define T_LOOT_FULL "\xb0\xa1\xb9\xe6\xc0\xcc\x20\xb0\xa1\xb5\xe6\x20\xc2\xf7\xbc\xad\x20\xc1\xdd\xc1\xf6\x20\xb8\xf8\xc7\xd1\x20\xb0\xcd\xc0\xcc\x20\xc0\xd6\xb4\xd9" // 가방이 가득 차서 줍지 못한 것이 있다
#define T_ENCOUNTER "\xb8\xf3\xbd\xba\xc5\xcd\xbf\xcd\x20\xb8\xb6\xc1\xd6\xc3\xc6\xb4\xd9\x21" // 몬스터와 마주쳤다!

enum {
    // 게임 함수
    FN_HERO = 0x147434,       // 주인공 개체(0x1b0e58)
    FN_CONTACT = 0x148ba4,    // (몬스터, 0) 몸통이 주인공 맞는 상자와 겹치면 참
    FN_DAMAGE = 0x14ad54,     // (몬스터, -피해, 종류) HP를 깎고 죽음 판정
    FN_MON_DEF = 0x14ebf0,    // (몬스터, 0 물리/1 마도) 방어
    FN_MON_ATK = 0x14e494,    // (몬스터, 공격 번호) 공격력
    FN_MON_ATK_KIND = 0x14e454, // (몬스터) 공격 종류(0 물리, 1 마도)
    FN_MON_CRIT = 0x14ebb4,   // (몬스터) 치명타 %
    FN_HERO_ATK = 0x101ff4,   // (슬롯) 무기 공격력
    FN_HERO_ATK_PCT = 0x102188, // (슬롯) 공격 배율 %
    FN_HERO_DEF = 0x101ef4,   // (0 물리/1 마도) 장비 방어 합
    FN_HERO_DEF_CUT = 0x101f6c, // (방어, 종류, 치명타) 추가 감소
    FN_RACE_ATK = 0x12e514,   // (주인공, 몬스터 속성) 주인공이 줄 때 상성 %
    FN_RACE_DEF = 0x12e6ec,   // (주인공, 몬스터 속성) 주인공이 받을 때 상성 %
    FN_REWARD = 0x14aa54,     // (몬스터) 드랍, 경험치, 사냥 카운트, 도감
    FN_SET_CMD = 0x14e580,    // (몬스터, 명령, 방향, 0)
    FN_APPLY_CMD = 0x14e814,  // (몬스터) 명령을 바로 반영
    FN_EQUIP = 0x128f7c,      // (장비칸) 그림 번호(+1, 없으면 0)
    FN_ATTACKABLE = 0x14b2cc, // (몬스터) 주인공이 칠 수 있는 상태(이 맵에 있음, 도주 중 아님, 무적 아님). 도주가 끝난 보스는 원래대로 돌린다
    FN_ON_MAP = 0x14b460,     // (몬스터) 지금 맵에 있음(보스는 +0x194 맵)
    FN_ESCAPING3 = 0x14ed34, FN_ESCAPING4 = 0x14ed6c, // (보스) 도주 중
    FN_END_ESCAPE = 0x14a2d8, // (보스, 1) 도주 동작을 끝낸다
    FN_PANEL = 0x172aa8,
    FN_SPR_DRAW = 0x17ead0,   // (화면, 그림, 동작, 방향, &프레임, x, y, 반전, 시트(-1 전부), 0)
    FN_SPR_ALPHA = 0x17f438,
    FN_SHADOW = 0x1046f0, FN_MON_KIND = 0x14eb8c, FN_MON_BIG = 0x14ec44, FN_MON_NO_SHADOW_A = 0x14ec14, FN_MON_NO_SHADOW_B = 0x14b3b8,  // (그림, 켜기, 투명도) 필드 사라지기(0x149064)
    FN_SPR_INIT = 0x17df3c, FN_SPR_FREE = 0x17dfc4, // 그림 객체 비우기, 풀기
    FN_LOAD_EMS = 0x14efb8,   // (몬스터) +0xaa 묶음과 효과 표를 +0x38 그림에 싣는다
    FN_LOAD_SHEETS = 0x14f040, // (몬스터) +0x10c 그림 3장, +0x112 팔레트 3개
    FN_NUM_INIT = 0x118420, FN_NUM_DRAW = 0x118528, FN_NUM_STEP = 0x1184bc,
    SPR_FRAME = 0x6a, SPR_HIT_X = 0, SPR_HIT_Y = 4, SPR_TINT = 0x60, SPR_ANIMS = 0x3c, SPR_ANIM_TABLE = 0x44,
    FN_DRAW_LINE = 0x12c50c,
    FN_SET_COLOR = 0x12d734,

    GOT_SCREEN = 0x2c, SCREEN_W = 0x48, SCREEN_H = 0x4c, GOT_MAP = 0x13c,
    GOT_MONSTERS = 0x20, MONSTER_SLOTS = 50, MONSTER_COUNT = 0xc8,

    // 몬스터(0x2bc)와 주인공은 같은 구조다
    E_X = 0x8, E_Y = 0xc, E_ACTIVE = 0x18, E_MAP = 0x1c, E_EMD = 0xa8, E_STATE = 0xac, E_DIR = 0xae,
    E_GRADE = 0xb1, E_RACE = 0xb3, E_NAME = 0xd0, E_HP = 0xec, E_HPMAX = 0xf0, E_LEVEL = 0x101,
    E_RESPAWNS = 0x138, E_SIZE = 0x2bc, E_SPRITE = 0x38, E_EMS = 0xaa, E_IMG = 0x10c, E_PAL = 0x112, E_LIFT = 0x29c, E_SHADOW_W = 0xc4, E_SHADOW_H = 200,
    // 보스(0x14ad54 안의 0x14b1a8, 0x14b208): 누적 피해 +0x1d0이 최대 HP × +0x17e%에 닿으면 도주 3, HP가 최대 × +0x17f% 이하로 처음 내려가면(+0x19a) 도주 4
    E_ESC_CUM_PCT = 0x17e, E_ESC_HP_PCT = 0x17f, E_ESC_HP_DONE = 0x19a, E_ESC_CUM = 0x1d0,
    E_INVULN = 0x295, E_INVULN2 = 0x297, E_SUMMONED = 0x296, E_RUN_TICKS = 0x2b3,
    GOT_SUMMONS = 0xbfc, GOT_STAGE = 0x13c, STAGE_MAP = 0x2d4,
    MAP_PRACTICE_A = 0xb2, MAP_PRACTICE_B = 0xb3, // 0x1750f0: 몬스터가 죽지 않고 주인공도 피해를 받지 않는 맵
    GRADE_KAMIKAZE = 5, // 에너지·자폭병: 원본은 닿으면 터진다(0x148e44). 턴제에서는 1턴 지난 다음 차례에 터진다
    E_STUN_PCT = 0x124, // 원본 경직 문턱(0x14b0e0): 누적 피해 ≥ 최대 HP × 이 값%
    GRADE_OBJECT = 4, // 결계석, 광석·약초 덩이, 책장, 상자: 칠 때마다 떨구는 채집 대상. 전투가 아니라 원본 그대로 친다
    CMD_EXPLODE = 2, E_CMD = 0xad, MON_DYING = 5, MON_DEAD = 7, CMD_DEAD = 100,
    H_CLASS = 0xb1, H_MP = 0x114, H_MPMAX = 0x118, H_LINK = 0x11c,

    MEMBERS = 0x1a9e58, MEMBER_SIZE = 0x74, MEMBER_NAME = 4, MEMBER_LEVEL = 0x35, MEMBER_EMD = 0x48, MEMBER_COUNT = 8,
    STORY_VARS = 0x1a2618 + 0x7b4, STORY_DONE = 2, EXPERT_VAR = 0xff,

    FN_LOAD_SEALED = 0x184b54, FN_FREE_HANDLE = 0x183fd8,
    EM_RECORD_TAIL = 0x30 + 5 * 10,

    TICK_MS = 40,               // 원본 기본 25fps(0x1592a4)
    MAX_POPUPS = 8, NUM_SIZE = 0xa4,
    MAX_ENEMIES = 5, MAX_PARTY = 4, MAX_ACTORS = MAX_ENEMIES + MAX_PARTY,
    GROUP_RANGE = 72,           // 정한 값: 닿은 몬스터에서 이 거리 안의 몬스터가 같이 싸운다
    COOLDOWN_MS = 2500,         // 정한 값: 전투가 끝난 뒤 다시 마주치지 않는 시간
    SKILL_MP = 15,              // 정한 값
    KEY_UP = -1, KEY_DOWN = -2, KEY_LEFT = -3, KEY_RIGHT = -4, KEY_OK = -5, KEY_CLEAR = -16,
};

static const uint8_t JOIN_VAR[MEMBER_COUNT] = {0x03, 0x03, 0x03, 0x03, 0x07, 0x05, 0x05, 0x0f}; // 엔지, 레이, 리키, 후거스, 보우머, 쟝, 펭, 팡

typedef enum { P_IDLE, P_WALK, P_HIT, P_DEAD, P_ATTACK, P_SKILL } pose;

typedef struct {
    bool used, party, hero, alive, guarding;
    char name[28];
    int hp, hpmax, mp, mpmax, atk, pdef, mdef, level, speed, race;
    uint32_t mon;          // 적이면 게임 몬스터 개체
    bool boss;
    int dealt;             // 적이 이번 전투에서 받은 피해 합(끝날 때 원본 피해 함수로 넘긴다)
    bool kamikaze;         // 등급 5. 자기 차례를 한 번 보낸 뒤 다음 차례에 터진다
    bool exploded;         // 스스로 터져 죽었다(정산은 원본 자폭과 같이 HP 전부를 피해로 넘긴다)
    int turns;             // 이번 전투에서 보낸 자기 차례 수
    int stun_pct, stun_acc; // 원본 경직 문턱과 이번 전투 누적 피해
    bool stunned;          // 다음 자기 차례를 쉰다
    bool stun_guard;       // 기절에서 풀린 뒤 한 번 행동할 때까지 다시 기절하지 않는다(원본 경직 사이 대기 +0x1d4 대신)
    uint32_t obj;          // 게임 그림 객체(0x70바이트). 적과 주인공은 필드 개체의 것(+0x38)을 그대로 쓴다
    uint32_t own;          // 동료: 그림을 싣으려고 만든 몬스터 모양 개체(끝날 때 푼다)
    int lift;              // 몬스터 공중 높이(+0x29c, 필드 그리기 0x149064와 같이 뺀다)
    ts_c0 c0;
    bool has_c0;
    int home_x, home_y, x, y;
    pose pose;
    int tick;              // 지금 동작의 경과 틱
    int alpha;
} actor;

static void pop_clear(void);

typedef enum { S_CLOSED, S_INTRO, S_NEXT, S_COMMAND, S_TARGET, S_ACT, S_MESSAGE, S_END } phase;
typedef enum { A_ATTACK, A_SKILL, A_GUARD, A_ESCAPE } action;

static struct {
    phase phase;
    actor a[MAX_ACTORS];
    int n;
    uint16_t *snapshot;
    int w, h, bpl;
    int order[MAX_ACTORS], order_n, order_at;
    int cur, cursor, target;
    action act;
    int step, step_tick, hit_tick;
    char msg[64];
    int msg_ticks;
    bool won, lost, fled, first_strike, ambushed;
    int boss_fled;         // 도주 문턱을 넘은 보스 번호 + 1
    uint64_t last_tick;
    struct { uint32_t num; int actor; } pop[MAX_POPUPS]; // 피해 숫자(원본 숫자 개체)
} B;

static uint32_t pending_monster;
static int pending_kind; // 0 접촉, 1 몬스터 공격, 2 주인공 공격
static uint64_t cooldown_until, check_at;
// 전투에서 이미 터진 자폭 몬스터. 정산(0x14ad54)이 필드에 폭발 명령 2를 주므로 그 동작을 마지막 프레임으로 넘겨
// 바로 끝(0x1495f8 → 0x14ab54 드랍)으로 가게 한다. 폭발을 두 번 보이지 않게 하려는 것이다.
static uint32_t blown[MAX_ENEMIES];
static uint64_t blown_until;
static uint32_t check_mon;
static int check_exp;
static int enabled = -1;

static bool turn_enabled(void) {
    if (enabled < 0) enabled = getenv("CHRONOS_TURN") != NULL;
    return enabled;
}

bool turn_battle_open(void) { return B.phase != S_CLOSED || pending_monster; }

// ---- 게임 값 읽기 ----

static int S8(uint32_t a) { cpu_t *c = &cpu; return (int8_t)R8(a); }
static int S16(uint32_t a) { cpu_t *c = &cpu; return (int16_t)(R8(a) | R8(a + 1) << 8); }
static int S32(uint32_t a) { cpu_t *c = &cpu; return (int32_t)R32(a); }
static void W32s(uint32_t a, int v) { cpu_t *c = &cpu; W32(a, (uint32_t)v); }

static bool monster_fightable(uint32_t m) {
    cpu_t *c = &cpu;
    if (!m || !R8(m + E_ACTIVE)) return false;
    if (S32(m + E_HP) <= 0) return false;
    int st = R8(m + E_STATE);
    if (st == MON_DYING || st == MON_DEAD) return false;
    int grade = R8(m + E_GRADE);
    return grade != GRADE_OBJECT && grade != 6; // 0은 보스, 5는 에너지·자폭병(닿으면 터진다), 6은 쓰는 곳이 없다
}

static uint32_t list_at(int got_off, int i) {
    cpu_t *c = &cpu;
    return R32(got(got_off) + 4 * (uint32_t)i);
}

static int list_count(int got_off) {
    cpu_t *c = &cpu;
    int n = (int)R32(got(got_off) + MONSTER_COUNT);
    return n < 0 ? 0 : n > MONSTER_SLOTS ? MONSTER_SLOTS : n;
}

static uint32_t monster_at(int i) { return list_at(GOT_MONSTERS, i); }
static int monster_count(void) { return list_count(GOT_MONSTERS); }

static bool practice_map(void) {
    cpu_t *c = &cpu;
    int map = S32(got(GOT_STAGE) + STAGE_MAP);
    return map == MAP_PRACTICE_A || map == MAP_PRACTICE_B;
}

// 지금 맵에서 같이 싸울 수 있는 몬스터. 보스는 맵을 옮겨 다니므로 원본 판정(0x14b460)을 쓴다.
static bool monster_here(uint32_t m) { return monster_fightable(m) && CALL(FN_ON_MAP, m); }

typedef struct { bool ok; int ems, img[3], pal[3], c0; } em_look;

// EMD/M 레코드(0x14f73c): 이름 뒤 +2 묶음, +4 그림 3개, +10 팔레트 3개, +16 C0 번호
static em_look em_load(int id) {
    em_look r = {0};
    cpu_t *c = &cpu;
    uint32_t mem = heap_alloc(0x48);
    snprintf(gstr(mem), 32, "/EMD/M/%03d", id / 10);
    uint32_t h = CALL(FN_LOAD_SEALED, mem);
    if (h) {
        const uint8_t *d = c->m + handle_data(h);
        size_t at = 2;
        for (int i = 0; i < id % 10; i++) at += 1u + d[at] + EM_RECORD_TAIL;
        const uint8_t *b = d + at + 1 + d[at];
        #define LE16(p) ((int16_t)((p)[0] | (p)[1] << 8))
        r.ems = LE16(b + 2);
        for (int i = 0; i < 3; i++) { r.img[i] = LE16(b + 4 + 2 * i); r.pal[i] = LE16(b + 10 + 2 * i); }
        r.c0 = LE16(b + 16);
        #undef LE16
        r.ok = true;
        W32(mem + 0x44, h);
        CALL(FN_FREE_HANDLE, mem + 0x44);
    }
    heap_free(mem);
    return r;
}

static void load_c0(actor *a, int id) {
    em_look l = em_load(id);
    if (!l.ok) { fprintf(stderr, "[turn] EMD/M %d 읽기 실패\n", id); return; }
    a->has_c0 = ts_c0_load(&a->c0, game_dir, l.c0);
}

// 동료 그림: 필드 몬스터와 같은 원본 경로로 싣는다. 빈 개체에 묶음(+0xaa), 그림(+0x10c), 팔레트(+0x112)를 넣고
// 0x14efb8(묶음, 효과 표), 0x14f040(시트 3장 + 팔레트, 0x17385c)을 부른다. 그리기는 원본 0x17ead0이 한다.
static void load_member_sprite(actor *a, int id) {
    cpu_t *c = &cpu;
    em_look l = em_load(id);
    if (!l.ok) { fprintf(stderr, "[turn] EMD/M %d 읽기 실패\n", id); return; }
    a->has_c0 = ts_c0_load(&a->c0, game_dir, l.c0);
    uint32_t e = heap_alloc(E_SIZE);
    memset(c->m + e, 0, E_SIZE);
    CALL(FN_SPR_INIT, e + E_SPRITE);
    W16(e + E_EMS, (uint16_t)l.ems);
    for (int i = 0; i < 3; i++) { W16(e + E_IMG + 2 * i, (uint16_t)l.img[i]); W16(e + E_PAL + 2 * i, (uint16_t)l.pal[i]); }
    CALL(FN_LOAD_EMS, e);
    CALL(FN_LOAD_SHEETS, e);
    a->own = e;
    a->obj = e + E_SPRITE;
}

static void free_sprite(actor *a) {
    if (a->own) {
        CALL(FN_SPR_FREE, a->obj);
        heap_free(a->own);
    }
    a->own = a->obj = 0;
}

// ---- 전투 준비 ----

static int rnd(int n) { return n > 0 ? rand() % n : 0; }

static void add_enemy(uint32_t m) {
    cpu_t *c = &cpu;
    actor *a = &B.a[B.n++];
    memset(a, 0, sizeof *a);
    a->used = a->alive = true;
    a->mon = m;
    memcpy(a->name, gstr(m + E_NAME), 27);
    a->hp = S32(m + E_HP);
    a->hpmax = S32(m + E_HPMAX);
    a->level = S8(m + E_LEVEL);
    a->race = R8(m + E_RACE);
    a->boss = R8(m + E_GRADE) == 0;
    a->kamikaze = R8(m + E_GRADE) == GRADE_KAMIKAZE;
    a->stun_pct = a->boss ? 0 : S8(m + E_STUN_PCT); // 보스는 기절하지 않는다(사용자 결정)
    a->dealt = 0;
    fprintf(stderr, "[turn] 적 %s 등급 %d HP %d 경직 %d%%\n", a->name, R8(m + E_GRADE), a->hp, a->stun_pct);
    a->speed = 8 + a->level / 2; // 정한 값
    a->alpha = 255;
    a->obj = m + E_SPRITE;
    a->lift = S32(m + E_LIFT);
    load_c0(a, S16(m + E_EMD));
}

static void add_hero(void) {
    cpu_t *c = &cpu;
    uint32_t h = CALL(FN_HERO);
    actor *a = &B.a[B.n++];
    memset(a, 0, sizeof *a);
    a->used = a->alive = a->party = a->hero = true;
    strcpy(a->name, T_HERO);
    a->hp = S32(h + E_HP);
    a->hpmax = S32(h + E_HPMAX);
    a->mp = S32(h + H_MP);
    a->mpmax = S32(h + H_MPMAX);
    a->level = S8(h + E_LEVEL);
    int weapon = (int)CALL(FN_HERO_ATK, 0), pct = (int)CALL(FN_HERO_ATK_PCT, 0);
    a->atk = weapon * pct / 100;
    if (a->atk <= 0) a->atk = 10 + a->level * 4; // 무기가 없으면(정한 값)
    a->speed = 12 + a->level / 2; // 정한 값
    a->alpha = 255;
    fprintf(stderr, "[turn] 주인공 Lv%d HP %d/%d MP %d/%d 공격 %d(무기 %d × %d%%) 방어 %d/%d\n", a->level, a->hp, a->hpmax, a->mp, a->mpmax,
            a->atk, weapon, pct, (int)CALL(FN_HERO_DEF, 0), (int)CALL(FN_HERO_DEF, 1));
    a->obj = h + E_SPRITE; // 주인공 그림은 필드 개체의 것(H/S<클래스>, 장비 시트 4장)
}

// 동료 수치는 클라이언트에 없다. 네트워크 방어진(member_stats)과 같은 정한 값: HP 100+40L, 공격 10+4L, 방어 5+2L
static void add_member(int i) {
    cpu_t *c = &cpu;
    uint32_t e = MEMBERS + (uint32_t)i * MEMBER_SIZE;
    actor *a = &B.a[B.n++];
    memset(a, 0, sizeof *a);
    a->used = a->alive = a->party = true;
    memcpy(a->name, gstr(e + MEMBER_NAME), 27);
    a->level = S8(e + MEMBER_LEVEL);
    if (a->level < 1) a->level = 1;
    a->hp = a->hpmax = 100 + 40 * a->level;
    a->atk = 10 + 4 * a->level;
    a->pdef = a->mdef = 5 + 2 * a->level;
    a->speed = 10 + a->level / 2;
    a->alpha = 255;
    load_member_sprite(a, S16(e + MEMBER_EMD));
}

static bool member_joined(int i) {
    cpu_t *c = &cpu;
    if (getenv("CHRONOS_TURN_ALL")) return true; // 시험용: 모든 동료 합류로 본다
    return R8(STORY_VARS + EXPERT_VAR) == STORY_DONE || R8(STORY_VARS + JOIN_VAR[i]) == STORY_DONE;
}

// 자리: 적은 왼쪽(오른쪽을 봄), 아군은 오른쪽(왼쪽을 봄). 화면 높이 320 기준 위쪽 3/5이 전장이다.
static void place(void) {
    int ne = 0, np = 0, W = B.w;
    for (int i = 0; i < B.n; i++) {
        actor *a = &B.a[i];
        if (a->party) {
            a->home_x = W * 68 / 100 + (np & 1) * 34 + np * 4; // 두 줄로 엇갈려 선다
            a->home_y = 84 + np * 32;
            np++;
        } else {
            static const int ox[MAX_ENEMIES] = {0, -36, -36, -72, -72}, oy[MAX_ENEMIES] = {0, -36, 36, -18, 54};
            a->home_x = W * 30 / 100 + ox[ne];
            a->home_y = 140 + oy[ne];
            ne++;
        }
        a->x = a->home_x;
        a->y = a->home_y;
    }
}

static void set_message(const char *s, int ticks) {
    snprintf(B.msg, sizeof B.msg, "%s", s);
    B.msg_ticks = ticks;
}

static void draw_text(int x, int y, int width, const char *text, uint32_t color, int center);
static void fill(int x, int y, int w, int h, int r, int g, int b);

// 전투 뒤 자동 줍기. 드랍은 *(GOT 0x15c) 목록의 개체(+0x24 종류, +0x22 id, +0x25 이름)로 생긴다(0x117d7c).
// 원본 줍기 0x143f1c(사각형)를 드랍마다 불러 가방 넣기, 원본 획득 표시, 수집 퀘스트 집계를 원본 그대로 탄다.
enum {
    GOT_DROPS = 0x15c, DROP_ID = 0x22, DROP_KIND = 0x24, DROP_NAME = 0x25,
    FN_PICKUP = 0x143f1c,
    LOOT_WINDOW_MS = 4000,  // 처치 동작이 끝나야 드랍이 생기므로 전투 뒤 이 시간 동안 줍는다
    LOOT_SHOW_MS = 3500,
    LOOT_LINES = 4, DROP_TRACK = 64,
};
static uint32_t loot_old[DROP_TRACK], loot_tried[DROP_TRACK];
static int loot_old_n, loot_tried_n;
static uint64_t loot_until, loot_show_until;
static struct { char name[32]; int n; } loot[LOOT_LINES];
static int loot_n;
static bool loot_full;

static int drops_list(uint32_t *out, int max) {
    cpu_t *c = &cpu;
    int k = 0;
    uint32_t n = R32(got(GOT_DROPS));
    for (int i = 0; n && i < 200 && k < max; i++, n = R32(n + 4)) {
        uint32_t e = R32(n + 0x10);
        if (e) out[k++] = e;
    }
    return k;
}

static bool in_set(const uint32_t *set, int n, uint32_t e) {
    for (int i = 0; i < n; i++) if (set[i] == e) return true;
    return false;
}

static void loot_add(const char *name) {
    fprintf(stderr, "[turn] 자동 줍기: %s\n", name);
    for (int i = 0; i < loot_n; i++) if (!strcmp(loot[i].name, name)) { loot[i].n++; return; }
    if (loot_n < LOOT_LINES) { snprintf(loot[loot_n].name, sizeof loot[0].name, "%s", name); loot[loot_n++].n = 1; }
    else loot[LOOT_LINES - 1].n++;
}

static void loot_tick(void) {
    if (!loot_until) return;
    uint64_t now = now_ms();
    if (now >= loot_until) { loot_until = 0; return; }
    cpu_t *c = &cpu;
    static uint32_t rect;
    if (!rect) rect = heap_alloc(16);
    uint32_t before[DROP_TRACK], after[DROP_TRACK];
    int nb = drops_list(before, DROP_TRACK);
    for (int i = 0; i < nb; i++) {
        uint32_t e = before[i];
        if (in_set(loot_old, loot_old_n, e) || in_set(loot_tried, loot_tried_n, e)) continue;
        char names[DROP_TRACK][32];
        for (int k = 0; k < nb; k++) snprintf(names[k], sizeof names[0], "%s", gstr(before[k] + DROP_NAME));
        W32s(rect, S32(e + 8) - 4);
        W32s(rect + 4, S32(e + 0xc) - 4);
        W32s(rect + 8, 8);
        W32s(rect + 12, 8);
        CALL(FN_PICKUP, rect);
        int na = drops_list(after, DROP_TRACK);
        bool got_e = false;
        for (int k = 0; k < nb; k++)
            if (!in_set(after, na, before[k])) { loot_add(names[k]); got_e |= before[k] == e; loot_show_until = now + LOOT_SHOW_MS; }
        if (!got_e) { // 가방이 차면 남는다. 같은 드랍을 다시 시도하지 않는다
            if (loot_tried_n < DROP_TRACK) loot_tried[loot_tried_n++] = e;
            loot_full = true;
            fprintf(stderr, "[turn] 자동 줍기 실패(가방): %s\n", gstr(e + DROP_NAME));
            loot_show_until = now + LOOT_SHOW_MS;
        }
        return; // 한 틱에 하나씩(원본 획득 표시가 겹치지 않게)
    }
}

// 필드 HUD 위에 자동 줍기 결과를 띄운다(padhint.c의 0x1444cc 감싸기에서 부른다).
void turn_loot_draw(void) {
    if (!loot_show_until || now_ms() >= loot_show_until || B.phase != S_CLOSED) return;
    cpu_t *c = &cpu;
    uint32_t sh = gfx_screen_handle();
    if (!sh) return;
    int w = (int)R32(handle_data(sh));
    int lines = loot_n + (loot_full ? 1 : 0), y = 34;
    if (!lines) return;
    fill(w / 2 - 110, y - 3, 220, lines * 14 + 6, 0x10, 0x18, 0x30);
    for (int i = 0; i < loot_n; i++, y += 14) {
        char s[64];
        snprintf(s, sizeof s, T_LOOT " %s x%d", loot[i].name, loot[i].n);
        draw_text(w / 2, y, 216, s, 0xffe060, 1);
    }
    if (loot_full) draw_text(w / 2, y, 216, T_LOOT_FULL, 0xff8080, 1);
}

static void open_battle(uint32_t first, int kind) {
    cpu_t *c = &cpu;
    uint32_t sh = gfx_screen_handle();
    if (!sh) return;
    uint32_t f = handle_data(sh);
    B.w = (int)R32(f); B.h = (int)R32(f + 4); B.bpl = (int)R32(f + 8);
    uint8_t *px = c->m + handle_data(R32(f + 16));
    B.snapshot = realloc(B.snapshot, (size_t)B.h * B.bpl);
    memcpy(B.snapshot, px, (size_t)B.h * B.bpl);
    // 배경 버퍼(화면 구조체 +4): 타일과 바닥 오브젝트만 구운 그림이라 캐릭터가 없다. 카메라(맵 구조체 +0x88, +0x8c)로 잘라 쓴다.
    uint32_t bgh = R32(got(GOT_SCREEN) + 4);
    if (bgh) {
        uint32_t bf = handle_data(bgh);
        int bw = (int)R32(bf), bh = (int)R32(bf + 4), bbpl = (int)R32(bf + 8);
        const uint8_t *bp = c->m + handle_data(R32(bf + 16));
        uint32_t map = got(GOT_MAP);
        int cx = S32(map + 0x88), cy = S32(map + 0x8c);
        fprintf(stderr, "[turn] 배경 버퍼 %dx%d, 화면 %dx%d, 카메라 %d,%d\n", bw, bh, B.w, B.h, cx, cy);
        if (bw >= B.w && bh >= B.h) {
            if (cx < 0) cx = 0;
            if (cy < 0) cy = 0;
            if (cx > bw - B.w) cx = bw - B.w;
            if (cy > bh - B.h) cy = bh - B.h;
            for (int y = 0; y < B.h; y++) memcpy((uint8_t *)B.snapshot + (size_t)y * B.bpl, bp + (size_t)(y + cy) * bbpl + (size_t)cx * 2, (size_t)B.w * 2);
        }
    }
    // 어둡게
    for (int y = 0; y < B.h; y++) {
        uint16_t *row = (uint16_t *)((uint8_t *)B.snapshot + (size_t)y * B.bpl);
        for (int x = 0; x < B.w; x++) {
            uint16_t p = row[x];
            row[x] = (uint16_t)(((p >> 11) * 3 / 5) << 11 | ((p >> 5 & 63) * 3 / 5) << 5 | (p & 31) * 3 / 5);
        }
    }
    B.n = 0;
    B.boss_fled = 0;
    loot_old_n = drops_list(loot_old, DROP_TRACK); // 전투 전부터 바닥에 있던 드랍은 줍지 않는다
    loot_until = 0;
    add_enemy(first);
    // 소환수를 부른 몬스터는 소환수가 다 죽을 때까지 무적이다(+0x295). 소환수를 먼저 모두 넣는다.
    for (int i = 0; i < list_count(GOT_SUMMONS) && B.n < MAX_ENEMIES; i++) {
        uint32_t m = list_at(GOT_SUMMONS, i);
        if (m != first && monster_here(m)) add_enemy(m);
    }
    int fx = S32(first + E_X), fy = S32(first + E_Y);
    for (int i = 0; i < monster_count() && B.n < MAX_ENEMIES; i++) {
        uint32_t m = monster_at(i);
        bool dup = false;
        for (int k = 0; k < B.n; k++) dup |= B.a[k].mon == m;
        if (dup || !monster_here(m)) continue;
        if (abs(S32(m + E_X) - fx) <= GROUP_RANGE && abs(S32(m + E_Y) - fy) <= GROUP_RANGE) add_enemy(m);
    }
    add_hero();
    uint32_t hero = CALL(FN_HERO);
    uint32_t link = R32(hero + H_LINK);
    int linked = link >= MEMBERS && link < MEMBERS + MEMBER_COUNT * MEMBER_SIZE ? (int)((link - MEMBERS) / MEMBER_SIZE) : -1;
    if (linked >= 0 && member_joined(linked)) add_member(linked);
    for (int i = 0; i < MEMBER_COUNT && B.n < MAX_ACTORS; i++)
        if (i != linked && member_joined(i) && B.n - 0 < MAX_ACTORS) {
            int party = 0;
            for (int k = 0; k < B.n; k++) party += B.a[k].party;
            if (party >= MAX_PARTY) break;
            add_member(i);
        }
    place();
    B.first_strike = kind == 2;
    B.ambushed = kind == 1;
    B.won = B.lost = B.fled = false;
    B.order_n = B.order_at = 0;
    pop_clear();
    B.phase = S_INTRO;
    B.step_tick = 0;
    set_message(B.first_strike ? T_FIRST : B.ambushed ? T_AMBUSH : T_ENCOUNTER, 30);
    B.last_tick = now_ms();
    int enemies = 0;
    for (int i = 0; i < B.n; i++) enemies += !B.a[i].party;
    fprintf(stderr, "[turn] 전투 시작: 적 %d, 아군 %d, 계기 %d\n", enemies, B.n - enemies, kind);
}

// ---- 그림 ----

static int pose_state(const actor *a, pose p) {
    switch (p) {
    case P_IDLE: return 0;
    case P_WALK: return 1;
    // 자폭 몬스터(등급 5)는 상태 2가 폭발이다: 원본은 죽을 때 명령 2를 주고(0x14a9f4) 그 동작이 끝나면 지운다(0x1495f8 → 0x14ab54).
    // 그래서 맞을 때는 대기 동작을 쓰고, 죽을 때(스스로 터지거나 맞아 죽거나) 상태 2를 끝까지 재생한다.
    case P_HIT: return a->kamikaze ? 0 : 2;
    case P_DEAD: return a->kamikaze ? 2 : 3;
    default: return a->boss ? 9 : 7; // 공격 번호 = 상태 - 7(보스 - 9), 0x148980
    }
}

// 주인공 동작(0x1336d8): 2 전투 대기, 3 전투 이동, 5 피격, 8 사망, 9~ 공격과 스킬(H/DS)
static int hero_anim(pose p) {
    switch (p) {
    case P_IDLE: return 2;
    case P_WALK: return 3;
    case P_HIT: return 5;
    case P_DEAD: return 8;
    case P_SKILL: return 12;
    default: return 9;
    }
}

// 그림 객체(0x17e078이 채운다): +0x3c 동작 수, +0x44 핸들 → 동작마다 0x48바이트, 앞 4바이트가 방향별 프레임 수(0x17f44c)
static int obj_anims(const actor *a) {
    cpu_t *c = &cpu;
    return a->obj ? R8(a->obj + SPR_ANIMS) : 0;
}

// 프레임 수. 그 방향이 비어 있으면 방향 0(정면)으로 바꾼다. 방향 3은 2를 좌우로 뒤집어 쓴다(0x17f44c).
static int obj_frames(const actor *a, int anim, int *dir) {
    cpu_t *c = &cpu;
    if (anim < 0 || anim >= obj_anims(a)) return 0;
    uint32_t t = R32(R32(a->obj + SPR_ANIM_TABLE)) + 8 + (uint32_t)anim * 0x48;
    int n = R8(t + (*dir == 3 ? 2 : *dir));
    if (!n && *dir) { *dir = 0; n = R8(t); }
    return n;
}

// 방향 0~3: 0 위, 1 아래, 2 왼쪽, 3 오른쪽(2의 좌우 반전). 필드에서 왼쪽 키 → +0xae 2, 오른쪽 키 → 3으로 확인.
// 적(왼쪽 자리)은 오른쪽, 아군(오른쪽 자리)은 왼쪽을 본다.
static void pose_anim(const actor *a, int *anim, int *dir) {
    int face = a->party ? 2 : 3;
    if (a->hero) {
        *anim = hero_anim(a->pose);
        if (*anim >= obj_anims(a)) *anim = 9;
        *dir = face;
        return;
    }
    int st = pose_state(a, a->pose);
    if (a->has_c0 && st < a->c0.count && a->c0.anim[st][face] >= 0) {
        *anim = a->c0.anim[st][face];
        *dir = a->c0.dir[st][face];
    } else if (a->has_c0 && a->c0.anim[0][face] >= 0) {
        *anim = a->c0.anim[0][face];
        *dir = a->c0.dir[0][face];
    } else {
        *anim = st;
        *dir = face;
    }
}

// 발밑 그림자 0x1046f0(개체, x, y, 폭, 높이). 필드와 같은 조건과 크기: 몬스터는 0x149064(공중·특수 종류는 없음, 폭 0x16, 큰 몬스터 0x32,
// 높이 +200), 주인공은 0x13549c(폭 +0xc4, 높이 +200, y - 2). 동료는 필드 개체가 없어 주인공 크기를 쓴다(정한 값).
static void draw_shadow(const actor *a) {
    cpu_t *c = &cpu;
    if (a->alpha < 255 || (a->kamikaze && !a->alive)) return;
    if (a->party) {
        uint32_t h = CALL(FN_HERO);
        CALL(FN_SHADOW, h, a->x, a->y - 2, R32(h + E_SHADOW_W), R32(h + E_SHADOW_H));
        return;
    }
    uint32_t m = a->mon;
    if (CALL(FN_MON_NO_SHADOW_A, m) & 0xff || CALL(FN_MON_NO_SHADOW_B, m) & 0xff) return;
    int kind = (int16_t)CALL(FN_MON_KIND, m);
    if (kind == 2 || kind == 3 || kind == 4 || kind == 5 || kind == 13 || kind == 14) return;
    CALL(FN_SHADOW, m, a->x, a->y - a->lift, CALL(FN_MON_BIG, m) & 0xff ? 0x32 : 0x16, R32(m + E_SHADOW_H));
}

// 원본 그리기 0x17ead0(화면, 그림, 동작, 방향, &프레임, x, y, 0, -1, 0)를 그대로 부른다. 필드 그리기(0x149064, 0x13549c)와 같다.
// 필드에서 걸어 둔 색 효과(+0x60~+0x65)는 끄고 그린 뒤 되돌린다. 투명도는 원본 사라지기와 같은 0x17f438(그림, 1, 값).
static void draw_actor(const actor *a, int anim, int dir, int frame) {
    cpu_t *c = &cpu;
    static uint32_t fbyte;
    if (!fbyte) fbyte = heap_alloc(4);
    uint8_t tint[6];
    memcpy(tint, c->m + a->obj + SPR_TINT, 6);
    W8(a->obj + SPR_TINT + 5, 0);
    W8(fbyte, (uint8_t)frame);
    if (a->alpha < 255) CALL(FN_SPR_ALPHA, a->obj, 1, a->alpha);
    CALL(FN_SPR_DRAW, R32(got(GOT_SCREEN)), a->obj, anim, dir, fbyte, a->x, a->y - a->lift, 0, 0xffffffffu, 0);
    if (a->alpha < 255) CALL(FN_SPR_ALPHA, a->obj, 0, 0xff);
    memcpy(c->m + a->obj + SPR_TINT, tint, 6);
}

static int pose_frames(const actor *a) {
    int anim, dir;
    pose_anim(a, &anim, &dir);
    return obj_frames(a, anim, &dir);
}

static bool pose_loops(pose p) { return p == P_IDLE || p == P_WALK; }

static void set_pose(actor *a, pose p) { a->pose = p; a->tick = 0; }

static void draw_text(int x, int y, int width, const char *text, uint32_t color, int center) {
    static uint32_t buf;
    if (!buf) buf = heap_alloc(80);
    int len = (int)strlen(text);
    if (len > 78) len = 78;
    memcpy(gstr(buf), text, len);
    gstr(buf)[len] = 0;
    cpu_t *c = &cpu;
    // 색은 0x12d734로 먼저 정하고 0x12c50c에는 -1(지금 색 유지)을 넘긴다. 대사 기록 창(dialog.c)과 같은 방식.
    CALL(FN_SET_COLOR, color >> 16 & 0xff, color >> 8 & 0xff, color & 0xff);
    CALL(FN_DRAW_LINE, R32(got(GOT_SCREEN)), x, y, width, buf, 0, 0, len, 0xffffffffu, 0, center);
}

static void fill(int x, int y, int w, int h, int r, int g, int b) {
    cpu_t *c = &cpu;
    CALL(FN_FILL_RECT, R32(got(GOT_SCREEN)), x, y, w, h, r, g, b);
}

static void bar(int x, int y, int w, int v, int max, int r, int g, int b) {
    fill(x, y, w, 3, 0x20, 0x20, 0x20);
    if (max > 0 && v > 0) fill(x, y, w * (v > max ? max : v) / max, 3, r, g, b);
}

static void draw_scene(void) {
    cpu_t *c = &cpu;
    uint32_t sh = gfx_screen_handle();
    uint32_t f = handle_data(sh);
    uint8_t *px = c->m + handle_data(R32(f + 16));
    memcpy(px, B.snapshot, (size_t)B.h * B.bpl);
    int panel_y = B.h - 96;
    // 발 y 순서로 그린다
    int idx[MAX_ACTORS];
    for (int i = 0; i < B.n; i++) idx[i] = i;
    for (int i = 1; i < B.n; i++)
        for (int k = i; k > 0 && B.a[idx[k]].y < B.a[idx[k - 1]].y; k--) { int s = idx[k]; idx[k] = idx[k - 1]; idx[k - 1] = s; }
    for (int i = 0; i < B.n; i++) {
        actor *a = &B.a[idx[i]];
        if (!a->used || a->alpha <= 0) continue;
        if (!a->obj) continue;
        int anim, dir;
        pose_anim(a, &anim, &dir);
        int n = obj_frames(a, anim, &dir);
        if (!n) continue;
        int fr = pose_loops(a->pose) ? a->tick % n : a->tick < n ? a->tick : n - 1;
        draw_shadow(a);
        draw_actor(a, anim, dir, fr);
    }
    // 고르는 대상 표시
    if (B.phase == S_TARGET) {
        actor *a = &B.a[B.target];
        fill(a->x - 3, a->y + 4, 7, 3, 0xff, 0xe0, 0x40);
    } else if (B.phase == S_COMMAND) {
        actor *a = &B.a[B.cur];
        fill(a->x - 3, a->y + 4, 7, 3, 0x60, 0xd0, 0xff);
    }
    // 피해 숫자
    for (int k = 0; k < MAX_POPUPS; k++) {
        if (!B.pop[k].num) continue;
        const actor *a = &B.a[B.pop[k].actor];
        CALL(FN_NUM_DRAW, B.pop[k].num, a->x + S32(a->obj + SPR_HIT_X), a->y - a->lift + S32(a->obj + SPR_HIT_Y));
        if (CALL(FN_NUM_STEP, B.pop[k].num) & 0xff) { heap_free(B.pop[k].num); B.pop[k].num = 0; }
    }
    // 위쪽 알림
    if (B.msg_ticks > 0 && B.msg[0]) {
        fill(0, 6, B.w, 18, 0x10, 0x18, 0x30);
        draw_text(B.w / 2, 9, B.w, B.msg, 0xe8d6ab, 1);
    }
    // 아래 창: 왼쪽 명령, 오른쪽 아군 상태
    CALL(FN_PANEL, 0, panel_y, B.w, B.h - panel_y);
    fill(2, panel_y + 2, B.w - 4, B.h - panel_y - 4, 0x10, 0x18, 0x30);
    int cmd_w = 70;
    if (B.phase == S_COMMAND) {
        static const char *cmds[4] = {T_ATTACK, T_SKILL, T_GUARD, T_ESCAPE};
        bool hero = B.a[B.cur].hero;
        for (int i = 0; i < 4; i++) {
            bool usable = hero || i != 1;
            uint32_t col = i == B.cursor ? 0xffe060 : usable ? 0xffffff : 0x707070;
            if (i == B.cursor) fill(8, panel_y + 8 + i * 20, cmd_w - 12, 17, 0x30, 0x5f, 0xae);
            draw_text(14, panel_y + 10 + i * 20, cmd_w, cmds[i], col, 0);
        }
    } else if (B.phase == S_TARGET) {
        draw_text(cmd_w / 2 + 4, panel_y + 10, cmd_w, T_TARGET, 0xe8d6ab, 1);
    }
    int px0 = cmd_w + 8, row = 0;
    for (int i = 0; i < B.n; i++) {
        actor *a = &B.a[i];
        if (!a->party) continue;
        int y = panel_y + 8 + row * 21;
        uint32_t col = !a->alive ? 0x808080 : (B.phase == S_COMMAND && i == B.cur) ? 0xffe060 : 0xffffff;
        draw_text(px0, y, 70, a->name, col, 0);
        char s[32];
        snprintf(s, sizeof s, "%d/%d", a->hp, a->hpmax);
        int bx = px0 + 62, bw = B.w - bx - 10;
        if (bw > 120) bw = 120;
        draw_text(bx, y - 1, bw, s, col, 0);
        bar(bx, y + 13, bw, a->hp, a->hpmax, 0xe0, 0x40, 0x40);
        if (a->hero) bar(bx, y + 17, bw, a->mp, a->mpmax, 0x40, 0x80, 0xff);
        row++;
    }
    // 적 HP(이름과 막대)
    for (int i = 0; i < B.n; i++) {
        actor *a = &B.a[i];
        if (a->party || !a->alive) continue;
        bar(a->home_x - 20, a->home_y + 6, 40, a->hp, a->hpmax, 0xe0, 0x40, 0x40);
        if (a->stunned) draw_text(a->home_x, a->home_y + 10, 60, T_STUN_TAG, 0xffe060, 1);
    }
    video_present(sh);
}

// ---- 전투 규칙(정한 값이 많다. work/turn-plan.md "시제품") ----

static int alive_count(bool party) {
    int n = 0;
    for (int i = 0; i < B.n; i++) n += B.a[i].party == party && B.a[i].alive;
    return n;
}

static void build_order(void) {
    B.order_n = 0;
    for (int i = 0; i < B.n; i++)
        if (B.a[i].alive) B.order[B.order_n++] = i;
    // 빠르기 순, 같으면 아군 먼저
    for (int i = 1; i < B.order_n; i++)
        for (int k = i; k > 0; k--) {
            actor *p = &B.a[B.order[k - 1]], *q = &B.a[B.order[k]];
            int sp = p->speed + rnd(4), sq = q->speed + rnd(4);
            if (sq > sp) { int s = B.order[k]; B.order[k] = B.order[k - 1]; B.order[k - 1] = s; }
        }
    B.order_at = 0;
}

static int first_alive(bool party) {
    for (int i = 0; i < B.n; i++)
        if (B.a[i].party == party && B.a[i].alive) return i;
    return -1;
}

// 아군 → 적: 원본 0x135a04와 같은 꼴(공격력 - 몬스터 방어, 최소 1, + 상성 0x102240 = (공격력-방어)×0x12e514%)
static int damage_to_enemy(actor *from, actor *to, bool skill) {
    cpu_t *c = &cpu;
    int atk = from->atk * (skill ? 180 : 100) / 100;
    atk = atk * (90 + rnd(21)) / 100;
    int def = (int)CALL(FN_MON_DEF, to->mon, 0);
    int dmg = atk - def;
    if (dmg < 1) dmg = 1;
    if (from->hero && atk - def > 0) dmg += (atk - def) * (int)CALL(FN_RACE_ATK, CALL(FN_HERO), (uint32_t)to->race) / 100;
    return dmg;
}

// 적 → 주인공: 원본 0x14de84(공격력, 치명타, 방어 0x101ef4 + 감소 0x101f6c, 상성 0x12e6ec). 적 → 동료: 공격력 - 방어(정한 값)
static int damage_to_party(actor *from, actor *to) {
    cpu_t *c = &cpu;
    uint32_t m = from->mon;
    int kind = (int)CALL(FN_MON_ATK_KIND, m);
    int atk = (int)CALL(FN_MON_ATK, m, 0);
    bool crit = rnd(100) <= (int)(int8_t)CALL(FN_MON_CRIT, m);
    if (crit) atk = atk * (S8(m + 0x178) * 7 + 100) / 100;
    atk = atk * (90 + rnd(21)) / 100;
    int dmg;
    if (to->hero) {
        int def = (int)CALL(FN_HERO_DEF, kind), cut = (int)CALL(FN_HERO_DEF_CUT, def, kind, crit);
        dmg = atk - (int16_t)(def + cut);
        dmg += dmg * (int)CALL(FN_RACE_DEF, CALL(FN_HERO), (uint32_t)from->race) / 100;
    } else {
        dmg = atk - (kind ? to->mdef : to->pdef);
    }
    if (dmg < 1) dmg = 1;
    if (to->guarding) dmg /= 2;
    return dmg < 1 ? 1 : dmg;
}

// 원본 0x14b1a8, 0x14b208과 같은 문턱. 넘으면 전투를 끝내고 필드에서 원본이 도주시킨다.
static bool boss_escapes(const actor *a) {
    cpu_t *c = &cpu;
    uint32_t m = a->mon;
    if (!a->boss || !a->alive || S8(m + E_RUN_TICKS) > 0) return false;
    int pct = S8(m + E_ESC_CUM_PCT);
    if (pct > 0 && abs(S32(m + E_ESC_CUM) - a->dealt) >= a->hpmax * pct / 100) return true;
    pct = S8(m + E_ESC_HP_PCT);
    return pct > 0 && !R8(m + E_ESC_HP_DONE) && a->hp <= a->hpmax * pct / 100;
}

// 피해 숫자: 필드의 0x10264c와 같이 숫자 개체를 0x118420(개체, 종류, 0, 값)으로 채운다. 종류 0은 몬스터가 받은 피해(주황 숫자)다.
// 원작은 주인공이 맞으면 숫자 없이 HUD 막대만 줄이므로(0x103ebc → 0x144100) 아군 피해도 같은 종류 0으로 보인다(정한 값). 필드는 개체 목록(+0x20)에 붙여 0x103ab8이 그리지만 전투는 멈춰 있으므로 직접 그린다:
// 0x118528(개체, x, y)로 그리고 0x1184bc가 참이면 끝. 위치는 그림의 맞는 점(그림 +0, +4)이다.
static void pop_clear(void) {
    for (int k = 0; k < MAX_POPUPS; k++) {
        if (B.pop[k].num) heap_free(B.pop[k].num);
        B.pop[k].num = 0;
    }
}

static void pop_add(const actor *to, int dmg) {
    cpu_t *c = &cpu;
    int k = 0;
    while (k < MAX_POPUPS && B.pop[k].num) k++;
    if (k == MAX_POPUPS || !to->obj) return;
    uint32_t n = heap_alloc(NUM_SIZE);
    memset(c->m + n, 0, NUM_SIZE);
    CALL(FN_NUM_INIT, n, 0, 0, (uint32_t)dmg);
    B.pop[k].num = n;
    B.pop[k].actor = (int)(to - B.a);
}

static void hit(actor *to, int dmg) {
    if (dmg > to->hp) dmg = to->hp;
    if (!to->party) to->dealt += dmg;
    to->hp -= dmg;
    if (to->hp < 0) to->hp = 0;
    pop_add(to, dmg);
    set_pose(to, to->hp > 0 ? P_HIT : P_DEAD);
    if (to->hp <= 0) to->alive = false;
    if (!B.boss_fled && boss_escapes(to)) B.boss_fled = (int)(to - B.a) + 1;
    // 원본 0x14b0e0과 같은 문턱의 경직. 턴제에서는 다음 자기 차례를 쉬는 기절로 옮긴다.
    if (!to->party && to->alive && to->stun_pct > 0 && !to->stunned && !to->stun_guard) {
        to->stun_acc += dmg;
        if (to->stun_acc >= to->hpmax * to->stun_pct / 100) {
            char msg[64];
            to->stun_acc = 0;
            to->stunned = true;
            snprintf(msg, sizeof msg, "%s " T_STUNNED, to->name);
            fprintf(stderr, "[turn] 기절: %s\n", to->name);
            set_message(msg, 25);
        }
    }
}

static void begin_action(int actor_i, action act, int target) {
    B.cur = actor_i;
    B.act = act;
    B.target = target;
    B.step = 0;
    B.step_tick = 0;
    B.phase = S_ACT;
}

static void next_turn(void);

static void enemy_turn(int i) {
    actor *a = &B.a[i];
    char msg[64];
    a->turns++;
    if (a->stunned) { // 기절: 이번 차례를 쉰다. 자폭 에너지도 터지지 않는다
        a->stunned = false;
        a->stun_guard = true;
        snprintf(msg, sizeof msg, "%s " T_STUN_SKIP, a->name);
        set_message(msg, 25);
        B.phase = S_MESSAGE;
        B.step_tick = 0;
        return;
    }
    a->stun_guard = false;
    if (a->kamikaze) {
        if (a->turns < 2) { // 첫 차례는 예고만 한다
            snprintf(msg, sizeof msg, "%s " T_GLOW, a->name);
            set_message(msg, 25);
        } else { // 아군 모두에게 피해를 주고 스스로 죽는다(원본 0x148e44가 HP 전부를 피해로 넘기는 것과 같게 정산)
            for (int k = 0; k < B.n; k++)
                if (B.a[k].party && B.a[k].alive) hit(&B.a[k], damage_to_party(a, &B.a[k]));
            a->exploded = true;
            a->alive = false;
            a->hp = 0;
            set_pose(a, P_DEAD);
            snprintf(msg, sizeof msg, "%s " T_EXPLODE, a->name);
            set_message(msg, 30);
            fprintf(stderr, "[turn] 자폭: %s\n", a->name);
        }
        B.phase = S_MESSAGE;
        B.step_tick = 0;
        return;
    }
    int targets[MAX_PARTY], n = 0;
    for (int k = 0; k < B.n; k++)
        if (B.a[k].party && B.a[k].alive) targets[n++] = k;
    if (!n) return;
    begin_action(i, A_ATTACK, targets[rnd(n)]);
}

static void next_turn(void) {
    if (B.boss_fled) {
        char msg[64];
        snprintf(msg, sizeof msg, "%s " T_BOSS_FLED, B.a[B.boss_fled - 1].name);
        set_message(msg, 40);
        B.fled = true;
        B.phase = S_END;
        B.step_tick = 0;
        return;
    }
    if (!alive_count(false)) { B.won = true; set_message(T_WIN, 40); B.phase = S_END; B.step_tick = 0; return; }
    if (!alive_count(true)) { B.lost = true; set_message(T_LOSE, 50); B.phase = S_END; B.step_tick = 0; return; }
    for (;;) {
        if (B.order_at >= B.order_n) build_order();
        int i = B.order[B.order_at++];
        actor *a = &B.a[i];
        if (!a->alive) continue;
        // 선제 공격이면 첫 바퀴는 아군만, 기습이면 적만
        if (B.first_strike && !a->party) { if (B.order_at >= B.order_n) B.first_strike = false; continue; }
        if (B.ambushed && a->party) { if (B.order_at >= B.order_n) B.ambushed = false; continue; }
        a->guarding = false;
        if (a->party) { B.cur = i; B.cursor = 0; B.phase = S_COMMAND; }
        else enemy_turn(i);
        if (B.order_at >= B.order_n) B.first_strike = B.ambushed = false;
        return;
    }
}

// 행동 진행: 0 다가가기(8틱) → 1 공격 동작(가운데에서 맞음) → 2 돌아가기(8틱) → 끝
static void act_tick(void) {
    actor *a = &B.a[B.cur], *t = &B.a[B.target];
    enum { WALK = 8 };
    if (B.act == A_GUARD) {
        a->guarding = true;
        set_message(T_GUARDING, 20);
        B.phase = S_MESSAGE;
        B.step_tick = 0;
        return;
    }
    if (B.act == A_ESCAPE) {
        bool ok = rnd(100) < 60; // 정한 값
        for (int i = 0; i < B.n; i++) ok &= !B.a[i].boss;
        if (ok) { B.fled = true; set_message(T_FLED, 30); B.phase = S_END; B.step_tick = 0; }
        else { set_message(T_NOFLEE, 25); B.phase = S_MESSAGE; B.step_tick = 0; }
        return;
    }
    int dir = a->party ? -1 : 1;
    int gx = t->home_x - dir * 28, gy = t->home_y + 2;
    switch (B.step) {
    case 0:
        if (B.step_tick == 0) set_pose(a, P_WALK);
        B.step_tick++;
        a->x = a->home_x + (gx - a->home_x) * B.step_tick / WALK;
        a->y = a->home_y + (gy - a->home_y) * B.step_tick / WALK;
        if (B.step_tick >= WALK) {
            set_pose(a, B.act == A_SKILL ? P_SKILL : P_ATTACK);
            int n = pose_frames(a);
            B.hit_tick = n > 1 ? n / 2 : 0;
            B.step = 1;
            B.step_tick = 0;
        }
        break;
    case 1: {
        int n = pose_frames(a);
        if (B.step_tick == B.hit_tick && t->alive) {
            int dmg = a->party ? damage_to_enemy(a, t, B.act == A_SKILL) : damage_to_party(a, t);
            hit(t, dmg);
        }
        B.step_tick++;
        if (B.step_tick >= (n > 0 ? n : 1) + 2) { set_pose(a, P_WALK); B.step = 2; B.step_tick = 0; }
        break;
    }
    case 2:
        B.step_tick++;
        a->x = gx + (a->home_x - gx) * B.step_tick / WALK;
        a->y = gy + (a->home_y - gy) * B.step_tick / WALK;
        if (B.step_tick >= WALK) {
            a->x = a->home_x;
            a->y = a->home_y;
            set_pose(a, P_IDLE);
            B.phase = S_MESSAGE;
            B.step_tick = 0;
        }
        break;
    }
}

// 적에게 준 피해를 원본 피해 함수로 넘긴다. 다 죽였으면 원래 죽음 → 보상(경험치, 드랍, 퀘스트 처치 수) → 처치 동작으로 이어지고,
// 살아 있으면 남은 HP와 보스 도주 문턱(누적 피해)이 원본 계산 그대로 반영된다.
static void settle_enemy(actor *a) {
    cpu_t *c = &cpu;
    uint32_t m = a->mon;
    if (!m || (!a->dealt && !a->exploded)) return;
    if (!a->alive) {
        // 소환수 보호(+0x295)와 도주 동작은 원본에서 피해를 막는다. 전투 안에서 이미 이겼으므로 푼다.
        W8(m + E_INVULN, 0);
        W8(m + E_INVULN2, 0);
        W8(m + E_RUN_TICKS, 0);
        if (CALL(FN_ESCAPING3, m) || CALL(FN_ESCAPING4, m)) CALL(FN_END_ESCAPE, m, 1);
        int hp = S32(m + E_HP);
        CALL(FN_DAMAGE, m, (uint32_t)-(hp > 0 ? hp : 1), 0);
        if (S32(m + E_HP) > 0) fprintf(stderr, "[turn] 원본이 피해를 막음: 몬스터 %s HP %d\n", a->name, S32(m + E_HP));
    } else {
        CALL(FN_DAMAGE, m, (uint32_t)-a->dealt, 0);
    }
}

static void finish(void) {
    cpu_t *c = &cpu;
    uint32_t hero = CALL(FN_HERO);
    // 소환수부터 정리해야 소환한 몬스터의 보호가 원본에서도 풀린다.
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < B.n; i++) {
            actor *a = &B.a[i];
            if (!a->party && (R8(a->mon + E_SUMMONED) != 0) == (pass == 0)) settle_enemy(a);
        }
    for (int i = 0; i < B.n; i++) {
        actor *a = &B.a[i];
        // 전멸하면 HP 0으로 돌려보낸다. 다음 프레임에 원본 주인공 갱신(0x132510)이 쓰러짐(0x140dbc)으로 넘긴다.
        if (a->hero) {
            W32s(hero + E_HP, a->hp > 0 ? a->hp : 0);
            W32s(hero + H_MP, a->mp);
        }
        free_sprite(a);
    }
    bool killed = false;
    for (int i = 0; i < B.n; i++) killed |= !B.a[i].party && !B.a[i].alive;
    fprintf(stderr, "[turn] 전투 끝: %s\n", B.won ? "승리" : B.boss_fled ? "보스 도주" : B.fled ? "도망" : "전멸");
    int nb = 0;
    for (int i = 0; i < B.n; i++)
        if (!B.a[i].party && B.a[i].kamikaze && !B.a[i].alive && nb < MAX_ENEMIES) blown[nb++] = B.a[i].mon;
    while (nb < MAX_ENEMIES) blown[nb++] = 0;
    blown_until = now_ms() + 1500;
    pop_clear();
    B.phase = S_CLOSED;
    B.n = 0;
    cooldown_until = now_ms() + COOLDOWN_MS;
    if (killed) { loot_until = now_ms() + LOOT_WINDOW_MS; loot_tried_n = 0; loot_n = 0; loot_full = false; }
    check_at = now_ms() + 2000;
    check_mon = B.won ? B.a[0].mon : 0;
    check_exp = S32(hero + 0xf4);
}

static void step(void) {
    for (int i = 0; i < B.n; i++) {
        actor *a = &B.a[i];
        a->tick++;
        if (a->pose == P_HIT && a->tick >= pose_frames(a) + 3) set_pose(a, P_IDLE);
        if (a->pose == P_DEAD && a->kamikaze && a->tick >= pose_frames(a)) a->alpha = 0; // 폭발이 끝나면 바로 지운다(0x14ab54)
        else if (a->pose == P_DEAD && a->tick > pose_frames(a) + 4 && a->alpha > 0) a->alpha -= 25;
    }
    if (B.msg_ticks > 0) B.msg_ticks--;
    switch (B.phase) {
    case S_INTRO:
        if (++B.step_tick >= 25) { build_order(); next_turn(); }
        break;
    case S_ACT: act_tick(); break;
    case S_MESSAGE:
        if (++B.step_tick >= 14) next_turn();
        break;
    case S_END:
        if (++B.step_tick >= 40) finish();
        break;
    default: break;
    }
}

void turn_battle_tick(void) {
    turn_test_tick();
    if (B.phase == S_CLOSED) loot_tick();
    if (B.phase == S_CLOSED && blown_until) {
        cpu_t *c = &cpu;
        for (int i = 0; i < MAX_ENEMIES; i++)
            if (blown[i] && R8(blown[i] + E_CMD) == CMD_EXPLODE) W8(blown[i] + E_SPRITE + SPR_FRAME, 0x7f); // 0x17f4e8: 마지막 ≤ +0x6a면 끝
        if (now_ms() >= blown_until) blown_until = 0;
    }
    if (pending_monster && B.phase == S_CLOSED) {
        uint32_t m = pending_monster;
        pending_monster = 0;
        if (monster_fightable(m)) open_battle(m, pending_kind);
        if (B.phase == S_CLOSED) return;
        draw_scene();
        return;
    }
    if (check_at && now_ms() >= check_at) { // 시험용 기록: 처치가 원래 경로를 탔는지
        cpu_t *c = &cpu;
        uint32_t hero = CALL(FN_HERO);
        if (check_mon) fprintf(stderr, "[turn] 2초 뒤: 경험치 %d → %d, 몬스터 상태 %d HP %d 남은 재출현 %d\n", check_exp, S32(hero + 0xf4), R8(check_mon + E_STATE), S32(check_mon + E_HP), S16(check_mon + E_RESPAWNS));
        check_at = 0;
    }
    if (B.phase == S_CLOSED) return;
    uint64_t now = now_ms();
    if (now - B.last_tick < TICK_MS) return;
    B.last_tick = now - (now - B.last_tick) % TICK_MS;
    step();
    if (B.phase != S_CLOSED) draw_scene();
}

static void choose_command(void) {
    actor *a = &B.a[B.cur];
    switch (B.cursor) {
    case 0: B.act = A_ATTACK; break;
    case 1:
        if (!a->hero) return;
        if (a->mp < SKILL_MP) { set_message(T_NOMP, 20); return; }
        a->mp -= SKILL_MP;
        B.act = A_SKILL;
        break;
    case 2: begin_action(B.cur, A_GUARD, B.cur); return;
    case 3: begin_action(B.cur, A_ESCAPE, B.cur); return;
    }
    B.target = first_alive(false);
    B.phase = S_TARGET;
}

static void move_target(int d) {
    int i = B.target;
    for (int k = 0; k < B.n; k++) {
        i = (i + d + B.n) % B.n;
        if (!B.a[i].party && B.a[i].alive) { B.target = i; return; }
    }
}

bool turn_battle_key(int type, int code) {
    if (B.phase == S_CLOSED) return pending_monster != 0;
    if (type != 2) return true;
    bool ok = code == KEY_OK || code == 53, back = code == KEY_CLEAR || code == 48;
    if (B.phase == S_COMMAND) {
        if (code == KEY_UP || code == 50) B.cursor = (B.cursor + 3) % 4;
        else if (code == KEY_DOWN || code == 56) B.cursor = (B.cursor + 1) % 4;
        else if (ok) choose_command();
    } else if (B.phase == S_TARGET) {
        if (code == KEY_UP || code == KEY_LEFT || code == 50 || code == 52) move_target(-1);
        else if (code == KEY_DOWN || code == KEY_RIGHT || code == 56 || code == 54) move_target(1);
        else if (ok) begin_action(B.cur, B.act, B.target);
        else if (back) {
            if (B.act == A_SKILL) B.a[B.cur].mp += SKILL_MP;
            B.phase = S_COMMAND;
        }
    }
    draw_scene();
    return true;
}

// ---- 게임 훅 ----

// 턴제를 쓰는 곳. 연습 맵(몬스터가 죽지 않음)은 원본 실시간 그대로 둔다.
static bool turn_here(void) { return turn_enabled() && !practice_map(); }

// 원본 그대로 치고 맞는 개체(채집 대상)
static bool original_target(uint32_t m) { cpu_t *c = &cpu; return R8(m + E_GRADE) == GRADE_OBJECT || R8(m + E_GRADE) == 6; }

static bool can_start(uint32_t m) {
    if (!turn_here() || B.phase != S_CLOSED || pending_monster || now_ms() < cooldown_until) return false;
    return monster_fightable(m) && CALL(FN_ATTACKABLE, m);
}

// 몬스터 갱신 1단계. 전투가 잡혔거나 열려 있으면 몬스터를 멈춘다. 몸이 닿으면 전투를 잡는다.
void f_48bfc(cpu_t *c) {
    uint32_t m = c->r[0];
    if (turn_here() && (pending_monster || B.phase != S_CLOSED)) return;
    if (can_start(m) && CALL(FN_CONTACT, m, 0)) {
        pending_monster = m;
        pending_kind = 0;
        return;
    }
    c->r[0] = m;
    orig_f_48bfc(c);
}

// 몬스터 → 주인공 공격. 턴제에서는 피해 대신 기습 전투가 된다.
void f_4de84(cpu_t *c) {
    uint32_t m = c->r[0];
    if (turn_here() && !original_target(m)) {
        if (can_start(m)) { pending_monster = m; pending_kind = 1; }
        c->r[0] = 0;
        return;
    }
    orig_f_4de84(c);
}

// 주인공 → 몬스터 타격. 턴제에서는 피해 대신 선제 공격 전투가 된다.
void f_35a04(cpu_t *c) {
    uint32_t m = c->r[0];
    if (turn_here() && !original_target(m)) {
        if (can_start(m)) { pending_monster = m; pending_kind = 2; }
        c->r[0] = (uint32_t)-1;
        return;
    }
    orig_f_35a04(c);
}

// 시험용 제어 명령(main.c ctrl_poll): "turn nearest"는 가장 가까운 몬스터, "turn boss"는 이 맵의 보스, "turn win"은 열린 전투를 바로 이긴다.
bool turn_ctrl(const char *line) {
    if (strncmp(line, "turn ", 5)) return false;
    if (turn_test_ctrl(line + 5)) return true;
    bool win = !strcmp(line + 5, "win"), lose = !strcmp(line + 5, "lose");
    if ((win || lose) && (B.phase == S_COMMAND || B.phase == S_TARGET)) { // 시험: 적(또는 아군)을 모두 쓰러뜨린다
        for (int i = 0; i < B.n; i++) if (B.a[i].party == lose && B.a[i].alive) hit(&B.a[i], B.a[i].hp);
        next_turn();
        return true;
    }
    if (!strcmp(line + 5, "nearest") && B.phase == S_CLOSED) {
        uint32_t hero = CALL(FN_HERO), best = 0;
        int bd = 1 << 30;
        for (int i = 0; i < monster_count(); i++) {
            uint32_t m = monster_at(i);
            if (!monster_here(m)) continue;
            int d = abs(S32(m + E_X) - S32(hero + E_X)) + abs(S32(m + E_Y) - S32(hero + E_Y));
            if (d < bd) { bd = d; best = m; }
        }
        if (best) { pending_monster = best; pending_kind = 0; }
    } else if (!strcmp(line + 5, "boss") && B.phase == S_CLOSED) { // 이 맵의 보스와 전투
        cpu_t *c = &cpu;
        uint32_t boss = R32(got(GOT_STAGE));
        fprintf(stderr, "[turn] 보스 %08x\n", boss);
        if (boss && monster_here(boss)) { pending_monster = boss; pending_kind = 0; }
    }
    return true;
}
