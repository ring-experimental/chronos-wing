// 크로노스 윙 네이티브 진입점. SDL3 창, 입력, 시험용 제어 파일.
// 사용법: chronos [게임 데이터 폴더] [--windowed] [--touch] [--width <폭>] [--save <폴더>] [--ctrl <파일>] [--soundfont <sf2>]
#include "host.h"
#include "ui.h"
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
enum { ANDROID_INPUT_POLL_MS = 8 };

void request_exit(void) { running = false; }
SDL_Window *host_window(void) { return win; }

// 스크립트 대사(0x1a2618 구조체, 키 처리는 0x11ec54)가 화면에 있는지 돌려준다. 대사(0x32, 0x35)가 뜨면 켜고 입력을 받는 화면이 뜨면 끈다.
// 장면 전환 중에는 상태가 잠깐 바뀌고 실행 값도 한 프레임씩 꺼지므로 그 사이에는 유지한다.
enum { SCRIPT_RUNNING = 0x1a2940, SCRIPT_STATE = 0x1a2944, GAME_POPUP_OPEN = 0x1a2604, NPC_SCENE_OPEN = 0x2100bc, DIALOG_HOLD_MS = 200 };

bool game_in_dialog(void) {
    static bool on;
    static uint64_t stopped_at;
    if (!cpu.m) return false;
    int state = (int)(cpu.m[SCRIPT_STATE] | cpu.m[SCRIPT_STATE + 1] << 8 | cpu.m[SCRIPT_STATE + 2] << 16 | (unsigned)cpu.m[SCRIPT_STATE + 3] << 24);
    if (cpu.m[GAME_POPUP_OPEN] || cpu.m[NPC_SCENE_OPEN] || state == 0x37 || state == 0x38) on = false;
    else if (!cpu.m[SCRIPT_RUNNING]) {
        if (now_ms() - stopped_at >= DIALOG_HOLD_MS) on = false;
        return on;
    } else if (state == 0x32 || state == 0x35) on = true;
    stopped_at = now_ms();
    return on;
}

// 스킬 단축키 구매 여부다. 주인공 구조체(0x1b0e58) +600이다.
bool game_shortcuts_bought(void) { return cpu.m && cpu.m[0x1b0e58 + 600]; }

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

bool android_prepare_data(void);
static bool frame_dirty;
static const char *winshot_path;

// 게임 화면을 비율대로 맞추고 그 위에 UI를 그린다.
static void render_frame(void) {
    int ow, oh;
    SDL_GetCurrentRenderOutputSize(ren, &ow, &oh);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    if (tex) {
        float tw, th;
        SDL_GetTextureSize(tex, &tw, &th);
        float scale = SDL_min(ow / tw, oh / th);
        SDL_FRect dst = {(ow - tw * scale) / 2, (oh - th * scale) / 2, tw * scale, th * scale};
        SDL_RenderTexture(ren, tex, NULL, &dst);
    }
    ui_render();
    if (winshot_path) {
        SDL_Surface *s = SDL_RenderReadPixels(ren, NULL);
        if (s) { SDL_SaveBMP(s, winshot_path); SDL_DestroySurface(s); }
        winshot_path = NULL;
    }
    SDL_RenderPresent(ren);
    frame_dirty = false;
}

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
    }
    SDL_UpdateTexture(tex, NULL, px, bpl);
    frame_dirty = true;
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
    if (dialog_log_key(type, code)) return;
    if (replay_key(type, code)) return;
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

// 대조용. 게스트 데이터 영역을 CHRONOS_FRAMELOG 폴더의 mem_<이름>.bin에 쓴다.
static void dump_memory(const char *name) {
    const char *dir = getenv("CHRONOS_FRAMELOG");
    if (!dir) return;
    char path[1100];
    snprintf(path, sizeof path, "%s/mem_%s.bin", dir, name);
    FILE *o = fopen(path, "wb");
    if (!o) return;
    fwrite(cpu.m + 0x19c8d4, 1, 0x400000 - 0x19c8d4, o);
    fclose(o);
}

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
        else if (!strcmp(cmd, "dump")) dump_memory(name);
    }
}

// 시험용. `finger down|move|up <id> <x> <y>`, `mouse down|move|up <x> <y>`. 좌표는 0~1 비율이다.
static void inject_pointer(const char *line) {
    char what[8], act[8];
    int id = 0;
    float x = 0, y = 0;
    int ww, wh;
    SDL_GetWindowSize(win, &ww, &wh);
    SDL_Event e;
    SDL_zero(e);
    if (sscanf(line, "%7s %7s", what, act) != 2) return;
    if (!strcmp(what, "finger")) {
        if (sscanf(line, "%*s %*s %d %f %f", &id, &x, &y) != 3) return;
        e.type = !strcmp(act, "down") ? SDL_EVENT_FINGER_DOWN : !strcmp(act, "up") ? SDL_EVENT_FINGER_UP : SDL_EVENT_FINGER_MOTION;
        e.tfinger.fingerID = id + 1;
        e.tfinger.x = x;
        e.tfinger.y = y;
    } else {
        if (sscanf(line, "%*s %*s %f %f", &x, &y) != 2) return;
        if (!strcmp(act, "move")) { e.type = SDL_EVENT_MOUSE_MOTION; e.motion.x = x * ww; e.motion.y = y * wh; }
        else { e.type = !strcmp(act, "down") ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP; e.button.button = SDL_BUTTON_LEFT; e.button.x = x * ww; e.button.y = y * wh; }
    }
    SDL_PushEvent(&e);
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
        else if (!strncmp(lines[i], "winshot ", 8)) { static char p[1100]; snprintf(p, sizeof p, "%s", lines[i] + 8); winshot_path = p; frame_dirty = true; }
        else if (!strcmp(lines[i], "bs")) { // 입력기가 보내는 Backspace. 글자와 순서가 섞이지 않게 바로 넘긴다.
            SDL_Event b = {0};
            b.key.type = SDL_EVENT_KEY_DOWN; b.key.scancode = SDL_SCANCODE_BACKSPACE; b.key.down = true;
            ime_event(&b);
            b.key.type = SDL_EVENT_KEY_UP; b.key.down = false;
            ime_event(&b);
        }
        else if (!strncmp(lines[i], "text ", 5)) ime_text(lines[i] + 5);
        else if (!strncmp(lines[i], "edit ", 5)) ime_compose(lines[i] + 5);
        else if (!strcmp(lines[i], "settings")) ui_toggle_settings();
        else if (!strncmp(lines[i], "finger ", 7) || !strncmp(lines[i], "mouse ", 6)) inject_pointer(lines[i]);
        else if (!strcmp(lines[i], "quit")) running = false;
    }
    (void)pending_release;
}

static void load_image(void) {
    cpu_t *c = &cpu;
    char path[1100];
    snprintf(path, sizeof path, "%s/client.bin", game_dir);
    FILE *f = fopen(path, "rb");
    if (!f) { SDL_Log("%s 없음", path); exit(1); }
    size_t n = fread(c->m + IMAGE_LOAD, 1, 0x100000, f);
    fclose(f);
    // 원본 시작 코드(0xc)가 하는 재배치와 같다. BSS는 새로 받은 페이지라 이미 0이다.
    uint32_t relocs = R32(IMAGE_LOAD + 0x70), base = IMAGE_LOAD + 0x74 + relocs * 4;
    for (uint32_t i = 0; i < relocs; i++) {
        uint32_t o = base + R32(IMAGE_LOAD + 0x74 + i * 4);
        W32(o, R32(o) + base);
    }
    uint32_t got0 = R32(base), got1 = R32(base + 4); // 머리 값도 위에서 재배치됐다.
    for (uint32_t o = got0; o < got1; o += 4) W32(o, R32(o) + base);
    fprintf(stderr, "이미지 %zu바이트 적재, 기준 %#x, GOT %#x-%#x\n", n, base, got0, got1);
}

// Android의 SDL_WaitEventTimeout은 잠들지 않고 시간이 될 때까지 돌아 코어 하나를 다 쓴다. 직접 재우고 입력은 짧은 간격으로 본다.
static bool wait_event(SDL_Event *e, int ms) {
#ifdef __ANDROID__
    if (SDL_PollEvent(e)) return true;
    if (ms > ANDROID_INPUT_POLL_MS) ms = ANDROID_INPUT_POLL_MS;
    if (ms > 0) SDL_Delay(ms);
    return SDL_PollEvent(e);
#else
    return SDL_WaitEventTimeout(e, ms);
#endif
}

int main(int argc, char **argv) {
    bool windowed = false, force_pad = false;
    const char *soundfont = NULL, *save = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--windowed")) windowed = true;
        else if (!strcmp(argv[i], "--touch")) force_pad = true;
        else if (!strcmp(argv[i], "--save") && i + 1 < argc) save = argv[++i];
        else if (!strcmp(argv[i], "--width") && i + 1 < argc) width_arg = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--ctrl") && i + 1 < argc) ctrl_path = argv[++i];
        else if (!strcmp(argv[i], "--soundfont") && i + 1 < argc) soundfont = argv[++i];
        else snprintf(game_dir, sizeof game_dir, "%s", argv[i]);
    }
    extern FILE *api_trace;
    if (getenv("CHRONOS_TRACE")) api_trace = fopen(getenv("CHRONOS_TRACE"), "w");
#ifdef __ANDROID__
    windowed = false;
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
#endif
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "1");
    SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "composition");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) { SDL_Log("SDL_Init: %s", SDL_GetError()); return 1; }
#ifdef __ANDROID__
    if (!android_prepare_data()) { SDL_Log("게임 데이터 풀기 실패: %s", SDL_GetError()); SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "크로노스 윙", "게임 데이터를 풀지 못했습니다.", NULL); return 1; }
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
        SDL_Log("창 생성 실패: %s", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(ren, 1);
    char ui_dir[1100], ui_cfg[1100];
    snprintf(ui_dir, sizeof ui_dir, "%s/../ui", game_dir);
    snprintf(ui_cfg, sizeof ui_cfg, "%s/../ui.ini", save_dir);
    if (!ui_init(win, ren, ui_dir, ui_cfg, force_pad)) SDL_Log("UI를 열지 못함: 패드와 설정 없이 진행");

    guest_mem_alloc();
    cpu.SP = STACK_TOP - 16;
    load_image();
    heap_init();
    api_install();
    audio_init(soundfont);

    uint32_t start[2] = {0, 0};
    guest_call(FN_START, start, 2);

    bool paused = false;
    while (running) {
        SDL_Event e;
        int64_t wait = timers_next_ms();
        // 시험용 제어 파일만 짧게 폴링한다. 평소에는 입력이나 다음 타이머까지 잔다.
        int64_t max_wait = ctrl_path ? 10 : 100;
        if (wait > max_wait || wait < 0) wait = max_wait;
        if (wait_event(&e, (int)wait)) {
            do {
                if (e.type == SDL_EVENT_QUIT) { SDL_Log("종료 이벤트"); running = false; }
                else if (e.type == SDL_EVENT_WILL_ENTER_BACKGROUND) ui_release_all();
                if (ui_event(&e)) continue;
                if (ime_event(&e)) continue;
                else if ((e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) && !e.key.repeat) {
                    if (e.key.scancode == SDL_SCANCODE_F11 && e.type == SDL_EVENT_KEY_DOWN) {
                        windowed = !windowed;
                        SDL_SetWindowFullscreen(win, !windowed);
                        continue;
                    }
                    if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_TAB) { ui_toggle_auto(); continue; }
                    if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_L) { dialog_log_toggle(); continue; }
                    int k = key_from_scancode(e.key.scancode);
                    if (k) send_key(e.type == SDL_EVENT_KEY_DOWN ? 2 : 3, k);
                }
            } while (SDL_PollEvent(&e));
        }
        ctrl_poll();
        savefile_poll();
        ime_poll();
        // 설정 중이나 파일 창이 열린 동안에는 게임을 멈춘다.
        bool modal = ui_modal() || savefile_dialog_open();
        if (modal != paused) { paused = modal; audio_pause(paused); }
        // 대사 기록 창이 열린 동안에도 게임을 멈춘다. 음악은 계속 나온다.
        bool frozen = dialog_log_is_open();
        if (!paused && !frozen) {
            dialog_auto_tick();
            replay_tick();
            timers_run();
        }
        if (frame_dirty || paused || frozen) render_frame();
    }
    SDL_Log("메인 루프 끝, 프레임 %llu", (unsigned long long)frames);
    fprintf(stderr, "종료, 프레임 %llu, 소리 최대값 %.3f\n", (unsigned long long)frames, audio_peak);
    SDL_Quit();
    return 0;
}
