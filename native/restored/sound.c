// 게임의 소리 의도를 호스트에 알린다. 원본은 클립이 하나뿐이라 호출 순서만으로는 효과음 때문에 끄는 배경음과 실제 정지를 구별할 수 없다.
#include "restored.h"

void orig_f_7d3a0(cpu_t *c);
void orig_f_7d564(cpu_t *c);
void orig_f_74200(cpu_t *c);
void orig_f_47aa0(cpu_t *c);

enum { FN_SOUND_LEVEL = 0x14821c };

// 효과음 재생(0x17d3a0)과 효과음 정지(0x17d564)는 클립이 배경음이어도 끈다. 배경음은 계속 재생한다.
void f_7d3a0(cpu_t *c) {
    audio_sfx_scope(true);
    orig_f_7d3a0(c);
    audio_sfx_scope(false);
}

void f_7d564(cpu_t *c) {
    audio_sfx_scope(true);
    orig_f_7d564(c);
    audio_sfx_scope(false);
}

// 배경음 해제(0x174200). 맵 전환과 타이틀 복귀가 여기를 지난다. 이때 클립은 효과음일 수 있다.
void f_74200(cpu_t *c) {
    orig_f_74200(c);
    audio_music_stop();
}

// 환경설정 닫기(0x147aa0). 배경음 음량 0이면 게임은 곡을 다시 틀지 않는다.
void f_47aa0(cpu_t *c) {
    orig_f_47aa0(c);
    if ((uint8_t)CALL(FN_SOUND_LEVEL, 0) == 0) audio_music_stop();
}
