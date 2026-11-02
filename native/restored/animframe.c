// 애니메이션 프레임 표 읽기(0x14fa10). 원본은 범위를 검사하지 않는다.
// 전환 표의 번호에는 프레임이 아니라 행동을 뜻하는 값도 있다(0x14a598은 7 또는 9를 빼서 +0x1a4 행동 표를 찾는다).
// 해적폭탄병EX는 프레임이 8칸인데 번호 8, 9를 쓴다. 0x14a598은 분기하기 전에 프레임을 먼저 읽으므로 표 밖을 읽어
// 보호 페이지에서 멈춘다. 표 밖이면 0으로 찬 프레임을 돌려준다.
#include "restored.h"
#include <stdio.h>
#include <stdlib.h>

void orig_f_4fa10(cpu_t *c);

enum { ZERO_FRAME_SIZE = 512, LOG_LIMIT = 8 };

static bool in_guest(uint32_t a) { return a >= GUEST_LOW && a <= GUEST_MEM - 4; }

void f_4fa10(cpu_t *c) {
    static uint32_t zero_frame;
    uint32_t table = c->r[0];
    int index = (int)c->r[1];
    uint32_t array = in_guest(table) ? R32(table) : 0;
    uint32_t frame = 0;
    // 배열 머리는 [+0] 바이트 길이, [+8]부터 항목이다.
    if (in_guest(array) && index >= 0 && (uint32_t)index * 4 < R32(array)) frame = R32(array + 8 + (uint32_t)index * 4);
    if (in_guest(frame) && in_guest(R32(frame))) { orig_f_4fa10(c); return; }
    static int logged;
    if (logged < LOG_LIMIT) {
        logged++;
        fprintf(stderr, "프레임 표 밖 읽기: 표 %#x, 번호 %d, 호출 %#x\n", table, index, c->LR);
    }
    if (!zero_frame) {
        zero_frame = heap_alloc(ZERO_FRAME_SIZE);
        if (zero_frame) W32(zero_frame, zero_frame + 4); // 항목은 데이터 주소를 가리킨다.
    }
    if (!zero_frame) { orig_f_4fa10(c); return; }
    c->r[1] = (uint32_t)index * 4 + array;
    c->r[3] = zero_frame;
    c->r[0] = zero_frame + 4 + 8;
}
