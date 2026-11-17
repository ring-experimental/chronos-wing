// 간접 호출, 원본 ARM 도우미, 타이머.
#include "host.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

cpu_t cpu;
static int nfn;
uint32_t host_last_call;
FILE *api_trace; // WIE `wipic call` 로그와 같은 형식(id, lr)

static int cmpf(const void *k, const void *e) {
    uint32_t a = *(const uint32_t *)k, b = ((const fn_entry_t *)e)->addr;
    return a < b ? -1 : a > b;
}

void guest_fault(cpu_t *c, uint32_t a) {
    fprintf(stderr, "게스트 메모리 오류: pc %#x 주소 %#x, LR %#x, SP %#x, r0-3 %#x %#x %#x %#x, 마지막 간접 호출 %#x\n",
            c->pc, a, c->LR, c->SP, c->r[0], c->r[1], c->r[2], c->r[3], host_last_call);
    abort();
}

// 0x882f0 memset, 0x883a4 memcpy, 0x884dc 64비트 나눗셈의 C 대응. setjmp/longjmp는 게임 코드가 쓰지 않는다.
const uint32_t arm_helper_addrs[] = {0x1882f0, 0x1883a4, 0x1884dc, 0};
// 인터프리터 빌드가 채운다(interp/interp.c). 비어 있으면 번역 코드의 fn_table을 쓴다.
void (*guest_exec)(cpu_t *, uint32_t);

bool arm_helper(cpu_t *c, uint32_t a) {
    uint32_t *r = c->r;
    switch (a) {
    case 0x1882f0: CK(r[0], r[2]); memset(c->m + r[0], (int)r[1], r[2]); return true;
    case 0x1883a4: CK(r[0], r[2]); CK(r[1], r[2]); memmove(c->m + r[0], c->m + r[1], r[2]); return true;
    case 0x1884dc: {
        int64_t n = (int64_t)(((uint64_t)r[1] << 32) | r[0]), d = (int64_t)(((uint64_t)r[3] << 32) | r[2]);
        int64_t q = d ? n / d : 0, m = d ? n % d : n;
        r[0] = (uint32_t)q; r[1] = (uint32_t)((uint64_t)q >> 32);
        r[2] = (uint32_t)m; r[3] = (uint32_t)((uint64_t)m >> 32);
        return true;
    }
    }
    return false;
}

void call_ind(cpu_t *c, uint32_t t) {
    host_last_call = t;
    if (t >= API_BASE) {
        uint32_t table = (t >> 16) & 0xfff, index = (t & 0xffff) >> 1;
        if (api_trace) fprintf(api_trace, "%#x %#x\n", table << 16 | index, c->LR);
        api_dispatch(c, table, index);
        return;
    }
    if (arm_helper(c, t & ~1u)) return;
    if (guest_exec) { guest_exec(c, t); return; }
    if (!nfn) while (fn_table[nfn].addr) nfn++;
    uint32_t a = t & ~1u;
    const fn_entry_t *e = bsearch(&a, fn_table, nfn, sizeof(fn_entry_t), cmpf);
    if (!e) {
        fprintf(stderr, "번역되지 않은 코드로 간접 호출: pc %#x 대상 %#x, LR %#x, r0-7 %#x %#x %#x %#x %#x %#x %#x %#x, SP %#x, [SP] %#x\n", c->pc, t, c->LR,
                c->r[0], c->r[1], c->r[2], c->r[3], c->r[4], c->r[5], c->r[6], c->r[7], c->SP, R32(c->SP));
        abort();
    }
    e->fn(c);
}

uint64_t guest_ticks; // 게스트 코드(와 그 안의 API) 실행에 쓴 시간. 종료 때 main.c가 보고한다.

uint32_t guest_call(uint32_t addr, const uint32_t *args, int n) {
    cpu_t *c = &cpu;
    uint32_t saved_sp = c->SP, saved_lr = c->LR;
    int extra = n > 4 ? n - 4 : 0;
    c->SP = (c->SP - extra * 4) & ~7u;
    for (int i = 0; i < n; i++) {
        if (i < 4) c->r[i] = args[i];
        else W32(c->SP + (i - 4) * 4, args[i]);
    }
    c->LR = 0xfffffffe;
    static int depth;
    uint64_t t0 = depth++ ? 0 : SDL_GetPerformanceCounter();
    call_ind(c, addr | 1);
    if (!--depth) guest_ticks += SDL_GetPerformanceCounter() - t0;
    c->SP = saved_sp;
    c->LR = saved_lr;
    return c->r[0];
}

// 타이머
typedef struct { uint32_t timer, cb, a0, a1; uint64_t due, seq; bool defer; bool used; } pend_t;
static pend_t pend[256];
static uint64_t seq;

// CHRONOS_VCLOCK: 대조용 가상 시계. 메인 루프가 기다릴 만큼 건너뛰고(vclock_skip) 타이머 콜백마다 1ms 흐른다. 실행 속도와 무관하게 같은 순서로 돈다.
static uint64_t vnow;
static int vclock = -1;
bool vclock_on(void) { if (vclock < 0) vclock = getenv("CHRONOS_VCLOCK") != NULL; return vclock; }
void vclock_skip(uint64_t ms) { vnow += ms; }
uint64_t now_ms(void) { return vclock_on() ? vnow : SDL_GetTicks(); }

void timer_unset(uint32_t timer) {
    for (int i = 0; i < 256; i++)
        if (pend[i].used && !pend[i].defer && pend[i].timer == timer) pend[i].used = false;
}

void timer_def(uint32_t timer, uint32_t cb) {
    timer_unset(timer);
    W32_(&cpu, timer, cb);
}

static pend_t *pend_new(void) {
    for (int i = 0; i < 256; i++)
        if (!pend[i].used) { pend[i].used = true; pend[i].seq = seq++; return &pend[i]; }
    fprintf(stderr, "타이머 슬롯 부족\n");
    abort();
}

int timer_set(uint32_t timer, uint64_t ms, uint32_t param) {
    for (int i = 0; i < 256; i++)
        if (pend[i].used && !pend[i].defer && pend[i].timer == timer) return -5;
    pend_t *p = pend_new();
    p->defer = false;
    p->timer = timer;
    p->cb = R32_(&cpu, timer);
    p->a0 = timer;
    p->a1 = param;
    p->due = now_ms() + ms;
    return 0;
}

void defer_call(uint32_t cb, uint32_t a0, uint32_t a1, uint64_t ms) {
    pend_t *p = pend_new();
    p->defer = true;
    p->timer = 0;
    p->cb = cb;
    p->a0 = a0;
    p->a1 = a1;
    p->due = now_ms() + ms;
}

void timers_run(void) {
    uint64_t now = now_ms();
    for (;;) {
        pend_t *best = NULL;
        for (int i = 0; i < 256; i++)
            if (pend[i].used && pend[i].due <= now && (!best || pend[i].due < best->due || (pend[i].due == best->due && pend[i].seq < best->seq)))
                best = &pend[i];
        if (!best) return;
        uint32_t args[2] = {best->a0, best->a1}, cb = best->cb;
        best->used = false;
        guest_call(cb, args, 2);
        if (vclock_on()) vnow++;
    }
}

int64_t timers_next_ms(void) {
    int64_t best = -1, now = (int64_t)now_ms();
    for (int i = 0; i < 256; i++)
        if (pend[i].used) {
            int64_t d = (int64_t)pend[i].due - now;
            if (d < 0) d = 0;
            if (best < 0 || d < best) best = d;
        }
    return best;
}
