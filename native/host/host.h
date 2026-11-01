// 네이티브 호스트 공용 선언. WIPI API 동작은 WIE(MIT, dlunch)의 wie-wipi-c를 참고했다.
#pragma once
#include "thumbrt.h"
#include <stdbool.h>

#define IMAGE_LOAD 0x100000u
#define HOST_TABLES 0x220000u
#define STACK_TOP 0x400000u
#define HEAP_START 0x400000u
#define GUEST_MEM 0x4000000u
#define GUEST_LOW 0x100000u
#define API_BASE 0xf0000000u

extern cpu_t cpu;
extern char game_dir[1024];
extern char save_dir[1024];

// 게스트 호출. 인자는 r0-r3, 그 뒤는 스택으로 넘긴다.
uint32_t guest_call(uint32_t addr, const uint32_t *args, int n);

static inline uint32_t arg(cpu_t *c, int i) { return i < 4 ? c->r[i] : R32_(c, c->SP + (i - 4) * 4); }
static inline char *gstr(uint32_t a) { return (char *)cpu.m + a; }

typedef void (*api_fn)(cpu_t *c);
void api_dispatch(cpu_t *c, uint32_t table, uint32_t index);
void api_install(void);
void ime_api(cpu_t *c, uint32_t i); // 표 4, ime.c
typedef union SDL_Event SDL_Event;
bool ime_event(const SDL_Event *e);
void ime_poll(void);
int ime_text_width(const char *s, int n);
void ime_text(const char *utf8);
void ime_compose(const char *utf8);
void uic_api(cpu_t *c, uint32_t i); // 표 9, ime.c

// 메모리
extern uint32_t host_last_call;
void guest_mem_alloc(void);
void heap_init(void);
uint32_t heap_alloc(uint32_t size);
void heap_free(uint32_t addr);
uint32_t handle_alloc(uint32_t size);
void handle_free(uint32_t h);
static inline uint32_t handle_data(uint32_t h) { return R32_(&cpu, h) + 8; }

// 타이머
void timer_def(uint32_t timer, uint32_t cb);
int timer_set(uint32_t timer, uint64_t ms, uint32_t param);
void timer_unset(uint32_t timer);
void defer_call(uint32_t cb, uint32_t a0, uint32_t a1, uint64_t ms);
void timers_run(void);
int64_t timers_next_ms(void);
uint64_t now_ms(void);

// 표시
void video_present(uint32_t fb_handle);
// restored/replay.c
void replay_tick(void);
bool replay_key(int type, int code);
extern bool host_save_sandbox; // 참이면 게임의 저장 파일 쓰기를 버린다(api.c).
// restored/dialog.c
bool dialog_auto(void);
void dialog_set_auto(bool on);
void dialog_auto_tick(void);
bool dialog_log_is_open(void);
void dialog_log_toggle(void);
void dialog_log_scroll(int rows);
bool dialog_log_key(int type, int code);
void host_key(int type, int code);
void audio_pause(bool paused);
struct SDL_Window *host_window(void);

enum { SAVE_TEXT_CHOOSE, SAVE_TEXT_IMPORT, SAVE_TEXT_EXPORT, SAVE_TEXT_CONFIRM, SAVE_TEXT_EXPORTED, SAVE_TEXT_EXPORT_FAILED, SAVE_TEXT_NO_SAVE, SAVE_TEXT_INVALID, SAVE_TEXT_IMPORT_FAILED, SAVE_TEXT_IMPORTED };
uint32_t savefile_text(int id);
void savefile_choose(bool import);
void savefile_ask_overwrite(void);
void savefile_overwrite_answer(bool yes);
void savefile_poll(void);
bool savefile_dialog_open(void);
extern int screen_w, screen_h;
void request_exit(void);

// 소리
void audio_init(const char *soundfont);
uint32_t audio_load(const uint8_t *data, uint32_t size);
void audio_play(uint32_t handle, bool repeat);
void audio_stop(uint32_t handle);
void audio_free(uint32_t handle);

// 그래픽 API (표 3)
void gfx_api(cpu_t *c, uint32_t index);
