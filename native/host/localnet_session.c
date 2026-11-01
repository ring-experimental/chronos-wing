// 내장 서버: 10 세션 공지 닉네임.
#include "host.h"
#include "localnet.h"
#include <string.h>

enum {
    OPT_SERVER = 0x20d2d8 + 0xb, // 0x1483c8. 팝업에서 고른 서버, 1 테기 2 리키
    REQ_NICK_AT = 30,            // 10.2 본문: 전화번호 12, 2바이트, 4바이트, 12바이트 뒤에 닉네임 10
    SERVER_RIKI = 2,
};

// 10.1, 10.2 응답의 안내문(0x150770, 0x150a30)은 로그로만 나가고 화면에 안 나온다. 입력 칸은 게임이 띄운다.
static const char MSG_NEED_NICK[] = "\xb4\xd0\xb3\xd7\xc0\xd3\xc0\xbb\x20\xc0\xd4\xb7\xc2\xc7\xcf\xbc\xbc\xbf\xe4\x2e"; // 닉네임을 입력하세요.
static const char MSG_NO_NOTICE[] = "\xb0\xf8\xc1\xf6\xb3\xbb\xbf\xeb\xc0\xcc\x20\xbe\xf8\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 공지내용이 없습니다.
static const char MSG_DONE[] = "\xb1\xb8\xc0\xd4\xb5\xc7\xbe\xfa\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 구입되었습니다.
static const char MSG_SAVED[] = "\xbc\xb3\xc1\xa4\xb5\xc7\xbe\xfa\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 설정되었습니다.

static void login(ln_req *q) {
    // 결과 0: 닉네임 + 다음 단계. -1: 계정 없음이라 요청 종류 0,1은 닉네임 입력(0x150770).
    // -2는 안내문을 띄우고 끊는 차단 응답이다. 내장 서버는 쓸 일이 없다.
    (void)q;
    if (!ln_has_account()) {
        ln_fail(10, 1, MSG_NEED_NICK);
        return;
    }
    ln_begin(10, 1, 0);
    ln_wbytes(ln_me()->nick, 10);
    ln_send();
}

static void create_nick(ln_req *q) {
    char nick[11] = {0};
    q->p += REQ_NICK_AT;
    ln_bytes(q, nick, 10);
    // 입력 칸은 한글 6자 이상도 받는다. 10바이트에서 잘려 반쪽이 된 글자를 버린다.
    for (int i = 0; i < 10 && nick[i]; i++)
        if ((nick[i] & 0x80) && ++i >= 10) nick[9] = 0;
    ln_profile *p = ln_me();
    memcpy(p->nick, nick, sizeof p->nick);
    ln_me_save();
    ln_begin(10, 2, 0);
    ln_wbytes(p->nick, 10);
    ln_send();
}

static void notice(void) {
    // 개수 0이면 0x150b30이 공지 포인터를 만들지 않아 화면(0x1574b4)이 널을 읽는다. 원본 폰은 0번지를 그냥 읽고 지나갔겠지만 여기서는 죽는다.
    // 그래서 한 건을 보낸다. 0x157e4c가 보여 주려던 문구와 같다. OK 한 번이면 0x1590e0이 10.10으로 넘어간다.
    ln_begin(10, 4, 0);
    ln_w16(1);
    ln_wstr16(MSG_NO_NOTICE);
    ln_send();
}

static void start_ship(ln_req *q) {
    int16_t ids[LN_MEMBER_MAX];
    int8_t levels[LN_MEMBER_MAX];
    ln_u8(q); // 주인공 +0x101
    int n = ln_u8(q);
    if (n > LN_MEMBER_MAX) n = LN_MEMBER_MAX;
    for (int i = 0; i < n; i++) {
        ids[i] = (int16_t)ln_u16(q);
        levels[i] = (int8_t)ln_u8(q);
    }
    ln_members_sync(ids, levels, n);
    ln_begin(10, 10, 0);
    ln_write_profile();
    ln_write_members();
    // 리키 서버만 배치 비용 7바이트를 받는다. 테기 서버는 게임이 12,3,2,1,1,1,3으로 고정한다(0x150f40). 같은 값을 쓴다.
    if (cpu.m[OPT_SERVER] == SERVER_RIKI) {
        static const uint8_t cost[7] = {12, 3, 2, 1, 1, 1, 3};
        ln_wbytes(cost, sizeof cost);
    }
    ln_send();
}

bool ln_session(ln_req *q) {
    switch (q->minor) {
    case 1: login(q); return true;
    case 2: create_nick(q); return true;
    case 4: notice(); return true;
    case 10: start_ship(q); return true;
    case 13:
        ln_begin(10, 13, 0);
        ln_write_profile();
        ln_send();
        return true;
    case 20: // 0x1511ec. 지급은 게임 코드가 한다.
        ln_begin(10, 20, 0);
        ln_wstr16(MSG_DONE);
        ln_send();
        return true;
    case 30: // 0x151310
        ln_begin(10, 30, 0);
        ln_wstr16(MSG_SAVED);
        ln_send();
        return true;
    }
    return false;
}
