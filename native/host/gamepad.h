// 게임패드 입력과 키 맵핑, 현재 조작 장치에 맞춘 도움말 글.
#pragma once
#include <SDL3/SDL.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void gamepad_init(void);
bool gamepad_event(const SDL_Event *e); // 패드 이벤트면 true.
void gamepad_tick(void);                // 메뉴 이동의 길게 누르기를 처리한다.
void gamepad_release_all(void);         // 눌린 게임 키를 모두 뗀다.
bool gamepad_ctrl(const char *line);    // 시험용 가상 패드 명령. 처리했으면 true.

bool gamepad_connected(void);
int gamepad_generation(void); // 맵핑, 연결, 패드 종류가 바뀌면 늘어난다.
int gamepad_action_count(void);
const char *gamepad_action_name(int action); // UTF-8
const char *gamepad_action_key(int action);  // 맵핑된 버튼 표시. UTF-8
void gamepad_bind(int action, int input);    // 다른 동작이 쓰던 버튼이면 두 동작의 버튼을 맞바꾼다.
void gamepad_bind_reset(void);
const char *gamepad_config_id(int action);
const char *gamepad_config_value(int action);
void gamepad_config_read(const char *id, const char *value);

// 조작 안내가 든 리소스면 바꾼 글을 돌려준다. 바꿀 필요가 없으면 NULL이다. 다음 호출까지 유효하다.
const uint8_t *gamepad_resource(const char *path, size_t *size);

// 게임 화면에 그리는 키 안내를 EUC-KR로 채운다. 패드로 조작 중이 아니면 false이고 원본 그림과 글을 쓴다.
enum { GAMEPAD_HINT_ITEM, GAMEPAD_HINT_MAP, GAMEPAD_HINT_SKIP };
bool gamepad_hint(int kind, char *out, int cap);
#ifdef __cplusplus
}
#endif
