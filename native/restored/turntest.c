// 턴제 모드 시험 도구. 제어 파일(--ctrl) 명령으로만 쓴다.
//   turn quest N  메인 퀘스트 N 장면의 시작 상태(스토리 값, 맵, 위치)를 일반 플레이에 적용하고 그 장면 스크립트를 띄운다.
//                 시작 상태는 스토리 다시보기와 같다(본 장면은 기록, 아니면 정적 분석 값). 레벨, 소지품, 동료 수치는 그대로다.
//   turn quests   스토리 값(+0x7b4)에서 0이 아닌 것을 출력한다.
//   turn warp M X Y  맵 M의 칸 (X, Y)로 옮긴다.
//   turn spawnboss EMD [here]  이 지역 보스를 EMD로 정해 원본 생성 함수로 만든다. here면 주인공 옆에 둔다.
//   turn flag N V, turn qset N V  스토리 값 하나를 바꾼다(+0x6b4, +0x7b4).
//   turn mons     몬스터 목록과 보스 슬롯을 출력한다.
//   turn auto ok|win|off  대사를 확인 키로 넘긴다(ok), 열린 전투를 바로 이긴다(win).
//   turn run N    지금 맵의 메인 N을 돌린다. turn st  상태 한 줄. turn give 종류 id 수  아이템 지급. turn peek 주소 길이
#include "restored.h"
#include "turn.h"
#include <stdio.h>
#include <stdlib.h>

enum {
    SCRIPT = 0x1a2618,
    SCRIPT_RUNNING = SCRIPT + 0x328,
    SCRIPT_WARPING = SCRIPT + 0x6a0,
    SCRIPT_RELOAD = SCRIPT + 0x6a1,
    SCRIPT_TO_FIELD = SCRIPT + 0x6a2,
    SCRIPT_FLAGS = SCRIPT + 0x6b4,
    SCRIPT_QUESTS = SCRIPT + 0x7b4,
    GOT_LOADING = 0xa48,
    GOT_WORLD = 0x13c,
    WORLD_MAP = 0x2d4,
    FN_SCRIPT_RUN = 0x119ce4,
    FN_SCRIPT_END = 0x119e30,
    FN_WARP = 0x11b4f8,
};

static int jump_main = -1, jump_map;
static int warp_to[3];
static bool warp_pending;
static uint64_t jump_since;
static bool auto_ok, auto_win;
static int run_main = -1;
static uint64_t ok_at;
void host_key(int type, int code);

static bool busy(void) {
    cpu_t *c = &cpu;
    return R8(got(GOT_LOADING)) || R8(SCRIPT_RUNNING) || R8(SCRIPT_WARPING) || R8(SCRIPT_RELOAD) || R8(SCRIPT_TO_FIELD);
}

static void jump(int key) {
    cpu_t *c = &cpu;
    int map, x, y, main, dir, time;
    uint8_t flags[256], quests[256];
    if (!replay_scene_start(key, &map, &x, &y, &main, &dir, &time, flags, quests)) return;
    CALL(FN_SCRIPT_END);
    // 지금 세이브에서 이미 켜진 값은 두고, 장면 시작 상태에서 켜진 값을 더한다(퀘스트 값은 장면 값으로 맞춘다).
    for (int i = 0; i < 256; i++) {
        if (flags[i]) W8(SCRIPT_FLAGS + i, flags[i]);
        W8(SCRIPT_QUESTS + i, quests[i]);
    }
    uint32_t args[5] = {(uint32_t)map, (uint32_t)(x >> 4), (uint32_t)(y >> 4), (uint32_t)dir, (uint32_t)time};
    guest_call(FN_WARP, args, 5);
    jump_main = main;
    jump_map = map;
    jump_since = now_ms();
    fprintf(stderr, "[turn] 퀘스트 %d 장면으로: 맵 %d (%d,%d) 메인 %d\n", key, map, x, y, main);
}

// 맵을 읽고 입장 스크립트가 끝나면 장면 스크립트를 띄운다(메인 0은 입장 스크립트 자체다).
void turn_test_tick(void) {
    uint64_t t = now_ms();
    if (auto_ok && t - ok_at > 250) { // 시험: 대사와 전투 메시지를 확인 키로 넘긴다
        cpu_t *c = &cpu;
        if ((turn_battle_open() || R8(SCRIPT_RUNNING)) && !R8(got(GOT_LOADING))) { ok_at = t; host_key(2, -5); host_key(3, -5); }
    }
    if (auto_win && turn_battle_open()) turn_ctrl("turn win");
    if (turn_battle_open()) return; // 전투 중에 맵을 옮기면 전투의 몬스터 포인터가 바뀐다
    if (run_main >= 0) {
        cpu_t *c = &cpu;
        if (busy()) return;
        fprintf(stderr, "[turn] 메인 %d 실행 (맵 %d)\n", run_main, (int)R32(got(GOT_WORLD) + WORLD_MAP));
        CALL(FN_SCRIPT_RUN, run_main);
        run_main = -1;
        return;
    }
    if (warp_pending) {
        cpu_t *c = &cpu;
        if (busy()) return;
        warp_pending = false;
        uint32_t args[5] = {(uint32_t)warp_to[0], (uint32_t)warp_to[1], (uint32_t)warp_to[2], 1, 0xff};
        guest_call(FN_WARP, args, 5);
        return;
    }
    if (jump_main < 0) return;
    cpu_t *c = &cpu;
    if (now_ms() - jump_since < 300 || busy()) return;
    if ((int)R32(got(GOT_WORLD) + WORLD_MAP) != jump_map) { jump_main = -1; return; }
    if (jump_main) CALL(FN_SCRIPT_RUN, jump_main);
    jump_main = -1;
}

bool turn_test_ctrl(const char *cmd) {
    cpu_t *c = &cpu;
    if (!strncmp(cmd, "auto", 4)) { auto_ok = strstr(cmd, "ok") != NULL; auto_win = strstr(cmd, "win") != NULL; return true; }
    if (!strncmp(cmd, "run ", 4)) { run_main = atoi(cmd + 4); return true; } // 지금 맵의 메인 N을 원본 조건 그대로 돌린다(접촉, 대화와 같다)
    if (!strncmp(cmd, "give ", 5)) { // turn give 종류 id 수: 스크립트 명령 0x13과 같은 아이템 지급(0x13ad9c)
        int k = 0, id = 0, n = 1;
        sscanf(cmd + 5, "%d %d %d", &k, &id, &n);
        uint32_t r = CALL(0x13ad9c, CALL(0x128f0c), k, id, n);
        fprintf(stderr, "[turn] 지급 %d/%d x%d -> %u\n", k, id, n, r);
        return true;
    }
    if (!strncmp(cmd, "npcopt ", 7)) { // turn npcopt 코드: 열린 NPC 선택지 첫 칸을 그 코드로(시험: 9 창고)
        uint32_t o = got(0xe70);
        fprintf(stderr, "[turn] NPC 선택지 %d개:", R8(o));
        for (int i = 0; i < R8(o) && i < 8; i++) fprintf(stderr, " %d", R8(o + 0x2c + i));
        fprintf(stderr, "\n");
        W8(o + 0x2c, (uint8_t)atoi(cmd + 7));
        return true;
    }
    if (!strncmp(cmd, "store ", 6)) { // turn store 종류 id: 창고에 넣기(0x13cc4c)
        int k = 0, id = 0;
        sscanf(cmd + 6, "%d %d", &k, &id);
        fprintf(stderr, "[turn] 창고 %d/%d -> %u\n", k, id, CALL(0x13cc4c, CALL(0x13c6a4, k), k, id, 1));
        return true;
    }
    if (!strncmp(cmd, "drop ", 5)) { // turn drop 종류 id 수: 주인공 자리에 드랍 만들기(0x117d7c, 턴제면 바로 가방)
        int k = 0, id = 0, n = 1;
        sscanf(cmd + 5, "%d %d %d", &k, &id, &n);
        uint32_t h = CALL(0x147434);
        for (int i = 0; i < n; i++) CALL(0x117d7c, R32(h + 8), R32(h + 0xc), (uint32_t)k, (uint32_t)id);
        return true;
    }
    if (!strncmp(cmd, "pouch ", 6)) { // turn pouch 종류 id 수: 허리주머니에 넣기(0x13b52c, bag.c)
        int k = 0, id = 0, n = 1;
        sscanf(cmd + 6, "%d %d %d", &k, &id, &n);
        uint32_t r = CALL(0x13b52c, CALL(0x128f30), k, id, n);
        fprintf(stderr, "[turn] 허리주머니 %d/%d x%d -> %u\n", k, id, n, r);
        return true;
    }
    if (!strncmp(cmd, "count ", 6)) { // turn count 종류 id: 조건 2와 같은 가방 수량(0x12b4d0)
        int k = 0, id = 0;
        sscanf(cmd + 6, "%d %d", &k, &id);
        fprintf(stderr, "[turn] 가방 %d/%d = %d\n", k, id, (int)CALL(0x12b4d0, 0, k, id));
        return true;
    }
    if (!strncmp(cmd, "ents", 4)) { // turn ents [GOT 오프셋 16진]: 엔티티 목록(기본 GOT 0x14, 드랍은 0x15c). 종류, 위치
        uint32_t l = got(cmd[4] ? (uint32_t)strtoul(cmd + 5, NULL, 16) : 0x14), n = R32(l);
        for (int i = 0; n && i < 200; i++, n = R32(n + 4)) {
            uint32_t e = R32(n + 0x10);
            if (e) fprintf(stderr, "[ent] 종류 %d %08x 위치 %d,%d +20 %d +22 %d +24 %d\n", (int8_t)R8(n + 0x14), e, (int)R32(e + 8), (int)R32(e + 0xc), (int16_t)R16(e + 0x20), (int16_t)R16(e + 0x22), R8(e + 0x24));
        }
        return true;
    }
    if (!strncmp(cmd, "texts", 5) || !strncmp(cmd, "say ", 4)) { // turn texts: 지금 맵 대사 중 턴제로 바꾼 글(파티추가, 스테이터스) 번호. turn say N: 그 글을 대사 창으로(0x11bde0)
        uint32_t t = R32(R32(0x136d30) + 0x136cda + R32(0x136d34));
        int n = (int)R32(t + 0x2c);
        if (cmd[0] == 's') { CALL(0x11bde0, (uint32_t)atoi(cmd + 4), 0, 0); return true; }
        for (int i = 0; i < n; i++) {
            uint32_t len, h = CALL(0x136ccc, (uint32_t)i, 0);
            (void)len;
            if (!h) continue;
            const char *txt = (const char *)c->m + R32(h) + 8;
            if (strstr(txt, "\xc6\xc4\xc6\xbc\xc3\xdf\xb0\xa1") || strstr(txt, "\xbd\xba\xc5\xd7\xc0\xcc\xc5\xcd\xbd\xba")) // 파티추가 / 스테이터스
                fprintf(stderr, "[turn] 글 %d/%d: %.60s\n", i, n, txt);
        }
        return true;
    }
    if (!strncmp(cmd, "ptoggle ", 8)) { // turn ptoggle 동료: 면담의 파티추가/파티제외와 같다
        const char *r;
        bool ok = turn_party_toggle(atoi(cmd + 8), &r);
        int p[3], n = turn_party_members(p);
        fprintf(stderr, "[turn] 파티 바꾸기 %s, 전투 파티 %d명:", ok ? "됨" : "안 됨", n);
        for (int k = 0; k < n; k++) fprintf(stderr, " %d", p[k]);
        fprintf(stderr, " / 합류:");
        for (int i = 0; i < 8; i++) if (turn_member_joined(i)) fprintf(stderr, " %d", i);
        fprintf(stderr, "\n");
        return true;
    }
    if (!strncmp(cmd, "peek ", 5)) { // turn peek 주소 길이(16진). gNN이면 GOT+NN이 가리키는 값부터
        const char *a = cmd + 5;
        uint32_t addr = a[0] == 'g' ? got((uint32_t)strtoul(a + 1, NULL, 16)) : (uint32_t)strtoul(a, NULL, 16);
        uint32_t n = (uint32_t)strtoul(strchr(a, ' ') ? strchr(a, ' ') + 1 : "40", NULL, 16);
        fprintf(stderr, "[peek] %08x:", addr);
        for (uint32_t i = 0; i < n; i++) fprintf(stderr, "%s%02x", i % 16 ? " " : "\n[peek] ", R8(addr + i));
        fprintf(stderr, "\n");
        return true;
    }
    if (!strcmp(cmd, "st")) { // 상태 한 줄: 맵, 주인공 칸, 바쁨, 전투, 퀘스트(메인 0~34)
        uint32_t h = CALL(0x147434);
        fprintf(stderr, "[st] 맵 %d 칸 %d,%d busy %d run %d battle %d hp %d Q", (int)R32(got(GOT_WORLD) + WORLD_MAP), (int)R32(h + 8) >> 4, (int)R32(h + 0xc) >> 4, busy(), R8(SCRIPT_RUNNING), turn_battle_open(), (int)R32(h + 0xec));
        for (int i = 0; i <= 34; i++) fprintf(stderr, "%d", R8(SCRIPT_QUESTS + i));
        fprintf(stderr, " q:");
        for (int i = 35; i < 256; i++) if (R8(SCRIPT_QUESTS + i)) fprintf(stderr, " %d=%d", i, R8(SCRIPT_QUESTS + i));
        fprintf(stderr, " F:");
        for (int i = 0; i < 256; i++) if (R8(SCRIPT_FLAGS + i)) fprintf(stderr, " %d", i);
        fprintf(stderr, "\n");
        return true;
    }
    if (!strncmp(cmd, "quest ", 6)) { jump(atoi(cmd + 6)); return true; }
    if (!strncmp(cmd, "warp ", 5)) { // turn warp 맵 칸x 칸y
        sscanf(cmd + 5, "%d %d %d", &warp_to[0], &warp_to[1], &warp_to[2]);
        warp_pending = true; // 장면이나 안내 창 도중에 옮기면 원본이 멈춘다. 스크립트가 끝난 뒤에 옮긴다
        return true;
    }
    if (!strncmp(cmd, "spawnboss ", 10)) { // turn spawnboss EMD: 지역 보스 기록을 바꾸고 원본 보스 생성(0x140a64)을 부른다
        W16(got(0x194), (uint16_t)atoi(cmd + 10));
        W8(got(0x194) + 0xc, 0);
        CALL(0x140a64, 0);
        uint32_t b = R32(got(0x13c));
        if (b && strstr(cmd, " here")) { // 시험: 지금 맵의 주인공 옆으로 옮긴다
            uint32_t h = CALL(0x147434);
            W16(b + 0x194, (uint16_t)R32(got(GOT_WORLD) + WORLD_MAP));
            W32(b + 8, R32(h + 8) - 48);
            W32(b + 0xc, R32(h + 0xc));
        }
        if (b) fprintf(stderr, "[turn] 보스 생성 %08x 맵 %d 위치 %d,%d HP %d\n", b, (int16_t)R16(b + 0x194), (int)R32(b + 8), (int)R32(b + 0xc), (int)R32(b + 0xec));
        return true;
    }
    if (!strncmp(cmd, "flag ", 5) || !strncmp(cmd, "qset ", 5)) { // turn flag N V(+0x6b4), turn qset N V(+0x7b4)
        int n = 0, v = 0;
        sscanf(cmd + 5, "%d %d", &n, &v);
        W8((cmd[0] == 'f' ? SCRIPT_FLAGS : SCRIPT_QUESTS) + (n & 255), v);
        return true;
    }
    if (!strncmp(cmd, "objective ", 10)) { // turn objective 퀘스트 EMD 수: 스크립트 명령 0x20과 같은 처치 목표 등록(0x128b64)
        int q = 0, mon = 0, n = 1;
        sscanf(cmd + 10, "%d %d %d", &q, &mon, &n);
        CALL(0x128b64, q, R32(SCRIPT), 99, mon, n);
        return true;
    }
    if (!strcmp(cmd, "areas")) { // 지역 표(*(GOT 0x16bc)) +0xc0 보스 id, +0xd4 확률
        uint32_t t = got(0x16bc);
        fprintf(stderr, "[turn] 지역 표 %08x 지금 지역 %d:", t, (int)R32(got(0x18c) + 0x138));
        for (int i = 0; t && i < 10; i++) fprintf(stderr, " [%d] %d/%d%%", i, (int16_t)R16(t + 0xc0 + 2 * i), R8(t + 0xd4 + i));
        fprintf(stderr, "\n");
        return true;
    }
    if (!strcmp(cmd, "mons")) { // 지금 몬스터 목록과 보스 슬롯
        uint32_t list = got(0x20), n = R32(list + 0xc8);
        for (uint32_t i = 0; i < n && i < 50; i++) {
            uint32_t m = R32(list + 4 * i);
            if (!m) continue;
            fprintf(stderr, "[turn] 몬스터 %u @%08x: %s EMD %d 등급 %d 맵 %d 위치 %d,%d HP %d 상태 %d\n", i, m, gstr(m + 0xd0), (int16_t)R16(m + 0xa8), R8(m + 0xb1), (int16_t)R16(m + 0x194), (int)R32(m + 8), (int)R32(m + 0xc), (int)R32(m + 0xec), R8(m + 0xac));
        }
        fprintf(stderr, "[turn] 보스 슬롯 %08x, 지역 보스 %d\n", R32(got(0x13c)), (int16_t)R16(got(0x194)));
        return true;
    }
    if (!strcmp(cmd, "quests")) {
        fprintf(stderr, "[turn] 퀘스트:");
        for (int i = 0; i < 256; i++) if (R8(SCRIPT_QUESTS + i)) fprintf(stderr, " %d=%d", i, R8(SCRIPT_QUESTS + i));
        fprintf(stderr, "\n[turn] 맵 %d\n", (int)R32(got(GOT_WORLD) + WORLD_MAP));
        return true;
    }
    return false;
}
