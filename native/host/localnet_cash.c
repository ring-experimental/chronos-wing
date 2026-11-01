// 내장 서버: 1(40~41) 캐쉬샵. 근거는 work/net-offline.md의 캐쉬샵 절.
#include "localnet.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

enum { DELIVERY_RECORD = 4, DELIVERY_MAX = 64, SMS_ADD = 10, SMS_MAX = 9999, DOUBLE_ADD = 5, DOUBLE_MAX = 99, NP_ADD = 1000, DAY = 86400 };

typedef enum { TO_DEPOT, TO_NP, TO_DOUBLE, TO_SHIELD, TO_SMS } target;

typedef struct {
    int32_t code; // 1.41 요청으로 돌아오는 값
    const char *name, *desc;
    int32_t won;
    target to;
    uint8_t type, qty; // TO_DEPOT만 쓴다
    int16_t id;
} product;

// 앞 넷은 클라이언트에 있는 상품이다. 코드와 가격은 표 0x19b4a0, 0x19b4b8, 물품은 0x191c10~, 설명은 ITM/DAT/37, 26, 38.
// 스킬 초기화(6), 특성 초기화(7)는 뺐다. 실행 코드가 싱글 상인의 0x16a320뿐이라 서버 응답으로는 일어나지 않는다.
// 이름은 10바이트 안에서 끝낸다. 확인 창이 앞 10바이트만 그려 글자가 갈린다(0x15e528). 수량은 설명에 적었다.
// 뒤 넷은 클라이언트에 이름과 가격이 없어 정한 상품이다. 이름은 프로필 화면 0x120ff8과 안내 0x182be0의 낱말을 썼다.
static const product PRODUCTS[] = {
    {8, "\xba\xce\xc8\xb0\xbc\xae\x20\x31\x30", // 부활석 10
     "\x72\xc1\xfc\xb0\xa1\xb9\xe6\xbf\xa1\x20\xc0\xd6\xc0\xb8\xb8\xe9\x20\x71\xc1\xa6\xc0\xda\xb8\xae\x20\xba\xce\xc8\xb0\x20\x72\xb0\xa1\xb4\xc9", // r짐가방에 있으면 q제자리 부활 r가능
     500, TO_DEPOT, 0x25, 10, 0},
    {9, "\xba\xce\xc8\xb0\xbc\xae\x20\x32\x30", // 부활석 20
     "\x72\xc1\xfc\xb0\xa1\xb9\xe6\xbf\xa1\x20\xc0\xd6\xc0\xb8\xb8\xe9\x20\x71\xc1\xa6\xc0\xda\xb8\xae\x20\xba\xce\xc8\xb0\x20\x72\xb0\xa1\xb4\xc9",
     1000, TO_DEPOT, 0x25, 20, 0},
    {10, "\xb0\xed\xb4\xeb\xba\xf1\xbe\xe0\x32\x30", // 고대비약20
     "\x71\x48\x50\x72\xbf\xcd\x20\x71\x4d\x50\x72\xb0\xa1\x20\x70\x38\x30\x30\xbe\xbf\x20\x72\xc8\xb8\xba\xb9\xb5\xc8\xb4\xd9\x2e", // qHPr와 qMPr가 p800씩 r회복된다.
     500, TO_DEPOT, 0x1a, 20, 0},
    {11, "\xc7\xe3\xb8\xae\xc1\xd6\xb8\xd3\xb4\xcf", // 허리주머니
     "\x71\xc7\xe3\xb8\xae\xc1\xd6\xb8\xd3\xb4\xcf\x20\x70\x31\xc4\xad\x20\x72\xc3\xdf\xb0\xa1\x28\xc3\xd1\x20\x32\xc4\xad\xb1\xee\xc1\xf6\x29", // q허리주머니 p1칸 r추가(총 2칸까지)
     500, TO_DEPOT, 0x26, 1, 0},
    {101, "\x4e\x50\x20\x31\x30\x30\x30", // NP 1000
     "\x71\x4e\x50\x72\xb8\xa6\x20\x70\x31\x30\x30\x30\x20\x72\xc3\xe6\xc0\xfc\xc7\xd1\xb4\xd9\x2e", // qNPr를 p1000 r충전한다.
     1000, TO_NP, 0, 0, 0},
    {102, "\xb4\xf5\xba\xed\xc4\xab\xbf\xee\xc5\xcd", // 더블카운터
     "\x71\xb4\xf5\xba\xed\xc4\xab\xbf\xee\xc5\xcd\x20\x72\xb8\xa6\x20\x70\x35\xb0\xb3\x20\x72\xc3\xdf\xb0\xa1\xc7\xd1\xb4\xd9\x2e", // q더블카운터 r를 p5개 r추가한다.
     1000, TO_DOUBLE, 0, 0, 0},
    {103, "\xba\xb8\xc8\xa3\xbd\xaf\xb5\xe5", // 보호쉴드
     "\x71\xba\xb8\xc8\xa3\xbd\xaf\xb5\xe5\x20\x72\xb8\xa6\x20\x70\x31\xc0\xcf\x20\x72\xbf\xac\xc0\xe5\xc7\xd1\xb4\xd9\x2e", // q보호쉴드 r를 p1일 r연장한다.
     1000, TO_SHIELD, 0, 0, 0},
    {104, "\x53\x4d\x53\x20\xbe\xcb\xb8\xae\xb9\xcc", // SMS 알리미
     "\x71\x53\x4d\x53\x20\xbe\xcb\xb8\xb2\x20\x72\xb0\xc7\xbc\xf6\xb8\xa6\x20\x70\x31\x30\xb0\xc7\x20\x72\xc3\xdf\xb0\xa1\xc7\xd1\xb4\xd9\x2e", // qSMS 알림 r건수를 p10건 r추가한다.
     1000, TO_SMS, 0, 0, 0},
};
enum { PRODUCT_COUNT = (int)(sizeof PRODUCTS / sizeof *PRODUCTS) };

static const char MSG_BOUGHT[] = "\xb1\xb8\xc0\xd4\xb5\xc7\xbe\xfa\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 구입되었습니다.
static const char MSG_DEPOT[] = "\x70\xb1\xb8\xc0\xd4\xc7\xb0\x72\xc0\xba\x20\x71\xc3\xa2\xb0\xed\x72\xb7\xce\x3b\x70\xb9\xe8\xb4\xde\x72\xb5\xc7\xbe\xfa\xbd\xc0\xb4\xcf\xb4\xd9"; // p구입품r은 q창고r로;p배달r되었습니다 (0x16a9d0 문구 형식)
static const char MSG_NO_PRODUCT[] = "\xbe\xf8\xb4\xc2\x20\xbb\xf3\xc7\xb0\xc0\xd4\xb4\xcf\xb4\xd9\x2e"; // 없는 상품입니다.
static const char MSG_FULL[] = "\xb4\xf5\x20\xc0\xcc\xbb\xf3\x20\xb1\xb8\xc0\xd4\xc7\xd2\x20\xbc\xf6\x20\xbe\xf8\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 더 이상 구입할 수 없습니다.
static const char MSG_SAVE_FAILED[] = "\xc0\xfa\xc0\xe5\xbf\xa1\x20\xbd\xc7\xc6\xd0\xc7\xdf\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 저장에 실패했습니다.

// net/cash_delivery: 레코드마다 종류 u8, 번호 be16, 개수 u8. NT 창고(3.1) 처리기가 읽어 자기 목록으로 옮기고 비운다.
static bool deliver(const product *p) {
    uint8_t buf[DELIVERY_RECORD * DELIVERY_MAX + DELIVERY_RECORD];
    long n = ln_load("cash_delivery", buf, sizeof buf);
    if (n < 0) n = 0;
    if (n + DELIVERY_RECORD > DELIVERY_RECORD * DELIVERY_MAX) return false;
    buf[n] = p->type;
    buf[n + 1] = (uint8_t)(p->id >> 8);
    buf[n + 2] = (uint8_t)p->id;
    buf[n + 3] = p->qty;
    return ln_save("cash_delivery", buf, (size_t)n + DELIVERY_RECORD);
}

// 프로필의 분 값은 저장만 하면 줄지 않는다. 끝나는 시각을 net/cash_shield에 두고 프로필을 쓸 때마다 다시 계산한다.
void ln_cash_tick(void) {
    int64_t end;
    if (ln_load("cash_shield", &end, sizeof end) != (long)sizeof end) return;
    ln_profile *p = ln_me();
    int64_t left = end - (int64_t)time(NULL);
    int32_t min = left > 0 ? (int32_t)((left + 59) / 60) : -1;
    if (p->shield_min == min) return;
    p->shield_min = min;
    ln_me_save();
}

static bool shield_extend(void) {
    int64_t end = 0, now = (int64_t)time(NULL);
    if (ln_load("cash_shield", &end, sizeof end) != (long)sizeof end || end < now) end = now;
    end += DAY;
    if (!ln_save("cash_shield", &end, sizeof end)) return false;
    ln_cash_tick();
    return true;
}

// net/sms_left: 남은 건수 int32. 1.33(0x152aec의 be16)이 읽어 보낸다.
static bool sms_add(void) {
    int32_t left = 0;
    if (ln_load("sms_left", &left, sizeof left) != (long)sizeof left) left = 0;
    if (left + SMS_ADD > SMS_MAX) return false;
    left += SMS_ADD;
    return ln_save("sms_left", &left, sizeof left);
}

static void list(void) {
    ln_begin(1, 40, 0);
    ln_w8(PRODUCT_COUNT);
    for (int i = 0; i < PRODUCT_COUNT; i++) {
        const product *p = &PRODUCTS[i];
        size_t name = strlen(p->name), desc = strlen(p->desc);
        ln_w32((uint32_t)p->code);
        ln_w8((uint8_t)name);
        ln_wbytes(p->name, name);
        ln_w8((uint8_t)desc);
        ln_wbytes(p->desc, desc);
        ln_w32((uint32_t)p->won);
    }
    ln_send();
}

// 실제 결제는 없다. 지급은 전부 서버 몫이고 게임은 안내문만 띄운다(0x1529e0).
static void buy(ln_req *q) {
    int32_t code = (int32_t)ln_u32(q);
    const product *p = NULL;
    for (int i = 0; i < PRODUCT_COUNT; i++)
        if (PRODUCTS[i].code == code) p = &PRODUCTS[i];
    if (!p) {
        ln_fail(1, 41, MSG_NO_PRODUCT);
        return;
    }
    const char *done = MSG_BOUGHT;
    ln_profile *me = ln_me();
    switch (p->to) {
    case TO_DEPOT:
        if (!deliver(p)) {
            ln_fail(1, 41, MSG_FULL);
            return;
        }
        done = MSG_DEPOT;
        break;
    case TO_NP:
        ln_np_add(NP_ADD);
        break;
    case TO_DOUBLE:
        if (me->double_counter + DOUBLE_ADD > DOUBLE_MAX) {
            ln_fail(1, 41, MSG_FULL);
            return;
        }
        me->double_counter += DOUBLE_ADD;
        ln_me_save();
        break;
    case TO_SHIELD:
        if (!shield_extend()) {
            ln_fail(1, 41, MSG_SAVE_FAILED);
            return;
        }
        break;
    case TO_SMS:
        if (!sms_add()) {
            ln_fail(1, 41, MSG_FULL);
            return;
        }
        break;
    }
    ln_begin(1, 41, 0);
    ln_wstr16(done);
    ln_send();
}

bool ln_cash(ln_req *q) {
    switch (q->minor) {
    case 40: list(); return true;
    case 41: buy(q); return true;
    }
    return false;
}
