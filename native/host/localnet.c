// 내장 서버의 소켓 계층. 소켓 표 슬롯은 0 netConnect, 1 netClose, 2 socket, 6 socketClose, 13 읽기 콜백, 14 쓰기 콜백,
// 30 connect, 31 write, 32 read이고 모두 게임의 netlair(0x155d1e~)가 부른다. 표는 work/wipi-api.md 참고.
#include "host.h"
#include "localnet.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

enum { SOCKET_FD = 1, E_WOULD_BLOCK = -0x13, PACKET_MAX = 0x800, HEADER = 12 };

static uint8_t in[PACKET_MAX * 4], out[PACKET_MAX * 64], building[PACKET_MAX];
static size_t in_len, out_len, out_pos, building_len;
static uint32_t read_cb, read_param;
static bool socket_open, peer_closed, net_log, net_log_read;

#define LOG(...) do { if (net_log) fprintf(stderr, "[net] " __VA_ARGS__); } while (0)

uint8_t ln_u8(ln_req *q) {
    if (q->p + 1 > q->end) { LOG("요청 %d,%d 끝을 넘어 읽음\n", q->major, q->minor); return 0; }
    return *q->p++;
}
uint16_t ln_u16(ln_req *q) { uint16_t a = ln_u8(q); return (uint16_t)(a << 8 | ln_u8(q)); }
uint32_t ln_u32(ln_req *q) { uint32_t a = ln_u16(q); return a << 16 | ln_u16(q); }
void ln_bytes(ln_req *q, void *o, size_t n) { uint8_t *d = o; while (n--) *d++ = ln_u8(q); }

void ln_w8(uint8_t v) {
    if (building_len >= PACKET_MAX) { fprintf(stderr, "[net] 응답이 패킷 크기를 넘음\n"); return; }
    building[building_len++] = v;
}
void ln_w16(uint16_t v) { ln_w8((uint8_t)(v >> 8)); ln_w8((uint8_t)v); }
void ln_w32(uint32_t v) { ln_w16((uint16_t)(v >> 16)); ln_w16((uint16_t)v); }
void ln_w64(uint64_t v) { ln_w32((uint32_t)(v >> 32)); ln_w32((uint32_t)v); }
void ln_wbytes(const void *p, size_t n) { const uint8_t *s = p; while (n--) ln_w8(*s++); }
void ln_wstr16(const char *s) { size_t n = strlen(s); ln_w16((uint16_t)n); ln_wbytes(s, n); }

void ln_begin(int major, int minor, int result) {
    building_len = 0;
    ln_w32(0);
    ln_w16((uint16_t)major);
    ln_w32((uint32_t)minor);
    ln_w8((uint8_t)result);
    ln_w8(0);
}

// 큐를 비운 직후 콜백을 예약한다. 게임은 읽기 실패(E_WOULD_BLOCK) 뒤 콜백을 걸고 기다린다.
static void wake_reader(void) {
    if (read_cb && (out_pos < out_len || peer_closed)) {
        uint32_t cb = read_cb;
        read_cb = 0;
        defer_call(cb, SOCKET_FD, 0, 1);
    }
}

void ln_send(void) {
    uint32_t n = (uint32_t)building_len;
    building[0] = (uint8_t)(n >> 24); building[1] = (uint8_t)(n >> 16); building[2] = (uint8_t)(n >> 8); building[3] = (uint8_t)n;
    if (out_pos == out_len) out_pos = out_len = 0;
    if (out_len + n > sizeof out) { fprintf(stderr, "[net] 응답 큐가 가득 참\n"); return; }
    memcpy(out + out_len, building, n);
    out_len += n;
    LOG("응답 %d,%d 결과 %d, %u바이트\n", building[4] << 8 | building[5], building[8] << 8 | building[9], (int8_t)building[10], n);
    wake_reader();
}

void ln_fail(int major, int minor, const char *euckr) {
    ln_begin(major, minor, -1);
    ln_wstr16(euckr);
    ln_send();
}

static void net_path(char *o, size_t cap, const char *name) {
    snprintf(o, cap, "%s/net", save_dir);
    mkdir(o, 0755);
    snprintf(o, cap, "%s/net/%s", save_dir, name);
}

long ln_load(const char *name, void *p, size_t cap) {
    char path[2048];
    net_path(path, sizeof path, name);
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    long n = (long)fread(p, 1, cap, f);
    fclose(f);
    return n;
}

bool ln_save(const char *name, const void *p, size_t n) {
    char path[2048], tmp[2100];
    net_path(path, sizeof path, name);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    bool ok = fwrite(p, 1, n, f) == n;
    ok = fclose(f) == 0 && ok;
    if (ok) {
        remove(path);
        ok = rename(tmp, path) == 0;
    }
    return ok;
}

static bool route(ln_req *q) {
    switch (q->major) {
    case 0: ln_shadow_tick(); return true; // 접속 유지. 응답은 없다.
    case 10: return ln_session(q);
    case 1:
        if (q->minor <= 11) return ln_mail(q);
        if (q->minor <= 27) return ln_staff(q);
        if (q->minor <= 33) return ln_sleep(q);
        return ln_cash(q);
    case 2: return ln_shadow(q);
    case 3: return q->minor < 20 ? ln_depot(q) : ln_auction(q);
    case 4: return ln_backup(q);
    }
    return false;
}

static void packets_run(void) {
    size_t pos = 0;
    while (in_len - pos >= HEADER) {
        const uint8_t *h = in + pos;
        uint32_t n = (uint32_t)h[0] << 24 | h[1] << 16 | h[2] << 8 | h[3];
        if (n < HEADER || n > PACKET_MAX) {
            fprintf(stderr, "[net] 잘못된 요청 길이 %u\n", n);
            pos = in_len;
            break;
        }
        if (in_len - pos < n) break;
        ln_req q = {h[4] << 8 | h[5], (int)((uint32_t)h[6] << 24 | h[7] << 16 | h[8] << 8 | h[9]), h[11], h + HEADER, h + n};
        LOG("요청 %d,%d %u바이트\n", q.major, q.minor, n);
        if (net_log) {
            for (uint32_t k = HEADER; k < n; k++) fprintf(stderr, "%02x", h[k]);
            if (n > HEADER) fputc('\n', stderr);
        }
        if (!route(&q)) {
            // 응답이 없으면 게임은 데이터 요청중에서 영원히 기다린다. 서버가 끊은 것으로 알려 끊김 처리(0x156308 -> 0x157b1c)로 보낸다.
            fprintf(stderr, "[net] 처리기 없는 요청 %d,%d, 연결을 끊는다\n", q.major, q.minor);
            peer_closed = true;
            wake_reader();
        }
        pos += n;
    }
    memmove(in, in + pos, in_len - pos);
    in_len -= pos;
}

void localnet_api(uint32_t slot) {
    cpu_t *c = &cpu;
    uint32_t *r = c->r;
    if (slot == 0) {
        static bool init;
        if (!init) { init = true; net_log = getenv("CHRONOS_NETLOG") != NULL; }
    }
    switch (slot) {
    case 0: defer_call(r[0], 0, r[1], 1); r[0] = 0; break;
    case 1: r[0] = 0; break;
    case 2:
        socket_open = true;
        peer_closed = false;
        in_len = out_len = out_pos = 0;
        read_cb = 0;
        r[0] = SOCKET_FD;
        break;
    case 6: socket_open = false; read_cb = 0; r[0] = 0; break;
    case 13: // SetReadCB(fd, cb, param)
        read_cb = r[1]; read_param = r[2];
        wake_reader();
        r[0] = 0;
        break;
    case 14: r[0] = 0; break; // 쓰기는 막히지 않으므로 콜백을 쓸 일이 없다.
    case 30: defer_call(arg(c, 4), SOCKET_FD, 0, 1); r[0] = 0; break; // Connect(fd, ip, port, 0, cb, param)
    case 31: { // Write(fd, buf, len)
        uint32_t n = r[2];
        if (!socket_open || in_len + n > sizeof in) { r[0] = (uint32_t)-1; break; }
        CK(r[1], n);
        memcpy(in + in_len, c->m + r[1], n);
        in_len += n;
        packets_run();
        r[0] = n;
        break;
    }
    case 32: { // Read(fd, buf, len)
        if (!socket_open) { r[0] = (uint32_t)-1; break; }
        size_t n = out_len - out_pos;
        if (!n) { r[0] = peer_closed ? (uint32_t)-1 : (uint32_t)E_WOULD_BLOCK; break; }
        if (n > r[2]) n = r[2];
        CK(r[1], (uint32_t)n);
        memcpy(c->m + r[1], out + out_pos, n);
        out_pos += n;
        r[0] = (uint32_t)n;
        break;
    }
    default:
        fprintf(stderr, "소켓 API %u 미구현\n", slot);
        r[0] = (uint32_t)-1;
    }
}
