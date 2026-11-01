// 내장 서버: 3(1~8) NT 창고와 선원 장비. 아이템은 net/depot에 둔다. 근거는 work/net-offline.md의 창고와 경매 절.
#include "host.h"
#include "localnet_depot.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

enum {
    DEPOT_MAGIC = 0x4c4e4431,
};

static const char MSG_FULL[] = "\xc3\xa2\xb0\xed\xb0\xa1\x20\xb0\xa1\xb5\xe6\x20\xc3\xa1\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 창고가 가득 찼습니다.
static const char MSG_NONE[] = "\xbe\xc6\xc0\xcc\xc5\xdb\xc0\xbb\x20\xc3\xa3\xc0\xbb\x20\xbc\xf6\x20\xbe\xf8\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 아이템을 찾을 수 없습니다.
static const char MSG_LISTED[] = "\xb0\xe6\xb8\xc5\xc1\xdf\xc0\xce\x20\xbe\xc6\xc0\xcc\xc5\xdb\xc0\xd4\xb4\xcf\xb4\xd9\x2e\x20\xb0\xe6\xb8\xc5\xbb\xf3\xc5\xc2\x20\xc7\xd8\xc1\xa6\xc8\xc4\xbf\xa1\x20\xc0\xcc\xb5\xbf\xc0\xcc\xb3\xaa\x20\xb9\xf6\xb8\xae\xb1\xe2\xb0\xa1\x20\xb0\xa1\xb4\xc9\xc7\xd5\xb4\xcf\xb4\xd9\x2e"; // 경매중인 아이템입니다. 경매상태 해제후에 이동이나 버리기가 가능합니다.(0x936fc)
static const char MSG_DENY[] = "\xb0\xe6\xb8\xc5\xc0\xe5\xbf\xa1\x20\xb5\xee\xb7\xcf\xc7\xd2\xbc\xf6\x20\xbe\xf8\xb4\xc2\x20\xbe\xc6\xc0\xcc\xc5\xdb\xc0\xd4\xb4\xcf\xb4\xd9\x2e"; // 경매장에 등록할수 없는 아이템입니다.(0x93650)
static const char MSG_EQUIP[] = "\xc0\xe5\xba\xf1\xb8\xa6\x20\xb1\xb3\xc3\xbc\xc7\xdf\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 장비를 교체했습니다.
static const char MSG_NO_MEMBER[] = "\xb5\xbf\xb7\xe1\xb8\xa6\x20\xc3\xa3\xc0\xbb\x20\xbc\xf6\x20\xbe\xf8\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 동료를 찾을 수 없습니다.

static struct {
    uint32_t magic, next_uid;
    int32_t count;
    depot_item items[DEPOT_MAX];
} store;
static bool store_loaded;

static void store_load(void) {
    if (store_loaded) return;
    store_loaded = true;
    if (ln_load("depot", &store, sizeof store) != (long)sizeof store || store.magic != DEPOT_MAGIC) {
        memset(&store, 0, sizeof store);
        store.magic = DEPOT_MAGIC;
        store.next_uid = 1;
    }
}

depot_item *depot_items(int *n) {
    store_load();
    *n = store.count;
    return store.items;
}

void depot_save(void) {
    store_load();
    if (!ln_save("depot", &store, sizeof store)) fprintf(stderr, "[net] 창고 저장 실패\n");
}

depot_item *depot_add(void) {
    store_load();
    if (store.count >= DEPOT_MAX) return NULL;
    depot_item *it = &store.items[store.count++];
    memset(it, 0, sizeof *it);
    it->uid = store.next_uid++;
    return it;
}

depot_item *depot_find(uint32_t uid) {
    store_load();
    for (int i = 0; i < store.count; i++)
        if (store.items[i].uid == uid) return &store.items[i];
    return NULL;
}

void depot_remove(depot_item *it) {
    size_t i = (size_t)(it - store.items);
    memmove(it, it + 1, ((size_t)store.count - i - 1) * sizeof *it);
    store.count--;
}

void depot_item_defaults(depot_item *it) {
    it->count = 1;
    it->opt[0] = 0;
    memset(it->opt + 1, 0xff, 4);
    it->refine[0] = it->refine[1] = 0;
    it->pal_len = 0;
}

// 공통 앞부분: 0x153d68과 0x15409c가 같은 순서로 읽는다. +0xd는 경매 상태다. 0 등록 불가, 1 경매 중, 2 등록 가능(0x13d6d8).
static void write_head(const depot_item *it, uint8_t count) {
    ln_w64(it->uid);
    ln_w8((uint8_t)it->kind);
    ln_w16((uint16_t)it->id);
    ln_w8(count);
    ln_w8(it->listed ? 1 : auction_kind_allowed(it->kind) ? 2 : 0);
    ln_wbytes(it->opt, sizeof it->opt);
    ln_w32(it->serial);
    ln_wbytes(it->refine, sizeof it->refine);
}

static void list_items(ln_req *q) {
    int page = (int8_t)ln_u8(q);
    if (page < 0 || page >= DEPOT_PAGES) page = 0;
    auction_settle();
    int n;
    const depot_item *items = depot_items(&n);
    int first = page * DEPOT_PAGE_ITEMS, shown = n - first;
    if (shown < 0) shown = 0;
    if (shown > DEPOT_PAGE_ITEMS) shown = DEPOT_PAGE_ITEMS;
    ln_begin(3, 1, 0);
    ln_w8(DEPOT_PAGES); // NS+0x28de 페이지 수. 화면 (%d/%d)
    ln_w8(0);           // NS+0x28df 읽는 코드가 없다
    ln_w8((uint8_t)shown);
    for (int i = 0; i < shown; i++) {
        const depot_item *it = &items[first + i];
        write_head(it, it->count);
        ln_w8(it->pal_len / 2); // +0x180 팔레트 칸 수. 3.3만 바이트 수와 내용을 준다
    }
    ln_send();
}

// 3.2 본문은 0x153974, 값은 0x13d1b0이 게임 아이템에서 옮긴다. 저장은 3.8에서 한다.
// 게임이 응답을 받아 자기 칸에서 지우고 저장한 뒤 3.8을 보낸다(0x13c2c0). 그 전에 꺼지면 싱글 쪽에 물건이 남으므로 창고에도 남기지 않아야 복제가 없다.
static void upload(ln_req *q) {
    ln_u32(q); ln_u32(q); // uid는 0으로 온다
    int8_t kind = (int8_t)ln_u8(q);
    int16_t id = (int16_t)ln_u16(q);
    uint8_t count = ln_u8(q);
    ln_u8(q); // +0xd 0으로 온다
    uint8_t opt[5];
    ln_bytes(q, opt, sizeof opt);
    uint32_t serial = ln_u32(q);
    uint8_t refine[2];
    ln_bytes(q, refine, sizeof refine);
    uint8_t len = ln_u8(q);
    uint8_t pal[DEPOT_PAL_MAX];
    if (len > sizeof pal) len = sizeof pal;
    ln_bytes(q, pal, len);
    // 뒤에 오는 장비 수치(종류 8~12)는 내려줄 칸이 없다. 게임이 3.1의 id로 다시 합성한다(0x16dfc8).
    depot_item *it = count ? depot_add() : NULL;
    if (!it) {
        ln_fail(3, 2, MSG_FULL);
        return;
    }
    it->kind = kind;
    it->id = id;
    it->count = count;
    memcpy(it->opt, opt, sizeof opt);
    it->serial = serial;
    memcpy(it->refine, refine, sizeof refine);
    it->pal_len = len;
    memcpy(it->pal, pal, len);
    ln_begin(3, 2, 0);
    ln_w16(0);
    ln_send();
}

// 응답은 0x15409c. 물건은 게임이 목록에서 고른 항목으로 자기 칸에 넣으므로(0x13dd94 상태 7) 여기서는 수량만 맞춘다.
static void download(ln_req *q) {
    ln_u32(q);
    depot_item *it = depot_find(ln_u32(q));
    int want = (int8_t)ln_u8(q);
    if (!it) { ln_fail(3, 3, MSG_NONE); return; }
    if (it->listed) { ln_fail(3, 3, MSG_LISTED); return; }
    if (want < 1 || want > it->count) want = it->count;
    ln_begin(3, 3, 0);
    write_head(it, (uint8_t)want);
    ln_w8(it->pal_len);
    ln_wbytes(it->pal, it->pal_len);
    ln_send();
    if (want == it->count) depot_remove(it);
    else it->count -= (uint8_t)want;
    depot_save();
}

static void ok_empty(int minor) {
    ln_begin(3, minor, 0);
    ln_w16(0); // 0x153f98은 결과 0이면 안내문을 표시하지 않는다
    ln_send();
}

static void discard(ln_req *q) {
    ln_u32(q);
    depot_item *it = depot_find(ln_u32(q));
    if (!it) { ln_fail(3, 4, MSG_NONE); return; }
    if (it->listed) { ln_fail(3, 4, MSG_LISTED); return; }
    depot_remove(it);
    depot_save();
    ok_empty(4);
}

static void auction_list(ln_req *q) {
    ln_u32(q);
    depot_item *it = depot_find(ln_u32(q));
    int32_t price = (int32_t)ln_u32(q);
    if (!it) { ln_fail(3, 5, MSG_NONE); return; }
    if (!auction_kind_allowed(it->kind) || price <= 0) { ln_fail(3, 5, MSG_DENY); return; }
    it->listed = 1;
    it->price = price;
    it->listed_at = (int64_t)time(NULL);
    depot_save();
    ok_empty(5);
}

static void auction_cancel(ln_req *q) {
    ln_u32(q);
    depot_item *it = depot_find(ln_u32(q));
    if (!it) { ln_fail(3, 6, MSG_NONE); return; }
    it->listed = 0;
    depot_save();
    ok_empty(6);
}

// 선원 장비. 면담 화면의 악세사리는 종류 0x13이다(0x15bbdc -> 0x11fa08(6)). 3.7 본문은 동료 id(+0x48)와 아이템 id, -1은 해제다(0x11f9ca, 0x15bc34).
// 아이템은 게임이 자기 칸에서 옮긴다(0x1205b8). 응답은 그 동료의 전체 값(0x1542a4)이다.
// 본문에 수치가 없고 동료 수치가 정한 값이므로 교체로 수치를 바꾸지 않는다. 받은 값을 그대로 돌려준다.
static void equip(ln_req *q) {
    int16_t member_id = (int16_t)ln_u32(q);
    ln_u16(q);
    int n;
    ln_member *members = ln_members(&n);
    for (int i = 0; i < n; i++) {
        if (members[i].id != member_id) continue;
        ln_begin(3, 7, 0);
        ln_write_member(&members[i]);
        ln_wstr16(MSG_EQUIP); // 결과 0이면 0x157e94가 그대로 띄운다
        ln_send();
        return;
    }
    ln_fail(3, 7, MSG_NO_MEMBER);
}

bool ln_depot(ln_req *q) {
    switch (q->minor) {
    case 1: list_items(q); return true;
    case 2: upload(q); return true;
    case 3: download(q); return true;
    case 4: discard(q); return true;
    case 5: auction_list(q); return true;
    case 6: auction_cancel(q); return true;
    case 7: equip(q); return true;
    case 8: // 라우터 0x153d18 표의 8번은 응답 처리가 없고 게임도 기다리지 않는다(0x13c2c0). 3.2 확정 통보다.
        depot_save();
        return true;
    }
    return false;
}
