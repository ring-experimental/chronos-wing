// 내장 서버. 게임이 소켓으로 보내는 요청을 프로세스 안에서 받아 응답한다.
// 패킷: 길이(be32, 머리 포함), 대분류(be16), 소분류(be32), 결과(u8), 예비(u8), 본문.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int major, minor;
    uint8_t flag;
    const uint8_t *p, *end;
} ln_req;

// 요청 읽기. 모자라면 0을 돌려주고 로그를 남긴다.
uint8_t ln_u8(ln_req *q);
uint16_t ln_u16(ln_req *q);
uint32_t ln_u32(ln_req *q);
void ln_bytes(ln_req *q, void *out, size_t n);

// 응답 쓰기. ln_begin으로 시작해 ln_send로 보낸다. 값은 big endian이다.
void ln_begin(int major, int minor, int result);
void ln_w8(uint8_t v);
void ln_w16(uint16_t v);
void ln_w32(uint32_t v);
void ln_w64(uint64_t v);
void ln_wbytes(const void *p, size_t n);
void ln_wstr16(const char *euckr); // be16 길이 + 내용
void ln_send(void);

// 실패 응답: 결과 -1, be16 길이 + 안내문. 처리기 대부분이 이 형식이다.
void ln_fail(int major, int minor, const char *euckr);

// 저장 폴더 net/<name>에 묶음을 읽고 쓴다. 읽기는 실제 크기를 돌려주고 없으면 -1이다.
long ln_load(const char *name, void *out, size_t cap);
bool ln_save(const char *name, const void *p, size_t n);

// 공통 상태(localnet_state.c). 저장은 net/ 아래 profile, members, upload다.
// 프로필은 응답 0x155070이 NS(0x20d758)+0x1d0~에 그대로 풀어 넣는 값이다. 괄호는 그 오프셋, 근거는 work/net-offline.md.
// 시간(분) 필드는 -1이면 표시하지 않는다. 0x120ff8 화면이 0x158fc8로 일/시/분으로 나눈다.
typedef struct {
    uint32_t magic;
    int32_t user_id;       // +0x1d0 읽는 코드 없음. 고정 1
    char nick[11];         // +0x1d4 10바이트. 비어 있으면 첫 접속이다.
    char guild[13];        // +0x1df 12바이트. 첫 바이트가 0이면 길드 줄을 안 그린다.
    int32_t np;            // +0x1ec
    int32_t level;         // +0x1f0 응답은 be32인데 게임은 u8로 자른다.
    int32_t fame;          // +0x1f4 명성치
    int32_t fame_today;    // +0x1f8 TODAY 증감, 음수면 -로 표시
    int32_t rank;          // +0x1fc RANK
    int32_t rank_today;    // +0x200
    int32_t hit, defend;   // +0x204 피격, +0x208 방어. 방어율은 방어*100/피격으로 게임이 계산한다.
    int32_t hit_month, defend_month; // +0x20c, +0x210 MONTH
    int32_t limit_attack, limit_counter, limit_double; // +0x214~ 나의 제한시간(초): 일반공격, 카운터, 더블카운터
    uint8_t sleeping;      // +0x22c 1이면 닉네임 옆에 휴면상태
    char grade[11];        // +0x22d 등급 이름 10바이트
    int32_t fame_max;      // +0x238 명성치 분모
    uint8_t sms;           // +0x23c SMS 설정(1.33 응답 0x152aec도 쓴다)
    uint8_t auto_protect;  // +0x240 자동보호 단계, 0이면 줄을 안 그린다
    int32_t sleep_min;     // +0x244 휴면설정 남은 분
    int32_t shield_min;    // +0x248 보호쉴드 남은 분
    int32_t np_regen_min;  // +0x24c NP 회복 남은 분
    int32_t np_bonus_min;  // +0x250 NP 보너스 남은 분
    uint8_t double_counter; // +0x254 더블카운터 남은 개수
} ln_profile;

// 프로필을 처음 쓸 때 net/profile에서 읽는다. 없으면 초기값이고 nick이 비어 있다.
ln_profile *ln_me(void);
void ln_me_save(void);
bool ln_has_account(void);
// NP를 더하고(음수면 뺀다) 0 아래로는 안 내려간다. 저장까지 하고 새 값을 돌려준다. 응답에 넣을 값은 p->np를 쓴다.
int32_t ln_np_add(int32_t delta);
// NP가 충분하면 빼고 true. 모자라면 그대로 두고 false.
bool ln_np_spend(int32_t cost);
// 명성치를 더하고 TODAY에 누적한다. fame_max를 넘으면 레벨을 올린다(fame_max는 레벨*100, 내가 정한 규칙).
void ln_fame_add(int32_t delta);
// 응답 본문에 0x155070 형식으로 프로필을 쓴다.
void ln_write_profile(void);

// 방어 부대 동료. 0x155210이 표 0x1a9e58(0x74바이트 단위)을 앞에서부터 덮어쓰는 값이다.
enum { LN_MEMBER_MAX = 8 };
typedef struct {
    int64_t uid;           // +0x68 양수면 일반 배치 비용(0xf3c). 음수 작은 값은 용병 종류 구간이다(0x164b70).
    int16_t id;            // +0x48 /DAT/SL의 값. 요청 10.10이 실어 온다.
    int8_t level;          // +0x35
    char name[27];         // +0x4 1바이트 길이 + 내용으로 나간다
    int16_t floor, room, slot; // +0x4e, +0x50, +0x52 배치 위치. 셋 다 0 이상일 때만 배치된 것(0x164b70). 없으면 -1
    uint8_t fatigue;       // +0x4a 피로도 0~100. 100이면 회색
    int32_t hp;            // +0x24
    int32_t attack;        // +0x5c 목록의 첫 값
    int16_t pdef, mdef;    // +0x30 물리방어, +0x32 마도방어
    int8_t crit;           // +0x34 치명타 %
} ln_member;

// 동료 목록. n에 개수를 돌려준다. 바꾼 뒤에는 ln_members_save를 부른다.
ln_member *ln_members(int *n);
void ln_members_save(void);
// 응답 본문에 0x155210 형식으로 쓴다(be16 개수 + 항목).
void ln_write_members(void);
// 요청 10.10의 (id, 레벨) 8쌍과 마지막 4.1의 합류 여부로 목록을 맞춘다. 배치와 피로도는 uid(슬롯+1)로 이어받는다.
void ln_members_sync(const int16_t *ids, const int8_t *levels, int n);

// 마지막 4.1 요청 본문(net/upload). 없으면 NULL. 형식은 work/net-offline.md의 세션 절.
const uint8_t *ln_upload(size_t *n);
void ln_upload_set(const uint8_t *body, size_t n);
// 동료 슬롯(0~9)의 합류 여부. 4.1 본문 오프셋 53부터 슬롯마다 10바이트다.
bool ln_upload_joined(int slot);

// 기능별 처리기. 처리했으면 true, 모르는 소분류면 false.
bool ln_session(ln_req *q); // 대분류 10
bool ln_mail(ln_req *q);    // 대분류 1, 소분류 1~11
bool ln_staff(ln_req *q);   // 대분류 1, 소분류 20~27
bool ln_sleep(ln_req *q);   // 대분류 1, 소분류 30~33
bool ln_cash(ln_req *q);    // 대분류 1, 소분류 40~41
void ln_cash_tick(void);    // 보호쉴드 남은 분을 다시 계산한다. 프로필을 쓰기 전에 부른다.
bool ln_shadow(ln_req *q);  // 대분류 2
bool ln_depot(ln_req *q);   // 대분류 3, 소분류 1~8
bool ln_auction(ln_req *q); // 대분류 3, 소분류 20~21
bool ln_backup(ln_req *q);  // 대분류 4

// api_net에서 부른다.
void localnet_api(uint32_t slot);
