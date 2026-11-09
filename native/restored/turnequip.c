// 턴제 모드의 동료 장비와 능력치(사용자 결정 2026-11-09: 동료도 주인공과 같은 6칸, 능력치는 자기 장비 + 레벨, 무기는 역할별 한 종류, 기본 장비를 준다).
// 동료가 입은 장비는 가방·창고 칸을 쓰지 않도록 자체 목록(원본 목록 구조, 0x13f10c로 넣는다)에 둔다. 칸은 아이템 +0x174 = 16 + 6×동료 + 칸.
// 벗기면 가방(0x13f10c)으로, 차 있으면 창고 장비 탭(30칸, 0x13cfc0)으로 돌아간다. 목록에서 뺄 때 원본 0x13f0b8은 아이템을 풀어 버리므로 연결만 직접 끊는다. 입히는 후보는 가방과 창고의 장착하지 않은 장비.
// 저장은 게임 저장 마지막 단계(0x143404(8))에 원본 목록 저장(0x1387e4)으로 TURNGEAR에, 읽기는 세이브 읽기 끝(turnparty.c의 0x1276b4)에서.
// 기본 장비: 원작 장비 제작 0x16dfc8(스킨 + 스톤)으로 등급 0 장비를 만든다(무기·투구·갑옷), 장갑·신발은 원작 수련용(13/0, 14/0).
// 근거는 work/turn-plan.md "동료 장비".
#include "restored.h"
#include "turn.h"
#include <stdio.h>
#include <stdlib.h>

void orig_f_43404(cpu_t *c);

enum {
    FN_BAG = 0x128f0c, BAG_LIST = 0x10, BAG_MAX = 36, GOT_STORAGE = 0x980, STORAGE_EQUIP_LIST = 0x14, STORAGE_MAX = 30,
    LIST_FIRST = 0, LIST_COUNT = 8, NODE_NEXT = 4, NODE_ITEM = 0x10, ITEM_SLOT = 0x174, ITEM_WHERE = 0x176,
    WHERE_BAG = 2, WHERE_STORAGE = 4,
    FN_LIST_ADD = 0x13f10c, FN_STORAGE_COUNT = 0x13ca98, FN_STORAGE_ADD = 0x13cfc0,
    FN_LIST_SAVE = 0x1387e4, FN_LIST_LOAD = 0x13881c, FN_READ_INT = 0x183cfc,
    FN_NEW_ITEM = 0x137494, FN_FREE_ITEM = 0x1372b8, FN_CRAFT = 0x16dfc8, GOT_CRAFT_PAIR = 0xa24,
    FN_ITEM_KIND = 0x138410, FN_ITEM_NAME_PTR = 0x13a28c,
    FN_W_BASE = 0x13a4a8, FN_W_PHYS = 0x13a4dc, FN_W_MAGIC = 0x13a4f0, FN_REFINE = 0x13a5c4,
    FN_DEF_PHYS = 0x13a518, FN_DEF_MAGIC = 0x13a530,
    MEMBERS = 0x1a9e58, MEMBER_SIZE = 0x74, M_LEVEL = 0x35,
    SLOT_BASE = 16, SLOTS = 6, MEMBER_COUNT = 8, GEAR_FILE_SIZE = 9000, SAVE_LAST_STEP = 8,
};

// 동료 무기 종류(원작 무기: 8 쌍수 격투, 9 양손 검, 10 마력 정령). 엔지·후거스 쌍수, 리키 양손(검을 든 그림), 나머지 마력.
static const int WEAPON_KIND[MEMBER_COUNT] = {8, 10, 9, 8, 10, 10, 10, 10};
// 역할 비율(정한 값): 공격 %, HP %. 순서 0 엔지 1 레이 2 리키 3 후거스 4 보우머 5 쟝 6 펭 7 팡
static const struct { int atk, hp; } RATE[MEMBER_COUNT] = {
    {100, 95}, {60, 85}, {80, 90}, {75, 130}, {65, 90}, {95, 80}, {95, 80}, {95, 80},
};
static const bool MAGIC[MEMBER_COUNT] = {false, true, false, false, true, true, true, true};

static uint32_t gear; // 자체 목록(+0 처음, +4 끝, +8 개수)

static uint32_t gear_list(void) {
    cpu_t *c = &cpu;
    if (!gear) { gear = heap_alloc(16); memset(c->m + gear, 0, 16); }
    return gear;
}

int turn_equip_slot(int member, int s) { return SLOT_BASE + SLOTS * member + s; }

// 목록에서 연결만 끊는다. 원본 0x13f0b8은 끊은 뒤 아이템을 풀어 버린다(0x146fc8). 노드가 곧 아이템이다(+0 앞, +4 뒤, 목록 +0 처음, +4 끝, +8 개수).
static void unlink_item(uint32_t list, uint32_t node) {
    cpu_t *c = &cpu;
    uint32_t prev = R32(node), next = R32(node + 4);
    if (prev) W32(prev + 4, next); else W32(list, next);
    if (next) W32(next, prev); else W32(list + 4, prev);
    W32(node, 0);
    W32(node + 4, 0);
    W32(list + LIST_COUNT, R32(list + LIST_COUNT) - 1);
}

static uint32_t find_in(uint32_t list, uint32_t it, uint32_t *node_out) {
    cpu_t *c = &cpu;
    for (uint32_t n = R32(list + LIST_FIRST); n; n = R32(n + NODE_NEXT))
        if (R32(n + NODE_ITEM) == it) { if (node_out) *node_out = n; return list; }
    return 0;
}

uint32_t turn_equip_item(int member, int s) {
    cpu_t *c = &cpu;
    int slot = turn_equip_slot(member, s);
    for (uint32_t n = R32(gear_list() + LIST_FIRST); n; n = R32(n + NODE_NEXT)) {
        uint32_t it = R32(n + NODE_ITEM);
        if (it && (int8_t)R8(it + ITEM_SLOT) == slot) return it;
    }
    return 0;
}

int turn_item_kind(uint32_t it) { cpu_t *c = &cpu; return it ? (int8_t)CALL(FN_ITEM_KIND, it) : -1; }

const char *turn_item_name(uint32_t it) {
    cpu_t *c = &cpu;
    if (!it) return "";
    uint32_t p = CALL(FN_ITEM_NAME_PTR, it);
    return p ? gstr(p) : "";
}

// 칸에 맞는 종류(원작 고르기 0x11fa08): 0 무기(역할별), 1 투구 11, 2 갑옷 12, 3 장갑 13, 4 신발 14, 5 악세서리 15~18
bool turn_equip_fits(int member, int s, uint32_t it) {
    int k = turn_item_kind(it);
    switch (s) {
    case 0: return k == WEAPON_KIND[member];
    case 1: return k == 11;
    case 2: return k == 12;
    case 3: return k == 13;
    case 4: return k == 14;
    case 5: return k >= 15 && k <= 18;
    }
    return false;
}

static uint32_t bag_list(void) { cpu_t *c = &cpu; return CALL(FN_BAG) + BAG_LIST; }
static uint32_t storage_list(void) { cpu_t *c = &cpu; return got(GOT_STORAGE) + STORAGE_EQUIP_LIST; }

// 칸에 넣을 수 있는 아이템(가방과 창고 장비 탭, 장착 중이 아닌 것). 반환은 개수.
int turn_equip_candidates(int member, int s, uint32_t *out, int max) {
    cpu_t *c = &cpu;
    int n = 0;
    uint32_t lists[2] = {storage_list(), bag_list()};
    for (int l = 0; l < 2; l++)
        for (uint32_t nd = R32(lists[l] + LIST_FIRST); nd && n < max; nd = R32(nd + NODE_NEXT)) {
            uint32_t it = R32(nd + NODE_ITEM);
            if (it && (int8_t)R8(it + ITEM_SLOT) < 0 && turn_equip_fits(member, s, it)) out[n++] = it;
        }
    return n;
}

// 벗긴 장비를 가방(차 있으면 창고 장비 탭)으로 돌려보낸다. 둘 다 차 있으면 거짓.
static bool put_back(uint32_t it) {
    cpu_t *c = &cpu;
    uint32_t node;
    if (find_in(gear_list(), it, &node)) unlink_item(gear_list(), node);
    W8(it + ITEM_SLOT, 0xff);
    if ((int)R32(bag_list() + LIST_COUNT) < BAG_MAX) { W8(it + ITEM_WHERE, WHERE_BAG); CALL(FN_LIST_ADD, bag_list(), it); return true; }
    if ((int8_t)CALL(FN_STORAGE_COUNT, 0) < STORAGE_MAX) { W8(it + ITEM_WHERE, WHERE_STORAGE); CALL(FN_STORAGE_ADD, it, 0); return true; }
    CALL(FN_LIST_ADD, gear_list(), it); // 돌려놓을 곳이 없으면 그대로 입힌다
    return false;
}

static bool room_for_one(void) {
    cpu_t *c = &cpu;
    return (int8_t)CALL(FN_STORAGE_COUNT, 0) < STORAGE_MAX || (int)R32(bag_list() + LIST_COUNT) < BAG_MAX;
}

// 입히기(it 0이면 벗기기). 실패하면 이유를 돌려준다(EUC-KR).
const char *turn_equip_set(int member, int s, uint32_t it) {
    cpu_t *c = &cpu;
    uint32_t old = turn_equip_item(member, s);
    if (old == it) return NULL;
    if (old && !it && !room_for_one()) return "\xc3\xa2\xb0\xed\xbf\xcd \xb0\xa1\xb9\xe6\xc0\xcc \xb0\xa1\xb5\xe6 \xc3\xa1\xb4\xd9"; // 창고와 가방이 가득 찼다
    if (it) { // 가방이나 창고에서 꺼낸다(그 자리가 비므로 바꿔 입을 때 돌려놓을 곳이 생긴다)
        uint32_t node, l = find_in(storage_list(), it, &node);
        if (!l) l = find_in(bag_list(), it, &node);
        if (!l) return "\xc0\xe5\xba\xf1\xb8\xa6 \xc3\xa3\xc1\xf6 \xb8\xf8\xc7\xdf\xb4\xd9"; // 장비를 찾지 못했다
        unlink_item(l, node);
    }
    if (old) put_back(old);
    if (it) {
        W8(it + ITEM_SLOT, (uint8_t)turn_equip_slot(member, s));
        CALL(FN_LIST_ADD, gear_list(), it);
    }
    fprintf(stderr, "[turn] 장비: 동료 %d 칸 %d %08x → %08x\n", member, s, old, it);
    return NULL;
}

// ---- 기본 장비 ----

// 원작 제작과 같은 순서(0x13dd94): 스킨(종류-5, 0) + 스톤으로 0x16dfc8(1 = 창고에 넣지 않음). 무작위 옵션은 지운다.
static uint32_t craft(int kind, int stone_kind, int stone_id) {
    cpu_t *c = &cpu;
    uint32_t pair = got(GOT_CRAFT_PAIR);
    uint32_t skin = CALL(FN_NEW_ITEM, 0xffffffffu, (uint32_t)(kind - 5), 0, 1);
    uint32_t stone = CALL(FN_NEW_ITEM, 0xffffffffu, (uint32_t)stone_kind, (uint32_t)stone_id, 1);
    W32(pair, skin);
    W32(pair + 4, stone);
    uint32_t it = CALL(FN_CRAFT, 1);
    CALL(FN_FREE_ITEM, skin);
    CALL(FN_FREE_ITEM, stone);
    W32(pair, 0);
    W32(pair + 4, 0);
    if (it) {
        W8(it + 0x177, 0);
        for (int k = 0; k < 4; k++) W8(it + 0x178 + k, 0xff);
        W8(it + 0x17d, 0);
    }
    return it;
}

static void wear(int member, int s, uint32_t it) {
    cpu_t *c = &cpu;
    if (!it) return;
    W8(it + ITEM_SLOT, (uint8_t)turn_equip_slot(member, s));
    CALL(FN_LIST_ADD, gear_list(), it);
}

// 등급 0 기본 장비(정한 값): 쌍수·양손은 파워젬(1,0), 마력은 무골(2,14), 투구·갑옷은 기본스톤(0,0), 장갑·신발은 수련용(13/0, 14/0). 악세서리는 없다.
void turn_equip_give_basic(int member) {
    cpu_t *c = &cpu;
    for (int s = 0; s < 5; s++) if (turn_equip_item(member, s)) return; // 이미 입은 것이 있으면 주지 않는다
    int wk = WEAPON_KIND[member];
    wear(member, 0, wk == 10 ? craft(10, 2, 14) : craft(wk, 1, 0));
    wear(member, 1, craft(11, 0, 0));
    wear(member, 2, craft(12, 0, 0));
    wear(member, 3, CALL(FN_NEW_ITEM, 2, 13, 0, 1));
    wear(member, 4, CALL(FN_NEW_ITEM, 2, 14, 0, 1));
    fprintf(stderr, "[turn] 장비: 동료 %d 기본 장비\n", member);
}

// ---- 저장과 읽기 ----

static void gear_path(char *out, size_t cap) { snprintf(out, cap, "%s/TURNGEAR", save_dir); }

void turn_equip_reset(void) {
    cpu_t *c = &cpu;
    memset(c->m + gear_list(), 0, 16); // 새 게임·읽기 전: 이전 목록은 버린다(아이템 풀은 게임 초기화가 비운다)
}

void turn_equip_load(void) {
    cpu_t *c = &cpu;
    turn_equip_reset();
    char p[1100];
    gear_path(p, sizeof p);
    FILE *f = fopen(p, "rb");
    if (!f) return;
    uint32_t buf = heap_alloc(GEAR_FILE_SIZE);
    size_t n = fread(c->m + buf, 1, GEAR_FILE_SIZE, f);
    fclose(f);
    if (n == GEAR_FILE_SIZE && (int)CALL(FN_READ_INT, buf, 0) > 0) CALL(FN_LIST_LOAD, gear_list(), buf, 0);
    heap_free(buf);
    fprintf(stderr, "[turn] 장비: 읽음 %d개\n", (int)R32(gear_list() + LIST_COUNT));
}

static void gear_save(void) {
    cpu_t *c = &cpu;
    uint32_t buf = heap_alloc(GEAR_FILE_SIZE);
    memset(c->m + buf, 0xff, GEAR_FILE_SIZE);
    CALL(FN_LIST_SAVE, gear_list(), buf, 0);
    char p[1100];
    gear_path(p, sizeof p);
    FILE *f = fopen(p, "wb");
    if (f) { fwrite(c->m + buf, 1, GEAR_FILE_SIZE, f); fclose(f); }
    heap_free(buf);
}

// 게임 저장 단계(0x143404). 마지막 단계(8, 동료 저장)에서 동료 장비와 파티도 저장한다.
void f_43404(cpu_t *c) {
    int step = (uint8_t)c->r[0];
    orig_f_43404(c);
    if (step == SAVE_LAST_STEP && turn_mode_on()) { gear_save(); turn_party_save(); fprintf(stderr, "[turn] 장비·파티 저장\n"); }
}

// ---- 능력치 ----

// 동료 능력치(턴제 전투와 스테이터스 화면). 공격은 원작 주인공 식(0x101ff4)과 같은 꼴: (기본 + 물리/마도 보정) × ((제련+20)/20),
// 레벨이 오를수록 (100+레벨)%로 늘고 역할 비율을 곱한다. 무기가 없으면 3 + 레벨. 방어는 투구·갑옷·장갑·신발 합(0x101ef4와 같이 갑옷 제련×2).
// HP는 (400 + 레벨×12) × 역할 %, MP는 60 + 레벨×4(정한 값).
void turn_member_stats(int i, member_stats *st) {
    cpu_t *c = &cpu;
    memset(st, 0, sizeof *st);
    int lv = R8(MEMBERS + (uint32_t)i * MEMBER_SIZE + M_LEVEL);
    if (lv < 1) lv = 1;
    st->level = lv;
    st->magic = MAGIC[i];
    uint32_t w = turn_equip_item(i, 0);
    int atk;
    if (w) {
        int base = (int)CALL(FN_W_BASE, w) + (int)(MAGIC[i] ? CALL(FN_W_MAGIC, w) : CALL(FN_W_PHYS, w));
        atk = base * (((int)CALL(FN_REFINE, w) + 20) / 20);
    } else atk = 3 + lv;
    st->atk = atk * (100 + lv) / 100 * RATE[i].atk / 100;
    for (int s = 1; s <= 4; s++) {
        uint32_t a = turn_equip_item(i, s);
        if (!a) continue;
        st->pdef += (int16_t)CALL(FN_DEF_PHYS, a);
        st->mdef += (int16_t)CALL(FN_DEF_MAGIC, a);
        if (s == 2) { int r = (int)CALL(FN_REFINE, a) * 2; st->pdef += r; st->mdef += r; }
    }
    st->hp = (400 + lv * 12) * RATE[i].hp / 100;
    st->mp = 60 + lv * 4;
}
