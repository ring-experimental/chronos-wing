// 턴제 모드의 파티 편성. 원작 소울링크(동료 한 명이 주인공과 같은 경험치를 받음)를 "파티 추가"로 바꾼다.
// - 파티는 주인공 + 동료 최대 3명. 저장은 세이브 폴더의 TURNPARTY(원작 +0 링크 바이트는 로드 때 한 명만 남으므로 쓰지 않는다).
// - 경험치(0x12eb44 감싸기): 파티원은 주인공과 같은 양, 합류했지만 대기 중인 동료는 절반(사용자 결정 2026-11-09).
// - 합류할 때 동료 레벨이 주인공보다 낮으면 주인공 레벨로 맞춘다(사용자 결정). 자리가 있으면 파티에 넣는다.
// - 비공정 면담의 소울링크 버튼은 "파티추가"/"파티제외"가 된다(0x15bf84 그리기 전 글을 바꾸고, 0x15bd54 확인 처리를 가로챈다).
// 근거는 work/turn-plan.md "파티 편성".
#include "restored.h"
#include "turn.h"
#include <stdio.h>
#include <stdlib.h>

void orig_f_2eb44(cpu_t *c);
void orig_f_5bd54(cpu_t *c);
void orig_f_5bf84(cpu_t *c);
void orig_f_276b4(cpu_t *c);

enum {
    MEMBERS = 0x1a9e58, MEMBER_SIZE = 0x74, MEMBER_COUNT = 8,
    M_LINK = 0, M_EXP = 0x28, M_NEED = 0x2c, M_LEVEL = 0x35, MAX_LEVEL = 99,
    FN_HERO = 0x147434, H_LEVEL = 0x101, H_LINK = 0x11c,
    FN_MEMBER_EXP = 0x17d0f0,          // (동료, 양) 경험치와 레벨 올림
    FN_NETWORK = 0x156d24,             // 네트워크 모드면 참
    STORY_VARS = 0x1a2618 + 0x7b4, STORY_DONE = 2, EXPERT_VAR = 0xff,
    GOT_TALK = 0x1ec, TALK_MEMBER = 0, TALK_STATE = 8, TALK_BUTTON = 0xc, // 면담 상태(0x15bd54)
    LABEL_LINK = 0x1954b0,             // 면담 버튼 "소울링크"(8바이트) — 라벨 표 0x19b2ac[1]
    FN_POPUP = 0x12988c, GOT_POPUP = 0x5c8, POPUP_TITLE = 0x24, POPUP_BODY = 0x138, POPUP_BUF = 0x35, POPUP_FREE_TEXT = 13,
    PARTY_MAX = 3,
};

static const uint8_t JOIN_VAR[MEMBER_COUNT] = {0x03, 0x03, 0x03, 0x03, 0x07, 0x05, 0x05, 0x0f}; // 엔지, 레이, 리키, 후거스, 보우머, 쟝, 펭, 팡

static int party[PARTY_MAX], party_n = -1; // -1: 아직 안 읽음
static bool in_game; // 세이브를 읽었거나(0x1276b4) 게임 안에서 전투·경험치·메뉴가 있었다. 타이틀에서는 주인공 값이 기본값이라 아무것도 하지 않는다
static unsigned seen;                      // 합류를 확인한 동료(레벨 맞춤을 한 번만 한다)

static uint32_t member(int i) { return MEMBERS + (uint32_t)i * MEMBER_SIZE; }

bool turn_member_joined(int i) {
    cpu_t *c = &cpu;
    if (i < 0 || i >= MEMBER_COUNT) return false;
    if (getenv("CHRONOS_TURN_ALL")) return true; // 시험용: 모든 동료 합류로 본다
    return R8(STORY_VARS + EXPERT_VAR) == STORY_DONE || R8(STORY_VARS + JOIN_VAR[i]) == STORY_DONE;
}

static void file_path(char *out, size_t cap) { snprintf(out, cap, "%s/TURNPARTY", save_dir); }

// 형식: "TP1" 개수 동료번호×3 합류확인(2바이트)
static void party_save(void) {
    char p[1100];
    file_path(p, sizeof p);
    FILE *f = fopen(p, "wb");
    if (!f) return;
    uint8_t b[9] = {'T', 'P', '1', (uint8_t)party_n, 0xff, 0xff, 0xff, (uint8_t)seen, (uint8_t)(seen >> 8)};
    for (int i = 0; i < party_n; i++) b[4 + i] = (uint8_t)party[i];
    fwrite(b, 1, sizeof b, f);
    fclose(f);
}

static bool in_party(int i) {
    for (int k = 0; k < party_n; k++) if (party[k] == i) return true;
    return false;
}

// 합류 때 레벨 맞춤: 주인공보다 낮으면 주인공 레벨, 경험치 0, 필요치는 원본 식 (L²-L+80)L(0x17d144)
static void catch_up(int i) {
    cpu_t *c = &cpu;
    int hero = (int8_t)R8(CALL(FN_HERO) + H_LEVEL), lv = R8(member(i) + M_LEVEL);
    if (hero > MAX_LEVEL) hero = MAX_LEVEL;
    if (lv >= hero) return;
    W8(member(i) + M_LEVEL, (uint8_t)hero);
    W32(member(i) + M_EXP, 0);
    W32(member(i) + M_NEED, (uint32_t)((hero * hero - hero + 80) * hero));
    fprintf(stderr, "[turn] 파티: 동료 %d 레벨 %d → %d(합류 맞춤)\n", i, lv, hero);
}

static void party_load(void) {
    char p[1100];
    file_path(p, sizeof p);
    party_n = 0;
    seen = 0;
    FILE *f = fopen(p, "rb");
    uint8_t b[9];
    if (f && fread(b, 1, sizeof b, f) == sizeof b && !memcmp(b, "TP1", 3) && b[3] <= PARTY_MAX) {
        for (int i = 0; i < b[3]; i++) if (b[4 + i] < MEMBER_COUNT) party[party_n++] = b[4 + i];
        seen = b[7] | b[8] << 8;
    } else {
        // 처음: 원작에서 소울링크한 동료를 앞에 두고 합류한 순서대로 채운다. 이미 합류한 동료는 지금 합류한 것으로 보고 레벨을 맞춘다.
        cpu_t *c = &cpu;
        uint32_t link = R32(CALL(FN_HERO) + H_LINK);
        int linked = link >= MEMBERS && link < MEMBERS + MEMBER_COUNT * MEMBER_SIZE ? (int)((link - MEMBERS) / MEMBER_SIZE) : -1;
        if (linked >= 0 && turn_member_joined(linked)) party[party_n++] = linked;
        for (int i = 0; i < MEMBER_COUNT; i++) {
            if (!turn_member_joined(i)) continue;
            seen |= 1u << i;
            catch_up(i);
            if (party_n < PARTY_MAX && !in_party(i)) party[party_n++] = i;
        }
        party_save();
    }
    if (f) fclose(f);
}

static void ensure(void) { if (party_n < 0) party_load(); }

// 매 틱(필드): 새로 합류한 동료를 찾아 레벨을 맞추고 자리가 있으면 넣는다.
// 세이브 읽기 마지막 단계(동료 읽기 0x12751c 뒤 소울링크 다시 걸기). 끝나면 파티 정보를 새로 읽는다.
void f_276b4(cpu_t *c) {
    orig_f_276b4(c);
    if (!turn_mode_on()) return;
    in_game = true;
    party_n = -1;
    fprintf(stderr, "[turn] 파티: 세이브 읽음\n");
}

void turn_party_tick(void) {
    if (!turn_mode_on() || !save_dir[0] || !in_game) return;
    ensure();
    bool changed = false;
    for (int i = 0; i < MEMBER_COUNT; i++) {
        if ((seen >> i & 1) || !turn_member_joined(i)) continue;
        seen |= 1u << i;
        catch_up(i);
        if (party_n < PARTY_MAX && !in_party(i)) party[party_n++] = i;
        changed = true;
    }
    if (changed) party_save();
}

// 전투 파티(합류한 동료만, 편성 순서)
int turn_party_members(int *out) {
    in_game = true;
    ensure();
    int n = 0;
    for (int k = 0; k < party_n; k++) if (turn_member_joined(party[k])) out[n++] = party[k];
    return n;
}

bool turn_party_toggle(int i, const char **result) {
    ensure();
    if (in_party(i)) {
        int k = 0;
        for (int j = 0; j < party_n; j++) if (party[j] != i) party[k++] = party[j];
        party_n = k;
        *result = "\xc6\xc4\xc6\xbc\xbf\xa1\xbc\xad \xbb\xb0\xbd\xc0\xb4\xcf\xb4\xd9"; // 파티에서 뺐습니다
    } else {
        if (party_n >= PARTY_MAX) { *result = "\xc6\xc4\xc6\xbc\xb0\xa1 \xb0\xa1\xb5\xe6 \xc3\xa1\xbd\xc0\xb4\xcf\xb4\xd9;(\xc3\xd6\xb4\xeb 3\xb8\xed)"; return false; } // 파티가 가득 찼습니다;(최대 3명)
        catch_up(i); // 합류 때 맞추지 못한 동료(이 기능 전 세이브)도 넣을 때 맞춘다
        party[party_n++] = i;
        *result = "\xc6\xc4\xc6\xbc\xbf\xa1 \xb3\xd6\xbe\xfa\xbd\xc0\xb4\xcf\xb4\xd9"; // 파티에 넣었습니다
    }
    party_save();
    return true;
}

// ---- 경험치 ----

// 주인공 경험치(주인공, 양). 원본은 맨 앞에서 소울링크 동료(+0x11c)에게 같은 양을 준다. 턴제에서는 그 대신 파티원 100%, 대기 동료 50%.
void f_2eb44(cpu_t *c) {
    uint32_t hero = c->r[0];
    int amount = (int)c->r[1];
    if (!turn_mode_on() || (CALL(FN_NETWORK) & 0xff)) { orig_f_2eb44(c); return; }
    in_game = true;
    ensure();
    uint32_t link = R32(hero + H_LINK);
    W32(hero + H_LINK, 0);
    c->r[0] = hero;
    c->r[1] = (uint32_t)amount;
    orig_f_2eb44(c);
    uint32_t ret = c->r[0];
    W32(hero + H_LINK, link);
    for (int i = 0; i < MEMBER_COUNT; i++) {
        if (!turn_member_joined(i)) continue;
        int give = in_party(i) ? amount : amount / 2;
        if (give > 0) CALL(FN_MEMBER_EXP, member(i), (uint32_t)give);
        fprintf(stderr, "[turn] 경험치: 동료 %d %s +%d → Lv%d %d/%d\n", i, in_party(i) ? "파티" : "대기", give, R8(member(i) + M_LEVEL), (int)R32(member(i) + M_EXP), (int)R32(member(i) + M_NEED));
    }
    c->r[0] = ret;
}

// ---- 면담 ----

static int talk_member(void) {
    cpu_t *c = &cpu;
    uint32_t m = R32(got(GOT_TALK) + TALK_MEMBER);
    if (m < MEMBERS || m >= MEMBERS + MEMBER_COUNT * MEMBER_SIZE) return -1;
    return (int)((m - MEMBERS) / MEMBER_SIZE);
}

static void set_label(const char *euc8) { cpu_t *c = &cpu; memcpy(c->m + LABEL_LINK, euc8, 8); }

// 면담 창 그리기. 소울링크 버튼 글을 이 동료의 파티 상태에 맞춘다.
void f_5bf84(cpu_t *c) {
    if (turn_mode_on() && !(CALL(FN_NETWORK) & 0xff)) {
        int i = talk_member();
        if (i >= 0) { ensure(); set_label(in_party(i) ? "\xc6\xc4\xc6\xbc\xc1\xa6\xbf\xdc" : "\xc6\xc4\xc6\xbc\xc3\xdf\xb0\xa1"); } // 파티제외 / 파티추가
    } else set_label("\xbc\xd2\xbf\xef\xb8\xb5\xc5\xa9"); // 소울링크
    orig_f_5bf84(c);
}

static void popup(const char *title, const char *body) {
    cpu_t *c = &cpu;
    static uint32_t t;
    if (!t) t = heap_alloc(32);
    snprintf(gstr(t), 32, "%s", title);
    CALL(FN_POPUP, 0, POPUP_FREE_TEXT, 0x68, 0x62);
    uint32_t p = got(GOT_POPUP);
    snprintf(gstr(p + POPUP_BUF), 0x100, "%s", body);
    W32(p + POPUP_TITLE, t);
    W32(p + POPUP_BODY, p + POPUP_BUF);
}

// 면담 확인 처리. 상태 0에서 두 번째 버튼(원작 소울링크)을 고르면 파티에 넣거나 뺀다.
void f_5bd54(cpu_t *c) {
    uint32_t s = got(GOT_TALK);
    if (turn_mode_on() && !(CALL(FN_NETWORK) & 0xff) && R8(s + TALK_STATE) == 0 && R8(s + TALK_BUTTON) == 1) {
        int i = talk_member();
        if (i >= 0) {
            const char *msg;
            turn_party_toggle(i, &msg);
            popup("\xc6\xc4\xc6\xbc \xc6\xed\xbc\xba", msg); // 파티 편성
            fprintf(stderr, "[turn] 파티: 면담으로 동료 %d 바꿈, %d명\n", i, party_n);
        }
        c->r[0] = 0;
        return;
    }
    orig_f_5bd54(c);
}
