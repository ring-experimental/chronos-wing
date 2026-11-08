// 턴제 모드 공통 선언.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { TS_MAX_STATES = 64 };
typedef struct {
    int count;
    int8_t anim[TS_MAX_STATES][4], dir[TS_MAX_STATES][4], fx[TS_MAX_STATES][4];
} ts_c0;

bool ts_c0_load(ts_c0 *t, const char *game, int n);

// 전투(turnbattle.c)
bool turn_battle_open(void);
void turn_battle_tick(void);
bool turn_battle_key(int type, int code);
bool turn_ctrl(const char *line);

// 시험 도구(turntest.c)
void turn_test_tick(void);
bool turn_test_ctrl(const char *cmd);
