// 화면 그리기의 안쪽 루프 세 개. 인터프리터 빌드에서 실행 명령의 절반 이상이 여기서 나와 읽는 C로 옮겼다.
// - 0x1069f4 반투명 사각형 채우기
// - 0x10a328 팔레트 이미지 그리기(투명색 건너뛰기)
// - 0x10b80c 팔레트 이미지 그리기 + 색 혼합(혼합 표 또는 회색, 불투명도)
// 픽셀은 RGB565다. 좌표와 주소 계산은 원본처럼 32비트로 넘친다.
#include "restored.h"

void orig_f_69f4(cpu_t *c);
void orig_f_a328(cpu_t *c);
void orig_f_b80c(cpu_t *c);

enum {
    GOT_SCREEN = 0x2c, SCREEN_IS_565 = 0x86, // 화면 정보의 픽셀 형식 표시
    GOT_KEY_COLOR = 0x100,                   // 투명색 변수
    GOT_BLEND_TABLE = 0x138,                 // 혼합 표: [방식][원본 성분 5비트][바탕 성분 5비트] → 성분
    FN_IMAGE_INFO = 0x105de4,                // (out, 핸들): 줄 바이트, ?, 폭, 높이, 픽셀 주소, 픽셀 바이트
    INFO_SIZE = 0x18, INFO_PITCH = 0, INFO_W = 8, INFO_H = 0xc, INFO_PIXELS = 0x10, INFO_BPP = 0x14,
    // 팔레트 이미지: +0 폭, +0x10 팔레트 핸들, +0x14 색 수, +0x1c 팔레트 주소(그릴 때마다 채운다), +0x20 데이터
    IMG_W = 0, IMG_PAL_HANDLE = 0x10, IMG_COLORS = 0x14, IMG_PAL = 0x1c, IMG_DATA = 0x20,
    DATA_HEAD = 8, FORMAT_4BIT = 0xb, FORMAT_8BIT = 0xc,
    MODE_GRAY = 0x10, OPAQUE = 0xff,
    RED = 0xf800, GREEN = 0x7e0, BLUE = 0x1f,
};

// ---- 반투명 사각형 ----

// (핸들, x, y, w, h, 불투명도, r, g, b). 이미지 안으로 자른 뒤 색을 불투명도만큼 섞는다. 색 성분은 0~255다.
static void fill_blend(cpu_t *c) {
    uint32_t handle = arg(c, 0), alpha = arg(c, 5) & 0xff;
    int32_t x = (int32_t)arg(c, 1), y = (int32_t)arg(c, 2), w = (int32_t)arg(c, 3), h = (int32_t)arg(c, 4);
    uint32_t r = arg(c, 6), g = arg(c, 7), b = arg(c, 8);
    if (!alpha) return;
    uint32_t sp = c->SP;
    c->SP = sp - 0x20;
    uint32_t info = c->SP;
    CALL(FN_IMAGE_INFO, info, handle);
    c->SP = sp;
    int32_t pitch = (int32_t)R32(info + INFO_PITCH), iw = (int32_t)R32(info + INFO_W), ih = (int32_t)R32(info + INFO_H);
    // 원본은 자른 높이를 호출한 쪽의 인자 칸(스택)에 되쓴다. 폭은 제 스택에만 둔다.
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; W32(sp, (uint32_t)h); }
    if (x + w > iw) w = iw - x;
    if (y + h > ih) { h = ih - y; W32(sp, (uint32_t)h); }
    uint32_t row = (uint32_t)pitch * (uint32_t)y + R8(info + INFO_BPP) * (uint32_t)x + R32(info + INFO_PIXELS);
    uint32_t keep = 0xff - alpha, add_r = alpha * r, add_g = alpha * g, add_b = alpha * b;
    for (int32_t j = 0; j < h; j++, row += (uint32_t)pitch) {
        uint32_t p = row;
        for (int32_t i = 0; i < w; i++, p += 2) {
            uint32_t d = R16(p);
            int32_t nr = (int32_t)(add_r + keep * ((d & RED) >> 8)) >> 8;
            int32_t ng = (int32_t)(add_g + keep * ((d & GREEN) >> 3)) >> 8;
            int32_t nb = (int32_t)(add_b + keep * ((d & BLUE) << 3)) >> 11;
            W16(p, (((uint32_t)nr << 8) & RED) | (((uint32_t)ng << 3) & GREEN) | ((uint32_t)nb & BLUE));
        }
    }
}

void f_69f4(cpu_t *c) {
    // 565가 아닌 화면은 원본이 픽셀마다 변환 함수를 부른다. 이 호스트의 화면은 565뿐이라 그 길은 원본에 맡긴다.
    if (R8(got(GOT_SCREEN) + SCREEN_IS_565) != 1) { orig_f_69f4(c); return; }
    if (selfcheck_level()) selfcheck_run(c, "fill_blend", 0, false, fill_blend, orig_f_69f4);
    else fill_blend(c);
}

// ---- 팔레트 이미지 ----

typedef struct {
    uint32_t dst, img, data, pal, key, table, mode;
    int32_t dx, dy, w, h, stride, sx, sy, turn, alpha;
    int32_t a5, a6; // 불투명도의 5비트, 6비트 값
    bool blend;
} blit_t;

// 방향(0~8)에 따라 원본의 j번째 줄이 놓이는 첫 칸과 한 픽셀마다의 이동. 칸은 대상의 픽셀 번호다.
// 0 그대로, 1 좌우 뒤집기, 2 상하 뒤집기, 3과 5 반 바퀴, 4와 6 시계·반시계 90도, 7과 8은 그 둘을 뒤집은 것.
static bool row_path(const blit_t *b, int32_t j, uint32_t *start, uint32_t *step) {
    uint32_t s = (uint32_t)b->stride, dx = (uint32_t)b->dx, dy = (uint32_t)b->dy, w = (uint32_t)b->w, h = (uint32_t)b->h, r = (uint32_t)j;
    switch (b->turn) {
    case 0: *start = s * (dy + r) + dx; *step = 1; return true;
    case 1: *start = s * (dy + r) + dx + w - 1; *step = (uint32_t)-1; return true;
    case 2: *start = s * (dy + h - 1 - r) + dx; *step = 1; return true;
    case 3: case 5: *start = s * (dy + h - 1 - r) + dx + w - 1; *step = (uint32_t)-1; return true;
    case 4: *start = s * dy + dx + h - 1 - r; *step = s; return true;
    case 6: *start = s * (dy + w - 1) + dx + r; *step = 0u - s; return true;
    case 7: *start = s * (dy + w - 1) + dx + h - 1 - r; *step = 0u - s; return true;
    case 8: *start = s * dy + dx + r; *step = s; return true;
    }
    return false;
}

// 팔레트 번호 하나를 대상 칸에 놓는다. 투명색이면 건너뛴다.
static inline void put(cpu_t *c, const blit_t *b, uint32_t cell, uint32_t index) {
    uint32_t at = b->dst + cell * 2, src = R16(b->pal + index * 2);
    if (!b->blend) {
        if (src != b->key) W16(at, src);
        return;
    }
    uint32_t dst = R16(at);
    if (src == b->key) return;
    uint32_t sr = src >> 11, sg = (src & GREEN) >> 6, sb = src & BLUE; // 초록도 5비트로 줄여 쓴다.
    uint32_t dr = dst >> 11, dg = (dst & GREEN) >> 6, db = dst & BLUE;
    uint32_t r, g, bl;
    if (b->mode == MODE_GRAY) {
        r = g = bl = (sr * 6 + sg * 4 + sb * 2) >> 4;
    } else {
        uint32_t t = b->table, m = b->mode << 5;
        r = R8(((m + sr) << 5) + t + dr);
        g = R8(((m + sg) << 5) + t + dg);
        bl = R8(((m + sb) << 5) + t + db);
    }
    g *= 2;
    if (b->alpha < OPAQUE) {
        r = (uint32_t)((int32_t)(b->a5 * r + dr * (31 - b->a5)) >> 5);
        g = (uint32_t)((int32_t)(g * b->a6 + dg * 2 * (62 - b->a6)) >> 6);
        bl = (uint32_t)((int32_t)(bl * b->a5 + db * (31 - b->a5)) >> 5);
    }
    W16(at, (r << 11) | ((g << 5) & GREEN) | (bl & BLUE));
}

static inline void draw(cpu_t *c, blit_t *b) {
    if (!b->dst || !b->img) return;
    b->data = R32(b->img + IMG_DATA);
    uint32_t head = R32(b->img + IMG_COLORS) * 2 + DATA_HEAD;
    b->pal = R32(R32(b->img + IMG_PAL_HANDLE)) + DATA_HEAD;
    W32(b->img + IMG_PAL, b->pal);
    uint32_t format = R8(b->data);
    int32_t iw = (int32_t)R32(b->img + IMG_W);
    if (format != FORMAT_4BIT && format != FORMAT_8BIT) return;
    b->key = R32(got(GOT_KEY_COLOR));
    b->table = got(GOT_BLEND_TABLE);
    uint32_t start, step;
    if (format == FORMAT_8BIT) {
        uint32_t src = head + (uint32_t)b->sx + (uint32_t)b->sy * (uint32_t)iw + 1;
        for (int32_t j = 0; j < b->h; j++, src += (uint32_t)iw) {
            if (!row_path(b, j, &start, &step)) return;
            for (int32_t i = 0; i < b->w; i++, start += step) put(c, b, start, R8(b->data + src + (uint32_t)i));
        }
        return;
    }
    // 4비트: 한 바이트에 두 픽셀(위 4비트가 먼저), 줄은 바이트 단위로 맞춘다.
    // 원본은 홀수 칸에서 시작하면 폭을 보지 않고 한 픽셀을 먼저 그리고, 남은 폭이 홀수면 끝에 하나를 더 그린다. 폭이 0 이하일 때도 같다.
    int32_t line = (iw & 1 ? iw + 1 : iw) >> 1;
    uint32_t src = head + (uint32_t)(b->sx >> 1) + (uint32_t)b->sy * (uint32_t)line + 1;
    // 혼합 없는 방향 0만은 줄 시작을 다시 계산하지 않고 그린 칸 수 + (줄 폭 - 폭)만큼 이어 간다. 폭이 0 이하면 결과가 달라진다.
    bool running = !b->blend && b->turn == 0;
    uint32_t next = 0;
    for (int32_t j = 0; j < b->h; j++, src += (uint32_t)line) {
        if (!row_path(b, j, &start, &step)) return;
        if (running && j) start = next;
        uint32_t at = b->data + src;
        int32_t rest = b->w;
        if (b->sx & 1) { put(c, b, start, R8(at++) & 0xf); start += step; rest--; }
        for (int32_t n = rest >> 1; n > 0; n--) {
            uint32_t two = R8(at++);
            put(c, b, start, two >> 4); start += step;
            put(c, b, start, two & 0xf); start += step;
        }
        if (rest & 1) { put(c, b, start, R8(at) >> 4); start += step; }
        next = start + (uint32_t)b->stride - (uint32_t)b->w;
    }
}

// (대상 픽셀, dx, dy, w, h, 대상 줄 폭, ?, 이미지, sx, sy, ?, ?, 방향 [, 불투명도, 혼합 방식])
static void read_args(cpu_t *c, blit_t *b, bool blend) {
    b->dst = arg(c, 0); b->dx = (int32_t)arg(c, 1); b->dy = (int32_t)arg(c, 2); b->w = (int32_t)arg(c, 3); b->h = (int32_t)arg(c, 4);
    b->stride = (int32_t)arg(c, 5); b->img = arg(c, 7); b->sx = (int32_t)arg(c, 8); b->sy = (int32_t)arg(c, 9); b->turn = (int32_t)arg(c, 12);
    b->blend = blend;
    if (!blend) return;
    b->alpha = (int32_t)arg(c, 13);
    b->mode = arg(c, 14);
    b->a5 = (b->alpha & 0xf8) >> 3;
    b->a6 = (b->alpha & 0xf8) >> 2;
}

static void draw_image(cpu_t *c) { blit_t b; read_args(c, &b, false); draw(c, &b); }
static void draw_image_blend(cpu_t *c) { blit_t b; read_args(c, &b, true); draw(c, &b); }

// ---- 자체 검사 ----

static uint32_t variant_of(cpu_t *c, bool blend) {
    uint32_t img = arg(c, 7), format = 0;
    if (arg(c, 0) && img) format = R8(R32(img + IMG_DATA));
    uint32_t turn = arg(c, 12) > 8 ? 9 : arg(c, 12);
    uint32_t v = (format & 0xf) << 8 | turn << 4 | (arg(c, 8) & 1) << 3;
    if (blend) v |= ((int32_t)arg(c, 13) < OPAQUE) << 2 | (arg(c, 14) == MODE_GRAY) << 1;
    return v;
}

// 실제 호출의 이미지 하나로 방향, 시작 칸, 크기(0 이하 포함), 혼합 값을 바꿔 가며 견준다. 대상은 따로 잡은 연습 버퍼다.
static void sweep(cpu_t *c, bool blend) {
    enum { SIDE = 160, MID = 60, IMAGES = 3 };
    static uint32_t scratch, done[2][2][IMAGES];
    static bool busy;
    uint32_t img = arg(c, 7);
    if (busy || !arg(c, 0) || !img) return;
    uint32_t format = R8(R32(img + IMG_DATA));
    if (format != FORMAT_4BIT && format != FORMAT_8BIT) return;
    uint32_t *seen = done[blend][format == FORMAT_8BIT];
    int slot = 0;
    while (slot < IMAGES && seen[slot] && seen[slot] != img) slot++;
    if (slot == IMAGES || seen[slot]) return;
    seen[slot] = img;
    if (!scratch) scratch = heap_alloc(SIDE * SIDE * 2);
    if (!scratch) return;
    busy = true;
    uint32_t regs[16], sp = c->SP, mode_seen = blend ? arg(c, 14) : 0;
    memcpy(regs, c->r, sizeof regs);
    c->SP = sp - 0x40;
    static const int8_t SIZES[][2] = {{5, 4}, {4, 5}, {1, 1}, {8, 3}, {0, 3}, {-1, 2}, {-2, 2}, {3, 0}};
    static const int32_t ALPHAS[] = {0, 0x80, 0xfe, 0xff, 0x1ff, -1};
    uint32_t modes[3] = {MODE_GRAY, mode_seen == MODE_GRAY ? 0 : mode_seen, 1};
    uint32_t seed = 12345;
    for (int32_t turn = -1; turn <= 9; turn++)
        for (int32_t sx = 0; sx < 2; sx++)
            for (int32_t sy = 0; sy < 2; sy++)
                for (size_t k = 0; k < sizeof SIZES / sizeof SIZES[0]; k++)
                    for (int a = 0; a < (blend ? 6 : 1); a++)
                        for (int m = 0; m < (blend ? 3 : 1); m++) {
                            for (uint32_t i = 0; i < SIDE * SIDE; i++) { seed = seed * 1664525u + 1013904223u; W16(scratch + i * 2, seed >> 16); }
                            c->r[0] = scratch; c->r[1] = MID; c->r[2] = MID; c->r[3] = (uint32_t)SIZES[k][0];
                            uint32_t rest[11] = {(uint32_t)SIZES[k][1], SIDE, 0, img, (uint32_t)sx, (uint32_t)sy, 0, 0, (uint32_t)turn, (uint32_t)ALPHAS[a], modes[m]};
                            for (int i = 0; i < 11; i++) W32(c->SP + i * 4, rest[i]);
                            c->LR = 0xfffffffe;
                            selfcheck_run(c, blend ? "sweep_blend" : "sweep_image", variant_of(c, blend), true, blend ? draw_image_blend : draw_image, blend ? orig_f_b80c : orig_f_a328);
                        }
    memcpy(c->r, regs, sizeof regs);
    busy = false;
}

void f_a328(cpu_t *c) {
    if (!selfcheck_level()) { draw_image(c); return; }
    if (selfcheck_level() == 2) sweep(c, false);
    selfcheck_run(c, "draw_image", variant_of(c, false), false, draw_image, orig_f_a328);
}

void f_b80c(cpu_t *c) {
    if (!selfcheck_level()) { draw_image_blend(c); return; }
    if (selfcheck_level() == 2) sweep(c, true);
    selfcheck_run(c, "draw_image_blend", variant_of(c, true), false, draw_image_blend, orig_f_b80c);
}
