// 내장 서버: 2 공략과 검색. 다른 플레이어는 서버에만 있던 데이터라 가상 함장으로 만든다. 규칙과 근거는 work/net-offline.md의 공략 절.
#include "host.h"
#include "localnet.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

enum {
    NS = 0x20d758,
    NS_IN_BATTLE = NS + 0x28f1, // 0x15894c. 전투 중이면 2.3, 2.4는 경고만 뜨고 묻지 않는다.
    STATE_MAGIC = 0x4c4e5333,
    CAPTAIN_N = 32,
    UID_BASE = 0x1000,
    PAGE = 8,               // 요청 2.10의 개수(0x152d38)이자 화면 줄 수(0x1656e8)
    DEATH_MAX = 24,         // 데쓰노트 칸. 원본 한도는 모른다.
    FIRST_CHECK_SEC = 60,
    COUNTER_CHECK_SEC = 30,
    ATTACK_EVERY_SEC = 6 * 3600,
    OFFLINE_ATTACK_MAX = 3,
    LOG_MAX = 32,
    TITLE_MAX = 49,         // 0x151d98이 0x32바이트 칸에 복사한다
    BODY_MAX = 120,
    UPLOAD_HERO_LEVEL = 39,
    NP_DAILY_PER_LEVEL = 50, // 키냥냥 대사의 일일 지급. 양은 정한 값이다.
    NP_LOST_MAX = 50,
    FATIGUE_PER_FIGHT = 2,
};

typedef struct { uint8_t x, y; } spot;
// 함선 첫 층(/MAP/NT/005 갑판)의 걷는 칸. 층 하나에 여섯 칸까지다(0x162d1c). 입구(10,11, 0x15f238) 둘레에 둔다.
static const spot DECK_SPOTS[6] = {{6, 10}, {8, 11}, {9, 9}, {7, 8}, {10, 8}, {5, 11}};

// 동료 /DAT/SL +0x48, 용병 /DAT/ML 첫 값. 스프라이트와 몬스터 표를 고르는 값이다(0x1753cc).
static const int16_t COMPANIONS[8] = {0x31, 0x38, 0x39, 0x32, 0x36, 0x34, 0x33, 0x35};
static const int16_t MERCS[6] = {0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd};

// 이름은 정한 값이다. 닉네임 칸이 10바이트라 한글 5자까지다.
static const char *const NAMES[CAPTAIN_N] = {
    "\xbd\xc3\xb0\xa3\xbf\xa9\xc7\xe0\xc0\xda", /* 시간여행자 */
    "\xc8\xe6\xc3\xb6\xb3\xaf\xb0\xb3", /* 흑철날개 */
    "\xb1\xb8\xb8\xa7\xb0\xa1\xb8\xa3\xb0\xb3", /* 구름가르개 */
    "\xb8\xb6\xb5\xb5\xc7\xd4\xc0\xe5", /* 마도함장 */
    "\xc7\xcf\xb4\xc3\xb1\xe6", /* 하늘길 */
    "\xb9\xd9\xb6\xf7\xbb\xe7\xb3\xc9\xb2\xdb", /* 바람사냥꾼 */
    "\xba\xd3\xc0\xba\xb5\xc0", /* 붉은돛 */
    "\xc5\xc2\xbe\xe7\xc0\xc7\xb5\xc0", /* 태양의돛 */
    "\xb4\xde\xba\xfb\xc7\xd4\xb4\xeb", /* 달빛함대 */
    "\xbf\xa1\xc5\xd7\xb8\xa3", /* 에테르 */
    "\xb1\xdd\xba\xfb\xbf\xad\xbc\xe8", /* 금빛열쇠 */
    "\xc3\xb5\xb5\xd5\xb1\xba\xc1\xd6", /* 천둥군주 */
    "\xbb\xf5\xba\xae\xc1\xa4\xc2\xfb", /* 새벽정찰 */
    "\xb1\xe2\xb0\xfc\xbb\xe7\xc0\xe5", /* 기관사장 */
    "\xc5\xe9\xb4\xcf\xb9\xd9\xc4\xfb", /* 톱니바퀴 */
    "\xb3\xeb\xc0\xbb\xc7\xd7\xc7\xd8", /* 노을항해 */
    "\xb0\xad\xc3\xb6\xc7\xd4\xc0\xe5", /* 강철함장 */
    "\xc0\xba\xba\xfb\xb5\xb6\xbc\xf6\xb8\xae", /* 은빛독수리 */
    "\xbe\xee\xb5\xd2\xc0\xc7\xb5\xc0", /* 어둠의돛 */
    "\xc7\xaa\xb8\xa5\xc7\xcf\xb4\xc3", /* 푸른하늘 */
    "\xc5\xa9\xb7\xce\xbf\xec", /* 크로우 */
    "\xc0\xba\xc7\xcf\xbc\xf6", /* 은하수 */
    "\xbf\xee\xc7\xd8\xc0\xce", /* 운해인 */
    "\xba\xc0\xc8\xb2\xc0\xc7\xb2\xde", /* 봉황의꿈 */
    "\xba\xb0\xb6\xcb\xc0\xcc", /* 별똥이 */
    "\xb9\xf8\xb0\xb3\xc7\xd4\xb4\xeb", /* 번개함대 */
    "\xbc\xf8\xc7\xb3", /* 순풍 */
    "\xb4\xeb\xb0\xf8\xbf\xf8\xc1\xa4\xb4\xeb", /* 대공원정대 */
    "\xc3\xe6\xc0\xc7\xb1\xe2\xbb\xe7", /* 충의기사 */
    "\xb1\xdd\xb0\xad\xc7\xd4\xc0\xe5", /* 금강함장 */
    "\xba\xd2\xb2\xc9\xc7\xd4\xc0\xe5", /* 불꽃함장 */
    "\xb9\xab\xc7\xd1\xb1\xcb\xb5\xb5", /* 무한궤도 */
};
// 12바이트 칸. 함장 순번으로 돌려 쓰고 빈 값은 길드 없음이다.
static const char *const GUILDS[4] = {
    "\xb1\xb8\xb8\xa7\xbf\xac\xc7\xd5", /* 구름연합 */
    "\xc3\xb6\xc0\xc7\xb3\xaf\xb0\xb3", /* 철의날개 */
    "\xb4\xde\xba\xfb\xc8\xb8", /* 달빛회 */
    "", /*  */
};
// 18바이트 칸(0x153540의 첫 값, 상세 화면 셋째 줄 0x165c9c). 뜻을 알 수 있는 라벨이 없어 함선 문구로 쓴다. 정한 값.
static const char *const MOTTOS[4] = {
    "\xb4\xfd\xba\xf4\x20\xc5\xd7\xb8\xe9\x20\xb4\xfd\xba\xad\xb6\xf3", /* 덤빌 테면 덤벼라 */
    "\xb9\xe6\xbe\xee\xc1\xf8\xc0\xba\x20\xc6\xb0\xc6\xb0\xc7\xcf\xb4\xd9", /* 방어진은 튼튼하다 */
    "\xc7\xcf\xb4\xc3\xc0\xba\x20\xb3\xd0\xb4\xd9", /* 하늘은 넓다 */
    "\xb5\xb9\xbe\xc6\xb0\xa5\x20\xb6\xa7\x20\xc1\xb6\xbd\xc9\xc7\xd8", /* 돌아갈 때 조심해 */
};
// 등급 이름도 클라이언트에 없다. 레벨 구간으로 정한 값이다.
static const struct { int level; const char *name; } GRADES[] = {
    {1, "\xc3\xca\xba\xb8\x20\xc7\xd4\xc0\xe5"}, /* 초보 함장 */
    {4, "\xc1\xa4\xbd\xc4\x20\xc7\xd4\xc0\xe5"}, /* 정식 함장 */
    {8, "\xb3\xeb\xb7\xc3\xc7\xd1\xc7\xd4\xc0\xe5"}, /* 노련한함장 */
};

typedef struct {
    uint32_t magic;
    int64_t last_tick;      // 마지막으로 가상 공격을 굴린 시각
    uint64_t rng;
    int32_t fame[CAPTAIN_N]; // 함장별 명성치. 내 공격 결과로 변한다.
    uint64_t death[DEATH_MAX];
    int32_t death_n;
    // 진행 중인 전투. 2.2가 끝낸다. 시작할 때 NP를 빼므로 도중에 끊어도 되돌리지 않는다.
    uint64_t battle_uid;
    int32_t battle_cost, battle_kind; // 0 일반, 1 카운터, 2 더블카운터
    // 푸시로 물어본 반격. 2.5, 2.6이 끝낸다.
    uint64_t offer_uid;
    int32_t offer_kind;
    // 전적(1.10, 1.11용, 우편 쪽과 병합할 때 맞출 형식)
    int32_t log_n, log_seq;
    int32_t day, month;     // 오늘 값(fame_today 등)을 비우는 기준
    uint64_t revenge_uid;   // 내 반격에 진 함장. 다음 가상 공격은 이 함장이 다시 온다.
    struct { uint64_t id; char title[TITLE_MAX + 1]; char body[BODY_MAX]; } log[LOG_MAX];
} shadow_state;

static shadow_state st;
static bool st_loaded;
static int64_t next_check;

static void st_save(void) {
    if (!ln_save("shadow", &st, sizeof st)) fprintf(stderr, "[net] 공략 상태 저장 실패\n");
}

static uint32_t rnd(void) { // xorshift. 시드와 함께 저장해 재시작해도 이어진다.
    uint64_t x = st.rng;
    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
    st.rng = x;
    return (uint32_t)(x >> 16);
}

// 함장 레벨은 순번으로 고정한다. 내 레벨이 올라도 위아래로 상대가 남는다.
static int captain_level(int i) { return 1 + i * 2 / 3; }
static int captain_base_fame(int i) { return captain_level(i) * captain_level(i) * 40 + (i * 37) % 60; }

static void st_load(void) {
    if (st_loaded) return;
    st_loaded = true;
    if (ln_load("shadow", &st, sizeof st) == (long)sizeof st && st.magic == STATE_MAGIC) return;
    memset(&st, 0, sizeof st);
    st.magic = STATE_MAGIC;
    st.rng = 0x9e3779b97f4a7c15ULL ^ (uint64_t)time(NULL);
    st.last_tick = (int64_t)time(NULL);
    for (int i = 0; i < CAPTAIN_N; i++) st.fame[i] = captain_base_fame(i);
}

static int find_uid(uint64_t uid) {
    return uid >= UID_BASE && uid < (uint64_t)UID_BASE + CAPTAIN_N ? (int)(uid - UID_BASE) : -1;
}
static int find_nick(const char *nick) {
    for (int i = 0; i < CAPTAIN_N; i++)
        if (!strncmp(NAMES[i], nick, 10)) return i;
    return -1;
}

static int my_score(void) {
    const ln_profile *p = ln_me();
    return (p->level - 1) * p->level * 50 + p->fame; // 레벨마다 fame_max(레벨*100)를 채운 합(ln_fame_add)
}

// 내 RANK와 TODAY 변화. 함장 중 나보다 높은 수 + 1이다.
static void update_my_rank(void) {
    ln_profile *p = ln_me();
    int r = 1, s = my_score();
    for (int i = 0; i < CAPTAIN_N; i++) if (st.fame[i] > s) r++;
    if (st.day) p->rank_today += p->rank - r; // 처음 순위를 매길 때는 변화가 아니다
    p->rank = r;
    for (size_t i = 0; i < sizeof GRADES / sizeof *GRADES; i++)
        if (p->level >= GRADES[i].level) { memset(p->grade, 0, sizeof p->grade); strncpy(p->grade, GRADES[i].name, 10); }
    ln_me_save();
}

static int captain_rank(int idx) {
    int r = 1;
    for (int i = 0; i < CAPTAIN_N; i++) if (st.fame[i] > st.fame[idx]) r++;
    if (my_score() > st.fame[idx]) r++;
    return r;
}

// 공격 소모와 보상. 클라이언트에 없어 정한 값이다.
static int attack_cost(int level) { return 20 + level * 10; }
static int win_np(int cost, int level, int my_level) { int d = level - my_level; return cost * 2 + (d > 0 ? d * 20 : 0); }
static int win_fame(int level, int my_level) { int d = level - my_level; int f = 10 + d * 5; return f < 2 ? 2 : f; }
enum { LOSE_FAME = -2 };
// 공략 제한시간(초). 내 함선의 0x214 초기값 180과 같은 눈금이다.
static int captain_limit(int level) { int t = 170 + level * 5; return t > 300 ? 300 : t; }

static void w_captain(int i) {
    int level = captain_level(i);
    char motto[19] = {0}, nick[11] = {0}, guild[13] = {0}, grade[11] = {0};
    strncpy(motto, MOTTOS[i % 4], 18);
    strncpy(nick, NAMES[i], 10);
    strncpy(guild, GUILDS[i % 4], 12);
    strncpy(grade, GRADES[level >= 8 ? 2 : level >= 4 ? 1 : 0].name, 10);
    ln_wbytes(motto, 18);
    ln_w8(0);   // 0x1658a6: 1이면 이름 앞에 표시가 붙는다. 뜻을 모른다.
    ln_w8('q'); // 이름 색 글자. 0이면 %c에서 문자열이 끊긴다.
    ln_w64((uint64_t)UID_BASE + (uint64_t)i);
    ln_wbytes(nick, 10);
    ln_w32((uint32_t)captain_rank(i)); // 0xcf0 RANK. 음수면 ?
    ln_w32((uint32_t)level);           // 0xd10 LV
    ln_w32((uint32_t)attack_cost(level)); // 0xd30 공격시 소모
    ln_w32((uint32_t)captain_limit(level)); // 0xd50 공략제한시간. 0이면 줄을 안 그린다
    ln_w32(0); ln_w32(0); ln_w32(0);   // 0xd70, 0xd90, 0xdb0 0xf10~으로 복사만 되고 읽는 코드가 없다
    ln_w32((uint32_t)st.fame[i]);      // 0xdd0 명성치
    ln_wbytes(grade, 10);              // 0xdf0 등급
    ln_wbytes(guild, 12);              // 0xc88 길드
}

static void search(ln_req *q) {
    char tag[3] = {0}, term[11] = {0};
    ln_bytes(q, tag, 2);
    ln_u8(q); // 개수 8
    int page = (int)ln_u32(q);
    ln_bytes(q, term, 10);
    int idx[CAPTAIN_N], n = 0;
    int my_level = ln_me()->level;
    if (!strcmp(tag, "AT")) { // 랜덤: 내 레벨 근처에서 고르되 새로고침마다 달라진다
        int pool[CAPTAIN_N], m = 0;
        for (int i = 0; i < CAPTAIN_N; i++) {
            int d = captain_level(i) - my_level;
            if (d >= -3 && d <= 5) pool[m++] = i;
        }
        if (!m) for (int i = 0; i < CAPTAIN_N; i++) pool[m++] = i;
        while (m && n < PAGE) {
            int k = (int)(rnd() % (uint32_t)m);
            idx[n++] = pool[k];
            pool[k] = pool[--m];
        }
    } else if (!strcmp(tag, "RK") || !strcmp(tag, "RV")) { // 명성 순. 라이벌은 내 바로 위아래다.
        int order[CAPTAIN_N];
        for (int i = 0; i < CAPTAIN_N; i++) order[i] = i;
        for (int i = 1; i < CAPTAIN_N; i++)
            for (int k = i; k > 0 && st.fame[order[k]] > st.fame[order[k - 1]]; k--) { int t = order[k]; order[k] = order[k - 1]; order[k - 1] = t; }
        if (!strcmp(tag, "RK")) {
            for (int i = 0; i < CAPTAIN_N; i++) idx[n++] = order[i];
        } else {
            int s = my_score(), at = 0;
            while (at < CAPTAIN_N && st.fame[order[at]] > s) at++;
            for (int i = at - 4 < 0 ? 0 : at - 4; i < CAPTAIN_N && n < PAGE; i++) idx[n++] = order[i];
        }
    } else if (!strcmp(tag, "UR") || !strcmp(tag, "GL")) {
        for (int i = 0; i < CAPTAIN_N; i++) {
            const char *s = tag[0] == 'U' ? NAMES[i] : GUILDS[i % 4];
            if (term[0] && strstr(s, term)) idx[n++] = i;
        }
    } else if (!strcmp(tag, "DT")) {
        for (int i = 0; i < st.death_n; i++) { int c = find_uid(st.death[i]); if (c >= 0) idx[n++] = c; }
    } else {
        ln_fail(2, 10, "?");
        return;
    }
    update_my_rank();
    int first = page * PAGE, count = n - first;
    if (count < 0) count = 0;
    if (count > PAGE) count = PAGE;
    ln_begin(2, 10, 0);
    ln_w8((uint8_t)count);
    for (int i = 0; i < count; i++) w_captain(idx[first + i]);
    ln_w8(first + count < n); // 0xf36 다음 쪽 있음
    ln_send();
}

// 방어 부대 항목. 형식은 0x155398, 뜻은 0x155210과 같다.
static void w_defender(int64_t uid, int16_t id, int level, const char *name, int floor, int x, int y, int fatigue, int hp, int atk, int pdef, int mdef, int crit) {
    size_t len = strnlen(name, 26);
    ln_w64((uint64_t)uid);
    ln_w16((uint16_t)id);
    ln_w8((uint8_t)level);
    ln_w8((uint8_t)len);
    ln_wbytes(name, len);
    ln_w16(0);                 // +0x778 읽는 코드가 없다
    ln_w16((uint16_t)floor);   // +0x77a
    ln_w16((uint16_t)x);       // +0x77c
    ln_w16((uint16_t)y);       // +0x77e
    ln_w8((uint8_t)fatigue);   // +0x780
    ln_w32(0);                 // +0x784
    ln_w8(0);                  // +0x788
    ln_w32((uint32_t)hp);      // HP
    ln_w8(1);                  // 공격력 개수
    ln_w32((uint32_t)atk);
    ln_w16((uint16_t)pdef);
    ln_w16((uint16_t)mdef);
    ln_w8((uint8_t)crit);
    ln_w8(0);                  // +0x7a5
}

// 4.1 본문의 주인공 레벨(0x154914 +0x101). 올린 적 없으면 1이다.
static int hero_level(void) {
    size_t n = 0;
    const uint8_t *u = ln_upload(&n);
    return u && n > UPLOAD_HERO_LEVEL && u[UPLOAD_HERO_LEVEL] > 0 ? u[UPLOAD_HERO_LEVEL] : 1;
}

// 동료, 용병과 같은 수치 규칙(localnet_state.c member_stats)을 쓴다.
static void w_captain_roster(int c) {
    int level = captain_level(c);
    int n = 2 + level / 3;
    if (n > 6) n = 6;
    int hero = hero_level();
    ln_w8((uint8_t)n);
    for (int i = 0; i < n; i++) {
        // 내 수준에 맞춘다. 함장 레벨은 네트워크 레벨이라 주인공 강함과 무관하다.
        int unit = hero + (level - ln_me()->level) / 2 + i % 3 - 1;
        if (unit < 1) unit = 1;
        if (unit > 99) unit = 99;
        bool merc = level >= 8 && i % 2;
        int16_t id = merc ? MERCS[(c + i) % 6] : COMPANIONS[(c + i) % 8];
        spot s = DECK_SPOTS[i];
        w_defender(merc ? -(int64_t)id : (int64_t)(i + 1), id, unit, NAMES[c], 0, s.x, s.y, 0,
                   100 + unit * 40, 10 + unit * 4, 5 + unit * 2, 5 + unit * 2, 5);
    }
}

static void begin_battle(int minor, int c, int limit) {
    st.battle_uid = (uint64_t)UID_BASE + (uint64_t)c;
    st_save();
    ln_begin(2, minor, 0);
    ln_w64(st.battle_uid);     // 0x6e4, 2.2가 돌려보낸다
    w_captain_roster(c);
    ln_w32((uint32_t)limit);   // 0x28c8 이번 전투 제한시간(초)
    ln_send();
}

static const char MSG_NOT_FOUND[] = "\xc7\xd4\xc0\xe5\xc0\xbb\x20\xc3\xa3\xc0\xbb\x20\xbc\xf6\x20\xbe\xf8\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 함장을 찾을 수 없습니다.
static const char MSG_NO_NP[] = "\x4e\x50\xb0\xa1\x20\xba\xce\xc1\xb7\xc7\xd5\xb4\xcf\xb4\xd9\x2e"; // NP가 부족합니다.
static const char MSG_OK[] = "\xc3\xb3\xb8\xae\xb5\xc7\xbe\xfa\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 처리되었습니다.
static const char MSG_NO_ROSTER[] = "\xb9\xe8\xc4\xa1\xb5\xc8\x20\xb9\xe6\xbe\xee\xc1\xf8\xc0\xcc\x20\xbe\xf8\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 배치된 방어진이 없습니다.
static const char MSG_FULL[] = "\xb5\xa5\xbe\xb2\xb3\xeb\xc6\xae\xb0\xa1\x20\xb0\xa1\xb5\xe6\x20\xc3\xa1\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 데쓰노트가 가득 찼습니다.
static const char FMT_ATTACKED[] = "\x25\x73\x20\xc7\xd4\xc0\xe5\xc0\xcc\x20\xb0\xf8\xb0\xdd\xc7\xd8\x20\xbf\xd4\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // %s 함장이 공격해 왔습니다.
static const char FMT_AGAIN[] = "\x25\x73\x20\xc7\xd4\xc0\xe5\xc0\xcc\x20\xc0\xe7\xc2\xf7\x20\xb0\xf8\xb0\xdd\xc7\xd8\x20\xbf\xd4\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // %s 함장이 재차 공격해 왔습니다.
static const char MSG_HELD[] = "\xc7\xd4\xbc\xb1\xc0\xbb\x20\xc1\xf6\xc4\xd1\x20\xb3\xc2\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 함선을 지켜 냈습니다.
static const char MSG_LOST[] = "\xc7\xd4\xbc\xb1\xc0\xbb\x20\xbb\xa9\xbe\xd1\xb0\xe5\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 함선을 빼앗겼습니다.
static const char MSG_DUP[] = "\xc0\xcc\xb9\xcc\x20\xb5\xee\xb7\xcf\xb5\xc8\x20\xc7\xd4\xc0\xe5\xc0\xd4\xb4\xcf\xb4\xd9\x2e"; // 이미 등록된 함장입니다.

static void attack(ln_req *q) {
    char nick[11] = {0};
    ln_bytes(q, nick, 10);
    int c = find_nick(nick);
    if (c < 0) { ln_fail(2, 1, MSG_NOT_FOUND); return; }
    int cost = attack_cost(captain_level(c));
    if (!ln_np_spend(cost)) { ln_fail(2, 1, MSG_NO_NP); return; }
    st.battle_cost = cost;
    st.battle_kind = 0;
    begin_battle(1, c, captain_limit(captain_level(c)));
}

static void log_add(const char *title, const char *body) {
    if (st.log_n == LOG_MAX) { memmove(st.log, st.log + 1, sizeof st.log[0] * (LOG_MAX - 1)); st.log_n--; }
    st.log[st.log_n].id = (uint64_t)++st.log_seq;
    snprintf(st.log[st.log_n].title, sizeof st.log[0].title, "%s", title);
    snprintf(st.log[st.log_n].body, sizeof st.log[0].body, "%s", body);
    st.log_n++;
}

static void settle(ln_req *q) {
    uint64_t uid = ((uint64_t)ln_u32(q) << 32);
    uid |= ln_u32(q);
    int win = (int8_t)ln_u8(q); // 0x6ed. 0x145220이 전멸이면 1, 시간 초과나 주인공 사망이면 0
    int c = find_uid(uid);
    if (c < 0 || uid != st.battle_uid) { ln_fail(2, 2, MSG_NOT_FOUND); return; }
    ln_profile *p = ln_me();
    int level = captain_level(c), cost = st.battle_cost, gain = 0, fame_delta;
    if (win > 0) {
        gain = win_np(attack_cost(level), level, p->level);
        fame_delta = win_fame(level, p->level);
        st.fame[c] -= fame_delta;
        if (st.fame[c] < 0) st.fame[c] = 0;
        ln_np_add(gain);
    } else {
        fame_delta = LOSE_FAME;
        st.fame[c] += 2;
    }
    if (p->fame + fame_delta < 0) fame_delta = -p->fame;
    ln_fame_add(fame_delta);
    update_my_rank();
    if (win > 0 && st.battle_kind) st.revenge_uid = uid; // 반격에 지면 다시 온다
    st.battle_uid = 0;
    st_save();
    ln_begin(2, 2, 0);
    ln_w32((uint32_t)cost);       // 0x220 소모
    ln_w32((uint32_t)gain);       // 0x224 획득
    ln_w32((uint32_t)p->np);      // 0x1ec 잔여
    ln_w32((uint32_t)fame_delta); // 0x228 명성 증감. 음수면 부호 없는 쪽 문자열로 그린다(0x144e50)
    ln_w32((uint32_t)p->fame);    // 0x1f4
    ln_wstr16(MSG_OK);
    ln_send();
}

static void counter_accept(void) {
    int c = find_uid(st.offer_uid);
    if (c < 0) { ln_fail(2, 5, MSG_NOT_FOUND); return; }
    ln_profile *p = ln_me();
    int kind = st.offer_kind;
    st.offer_uid = 0; st.offer_kind = 0;
    st.battle_cost = 0;
    st.battle_kind = kind;
    if (kind == 2 && p->double_counter) { p->double_counter--; ln_me_save(); }
    begin_battle(5, c, kind == 2 ? p->limit_double : p->limit_counter);
}

static void counter_decline(void) {
    st.offer_uid = 0; st.offer_kind = 0;
    st_save();
    ln_begin(2, 6, 0);
    ln_wstr16(MSG_OK);
    ln_send();
}

static void my_shadow(void) { // 2.8 모의전투. 배치된 방어 부대만 나온다.
    int n, placed = 0;
    ln_member *m = ln_members(&n);
    for (int i = 0; i < n; i++) if (m[i].floor >= 0 && m[i].room >= 0 && m[i].slot >= 0) placed++;
    if (!placed) { ln_fail(2, 8, MSG_NO_ROSTER); return; }
    ln_begin(2, 8, 0);
    ln_w8((uint8_t)placed);
    for (int i = 0; i < n; i++)
        if (m[i].floor >= 0 && m[i].room >= 0 && m[i].slot >= 0)
            w_defender(m[i].uid, m[i].id, m[i].level, m[i].name, m[i].floor, m[i].room, m[i].slot, m[i].fatigue, m[i].hp, m[i].attack, m[i].pdef, m[i].mdef, m[i].crit);
    ln_w32((uint32_t)ln_me()->limit_attack); // 0x214
    ln_send();
}

static void death_add(ln_req *q) {
    char nick[11] = {0};
    ln_bytes(q, nick, 10);
    int c = find_nick(nick);
    if (c < 0) { ln_fail(2, 11, MSG_NOT_FOUND); return; }
    uint64_t uid = (uint64_t)UID_BASE + (uint64_t)c;
    for (int i = 0; i < st.death_n; i++)
        if (st.death[i] == uid) { ln_fail(2, 11, MSG_DUP); return; }
    if (st.death_n == DEATH_MAX) { ln_fail(2, 11, MSG_FULL); return; }
    st.death[st.death_n++] = uid;
    st_save();
    ln_begin(2, 11, 0);
    ln_wstr16(MSG_OK);
    ln_send();
}

static void death_del(ln_req *q) {
    uint64_t uid = (uint64_t)ln_u32(q) << 32;
    uid |= ln_u32(q);
    for (int i = 0; i < st.death_n; i++)
        if (st.death[i] == uid) {
            st.death[i] = st.death[--st.death_n];
            break;
        }
    st_save();
    ln_begin(2, 12, 0);
    ln_wstr16(MSG_OK);
    ln_send();
}

int ln_shadow_log_count(void) {
    st_load();
    return st.log_n;
}

void ln_shadow_log_at(int i, uint64_t *id, const char **title, const char **body) {
    st_load();
    *id = st.log[i].id;
    *title = st.log[i].title;
    *body = st.log[i].body;
}

// 가상 함장의 공격 한 건. 전적과 프로필만 바꾼다. 결과는 배치 수준과 함장 레벨을 견준다.
static int virtual_attack(void) {
    ln_profile *p = ln_me();
    int c = find_uid(st.revenge_uid);
    if (c < 0) { // 내 레벨 근처에서 고른다
        int level = p->level + (int)(rnd() % 5) - 2;
        c = (int)(rnd() % CAPTAIN_N);
        for (int i = 0; i < CAPTAIN_N; i++) { int k = (c + i) % CAPTAIN_N; if (captain_level(k) == level) { c = k; break; } }
    }
    int n, power = 0;
    ln_member *m = ln_members(&n);
    bool changed = false;
    for (int i = 0; i < n; i++)
        if (m[i].floor >= 0 && m[i].room >= 0 && m[i].slot >= 0) {
            power += m[i].level;
            int f = m[i].fatigue + FATIGUE_PER_FIGHT;
            m[i].fatigue = (uint8_t)(f > 100 ? 100 : f);
            changed = true;
        }
    if (changed) ln_members_save();
    bool held = power > 0 && power + (int)(rnd() % 20) > hero_level() + captain_level(c) * 2 + 10;
    char title[TITLE_MAX + 1];
    p->hit += 1;
    p->hit_month += 1;
    if (held) {
        p->defend += 1;
        p->defend_month += 1;
        ln_fame_add(1);
    } else {
        int lost = p->np / 20;
        if (lost > NP_LOST_MAX) lost = NP_LOST_MAX;
        ln_np_add(-lost);
        if (p->fame > 0) ln_fame_add(-1);
    }
    snprintf(title, sizeof title, "%s", NAMES[c]);
    log_add(title, held ? MSG_HELD : MSG_LOST);
    update_my_rank();
    return c;
}

// 접속 유지(0,0)마다 불린다. 원본은 다른 유저의 공격을 언제든 2.3, 2.4로 밀어 넣었다(0x153138, tdf/HELP "실시간으로 침공").
void ln_shadow_tick(void) {
    st_load();
    if (!ln_has_account()) return;
    int64_t now = (int64_t)time(NULL);
    if (!next_check) next_check = now + FIRST_CHECK_SEC; // 함선 화면이 뜨기 전에는 묻지 않는다
    if (now < next_check) return;
    next_check = now + COUNTER_CHECK_SEC;
    ln_profile *p = ln_me();
    time_t t = (time_t)now;
    struct tm *tm = localtime(&t);
    int day = tm->tm_year * 1000 + tm->tm_yday, month = tm->tm_year * 100 + tm->tm_mon;
    bool dirty = false;
    if (day != st.day) { // 오늘 값을 비우고 일일 NP를 준다. 첫날은 초기값 그대로다.
        if (st.day) {
            p->fame_today = 0;
            p->rank_today = 0;
            ln_np_add(NP_DAILY_PER_LEVEL * p->level);
        }
        st.day = day;
        dirty = true;
    }
    if (month != st.month) {
        if (st.month) p->hit_month = p->defend_month = 0;
        st.month = month;
        dirty = true;
    }
    if (dirty) ln_me_save();
    if (!st.last_tick) st.last_tick = now;
    int64_t elapsed = now - st.last_tick;
    if (elapsed < ATTACK_EVERY_SEC) {
        if (dirty) st_save();
        return;
    }
    int count = (int)(elapsed / ATTACK_EVERY_SEC);
    if (count > OFFLINE_ATTACK_MAX) count = OFFLINE_ATTACK_MAX;
    st.last_tick = now;
    int c = -1;
    for (int i = 0; i < count; i++) c = virtual_attack();
    bool revenge = st.revenge_uid != 0;
    st.revenge_uid = 0;
    // 마지막 한 건만 묻는다. 전투 중이면 게임이 경고만 띄우므로 묻는 상태를 남기지 않는다(0x153138).
    bool ask = !cpu.m[NS_IN_BATTLE];
    int kind = revenge && p->double_counter > 0 ? 2 : 1;
    uint64_t uid = (uint64_t)UID_BASE + (uint64_t)c;
    char text[80];
    snprintf(text, sizeof text, kind == 2 ? FMT_AGAIN : FMT_ATTACKED, NAMES[c]);
    if (ask) { st.offer_uid = uid; st.offer_kind = kind; }
    st_save();
    ln_begin(2, kind == 2 ? 4 : 3, 0);
    ln_w64(uid);                // 0x6e4. 전투 중이 아니면 이 값으로 2.5를 보낸다.
    ln_wstr16(text);
    ln_send();
}

bool ln_shadow(ln_req *q) {
    st_load();
    switch (q->minor) {
    case 1: attack(q); return true;
    case 2: settle(q); return true;
    case 5: counter_accept(); return true;
    case 6: counter_decline(); return true;
    case 8: my_shadow(); return true;
    case 10: search(q); return true;
    case 11: death_add(q); return true;
    case 12: death_del(q); return true;
    }
    return false;
}
