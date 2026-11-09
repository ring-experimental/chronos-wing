// 짐가방과 허리주머니 용량. 짐가방은 36칸(6열)을 3행씩 스크롤하고 허리주머니 슬롯당 수량은 99개다.
// 원본 0x13ad90, 0x13aad8, 0x13aa04, 0x13aa74, 0x13aeb8, 0x13b52c를 옮겼다.
#include "restored.h"
#include "turn.h"
#include <stdio.h>
#include <sys/stat.h>

enum {
    BAG_SLOTS = 36,
    BAG_COLS = 6,
    BAG_VISIBLE_ROWS = 3,
    CELL = 20,
    CELL_INNER = 18,
    BAR_GAP = 1,
    BAR_W = 3,
    COUNT_ABOVE = 17, // 그리드 위 "짐가방" 제목 줄
    COUNT_TEXT_MAX = 8,

    POUCH_STACK = 99,
    KIND_POUCH = 3,

    // 아이템 기록은 최대 231바이트이므로 36칸에 8320바이트면 된다. 창고와 같은 크기를 쓴다.
    BAGSAVE_SIZE = 9000,
    BAGSAVE_OLD_SIZE = 5400, // 18칸 시절 파일. 크기가 다르면 해시가 틀려 읽히지 않는다.

    FILTER_PAGE_SIZE = 0xcc,
    FILTER_PAGE_COUNT = 200,
    FILTER_PAGE_ITEMS = 30,
    FILTER_PAGES = 4,

    GOT_SCREEN = 0x2c,
    GOT_BAG_FILE = 0x970,

    BAG_CURSOR = 4,
    BAG_MARK = 5,
    BAG_LIST = 0x10,
    BAG_LIST_COUNT = 0x18,
    LIST_FIRST = 0,
    NODE_NEXT = 4,
    NODE_ITEM = 0x10,
    ITEM_KIND = 0x20,
    POUCH_COUNT = 8,

    FN_BAG = 0x128f0c,
    FN_MEMSET = 0x188980,
    FN_FILE_READ = 0x1849fc,
    FN_FILE_WRITE = 0x184ac4,
    FN_READ_INT = 0x183cfc,
    FN_BAG_CLEAR = 0x13a9f4,
    FN_LIST_LOAD = 0x13881c,
    FN_LIST_SAVE = 0x1387e4,
    FN_DRAW_SLOT = 0x138894,
    FN_DRAW_CURSOR = 0x11f10c,
    FN_DRAW_TEXT = 0x12c1ec,
    FN_FILTER_ADD = 0x13ef50,
    FN_STACKABLE = 0x138a24,
    FN_STACK_ADD = 0x1385d4,
    FN_NEW_ITEM = 0x137494,
    FN_POUCH_FREE = 0x13b190,
    FN_POUCH_INSERT = 0x13b19c,
};

static uint32_t file_buffer(void) {
    static uint32_t buffer;
    if (!buffer) buffer = heap_alloc(TURN_BAG_FILE_SIZE); // 턴제 127칸 파일도 담는다(아이템 기록 최대 231바이트)
    return buffer;
}

// 칸 수: 본편 36, 턴제 127(turnbag.c).
static int bag_slots(void) { return turn_bag_wide() ? TURN_BAG_SLOTS : BAG_SLOTS; }

// 비어 있지 않은 칸만 커서가 지나가므로 스크롤도 커서를 따라간다. 커서가 없으면(-1, 허리주머니 쪽) 그대로 둔다.
static int scroll_row(int cursor, int rows, int cols, int visible) {
    static int top;
    if (cursor >= 0) {
        int row = cursor / cols;
        if (row < top) top = row;
        if (row >= top + visible) top = row - visible + 1;
    }
    int last = rows > visible ? rows - visible : 0;
    if (top > last) top = last;
    return top;
}

static void draw_scrollbar(cpu_t *c, uint32_t screen, int x, int y, int top, int rows, int visible) {
    int height = visible * CELL - (CELL - CELL_INNER);
    int thumb = height * visible / rows;
    int offset = (height - thumb) * top / (rows - visible);
    CALL(FN_FILL_RECT, screen, x, y, BAR_W, height, 0x40, 0x30, 0x20);
    CALL(FN_FILL_RECT, screen, x, y + offset, BAR_W, thumb, 0xe8, 0xd6, 0xab);
}

static void draw_count(cpu_t *c, uint32_t screen, int right, int y, int count) {
    static uint32_t text;
    if (!text) text = heap_alloc(COUNT_TEXT_MAX);
    int len = snprintf(gstr(text), COUNT_TEXT_MAX, "%d/%d", count, bag_slots());
    CALL(FN_DRAW_TEXT, screen, right, y, text, len, 2);
}

void f_3ad90(cpu_t *c) { c->r[0] = (uint32_t)(int32_t)(int8_t)(bag_slots() - (int)R32(c->r[0] + BAG_LIST_COUNT)); }

void f_3aad8(cpu_t *c) {
    uint32_t bag = c->r[0];
    int grid_x = (int)c->r[1], grid_y = (int)c->r[2];
    bool focused = (int8_t)c->r[3] != 0;
    uint32_t screen = R32(got(GOT_SCREEN));
    int count = (int)R32(bag + BAG_LIST_COUNT);
    int cols = turn_bag_cols(), visible = turn_bag_rows();
    if (turn_bag_wide()) { // 넓은 창(240): 원작 자리(창 + 0x1d) 대신 격자와 스크롤 막대를 창 가운데에 둔다
        int span = cols * CELL - (CELL - CELL_INNER) + BAR_GAP + BAR_W;
        grid_x = grid_x - 0x1d + (240 - span) / 2;
    }
    int rows = (count + cols - 1) / cols;
    int cursor = (int8_t)R8(bag + BAG_CURSOR);
    int mark = (int8_t)R8(bag + BAG_MARK);
    int top = scroll_row(cursor, rows, cols, visible);

    int index = top * cols;
    uint32_t node = R32(bag + BAG_LIST + LIST_FIRST);
    for (int i = 0; i < index && node; i++) node = R32(node + NODE_NEXT);

    int cursor_x = -1, cursor_y = -1;
    for (int row = 0; row < visible; row++) {
        for (int col = 0; col < cols; col++, index++) {
            int x = grid_x + col * CELL, y = grid_y + row * CELL;
            CALL(FN_DRAW_SLOT, node ? R32(node + NODE_ITEM) : 0, x, y, mark == index);
            if (cursor == index) { cursor_x = x; cursor_y = y; }
            if (node) node = R32(node + NODE_NEXT);
        }
    }
    if (rows > visible)
        draw_scrollbar(c, screen, grid_x + cols * CELL + BAR_GAP - (CELL - CELL_INNER), grid_y, top, rows, visible);
    draw_count(c, screen, grid_x + cols * CELL - (CELL - CELL_INNER), grid_y - COUNT_ABOVE, count);
    if (focused && cursor_x != -1) CALL(FN_DRAW_CURSOR, bag + BAG_CURSOR, cursor_x, cursor_y, CELL_INNER, CELL_INNER, 1);
}

static int read_bag_file(cpu_t *c, uint32_t buffer, int size) {
    CALL(FN_MEMSET, buffer, 0xffffffffu, BAGSAVE_SIZE);
    return (int)CALL(FN_FILE_READ, got(GOT_BAG_FILE), buffer, size);
}

// 턴제 127칸 파일. 본편 BAGSAVE(9000바이트 고정, 해시)에는 36칸까지만 들어가므로 따로 둔다. 해시 없이 원작 목록 저장 형식 그대로다.
static void turn_bag_path(char *out, size_t cap) { snprintf(out, cap, "%s/TURNBAG", save_dir); }
static long file_time(const char *p) { struct stat st; return stat(p, &st) == 0 ? (long)st.st_mtime : -1; }

// TURNBAG이 있고 BAGSAVE보다 오래되지 않았으면 그것을 읽는다(본편으로 같은 폴더에 저장한 뒤면 BAGSAVE가 새것이다).
static bool turn_bag_load(cpu_t *c, uint32_t buffer, uint32_t bag) {
    char p[1100], b[1100];
    turn_bag_path(p, sizeof p);
    snprintf(b, sizeof b, "%s/BAGSAVE", save_dir);
    long tp = file_time(p), tb = file_time(b);
    if (tp < 0 || (tb >= 0 && tb > tp)) return false;
    FILE *f = fopen(p, "rb");
    if (!f) return false;
    CALL(FN_MEMSET, buffer, 0xffffffffu, TURN_BAG_FILE_SIZE);
    size_t n = fread(c->m + buffer, 1, TURN_BAG_FILE_SIZE, f);
    fclose(f);
    if (n != TURN_BAG_FILE_SIZE) return false;
    CALL(FN_BAG_CLEAR);
    if ((int)CALL(FN_READ_INT, buffer, 0) > 0) CALL(FN_LIST_LOAD, bag + BAG_LIST, buffer, 0);
    fprintf(stderr, "[turn] 가방: TURNBAG 읽음 %d칸\n", (int)R32(bag + BAG_LIST_COUNT));
    return true;
}

static void turn_bag_save(cpu_t *c, uint32_t buffer, uint32_t bag, bool erase) {
    CALL(FN_MEMSET, buffer, 0xffffffffu, TURN_BAG_FILE_SIZE);
    if (!erase) CALL(FN_LIST_SAVE, bag + BAG_LIST, buffer, 0);
    char p[1100];
    turn_bag_path(p, sizeof p);
    FILE *f = fopen(p, "wb");
    if (f) { fwrite(c->m + buffer, 1, TURN_BAG_FILE_SIZE, f); fclose(f); }
}

void f_3aa04(cpu_t *c) {
    uint32_t buffer = file_buffer();
    uint32_t bag = CALL(FN_BAG);
    if (turn_bag_wide() && turn_bag_load(c, buffer, bag)) return;
    int result = read_bag_file(c, buffer, BAGSAVE_SIZE);
    if (result < 0) result = read_bag_file(c, buffer, BAGSAVE_OLD_SIZE);
    CALL(FN_BAG_CLEAR);
    if (result > 0 && (int)CALL(FN_READ_INT, buffer, 0) > 0) CALL(FN_LIST_LOAD, bag + BAG_LIST, buffer, 0);
}

void f_3aa74(cpu_t *c) {
    bool erase = (int8_t)c->r[0] != 0;
    uint32_t buffer = file_buffer();
    uint32_t bag = CALL(FN_BAG);
    bool over = (int)R32(bag + BAG_LIST_COUNT) > BAG_SLOTS;
    if (turn_bag_wide()) turn_bag_save(c, buffer, bag, erase);
    // 본편 파일에는 36칸까지만 들어간다. 넘치면 본편 BAGSAVE는 빈 가방으로 쓴다(턴제는 TURNBAG을 읽는다).
    CALL(FN_MEMSET, buffer, 0xffffffffu, BAGSAVE_SIZE);
    if (!erase && !over) CALL(FN_LIST_SAVE, bag + BAG_LIST, buffer, 0);
    CALL(FN_FILE_WRITE, got(GOT_BAG_FILE), buffer, BAGSAVE_SIZE);
}

// 창고에 이어 짐가방의 조건 맞는 아이템을 4페이지 목록에 넣는다. 원본은 다섯 번째 페이지로 넘치므로 끝에서 멈춘다.
void f_3aeb8(cpu_t *c) {
    uint32_t pages = c->r[0];
    int min_kind = (int8_t)c->r[1], max_kind = (int8_t)c->r[2];
    uint32_t bag = CALL(FN_BAG);
    int page = 0;
    while (page < FILTER_PAGES - 1 && (int)R32(pages + page * FILTER_PAGE_SIZE + FILTER_PAGE_COUNT) == FILTER_PAGE_ITEMS) page++;
    for (uint32_t node = R32(bag + BAG_LIST + LIST_FIRST); node; node = R32(node + NODE_NEXT)) {
        uint32_t item = R32(node + NODE_ITEM);
        int kind = (int8_t)R8(item + ITEM_KIND);
        if (kind < min_kind || kind > max_kind) continue;
        if ((int)R32(pages + page * FILTER_PAGE_SIZE + FILTER_PAGE_COUNT) == FILTER_PAGE_ITEMS && ++page == FILTER_PAGES) return;
        CALL(FN_FILTER_ADD, pages + page * FILTER_PAGE_SIZE, item);
    }
}

void f_3b52c(cpu_t *c) {
    uint32_t pouch = c->r[0];
    int kind = (int8_t)c->r[1], id = (int16_t)c->r[2], amount = (int8_t)c->r[3];
    int free_slots = (int8_t)CALL(FN_POUCH_FREE, pouch);
    int limit = (CALL(FN_STACKABLE, kind) & 0xff) ? POUCH_STACK : 1;
    int fits = (int8_t)CALL(FN_STACK_ADD, pouch, kind, id, amount, limit, free_slots, 0);
    if (fits >= -3 && fits <= -1) { c->r[0] = 0; return; }
    int left = (int8_t)CALL(FN_STACK_ADD, pouch, kind, id, amount, limit, free_slots, 1);
    while (left > 0) {
        int part = left < POUCH_STACK ? left : POUCH_STACK;
        uint32_t item = CALL(FN_NEW_ITEM, KIND_POUCH, kind, id, part);
        CALL(FN_POUCH_INSERT, pouch, item, (int8_t)R8(pouch + POUCH_COUNT));
        left -= part;
    }
    c->r[0] = 1;
}
