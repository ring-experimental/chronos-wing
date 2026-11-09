// 턴제 모드에서 필드 드랍을 없앤다. 드랍 개체를 만드는 곳은 0x117d7c 하나뿐이다(호출은 0x14ab54 안 한 곳: 처치 보상 0x14aa54와
// 채집 대상이 맞을 때 0x1495f8). 그 자리에서 원본 가방 넣기 0x13ad9c(가방, 종류, id, 1)를 부르고, 차면 창고 0x13cc4c(칸, 종류, id, 1)에 넣는다.
// 가방 넣기가 수집 퀘스트 집계(0x128d98)까지 한다. 창고에 들어간 것은 원본처럼 꺼낼 때까지 세지 않는다.
// 얻은 것은 게임을 멈춘 확인 창으로 보인다. 근거는 work/turn-plan.md "스킬·역할·상태이상·드랍 없애기".
#include "restored.h"
#include "turn.h"
#include <stdio.h>
#include <stdlib.h>

void orig_f_17d7c(cpu_t *c);
uint32_t gfx_screen_handle(void);
bool turn_mode_on(void);

enum {
    FN_BAG = 0x128f0c, FN_BAG_ADD = 0x13ad9c,          // (가방, 종류, id, 수) 성공이면 참. 실패하면 가방은 그대로
    FN_STORE_TAB = 0x13c6a4, FN_STORE_ADD = 0x13cc4c,  // (종류) 창고 칸, (칸, 종류, id, 수)
    FN_DROP_NAME = 0x117df0,                            // (개체, 종류, id) 개체 +0x25에 이름(미식별 포함)
    FN_PANEL = 0x172aa8,
    DROP_NAME = 0x25, NAME_BUF = 0x48,
    E_STATE = 0xac, E_ACTIVE = 0x18, MON_DEAD = 7,
    LINES = 8, NAME_CAP = 32, WAIT_MAX = 6,
    QUIET_MS = 250,        // 마지막 드랍 뒤 이만큼 조용하면 창을 연다(한 번에 여러 개 떨어지는 것을 모은다)
    BATTLE_WAIT_MS = 5000, // 전투 뒤 처치 동작이 끝나 보상이 나올 때까지 기다리는 최대 시간
    KEY_OK = -5, KEY_CLEAR = -16,
};

typedef enum { TO_BAG, TO_STORE, TO_LOST } dest;
static struct { char name[NAME_CAP]; int n; dest to; } got_items[LINES];
static int got_n, got_more;
static uint64_t last_drop;
static uint32_t wait_mon[WAIT_MAX];
static int wait_n;
static uint64_t wait_until;
static bool open_now;
static uint16_t *shot;
static size_t shot_size;

bool turn_loot_open(void) { return open_now; }

static void add_line(const char *name, dest to) {
    for (int i = 0; i < got_n; i++)
        if (got_items[i].to == to && !strcmp(got_items[i].name, name)) { got_items[i].n++; return; }
    if (got_n == LINES) { got_more++; return; }
    snprintf(got_items[got_n].name, NAME_CAP, "%s", name);
    got_items[got_n].n = 1;
    got_items[got_n].to = to;
    got_n++;
}

// 드랍 만들기(x, y, 종류 s8, id s16). 턴제 모드에서는 바닥에 두지 않는다.
void f_17d7c(cpu_t *c) {
    if (!turn_mode_on()) { orig_f_17d7c(c); return; }
    int kind = (int8_t)c->r[2], id = (int16_t)c->r[3];
    static uint32_t nb;
    if (!nb) nb = heap_alloc(NAME_BUF);
    memset(c->m + nb, 0, NAME_BUF);
    CALL(FN_DROP_NAME, nb, (uint32_t)kind, (uint32_t)id);
    char name[NAME_CAP];
    snprintf(name, sizeof name, "%s", gstr(nb + DROP_NAME));
    dest to = TO_LOST;
    if (CALL(FN_BAG_ADD, CALL(FN_BAG), (uint32_t)kind, (uint32_t)id, 1) & 0xff) to = TO_BAG;
    else if (CALL(FN_STORE_ADD, CALL(FN_STORE_TAB, (uint32_t)kind), (uint32_t)kind, (uint32_t)id, 1) & 0xff) to = TO_STORE;
    fprintf(stderr, "[turn] 드랍 바로 넣기: 종류 %d id %d → %s\n", kind, id, to == TO_BAG ? "가방" : to == TO_STORE ? "창고" : "자리 없음");
    add_line(name, to);
    last_drop = now_ms();
}

// 전투에서 쓰러뜨린 몬스터. 필드에서 처치 동작이 끝나야(상태 7) 보상이 나오므로 그때까지 창을 미룬다.
void turn_loot_expect(const uint32_t *mons, int n) {
    wait_n = 0;
    for (int i = 0; i < n && wait_n < WAIT_MAX; i++) if (mons[i]) wait_mon[wait_n++] = mons[i];
    wait_until = wait_n ? now_ms() + BATTLE_WAIT_MS : 0;
}

static bool still_waiting(void) {
    if (!wait_until) return false;
    if (now_ms() >= wait_until) { wait_until = 0; return false; }
    cpu_t *c = &cpu;
    for (int i = 0; i < wait_n; i++) {
        uint32_t m = wait_mon[i];
        if (R8(m + E_ACTIVE) && R8(m + E_STATE) != MON_DEAD) return true;
    }
    wait_until = 0;
    return false;
}

static void draw(void) {
    cpu_t *c = &cpu;
    uint32_t sh = gfx_screen_handle();
    if (!sh) return;
    uint32_t f = handle_data(sh);
    int w = (int)R32(f), h = (int)R32(f + 4), bpl = (int)R32(f + 8);
    uint8_t *px = c->m + handle_data(R32(f + 16));
    if (shot) memcpy(px, shot, (size_t)h * bpl);
    int rows = got_n + (got_more ? 1 : 0);
    int pw = w < 240 ? w - 16 : 220, ph = 44 + rows * 15, x = (w - pw) / 2, y = (h - ph) / 2;
    CALL(FN_PANEL, x, y, pw, ph);
    turn_fill(x + 2, y + 2, pw - 4, ph - 4, 0x10, 0x18, 0x30);
    turn_draw_text(w / 2, y + 7, pw, "\xc8\xb9\xb5\xe6\xc7\xd1 \xbe\xc6\xc0\xcc\xc5\xdb", 0xe8d6ab, 1); // 획득한 아이템
    int ly = y + 24;
    for (int i = 0; i < got_n; i++, ly += 15) {
        char s[80];
        const char *tail = got_items[i].to == TO_STORE ? " (\xc3\xa2\xb0\xed\xb7\xce \xba\xb8\xb3\xbf)" : got_items[i].to == TO_LOST ? " (\xb0\xa1\xb5\xe6\x20\xc2\xf7\xbc\xad\x20\xb9\xf6\xb7\xc1\xc1\xfc)" : ""; //  (창고로 보냄) /  (가득 차서 버려짐): 가방과 창고가 모두 차면 바닥에 두지 않고 버린다
        snprintf(s, sizeof s, "%s x%d%s", got_items[i].name, got_items[i].n, tail);
        uint32_t col = got_items[i].to == TO_BAG ? 0xffffff : got_items[i].to == TO_STORE ? 0x80d0ff : 0xff8080;
        turn_draw_text(x + 10, ly, pw - 16, s, col, 0);
    }
    if (got_more) {
        char s[40];
        snprintf(s, sizeof s, "\xbf\xdc %d\xb0\xb3", got_more); // 외 %d개
        turn_draw_text(x + 10, ly, pw - 16, s, 0xc0c0c0, 0);
    }
    turn_draw_text(w / 2, y + ph - 16, pw, "\xc8\xae\xc0\xce", 0xffe060, 1); // 확인
    video_present(sh);
}

// 필드에서 매 틱(전투가 닫혀 있을 때). 창이 열려 있으면 참.
bool turn_loot_tick(void) {
    if (open_now) { draw(); return true; }
    if (!got_n && !got_more) return false;
    if (now_ms() - last_drop < QUIET_MS || still_waiting()) return false;
    cpu_t *c = &cpu;
    uint32_t sh = gfx_screen_handle();
    if (!sh) return false;
    uint32_t f = handle_data(sh);
    size_t size = (size_t)R32(f + 4) * R32(f + 8);
    if (size > shot_size) { shot = realloc(shot, size); shot_size = size; }
    memcpy(shot, c->m + handle_data(R32(f + 16)), size);
    open_now = true;
    draw();
    return true;
}

bool turn_loot_key(int type, int code) {
    if (type == 2 && (code == KEY_OK || code == '5' || code == KEY_CLEAR || code == '0')) {
        open_now = false;
        got_n = got_more = 0;
    }
    return true;
}
