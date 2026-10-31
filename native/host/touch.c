// 화면 터치 패드. 원작 키패드를 손가락으로 누르는 용도다. Android 기본, 데스크톱은 --touch로 켠다.
#include "host.h"
#include <SDL3/SDL.h>
#include <stdlib.h>

enum { GROUP_PAD, GROUP_MAIN, GROUP_NUM, GROUP_TOGGLE };
typedef struct { const char *label; int code, group; float x, y, w, h; int presses; } button_t;

#define KEY_SIZE 34.0f
static button_t buttons[] = {
    {"^", -1, GROUP_PAD}, {"v", -2, GROUP_PAD}, {"<", -3, GROUP_PAD}, {">", -4, GROUP_PAD},
    {"OK", -5, GROUP_MAIN}, {"C", -16, GROUP_MAIN}, {"*", '*', GROUP_MAIN}, {"#", '#', GROUP_MAIN},
    {"L", -6, GROUP_MAIN}, {"R", -7, GROUP_MAIN}, {"123", 0, GROUP_TOGGLE},
    {"1", '1', GROUP_NUM}, {"2", '2', GROUP_NUM}, {"3", '3', GROUP_NUM},
    {"4", '4', GROUP_NUM}, {"5", '5', GROUP_NUM}, {"6", '6', GROUP_NUM},
    {"7", '7', GROUP_NUM}, {"8", '8', GROUP_NUM}, {"9", '9', GROUP_NUM}, {"0", '0', GROUP_NUM},
};
#define NBUTTONS ((int)(sizeof buttons / sizeof buttons[0]))
#define MAX_FINGERS 10

bool touch_enabled;
static bool show_numbers;
static struct { SDL_FingerID id; int button; bool used; } fingers[MAX_FINGERS];

static void place(int i, float x, float y, float w, float h) {
    buttons[i].x = x; buttons[i].y = y; buttons[i].w = w; buttons[i].h = h;
}

static void layout(int w, int h) {
    float s = KEY_SIZE, px = 6, py = h - s * 3 - 6;
    place(0, px + s, py, s, s);
    place(1, px + s, py + s * 2, s, s);
    place(2, px, py + s, s, s);
    place(3, px + s * 2, py + s, s, s);
    place(4, w - 56.0f, h - 62.0f, 50, 50);
    place(5, w - 96.0f, h - 40.0f, 32, 28);
    place(6, w - 96.0f, h - 76.0f, 32, 28);
    place(7, w - 56.0f, h - 96.0f, 32, 28);
    place(8, 4, 4, 34, 24);
    place(9, w - 38.0f, 4, 34, 24);
    place(10, w - 80.0f, 4, 38, 24);
    float nx = w - 96.0f, ny = 34;
    for (int n = 0; n < 9; n++) place(11 + n, nx + n % 3 * 31.0f, ny + n / 3 * 27.0f, 29, 25);
    place(20, nx + 31.0f, ny + 81.0f, 29, 25);
}

static bool visible(int i) { return buttons[i].group != GROUP_NUM || show_numbers; }

static int button_at(float x, float y) {
    for (int i = NBUTTONS - 1; i >= 0; i--) {
        button_t *b = &buttons[i];
        if (visible(i) && x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h) return i;
    }
    return -1;
}

static void press(int i) {
    if (i < 0) return;
    if (buttons[i].group == GROUP_TOGGLE) { show_numbers = !show_numbers; return; }
    if (buttons[i].presses++ == 0) host_key(2, buttons[i].code);
}

static void release(int i) {
    if (i < 0 || buttons[i].group == GROUP_TOGGLE) return;
    if (buttons[i].presses > 0 && --buttons[i].presses == 0) host_key(3, buttons[i].code);
}

void touch_release_all(void) {
    for (int f = 0; f < MAX_FINGERS; f++)
        if (fingers[f].used) { release(fingers[f].button); fingers[f].used = false; }
}

void touch_event(SDL_Renderer *ren, SDL_Event *e) {
    if (!touch_enabled) return;
    if (e->type != SDL_EVENT_FINGER_DOWN && e->type != SDL_EVENT_FINGER_MOTION && e->type != SDL_EVENT_FINGER_UP && e->type != SDL_EVENT_FINGER_CANCELED) return;
    SDL_Event ev = *e;
    SDL_ConvertEventToRenderCoordinates(ren, &ev);
    int slot = -1;
    for (int f = 0; f < MAX_FINGERS; f++) if (fingers[f].used && fingers[f].id == ev.tfinger.fingerID) slot = f;
    int hit = button_at(ev.tfinger.x, ev.tfinger.y);
    if (ev.type == SDL_EVENT_FINGER_DOWN) {
        if (slot >= 0) return;
        for (int f = 0; f < MAX_FINGERS && slot < 0; f++) if (!fingers[f].used) slot = f;
        if (slot < 0) return;
        fingers[slot].id = ev.tfinger.fingerID;
        fingers[slot].button = hit;
        fingers[slot].used = true;
        press(hit);
    } else if (slot >= 0 && ev.type == SDL_EVENT_FINGER_MOTION) {
        // 방향 패드만 손가락을 떼지 않고 옮겨 갈 수 있다.
        int cur = fingers[slot].button;
        if (hit != cur && cur >= 0 && buttons[cur].group == GROUP_PAD && hit >= 0 && buttons[hit].group == GROUP_PAD) {
            press(hit);
            release(cur);
            fingers[slot].button = hit;
        }
    } else if (slot >= 0) {
        release(fingers[slot].button);
        fingers[slot].used = false;
    }
}

void touch_draw(SDL_Renderer *ren, int w, int h) {
    if (!touch_enabled) return;
    layout(w, h);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < NBUTTONS; i++) {
        button_t *b = &buttons[i];
        if (!visible(i)) continue;
        SDL_FRect r = {b->x, b->y, b->w, b->h};
        Uint8 a = b->presses ? 150 : 70;
        SDL_SetRenderDrawColor(ren, 255, 255, 255, a);
        SDL_RenderFillRect(ren, &r);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 170);
        SDL_RenderRect(ren, &r);
        SDL_RenderDebugText(ren, b->x + (b->w - 8.0f * (float)SDL_strlen(b->label)) / 2, b->y + (b->h - 8) / 2, b->label);
    }
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
}
