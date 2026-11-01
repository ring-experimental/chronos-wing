// 내장 서버: 1(30~33) 휴면과 SMS 설정. 필드 근거와 정한 값은 work/net-offline.md의 휴면, SMS 절.
#include "localnet.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

enum {
    STATE_MAGIC = 0x4c4e5331,
    SMS_NONE = -1, // 1.33 요청의 조회값이자 응답의 "건수 없음"(0x18264a, 0x182aae)
    SMS_MAX = 3,   // 비트 1 공격정보, 2 상태정보(0x182e74, 표 0x99468)
};

// 끝나는 시각은 프로필과 따로 둔다. 프로필에는 응답 형식인 남은 분만 있어 실제 시간이 흐른 것을 알 수 없다.
typedef struct {
    uint32_t magic;
    int32_t sms_count; // +0x23e. 0x155070이 안 쓰고 1.33 응답으로만 온다. 구매 전에는 0이다.
    int64_t sleep_until, shield_until;
} sleep_state;

static sleep_state st;
static bool st_loaded;

// 서버에만 있던 값이라 정했다. 0x1825c4의 창 높이는 2, 3개만 있고 이름 칸(NS+0x2675, 9바이트 단위)과 포인터 표 0x19ba48도 3개다.
// 값은 고른 항목에서 1.31 요청으로 그대로 돌아온다(NS+0x2690). 시간 단위로 정했다.
static const struct {
    const char *name;
    int8_t hours;
} periods[3] = {
    {"8\xbd\xc3\xb0\xa3", 8}, // 8시간
    {"1\xc0\xcf", 24},        // 1일
    {"3\xc0\xcf", 72},        // 3일
};

static const char MSG_BUSY[] = "\xc8\xde\xb8\xe9\x20\xc1\xdf\xbf\xa1\xb4\xc2\x20\xba\xaf\xb0\xe6\xc7\xd2\x20\xbc\xf6\x20\xbe\xf8\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 휴면 중에는 변경할 수 없습니다.
static const char MSG_BAD_PERIOD[] = "\xc0\xdf\xb8\xf8\xb5\xc8\x20\xb1\xe2\xb0\xa3\xc0\xd4\xb4\xcf\xb4\xd9\x2e"; // 잘못된 기간입니다.
static const char MSG_SLEEP_SET[] = " \xc8\xde\xb8\xe9\xc0\xcc\x20\xbc\xb3\xc1\xa4\xb5\xc7\xbe\xfa\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // (기간) 휴면이 설정되었습니다.
static const char MSG_SMS_SET[] = "\xbc\xb3\xc1\xa4\xb5\xc7\xbe\xfa\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 설정되었습니다.
static const char MSG_SMS_NONE[] = "\xb3\xb2\xc0\xba\x20\xbe\xcb\xb8\xb2\x20\xb0\xc7\xbc\xf6\xb0\xa1\x20\xbe\xf8\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 남은 알림 건수가 없습니다.

static sleep_state *state(void) {
    if (!st_loaded) {
        st_loaded = true;
        if (ln_load("sleep", &st, sizeof st) != (long)sizeof st || st.magic != STATE_MAGIC) {
            memset(&st, 0, sizeof st);
            st.magic = STATE_MAGIC;
        }
    }
    return &st;
}

static void state_save(void) {
    if (!ln_save("sleep", state(), sizeof st)) fprintf(stderr, "[net] 휴면 상태 저장 실패\n");
}

// 남은 분. 끝났으면 -1(표시 안 함). 1초라도 남으면 1분으로 친다.
static int32_t minutes_left(int64_t until) {
    int64_t left = until - (int64_t)time(NULL);
    return left > 0 ? (int32_t)((left + 59) / 60) : -1;
}

void ln_sleep_tick(void) {
    ln_profile *p = ln_me();
    const sleep_state *s = state();
    p->sleep_min = minutes_left(s->sleep_until);
    p->shield_min = minutes_left(s->shield_until);
    p->sleeping = p->sleep_min >= 0;
}

bool ln_is_sleeping(void) { return minutes_left(state()->sleep_until) >= 0; }

bool ln_is_protected(void) { return ln_is_sleeping() || minutes_left(state()->shield_until) >= 0; }

void ln_shield_add(int minutes) {
    sleep_state *s = state();
    int64_t now = (int64_t)time(NULL);
    s->shield_until = (s->shield_until > now ? s->shield_until : now) + (int64_t)minutes * 60;
    state_save();
}

void ln_sms_add(int count) {
    state()->sms_count += count;
    state_save();
}

static void sleep_list(void) {
    if (ln_is_sleeping()) { // 게임은 기간 중인지 안 본다(0x1825b8). 확인 문구대로 해제와 변경을 막는다.
        ln_fail(1, 30, MSG_BUSY);
        return;
    }
    ln_begin(1, 30, 0);
    ln_w8(3);
    for (int i = 0; i < 3; i++) {
        size_t len = strlen(periods[i].name);
        ln_w8((uint8_t)len);
        ln_wbytes(periods[i].name, len);
        ln_w8((uint8_t)periods[i].hours);
    }
    ln_send();
}

static void sleep_set(ln_req *q) {
    int8_t hours = (int8_t)ln_u8(q);
    if (ln_is_sleeping()) {
        ln_fail(1, 31, MSG_BUSY);
        return;
    }
    const char *name = NULL;
    for (int i = 0; i < 3; i++)
        if (periods[i].hours == hours) name = periods[i].name;
    if (!name) {
        ln_fail(1, 31, MSG_BAD_PERIOD);
        return;
    }
    // 보호쉴드도 휴면이 끝날 때까지 늘린다. 공격 차단을 내 정보에 보이기 위해 정한 규칙이다.
    sleep_state *s = state();
    s->sleep_until = (int64_t)time(NULL) + (int64_t)hours * 3600;
    if (s->shield_until < s->sleep_until) s->shield_until = s->sleep_until;
    state_save();
    ln_sleep_tick();
    ln_me_save();
    // 0x15277c는 안내문만 읽고 NS+0x22c를 스스로 1로 둔다. 남은 분은 다음 10.13에서 간다.
    ln_begin(1, 31, 0);
    // 게임은 항상 첫 항목 값을 보낸다(절 참조). 실제로 걸린 기간을 안내문에 적는다.
    char msg[64];
    snprintf(msg, sizeof msg, "%s%s", name, MSG_SLEEP_SET);
    ln_wstr16(msg);
    ln_send();
}

static void sms_setting(ln_req *q) {
    int value = (int8_t)ln_u8(q);
    const sleep_state *s = state();
    ln_profile *p = ln_me();
    if (value != SMS_NONE) {
        if (value < 0 || value > SMS_MAX || s->sms_count <= 0) {
            ln_fail(1, 33, MSG_SMS_NONE);
            return;
        }
        p->sms = (uint8_t)value;
        ln_me_save();
    }
    // 0x152aec: 설정 u8, 건수 s16, 안내문. 조회(요청 전 NS+0x23c가 -1)는 안내문을 안 띄운다.
    // 건수가 0이면 설정 -1을 보내야 0x182aae가 "SMS 알리미를 구매하세요"를 그린다.
    ln_begin(1, 33, 0);
    ln_w8(s->sms_count > 0 ? p->sms : (uint8_t)SMS_NONE);
    ln_w16((uint16_t)s->sms_count);
    ln_wstr16(value == SMS_NONE ? "" : MSG_SMS_SET);
    ln_send();
}

bool ln_sleep(ln_req *q) {
    switch (q->minor) {
    case 30: sleep_list(); return true;
    case 31: sleep_set(q); return true;
    case 33: sms_setting(q); return true;
    }
    return false;
}
