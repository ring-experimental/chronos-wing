// 게스트 힙. 주소순 빈 목록, 인접 블록 병합. 머리 8바이트에 크기를 둔다.
#include "host.h"

static uint32_t free_head;
#define HDR 8u

void heap_init(void) {
    cpu_t *c = &cpu;
    free_head = HEAP_START;
    W32(HEAP_START, GUEST_MEM - HEAP_START);
    W32(HEAP_START + 4, 0);
}

uint32_t heap_alloc(uint32_t size) {
    cpu_t *c = &cpu;
    uint32_t need = (size + HDR + 7) & ~7u, prev = 0, p = free_head;
    while (p) {
        uint32_t sz = R32(p), next = R32(p + 4);
        if (sz >= need) {
            uint32_t rest = sz - need, follow = next;
            if (rest >= 16) {
                follow = p + need;
                W32(follow, rest);
                W32(follow + 4, next);
            } else {
                need = sz;
            }
            if (prev) W32(prev + 4, follow); else free_head = follow;
            W32(p, need);
            W32(p + 4, 0x4c4c4f43);
            memset(c->m + p + HDR, 0, need - HDR);
            return p + HDR;
        }
        prev = p;
        p = next;
    }
    return 0;
}

void heap_free(uint32_t addr) {
    cpu_t *c = &cpu;
    if (!addr) return;
    uint32_t b = addr - HDR, sz = R32(b), prev = 0, p = free_head;
    while (p && p < b) { prev = p; p = R32(p + 4); }
    if (p && b + sz == p) { sz += R32(p); p = R32(p + 4); }
    W32(b, sz);
    W32(b + 4, p);
    if (prev) {
        if (prev + R32(prev) == b) { W32(prev, R32(prev) + sz); W32(prev + 4, p); }
        else W32(prev + 4, b);
    } else {
        free_head = b;
    }
}

// WIPI 메모리 핸들: [h]=h+4, [h+4]=크기, 데이터는 h+12.
uint32_t handle_alloc(uint32_t size) {
    cpu_t *c = &cpu;
    uint32_t h = heap_alloc(size + 12);
    if (!h) return 0;
    W32(h, h + 4);
    W32(h + 4, size);
    return h;
}

void handle_free(uint32_t h) { heap_free(h); }
