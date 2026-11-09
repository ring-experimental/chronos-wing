// 턴제 전투의 스킬 표와 허리주머니 아이템.
// 주인공: 원작 스킬 표(0x1abcb8, 0x130 × 39)에서 배운 것만(+0 != 0xff) 고르고, 동작은 H/DS<클래스>의 줄(+0x1d~+0x1e)을 쓴다.
// 피해는 전투 쪽에서 원본 식(0x101ff4(동작) × 0x102188(동작))으로 계산한다. 동료 스킬은 역할에 맞춰 정한 값이다.
// 근거와 정한 값은 work/turn-plan.md "스킬·역할·상태이상·드랍 없애기".
#include "restored.h"
#include "turn.h"
#include <stdio.h>
#include <stdlib.h>

enum {
    SKILLS = 0x1abcb8, SKILL_SIZE = 0x130, SK_STATE = 0, SK_NAME = 2, SK_ROW_FIRST = 0x1d, SK_ROW_LAST = 0x1e, SK_LEVEL = 0x1f,
    NOT_LEARNED = 0xff,
    DS_ROW = 9, DS_MAX = 64,
    FN_POUCH = 0x128f30, POUCH_SLOTS = 0x12, POUCH_CURSOR = 0x13,
    FN_POUCH_ITEM = 0x1387a4, FN_ITEM_KIND = 0x138410, FN_ITEM_ID = 0x138428, FN_ITEM_COUNT = 0x13843c,
    FN_ITEM_ADD_COUNT = 0x138458, FN_POUCH_REMOVE = 0x13b1cc, FN_ITEM_NAME = 0x117df0,
};

static const int CLASS_BASE[3] = {0, 13, 23}, CLASS_COUNT[3] = {13, 10, 16};

typedef struct { int group, magic, base, per, mp, mp_gain, kb, anim; } ds_row;
static ds_row ds[3][DS_MAX];
static int ds_n[3] = {-1, -1, -1};

static void ds_load(int cls) {
    if (ds_n[cls] >= 0) return;
    ds_n[cls] = 0;
    char path[1100];
    snprintf(path, sizeof path, "%s/H/DS%03d", game_dir, cls);
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "[turn] %s \xbe\xf8\xc0\xbd\n", path); return; } // [turn] %s 없음\n
    uint8_t b[2 + DS_ROW * DS_MAX];
    size_t len = fread(b, 1, sizeof b, f);
    fclose(f);
    int n = len >= 2 ? b[0] | b[1] << 8 : 0;
    for (int r = 0; r < n && r < DS_MAX && 2 + DS_ROW * (size_t)(r + 1) <= len; r++) {
        // [줄][그룹][마법][기본%][레벨당%][MP 소모][적중 MP][넉백][동작 id]
        const uint8_t *q = b + 2 + DS_ROW * r;
        ds[cls][r] = (ds_row){q[1], q[2], q[3], q[4], q[5], q[6], q[7], q[8]};
        ds_n[cls] = r + 1;
    }
}

static bool learned(int idx) { cpu_t *c = &cpu; return R8(SKILLS + idx * SKILL_SIZE + SK_STATE) != NOT_LEARNED; }

// 턴제 규칙. 원작 스킬 번호(분류 안 번호)별로 대상과 부가 효과를 정한다. 정령의 기도와 소환은 마법을 쓰기 위한 준비라 명령으로 두지 않는다.
typedef struct { int target, effect, status, chance, amount, turns, need, fx; bool hide; } rule;
static rule hero_rule(int cls, int k) {
    rule r = {TG_ENEMY, EF_HIT, 0, 0, 0, 0, NEED_NONE, -1, false};
    switch (cls * 100 + k) {
    case 0: case 1: case 100: case 101: case 200: r.hide = true; break;           // 공격 명령으로 쓴다
    case 3: r.effect = EF_STATUS; r.status = ST_GROGGY; r.chance = 100; break;    // 크런치: 붙잡아 한 차례 묶는다
    case 4: r.status = ST_GROGGY; r.chance = 60; break;                           // 래피도 슬링: 던져 넘어뜨린다(넉백 7)
    case 8: r.target = TG_SELF; r.effect = EF_EVADE; r.turns = 1; break;          // 롤링
    case 10: r.target = TG_SELF; r.effect = EF_POWERBURN; r.turns = 3; r.need = NEED_MP_FULL; break; // 파워번(원작: MP 가득일 때)
    case 11: case 12: r.need = NEED_POWERBURN; break;                              // 파워콤보, 파워피니쉬(원작: 파워번 상태에서)
    case 102: r.hide = true; break;                                               // 스피어링: 스피어링 콤보를 배우면 그쪽을 쓴다(아래)
    case 108: r.target = TG_SELF; r.effect = EF_CHARGE; break;                    // 차지: 다음 공격 ×1.3(원본 0x101ff4의 검 차지 배율)
    case 109: r.target = TG_ENEMIES; break;                                       // 휠윈드
    case 201: case 203: case 207: case 211: r.hide = true; break;                 // 정령의 기도, 운디네·사라만다·가이아 소환
    case 202: r.target = TG_SELF; r.effect = EF_EVADE; r.turns = 1; break;        // 워프스텝
    case 204: r.status = ST_SLOW; r.chance = 100; r.fx = 0x25; break;             // 아이스볼트: 원작 둔화
    case 205: r.status = ST_SNOW; r.chance = 80; r.fx = 0x27; break;              // 프로스트월: 원작 얼림
    case 206: r.target = TG_ENEMIES; r.fx = 0x2c; break;                          // 블리자드
    case 208: r.status = ST_BURN; r.chance = 30; r.fx = 0x30; break;              // 파이어볼
    case 209: r.target = TG_ENEMIES; r.status = ST_BURN; r.chance = 25; r.fx = 0x32; break; // 플레임월
    case 210: r.status = ST_BURN; r.chance = 40; r.fx = 0x33; break;              // 메테오
    case 212: r.target = TG_ALLIES; r.effect = EF_BUFF_DEF; r.amount = 20; r.turns = 3; break; // 가이아트릭: 방어강화제와 같은 -20%
    case 213: r.status = ST_POISON; r.chance = 100; r.fx = 0x38; break;           // 루팅트랩: 원작 지속 피해
    case 214: r.target = TG_ENEMIES; r.fx = 0x24; break;                          // 어스퀘이커
    case 215: r.target = TG_SELF; r.effect = EF_MP; r.amount = 25; break;         // 정신집중: MP 최대치의 25%
    }
    return r;
}

static void from_rows(tskill *s, int cls, int idx) {
    cpu_t *c = &cpu;
    uint32_t rec = SKILLS + idx * SKILL_SIZE;
    int first = R8(rec + SK_ROW_FIRST), last = R8(rec + SK_ROW_LAST), sum = 0;
    memcpy(s->name, gstr(rec + SK_NAME), sizeof s->name - 1);
    s->name[sizeof s->name - 1] = 0;
    s->level = R8(rec + SK_LEVEL);
    for (int r = first; r <= last && r < ds_n[cls] && s->nmotion < TS_MOTIONS; r++) {
        const ds_row *d = &ds[cls][r];
        s->motion[s->nmotion++] = d->anim;
        if (d->mp > s->mp) s->mp = d->mp; // 콤보도 원작 설명의 MP는 한 번이다
        s->mp_gain += d->mp_gain;
        s->magic |= d->magic;
        sum += d->base + s->level * d->per;
    }
    s->power = sum;
    s->hits = s->nmotion;
}

// 주인공 스킬. basic에는 공격 명령(클래스 기본 콤보)을, out에는 배운 스킬 명령을 채운다.
int turn_hero_skills(int cls, tskill *basic, tskill *out, int max) {
    if (cls < 0 || cls > 2) cls = 0;
    ds_load(cls);
    int base = CLASS_BASE[cls], n = 0;
    memset(basic, 0, sizeof *basic);
    // 공격: 콤보를 배웠으면 콤보(격투 1, 검 1), 아니면 첫 기술. 정령은 마도탄 콤보(MP는 받지 않는다)
    int b = cls == 2 ? 0 : learned(base + 1) ? 1 : 0;
    from_rows(basic, cls, base + b);
    basic->target = TG_ENEMY;
    basic->effect = EF_HIT;
    basic->mp = 0;
    basic->ranged = cls == 2;
    basic->fx = cls == 2 ? 0x24 : -1;
    for (int k = 0; k < CLASS_COUNT[cls] && n < max; k++) {
        int idx = base + k;
        if (!learned(idx)) continue;
        rule r = hero_rule(cls, k);
        if (cls == 1 && k == 2 && !learned(base + 3)) r.hide = false; // 스피어링 콤보가 없으면 스피어링을 보인다
        if (r.hide) continue;
        tskill *s = &out[n];
        memset(s, 0, sizeof *s);
        from_rows(s, cls, idx);
        s->target = r.target;
        s->effect = r.effect;
        s->status = r.status;
        s->chance = r.chance;
        s->amount = r.amount;
        s->turns = r.turns;
        s->need = r.need;
        s->fx = r.fx;
        s->ranged = cls == 2;
        if (s->effect != EF_HIT && s->effect != EF_STATUS) s->power = 0;
        // 원작에서 MP를 안 쓰는 연계기(피니쉬 해머, 데몬즈 커터 등)는 실시간 콤보의 대가가 없으므로 위력에 비례한 MP를 받는다(정한 값).
        if (!s->mp && (s->effect == EF_HIT || s->effect == EF_STATUS)) s->mp = 5 + s->power / 25;
        n++;
    }
    return n;
}

// 동료 순서는 0 엔지, 1 레이, 2 리키, 3 후거스, 4 보우머, 5 쟝, 6 펭, 7 팡. 기본 공격에도 같은 원작 상태이상을 10%로 붙인다.
typedef struct { const char *name; int mp, target, effect, power, magic, hits, pierce, status, chance, amount, turns; } mdef;
// 동료 스킬(정한 값). 상태이상은 원작 동료 공격표(EMD/C3 37~45, 공격 0)를 따른다(사용자 결정 2026-11-09):
// 엔지 눈사람, 레이·후거스 그로기, 리키·쟝 둔화(이속저주), 펭 화상, 팡 감전, 보우머 없음. 역할군(딜러·힐러·디버퍼·탱커·버퍼·마법)은 그대로 둔다.
static const mdef MEMBER_SKILLS[8][5] = {
    {{"\xbe\xf3\xc0\xbd \xba\xa3\xb1\xe2", 10, TG_ENEMY, EF_HIT, 160, 0, 2, 0, ST_SNOW, 30}, {"\xb0\xfc\xc5\xeb \xc0\xcf\xb0\xdd", 16, TG_ENEMY, EF_HIT, 170, 0, 1, 1}}, // 얼음 베기 / 관통 일격
    {{"\xc4\xa1\xc0\xaf\xc0\xc7 \xb1\xe2\xbf\xf8", 10, TG_ALLY, EF_HEAL, 120, 1, 1, 0, 0, 0, 20}, {"\xc8\xb8\xba\xb9\xc0\xc7 \xb1\xe2\xbf\xf8", 22, TG_ALLIES, EF_HEAL, 60, 1, 1, 0, 0, 0, 12}, // 치유의 기원 / 회복의 기원
     {"\xba\xce\xc8\xb0\xc0\xc7 \xb1\xe2\xbf\xf8", 30, TG_DEAD, EF_REVIVE, 0, 1, 1, 0, 0, 0, 30}, {"\xc1\xa4\xc8\xad\xc0\xc7 \xb1\xe2\xbf\xf8", 8, TG_ALLY, EF_CURE, 0, 1, 1}, // 부활의 기원 / 정화의 기원
     {"\xc1\xa6\xbe\xd0\xc0\xc7 \xb1\xe2\xbf\xf8", 8, TG_ENEMY, EF_HIT, 80, 1, 1, 0, ST_GROGGY, 50}}, // 제압의 기원
    {{"\xb9\xdf\xb8\xf1 \xba\xa3\xb1\xe2", 8, TG_ENEMY, EF_HIT, 70, 0, 1, 0, ST_SLOW, 90}, {"\xc8\xd6\xb0\xa8\xb4\xc2 \xb0\xcb\xc7\xb3", 14, TG_ENEMIES, EF_HIT, 40, 0, 1, 0, ST_SLOW, 60}}, // 발목 베기 / 휘감는 검풍
    {{"\xb5\xb5\xb9\xdf", 6, TG_SELF, EF_TAUNT, 0, 0, 1, 0, 0, 0, 0, 3}, {"\xbc\xf6\xc8\xa3\xc0\xc7 \xc7\xd4\xbc\xba", 14, TG_ALLIES, EF_BUFF_DEF, 0, 0, 1, 0, 0, 0, 20, 3}, // 도발 / 수호의 함성
     {"\xb9\xdd\xb0\xdd \xc0\xda\xbc\xbc", 10, TG_SELF, EF_COUNTER, 0, 0, 1, 0, 0, 0, 0, 3}, {"\xb9\xe6\xc6\xd0 \xb0\xad\xc5\xb8", 10, TG_ENEMY, EF_HIT, 110, 0, 1, 0, ST_GROGGY, 50}}, // 반격 자세 / 방패 강타
    {{"\xc0\xfc\xc0\xc7 \xb0\xed\xbe\xe7", 14, TG_ALLIES, EF_BUFF_ATK, 0, 0, 1, 0, 0, 0, 20, 3}, {"\xbd\xc5\xbc\xd3\xc0\xc7 \xb3\xeb\xb7\xa1", 12, TG_ALLIES, EF_BUFF_SPD, 0, 0, 1, 0, 0, 0, 30, 3}}, // 전의 고양 / 신속의 노래
    {{"\xbd\xc3\xb0\xa3 \xc1\xf6\xbf\xac", 12, TG_ENEMY, EF_HIT, 130, 1, 1, 0, ST_SLOW, 70}, {"\xbd\xc3\xb0\xa3 \xbf\xd6\xb0\xee", 26, TG_ENEMIES, EF_HIT, 80, 1, 1, 0, ST_SLOW, 40}}, // 시간 지연 / 시간 왜곡
    {{"\xc8\xad\xbf\xb0\xb1\xb8", 12, TG_ENEMY, EF_HIT, 150, 1, 1, 0, ST_BURN, 35}, {"\xc8\xad\xbf\xb0 \xc6\xf8\xc7\xb3", 26, TG_ENEMIES, EF_HIT, 90, 1, 1, 0, ST_BURN, 20}}, // 화염구 / 화염 폭풍
    {{"\xb9\xf8\xb0\xb3", 12, TG_ENEMY, EF_HIT, 150, 1, 1, 0, ST_SHOCK, 35}, {"\xbf\xac\xbc\xe2 \xb9\xf8\xb0\xb3", 26, TG_ENEMIES, EF_HIT, 90, 1, 1, 0, ST_SHOCK, 20}}, // 번개 / 연쇄 번개
};
// 동료 스킬을 배우는 레벨(정한 값, 사용자 결정: 주인공 말고는 레벨이 오르면 스킬이 열린다). 순서는 위 표와 같다.
static const int MEMBER_UNLOCK[8][5] = {
    {1, 12}, {1, 16, 32, 8, 24}, {1, 12}, {1, 16, 24, 8}, {1, 12}, {1, 12}, {1, 12}, {1, 12},
};
static const struct { int status, chance; bool magic; } MEMBER_BASIC[8] = {
    {ST_SNOW, 10, false}, {ST_GROGGY, 10, true}, {ST_SLOW, 10, false}, {ST_GROGGY, 10, false},
    {0, 0, false}, {ST_SLOW, 10, true}, {ST_BURN, 10, true}, {ST_SHOCK, 10, true},
};

int turn_member_skills(int member, int level, tskill *basic, tskill *out, int max) {
    memset(basic, 0, sizeof *basic);
    if (member < 0 || member > 7) return 0;
    snprintf(basic->name, sizeof basic->name, "%s", "\xb0\xf8\xb0\xdd"); // 공격
    basic->target = TG_ENEMY;
    basic->effect = EF_HIT;
    basic->power = 100;
    basic->hits = 1;
    basic->magic = MEMBER_BASIC[member].magic;
    basic->status = MEMBER_BASIC[member].status;
    basic->chance = MEMBER_BASIC[member].chance;
    basic->fx = -1;
    int n = 0;
    for (int k = 0; k < 5 && n < max && MEMBER_SKILLS[member][k].name; k++) {
        const mdef *d = &MEMBER_SKILLS[member][k];
        tskill *s = &out[n++];
        memset(s, 0, sizeof *s);
        snprintf(s->name, sizeof s->name, "%s", d->name);
        s->mp = d->mp; s->target = d->target; s->effect = d->effect; s->power = d->power; s->magic = d->magic;
        s->hits = d->hits ? d->hits : 1; s->pierce = d->pierce; s->status = d->status; s->chance = d->chance;
        s->amount = d->amount; s->turns = d->turns; s->fx = -1;
        s->ranged = d->magic || d->target != TG_ENEMY;
        s->unlock = MEMBER_UNLOCK[member][k];
    }
    // 배우는 레벨 순으로 보인다
    for (int i = 1; i < n; i++)
        for (int k = i; k > 0 && out[k].unlock < out[k - 1].unlock; k--) { tskill t = out[k]; out[k] = out[k - 1]; out[k - 1] = t; }
    (void)level;
    return n;
}

const char *turn_status_name(int s) {
    static const char *names[ST_COUNT] = {"", "\xc1\xdf\xb5\xb6", "\xb0\xa8\xc0\xfc", "\xc8\xad\xbb\xf3", "\xb1\xd7\xb7\xce\xb1\xe2", "\xb4\xab\xbb\xe7\xb6\xf7", "\xbd\xc7\xb8\xed", "\xb9\xe6\xb1\xcd", "\xb5\xd0\xc8\xad"}; // 중독 / 감전 / 화상 / 그로기 / 눈사람 / 실명 / 방귀 / 둔화
    return s > 0 && s < ST_COUNT ? names[s] : "";
}

// ---- 허리주머니(0x128f30: +8 개수, +0x12 칸 수, +0x13 커서). 원본 사용 0x13b1d4와 같은 함수로 읽고 줄인다 ----

int turn_pouch_items(titem *out, int max) {
    cpu_t *c = &cpu;
    uint32_t p = CALL(FN_POUCH);
    if (!p) return 0;
    static uint32_t nb;
    if (!nb) nb = heap_alloc(0x48);
    int n = 0, slots = (int8_t)R8(p + POUCH_SLOTS);
    fprintf(stderr, "[turn] 허리주머니 %08x 칸 %d 개수 %d\n", p, slots, (int)R32(p + 8));
    for (int i = 0; i < slots && n < max; i++) {
        uint32_t it = CALL(FN_POUCH_ITEM, p, (uint32_t)i);
        if (!it) continue;
        titem *t = &out[n++];
        t->item = it;
        t->slot = i;
        t->kind = (int8_t)CALL(FN_ITEM_KIND, it);
        t->id = (int16_t)CALL(FN_ITEM_ID, it);
        t->count = (int8_t)CALL(FN_ITEM_COUNT, it);
        memset(c->m + nb, 0, 0x48);
        CALL(FN_ITEM_NAME, nb, (uint32_t)t->kind, (uint32_t)t->id);
        snprintf(t->name, sizeof t->name, "%s", gstr(nb + 0x25));
        fprintf(stderr, "[turn]   칸 %d 종류 %d id %d 수 %d\n", i, t->kind, t->id, t->count);
    }
    return n;
}

// 원본 0x13b1d4의 뒷부분: 하나 남았으면 칸에서 빼고, 아니면 하나 줄인다.
void turn_pouch_consume(const titem *t) {
    cpu_t *c = &cpu;
    uint32_t p = CALL(FN_POUCH);
    if ((int8_t)CALL(FN_ITEM_COUNT, t->item) - 1 < 1) CALL(FN_POUCH_REMOVE, p, t->item, (uint32_t)t->slot);
    else CALL(FN_ITEM_ADD_COUNT, t->item, (uint32_t)-1);
}

// 전투에서 쓸 수 있는 아이템(ITM/DAT 번호 = 종류). 효과 수치는 원본 사용 함수(0x13a688)의 기본값(건강함 옵션 0일 때).
bool turn_item_effect(int kind, int *effect, int *hp, int *mp, int *status, int *turns) {
    *hp = *mp = *status = *turns = 0;
    switch (kind) {
    case 20: *effect = EF_HEAL; *hp = 200; return true;
    case 21: *effect = EF_HEAL; *hp = 500; return true;
    case 22: *effect = EF_HEAL; *hp = 1000; return true;
    case 23: *effect = EF_MP; *mp = 100; return true;
    case 24: *effect = EF_MP; *mp = 200; return true;
    case 25: *effect = EF_MP; *mp = 300; return true;
    case 26: *effect = EF_HEAL; *hp = 800; *mp = 800; return true;
    case 27: case 28: *effect = EF_MPLOCK; *turns = kind == 27 ? 3 : 4; return true; // 각성제: 원작은 그동안 MP가 줄지 않는다
    case 29: *effect = EF_BUFF_ATK; *hp = 20; *turns = 5; return true;              // 공격강화제: 공격 배율 +20%p
    case 30: *effect = EF_BUFF_DEF; *hp = 20; *turns = 5; return true;              // 방어강화제: 받는 피해 -20%
    case 31: *effect = EF_CURE; *status = ST_SNOW; return true;                     // 핫팩
    case 32: *effect = EF_CURE; *status = ST_POISON; return true;                   // 해독제
    case 33: *effect = EF_CURE; *status = ST_BURN; return true;                     // 소화기
    case 34: *effect = EF_CURE; *status = ST_BLIND; return true;                    // 안약
    case 35: *effect = EF_CURE; *status = ST_SLOW; return true;                     // 런닝드링크
    }
    return false;
}
