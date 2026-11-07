// 턴제 전투 화면의 캐릭터 그리기. 게임 데이터 파일을 직접 읽어 화면 버퍼(RGB565)에 그린다.
// 형식: 그림(머리 9바이트 + 팔레트 + 화소, 0x109854), 시트 묶음 EMI(0x12c08c), 동작 묶음 EMS(0x17e078),
// 팔레트 바꾸기 EMP(0x173aec), 상태 → 동작 표 EMD/C0(0x14e37c). 근거는 work/turn-plan.md "조사 결과".
#include "turn.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { KEY565 = 0xf81f, MAX_PARTS = 64 };

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

static int u16(const uint8_t *b) { return b[0] | b[1] << 8; }
static int s16(const uint8_t *b) { return (int16_t)(b[0] | b[1] << 8); }

static bool image_parse(ts_image *im, const uint8_t *b, long n) {
    memset(im, 0, sizeof *im);
    if (n < 9) return false;
    int kind = b[0];
    im->stride = u16(b + 1); im->h = u16(b + 3); im->w = u16(b + 5); im->npal = u16(b + 7);
    int bits = kind == 11 ? 4 : kind == 12 ? 8 : 0;
    long body = (long)im->stride * im->h * bits / 8;
    if (!bits || im->npal > 256 || 9 + 2L * im->npal + body > n || im->w > im->stride) return false;
    for (int i = 0; i < im->npal; i++) {
        uint16_t p = (uint16_t)u16(b + 9 + 2 * i);
        im->pal[i] = p == KEY565 ? 0 : p; // 0x109854: 0xf81f를 0으로 바꾸고 0을 투명으로 친다
    }
    const uint8_t *src = b + 9 + 2 * im->npal;
    long np = (long)im->stride * im->h;
    im->px = malloc(np);
    if (bits == 8) memcpy(im->px, src, np);
    else for (long i = 0; i < np; i++) im->px[i] = i & 1 ? src[i >> 1] & 15 : src[i >> 1] >> 4;
    return true;
}

bool ts_image_load(ts_image *im, const char *path) {
    long n;
    uint8_t *b = slurp(path, &n);
    if (!b) { memset(im, 0, sizeof *im); return false; }
    bool ok = image_parse(im, b, n);
    free(b);
    return ok;
}

void ts_image_free(ts_image *im) { free(im->px); memset(im, 0, sizeof *im); }

static int sheet_load(ts_image **out, const char *path) {
    *out = NULL;
    long n;
    uint8_t *b = slurp(path, &n);
    if (!b || n < 4) { free(b); return 0; }
    int cnt = u16(b) | u16(b + 2) << 16, k = 0;
    long o = 4;
    if (cnt <= 0 || cnt > 4096) { free(b); return 0; }
    *out = calloc(cnt, sizeof **out);
    for (; k < cnt && o + 4 <= n; k++) {
        long sz = (long)(u16(b + o) | u16(b + o + 2) << 16);
        if (o + 4 + sz > n || !image_parse(&(*out)[k], b + o + 4, sz)) break;
        o += 4 + sz;
    }
    free(b);
    return k;
}

// EMP: 첫 바이트가 0이 아니면 (원래 색, 바꿀 색) RGB888 쌍이 이어진다. 그림마다 처음 맞는 색 하나를 바꾼다.
static uint16_t rgb565(uint32_t c) { return (uint16_t)(((c >> 16 & 0xff) >> 3) << 11 | ((c >> 8 & 0xff) >> 2) << 5 | (c & 0xff) >> 3); }

static void apply_emp(ts_image *ims, int n, const char *path) {
    long len;
    uint8_t *b = slurp(path, &len);
    if (!b) return;
    if (len < 1 || !b[0]) { free(b); return; }
    for (long o = 1; o + 8 <= len; o += 8) {
        uint16_t from = rgb565(b[o] | b[o + 1] << 8 | (uint32_t)b[o + 2] << 16), to = rgb565(b[o + 4] | b[o + 5] << 8 | (uint32_t)b[o + 6] << 16);
        for (int k = 0; k < n; k++)
            for (int i = 0; i < ims[k].npal; i++)
                if (ims[k].pal[i] && ims[k].pal[i] == from) { ims[k].pal[i] = to; break; }
    }
    free(b);
}

static bool ems_load(ts_sprite *s, const char *path) {
    long n, o = 0;
    uint8_t *b = slurp(path, &n);
    if (!b || n < 2) { free(b); return false; }
#define NEED(k) if (o + (k) > n) goto bad
    o = 1 + b[0];
    NEED(1);
    s->nanims = b[o++];
    s->anims = calloc(s->nanims + 1, sizeof *s->anims);
    for (int a = 0; a < s->nanims; a++) {
        NEED(1);
        ts_anim *an = &s->anims[a];
        an->ndirs = b[o++];
        an->dirs = calloc(an->ndirs + 1, sizeof *an->dirs);
        for (int d = 0; d < an->ndirs; d++) {
            NEED(1);
            ts_dir *di = &an->dirs[d];
            di->nframes = b[o++];
            di->frames = calloc(di->nframes + 1, sizeof *di->frames);
            for (int f = 0; f < di->nframes; f++) {
                NEED(1);
                int np = (int8_t)b[o++];
                ts_frame *fr = &di->frames[f];
                fr->nparts = np > 0 ? np : 0;
                fr->parts = calloc(fr->nparts + 1, sizeof *fr->parts);
                for (int p = 0; p < fr->nparts; p++) {
                    NEED(4);
                    ts_part *pt = &fr->parts[p];
                    pt->fl = b[o]; pt->img = b[o + 1]; pt->dx = (int8_t)b[o + 2]; pt->dy = (int8_t)b[o + 3];
                    if (pt->fl & 0x80) {
                        NEED(9);
                        pt->sx = b[o + 4]; pt->sy = b[o + 5]; pt->rot = b[o + 6]; pt->alpha = b[o + 7]; pt->ext = b[o + 8];
                        o += 9;
                    } else o += 4;
                }
            }
        }
    }
#undef NEED
    free(b);
    return true;
bad:
    free(b);
    return false;
}

void ts_sprite_free(ts_sprite *s) {
    for (int a = 0; a < s->nanims; a++) {
        for (int d = 0; d < s->anims[a].ndirs; d++) {
            for (int f = 0; f < s->anims[a].dirs[d].nframes; f++) free(s->anims[a].dirs[d].frames[f].parts);
            free(s->anims[a].dirs[d].frames);
        }
        free(s->anims[a].dirs);
    }
    free(s->anims);
    for (int i = 0; i < 4; i++) {
        for (int k = 0; k < s->nsheet[i]; k++) ts_image_free(&s->sheet[i][k]);
        free(s->sheet[i]);
    }
    memset(s, 0, sizeof *s);
}

bool ts_sprite_load(ts_sprite *s, const char *game, int ems, const char *sheet_paths[4], const int pal[4]) {
    char path[1100];
    snprintf(path, sizeof path, "%s/EMS/%03d", game, ems);
    return ts_sprite_load_path(s, path, sheet_paths, pal, game);
}

bool ts_sprite_load_path(ts_sprite *s, const char *ems_path, const char *sheet_paths[4], const int pal[4], const char *game) {
    memset(s, 0, sizeof *s);
    char path[1100];
    if (!ems_load(s, ems_path)) { fprintf(stderr, "[turn] %s 읽기 실패\n", ems_path); ts_sprite_free(s); return false; }
    for (int i = 0; i < 4; i++) {
        if (!sheet_paths[i]) continue;
        s->nsheet[i] = sheet_load(&s->sheet[i], sheet_paths[i]);
        if (pal && pal[i] >= 0 && s->nsheet[i]) {
            snprintf(path, sizeof path, "%s/EMP/%03d", game, pal[i]);
            apply_emp(s->sheet[i], s->nsheet[i], path);
        }
    }
    return true;
}

// 몬스터(EMD/M 레코드의 묶음, 시트 3장, 팔레트 3개)
bool ts_monster_sprite(ts_sprite *s, const char *game, int ems, const int img[3], const int pal[3]) {
    char p[3][1100];
    const char *paths[4] = {0};
    int pals[4] = {-1, -1, -1, -1};
    for (int i = 0; i < 3; i++) {
        if (img[i] < 0) continue;
        snprintf(p[i], sizeof p[i], "%s/EMI/%03d/%03d", game, i, img[i]);
        paths[i] = p[i];
        pals[i] = pal[i];
    }
    return ts_sprite_load(s, game, ems, paths, pals);
}

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

static inline uint16_t blend565(uint16_t dst, uint16_t src, int a) {
    int r = ((src >> 11) * a + (dst >> 11) * (255 - a)) / 255;
    int g = ((src >> 5 & 63) * a + (dst >> 5 & 63) * (255 - a)) / 255;
    int b = ((src & 31) * a + (dst & 31) * (255 - a)) / 255;
    return (uint16_t)(r << 11 | g << 5 | b);
}

static inline uint16_t add565(uint16_t dst, uint16_t src) {
    int r = (dst >> 11) + (src >> 11), g = (dst >> 5 & 63) + (src >> 5 & 63), b = (dst & 31) + (src & 31);
    return (uint16_t)((r > 31 ? 31 : r) << 11 | (g > 63 ? 63 : g) << 5 | (b > 31 ? 31 : b));
}

// 부품 하나. 확장 부품(플래그 0x80): 배율 = 값×4 %, 회전 = 값×2°, 투명도, 마지막 바이트 아래 2비트 뒤집기(1 좌우, 2·3 상하),
// 위 6비트 그리기 방식(7은 더하기로 본다, 추정). mirror면 발을 축으로 좌우를 뒤집는다(방향 3 = 방향 2 반전, 0x17ead0).
// 방향 2가 오른쪽을 본다(원작 필드 화면의 주인공 무기 위치로 확인).
static void put_part(const ts_target *t, int fx, int fy, bool mirror, const ts_image *im, const ts_part *pt, int alpha_all) {
    bool ext = pt->fl & 0x80;
    float sx = ext ? pt->sx * 4 / 100.0f : 1, sy = ext ? pt->sy * 4 / 100.0f : 1;
    if (sx <= 0 || sy <= 0) return;
    float ang = ext ? pt->rot * 2 * 3.14159265f / 180 : 0;
    // 기본 부품은 플래그 (fl >> 5) & 3이 뒤집기다: 1 좌우, 2·3 상하(0x17ebda → 0x17f354~0x17f360). 반전 방향에서는 좌우를 한 번 더 뒤집는다(0x17f366).
    int bits = pt->fl >> 5 & 3;
    int flip = ext ? ((pt->ext & 3) == 1 ? 1 : (pt->ext & 3) ? 2 : 0) : bits == 1 ? 1 : bits ? 2 : 0, mode = ext ? pt->ext >> 2 : 0;
    int alpha = ext ? pt->alpha : 255;
    alpha = alpha * alpha_all / 255;
    if (alpha <= 0) return;
    // 원본은 반전 방향에서 dx 부호만 바꾸고 뒤집기를 바꾼다(0x17f366). 그래서 뒤집힌 그림은 dx가 오른쪽 끝, 상하 뒤집힌 그림은 dy가 아래 끝이다.
    float left = pt->dx, top = pt->dy;
    if (!ext && (flip & 1)) left -= im->w;
    if (!ext && (flip & 2)) top -= im->h;
    float px = left + im->w * sx / 2, py = fy + top + im->h * sy / 2;
    if (mirror) { px = fx - px; flip ^= 1; ang = -ang; }
    else px = fx + px;
    float ca = cosf(ang), sa = sinf(ang);
    float r = 0.75f * (im->w * sx + im->h * sy) + 2;
    int y0 = (int)(py - r), y1 = (int)(py + r), x0 = (int)(px - r), x1 = (int)(px + r);
    if (y0 < t->clip_y0) y0 = t->clip_y0;
    if (y1 >= t->clip_y1) y1 = t->clip_y1 - 1;
    if (x0 < 0) x0 = 0;
    if (x1 >= t->w) x1 = t->w - 1;
    bool plain = !ext || (pt->rot == 0 && sx == 1 && sy == 1);
    for (int Y = y0; Y <= y1; Y++) {
        uint16_t *row = (uint16_t *)(t->px + (size_t)Y * t->bpl);
        for (int X = x0; X <= x1; X++) {
            float dx = X + 0.5f - px, dy = Y + 0.5f - py;
            float u, v;
            if (plain) { u = dx + im->w / 2.0f; v = dy + im->h / 2.0f; }
            else { u = (ca * dx + sa * dy) / sx + im->w / 2.0f; v = (-sa * dx + ca * dy) / sy + im->h / 2.0f; }
            int iu = (int)floorf(u), iv = (int)floorf(v);
            if (iu < 0 || iv < 0 || iu >= im->w || iv >= im->h) continue;
            if (flip & 1) iu = im->w - 1 - iu;
            if (flip & 2) iv = im->h - 1 - iv;
            uint16_t c = im->pal[im->px[iv * im->stride + iu]];
            if (!c) continue;
            if (mode == 7) row[X] = add565(row[X], alpha < 255 ? blend565(0, c, alpha) : c);
            else row[X] = alpha < 255 ? blend565(row[X], c, alpha) : c;
        }
    }
}

int ts_frames(const ts_sprite *s, int anim, int dir) {
    if (anim < 0 || anim >= s->nanims || !s->anims[anim].ndirs) return 0;
    if (dir >= s->anims[anim].ndirs) dir = 0;
    return s->anims[anim].dirs[dir].nframes;
}

// 동작·방향·프레임을 발 위치 (fx, fy)에 그린다. 방향이 없는 동작은 방향 0(정면)을 쓴다.
void ts_draw(const ts_target *t, const ts_sprite *s, int anim, int dir, int frame, int fx, int fy, bool mirror, int alpha) {
    if (anim < 0 || anim >= s->nanims) return;
    const ts_anim *an = &s->anims[anim];
    if (!an->ndirs) return;
    if (dir >= an->ndirs) { dir = 0; }
    const ts_dir *di = &an->dirs[dir];
    if (!di->nframes) return;
    if (frame >= di->nframes) frame = di->nframes - 1;
    if (frame < 0) frame = 0;
    const ts_frame *fr = &di->frames[frame];
    for (int p = 0; p < fr->nparts; p++) {
        const ts_part *pt = &fr->parts[p];
        int sh = pt->fl & 0x1f;
        if (sh > 3 || pt->img >= s->nsheet[sh]) continue;
        put_part(t, fx, fy, mirror, &s->sheet[sh][pt->img], pt, alpha);
    }
}
