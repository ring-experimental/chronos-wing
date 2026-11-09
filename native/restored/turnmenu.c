// 턴제 모드의 필드 메뉴 개편(사용자 결정 2026-11-09): 원작 "주인공"을 "스테이터스"로, 그 아래에 "팀원 편성"을 넣는다.
// - 항목 목록 0x120cfc(id 배열)을 감싸 싱글 목록 [0 주인공, 1, 2, 3/4, 5]을 [0, 7, 1, 2, 3/4, 5]로 바꾼다. 7은 네트워크 전용(코스튬자랑) 번호를 빌린다.
//   라벨 표 0x19aeec(메모리에는 실제 주소로 옮겨져 있다)의 0번과 7번을 새 글로 가리킨다.
// - 페이지 열기 0x120d8c(번호)를 감싸 0이면 멤버 목록, 7이면 팀원 편성 창을 연다. 창은 게임을 멈추고(turn_battle_open) 메뉴 화면 위에 그린다.
//   멤버 목록에서 고른 사람(주인공 포함)의 화면(능력치·장비·스킬)으로 간다. 원작 무장상태는 싱글에서 교체가 막혀 있어 주인공 장비도 여기서 바꾼다.
// 근거는 work/turn-plan.md "메뉴 개편".
#include "restored.h"
#include "turn.h"
#include <stdio.h>
#include <stdlib.h>

void orig_f_20cfc(cpu_t *c);
void orig_f_20d8c(cpu_t *c);
uint32_t gfx_screen_handle(void);

enum {
    LABELS = 0x19aeec, ID_HERO = 0, ID_TEAM = 7,
    FN_PANEL = 0x172aa8,
    MEMBERS = 0x1a9e58, MEMBER_SIZE = 0x74, M_NAME = 4, M_EXP = 0x28, M_NEED = 0x2c, MEMBER_COUNT = 8,
    FN_HERO = 0x147434, H_LEVEL = 0x101, H_CLASS = 0xb1, H_HP = 0xec, H_HPMAX = 0xf0, H_MP = 0x114, H_MPMAX = 0x118,
    H_EXP = 0xf4, FN_HERO_NEED = 0x12ebfc, FN_PATK = 0x1022c8, FN_MATK = 0x102348, FN_DEF = 0x101ef4, FN_DEF_ADD = 0x101fc0, FN_CRIT = 0x102280,
    KEY_UP = -1, KEY_DOWN = -2, KEY_LEFT = -3, KEY_RIGHT = -4, KEY_OK = -5, KEY_CLEAR = -16,
    ROWS = 8, MAX_CAND = TURN_BAG_SLOTS + 40, // 가방 127칸 + 창고 장비 탭
};

typedef enum { M_NONE, M_LIST, M_MEMBER, M_PICK, M_TEAM } mode;
static struct {
    mode mode;
    int cursor, top;
    int list[MEMBER_COUNT + 1], nlist; // 멤버 목록: -1 주인공, 0~7 동료
    int member, tab, slot;             // 동료 화면: 탭 0 능력치, 1 장비, 2 스킬
    uint32_t cand[MAX_CAND];
    int ncand;
    char note[64];
    uint16_t *shot;
    size_t shot_size;
} M;

bool turn_menu_open(void) { return M.mode != M_NONE; }

static void W32x(uint32_t a, uint32_t v) { cpu_t *c = &cpu; W32(a, v); }

static uint32_t label(const char *euc) {
    uint32_t p = heap_alloc(24);
    snprintf(gstr(p), 24, "%s", euc);
    return p;
}

// 항목 목록(id 배열, 0xff 끝). 싱글 목록이면 팀원 편성을 끼우고 라벨을 바꾼다. 네트워크 목록이면 원래 라벨로 돌린다.
void f_20cfc(cpu_t *c) {
    static uint32_t ids, l_status, l_team, o_hero, o_team;
    uint32_t arr = c->r[0];
    if (!o_hero) { o_hero = R32(LABELS + 4 * ID_HERO); o_team = R32(LABELS + 4 * ID_TEAM); }
    if (!turn_mode_on() || R8(arr) != ID_HERO) {
        W32x(LABELS + 4 * ID_HERO, o_hero);
        W32x(LABELS + 4 * ID_TEAM, o_team);
        orig_f_20cfc(c);
        return;
    }
    if (!ids) { ids = heap_alloc(8); l_status = label("\xbd\xba\xc5\xd7\xc0\xcc\xc5\xcd\xbd\xba"); l_team = label("\xc6\xc0\xbf\xf8 \xc6\xed\xbc\xba"); } // 스테이터스 / 팀원 편성
    uint8_t out[8];
    int n = 0;
    for (int i = 0; i < 7 && n < 7; i++) {
        uint8_t v = R8(arr + i);
        if (v == 0xff) continue;
        out[n++] = v;
        if (v == ID_HERO && n < 7) out[n++] = ID_TEAM;
    }
    while (n < 8) out[n++] = 0xff;
    memcpy(c->m + ids, out, 8);
    W32x(LABELS + 4 * ID_HERO, l_status);
    W32x(LABELS + 4 * ID_TEAM, l_team);
    c->r[0] = ids;
    orig_f_20cfc(c);
}

static void snapshot(void) {
    cpu_t *c = &cpu;
    uint32_t sh = gfx_screen_handle();
    if (!sh) return;
    uint32_t f = handle_data(sh);
    size_t size = (size_t)R32(f + 4) * R32(f + 8);
    if (size > M.shot_size) { M.shot = realloc(M.shot, size); M.shot_size = size; }
    memcpy(M.shot, c->m + handle_data(R32(f + 16)), size);
}

static void build_list(void) {
    M.nlist = 0;
    M.list[M.nlist++] = -1;
    int party[3], np = turn_party_members(party);
    for (int k = 0; k < np; k++) M.list[M.nlist++] = party[k];
    for (int i = 0; i < MEMBER_COUNT; i++) {
        bool in = false;
        for (int k = 0; k < np; k++) in |= party[k] == i;
        if (!in && turn_member_joined(i)) M.list[M.nlist++] = i;
    }
}

static void open_mode(mode m) {
    if (M.mode == M_NONE) snapshot();
    M.mode = m;
    M.cursor = M.top = 0;
    M.note[0] = 0;
    if (m == M_LIST || m == M_TEAM) build_list();
}

// 페이지 열기. 턴제 모드에서 0(스테이터스)과 7(팀원 편성)은 자체 창으로 연다.
void f_20d8c(cpu_t *c) {
    int page = (int8_t)c->r[0];
    if (turn_mode_on() && (page == ID_HERO || page == ID_TEAM)) {
        open_mode(page == ID_HERO ? M_LIST : M_TEAM);
        return;
    }
    orig_f_20d8c(c);
}

// ---- 그리기 ----

static const char *member_name(int i) { return gstr(MEMBERS + (uint32_t)i * MEMBER_SIZE + M_NAME); }
static bool in_party(int i) {
    int p[3], n = turn_party_members(p);
    for (int k = 0; k < n; k++) if (p[k] == i) return true;
    return false;
}

static const char *SLOT_NAME[6] = {"\xb9\xab\xb1\xe2", "\xc5\xf5\xb1\xb8", "\xb0\xa9\xbf\xca", "\xc0\xe5\xb0\xa9", "\xbd\xc5\xb9\xdf", "\xbe\xc7\xbc\xbc\xbc\xad\xb8\xae"}; // 무기 / 투구 / 갑옷 / 장갑 / 신발 / 악세서리
static const char *CLASS_NAME[3] = {"\xb0\xdd\xc5\xf5", "\xb0\xcb", "\xc1\xa4\xb7\xc9"}; // 격투 / 검 / 정령
static const char *TAB_NAME[3] = {"\xb4\xc9\xb7\xc2\xc4\xa1", "\xc0\xe5\xba\xf1", "\xbd\xba\xc5\xb3"}; // 능력치 / 장비 / 스킬

static void row(int x, int y, int w, bool sel, const char *s, uint32_t col) {
    if (sel) turn_fill(x - 2, y - 2, w + 4, 15, 0x30, 0x5f, 0xae);
    turn_draw_text(x, y, w, s, sel ? 0xffe060 : col, 0);
}

static void draw(void) {
    cpu_t *c = &cpu;
    uint32_t sh = gfx_screen_handle();
    if (!sh) return;
    uint32_t f = handle_data(sh);
    int W = (int)R32(f), H = (int)R32(f + 4), bpl = (int)R32(f + 8);
    if (M.shot) memcpy(c->m + handle_data(R32(f + 16)), M.shot, (size_t)H * bpl);
    int pw = W < 260 ? W - 12 : 240, ph = 200, x = (W - pw) / 2, y = (H - ph) / 2, lx = x + 12, lw = pw - 24;
    CALL(FN_PANEL, x, y, pw, ph);
    turn_fill(x + 2, y + 2, pw - 4, ph - 4, 0x10, 0x18, 0x30);
    char s[96];
    int ly = y + 28;
    switch (M.mode) {
    case M_LIST: {
        turn_draw_text(W / 2, y + 8, pw, "\xbd\xba\xc5\xd7\xc0\xcc\xc5\xcd\xbd\xba", 0xe8d6ab, 1); // 스테이터스
        for (int k = 0; k < M.nlist; k++, ly += 16) {
            int i = M.list[k];
            if (i < 0) snprintf(s, sizeof s, "%s  Lv%d  (\xc1\xd6\xc0\xce\xb0\xf8)", "\xc4\xab\xc0\xcc", (int8_t)R8(CALL(FN_HERO) + H_LEVEL)); // %s  Lv%d  (주인공) / 카이
            else {
                member_stats st;
                turn_member_stats(i, &st);
                snprintf(s, sizeof s, "%s  Lv%d  %s", member_name(i), st.level, in_party(i) ? "(\xc6\xc4\xc6\xbc)" : "(\xb4\xeb\xb1\xe2)"); // (파티) / (대기)
            }
            row(lx, ly, lw, k == M.cursor, s, i >= 0 && !in_party(i) ? 0xa0a0a0 : 0xffffff);
        }
        break;
    }
    case M_TEAM: {
        int p[3], np = turn_party_members(p);
        snprintf(s, sizeof s, "\xc6\xc0\xbf\xf8 \xc6\xed\xbc\xba  (%d/3)", np); // 팀원 편성  (%d/3)
        turn_draw_text(W / 2, y + 8, pw, s, 0xe8d6ab, 1);
        for (int k = 1; k < M.nlist; k++, ly += 16) {
            int i = M.list[k];
            member_stats st;
            turn_member_stats(i, &st);
            snprintf(s, sizeof s, "%s %s  Lv%d", in_party(i) ? "[\xc6\xc4\xc6\xbc]" : "[    ]", member_name(i), st.level); // [파티]
            row(lx, ly, lw, k - 1 == M.cursor, s, in_party(i) ? 0xffffff : 0xa0a0a0);
        }
        if (M.nlist <= 1) turn_draw_text(W / 2, ly, pw, "\xc7\xd5\xb7\xf9\xc7\xd1 \xb5\xbf\xb7\xe1\xb0\xa1 \xbe\xf8\xb4\xd9", 0xc0c0c0, 1); // 합류한 동료가 없다
        turn_draw_text(W / 2, y + ph - 34, pw, "\xc8\xae\xc0\xce: \xb3\xd6\xb1\xe2/\xbb\xa9\xb1\xe2   \xc3\xeb\xbc\xd2: \xb4\xdd\xb1\xe2", 0xc0c0c0, 1); // 확인: 넣기/빼기   취소: 닫기
        break;
    }
    case M_MEMBER: case M_PICK: {
        int i = M.member;
        member_stats st;
        uint32_t h = CALL(FN_HERO);
        if (i >= 0) turn_member_stats(i, &st);
        else { memset(&st, 0, sizeof st); st.level = (int8_t)R8(h + H_LEVEL); }
        if (i >= 0) snprintf(s, sizeof s, "%s  Lv%d", member_name(i), st.level);
        else snprintf(s, sizeof s, "\xc4\xab\xc0\xcc  Lv%d", st.level); // 카이
        turn_draw_text(lx, y + 8, lw, s, 0xe8d6ab, 0);
        for (int t = 0; t < 3; t++) {
            int tx = x + pw - 150 + t * 48;
            if (t == M.tab) turn_fill(tx - 2, y + 6, 46, 15, 0x30, 0x5f, 0xae);
            turn_draw_text(tx + 21, y + 8, 46, TAB_NAME[t], t == M.tab ? 0xffe060 : 0xc0c0c0, 1);
        }
        if (M.tab == 0 && i < 0) { // 주인공: 원작 스테이터스와 같은 값(0x122fb0)
            char b[9][48];
            snprintf(b[0], 48, "EXP  %d / %d", (int)R32(h + H_EXP), (int)CALL(FN_HERO_NEED, h));
            snprintf(b[1], 48, "HP   %d / %d", (int)R32(h + H_HP), (int)R32(h + H_HPMAX));
            snprintf(b[2], 48, "MP   %d / %d", (int)R32(h + H_MP), (int)R32(h + H_MPMAX));
            snprintf(b[3], 48, "\xb9\xb0\xb8\xae\xb0\xf8\xb0\xdd  %d", (int)CALL(FN_PATK)); // 물리공격
            snprintf(b[4], 48, "\xb8\xb6\xb5\xb5\xb0\xf8\xb0\xdd  %d", (int)CALL(FN_MATK)); // 마도공격
            for (int d = 0; d < 2; d++) {
                int v = (int)CALL(FN_DEF, d);
                v += (int)CALL(FN_DEF_ADD, v, d);
                snprintf(b[5 + d], 48, "%s  %d", d ? "\xb8\xb6\xb5\xb5\xb9\xe6\xbe\xee" : "\xb9\xb0\xb8\xae\xb9\xe6\xbe\xee", v); // 마도방어 / 물리방어
            }
            snprintf(b[7], 48, "\xc4\xa1\xb8\xed\xc5\xb8  %d%%", (int)CALL(FN_CRIT)); // 치명타
            snprintf(b[8], 48, "\xc1\xf7\xbe\xf7  %s", CLASS_NAME[R8(h + H_CLASS) % 3]); // 직업  (무기 종류가 정한다)
            for (int k = 0; k < 9; k++) turn_draw_text(lx, ly + k * 17, lw, b[k], 0xffffff, 0);
        } else if (M.tab == 0) {
            uint32_t e = MEMBERS + (uint32_t)i * MEMBER_SIZE;
            const char *lines[8];
            char b[8][48];
            snprintf(b[0], 48, "EXP  %d / %d", (int)R32(e + M_EXP), (int)R32(e + M_NEED));
            snprintf(b[1], 48, "HP   %d", st.hp);
            snprintf(b[2], 48, "MP   %d", st.mp);
            snprintf(b[3], 48, "%s  %d", st.magic ? "\xb8\xb6\xb5\xb5\xb0\xf8\xb0\xdd" : "\xb9\xb0\xb8\xae\xb0\xf8\xb0\xdd", st.atk); // 마도공격 / 물리공격
            snprintf(b[4], 48, "\xb9\xb0\xb8\xae\xb9\xe6\xbe\xee  %d", st.pdef); // 물리방어  %d
            snprintf(b[5], 48, "\xb8\xb6\xb5\xb5\xb9\xe6\xbe\xee  %d", st.mdef); // 마도방어  %d
            snprintf(b[6], 48, "%s", in_party(i) ? "\xc6\xc4\xc6\xbc\xbf\xf8(\xb0\xe6\xc7\xe8\xc4\xa1 100%)" : "\xb4\xeb\xb1\xe2 \xc1\xdf(\xb0\xe6\xc7\xe8\xc4\xa1 50%)"); // 파티원(경험치 100%) / 대기 중(경험치 50%)
            for (int k = 0; k < 7; k++) { lines[k] = b[k]; turn_draw_text(lx, ly + k * 18, lw, lines[k], 0xffffff, 0); }
        } else if (M.tab == 1 && M.mode == M_MEMBER) {
            for (int sl = 0; sl < 6; sl++, ly += 20) {
                uint32_t it = turn_equip_item(i, sl);
                snprintf(s, sizeof s, "%s: %s", SLOT_NAME[sl], it ? turn_item_name(it) : "-");
                row(lx, ly, lw, sl == M.slot, s, it ? 0xffffff : 0xa0a0a0);
            }
            turn_draw_text(W / 2, y + ph - 34, pw, "\xc8\xae\xc0\xce: \xb9\xd9\xb2\xd9\xb1\xe2", 0xc0c0c0, 1); // 확인: 바꾸기
        } else if (M.tab == 1) {
            snprintf(s, sizeof s, "%s \xb0\xed\xb8\xa3\xb1\xe2", SLOT_NAME[M.slot]); // %s 고르기
            turn_draw_text(lx, ly - 2, lw, s, 0xe8d6ab, 0);
            ly += 16;
            int n = M.ncand + 1;
            for (int k = M.top; k < n && k < M.top + ROWS; k++, ly += 16) {
                if (k == 0) row(lx, ly, lw, M.cursor == 0, "(\xc7\xd8\xc1\xa6)", 0xffffff); // (해제)
                else row(lx, ly, lw, M.cursor == k, turn_item_name(M.cand[k - 1]), 0xffffff);
            }
            if (!M.ncand) turn_draw_text(lx, ly, lw, "\xb0\xa1\xb9\xe6\xb0\xfa \xc3\xa2\xb0\xed\xbf\xa1 \xb8\xc2\xb4\xc2 \xc0\xe5\xba\xf1\xb0\xa1 \xbe\xf8\xb4\xd9", 0xa0a0a0, 0); // 가방과 창고에 맞는 장비가 없다
        } else {
            tskill basic, sk[14];
            int n = i >= 0 ? turn_member_skills(i, st.level, &basic, sk, 14) : turn_hero_skills(R8(h + H_CLASS), &basic, sk, 14);
            if (i < 0 && !n) turn_draw_text(lx, ly, lw, "\xb9\xe8\xbf\xee \xbd\xba\xc5\xb3\xc0\xcc \xbe\xf8\xb4\xd9", 0xa0a0a0, 0); // 배운 스킬이 없다
            int step = n > 9 ? 13 : 18; // 주인공은 스킬이 많다(격투 13개까지)
            for (int k = 0; k < n; k++, ly += step) {
                bool open = sk[k].unlock <= st.level;
                snprintf(s, sizeof s, "%s", sk[k].name);
                turn_draw_text(lx, ly, lw - 70, s, open ? 0xffffff : 0x707070, 0);
                if (open) snprintf(s, sizeof s, "MP %d", sk[k].mp);
                else snprintf(s, sizeof s, "Lv%d", sk[k].unlock);
                turn_draw_text(x + pw - 70, ly, 60, s, open ? 0xffffff : 0x707070, 0);
            }
        }
        break;
    }
    default: break;
    }
    if (M.note[0]) turn_draw_text(W / 2, y + ph - 18, pw, M.note, 0xffa040, 1);
    video_present(sh);
}

bool turn_menu_tick(void) {
    if (M.mode == M_NONE) return false;
    draw();
    return true;
}

// ---- 키 ----

bool turn_menu_key(int type, int code) {
    if (type != 2) return true;
    bool ok = code == KEY_OK || code == '5', back = code == KEY_CLEAR || code == '0';
    bool up = code == KEY_UP || code == '2', down = code == KEY_DOWN || code == '8';
    bool left = code == KEY_LEFT || code == '4', right = code == KEY_RIGHT || code == '6';
    M.note[0] = 0;
    switch (M.mode) {
    case M_LIST:
        if (up) M.cursor = (M.cursor + M.nlist - 1) % M.nlist;
        else if (down) M.cursor = (M.cursor + 1) % M.nlist;
        else if (back) M.mode = M_NONE;
        else if (ok) {
            int i = M.list[M.cursor];
            M.member = i; M.tab = 0; M.slot = 0; M.mode = M_MEMBER; // 주인공(-1)도 같은 화면(능력치·장비·스킬)
        }
        break;
    case M_MEMBER:
        if (left) M.tab = (M.tab + 2) % 3;
        else if (right) M.tab = (M.tab + 1) % 3;
        else if (back) { int keep = M.member; open_mode(M_LIST); for (int k = 0; k < M.nlist; k++) if (M.list[k] == keep) M.cursor = k; }
        else if (M.tab == 1 && up) M.slot = (M.slot + 5) % 6;
        else if (M.tab == 1 && down) M.slot = (M.slot + 1) % 6;
        else if (M.tab == 1 && ok) {
            M.ncand = turn_equip_candidates(M.member, M.slot, M.cand, MAX_CAND);
            M.cursor = M.top = 0;
            M.mode = M_PICK;
        }
        break;
    case M_PICK: {
        int n = M.ncand + 1;
        if (up) M.cursor = (M.cursor + n - 1) % n;
        else if (down) M.cursor = (M.cursor + 1) % n;
        else if (back) M.mode = M_MEMBER;
        else if (ok) {
            const char *why = turn_equip_set(M.member, M.slot, M.cursor ? M.cand[M.cursor - 1] : 0);
            if (why) snprintf(M.note, sizeof M.note, "%s", why);
            M.mode = M_MEMBER;
        }
        if (M.cursor < M.top) M.top = M.cursor;
        if (M.cursor >= M.top + ROWS) M.top = M.cursor - ROWS + 1;
        break;
    }
    case M_TEAM: {
        int n = M.nlist - 1;
        if (n <= 0) { if (back || ok) M.mode = M_NONE; break; }
        if (up) M.cursor = (M.cursor + n - 1) % n;
        else if (down) M.cursor = (M.cursor + 1) % n;
        else if (back) M.mode = M_NONE;
        else if (ok) {
            const char *r;
            turn_party_toggle(M.list[M.cursor + 1], &r);
            snprintf(M.note, sizeof M.note, "%s", r);
            for (char *p = M.note; *p; p++) if (*p == ';') *p = ' ';
        }
        break;
    }
    default: break;
    }
    if (M.mode != M_NONE) draw();
    return true;
}
