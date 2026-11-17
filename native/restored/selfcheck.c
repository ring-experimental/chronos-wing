// 읽는 C로 옮긴 함수의 자체 검사. 같은 레지스터와 메모리에서 읽는 C와 원본을 차례로 실행하고 게스트 메모리를 견준다.
// 원본이 쓰고 버리는 스택(들어올 때의 SP 아래)은 견주지 않는다. 검사 뒤에는 원본의 결과를 남긴다.
// 갈래(variant)마다 처음 FIRST번은 모두, 그 뒤로는 EVERY번에 한 번 견준다(always면 매번). 종료할 때 갈래별 횟수와 불일치를 적는다.
#include "restored.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { FIRST = 40, EVERY = 400, SLOTS = 1024, STACK_SPAN = 0x80000, REPORT_LIMIT = 12 };

extern uint32_t heap_high;
typedef struct { const char *name; uint32_t variant; uint64_t calls, checked, bad; bool used; } slot_t;
static slot_t slots[SLOTS];
static uint8_t *before, *after;
static int reported;

int selfcheck_level(void) {
    static int level = -1;
    if (level < 0) {
        const char *v = getenv("CHRONOS_SELFCHECK");
        level = !v ? 0 : !strcmp(v, "sweep") ? 2 : 1;
    }
    return level;
}

static void summary(void) {
    uint64_t checked = 0, bad = 0;
    for (int i = 0; i < SLOTS; i++)
        if (slots[i].used) {
            fprintf(stderr, "자체 검사 %s 갈래 %#x: 호출 %llu, 견줌 %llu, 불일치 %llu\n", slots[i].name, slots[i].variant,
                    (unsigned long long)slots[i].calls, (unsigned long long)slots[i].checked, (unsigned long long)slots[i].bad);
            checked += slots[i].checked;
            bad += slots[i].bad;
        }
    fprintf(stderr, "자체 검사 합계: 견줌 %llu, 불일치 %llu\n", (unsigned long long)checked, (unsigned long long)bad);
}

static slot_t *find(const char *name, uint32_t variant) {
    static bool hooked;
    if (!hooked) { hooked = true; atexit(summary); }
    uint32_t i = (variant * 2654435761u ^ (uint32_t)(uintptr_t)name) % SLOTS;
    while (slots[i].used && (slots[i].name != name || slots[i].variant != variant)) i = (i + 1) % SLOTS;
    slots[i].used = true;
    slots[i].name = name;
    slots[i].variant = variant;
    return &slots[i];
}

void selfcheck_run(cpu_t *c, const char *name, uint32_t variant, bool always, void (*native)(cpu_t *), void (*orig)(cpu_t *)) {
    slot_t *s = find(name, variant);
    if (s->calls++ >= FIRST && s->calls % EVERY && !always) { native(c); return; }
    uint32_t lo = GUEST_LOW, hi = heap_high, sp = c->SP;
    uint32_t dead_lo = sp - STACK_SPAN;
    size_t n = hi - lo;
    if (!before) { before = malloc(GUEST_MEM - GUEST_LOW); after = malloc(GUEST_MEM - GUEST_LOW); }
    uint32_t regs[16], args[12];
    memcpy(regs, c->r, sizeof regs);
    for (int i = 0; i < 12; i++) args[i] = R32(sp + i * 4);
    memcpy(before, c->m + lo, n);
    native(c);
    if (heap_high != hi) { fprintf(stderr, "자체 검사 %s: 검사 중 힙이 늘어 견줄 수 없음\n", name); return; }
    memcpy(after, c->m + lo, n);
    memcpy(c->m + lo, before, n);
    memcpy(c->r, regs, sizeof regs);
    orig(c);
    s->checked++;
    const uint8_t *now = c->m + lo;
    bool same = !memcmp(now, after, dead_lo - lo) && !memcmp(now + (sp - lo), after + (sp - lo), hi - sp);
    if (same) return;
    s->bad++;
    if (reported++ >= REPORT_LIMIT) return;
    uint32_t first = 0, count = 0;
    for (uint32_t a = lo; a < hi; a++) {
        if (a >= dead_lo && a < sp) continue;
        if (now[a - lo] != after[a - lo]) { if (!count) first = a; count++; }
    }
    fprintf(stderr, "자체 검사 불일치 %s 갈래 %#x: %u바이트, 첫 주소 %#x (원본 %02x, 읽는 C %02x)\n  r0-3 %#x %#x %#x %#x 스택",
            name, variant, count, first, now[first - lo], after[first - lo], regs[0], regs[1], regs[2], regs[3]);
    for (int i = 0; i < 12; i++) fprintf(stderr, " %#x", args[i]);
    fprintf(stderr, "\n");
}
