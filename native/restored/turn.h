// 턴제 모드 공통 선언.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 8비트 팔레트 그림. 팔레트 값 0은 투명(0xf81f도 읽을 때 0으로 바꾼다).
typedef struct {
    int w, h, stride, npal;
    uint16_t pal[256];
    uint8_t *px;
} ts_image;

typedef struct { uint8_t fl, img; int8_t dx, dy; uint8_t sx, sy, rot, alpha, ext; } ts_part;
typedef struct { int nparts; ts_part *parts; } ts_frame;
typedef struct { int nframes; ts_frame *frames; } ts_dir;
typedef struct { int ndirs; ts_dir *dirs; } ts_anim;

typedef struct {
    int nanims;
    ts_anim *anims;
    ts_image *sheet[4];
    int nsheet[4];
} ts_sprite;

enum { TS_MAX_STATES = 64 };
typedef struct {
    int count;
    int8_t anim[TS_MAX_STATES][4], dir[TS_MAX_STATES][4], fx[TS_MAX_STATES][4];
} ts_c0;

// 그릴 곳: RGB565 화면 버퍼. clip_y0~clip_y1 줄 안에만 그린다.
typedef struct {
    uint8_t *px;
    int w, h, bpl, clip_y0, clip_y1;
} ts_target;

bool ts_image_load(ts_image *im, const char *path);
void ts_image_free(ts_image *im);
bool ts_sprite_load(ts_sprite *s, const char *game, int ems, const char *sheet_paths[4], const int pal[4]);
bool ts_sprite_load_path(ts_sprite *s, const char *ems_path, const char *sheet_paths[4], const int pal[4], const char *game);
bool ts_monster_sprite(ts_sprite *s, const char *game, int ems, const int img[3], const int pal[3]);
void ts_sprite_free(ts_sprite *s);
bool ts_c0_load(ts_c0 *t, const char *game, int n);
int ts_frames(const ts_sprite *s, int anim, int dir);
void ts_draw(const ts_target *t, const ts_sprite *s, int anim, int dir, int frame, int fx, int fy, bool mirror, int alpha);

// 전투(turnbattle.c)
bool turn_battle_open(void);
void turn_battle_tick(void);
bool turn_battle_key(int type, int code);
bool turn_ctrl(const char *line);
