// 내장 서버: 4 백업, 복구, 코스튬 전송.
#include "host.h"
#include "localnet.h"
#include <stdio.h>
#include <string.h>

enum {
    BACKUP_SIZE = 423,     // 4.1 본문: be32, 7, be32, 6*be32, 4, be16, be32, 1, 3, 동료 10*10, 소지품 64*3, 표 39*2
    BACKUP_SAVE_AT = 11,   // 0x143b70이 돌려준 세이브 읽은 길이. 0이면 세이브가 없다.
    PICTURE_SIZE = 7000,   // 0x134918 반환값
    CHUNK_MAX = 1000,      // 0x120f3e
    SCREEN_INFO = 0x19c8d4 + 0x2c, // 화면 정보 포인터. +0x86이 0x134918의 복사 방식이다.
    SCREEN_MODE = 0x86,
    PNG_MAX = 16384,
};

static const char MSG_NO_BACKUP[] = "\xba\xb9\xb1\xb8\xc7\xd2\x20\xb5\xa5\xc0\xcc\xc5\xcd\xb0\xa1\x20\xbe\xf8\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 복구할 데이터가 없습니다.
static const char MSG_SAVED[] = "\xc4\xda\xbd\xba\xc6\xac\x20\xb1\xd7\xb8\xb2\xc0\xbb\x20\xc0\xfa\xc0\xe5\xc7\xdf\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 코스튬 그림을 저장했습니다.
static const char MSG_SAVE_FAILED[] = "\xc4\xda\xbd\xba\xc6\xac\x20\xb1\xd7\xb8\xb2\xc0\xbb\x20\xc0\xfa\xc0\xe5\xc7\xcf\xc1\xf6\x20\xb8\xf8\xc7\xdf\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 코스튬 그림을 저장하지 못했습니다.

static uint8_t picture[PICTURE_SIZE];
static uint32_t picture_len;

static void put_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}

static uint32_t crc32_of(const uint8_t *p, size_t n) {
    uint32_t c = 0xffffffffu;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++) c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1;
    }
    return ~c;
}

static size_t png_chunk(uint8_t *o, const char *type, const uint8_t *data, size_t n) {
    put_be32(o, (uint32_t)n);
    memcpy(o + 4, type, 4);
    if (n) memcpy(o + 8, data, n);
    put_be32(o + 8 + n, crc32_of(o + 4, n + 4));
    return n + 12;
}

// RGB565 그림을 비압축 PNG로 만든다. 압축 라이브러리를 들이지 않으려고 stored 블록 하나로 쓴다.
static size_t png_encode(uint8_t *o, const uint8_t *rgb565, int w, int h) {
    static const uint8_t SIGNATURE[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    static uint8_t raw[PNG_MAX], idat[PNG_MAX];
    uint8_t ihdr[13] = {0};
    size_t n = 0, raw_len = 0;
    for (int y = 0; y < h; y++) {
        raw[raw_len++] = 0;
        for (int x = 0; x < w; x++) {
            const uint8_t *s = rgb565 + (size_t)(y * w + x) * 2;
            unsigned v = s[0] | (unsigned)s[1] << 8;
            raw[raw_len++] = (uint8_t)((v >> 11) * 255 / 31);
            raw[raw_len++] = (uint8_t)(((v >> 5) & 63) * 255 / 63);
            raw[raw_len++] = (uint8_t)((v & 31) * 255 / 31);
        }
    }
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw_len; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
    idat[0] = 0x78; idat[1] = 0x01;
    idat[2] = 1; // 마지막 stored 블록
    idat[3] = (uint8_t)raw_len; idat[4] = (uint8_t)(raw_len >> 8);
    idat[5] = (uint8_t)~raw_len; idat[6] = (uint8_t)(~raw_len >> 8);
    memcpy(idat + 7, raw, raw_len);
    put_be32(idat + 7 + raw_len, b << 16 | a);
    put_be32(ihdr, (uint32_t)w);
    put_be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8; ihdr[9] = 2; // 8비트 RGB
    memcpy(o, SIGNATURE, 8);
    n = 8;
    n += png_chunk(o + n, "IHDR", ihdr, sizeof ihdr);
    n += png_chunk(o + n, "IDAT", idat, 7 + raw_len + 4);
    n += png_chunk(o + n, "IEND", NULL, 0);
    return n;
}

// 0x134918: 방식 1은 70x50, 방식 2는 50x70으로 잘라 16비트로 담는다.
static bool picture_save(void) {
    static uint8_t png[PNG_MAX + 64];
    bool wide = cpu.m[R32_(&cpu, SCREEN_INFO) + SCREEN_MODE] == 1;
    size_t n = png_encode(png, picture, wide ? 70 : 50, wide ? 50 : 70);
    return ln_save("costume.png", png, n);
}

// 0x154ae4: u8 마지막 조각, be32 위치, be32 크기, 데이터. 0x120f0a와 0x122074가 1000바이트씩 앞에서부터 보낸다.
// 응답 0x154f50은 마지막 조각일 때만 안내문을 띄우고, 결과 -1이면 안내문을 띄운 뒤 전송을 멈춘다(0x12203e).
static void costume(ln_req *q) {
    uint8_t last = ln_u8(q);
    uint32_t at = ln_u32(q), size = ln_u32(q);
    bool ok = size <= CHUNK_MAX && size == (size_t)(q->end - q->p) && at + size <= PICTURE_SIZE && at == (at ? picture_len : 0);
    if (!ok) {
        picture_len = 0;
        ln_fail(4, 3, MSG_SAVE_FAILED);
        return;
    }
    ln_bytes(q, picture + at, size);
    picture_len = at + size;
    if (!last) {
        ln_begin(4, 3, 0);
        ln_w16(0);
        ln_send();
        return;
    }
    if (picture_len != PICTURE_SIZE || !picture_save()) {
        picture_len = 0;
        ln_fail(4, 3, MSG_SAVE_FAILED);
        return;
    }
    picture_len = 0;
    ln_begin(4, 3, 0);
    ln_wstr16(MSG_SAVED);
    ln_send();
}

static void upload(ln_req *q) {
    size_t n = (size_t)(q->end - q->p);
    ln_upload_set(q->p, n);
    // 세이브 파일이 없는 새 설치는 빈 값이 올라온다. 그 값으로 복구 자료를 덮지 않는다.
    if (n == BACKUP_SIZE) {
        const uint8_t *s = q->p + BACKUP_SAVE_AT;
        int32_t saved = (int32_t)((uint32_t)s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
        if (saved > 0 && !ln_save("backup", q->p, n)) fprintf(stderr, "[net] 복구 자료 저장 실패\n");
    }
    // 0x154b88: 결과가 0이든 -1이든 다음은 공지 요청이다.
    ln_begin(4, 1, 0);
    ln_send();
}

// 0x154be4는 4.1 본문과 같은 순서로 읽어 캐릭터에 넣는다. 첫 값은 OPT+0x10으로, 다음 4.1에 다시 실려 온다.
static void restore(void) {
    static uint8_t body[BACKUP_SIZE];
    if (ln_load("backup", body, sizeof body) != BACKUP_SIZE) {
        ln_fail(4, 2, MSG_NO_BACKUP);
        return;
    }
    ln_begin(4, 2, 0);
    ln_wbytes(body, sizeof body);
    ln_send();
}

bool ln_backup(ln_req *q) {
    switch (q->minor) {
    case 1: upload(q); return true;
    case 2: restore(); return true;
    case 3: costume(q); return true;
    }
    return false;
}
