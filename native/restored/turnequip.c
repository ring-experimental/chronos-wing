// 턴제 모드의 동료 장비와 능력치(사용자 결정 2026-11-09: 동료도 주인공과 같은 장비 칸, 능력치는 자기 장비 + 레벨, 무기는 역할별 한 종류).
// 장착 표시는 원본처럼 아이템 +0x174(칸 번호, -1 없음)에 둔다. 동료 칸은 16 + 6×동료 + 칸(16~63)이라 원작 주인공(0~5)·선원장비(6~15)와 겹치지 않고,
// 가방·창고 저장(0x137b48)에 그대로 남는다. 장착 중인 아이템은 원본이 제작·판매 목록에서 빼고 가방에 E를 그린다.
// 원본 장착 0x1205b8(칸, 아이템)은 칸 > 5를 선원장비(동료 표 +0x40)로 쓰므로 16 이상은 감싸서 직접 처리한다(창고 메뉴 '해제'가 이 경로).
// 근거는 work/turn-plan.md "동료 장비".
#include "restored.h"
#include "turn.h"
#include <stdio.h>
#include <stdlib.h>

void orig_f_205b8(cpu_t *c);

enum {
    FN_BAG = 0x128f0c, BAG_LIST = 0x10, GOT_STORAGE = 0x980, STORAGE_EQUIP_LIST = 0x14,
    NODE_NEXT = 4, NODE_ITEM = 0x10, ITEM_SLOT = 0x174,
    FN_ITEM_KIND = 0x138410, FN_ITEM_NAME_PTR = 0x13a28c,
    FN_W_BASE = 0x13a4a8, FN_W_PHYS = 0x13a4dc, FN_W_MAGIC = 0x13a4f0, FN_REFINE = 0x13a5c4,
    FN_DEF_PHYS = 0x13a518, FN_DEF_MAGIC = 0x13a530,
    MEMBERS = 0x1a9e58, MEMBER_SIZE = 0x74, M_LEVEL = 0x35,
    SLOT_BASE = 16, SLOTS = 6, MEMBER_COUNT = 8,
};

// 동료 무기 종류(원작 무기: 8 쌍수 격투, 9 양손 검, 10 마력 정령). 엔지·후거스 쌍수, 리키 양손(검을 든 그림), 나머지 마력.
static const int WEAPON_KIND[MEMBER_COUNT] = {8, 10, 9, 8, 10, 10, 10, 10};
// 역할 비율(정한 값): 공격 %, HP %. 순서 0 엔지 1 레이 2 리키 3 후거스 4 보우머 5 쟝 6 펭 7 팡
static const struct { int atk, hp; } RATE[MEMBER_COUNT] = {
    {100, 95}, {60, 85}, {80, 90}, {75, 130}, {65, 90}, {95, 80}, {95, 80}, {95, 80},
};
static const bool MAGIC[MEMBER_COUNT] = {false, true, false, false, true, true, true, true};

int turn_equip_slot(int member, int s) { return SLOT_BASE + SLOTS * member + s; }

static uint32_t first_node(uint32_t list) { cpu_t *c = &cpu; return R32(list); }

// 가방과 창고 장비 탭의 아이템을 차례로 돌려준다(끝이면 0).
typedef struct { int list; uint32_t node; } walker;
static uint32_t next_item(walker *w) {
    cpu_t *c = &cpu;
    for (;;) {
        if (!w->node) {
            if (w->list == 0) w->node = first_node(CALL(FN_BAG) + BAG_LIST);
            else if (w->list == 1) w->node = first_node(got(GOT_STORAGE) + STORAGE_EQUIP_LIST);
            else return 0;
            w->list++;
            if (!w->node) continue;
        } else {
            w->node = R32(w->node + NODE_NEXT);
            if (!w->node) continue;
        }
        uint32_t it = R32(w->node + NODE_ITEM);
        if (it) return it;
    }
}

uint32_t turn_equip_item(int member, int s) {
    cpu_t *c = &cpu;
    walker w = {0, 0};
    int slot = turn_equip_slot(member, s);
    for (uint32_t it; (it = next_item(&w));) if ((int8_t)R8(it + ITEM_SLOT) == slot) return it;
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

// 칸에 넣을 수 있는 아이템(장착 중이 아닌 것). 반환은 개수.
int turn_equip_candidates(int member, int s, uint32_t *out, int max) {
    cpu_t *c = &cpu;
    walker w = {0, 0};
    int n = 0;
    for (uint32_t it; (it = next_item(&w)) && n < max;)
        if ((int8_t)R8(it + ITEM_SLOT) < 0 && turn_equip_fits(member, s, it)) out[n++] = it;
    return n;
}

void turn_equip_set(int member, int s, uint32_t it) {
    cpu_t *c = &cpu;
    uint32_t old = turn_equip_item(member, s);
    if (old) W8(old + ITEM_SLOT, 0xff);
    if (it) W8(it + ITEM_SLOT, (uint8_t)turn_equip_slot(member, s));
    fprintf(stderr, "[turn] 장비: 동료 %d 칸 %d %08x → %08x\n", member, s, old, it);
}

// 원본 장착(칸, 아이템). 칸 16 이상은 동료 장비라 직접 처리한다(아이템 0이면 해제).
void f_205b8(cpu_t *c) {
    int slot = (int8_t)c->r[0];
    uint32_t it = c->r[1];
    if (slot >= SLOT_BASE && slot < SLOT_BASE + SLOTS * MEMBER_COUNT) {
        turn_equip_set((slot - SLOT_BASE) / SLOTS, (slot - SLOT_BASE) % SLOTS, it);
        return;
    }
    orig_f_205b8(c);
}

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
