// 내장 서버: 1(20~27) 용병과 방어진. 근거는 work/net-offline.md의 용병과 방어진 절.
#include "host.h"
#include "localnet.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

enum {
    FN_LOAD_SEALED = 0x184b54, // 해시 확인 후 복호화해서 읽기. 핸들을 돌려준다
    FN_LOAD_PLAIN = 0x183f44,  // (경로, 크기 포인터). 핸들을 돌려준다
    FN_FREE_HANDLE = 0x183fd8, // (핸들 변수 포인터)
    MERC_FIRST = 0xf8, MERC_BOSS_LAST = 0x103, MERC_LAST = 0x111, // /DAT/ML
    CONTRACT_DAYS = 30,        // 정한 값
    C3_DMG = 4, C3_SIZE = 16,  // EMD/C3 항목 0x14f44c
    EM_RECORD_TAIL = 0x30 + 5 * 10, // 이름 뒤 레코드 길이 0x14f704
};

static const char MSG_NO_UNIT[] = "\xb4\xeb\xbb\xf3\xc0\xbb\x20\xc3\xa3\xc0\xbb\x20\xbc\xf6\x20\xbe\xf8\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 대상을 찾을 수 없습니다.
static const char MSG_NO_NP[] = "\x4e\x50\xb0\xa1\x20\xba\xce\xc1\xb7\xc7\xd5\xb4\xcf\xb4\xd9\x2e"; // NP가 부족합니다.
static const char MSG_NOT_MERC[] = "\xb0\xe8\xbe\xe0\xc7\xd2\x20\xbc\xf6\x20\xbe\xf8\xb4\xc2\x20\xb4\xeb\xbb\xf3\xc0\xd4\xb4\xcf\xb4\xd9\x2e"; // 계약할 수 없는 대상입니다.
static const char MSG_HAVE[] = "\xc0\xcc\xb9\xcc\x20\xb0\xe8\xbe\xe0\xc7\xd1\x20\xbf\xeb\xba\xb4\xc0\xd4\xb4\xcf\xb4\xd9\x2e"; // 이미 계약한 용병입니다.
static const char MSG_CONTRACT[] = "\xbf\xeb\xba\xb4\xb0\xfa\x20\xb0\xe8\xbe\xe0\xc7\xdf\xbd\xc0\xb4\xcf\xb4\xd9\x2e\x20\xb9\xe6\xbe\xee\xc1\xf8\xb0\xfc\xb8\xae\xbf\xa1\xbc\xad\x20\xb9\xe8\xc4\xa1\xc7\xcf\xbc\xbc\xbf\xe4\x2e"; // 용병과 계약했습니다. 방어진관리에서 배치하세요.
static const char MSG_RESTED[] = "\xc7\xc7\xb7\xce\xb5\xb5\xb8\xa6\x20\xc8\xb8\xba\xb9\xc7\xdf\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 피로도를 회복했습니다.
static const char MSG_NOT_TIRED[] = "\xc8\xb8\xba\xb9\xc7\xd2\x20\xc7\xc7\xb7\xce\xb5\xb5\xb0\xa1\x20\xbe\xf8\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 회복할 피로도가 없습니다.
static const char MSG_NOT_JOINED[] = "\xbe\xc6\xc1\xf7\x20\xc7\xd5\xb7\xf9\xc7\xcf\xc1\xf6\x20\xbe\xca\xc0\xba\x20\xb5\xbf\xb7\xe1\xc0\xd4\xb4\xcf\xb4\xd9\x2e"; // 아직 합류하지 않은 동료입니다.
static const char MSG_SAVED[] = "\xb9\xe8\xc4\xa1\xc1\xa4\xba\xb8\xb8\xa6\x20\xc0\xfa\xc0\xe5\xc7\xdf\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 배치정보를 저장했습니다.
static const char MSG_EXTENDED[] = "\xb0\xe8\xbe\xe0\xb1\xe2\xb0\xa3\xc0\xbb\x20\xbf\xac\xc0\xe5\xc7\xdf\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 계약기간을 연장했습니다.
static const char FMT_ASK[] = "\x25\x64\xc0\xcf\x20\xbf\xac\xc0\xe5\xbf\xa1\x20\x25\x64\x20\x4e\x50\xb0\xa1\x20\xc7\xca\xbf\xe4\xc7\xd5\xb4\xcf\xb4\xd9\x2e\x20\xbf\xac\xc0\xe5\xc7\xcf\xbd\xc3\xb0\xda\xbd\xc0\xb4\xcf\xb1\xee\x3f"; // %d일 연장에 %d NP가 필요합니다. 연장하시겠습니까?

// EMD/M 한 레코드에서 쓰는 값. 동료(49~57)와 용병(0xf8~0x111)의 수치는 절대값이 아니라 비율이다.
typedef struct {
    bool ok;
    char name[27];
    int8_t level;
    int hp_pct, pdef_pct, mdef_pct, crit, grade;
    int attack_pct[3], attack_count, attack_rep; // attack_rep: 0이 아닌 첫 비율
} em_rec;

static int le16(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }
static int le32(const uint8_t *p) { return (int32_t)(p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24); }

static void load_attacks(em_rec *r, int c3) {
    uint32_t mem = heap_alloc(0x48);
    snprintf(gstr(mem), 32, "/EMD/C3/%03d", c3);
    uint32_t a[2] = {mem, mem + 0x40};
    uint32_t h = guest_call(FN_LOAD_PLAIN, a, 2);
    if (h) {
        const uint8_t *d = cpu.m + handle_data(h);
        int n = le16(d);
        for (int i = 0; i < n; i++) {
            int pct = le32(d + 2 + i * C3_SIZE + C3_DMG);
            if (i < 3) r->attack_pct[i] = pct;
            if (pct && !r->attack_rep) r->attack_rep = pct;
        }
        r->attack_count = n < 1 ? 1 : n > 3 ? 3 : n;
        W32_(&cpu, mem + 0x44, h);
        uint32_t f[1] = {mem + 0x44};
        guest_call(FN_FREE_HANDLE, f, 1);
    }
    heap_free(mem);
}

// id/10 번 파일의 id%10 번째 레코드(0x14eed8). 파일을 읽을 수 없으면 ok가 false다.
static const em_rec *em_load(int id) {
    static em_rec cache[MERC_LAST + 1];
    if (id < 0 || id > MERC_LAST) return NULL;
    em_rec *r = &cache[id];
    if (r->ok) return r;
    uint32_t mem = heap_alloc(0x48);
    snprintf(gstr(mem), 32, "/EMD/M/%03d", id / 10);
    uint32_t a[1] = {mem};
    uint32_t h = guest_call(FN_LOAD_SEALED, a, 1);
    int c3 = -1;
    if (h) {
        const uint8_t *d = cpu.m + handle_data(h);
        size_t at = 2;
        for (int i = 0; i < id % 10; i++) at += 1u + (uint8_t)d[at] + EM_RECORD_TAIL;
        int len = d[at];
        const uint8_t *b = d + at + 1 + len;
        memcpy(r->name, d + at + 1, len < 26 ? len : 26);
        r->level = (int8_t)b[0x1c];
        r->hp_pct = le32(b + 0x1e);
        r->pdef_pct = le16(b + 0x22);
        r->mdef_pct = le16(b + 0x24);
        r->crit = b[0x26];
        r->grade = b[0x27];
        c3 = le16(b + 4 + 9 * 2); // 0x10c부터 12칸 중 +0x11e. 0x14f170의 C3 번호
        r->ok = true;
        W32_(&cpu, mem + 0x44, h);
        uint32_t f[1] = {mem + 0x44};
        guest_call(FN_FREE_HANDLE, f, 1);
    } else {
        fprintf(stderr, "[net] EMD/M/%03d 읽기 실패\n", id / 10);
    }
    heap_free(mem);
    if (r->ok && c3 >= 0) load_attacks(r, c3);
    return r->ok ? r : NULL;
}

// 기준 곡선: 비율 100%일 때의 값. 클라이언트에 공식이 없어 EMD/M의 일반 몬스터(레벨별 중앙값)에 직선을 맞췄다. 정한 값이다.
static int base_hp(int l) { int v = 52 * l - 266; return v < 200 ? 200 : v; }
static int base_pdef(int l) { return l * 3 / 5 + 18; }
static int base_mdef(int l) { return l * 12 / 25 + 10; }
static int base_attack(int l) { return l * 25 / 4 + 32; }

void ln_unit_stats(ln_member *m) {
    const em_rec *r = em_load(m->id);
    int l = m->level < 1 ? 1 : m->level;
    if (!r) {
        m->hp = base_hp(l);
        m->attack = base_attack(l);
        m->attack_count = 1;
        m->pdef = (int16_t)base_pdef(l);
        m->mdef = (int16_t)base_mdef(l);
        return;
    }
    m->hp = base_hp(l) * r->hp_pct / 100 + m->bonus_hp;
    if (m->hp < 1) m->hp = 1;
    m->attack_count = (uint8_t)(r->attack_count < 1 ? 1 : r->attack_count);
    // 첫 칸이 0이면 화면에 공격력 0으로 나온다. 대표 비율을 넣는다. 해당 행동 공격력이 올라가는 것은 감수한다.
    int first = r->attack_pct[0] ? r->attack_pct[0] : r->attack_rep;
    m->attack = base_attack(l) * first / 100 + m->bonus_attack;
    m->attack_more[0] = base_attack(l) * r->attack_pct[1] / 100;
    m->attack_more[1] = base_attack(l) * r->attack_pct[2] / 100;
    m->pdef = (int16_t)(base_pdef(l) * r->pdef_pct / 100 + m->bonus_pdef);
    m->mdef = (int16_t)(base_mdef(l) * r->mdef_pct / 100 + m->bonus_mdef);
    m->crit = (int8_t)(r->crit + m->bonus_crit);
    m->grade = (uint8_t)r->grade;
    m->recover_cost = (int16_t)(l * 2); // 정한 값
}

static ln_member mercs[LN_MERC_MAX];
static int merc_count = -1;

ln_member *ln_mercs(int *n) {
    if (merc_count < 0) {
        long got = ln_load("mercs", mercs, sizeof mercs);
        merc_count = got > 0 ? (int)(got / (long)sizeof(ln_member)) : 0;
    }
    if (n) *n = merc_count;
    return mercs;
}

void ln_mercs_save(void) {
    ln_mercs(NULL);
    if (!ln_save("mercs", mercs, (size_t)merc_count * sizeof(ln_member))) fprintf(stderr, "[net] 용병 저장 실패\n");
}

ln_member *ln_unit_find(int64_t uid) {
    int n;
    ln_member *list = uid < 0 ? ln_mercs(&n) : ln_members(&n);
    for (int i = 0; i < n; i++)
        if (list[i].uid == uid) return &list[i];
    return NULL;
}

static void unit_save(const ln_member *m) {
    if (m->uid < 0) ln_mercs_save();
    else ln_members_save();
}

static bool is_placed(const ln_member *m) { return m->floor >= 0 && m->room >= 0 && m->slot >= 0; }
static bool in_service(const ln_member *m) { return is_placed(m) && m->fatigue < 100 && (m->uid > 0 || m->expire > (int64_t)time(NULL)); }

int ln_defense(const ln_member **out, int cap) {
    int total = 0, n;
    const ln_member *list = ln_members(&n);
    for (int i = 0; i < n && total < cap; i++)
        if (in_service(&list[i])) out[total++] = &list[i];
    list = ln_mercs(&n);
    for (int i = 0; i < n && total < cap; i++)
        if (in_service(&list[i])) out[total++] = &list[i];
    return total;
}

void ln_write_defense(void) {
    const ln_member *d[LN_DEFENSE_MAX];
    int n = ln_defense(d, LN_DEFENSE_MAX);
    ln_w8((uint8_t)n);
    for (int i = 0; i < n; i++) ln_write_unit(d[i]);
}

bool ln_unit_fatigue_add(int64_t uid, int delta) {
    ln_member *m = ln_unit_find(uid);
    if (!m) return false;
    int v = m->fatigue + delta;
    m->fatigue = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
    unit_save(m);
    return true;
}

static int64_t read_uid(ln_req *q) {
    uint64_t hi = ln_u32(q);
    return (int64_t)(hi << 32 | ln_u32(q));
}

static bool is_merc_id(int id) { return id >= MERC_FIRST && id <= MERC_LAST; }
// 계약과 연장 비용. 클라이언트에 없다. 정한 값이다.
static int merc_cost(int id, int level) { return level * (id <= MERC_BOSS_LAST ? 30 : 10); }

// 1.20: 값을 그대로 돌려준다. 0이면 동료, 받은 쪽이 다시 1을 요청하고 1이면 용병 뒤 화면 7로 간다(0x152040).
static void list_units(ln_req *q) {
    uint8_t which = ln_u8(q);
    ln_begin(1, 20, 0);
    ln_w8(which);
    if (which == 0) {
        ln_write_members();
    } else {
        int n;
        const ln_member *list = ln_mercs(&n);
        ln_w16((uint16_t)n);
        for (int i = 0; i < n; i++) ln_write_unit(&list[i]);
    }
    ln_send();
}

// 1.21: 맵의 용병 NPC에서 계약. uid는 -id다(0x15162c). 응답은 안내문뿐이고 게임이 OPT 서버를 2로 저장한다(0x1522a8).
static void contract(ln_req *q) {
    int64_t uid = read_uid(q);
    int id = (int)-uid;
    const em_rec *r = is_merc_id(id) ? em_load(id) : NULL;
    int n;
    ln_member *list = ln_mercs(&n);
    if (!r || n >= LN_MERC_MAX) { ln_fail(1, 21, MSG_NOT_MERC); return; }
    if (ln_unit_find(uid)) { ln_fail(1, 21, MSG_HAVE); return; }
    if (!ln_np_spend(merc_cost(id, r->level))) { ln_fail(1, 21, MSG_NO_NP); return; }
    ln_member *m = &list[merc_count++];
    memset(m, 0, sizeof *m);
    m->uid = uid;
    m->id = (int16_t)id;
    m->level = r->level;
    memcpy(m->name, r->name, sizeof m->name);
    m->floor = m->room = m->slot = -1;
    m->expire = (int64_t)time(NULL) + CONTRACT_DAYS * 86400;
    ln_unit_stats(m);
    ln_mercs_save();
    ln_begin(1, 21, 0);
    ln_wstr16(MSG_CONTRACT);
    ln_send();
}

// 1.23: 피로회복. 응답 NP, 안내문. 받은 쪽이 피로도를 0으로 만든다(0x16415c).
static void recover(ln_req *q) {
    ln_member *m = ln_unit_find(read_uid(q));
    if (!m) { ln_fail(1, 23, MSG_NO_UNIT); return; }
    if (m->uid > 0 && !m->joined) { ln_fail(1, 23, MSG_NOT_JOINED); return; }
    if (m->fatigue == 0) { ln_fail(1, 23, MSG_NOT_TIRED); return; }
    if (!ln_np_spend(m->recover_cost)) { ln_fail(1, 23, MSG_NO_NP); return; }
    m->fatigue = 0;
    unit_save(m);
    ln_begin(1, 23, 0);
    ln_w32((uint32_t)ln_me()->np);
    ln_wstr16(MSG_RESTED);
    ln_send();
}

// 1.24: u8 개수 뒤 동료, 용병 순서로 (uid be64, 층, 칸 x, 칸 y)(0x1516b4). 게임이 한도를 이미 검사했다(0x165f00). 응답 후 게임이 메뉴로 돌아간다.
static void save_placement(ln_req *q) {
    int n = ln_u8(q);
    for (int i = 0; i < n; i++) {
        int64_t uid = read_uid(q);
        int16_t floor = (int16_t)ln_u16(q), room = (int16_t)ln_u16(q), slot = (int16_t)ln_u16(q);
        ln_member *m = ln_unit_find(uid);
        if (!m) continue;
        m->floor = floor;
        m->room = room;
        m->slot = slot;
    }
    ln_members_save();
    ln_mercs_save();
    ln_begin(1, 24, 0);
    ln_wstr16(MSG_SAVED);
    ln_send();
}

// 1.25: 계약연장 문의. 안내문이 곧 묻는 글이고 화면 0x10에서 예면 1.26이 온다(0x1522a8, 0x1627e4).
static void extend_ask(ln_req *q) {
    ln_member *m = ln_unit_find(read_uid(q));
    if (!m || m->uid >= 0) { ln_fail(1, 25, MSG_NO_UNIT); return; }
    char text[100];
    snprintf(text, sizeof text, FMT_ASK, CONTRACT_DAYS, merc_cost(m->id, m->level));
    ln_begin(1, 25, 0);
    ln_wstr16(text);
    ln_send();
}

// 1.26: 계약연장. 응답 NP, 남은 일(+0x54), 안내문.
static void extend(ln_req *q) {
    ln_member *m = ln_unit_find(read_uid(q));
    if (!m || m->uid >= 0) { ln_fail(1, 26, MSG_NO_UNIT); return; }
    if (!ln_np_spend(merc_cost(m->id, m->level))) { ln_fail(1, 26, MSG_NO_NP); return; }
    int64_t now = (int64_t)time(NULL);
    m->expire = (m->expire > now ? m->expire : now) + CONTRACT_DAYS * 86400;
    ln_mercs_save();
    ln_begin(1, 26, 0);
    ln_w32((uint32_t)ln_me()->np);
    ln_w32((uint32_t)((m->expire - now) / 86400));
    ln_wstr16(MSG_EXTENDED);
    ln_send();
}

// 1.27: 계약 전 정보. 본문 id. 응답을 NS+0x768에 풀고 계약 창 0x17054c가 그린다(0x152478).
static void merc_info(ln_req *q) {
    int id = (int)ln_u32(q);
    const em_rec *r = is_merc_id(id) ? em_load(id) : NULL;
    if (!r) { ln_fail(1, 27, MSG_NOT_MERC); return; }
    ln_member m;
    memset(&m, 0, sizeof m);
    m.id = (int16_t)id;
    m.level = r->level;
    ln_unit_stats(&m);
    ln_begin(1, 27, 0);
    ln_w8((uint8_t)r->level);
    ln_w32((uint32_t)m.hp);
    ln_w32((uint32_t)m.attack);
    ln_w32((uint32_t)m.crit);
    ln_w32((uint32_t)m.mdef); // NS+0x7a2
    ln_w32((uint32_t)m.pdef); // NS+0x7a0
    ln_w32((uint32_t)merc_cost(id, r->level));
    ln_send();
}

bool ln_staff(ln_req *q) {
    switch (q->minor) {
    case 20: list_units(q); return true;
    case 21: contract(q); return true;
    case 23: recover(q); return true;
    case 24: save_placement(q); return true;
    case 25: extend_ask(q); return true;
    case 26: extend(q); return true;
    case 27: merc_info(q); return true;
    }
    return false; // 1.22(0x151664)는 부르는 코드가 없다
}
