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
void ui_release_all(void);
void ui_render(void);
#ifdef __cplusplus
}
#endif
