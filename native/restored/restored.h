// 읽는 C로 옮긴 함수들의 공통 도구.
#include "host.h"

#define CALL(addr, ...) guest_call((addr), (const uint32_t[]){__VA_ARGS__}, sizeof((const uint32_t[]){__VA_ARGS__}) / 4)

enum { GOT = 0x19c8d4, FN_FILL_RECT = 0x106998 };

static inline uint32_t got(uint32_t off) { cpu_t *c = &cpu; return R32(GOT + off); }
