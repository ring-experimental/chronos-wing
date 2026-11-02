// 게임 위에 얹는 UI(터치 패드, 설정). 구현은 RmlUi, 화면 문서는 ui/ 폴더에 있다.
#pragma once
#include <SDL3/SDL.h>
#ifdef __cplusplus
extern "C" {
#endif
bool ui_init(SDL_Window *win, SDL_Renderer *ren, const char *ui_dir, const char *config_path, bool force_pad);
bool ui_event(const SDL_Event *e); // true면 게임으로 넘기지 않는다.
bool ui_modal(void);               // 설정이나 배치 편집 중이면 게임을 멈춘다.
void ui_toggle_settings(void);
void ui_toggle_auto(void);
void ui_release_all(void);
void ui_render(void);
// 패드로 설정 창을 다룬다. 배치는 맵핑과 상관없이 고정이다.
enum { UI_NAV_UP, UI_NAV_DOWN, UI_NAV_LEFT, UI_NAV_RIGHT, UI_NAV_OK, UI_NAV_BACK };
void ui_pad_nav(int nav);
bool ui_pad_capturing(void);     // 맵핑 창이 다음 버튼을 기다리는 중이면 true.
bool ui_pad_capture(int input);  // 기다리는 중이면 이 버튼을 맵핑하고 true.
#ifdef __cplusplus
}
#endif
