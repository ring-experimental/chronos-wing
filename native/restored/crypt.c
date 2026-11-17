// 세이브와 데이터 파일 암호(DES)의 16라운드(0x105b8c). 비트 하나를 32비트 칸 하나에 담은 배열로 계산한다.
// 호출하는 쪽(0x105a00대)이 8바이트 블록을 비트 배열로 풀고(0x105d6c) 다시 묶는다(0x105db8).
// 표는 GOT의 핸들 변수에 있고 데이터는 핸들 머리 8바이트 뒤다.
#include "restored.h"

void orig_f_5b8c(cpu_t *c);

enum {
    GOT_EXPAND = 0x94, GOT_PERMUTE = 0x98, GOT_SBOX = 0x9c, GOT_ROUND_KEYS = 0xa8, TABLE_DATA = 8,
    HALF_BITS = 32, WIDE_BITS = 48, ROUNDS = 16, BOXES = 8, BOX_IN = 6, BOX_OUT = 4,
};

static uint32_t table(uint32_t off) {
    cpu_t *c = &cpu;
    return R32(R32(got(off))) + TABLE_DATA;
}

// r0 = 왼쪽 절반, r1 = 오른쪽 절반(각 32칸). 라운드마다 오른쪽을 넓혀 키와 섞고 S-box, 순열을 거쳐 왼쪽과 XOR한 뒤 두 절반을 맞바꾼다.
static void des_rounds(cpu_t *c) {
    uint32_t left = c->r[0], right = c->r[1];
    uint32_t expand = table(GOT_EXPAND), permute = table(GOT_PERMUTE), sbox = table(GOT_SBOX), keys = table(GOT_ROUND_KEYS);
    for (int round = 0; round < ROUNDS; round++) {
        uint32_t old[HALF_BITS], mixed[WIDE_BITS], out[HALF_BITS], f[HALF_BITS];
        for (int i = 0; i < HALF_BITS; i++) old[i] = R32(right + i * 4);
        for (int i = 0; i < WIDE_BITS; i++) mixed[i] = R32(right + R8(expand + i) * 4 - 4) ^ R8(keys + round * WIDE_BITS + i);
        for (int b = 0; b < BOXES; b++) {
            const uint32_t *p = mixed + b * BOX_IN;
            uint32_t v = R8(sbox + (b * 4 + p[0] * 2 + p[5]) * 16 + ((p[1] * 2 + p[2]) * 2 + p[3]) * 2 + p[4]);
            for (int k = 0; k < BOX_OUT; k++) out[b * BOX_OUT + k] = (v >> (BOX_OUT - 1 - k)) & 1;
        }
        for (int i = 0; i < HALF_BITS; i++) f[i] = out[R8(permute + i) - 1];
        for (int i = 0; i < HALF_BITS; i++) W32(right + i * 4, R32(left + i * 4) ^ f[i]);
        for (int i = 0; i < HALF_BITS; i++) W32(left + i * 4, old[i]);
    }
}

void f_5b8c(cpu_t *c) {
    // 순열 표가 1~32를 벗어나면 원본은 스택의 다른 값을 읽는다. 그 경우는 원본에 맡긴다.
    uint32_t permute = table(GOT_PERMUTE);
    for (int i = 0; i < HALF_BITS; i++)
        if ((uint32_t)(R8(permute + i) - 1) >= HALF_BITS) { orig_f_5b8c(c); return; }
    if (selfcheck_level()) selfcheck_run(c, "des_rounds", 0, false, des_rounds, orig_f_5b8c);
    else des_rounds(c);
}
