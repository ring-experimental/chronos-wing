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

// 공통 그리기(turnbattle.c)
bool turn_mode_on(void);
void turn_draw_text(int x, int y, int width, const char *text, uint32_t color, int center);
void turn_fill(int x, int y, int w, int h, int r, int g, int b);

// 드랍 바로 넣기와 획득 창(turnloot.c)
bool turn_loot_open(void);
bool turn_loot_tick(void);
bool turn_loot_key(int type, int code);
void turn_loot_expect(const uint32_t *mons, int n);

// 스킬과 허리주머니(turnskill.c)
enum { ST_NONE, ST_POISON, ST_SHOCK, ST_BURN, ST_GROGGY, ST_SNOW, ST_BLIND, ST_FART, ST_SLOW, ST_COUNT }; // 원작 상태 번호(0x14d908)
enum { TG_ENEMY, TG_ENEMIES, TG_ALLY, TG_ALLIES, TG_DEAD, TG_SELF };
enum { EF_HIT, EF_STATUS, EF_HEAL, EF_REVIVE, EF_CURE, EF_MP, EF_MPLOCK, EF_BUFF_ATK, EF_BUFF_DEF, EF_BUFF_SPD,
       EF_TAUNT, EF_COUNTER, EF_EVADE, EF_CHARGE, EF_POWERBURN };
enum { NEED_NONE, NEED_MP_FULL, NEED_POWERBURN };
enum { TS_MOTIONS = 8 };
typedef struct {
    char name[28];
    int mp, target, effect, power, magic, hits, pierce, status, chance, amount, turns, need, fx, ranged, level, unlock; // unlock: 동료가 배우는 레벨
    int motion[TS_MOTIONS], nmotion, mp_gain; // 주인공: H/DS 줄의 동작 id(피해는 원본 식)
} tskill;
typedef struct { uint32_t item; int slot, kind, id, count; char name[32]; } titem;

int turn_hero_skills(int cls, tskill *basic, tskill *out, int max);
int turn_member_skills(int member, int level, tskill *basic, tskill *out, int max);
const char *turn_status_name(int s);
int turn_pouch_items(titem *out, int max);
void turn_pouch_consume(const titem *t);
bool turn_item_effect(int kind, int *effect, int *hp, int *mp, int *status, int *turns);

// 파티 편성(turnparty.c)
bool turn_member_joined(int i);
int turn_party_members(int *out);
void turn_party_tick(void);
bool turn_party_toggle(int i, const char **result);
