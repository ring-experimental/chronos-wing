// 내장 서버: 3(20~21) 경매. 내가 올린 물건은 창고 저장소에 있고, 다른 판매자 물건은 /ITM/DAT 표로 만든다.
// 생성 규칙과 가격 근거는 work/net-offline.md의 창고와 경매 절.
#include "host.h"
#include "localnet_depot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum {
    NS = 0x20d758, NS_NP = 0x1ec,
    FN_DECRYPT = 0x105908,
    CATEGORIES = 15,
    PAGE_ITEMS = 30,       // 요청의 0x1e, 응답을 담는 표도 30칸(NS+0x146c, 0x1158바이트)
    MARKET_PER_KIND = 12,  // 정한 값
    SALE_BASE = 100000,    // 시장 판매번호 = 기준 + 종류*1000 + id. 창고 uid와 겹치지 않는다
    SOLD_MAX = 64,
    KIND_SCROLL = 71,
    PRICE_RATE = 10,       // 표 가격/10 = 기준 NP. 정한 값
    SELL_AFTER_SECONDS = 86400,
    TABLE_MAX = 200,
    OPTION_TABLE_GOT = 0x19c8d4 + 0x514, // 0x12bbfc가 옵션 표를 얻는 곳
    OPTION_ENTRY = 0x24, OPTION_MIN = 0x1c, OPTION_MAX = 0x1d,
};

static const char MSG_NONE[] = "\xc0\xcc\xb9\xcc\x20\xc6\xc7\xb8\xc5\xb5\xc8\x20\xb9\xb0\xc7\xb0\xc0\xd4\xb4\xcf\xb4\xd9\x2e"; // 이미 판매된 물품입니다.
static const char MSG_NP[] = "\x4e\x50\xb0\xa1\x20\xba\xce\xc1\xb7\xc7\xd5\xb4\xcf\xb4\xd9\x2e"; // NP가 부족합니다.
static const char MSG_OWN[] = "\xc0\xda\xbd\xc5\xc0\xcc\x20\xb5\xee\xb7\xcf\xc7\xd1\x20\xb9\xb0\xc7\xb0\xc0\xd4\xb4\xcf\xb4\xd9\x2e"; // 자신이 등록한 물품입니다.
static const char MSG_BUY[] = "\xb1\xb8\xc0\xd4\xc7\xdf\xbd\xc0\xb4\xcf\xb4\xd9\x2e\x20\x4e\x54\x20\xc3\xa2\xb0\xed\xb7\xce\x20\xba\xb8\xb3\xc2\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 구입했습니다. NT 창고로 보냈습니다.
static const char MSG_FULL[] = "\xc3\xa2\xb0\xed\xb0\xa1\x20\xb0\xa1\xb5\xe6\x20\xc3\xa1\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 창고가 가득 찼습니다.

// 필터 이름표 0x9b320의 번호 -> /ITM/DAT 파일. 0은 판매번호 검색이다.
static const int8_t CATEGORY_KIND[CATEGORIES] = {-1, 3, 4, 5, 6, 7, 0, 1, 2, 13, 14, 15, 16, 17, KIND_SCROLL};

// 다른 플레이어 이름은 클라이언트에 없다. 정한 이름이다.
static const char *const SELLERS[] = {
    "\xb9\xd9\xb4\xd9\xb4\xc1\xb4\xeb", "\xc3\xb6\xb0\xa5\xb8\xc5\xb1\xe2", "\xba\xd3\xc0\xba\xb4\xe9", "\xc0\xba\xba\xfb\xb5\xc0",
    "\xb1\xb8\xb8\xa7\xbb\xf3\xc0\xce", "\xba\xb0\xb6\xcb\xba\xb0", "\xc7\xaa\xb8\xa5\xb0\xed\xb7\xa1", "\xc8\xb2\xb1\xdd\xb3\xaa\xc4\xa7",
};

bool auction_kind_allowed(int kind) {
    for (int i = 1; i < CATEGORIES; i++)
        if (CATEGORY_KIND[i] == kind) return true;
    return false;
}

// 아이템 표. 형식은 0x137654와 0x1377a0이다. 개수 u16, 항목은 종류 u8, id u16, 이름 길이 u8 + 이름, 가격 u32, 분류별 값, 설명 길이 u8 + 설명(분류 2, 3은 설명 없음).
typedef struct { int16_t id; int32_t price; } table_row;
static table_row tables[CATEGORIES][TABLE_MAX];
static int table_rows[CATEGORIES];
static bool table_loaded[CATEGORIES];

static uint8_t *read_table_file(int kind, long *size) {
    char path[1200];
    snprintf(path, sizeof path, "%s/ITM/DAT/%02d", game_dir, kind);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)n);
    if (d && fread(d, 1, (size_t)n, f) != (size_t)n) { free(d); d = NULL; }
    fclose(f);
    if (!d) return NULL;
    if (kind <= 2) {
        // 0x1375ec: 0~2번은 앞 16바이트 해시 뒤가 암호문이다(0x184b54).
        long body = n - 16;
        uint32_t buf = heap_alloc((uint32_t)body);
        memcpy(cpu.m + buf, d + 16, (size_t)body);
        uint32_t a[] = {buf, (uint32_t)body};
        guest_call(FN_DECRYPT, a, 2);
        memcpy(d, cpu.m + buf, (size_t)body);
        heap_free(buf);
        n = body;
    }
    *size = n;
    return d;
}

// 가격 뒤 값의 길이. 0x137654 표.
static long extra_size(int kind, const uint8_t *d, long p) {
    if (kind <= 2) { // 분류 0: 1, 문자열 둘, 4+16+16+4+4+12
        long q = p + 1;
        q += 1 + d[q];
        q += 1 + d[q];
        return q + 4 + 16 + 16 + 4 + 4 + 12 - p;
    }
    if (kind <= 7) return 2 + 12;
    if (kind <= 14) return 7 + 12;
    if (kind <= 18) return 2 + 12;
    return 0; // 분류 10
}

static void load_table(int category) {
    if (table_loaded[category]) return;
    table_loaded[category] = true;
    int kind = CATEGORY_KIND[category];
    long size = 0;
    uint8_t *d = read_table_file(kind, &size);
    if (!d) { fprintf(stderr, "[net] 아이템 표 %d 읽기 실패\n", kind); return; }
    int n = d[0] | d[1] << 8;
    long p = 2;
    for (int i = 0; i < n && p + 8 < size; i++) {
        int id = d[p + 1] | d[p + 2] << 8, name = d[p + 3];
        long price_at = p + 4 + name;
        int32_t price = (int32_t)((uint32_t)d[price_at] | (uint32_t)d[price_at + 1] << 8 | (uint32_t)d[price_at + 2] << 16 | (uint32_t)d[price_at + 3] << 24);
        p = price_at + 4;
        p += extra_size(kind, d, p);
        p += 1 + d[p];
        if (table_rows[category] < TABLE_MAX) tables[category][table_rows[category]++] = (table_row){(int16_t)id, price};
    }
    free(d);
}

static int category_of(int kind) {
    for (int i = 1; i < CATEGORIES; i++)
        if (CATEGORY_KIND[i] == kind) return i;
    return -1;
}

// 기준 NP. 표 가격이 0이면 파는 물건이 아니므로 0이다.
static int32_t reference_np(int kind, int id) {
    int c = category_of(kind);
    if (c < 0) return 0;
    load_table(c);
    for (int i = 0; i < table_rows[c]; i++) {
        if (tables[c][i].id != id) continue;
        int32_t np = tables[c][i].price / PRICE_RATE;
        return tables[c][i].price > 0 && np < 10 ? 10 : np;
    }
    return 0;
}

static uint32_t mix(uint32_t a, uint32_t b) {
    uint32_t h = a * 2654435761u ^ b * 2246822519u;
    h ^= h >> 15; h *= 3266489917u; h ^= h >> 13;
    return h;
}

static uint32_t today(void) { return (uint32_t)((int64_t)time(NULL) / 86400); }

// 산 물건은 그날 시장에서 뺀다.
static struct {
    uint32_t day;
    int32_t count;
    uint32_t sold[SOLD_MAX];
} market;
static bool market_loaded;

static void market_load(void) {
    if (!market_loaded) {
        market_loaded = true;
        if (ln_load("market", &market, sizeof market) != (long)sizeof market) memset(&market, 0, sizeof market);
    }
    if (market.day != today()) {
        market.day = today();
        market.count = 0;
    }
}

static bool market_sold(uint32_t sale) {
    market_load();
    for (int i = 0; i < market.count; i++)
        if (market.sold[i] == sale) return true;
    return false;
}

typedef struct {
    uint32_t sale;
    int8_t kind;
    int16_t id;
    int32_t price;
    const char *seller;
} market_row;

// 분류에서 날마다 정해진 MARKET_PER_KIND개를 고른다. 같은 날에는 목록이 같다. 가격은 기준 NP의 80~120%, 10 단위.
static int market_rows(int category, market_row *out) {
    load_table(category);
    int kind = CATEGORY_KIND[category], n = 0;
    uint32_t day = today();
    uint32_t best[MARKET_PER_KIND];
    for (int i = 0; i < table_rows[category]; i++) {
        table_row r = tables[category][i];
        if (r.price <= 0) continue;
        uint32_t h = mix(day, (uint32_t)kind << 16 | (uint16_t)r.id);
        int at = n;
        while (at > 0 && best[at - 1] > h) at--;
        if (at >= MARKET_PER_KIND) continue;
        int last = n < MARKET_PER_KIND ? n : MARKET_PER_KIND - 1;
        memmove(best + at + 1, best + at, (size_t)(last - at) * sizeof *best);
        memmove(out + at + 1, out + at, (size_t)(last - at) * sizeof *out);
        best[at] = h;
        int32_t np = reference_np(kind, r.id);
        int32_t price = np * (80 + (int32_t)(h >> 8 & 0xffff) % 41) / 100 / 10 * 10;
        out[at] = (market_row){SALE_BASE + (uint32_t)kind * 1000 + (uint16_t)r.id, (int8_t)kind, r.id, price < 10 ? 10 : price,
                               SELLERS[(h >> 24) % (sizeof SELLERS / sizeof *SELLERS)]};
        if (n < MARKET_PER_KIND) n++;
    }
    return n;
}

static void market_item(const market_row *r, depot_item *it) {
    memset(it, 0, sizeof *it);
    it->uid = r->sale;
    it->kind = r->kind;
    it->id = r->id;
    depot_item_defaults(it);
    it->serial = r->sale;
    if (r->kind == KIND_SCROLL) {
        // 게임 표에서 나오는 값이 아이템 하나마다 임의(0x12bbfc)다. 시장에서는 범위 가운데로 고정한다.
        // +0x177=1, +0x178=id, +0x17a=값. 0x13dd94가 내려받을 때 이 셋을 쓴다.
        uint32_t table = R32_(&cpu, OPTION_TABLE_GOT);
        uint32_t row = table + (uint32_t)r->id * OPTION_ENTRY;
        int mn = (int8_t)R8_(&cpu, row + OPTION_MIN), mx = (int8_t)R8_(&cpu, row + OPTION_MAX);
        it->opt[0] = 1;
        it->opt[1] = (uint8_t)r->id;
        it->opt[2] = (uint8_t)((mn + mx) / 2);
    }
}

// 0x1544f4 레코드: 판매번호 be64(+0x5c), uid be64(+0), 0x153d68과 같은 값, 가격 be32(+0x58, 게임이 아이템 +0x5c에 넣어 표시), 판매자 10바이트(+0x64).
static void write_row(const depot_item *it, int32_t price, const char *seller) {
    char name[10] = {0};
    strncpy(name, seller, sizeof name);
    ln_w64(it->uid);
    ln_w64(it->uid);
    ln_w8((uint8_t)it->kind);
    ln_w16((uint16_t)it->id);
    ln_w8(it->count);
    ln_w8(1); // 표시에 쓰지 않는다. 경매 중
    ln_wbytes(it->opt, sizeof it->opt);
    ln_w32(it->serial);
    ln_wbytes(it->refine, sizeof it->refine);
    ln_w8(it->pal_len / 2);
    ln_w32((uint32_t)price);
    ln_wbytes(name, sizeof name);
}

typedef struct { depot_item item; int32_t price; const char *seller; } row;

// 이 조건의 전체 줄을 모은다. 내 물건이 앞이다.
static int collect(int category, uint32_t search, row *out, int cap) {
    int n = 0, have;
    const depot_item *mine = depot_items(&have);
    for (int i = 0; i < have && n < cap; i++) {
        if (!mine[i].listed) continue;
        bool match = category == 0 ? mine[i].uid == search : CATEGORY_KIND[category] == mine[i].kind;
        if (match) out[n++] = (row){mine[i], mine[i].price, ln_me()->nick};
    }
    for (int c = category ? category : 1; c < (category ? category + 1 : CATEGORIES); c++) {
        market_row rows[MARKET_PER_KIND];
        int m = market_rows(c, rows);
        for (int i = 0; i < m && n < cap; i++) {
            if (market_sold(rows[i].sale) || (category == 0 && rows[i].sale != search)) continue;
            market_item(&rows[i], &out[n].item);
            out[n].price = rows[i].price;
            out[n].seller = rows[i].seller;
            n++;
        }
    }
    return n;
}

static void search_items(ln_req *q) {
    int category = (int8_t)ln_u8(q);
    int per_page = (int8_t)ln_u8(q);
    int page = (int8_t)ln_u8(q);
    ln_u32(q);
    uint32_t search = ln_u32(q);
    if (category < 0 || category >= CATEGORIES) category = 0;
    if (per_page <= 0 || per_page > PAGE_ITEMS) per_page = PAGE_ITEMS;
    if (page < 0) page = 0;
    auction_settle();
    static row rows[DEPOT_MAX + MARKET_PER_KIND * CATEGORIES];
    int n = collect(category, search, rows, (int)(sizeof rows / sizeof *rows));
    int first = page * per_page, shown = n - first;
    if (shown < 0) shown = 0;
    if (shown > per_page) shown = per_page;
    ln_begin(3, 20, 0);
    ln_w8((uint8_t)category); // 0x1584f8. 제목 표의 번호로 쓴다
    ln_w8((uint8_t)shown);    // NS+0xbdd
    for (int i = 0; i < shown; i++) write_row(&rows[first + i].item, rows[first + i].price, rows[first + i].seller);
    ln_w8(first + shown < n ? 1 : 0); // NS+0xf36 다음 페이지 표시
    ln_send();
}

static void buy(ln_req *q) {
    ln_u32(q);
    uint32_t sale = ln_u32(q);
    ln_u32(q); // 0x1482ec 일련번호. 쓸 곳이 없다
    if (depot_find(sale)) { ln_fail(3, 21, MSG_OWN); return; }
    if (sale < SALE_BASE) { ln_fail(3, 21, MSG_NONE); return; }
    int kind = (int)(sale - SALE_BASE) / 1000, category = category_of(kind);
    if (category < 0 || market_sold(sale)) { ln_fail(3, 21, MSG_NONE); return; }
    market_row rows[MARKET_PER_KIND];
    int n = market_rows(category, rows);
    const market_row *r = NULL;
    for (int i = 0; i < n; i++)
        if (rows[i].sale == sale) r = &rows[i];
    if (!r) { ln_fail(3, 21, MSG_NONE); return; }
    if (market.count >= SOLD_MAX) { ln_fail(3, 21, MSG_NONE); return; }
    if (ln_me()->np < r->price) { ln_fail(3, 21, MSG_NP); return; }
    depot_item *it = depot_add();
    if (!it) { ln_fail(3, 21, MSG_FULL); return; }
    uint32_t uid = it->uid;
    market_item(r, it);
    it->uid = uid;
    ln_np_spend(r->price);
    market.sold[market.count++] = sale;
    if (!ln_save("market", &market, sizeof market)) fprintf(stderr, "[net] 시장 저장 실패\n");
    depot_save();
    ln_begin(3, 21, 0);
    ln_w32((uint32_t)ln_me()->np);
    ln_wstr16(MSG_BUY);
    ln_send();
}

// 다른 플레이어가 없으므로 시세보다 싼 물건만 하루 뒤 팔린다. 상한은 시장 최저가(기준의 80%)보다 낮은 70%여서 사서 되팔아 NP를 늘릴 수 없다.
// 클라이언트에 판매 완료 문구나 전용 응답이 없다. 입금은 프로필에 바로 하고, 화면에 보이는 NS 값도 맞춘다(0x154760처럼 게임이 쓰는 칸).
void auction_settle(void) {
    int n;
    depot_item *items = depot_items(&n);
    int64_t now = (int64_t)time(NULL);
    bool changed = false;
    for (int i = n - 1; i >= 0; i--) {
        depot_item *it = &items[i];
        if (!it->listed || now - it->listed_at < SELL_AFTER_SECONDS) continue;
        int64_t limit = (int64_t)reference_np(it->kind, it->id) * it->count * 7 / 10;
        if (it->price > limit) continue;
        W32_(&cpu, NS + NS_NP, (uint32_t)ln_np_add(it->price));
        depot_remove(it);
        changed = true;
    }
    if (changed) depot_save();
}

bool ln_auction(ln_req *q) {
    switch (q->minor) {
    case 20: search_items(q); return true;
    case 21: buy(q); return true;
    }
    return false;
}
