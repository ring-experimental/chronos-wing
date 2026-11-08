// 턴제 전투 화면의 몬스터 상태 → 동작 표. 그리기는 원본 0x17ead0을 부른다(turnbattle.c draw_actor).
// 형식: EMD/C0(0x14e37c). 근거는 work/turn-plan.md "조사 결과".
#include "turn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *slurp(const char *path, long *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(n ? n : 1);
    if (fread(b, 1, n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *len = n;
    return b;
}

static int s16(const uint8_t *b) { return (int16_t)(b[0] | b[1] << 8); }

// EMD/C0: 머리 s16(항목 수 × 4) + 항목마다 방향 4개 × (동작, 방향, 겹쳐 그릴 효과 동작) 바이트 3개(0x14e37c)
bool ts_c0_load(ts_c0 *t, const char *game, int n) {
    memset(t, 0, sizeof *t);
    if (n < 0) return false;
    char path[1100];
    snprintf(path, sizeof path, "%s/EMD/C0/%03d", game, n);
    long len;
    uint8_t *b = slurp(path, &len);
    if (!b) return false;
    int cnt = s16(b) / 4;
    if (cnt > TS_MAX_STATES) cnt = TS_MAX_STATES;
    for (int e = 0; e < cnt && 2 + 12 * e + 12 <= len; e++)
        for (int d = 0; d < 4; d++) {
            const uint8_t *q = b + 2 + 12 * e + 3 * d;
            t->anim[e][d] = (int8_t)q[0];
            t->dir[e][d] = (int8_t)q[1];
            t->fx[e][d] = (int8_t)q[2];
        }
    t->count = cnt;
    free(b);
    return true;
}
