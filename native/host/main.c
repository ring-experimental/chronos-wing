// 크로노스 윙 네이티브 진입점. SDL3 창, 입력, 시험용 제어 파일.
// 사용법: chronos [게임 데이터 폴더] [--windowed] [--touch] [--width <폭>] [--save <폴더>] [--ctrl <파일>] [--soundfont <sf2>]
#include "host.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char game_dir[1024], save_dir[1024];
static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static bool running = true;
static const char *ctrl_path;
static uint32_t *last_frame;
static uint64_t frames;
extern float audio_peak;

static int width_arg;

void request_exit(void) { running = false; }

// 높이 320을 유지하고 폭은 모니터 비율을 따른다(240~640). 창 모드 기본값은 원작 폭이다.
static int choose_width(bool windowed) {
    int w = width_arg;
    if (!w && !windowed) {
        const SDL_DisplayMode *m = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
#ifdef __ANDROID__
        // 시작 순간에는 세로로 보고될 수 있어 긴 변을 폭으로 쓴다.
        w = m && m->h ? 320 * SDL_max(m->w, m->h) / SDL_min(m->w, m->h) : 240;
#else
        w = m && m->h ? 320 * m->w / m->h : 240;
#endif
    }
    if (w < 240) w = 240;
    if (w > 640) w = 640;
    return w & ~1;
}

static const uint32_t FN_START = 0x1592a4, FN_INPUT = 0x1593b0;

// 대조용. CHRONOS_FRAMELOG=<폴더>: 표시한 프레임마다 log.txt에 `순번 해시 폭 높이`, 처음 본 프레임은 <해시>.raw(RGB565).
static void frame_log(const uint8_t *px, int w, int h, int bpl) {
    static const char *dir;
    static int init;
    static uint32_t seen[65536];
    if (!init) { init = 1; dir = getenv("CHRONOS_FRAMELOG"); }
    if (!dir) return;
    uint32_t hash = 0x811c9dc5u;
    for (int y = 0; y < h; y++)
        for (int i = 0; i < w * 2; i++) hash = (hash ^ px[y * bpl + i]) * 16777619u;
    char path[1100];
    snprintf(path, sizeof path, "%s/log.txt", dir);
    FILE *o = fopen(path, "a");
    if (o) { fprintf(o, "%llu %08x %d %d\n", (unsigned long long)frames, hash, w, h); fclose(o); }
    uint32_t slot = hash & 0xffff;
    while (seen[slot] && seen[slot] != hash) slot = (slot + 1) & 0xffff;
    if (seen[slot]) return;
    seen[slot] = hash;
    snprintf(path, sizeof path, "%s/%08x.raw", dir, hash);
    o = fopen(path, "wb");
    if (!o) return;
    for (int y = 0; y < h; y++) fwrite(px + y * bpl, 2, w, o);
    fclose(o);
}

void touch_draw(SDL_Renderer *ren, int w, int h);
void touch_event(SDL_Renderer *ren, SDL_Event *e);
void touch_release_all(void);
extern bool touch_enabled;
bool android_prepare_data(void);

void video_present(uint32_t fb) {
    cpu_t *c = &cpu;
    uint32_t f = handle_data(fb);
    int w = (int)R32(f), h = (int)R32(f + 4), bpl = (int)R32(f + 8);
    uint8_t *px = c->m + handle_data(R32(f + 16));
    frame_log(px, w, h, bpl);
    frames++;
    if (!tex) {
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w, h);
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        SDL_SetRenderLogicalPresentation(ren, w, h, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    }
    SDL_UpdateTexture(tex, NULL, px, bpl);
    SDL_RenderClear(ren);
    SDL_RenderTexture(ren, tex, NULL, NULL);
    touch_draw(ren, w, h);
    SDL_RenderPresent(ren);
}

static void write_shot(const char *path) {
    cpu_t *c = &cpu;
    uint32_t h = 0;
    extern uint32_t gfx_screen_handle(void);
    h = gfx_screen_handle();
    if (!h) return;
    uint32_t f = handle_data(h);
    int w = (int)R32(f), ht = (int)R32(f + 4), bpl = (int)R32(f + 8);
    uint8_t *px = c->m + handle_data(R32(f + 16));
    FILE *o = fopen(path, "wb");
    if (!o) return;
    fprintf(o, "P6\n%d %d\n255\n", w, ht);
    for (int y = 0; y < ht; y++)
        for (int x = 0; x < w; x++) {
            uint16_t p;
            memcpy(&p, px + y * bpl + x * 2, 2);
            uint8_t rgb[3] = {(uint8_t)(((p >> 11 & 31) * 255 + 15) / 31), (uint8_t)(((p >> 5 & 63) * 255 + 31) / 63), (uint8_t)(((p & 31) * 255 + 15) / 31)};
            fwrite(rgb, 1, 3, o);
        }
    fclose(o);
}

static void send_key(int type, int code) {
    uint32_t a[3] = {(uint32_t)type, (uint32_t)code, 0};
    guest_call(FN_INPUT, a, 3);
}

static int key_from_scancode(SDL_Scancode s) {
    switch (s) {
    case SDL_SCANCODE_AC_BACK: return -16;
    case SDL_SCANCODE_UP: return -1;
    case SDL_SCANCODE_DOWN: return -2;
    case SDL_SCANCODE_LEFT: return -3;
    case SDL_SCANCODE_RIGHT: return -4;
    case SDL_SCANCODE_SPACE: case SDL_SCANCODE_RETURN: return -5;
    case SDL_SCANCODE_LSHIFT: return -6;
    case SDL_SCANCODE_RSHIFT: return -7;
    case SDL_SCANCODE_BACKSPACE: return -16;
    case SDL_SCANCODE_1: return '1';
    case SDL_SCANCODE_2: return '2';
    case SDL_SCANCODE_3: return '3';
    case SDL_SCANCODE_Q: return '4';
    case SDL_SCANCODE_W: return '5';
    case SDL_SCANCODE_E: return '6';
    case SDL_SCANCODE_A: return '7';
    case SDL_SCANCODE_S: return '8';
    case SDL_SCANCODE_D: return '9';
    case SDL_SCANCODE_Z: return '*';
    case SDL_SCANCODE_X: return '0';
    case SDL_SCANCODE_C: return '#';
    default: return 0;
    }
}

void host_key(int type, int code) { send_key(type, code); }

static int key_from_name(const char *n) {
    static const struct { const char *name; int code; } K[] = {{"up",-1},{"down",-2},{"left",-3},{"right",-4},{"ok",-5},{"lsk",-6},{"rsk",-7},{"clear",-16},{"star",'*'},{"hash",'#'}};
    for (size_t i = 0; i < sizeof K / sizeof K[0]; i++) if (!strcmp(n, K[i].name)) return K[i].code;
    if (n[0] >= '0' && n[0] <= '9' && !n[1]) return n[0];
    return 0;
}

// WIE_CTRL과 같은 형식: key <이름>, shot <ppm>, quit. 줄 단위로 읽고 지운다.
static int pending_release, release_code;
static uint64_t release_at;

// CHRONOS_SCRIPT=<파일>: `<프레임> key|up|quit [이름]` 줄을 그만큼 표시한 뒤 실행한다.
static void script_poll(void) {
    static FILE *f;
    static int init, have;
    static unsigned long long at;
    static char cmd[16], name[16];
    if (!init) { init = 1; const char *p = getenv("CHRONOS_SCRIPT"); if (p) f = fopen(p, "r"); }
    while (f) {
        if (!have) {
            char line[64];
            if (!fgets(line, sizeof line, f)) { fclose(f); f = NULL; return; }
            name[0] = 0;
            if (sscanf(line, "%llu %15s %15s", &at, cmd, name) < 2) continue;
            have = 1;
        }
        if (at > frames) return;
        have = 0;
        int k = key_from_name(name);
        if (!strcmp(cmd, "key") && k) send_key(2, k);
        else if (!strcmp(cmd, "up") && k) send_key(3, k);
        else if (!strcmp(cmd, "quit")) running = false;
    }
}

static void ctrl_poll(void) {
    script_poll();
    if (release_code && now_ms() >= release_at) { send_key(3, release_code); release_code = 0; }
    if (!ctrl_path) return;
    FILE *f = fopen(ctrl_path, "r");
    if (!f) return;
    char line[1100];
    char lines[32][1100];
    int n = 0;
    while (n < 32 && fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        strcpy(lines[n++], line);
    }
    fclose(f);
    remove(ctrl_path);
    for (int i = 0; i < n; i++) {
        if (!strncmp(lines[i], "key ", 4)) {
            int k = key_from_name(lines[i] + 4);
            if (k) { send_key(2, k); release_code = k; release_at = now_ms() + 150; }
        } else if (!strncmp(lines[i], "shot ", 5)) write_shot(lines[i] + 5);
        else if (!strcmp(lines[i], "quit")) running = false;
    }
    (void)pending_release;
}

static void load_image(void) {
    cpu_t *c = &cpu;
    char path[1100];
    snprintf(path, sizeof path, "%s/client.bin", game_dir);
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "%s 없음\n", path); exit(1); }
    size_t n = fread(c->m + IMAGE_LOAD, 1, 0x100000, f);
    fclose(f);
    // 원본 시작 코드(0xc)가 하는 재배치와 같다. BSS는 calloc으로 이미 0이다.
    uint32_t relocs = R32(IMAGE_LOAD + 0x70), base = IMAGE_LOAD + 0x74 + relocs * 4;
    for (uint32_t i = 0; i < relocs; i++) {
        uint32_t o = base + R32(IMAGE_LOAD + 0x74 + i * 4);
        W32(o, R32(o) + base);
    }
    uint32_t got0 = R32(base), got1 = R32(base + 4); // 머리 값도 위에서 재배치됐다.
    for (uint32_t o = got0; o < got1; o += 4) W32(o, R32(o) + base);
    fprintf(stderr, "이미지 %zu바이트 적재, 기준 %#x, GOT %#x-%#x\n", n, base, got0, got1);
}

int main(int argc, char **argv) {
    bool windowed = false;
    const char *soundfont = NULL, *save = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--windowed")) windowed = true;
        else if (!strcmp(argv[i], "--touch")) touch_enabled = true;
        else if (!strcmp(argv[i], "--save") && i + 1 < argc) save = argv[++i];
        else if (!strcmp(argv[i], "--width") && i + 1 < argc) width_arg = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--ctrl") && i + 1 < argc) ctrl_path = argv[++i];
        else if (!strcmp(argv[i], "--soundfont") && i + 1 < argc) soundfont = argv[++i];
        else snprintf(game_dir, sizeof game_dir, "%s", argv[i]);
    }
    extern FILE *api_trace;
    if (getenv("CHRONOS_TRACE")) api_trace = fopen(getenv("CHRONOS_TRACE"), "w");
#ifdef __ANDROID__
    touch_enabled = true;
    windowed = false;
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
#endif
    if (touch_enabled) SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "1");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
#ifdef __ANDROID__
    if (!android_prepare_data()) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "크로노스 윙", "게임 데이터를 풀지 못했습니다.", NULL); return 1; }
#endif
    // 인자가 없으면 실행 파일 옆의 game 폴더를 쓴다.
    if (!game_dir[0]) snprintf(game_dir, sizeof game_dir, "%sgame", SDL_GetBasePath());
    screen_w = choose_width(windowed);
    if (save) snprintf(save_dir, sizeof save_dir, "%s", save);
    else { char *p = SDL_GetPrefPath("chronos-wing", "save"); snprintf(save_dir, sizeof save_dir, "%s", p); SDL_free(p); }
    size_t l = strlen(save_dir);
    if (l && save_dir[l - 1] == '/') save_dir[l - 1] = 0;
    SDL_CreateDirectory(save_dir);

    if (!SDL_CreateWindowAndRenderer("크로노스 윙", screen_w * 2, 640, windowed ? SDL_WINDOW_RESIZABLE : SDL_WINDOW_FULLSCREEN, &win, &ren)) {
        fprintf(stderr, "창 생성 실패: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(ren, 1);

    cpu.m = calloc(1, GUEST_MEM);
    cpu.memsz = GUEST_MEM;
    cpu.SP = STACK_TOP - 16;
    load_image();
    heap_init();
    api_install();
    audio_init(soundfont);

    uint32_t start[2] = {0, 0};
    guest_call(FN_START, start, 2);

    while (running) {
        SDL_Event e;
        int64_t wait = timers_next_ms();
        if (wait > 10 || wait < 0) wait = 10;
        if (SDL_WaitEventTimeout(&e, (int)wait)) {
            do {
                touch_event(ren, &e);
                if (e.type == SDL_EVENT_QUIT) running = false;
                else if (e.type == SDL_EVENT_WILL_ENTER_BACKGROUND) touch_release_all();
                else if ((e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) && !e.key.repeat) {
                    if (e.key.scancode == SDL_SCANCODE_F11 && e.type == SDL_EVENT_KEY_DOWN) {
                        windowed = !windowed;
                        SDL_SetWindowFullscreen(win, !windowed);
                        continue;
                    }
                    int k = key_from_scancode(e.key.scancode);
                    if (k) send_key(e.type == SDL_EVENT_KEY_DOWN ? 2 : 3, k);
                }
            } while (SDL_PollEvent(&e));
        }
        ctrl_poll();
        timers_run();
    }
    fprintf(stderr, "종료, 프레임 %llu, 소리 최대값 %.3f\n", (unsigned long long)frames, audio_peak);
    SDL_Quit();
    return 0;
}
