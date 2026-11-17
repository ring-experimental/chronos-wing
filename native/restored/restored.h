// 읽는 C로 옮긴 함수들의 공통 도구.
#include "host.h"

#define CALL(addr, ...) guest_call((addr), (const uint32_t[]){__VA_ARGS__}, sizeof((const uint32_t[]){__VA_ARGS__}) / 4)

enum { GOT = 0x19c8d4, FN_FILL_RECT = 0x106998 };

static inline uint32_t got(uint32_t off) { cpu_t *c = &cpu; return R32(GOT + off); }

// 자체 검사(selfcheck.c). CHRONOS_SELFCHECK가 있으면 읽는 C와 원본을 같은 상태에서 실행해 메모리 결과를 견준다.
// 값이 sweep이면 그리기 함수는 실제 호출의 이미지로 방향, 크기, 혼합 값을 바꿔 가며 더 견준다.
int selfcheck_level(void);
void selfcheck_run(cpu_t *c, const char *name, uint32_t variant, bool always, void (*native)(cpu_t *), void (*orig)(cpu_t *));
