// 턴제 모드 켜고 끄기와 전용 저장 폴더(사용자 결정: 스토리모드 엔딩 뒤 메인메뉴에서 열리고 본편 세이브와 연계하지 않는다).
// - 메인메뉴 "턴제 모드"(devmode.c 항목 16)를 고르면 턴제를 켜고 저장 폴더를 save_dir/turn으로 바꾼 뒤 원작 스토리모드(상태 0)로 넘긴다.
//   원작 스토리모드는 세이브가 있으면 이어 하고 없으면 새로 시작한다. 세이브 파일 경로는 열 때마다 save_dir로 정해진다(host/api.c).
// - 메인메뉴에서 다른 항목을 고르거나 좌우로 움직이면(0x1453bc, 0x14609c) 본편 폴더와 본편 모드로 되돌린다. 메인메뉴 상태 함수는 메뉴 안에서만 불린다.
// - 해금: 스토리 다시보기의 최종장(퀘스트 34 완료) 기록. 본편 세이브의 퀘스트 값으로도 소급한다(replay.c).
// - 시험: 환경 변수 CHRONOS_TURN이면 처음부터 켜고 폴더는 바꾸지 않는다(--save 그대로).
#include "restored.h"
#include "turn.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

bool replay_final_seen(void);
void turn_set_enabled(bool on);
void turn_reset_state(void);

static char base_dir[1024];
static bool switched;

bool turn_env_forced(void) { return getenv("CHRONOS_TURN") != NULL; }

bool turn_unlocked(void) { return turn_env_forced() || replay_final_seen(); }

// 메인메뉴에서 턴제 모드를 골랐다.
void turn_begin(void) {
    if (turn_env_forced()) return;
    if (!switched) {
        snprintf(base_dir, sizeof base_dir, "%s", save_dir);
        snprintf(save_dir, sizeof save_dir, "%s/turn", base_dir);
        mkdir(save_dir, 0755);
        switched = true;
    }
    turn_reset_state();
    turn_set_enabled(true);
    fprintf(stderr, "[turn] 턴제 모드 시작, 저장 폴더 %s\n", save_dir);
}

// 메인메뉴에서 턴제 모드가 아닌 동작을 했다. 턴제 게임에서 돌아왔으면 본편으로 되돌린다.
void turn_back_to_main(void) {
    if (!switched) return;
    snprintf(save_dir, sizeof save_dir, "%s", base_dir);
    switched = false;
    turn_set_enabled(false);
    turn_reset_state();
    fprintf(stderr, "[turn] 본편으로 돌아옴, 저장 폴더 %s\n", save_dir);
}
