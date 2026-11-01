// 캐릭터 관리 메뉴. 서버가 필요한 캐릭터 복구 팝업을 세이브 들여오기/내보내기로 바꾼다. 원본 0x1457d8, 0x145c34를 감싼다.
#include "restored.h"

void orig_f_457d8(cpu_t *c);
void orig_f_45c34(cpu_t *c);

enum {
    GOT_MENU_STATE = 0xb8c,
    GOT_POPUP = 0x1e8,
    GOT_RECOVER_TEXT = 0xbc0,
    YES_NO_LABELS = 0x19b148,
    MENU_RECOVER = 3,
    SUB_POPUP = 0,
    SUB_IDLE = 8,
    FN_POPUP_OPEN = 0x1722b0,
    FN_POPUP_CLOSE = 0x172330,
    FN_POPUP_SELECTED = 0x172338,
};

static bool confirming;

static bool recover_popup(cpu_t *c) {
    uint32_t state = got(GOT_MENU_STATE);
    return R8(state) == MENU_RECOVER && R8(state + 1) == SUB_POPUP;
}

void savefile_ask_overwrite(void) {
    cpu_t *c = &cpu;
    confirming = true;
    W8(got(GOT_MENU_STATE) + 1, SUB_POPUP);
    CALL(FN_POPUP_OPEN, got(GOT_POPUP), 145, 2, 2, 1);
}

// 예, 아니오를 고른 뒤 매번 불린다.
void f_457d8(cpu_t *c) {
    if (!recover_popup(c)) { orig_f_457d8(c); return; }
    uint32_t popup = got(GOT_POPUP);
    int selected = (int)CALL(FN_POPUP_SELECTED, popup);
    bool was_confirming = confirming;
    confirming = false;
    W8(got(GOT_MENU_STATE) + 1, SUB_IDLE);
    CALL(FN_POPUP_CLOSE, popup);
    if (was_confirming) savefile_overwrite_answer(selected == 0);
    else savefile_choose(selected == 0);
}

// 팝업을 그릴 때만 글과 선택지를 바꿔 넣는다.
void f_45c34(cpu_t *c) {
    if (!recover_popup(c)) { confirming = false; orig_f_45c34(c); return; }
    uint32_t text_slot = GOT + GOT_RECOVER_TEXT, text = R32(text_slot);
    uint32_t first = R32(YES_NO_LABELS), second = R32(YES_NO_LABELS + 4);
    W32(text_slot, savefile_text(confirming ? SAVE_TEXT_CONFIRM : SAVE_TEXT_CHOOSE));
    if (!confirming) {
        W32(YES_NO_LABELS, savefile_text(SAVE_TEXT_IMPORT));
        W32(YES_NO_LABELS + 4, savefile_text(SAVE_TEXT_EXPORT));
    }
    orig_f_45c34(c);
    W32(text_slot, text);
    W32(YES_NO_LABELS, first);
    W32(YES_NO_LABELS + 4, second);
}
