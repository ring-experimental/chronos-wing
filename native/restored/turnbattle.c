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
#define T_SKILL "\xbd\xba\xc5\xb3" // 스킬
#define T_LOCKED "\xbe\xc6\xc1\xf7\x20\xb9\xe8\xbf\xec\xc1\xf6\x20\xb8\xf8\xc7\xd1\x20\xbd\xba\xc5\xb3\xc0\xcc\xb4\xd9" // 아직 배우지 못한 스킬이다
#define T_ITEM "\xbe\xc6\xc0\xcc\xc5\xdb" // 아이템
#define T_NO_SKILL "\xbe\xb5 \xbc\xf6 \xc0\xd6\xb4\xc2 \xbd\xba\xc5\xb3\xc0\xcc \xbe\xf8\xb4\xd9" // 쓸 수 있는 스킬이 없다
#define T_NO_ITEM "\xc7\xe3\xb8\xae\xc1\xd6\xb8\xd3\xb4\xcf\xbf\xa1 \xbe\xb5 \xbe\xc6\xc0\xcc\xc5\xdb\xc0\xcc \xbe\xf8\xb4\xd9" // 허리주머니에 쓸 아이템이 없다
#define T_NEED_FULL "MP\xb0\xa1 \xb0\xa1\xb5\xe6 \xc2\xf7\xbe\xdf \xbe\xb5 \xbc\xf6 \xc0\xd6\xb4\xd9" // MP가 가득 차야 쓸 수 있다
#define T_NEED_BURN "\xc6\xc4\xbf\xf6\xb9\xf8 \xbb\xf3\xc5\xc2\xbf\xa1\xbc\xad\xb8\xb8 \xbe\xb5 \xbc\xf6 \xc0\xd6\xb4\xd9" // 파워번 상태에서만 쓸 수 있다
#define T_NO_DEAD "\xbe\xb2\xb7\xaf\xc1\xf8 \xb5\xbf\xb7\xe1\xb0\xa1 \xbe\xf8\xb4\xd9" // 쓰러진 동료가 없다
#define T_MISS "\xb0\xf8\xb0\xdd\xc0\xcc \xba\xf8\xb3\xaa\xb0\xac\xb4\xd9" // 공격이 빗나갔다
#define T_DODGE "\xb0\xf8\xb0\xdd\xc0\xbb \xc7\xc7\xc7\xdf\xb4\xd9!" // 공격을 피했다!
#define T_COUNTER "\xb9\xdd\xb0\xdd!" // 반격!
#define T_FART_BLOCK "\xb9\xe6\xb1\xcd \xb6\xa7\xb9\xae\xbf\xa1 \xc7\xe3\xb8\xae\xc1\xd6\xb8\xd3\xb4\xcf\xb0\xa1 \xbf\xad\xb8\xae\xc1\xf6 \xbe\xca\xb4\xc2\xb4\xd9" // 방귀 때문에 허리주머니가 열리지 않는다
#define T_POISON_HURT "\xb5\xb6 \xc7\xc7\xc7\xd8\xb8\xa6 \xc0\xd4\xbe\xfa\xb4\xd9" // 독 피해를 입었다
#define T_USED "\xbb\xe7\xbf\xeb" // 사용
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
#define T_STUN_SKIP "\xbf\xf2\xc1\xf7\xc0\xcc\xc1\xf6\x20\xb8\xf8\xc7\xd1\xb4\xd9" // 움직이지 못한다
#define T_GLOW "\xb8\xf6\xc0\xcc\x20\xba\xfb\xb3\xaa\xb1\xe2\x20\xbd\xc3\xc0\xdb\xc7\xdf\xb4\xd9\x21" // 몸이 빛나기 시작했다!
#define T_EXPLODE "\xc6\xf8\xb9\xdf\xc7\xdf\xb4\xd9\x21" // 폭발했다!
#define T_STUN_TAG "\xb1\xe2\xc0\xfd" // 기절
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
    E_ATTACK_TABLE = 0x1a4, E_ATTACKS = 0x1a8, E_ATTACK = 0x1aa, // 공격표(EMD/C3, 0x14f44c): 항목 +0 물리/마도, +1 상태이상, +4 공격력, +0xf 행동 종류
    FN_ATTACK_ENTRY = 0x14fa20, FN_MON_STATUS = 0x14e474,
    FN_OPTION = 0x101e18, FN_SCROLL = 0x12bb48, FN_HIT_SPARK = 0x12ef28, // (주인공, 동작) 맞은 불꽃 동작 id(주인공 묶음), 없으면 -1
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
    // 턴제 환산(정한 값): 실시간 전투는 적 한 번 공격하는 동안 주인공이 여러 번 치고 피하므로, 적 HP와 적이 주는 피해를 줄인다.
    // 처치·부분 피해 정산과 보스 도주 문턱은 원본 단위로 되돌려 계산한다.
    // 적 HP는 전투마다 파티 화력에 맞춘다(사용자 결정 2026-11-09): 파티 한 바퀴 예상 피해로 처음 마주친 적이 일반이면 약 3바퀴, 보스가 있으면 약 8바퀴.
    // 같은 배율을 모든 적에 써서 원작 HP 비율은 유지한다. 배율은 원작 HP의 3%~500%로 묶는다(초반에는 파티 4인이 원작 한 명보다 세서 원작보다 커질 수 있다).
    ROUNDS_NORMAL = 3, ROUNDS_BOSS = 8, HP_SCALE_MIN = 3, HP_SCALE_MAX = 500,
    // 적 피해도 전투마다 맞춘다(사용자 결정): 처음 마주친 적의 한 번 공격 예상 피해(파티 평균)가 대상 최대 HP의 약 12%(보스 20%)가 되게.
    HIT_PCT_NORMAL = 12, HIT_PCT_BOSS = 20, DMG_SCALE_MIN = 10, DMG_SCALE_MAX = 1000, // 배율 %, 원작 피해의 0.1~10배
    COOLDOWN_MS = 2500,         // 정한 값: 전투가 끝난 뒤 다시 마주치지 않는 시간
    KEY_UP = -1, KEY_DOWN = -2, KEY_LEFT = -3, KEY_RIGHT = -4, KEY_OK = -5, KEY_CLEAR = -16,
};


typedef enum { P_IDLE, P_WALK, P_HIT, P_DEAD, P_ATTACK, P_SKILL } pose;

enum { MAX_SKILLS = 16, MAX_ITEMS = 8, MAX_FX = 12, MENU_ROWS = 4 };

typedef struct {
    bool used, party, hero, alive, guarding;
    char name[28];
    int hp, hpmax, mp, mpmax, atk, pdef, mdef, level, speed, race;
    int ohp, ohpmax;       // 적: 원작 HP(정산은 원본 단위로)
    int member;            // 동료 번호(0 엔지 ~ 7 팡), 아니면 -1
    int cls;               // 주인공 클래스(0 격투, 1 검, 2 정령)
    tskill basic, skills[MAX_SKILLS];
    int nskills;
    uint32_t mon;          // 적이면 게임 몬스터 개체
    bool boss;
    int dealt;             // 적이 이번 전투에서 받은 피해 합(끝날 때 원본 피해 함수로 넘긴다)
    bool kamikaze;         // 등급 5. 자기 차례를 한 번 보낸 뒤 다음 차례에 터진다
    bool exploded;         // 스스로 터져 죽었다(정산은 원본 자폭과 같이 HP 전부를 피해로 넘긴다)
    int turns;             // 이번 전투에서 보낸 자기 차례 수
    int stun_pct, stun_acc; // 원본 경직 문턱과 이번 전투 누적 피해(넘으면 그로기)
    int st[ST_COUNT];      // 상태이상 남은 차례 수(원작 번호 1~8)
    bool st_guard;         // 행동 불가 상태가 풀린 뒤 한 번 행동할 때까지 다시 걸리지 않는다(원본 경직 사이 대기 +0x1d4 대신)
    int buf_atk, buf_atk_t, buf_def, buf_def_t, buf_spd, buf_spd_t; // 버프 %와 남은 차례
    int taunt_t, counter_t, burn_t, mplock_t; // 도발, 반격, 파워번, 각성제(MP 소모 없음)
    bool evade, charge;    // 다음 공격 한 번 피함, 다음 공격 ×1.3
    int attack;            // 적: 이번 공격 번호(원본 +0x1aa, 공격표 +0x1a4)
    uint32_t obj;          // 게임 그림 객체(0x70바이트). 적과 주인공은 필드 개체의 것(+0x38)을 그대로 쓴다
    uint32_t own;          // 동료: 그림을 싣으려고 만든 몬스터 모양 개체(끝날 때 푼다)
    int lift;              // 몬스터 공중 높이(+0x29c, 필드 그리기 0x149064와 같이 뺀다)
    ts_c0 c0;
    bool has_c0;
    int home_x, home_y, x, y;
    pose pose;
    int anim;              // P_SKILL일 때 주인공 동작 id
    int tick;              // 지금 동작의 경과 틱
    int alpha;
} actor;

static void pop_clear(void);

typedef enum { S_CLOSED, S_INTRO, S_NEXT, S_COMMAND, S_SKILLS, S_ITEMS, S_TARGET, S_ACT, S_MESSAGE, S_END } phase;
typedef enum { A_ATTACK, A_SKILL, A_ITEM, A_GUARD, A_ESCAPE, A_COUNT } action;

static struct {
    phase phase;
    actor a[MAX_ACTORS];
    int n;
    uint16_t *snapshot;
    int w, h, bpl;
    int order[MAX_ACTORS], order_n, order_at;
    int cur, cursor, target, menu, menu_top;
    action act;
    tskill sk;             // 지금 쓰는 기술(공격도 기술 하나로 다룬다)
    titem items[MAX_ITEMS];
    int nitems, item;
    int targets[MAX_ACTORS], ntargets;
    int step, step_tick, hit_tick, hit_i;
    char msg[64];
    int msg_ticks;
    bool won, lost, fled, first_strike, ambushed;
    int boss_fled;         // 도주 문턱을 넘은 보스 번호 + 1
    int dmg_pct;           // 이번 전투의 적 피해 배율 %
    uint64_t last_tick;
    struct { uint32_t num; int actor; } pop[MAX_POPUPS]; // 피해 숫자(원본 숫자 개체)
    struct { int anim, x, y, tick; } fx[MAX_FX];         // 주인공 동작 묶음의 효과 동작(0x17ead0)
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
    if (enabled < 0) enabled = getenv("CHRONOS_TURN") != NULL; // 시험: 처음부터 켠다. 보통은 메인메뉴 "턴제 모드"가 켠다(turnmode.c)
    return enabled;
}

void turn_set_enabled(bool on) { enabled = on; }

bool turn_mode_on(void) { return turn_enabled(); }

bool turn_battle_open(void) { return B.phase != S_CLOSED || pending_monster || turn_loot_open() || turn_menu_open(); }

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

static int orig_dealt(const actor *a) { return a->hpmax > 0 ? (int)((int64_t)a->dealt * a->ohpmax / a->hpmax) : a->dealt; } // 원본 단위 피해

static actor *new_actor(void) {
    actor *a = &B.a[B.n++];
    memset(a, 0, sizeof *a);
    a->used = a->alive = true;
    a->member = -1;
    a->alpha = 255;
    return a;
}

static void add_enemy(uint32_t m) {
    cpu_t *c = &cpu;
    actor *a = new_actor();
    a->mon = m;
    memcpy(a->name, gstr(m + E_NAME), 27);
    a->hp = a->ohp = S32(m + E_HP); // 원작 HP. 파티가 정해진 뒤 scale_enemies가 줄인다
    a->hpmax = a->ohpmax = S32(m + E_HPMAX);
    a->level = S8(m + E_LEVEL);
    a->race = R8(m + E_RACE);
    a->boss = R8(m + E_GRADE) == 0;
    a->kamikaze = R8(m + E_GRADE) == GRADE_KAMIKAZE;
    a->stun_pct = a->boss ? 0 : S8(m + E_STUN_PCT); // 보스는 기절하지 않는다(사용자 결정)
    fprintf(stderr, "[turn] 적 %s 등급 %d HP %d 경직 %d%% 공격 %d개\n", a->name, R8(m + E_GRADE), a->hp, a->stun_pct, S16(m + E_ATTACKS));
    a->speed = 8 + a->level / 2; // 정한 값
    a->obj = m + E_SPRITE;
    a->lift = S32(m + E_LIFT);
    load_c0(a, S16(m + E_EMD));
}

static void add_hero(void) {
    cpu_t *c = &cpu;
    uint32_t h = CALL(FN_HERO);
    actor *a = new_actor();
    a->party = a->hero = true;
    strcpy(a->name, T_HERO);
    a->hp = S32(h + E_HP);
    a->hpmax = S32(h + E_HPMAX);
    a->mp = S32(h + H_MP);
    a->mpmax = S32(h + H_MPMAX);
    a->level = S8(h + E_LEVEL);
    a->cls = R8(h + H_CLASS);
    a->nskills = turn_hero_skills(a->cls, &a->basic, a->skills, MAX_SKILLS);
    int m = a->basic.nmotion ? a->basic.motion[0] : 9;
    a->atk = (int)CALL(FN_HERO_ATK, m) * (int)CALL(FN_HERO_ATK_PCT, m) / 100;
    if (a->atk <= 0) a->atk = 10 + a->level * 4; // 무기가 없으면(정한 값)
    a->speed = 12 + a->level / 2; // 정한 값
    fprintf(stderr, "[turn] 주인공 Lv%d 클래스 %d HP %d/%d MP %d/%d 첫 타 %d, 공격 %s %d타 %d%%, 스킬 %d개\n", a->level, a->cls, a->hp, a->hpmax,
            a->mp, a->mpmax, a->atk, a->basic.name, a->basic.hits, a->basic.power, a->nskills);
    for (int i = 0; i < a->nskills; i++)
        fprintf(stderr, "[turn]   스킬 %s Lv%d MP %d %d타 %d%% 대상 %d 효과 %d\n", a->skills[i].name, a->skills[i].level, a->skills[i].mp,
                a->skills[i].hits, a->skills[i].power, a->skills[i].target, a->skills[i].effect);
    a->obj = h + E_SPRITE; // 주인공 그림은 필드 개체의 것(H/S<클래스>, 장비 시트 4장)
}

// 동료 수치는 클라이언트에 없다. 자기 장비와 레벨로 정한다(turnequip.c turn_member_stats, 사용자 결정).
static void add_member(int i) {
    cpu_t *c = &cpu;
    uint32_t e = MEMBERS + (uint32_t)i * MEMBER_SIZE;
    actor *a = new_actor();
    a->party = true;
    a->member = i;
    memcpy(a->name, gstr(e + MEMBER_NAME), 27);
    member_stats st;
    turn_member_stats(i, &st);
    a->level = st.level;
    a->hp = a->hpmax = st.hp;
    a->mp = a->mpmax = st.mp;
    a->atk = st.atk;
    a->pdef = st.pdef;
    a->mdef = st.mdef;
    a->speed = 10 + a->level / 2;
    fprintf(stderr, "[turn] 동료 %d Lv%d HP %d 공격 %d 방어 %d/%d MP %d\n", i, a->level, a->hp, a->atk, a->pdef, a->mdef, a->mp);
    a->nskills = turn_member_skills(i, a->level, &a->basic, a->skills, MAX_SKILLS);
    load_member_sprite(a, S16(e + MEMBER_EMD));
}


// 자리: 적은 왼쪽(오른쪽을 봄), 아군은 오른쪽(왼쪽을 봄). 화면 높이 320 기준 위쪽 3/5이 전장이다.
static int enemy_damage_x(actor *from, actor *to, bool est);

// 파티 한 바퀴 예상 피해(무작위 없이): 주인공은 기본 콤보 동작마다 원본 식(0x101ff4 × 0x102188 - 방어 + 상성), 동료는 공격력 - 방어.
static int party_round_damage(const actor *e) {
    cpu_t *c = &cpu;
    int total = 0;
    for (int i = 0; i < B.n; i++) {
        const actor *a = &B.a[i];
        if (!a->party) continue;
        const tskill *k = &a->basic;
        int def = (int)CALL(FN_MON_DEF, e->mon, (uint32_t)(k->magic != 0));
        if (a->hero) {
            int n = k->nmotion ? k->nmotion : 1;
            for (int h = 0; h < n; h++) {
                int m = k->nmotion ? k->motion[h] : 9;
                int atk = (int)CALL(FN_HERO_ATK, (uint32_t)m) * (int)CALL(FN_HERO_ATK_PCT, (uint32_t)m) / 100;
                if (atk <= 0) atk = a->atk / n;
                int d = atk - def;
                if (d > 0) d += d * (int)CALL(FN_RACE_ATK, CALL(FN_HERO), (uint32_t)e->race) / 100;
                total += d > 1 ? d : 1;
            }
        } else {
            int d = a->atk - def;
            total += d > 1 ? d : 1;
        }
    }
    return total;
}

static void scale_enemies(void) {
    B.dmg_pct = 100;
    const actor *ref = NULL;
    bool boss = false;
    for (int i = 0; i < B.n; i++) {
        if (B.a[i].party) continue;
        if (!ref) ref = &B.a[i];
        boss |= B.a[i].boss;
        if (B.a[i].boss) ref = &B.a[i];
    }
    if (!ref || ref->ohpmax <= 0) return;
    int64_t want = (int64_t)party_round_damage(ref) * (boss ? ROUNDS_BOSS : ROUNDS_NORMAL);
    int64_t pm = want * 1000 / ref->ohpmax; // 천분율
    if (pm < HP_SCALE_MIN * 10) pm = HP_SCALE_MIN * 10;
    if (pm > HP_SCALE_MAX * 10) pm = HP_SCALE_MAX * 10;
    for (int i = 0; i < B.n; i++) {
        actor *a = &B.a[i];
        if (a->party) continue;
        a->hpmax = (int)(a->ohpmax * pm / 1000);
        a->hp = (int)(a->ohp * pm / 1000);
        if (a->hpmax < 1) a->hpmax = 1;
        if (a->hp < 1 && a->ohp > 0) a->hp = 1;
    }
    // 적 피해 배율: 기준 적의 첫 공격(공격표 0번)을 파티원마다 예상해 평균 HP 비율을 목표에 맞춘다
    actor *r = (actor *)ref;
    int saved = r->attack, sum_d = 0, sum_hp = 0;
    r->attack = 0;
    for (int i = 0; i < B.n; i++)
        if (B.a[i].party) { sum_d += enemy_damage_x(r, &B.a[i], true); sum_hp += B.a[i].hpmax; }
    r->attack = saved;
    int hit = boss ? HIT_PCT_BOSS : HIT_PCT_NORMAL;
    int64_t dp = sum_d > 0 ? (int64_t)sum_hp * hit / sum_d : 100;
    if (dp < DMG_SCALE_MIN) dp = DMG_SCALE_MIN;
    if (dp > DMG_SCALE_MAX) dp = DMG_SCALE_MAX;
    B.dmg_pct = (int)dp;
    fprintf(stderr, "[turn] 적 피해 배율 %d%%(예상 한 번 %d, 아군 HP 합 %d)\n", B.dmg_pct, sum_d, sum_hp);
    fprintf(stderr, "[turn] 적 HP 배율 %d.%d%%(파티 한 바퀴 %d, 기준 %s HP %d, 목표 %d바퀴)\n", (int)(pm / 10), (int)(pm % 10),
            party_round_damage(ref), boss ? "보스" : "일반", ref->ohpmax, boss ? ROUNDS_BOSS : ROUNDS_NORMAL);
}

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

#define draw_text turn_draw_text
#define fill turn_fill

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
    int mem[MAX_PARTY], nm = turn_party_members(mem); // 파티 편성(turnparty.c): 주인공 + 동료 최대 3명
    for (int i = 0; i < nm && i < MAX_PARTY - 1; i++) add_member(mem[i]);
    scale_enemies();
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
    default: { // 공격 번호 = 상태 - 7(보스 - 9), 0x148980. 그 상태 동작이 없으면 첫 공격 동작
        int st = (a->boss ? 9 : 7) + a->attack;
        if (a->has_c0 && (st >= a->c0.count || a->c0.anim[st][3] < 0)) st = a->boss ? 9 : 7;
        return st;
    }
    }
}

// 주인공 동작(0x1336d8): 2 전투 대기, 3 전투 이동, 5 피격, 8 사망, 9~ 공격과 스킬(H/DS)
static int hero_anim(const actor *a, pose p) {
    switch (p) {
    case P_IDLE: return 2;
    case P_WALK: return 3;
    case P_HIT: return 5;
    case P_DEAD: return 8;
    case P_SKILL: return a->anim; // H/DS 줄의 동작 id
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
        *anim = hero_anim(a, a->pose);
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

void turn_draw_text(int x, int y, int width, const char *text, uint32_t color, int center) {
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

void turn_fill(int x, int y, int w, int h, int r, int g, int b) {
    cpu_t *c = &cpu;
    CALL(FN_FILL_RECT, R32(got(GOT_SCREEN)), x, y, w, h, r, g, b);
}

static void bar(int x, int y, int w, int v, int max, int r, int g, int b) {
    fill(x, y, w, 3, 0x20, 0x20, 0x20);
    if (max > 0 && v > 0) fill(x, y, w * (v > max ? max : v) / max, 3, r, g, b);
}

static bool actor_tags(const actor *a, char *out, int cap, bool brief);
static bool skill_usable(const actor *a, const tskill *k, const char **why);

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
    } else if (B.phase == S_COMMAND || B.phase == S_SKILLS || B.phase == S_ITEMS) {
        actor *a = &B.a[B.cur];
        fill(a->x - 3, a->y + 4, 7, 3, 0x60, 0xd0, 0xff);
    }
    // 기술 효과(주인공 동작 묶음의 동작, 원본 0x17ead0)
    uint32_t hero_obj = 0;
    for (int i = 0; i < B.n; i++) if (B.a[i].hero) hero_obj = B.a[i].obj;
    for (int k = 0; k < MAX_FX; k++) {
        if (!B.fx[k].anim || !hero_obj) continue;
        actor fake = {0};
        fake.obj = hero_obj;
        int dir = 3, n = obj_frames(&fake, B.fx[k].anim, &dir);
        if (B.fx[k].tick >= n) { B.fx[k].anim = 0; continue; }
        fake.x = B.fx[k].x; fake.y = B.fx[k].y; fake.alpha = 255;
        draw_actor(&fake, B.fx[k].anim, dir, B.fx[k].tick);
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
    // 적 HP(막대)와 상태
    for (int i = 0; i < B.n; i++) {
        actor *a = &B.a[i];
        if (a->party || !a->alive) continue;
        bar(a->home_x - 20, a->home_y + 6, 40, a->hp, a->hpmax, 0xe0, 0x40, 0x40);
        char tags[48];
        if (actor_tags(a, tags, sizeof tags, false)) draw_text(a->home_x, a->home_y + 10, 90, tags, 0xffe060, 1);
    }
    // 아래 창: 왼쪽 명령, 오른쪽 아군 상태. 스킬·아이템 목록은 창 전체를 쓴다.
    CALL(FN_PANEL, 0, panel_y, B.w, B.h - panel_y);
    fill(2, panel_y + 2, B.w - 4, B.h - panel_y - 4, 0x10, 0x18, 0x30);
    int cmd_w = 70;
    if (B.phase == S_SKILLS || B.phase == S_ITEMS) {
        actor *a = &B.a[B.cur];
        bool skills = B.phase == S_SKILLS;
        int n = skills ? a->nskills : B.nitems;
        char s[80];
        snprintf(s, sizeof s, "%s  MP %d/%d", a->name, a->mp, a->mpmax);
        draw_text(10, panel_y + 6, B.w - 20, s, 0xe8d6ab, 0);
        if (!n) draw_text(B.w / 2, panel_y + 30, B.w, skills ? T_NO_SKILL : T_NO_ITEM, 0xc0c0c0, 1);
        for (int r = 0; r < MENU_ROWS && B.menu_top + r < n; r++) {
            int i = B.menu_top + r, y = panel_y + 24 + r * 17;
            bool ok;
            if (skills) {
                const tskill *k = &a->skills[i];
                ok = skill_usable(a, k, NULL);
                snprintf(s, sizeof s, "%s", k->name);
            } else {
                ok = true;
                snprintf(s, sizeof s, "%s", B.items[i].name);
            }
            uint32_t col = i == B.menu ? 0xffe060 : ok ? 0xffffff : 0x707070;
            if (i == B.menu) fill(6, y - 2, B.w - 12, 17, 0x30, 0x5f, 0xae);
            draw_text(12, y, B.w - 90, s, col, 0);
            if (skills && a->skills[i].unlock > a->level) snprintf(s, sizeof s, "Lv%d", a->skills[i].unlock);
            else if (skills) snprintf(s, sizeof s, "MP %d", a->mplock_t > 0 ? 0 : a->skills[i].mp);
            else snprintf(s, sizeof s, "x%d", B.items[i].count);
            draw_text(B.w - 70, y, 60, s, col, 0);
        }
        if (B.menu_top > 0) draw_text(B.w - 14, panel_y + 6, 10, "^", 0xffffff, 0);
        if (B.menu_top + MENU_ROWS < n) draw_text(B.w - 14, B.h - 16, 10, "v", 0xffffff, 0);
        video_present(sh);
        return;
    }
    if (B.phase == S_COMMAND) {
        static const char *cmds[A_COUNT] = {T_ATTACK, T_SKILL, T_ITEM, T_GUARD, T_ESCAPE};
        const actor *a = &B.a[B.cur];
        for (int i = 0; i < A_COUNT; i++) {
            bool usable = (i != A_SKILL || a->nskills) && (i != A_ITEM || !a->st[ST_FART]);
            uint32_t col = i == B.cursor ? 0xffe060 : usable ? 0xffffff : 0x707070;
            if (i == B.cursor) fill(8, panel_y + 5 + i * 17, cmd_w - 12, 16, 0x30, 0x5f, 0xae);
            draw_text(14, panel_y + 6 + i * 17, cmd_w, cmds[i], col, 0);
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
        bar(bx, y + 17, bw, a->mp, a->mpmax, 0x40, 0x80, 0xff);
        char tags[48];
        if (a->alive && actor_tags(a, tags, sizeof tags, true)) draw_text(bx + bw - 44, y - 1, 46, tags, 0xffa040, 0);
        row++;
    }
    video_present(sh);
}

// ---- 전투 규칙(정한 값이 많다. work/turn-plan.md "시제품", "스킬·역할·상태이상·드랍 없애기") ----

// 상태이상 지속(자기 차례 수). 원작 틱(중독 160, 감전 10, 화상 100, 그로기 20, 눈사람 130, 실명 150, 방귀 100, 이속저주 150)을 차례로 줄인 정한 값.
static const int ST_TURNS[ST_COUNT] = {0, 5, 1, 3, 1, 2, 3, 3, 3};
// 원작 저항(0x14d908): 옵션(0x101e18, DAT/PRO 번호) + 주문서(0x12bb48) + 8. -1은 그 저항이 없음
static const int ST_OPTION[ST_COUNT] = {0, 16, 17, 18, -1, -1, 19, 20, -1};
static const int ST_SCROLL[ST_COUNT] = {0, 3, 4, 5, 6, 7, 8, 9, 11};

static bool st_skip(int s) { return s == ST_SHOCK || s == ST_GROGGY || s == ST_SNOW; } // 행동 불가(원작: 감전·그로기 입력 막힘, 눈사람 공격 불가)

static bool any_skip(const actor *a) {
    for (int s = 1; s < ST_COUNT; s++) if (st_skip(s) && a->st[s] > 0) return true;
    return false;
}

static int alive_count(bool party) {
    int n = 0;
    for (int i = 0; i < B.n; i++) n += B.a[i].party == party && B.a[i].alive;
    return n;
}

static int eff_speed(const actor *a) {
    int s = a->speed * (100 + (a->buf_spd_t > 0 ? a->buf_spd : 0)) / 100;
    if (a->st[ST_SLOW] > 0) s /= 2; // 원작 이속저주: 이동속도 절반
    return s;
}

static void build_order(void) {
    B.order_n = 0;
    for (int i = 0; i < B.n; i++)
        if (B.a[i].alive) B.order[B.order_n++] = i;
    // 빠르기 순, 같으면 아군 먼저
    for (int i = 1; i < B.order_n; i++)
        for (int k = i; k > 0; k--) {
            actor *p = &B.a[B.order[k - 1]], *q = &B.a[B.order[k]];
            int sp = eff_speed(p) + rnd(4), sq = eff_speed(q) + rnd(4);
            if (sq > sp) { int s = B.order[k]; B.order[k] = B.order[k - 1]; B.order[k - 1] = s; }
        }
    B.order_at = 0;
}

static bool actor_tags(const actor *a, char *out, int cap, bool brief) {
    static const char *SHORT[ST_COUNT] = {"", "\xb5\xb6", "\xc0\xfc", "\xc8\xad", "\xb1\xe2", "\xb4\xab", "\xbd\xc7", "\xb9\xe6", "\xb5\xd0"}; // 독 전 화 기 눈 실 방 둔
    int len = 0, n = 0, max = brief ? 3 : 2;
    out[0] = 0;
    #define TAG(t) do { if (n < max) { len += snprintf(out + len, cap - len, "%s%s", n ? " " : "", (t)); n++; } } while (0)
    for (int s = 1; s < ST_COUNT; s++) if (a->st[s] > 0) TAG(brief ? SHORT[s] : turn_status_name(s));
    if (a->buf_atk_t > 0) TAG(brief ? "\xb0\xf8" : "\xb0\xf8\xb0\xdd\xa1\xe8"); // 공 공격↑
    if (a->buf_def_t > 0) TAG(brief ? "\xb9\xe6" : "\xb9\xe6\xbe\xee\xa1\xe8"); // 방 방어↑
    if (a->buf_spd_t > 0) TAG(brief ? "\xbc\xd3" : "\xbc\xd3\xb5\xb5\xa1\xe8"); // 속 속도↑
    if (a->burn_t > 0) TAG(brief ? "\xc6\xc4" : "\xc6\xc4\xbf\xf6\xb9\xf8"); // 파 파워번
    if (a->taunt_t > 0) TAG(brief ? "\xb5\xb5" : "\xb5\xb5\xb9\xdf"); // 도 도발
    if (a->counter_t > 0) TAG(brief ? "\xb9\xdd" : "\xb9\xdd\xb0\xdd"); // 반 반격
    #undef TAG
    return n > 0;
}

static int skill_cost(const actor *a, const tskill *k) { return a->mplock_t > 0 ? 0 : k->mp; }

static bool any_dead_ally(void) {
    for (int i = 0; i < B.n; i++) if (B.a[i].party && !B.a[i].alive) return true;
    return false;
}

static bool skill_usable(const actor *a, const tskill *k, const char **why) {
    const char *r = NULL;
    if (k->unlock > a->level) r = T_LOCKED;
    else if (a->mp < skill_cost(a, k)) r = T_NOMP;
    else if (k->need == NEED_MP_FULL && a->mp < a->mpmax) r = T_NEED_FULL;
    else if (k->need == NEED_POWERBURN && a->burn_t <= 0) r = T_NEED_BURN;
    else if (k->target == TG_DEAD && !any_dead_ally()) r = T_NO_DEAD;
    if (why) *why = r;
    return !r;
}

// 원본 0x14b1a8, 0x14b208과 같은 문턱. 넘으면 전투를 끝내고 필드에서 원본이 도주시킨다.
static bool boss_escapes(const actor *a) {
    cpu_t *c = &cpu;
    uint32_t m = a->mon;
    if (!a->boss || !a->alive || S8(m + E_RUN_TICKS) > 0) return false;
    int pct = S8(m + E_ESC_CUM_PCT);
    if (pct > 0 && abs(S32(m + E_ESC_CUM) - orig_dealt(a)) >= S32(m + E_HPMAX) * pct / 100) return true;
    pct = S8(m + E_ESC_HP_PCT);
    return pct > 0 && !R8(m + E_ESC_HP_DONE) && a->hp <= a->hpmax * pct / 100;
}

// 피해 숫자: 필드의 0x10264c와 같이 숫자 개체를 0x118420(개체, 종류, 0, 값)으로 채운다. 종류 0은 몬스터가 받은 피해(주황 숫자), 2는 회색 +숫자(회복에 쓴다).
// 원작은 주인공이 맞으면 숫자 없이 HUD 막대만 줄이므로(0x103ebc → 0x144100) 아군 피해도 같은 종류 0으로 보인다(정한 값). 필드는 개체 목록(+0x20)에 붙여 0x103ab8이 그리지만 전투는 멈춰 있으므로 직접 그린다:
// 0x118528(개체, x, y)로 그리고 0x1184bc가 참이면 끝. 위치는 그림의 맞는 점(그림 +0, +4)이다.
static void pop_clear(void) {
    for (int k = 0; k < MAX_POPUPS; k++) {
        if (B.pop[k].num) heap_free(B.pop[k].num);
        B.pop[k].num = 0;
    }
    memset(B.fx, 0, sizeof B.fx);
}

static void pop_add(const actor *to, int value, int kind) {
    cpu_t *c = &cpu;
    int k = 0;
    while (k < MAX_POPUPS && B.pop[k].num) k++;
    if (k == MAX_POPUPS || !to->obj) return;
    uint32_t n = heap_alloc(NUM_SIZE);
    memset(c->m + n, 0, NUM_SIZE);
    CALL(FN_NUM_INIT, n, (uint32_t)kind, 0, (uint32_t)value);
    B.pop[k].num = n;
    B.pop[k].actor = (int)(to - B.a);
}

static void fx_add(int anim, const actor *at) {
    if (anim <= 0) return;
    for (int k = 0; k < MAX_FX; k++)
        if (!B.fx[k].anim) { B.fx[k].anim = anim; B.fx[k].x = at->x; B.fx[k].y = at->y - at->lift; B.fx[k].tick = 0; return; }
}

static void hit(actor *to, int dmg) {
    if (dmg > to->hp) dmg = to->hp;
    if (!to->party) to->dealt += dmg;
    to->hp -= dmg;
    if (to->hp < 0) to->hp = 0;
    pop_add(to, dmg, 0);
    set_pose(to, to->hp > 0 ? P_HIT : P_DEAD);
    if (to->hp <= 0) {
        to->alive = false;
        memset(to->st, 0, sizeof to->st);
        to->buf_atk_t = to->buf_def_t = to->buf_spd_t = to->taunt_t = to->counter_t = to->burn_t = to->mplock_t = 0;
        to->evade = to->charge = false;
    }
    if (!B.boss_fled && boss_escapes(to)) B.boss_fled = (int)(to - B.a) + 1;
    // 원본 0x14b0e0과 같은 문턱의 경직. 턴제에서는 다음 자기 차례를 쉬는 그로기로 옮긴다.
    if (!to->party && to->alive && to->stun_pct > 0 && !to->st[ST_GROGGY] && !to->st_guard) {
        to->stun_acc += dmg;
        if (to->stun_acc >= to->hpmax * to->stun_pct / 100) {
            char msg[64];
            to->stun_acc = 0;
            to->st[ST_GROGGY] = 1;
            snprintf(msg, sizeof msg, "%s " T_STUNNED, to->name);
            fprintf(stderr, "[turn] 기절: %s\n", to->name);
            set_message(msg, 25);
        }
    }
}

static void heal(actor *to, int hp, int mp) {
    if (!to->alive) return;
    if (hp > to->hpmax - to->hp) hp = to->hpmax - to->hp;
    if (mp > to->mpmax - to->mp) mp = to->mpmax - to->mp;
    if (hp < 0) hp = 0;
    if (mp < 0) mp = 0;
    to->hp += hp;
    to->mp += mp;
    pop_add(to, hp ? hp : mp, 2);
}

// 상태이상 걸기. 몬스터 → 주인공은 원작 확률(100 - 저항), 몬스터 → 동료는 원작 기본 저항 8만 뺀다(동료 장비가 없으므로).
// 보스와 막 풀린 대상에게는 행동 불가 상태가 걸리지 않는다.
static bool try_status(const actor *from, actor *to, int s, int chance) {
    cpu_t *c = &cpu;
    if (s <= 0 || s >= ST_COUNT || !to->alive || to->st[s] > 0) return false;
    if (st_skip(s) && (to->boss || to->st_guard)) return false;
    if (from && !from->party && to->party) {
        int r = 8;
        if (to->hero) {
            if (ST_OPTION[s] >= 0) r += (int)CALL(FN_OPTION, (uint32_t)ST_OPTION[s]);
            r += (int)(int8_t)CALL(FN_SCROLL, (uint32_t)ST_SCROLL[s]);
        }
        chance = 100 - r;
    }
    if (rnd(100) >= chance) return false;
    to->st[s] = ST_TURNS[s];
    char msg[64];
    snprintf(msg, sizeof msg, "%s %s!", to->name, turn_status_name(s));
    set_message(msg, 25);
    fprintf(stderr, "[turn] 상태이상: %s %s(확률 %d)\n", to->name, turn_status_name(s), chance);
    if (s == ST_SHOCK) hit(to, to->hpmax / 20 > 0 ? to->hpmax / 20 : 1); // 원작: 걸릴 때 최대 HP/20 피해
    return true;
}

static int vary(int v) { return v * (90 + rnd(21)) / 100; }

// 주인공 → 적: 원본 0x101ff4(동작) × 0x102188(동작)% 뒤 ±10%(0x12f1e8), 몬스터 방어 0x14ebf0(m, 마법), 상성 0x102240 = (피해-방어)×0x12e514%(0x135a04)
static int hero_damage(actor *from, actor *to, const tskill *k, int i) {
    cpu_t *c = &cpu;
    int m = k->nmotion ? k->motion[i % k->nmotion] : 9;
    int atk = (int)CALL(FN_HERO_ATK, (uint32_t)m) * (int)CALL(FN_HERO_ATK_PCT, (uint32_t)m) / 100;
    if (atk <= 0) atk = from->atk * (k->power ? k->power : 100) / (100 * (k->hits ? k->hits : 1));
    atk = vary(atk);
    int def = (int)CALL(FN_MON_DEF, to->mon, (uint32_t)(k->magic != 0));
    int dmg = atk - def;
    if (dmg < 1) dmg = 1;
    if (atk - def > 0) dmg += (atk - def) * (int)CALL(FN_RACE_ATK, CALL(FN_HERO), (uint32_t)to->race) / 100;
    return dmg;
}

// 동료 → 적: 공격력 × 위력% ÷ 타수 - 몬스터 방어(관통이면 0). 정한 값
static int member_damage(actor *from, actor *to, const tskill *k) {
    cpu_t *c = &cpu;
    int atk = vary(from->atk * k->power / (100 * (k->hits ? k->hits : 1)));
    int def = k->pierce ? 0 : (int)CALL(FN_MON_DEF, to->mon, (uint32_t)(k->magic != 0));
    int dmg = atk - def;
    return dmg < 1 ? 1 : dmg;
}

// 적 → 주인공: 원본 0x14de84(공격력, 치명타, 방어 0x101ef4 + 감소 0x101f6c, 상성 0x12e6ec). 적 → 동료: 공격력 - 방어(정한 값).
// 공격력·종류는 이번 공격 번호(+0x1aa)의 공격표 항목에서 원본 함수로 읽는다. est면 치명타·무작위 없이 배율 전 값(전투 배율 계산용).
static int enemy_damage_x(actor *from, actor *to, bool est);
static int enemy_damage(actor *from, actor *to) { return enemy_damage_x(from, to, false); }
static int enemy_damage_x(actor *from, actor *to, bool est) {
    cpu_t *c = &cpu;
    uint32_t m = from->mon;
    int saved = R8(m + E_ATTACK);
    W8(m + E_ATTACK, (uint8_t)from->attack);
    int kind = (int)CALL(FN_MON_ATK_KIND, m);
    int atk = (int)CALL(FN_MON_ATK, m, 0);
    W8(m + E_ATTACK, (uint8_t)saved);
    bool crit = !est && rnd(100) <= (int)(int8_t)CALL(FN_MON_CRIT, m);
    if (crit) atk = atk * (S8(m + 0x178) * 7 + 100) / 100;
    if (!est) atk = vary(atk);
    int dmg;
    if (to->hero) {
        int def = (int)CALL(FN_HERO_DEF, kind), cut = (int)CALL(FN_HERO_DEF_CUT, def, kind, crit);
        dmg = atk - (int16_t)(def + cut);
        dmg += dmg * (int)CALL(FN_RACE_DEF, CALL(FN_HERO), (uint32_t)from->race) / 100;
    } else {
        dmg = atk - (kind ? to->mdef : to->pdef);
    }
    if (!est) dmg = (int)((int64_t)dmg * B.dmg_pct / 100);
    return dmg < 1 ? 1 : dmg;
}

// 버프와 상태에 따른 배율
static int scale_damage(const actor *from, const actor *to, int dmg) {
    int pct = 100;
    if (from->buf_atk_t > 0) pct += from->buf_atk;
    if (from->burn_t > 0) pct += 30; // 파워번(정한 값)
    dmg = dmg * pct / 100;
    if (from->charge) dmg = dmg * 13 / 10; // 원본 0x101ff4의 검 차지 배율
    if (to->st[ST_BURN] > 0) dmg *= 2;    // 원작 화상: 받는 피해 2배(tdf/TIP)
    if (to->buf_def_t > 0) dmg = dmg * (100 - to->buf_def) / 100;
    if (to->guarding) dmg /= 2;
    return dmg < 1 ? 1 : dmg;
}

// 한 타. 회피·실명·눈사람·반격과 부가 상태이상까지 처리한다.
static void strike(actor *from, actor *to, const tskill *k, int i) {
    char msg[64];
    int hp0 = to->hp;
    if (!to->alive) return;
    if (from->st[ST_BLIND] > 0 && rnd(100) < 50) { // 실명: 원작은 화면이 가려진다. 턴제에서는 절반 빗나감(정한 값)
        snprintf(msg, sizeof msg, "%s " T_MISS, from->name);
        set_message(msg, 20);
        return;
    }
    if (to->evade) {
        to->evade = false;
        snprintf(msg, sizeof msg, "%s " T_DODGE, to->name);
        set_message(msg, 20);
        return;
    }
    int dmg = from->party ? (from->hero ? hero_damage(from, to, k, i) : member_damage(from, to, k)) : enemy_damage(from, to);
    dmg = scale_damage(from, to, dmg);
    if (to->party && to->st[ST_SNOW] > 0) dmg = 0; // 원작 눈사람: 몬스터 피해 0(0x14de84)
    if (!to->party && to->st[ST_SNOW] > 0 && k->magic) to->st[ST_SNOW] = 0; // 원작 프로스트월: 얼음은 다음 마법 한 방에 깨진다
    if (dmg > 0) hit(to, dmg);
    if (from->hero && !k->magic && k->hits) from->mp += k->mp_gain / k->hits; // 원작: 적중하면 MP를 얻는다(H/DS +6)
    if (from->mp > from->mpmax) from->mp = from->mpmax;
    fprintf(stderr, "[turn] 타격 %d → %d: %d(HP %d → %d) 상태 %d %d%%\n", (int)(from - B.a), (int)(to - B.a), dmg, hp0, to->hp, k->status, k->chance);
    if (k->status) try_status(from, to, k->status, k->chance);
    // 반격: 적의 직접 공격을 받고 살아 있으면 기본 공격의 70%로 되친다(정한 값)
    if (to->party && to->alive && to->counter_t > 0 && !from->party && from->alive && !k->ranged) {
        int back = to->hero ? hero_damage(to, from, &to->basic, 0) : member_damage(to, from, &to->basic);
        hit(from, back * 7 / 10 > 0 ? back * 7 / 10 : 1);
        snprintf(msg, sizeof msg, "%s " T_COUNTER, to->name);
        set_message(msg, 20);
    }
}

static void apply_effect(actor *from, actor *to, const tskill *k) {
    switch (k->effect) {
    case EF_STATUS: try_status(from, to, k->status, k->chance); break;
    case EF_HEAL: heal(to, to->hpmax * k->amount / 100 + from->atk * k->power / 100, 0); break;
    case EF_REVIVE:
        if (to->alive) break;
        to->alive = true;
        to->hp = to->hpmax * k->amount / 100 > 0 ? to->hpmax * k->amount / 100 : 1;
        to->alpha = 255;
        set_pose(to, P_IDLE);
        pop_add(to, to->hp, 2);
        break;
    case EF_CURE:
        for (int s = 1; s < ST_COUNT; s++) if (!k->status || s == k->status) to->st[s] = 0;
        break;
    case EF_MP: heal(to, 0, to->mpmax * k->amount / 100); break;
    case EF_BUFF_ATK: to->buf_atk = k->amount; to->buf_atk_t = k->turns; break;
    case EF_BUFF_DEF: to->buf_def = k->amount; to->buf_def_t = k->turns; break;
    case EF_BUFF_SPD: to->buf_spd = k->amount; to->buf_spd_t = k->turns; break;
    case EF_TAUNT: to->taunt_t = k->turns; break;
    case EF_COUNTER: to->counter_t = k->turns; break;
    case EF_EVADE: to->evade = true; break;
    case EF_CHARGE: to->charge = true; break;
    case EF_POWERBURN: to->burn_t = k->turns; break;
    }
}

// 허리주머니 아이템 사용(원작 효과 0x13a688의 기본값). 하나 쓰고 줄인다.
static void use_item(actor *to, const titem *it) {
    int effect, hp, mp, status, turns;
    if (!turn_item_effect(it->kind, &effect, &hp, &mp, &status, &turns)) return;
    switch (effect) {
    case EF_HEAL: case EF_MP: heal(to, hp, mp); break;
    case EF_CURE: to->st[status] = 0; break;
    case EF_MPLOCK: to->mplock_t = turns; break;
    case EF_BUFF_ATK: to->buf_atk = hp; to->buf_atk_t = turns; break;
    case EF_BUFF_DEF: to->buf_def = hp; to->buf_def_t = turns; break;
    }
    turn_pouch_consume(it);
    fprintf(stderr, "[turn] 아이템 %s → %s\n", it->name, to->name);
}

static void begin_action(int actor_i, action act, int target) {
    B.cur = actor_i;
    B.act = act;
    B.target = target;
    B.step = 0;
    B.step_tick = 0;
    B.hit_i = 0;
    B.phase = S_ACT;
}

// 기술 대상. 적 쪽/아군 쪽은 쓰는 쪽 기준이다.
static void resolve_targets(void) {
    actor *a = &B.a[B.cur];
    B.ntargets = 0;
    int t = B.sk.target;
    for (int i = 0; i < B.n; i++) {
        actor *o = &B.a[i];
        bool foe = o->party != a->party;
        bool ok = t == TG_ENEMIES ? foe && o->alive : t == TG_ALLIES ? !foe && o->alive : false;
        if (ok) B.targets[B.ntargets++] = i;
    }
    if (t == TG_ENEMY || t == TG_ALLY || t == TG_DEAD) B.targets[B.ntargets++] = B.target;
    if (t == TG_SELF) B.targets[B.ntargets++] = B.cur;
}

static void next_turn(void);

// 이번 공격: 공격표(+0x1a4, 개수 +0x1a8)에서 공격력이나 상태이상이 있는 것 중 하나(정한 값: 고르게). 행동 종류 24(실명, 공격력 0)는 피해 없이 상태만 건다.
static void pick_attack(actor *a) {
    cpu_t *c = &cpu;
    uint32_t m = a->mon;
    int n = S16(m + E_ATTACKS), cand[16], nc = 0;
    for (int i = 0; i < n && i < 16; i++) {
        uint32_t e = CALL(FN_ATTACK_ENTRY, R32(m + E_ATTACK_TABLE), (uint32_t)i);
        if (e && (S32(e + 4) > 0 || S8(e + 1) > 0)) cand[nc++] = i;
    }
    a->attack = nc ? cand[rnd(nc)] : 0;
    memset(&B.sk, 0, sizeof B.sk);
    B.sk.target = TG_ENEMY;
    B.sk.effect = EF_HIT;
    B.sk.hits = 1;
    B.sk.fx = -1;
    B.sk.chance = 100;
    if (n > 0) {
        uint32_t e = CALL(FN_ATTACK_ENTRY, R32(m + E_ATTACK_TABLE), (uint32_t)a->attack);
        B.sk.status = S8(e + 1);
        B.sk.magic = S8(e + 0);
        if (S32(e + 4) <= 0 && B.sk.status) { B.sk.effect = EF_STATUS; B.sk.ranged = true; }
    }
}

static void enemy_turn(int i) {
    actor *a = &B.a[i];
    char msg[64];
    a->turns++;
    if (a->kamikaze) {
        if (a->turns < 2) { // 첫 차례는 예고만 한다
            snprintf(msg, sizeof msg, "%s " T_GLOW, a->name);
            set_message(msg, 25);
        } else { // 아군 모두에게 피해를 주고 스스로 죽는다(원본 0x148e44가 HP 전부를 피해로 넘기는 것과 같게 정산)
            a->attack = 0;
            for (int k = 0; k < B.n; k++)
                if (B.a[k].party && B.a[k].alive) hit(&B.a[k], scale_damage(a, &B.a[k], enemy_damage(a, &B.a[k])));
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
    int targets[MAX_PARTY], n = 0, taunt[MAX_PARTY], nt = 0;
    for (int k = 0; k < B.n; k++)
        if (B.a[k].party && B.a[k].alive) {
            targets[n++] = k;
            if (B.a[k].taunt_t > 0) taunt[nt++] = k;
        }
    if (!n) return;
    pick_attack(a);
    int t = nt && rnd(100) < 75 ? taunt[rnd(nt)] : targets[rnd(n)]; // 도발: 75%는 도발한 쪽을 노린다(정한 값)
    begin_action(i, A_ATTACK, t);
}

// 자기 차례 시작: 중독 피해, 행동 불가 확인, 상태·버프 차례 줄이기. 행동할 수 있으면 참.
static bool start_turn(actor *a) {
    char msg[64];
    if (a->st[ST_POISON] > 0) { // 원작 중독: 최대 HP/25씩 5번
        hit(a, a->hpmax / 25 > 0 ? a->hpmax / 25 : 1);
        snprintf(msg, sizeof msg, "%s " T_POISON_HURT, a->name);
        set_message(msg, 20);
    }
    bool skip = a->alive && any_skip(a);
    for (int s = 1; s < ST_COUNT; s++) if (a->st[s] > 0) a->st[s]--;
    int *t[] = {&a->buf_atk_t, &a->buf_def_t, &a->buf_spd_t, &a->taunt_t, &a->counter_t, &a->burn_t, &a->mplock_t};
    for (size_t k = 0; k < sizeof t / sizeof t[0]; k++) if (*t[k] > 0) (*t[k])--;
    if (!a->alive) { B.phase = S_MESSAGE; B.step_tick = 0; return false; }
    if (skip) {
        a->st_guard = true;
        snprintf(msg, sizeof msg, "%s " T_STUN_SKIP, a->name);
        set_message(msg, 25);
        B.phase = S_MESSAGE;
        B.step_tick = 0;
        return false;
    }
    a->st_guard = false;
    if (a->party && !a->hero) { a->mp += 3; if (a->mp > a->mpmax) a->mp = a->mpmax; } // 동료 MP 회복(정한 값)
    if (B.phase == S_MESSAGE) B.step_tick = 0;
    return true;
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
        if (B.order_at >= B.order_n) B.first_strike = B.ambushed = false;
        if (!start_turn(a)) return;
        if (a->party) { B.cur = i; B.cursor = 0; B.phase = S_COMMAND; fprintf(stderr, "[turn] 차례: 아군 %d HP %d MP %d\n", i, a->hp, a->mp); }
        else enemy_turn(i);
        return;
    }
}

// 다가가서 치는 기술인가. 정령 주인공과 마법·지원 기술은 제자리에서 쓴다.
static bool melee(const actor *a, const tskill *k) {
    if (k->effect != EF_HIT && !(k->effect == EF_STATUS && !k->ranged && a->cls == 0 && a->hero)) return false;
    if (k->target != TG_ENEMY && k->target != TG_ENEMIES) return false;
    if (a->hero) return a->cls != 2;
    return !k->ranged;
}

static int hit_count(const actor *a, const tskill *k) {
    if (a->hero && k->nmotion) return k->nmotion;
    return k->hits > 0 ? k->hits : 1;
}

// 행동 진행: 0 다가가기(8틱) → 1 동작마다 가운데 프레임에서 맞음 → 2 돌아가기(8틱) → 끝
static void act_tick(void) {
    actor *a = &B.a[B.cur];
    enum { WALK = 8 };
    char msg[64];
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
    if (B.act == A_ITEM) {
        use_item(&B.a[B.target], &B.items[B.item]);
        snprintf(msg, sizeof msg, "%s " T_USED, B.items[B.item].name);
        set_message(msg, 20);
        B.phase = S_MESSAGE;
        B.step_tick = 0;
        return;
    }
    if (B.step == 0 && B.step_tick == 0) {
        resolve_targets();
        if (B.sk.target != TG_SELF && B.ntargets && B.targets[0] != B.cur && B.sk.target != TG_ALLIES) {
            snprintf(msg, sizeof msg, "%s", B.sk.name);
            if (a->party && B.act == A_SKILL) set_message(msg, 20);
        } else if (a->party && B.act == A_SKILL) set_message(B.sk.name, 20);
    }
    bool walk = melee(a, &B.sk) && B.ntargets;
    actor *t = B.ntargets ? &B.a[B.targets[0]] : a;
    int dir = a->party ? -1 : 1;
    int gx = t->home_x - dir * 28, gy = t->home_y + 2;
    switch (B.step) {
    case 0:
        if (!walk) { B.step = 1; B.step_tick = -1; break; }
        if (B.step_tick == 0) set_pose(a, P_WALK);
        B.step_tick++;
        a->x = a->home_x + (gx - a->home_x) * B.step_tick / WALK;
        a->y = a->home_y + (gy - a->home_y) * B.step_tick / WALK;
        if (B.step_tick >= WALK) { B.step = 1; B.step_tick = -1; }
        break;
    case 1: {
        if (B.step_tick < 0) { // 이번 타의 동작 시작
            if (a->hero && B.sk.nmotion) { a->anim = B.sk.motion[B.hit_i % B.sk.nmotion]; set_pose(a, P_SKILL); }
            else set_pose(a, P_ATTACK);
            int n = pose_frames(a);
            B.hit_tick = n > 1 ? n / 2 : 0;
            B.step_tick = 0;
        }
        int n = pose_frames(a);
        if (B.step_tick == B.hit_tick) {
            for (int k = 0; k < B.ntargets; k++) {
                actor *o = &B.a[B.targets[k]];
                if (B.sk.effect == EF_HIT) {
                    if (!o->alive) continue;
                    strike(a, o, &B.sk, B.hit_i);
                    if (a->hero) fx_add(B.sk.fx >= 0 ? B.sk.fx : (int)CALL(FN_HIT_SPARK, CALL(FN_HERO), (uint32_t)a->anim), o);
                } else if (B.hit_i == 0) {
                    apply_effect(a, o, &B.sk);
                    if (a->hero && B.sk.fx >= 0) fx_add(B.sk.fx, o);
                }
            }
        }
        B.step_tick++;
        if (B.step_tick >= (n > 0 ? n : 1) + 2) {
            bool more = ++B.hit_i < hit_count(a, &B.sk) && B.sk.effect == EF_HIT;
            bool any = false;
            for (int k = 0; k < B.ntargets; k++) any |= B.a[B.targets[k]].alive;
            if (more && any) { B.step_tick = -1; break; }
            if (B.sk.effect == EF_HIT) a->charge = false;
            if (walk) { set_pose(a, P_WALK); B.step = 2; B.step_tick = 0; }
            else { set_pose(a, P_IDLE); B.phase = S_MESSAGE; B.step_tick = 0; }
        }
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
        int d = orig_dealt(a), left = S32(m + E_HP) - 1;
        CALL(FN_DAMAGE, m, (uint32_t)-(d < left ? d : left > 0 ? left : 0), 0); // 살아 남은 적은 원본에서도 살아 있게
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
    fprintf(stderr, "[turn] 전투 끝: %s\n", B.won ? "승리" : B.boss_fled ? "보스 도주" : B.fled ? "도망" : "전멸");
    int nb = 0;
    for (int i = 0; i < B.n; i++)
        if (!B.a[i].party && B.a[i].kamikaze && !B.a[i].alive && nb < MAX_ENEMIES) blown[nb++] = B.a[i].mon;
    while (nb < MAX_ENEMIES) blown[nb++] = 0;
    blown_until = now_ms() + 1500;
    uint32_t dead[MAX_ENEMIES];
    int nd = 0;
    for (int i = 0; i < B.n; i++)
        if (!B.a[i].party && !B.a[i].alive && nd < MAX_ENEMIES) dead[nd++] = B.a[i].mon;
    turn_loot_expect(dead, nd, !B.won); // 처치 동작이 끝나 보상이 나온 뒤 획득 창을 한 번에 연다. 도망·전멸이면 드롭은 버린다(경험치는 준다)
    pop_clear();
    B.phase = S_CLOSED;
    B.n = 0;
    cooldown_until = now_ms() + COOLDOWN_MS;
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
    for (int k = 0; k < MAX_FX; k++) if (B.fx[k].anim) B.fx[k].tick++;
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
    if (B.phase == S_CLOSED) turn_party_tick();
    if (B.phase == S_CLOSED && turn_menu_tick()) return;
    if (B.phase == S_CLOSED && turn_loot_tick()) return;
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

static bool valid_target(int i) {
    const actor *o = &B.a[i];
    if (B.act == A_ITEM) return o->party && o->alive;
    switch (B.sk.target) {
    case TG_ENEMY: return !o->party && o->alive;
    case TG_ALLY: return o->party && o->alive;
    case TG_DEAD: return o->party && !o->alive;
    }
    return false;
}

static void move_target(int d) {
    int i = B.target;
    for (int k = 0; k < B.n; k++) {
        i = (i + d + B.n) % B.n;
        if (valid_target(i)) { B.target = i; return; }
    }
}

static void commit(void) {
    actor *a = &B.a[B.cur];
    if (B.act == A_SKILL) a->mp -= skill_cost(a, &B.sk);
    begin_action(B.cur, B.act, B.target);
}

// 대상 고르기. 전체·자신 대상이면 바로 쓴다.
static void start_target(void) {
    int t = B.act == A_ITEM ? TG_ALLY : B.sk.target;
    if (t == TG_ENEMIES || t == TG_ALLIES || t == TG_SELF) { B.target = B.cur; commit(); return; }
    B.target = t == TG_ALLY ? B.cur : -1;
    if (B.target < 0 || !valid_target(B.target)) { B.target = 0; if (!valid_target(0)) move_target(1); }
    B.phase = S_TARGET;
}

// 허리주머니에서 전투에 쓸 수 있는 것만 고른다.
static void load_items(void) {
    titem all[MAX_ITEMS];
    int n = turn_pouch_items(all, MAX_ITEMS);
    B.nitems = 0;
    for (int i = 0; i < n; i++) {
        int e, hp, mp, st, tu;
        if (turn_item_effect(all[i].kind, &e, &hp, &mp, &st, &tu)) B.items[B.nitems++] = all[i];
    }
}

static void choose_command(void) {
    actor *a = &B.a[B.cur];
    switch (B.cursor) {
    case A_ATTACK:
        B.act = A_ATTACK;
        B.sk = a->basic;
        start_target();
        return;
    case A_SKILL:
        if (!a->nskills) { set_message(T_NO_SKILL, 20); return; }
        B.act = A_SKILL;
        B.menu = B.menu_top = 0;
        B.phase = S_SKILLS;
        return;
    case A_ITEM:
        if (a->st[ST_FART] > 0) { set_message(T_FART_BLOCK, 25); return; } // 원작 방귀: 허리주머니가 열리지 않는다
        load_items();
        if (!B.nitems) { set_message(T_NO_ITEM, 25); return; }
        B.act = A_ITEM;
        B.menu = B.menu_top = 0;
        B.phase = S_ITEMS;
        return;
    case A_GUARD: begin_action(B.cur, A_GUARD, B.cur); return;
    case A_ESCAPE: begin_action(B.cur, A_ESCAPE, B.cur); return;
    }
}

static void menu_move(int d, int n) {
    if (n <= 0) return;
    B.menu = (B.menu + d + n) % n;
    if (B.menu < B.menu_top) B.menu_top = B.menu;
    if (B.menu >= B.menu_top + MENU_ROWS) B.menu_top = B.menu - MENU_ROWS + 1;
}

bool turn_battle_key(int type, int code) {
    if (B.phase == S_CLOSED && turn_menu_open()) return turn_menu_key(type, code);
    if (B.phase == S_CLOSED && turn_loot_open()) return turn_loot_key(type, code);
    if (B.phase == S_CLOSED) return pending_monster != 0;
    if (type != 2) return true;
    bool ok = code == KEY_OK || code == 53, back = code == KEY_CLEAR || code == 48;
    bool up = code == KEY_UP || code == 50, down = code == KEY_DOWN || code == 56;
    actor *a = &B.a[B.cur];
    if (B.phase == S_COMMAND) {
        if (up) B.cursor = (B.cursor + A_COUNT - 1) % A_COUNT;
        else if (down) B.cursor = (B.cursor + 1) % A_COUNT;
        else if (ok) choose_command();
    } else if (B.phase == S_SKILLS) {
        if (up) menu_move(-1, a->nskills);
        else if (down) menu_move(1, a->nskills);
        else if (back) B.phase = S_COMMAND;
        else if (ok && a->nskills) {
            const char *why;
            if (!skill_usable(a, &a->skills[B.menu], &why)) set_message(why, 20);
            else { B.sk = a->skills[B.menu]; start_target(); }
        }
    } else if (B.phase == S_ITEMS) {
        if (up) menu_move(-1, B.nitems);
        else if (down) menu_move(1, B.nitems);
        else if (back) B.phase = S_COMMAND;
        else if (ok && B.nitems) { B.item = B.menu; start_target(); }
    } else if (B.phase == S_TARGET) {
        if (up || code == KEY_LEFT || code == 52) move_target(-1);
        else if (down || code == KEY_RIGHT || code == 54) move_target(1);
        else if (ok) commit();
        else if (back) B.phase = B.act == A_SKILL ? S_SKILLS : B.act == A_ITEM ? S_ITEMS : S_COMMAND;
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

// 몬스터 갱신 1단계. 전투가 잡혔거나 열려 있으면 몬스터를 멈춘다.
// 몸이 닿기만 해서는 전투가 되지 않는다(사용자 결정 2026-11-09: 달려서 지나칠 수 있게). 주인공이 때리면 선제, 몬스터 공격에 맞으면 기습.
void f_48bfc(cpu_t *c) {
    if (turn_here() && (pending_monster || B.phase != S_CLOSED)) return;
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
    if (!strcmp(line + 5, "tough") && B.phase != S_CLOSED) { // 시험: 적 HP를 10배로(이 전투 안에서만)
        for (int i = 0; i < B.n; i++) if (!B.a[i].party) { B.a[i].hpmax *= 10; B.a[i].hp *= 10; }
        return true;
    }
    if (!strncmp(line + 5, "status ", 7) && B.phase != S_CLOSED) { // 시험: "status 대상번호 상태번호"
        int i = 0, s = 0;
        if (sscanf(line + 12, "%d %d", &i, &s) == 2 && i >= 0 && i < B.n && s > 0 && s < ST_COUNT) B.a[i].st[s] = ST_TURNS[s];
        return true;
    }
    if (!strncmp(line + 5, "inflict ", 8) && B.phase != S_CLOSED) { // 시험: 첫 적이 대상에게 상태이상을 건다(원작 저항 식)
        int i = 0, s = 0, e = -1;
        sscanf(line + 13, "%d %d", &i, &s);
        for (int k = 0; k < B.n; k++) if (!B.a[k].party && e < 0) e = k;
        if (e >= 0 && i >= 0 && i < B.n) try_status(&B.a[e], &B.a[i], s, 100);
        return true;
    }
    if (!strcmp(line + 5, "endflee") && B.phase != S_CLOSED) { // 시험: 지금 상태 그대로 도망으로 끝낸다(쓰러뜨린 적이 있어도)
        B.fled = true; set_message(T_FLED, 30); B.phase = S_END; B.step_tick = 0;
        return true;
    }
    if (!strncmp(line + 5, "kill ", 5) && B.phase != S_CLOSED) { // 시험: 한 명 쓰러뜨리기
        int i = atoi(line + 10);
        if (i >= 0 && i < B.n && B.a[i].alive) hit(&B.a[i], B.a[i].hp);
        return true;
    }
    if (!strncmp(line + 5, "mp ", 3) && B.phase != S_CLOSED) { // 시험: 아군 MP
        int v = atoi(line + 8);
        for (int i = 0; i < B.n; i++) if (B.a[i].party) B.a[i].mp = v < B.a[i].mpmax ? v : B.a[i].mpmax;
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
