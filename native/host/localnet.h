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
void ln_wbytes(const void *p, size_t n);
void ln_wstr16(const char *euckr); // be16 길이 + 내용
void ln_send(void);

// 실패 응답: 결과 -1, be16 길이 + 안내문. 처리기 대부분이 이 형식이다.
void ln_fail(int major, int minor, const char *euckr);

// 저장 폴더 net/<name>에 묶음을 읽고 쓴다. 읽기는 실제 크기를 돌려주고 없으면 -1이다.
long ln_load(const char *name, void *out, size_t cap);
bool ln_save(const char *name, const void *p, size_t n);

// 기능별 처리기. 처리했으면 true, 모르는 소분류면 false.
bool ln_session(ln_req *q); // 대분류 10
bool ln_mail(ln_req *q);    // 대분류 1, 소분류 1~11
bool ln_staff(ln_req *q);   // 대분류 1, 소분류 20~27
bool ln_sleep(ln_req *q);   // 대분류 1, 소분류 30~33
bool ln_cash(ln_req *q);    // 대분류 1, 소분류 40~41
bool ln_shadow(ln_req *q);  // 대분류 2
bool ln_depot(ln_req *q);   // 대분류 3, 소분류 1~8
bool ln_auction(ln_req *q); // 대분류 3, 소분류 20~21
bool ln_backup(ln_req *q);  // 대분류 4

// api_net에서 부른다.
void localnet_api(uint32_t slot);
