// 메인메뉴 항목이 여는 창. 게임 상태값은 모두 개발자모드의 빈 상태 8을 함께 쓰고, devmode.c가 고른 창으로 나눈다.
#pragma once
#include "restored.h"

typedef struct {
    const char *label;          // 메인메뉴에 보일 EUC-KR 이름
    void (*open)(void);         // 상태 8에 들어온 직후
    void (*draw)(int x, int y); // 0xb0 x 0xa0 창틀의 왼쪽 위. 창틀은 각 창이 그린다.
    void (*key)(int key);       // 메뉴 애니메이션이 멈춰 있을 때 누른 키
    void (*tick)(void);         // 매 프레임. 없어도 된다.
    void (*close)(void);        // 닫힘 애니메이션이 끝난 뒤. 없어도 된다.
} menu_panel;

extern const menu_panel music_panel;
extern const menu_panel replay_panel;
extern const menu_panel turn_panel;

uint32_t menu_guest_text(const char *s); // 고정 문자열만 넘긴다. 게스트 메모리에 한 번 복사해 계속 쓴다.
uint32_t menu_color(int r, int g, int b);
void menu_leave(void);                   // 닫기 시작
void menu_draw_frame(int x, int y);      // 원본 창틀
