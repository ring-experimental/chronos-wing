// 세이브 들여오기/내보내기. 서비스가 끝난 서버 복구를 대신한다. 파일 선택만 운영체제 창을 쓰고 안내는 게임 UI로 띄운다.
#include "host.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { FN_SHOW_MESSAGE = 0x157d84, FN_HASH = 0x13fc74, FN_DECRYPT = 0x105908, NET_UI = 0x20d758, MESSAGE_OPEN = 3 };

// 묶음 형식: 식별값 8바이트, 파일 수, (이름 16바이트, 크기, 내용)*, 앞 전체의 FNV-1a 64비트. 정수는 리틀 엔디언이다.
// CERTIDATA는 기기 확인값이라 옮기지 않는다.
static const char MAGIC[8] = "CWSAVE1\n";
#define NAME_LEN 16
#define MAX_FILE (1024 * 1024)
static const char *const FILES[] = {"BAGSAVE", "GAMEDATA", "HERODATA", "MAINGAMEDATA", "OPTDATA", "POUCHSAVE", "STORAGESAVE0", "STORAGESAVE1", "STORAGESAVE2", "STORAGESAVE3"};
#define NFILES ((int)(sizeof FILES / sizeof FILES[0]))
static const char *const REQUIRED[] = {"GAMEDATA", "HERODATA", "MAINGAMEDATA"};

// 게임 글꼴이 읽는 EUC-KR이다.
static const char *const TEXTS[] = {
    "\xbc\xbc\xc0\xcc\xba\xea\x20\xc6\xc4\xc0\xcf\xc0\xbb\x20\xb5\xe9\xbf\xa9\xbf\xc0\xb0\xc5\xb3\xaa\x20\xb3\xbb\xba\xb8\xb3\xc0\xb4\xcf\xb4\xd9\x2e", // 세이브 파일을 들여오거나 내보냅니다.,
    "\xb5\xe9\xbf\xa9\xbf\xc0\xb1\xe2", // 들여오기,
    "\xb3\xbb\xba\xb8\xb3\xbb\xb1\xe2", // 내보내기,
    "\xc7\xf6\xc0\xe7\x20\xbc\xbc\xc0\xcc\xba\xea\xb8\xa6\x20\xb5\xa4\xbe\xee\xbe\xb9\xb4\xcf\xb4\xd9\x2e\x20\xb0\xe8\xbc\xd3\xc7\xd2\xb1\xee\xbf\xe4\x3f", // 현재 세이브를 덮어씁니다. 계속할까요?,
    "\xbc\xbc\xc0\xcc\xba\xea\xb8\xa6\x20\xb3\xbb\xba\xb8\xb3\xc2\xbd\xc0\xb4\xcf\xb4\xd9\x2e", // 세이브를 내보냈습니다.,
    "\xbc\xbc\xc0\xcc\xba\xea\xb8\xa6\x20\xb3\xbb\xba\xb8\xb3\xbb\xc1\xf6\x20\xb8\xf8\xc7\xdf\xbd\xc0\xb4\xcf\xb4\xd9\x2e", // 세이브를 내보내지 못했습니다.,
    "\xb3\xbb\xba\xb8\xb3\xbe\x20\xbc\xbc\xc0\xcc\xba\xea\xb0\xa1\x20\xbe\xf8\xbd\xc0\xb4\xcf\xb4\xd9\x2e", // 내보낼 세이브가 없습니다.,
    "\xbf\xc3\xb9\xd9\xb8\xa5\x20\xbc\xbc\xc0\xcc\xba\xea\x20\xc6\xc4\xc0\xcf\xc0\xcc\x20\xbe\xc6\xb4\xd5\xb4\xcf\xb4\xd9\x2e", // 올바른 세이브 파일이 아닙니다.,
    "\xbc\xbc\xc0\xcc\xba\xea\xb8\xa6\x20\xba\xb9\xb1\xb8\xc7\xcf\xc1\xf6\x20\xb8\xf8\xc7\xdf\xbd\xc0\xb4\xcf\xb4\xd9\x2e\x20\xb1\xe2\xc1\xb8\x20\xbc\xbc\xc0\xcc\xba\xea\xb4\xc2\x20\xb1\xd7\xb4\xeb\xb7\xce\xc0\xd4\xb4\xcf\xb4\xd9\x2e", // 세이브를 복구하지 못했습니다. 기존 세이브는 그대로입니다.,
    "\xbc\xbc\xc0\xcc\xba\xea\xb8\xa6\x20\xba\xb9\xb1\xb8\xc7\xdf\xbd\xc0\xb4\xcf\xb4\xd9\x2e\x20\xc8\xae\xc0\xce\xc0\xbb\x20\xb4\xa9\xb8\xa3\xb8\xe9\x20\xb0\xd4\xc0\xd3\xc0\xbb\x20\xc1\xbe\xb7\xe1\xc7\xd5\xb4\xcf\xb4\xd9\x2e\x20\xb4\xd9\xbd\xc3\x20\xbd\xc7\xc7\xe0\xc7\xd8\x20\xc1\xd6\xbc\xbc\xbf\xe4\x2e", // 세이브를 복구했습니다. 확인을 누르면 게임을 종료합니다. 다시 실행해 주세요.
};
static uint32_t guest_text[sizeof TEXTS / sizeof TEXTS[0]];

uint32_t savefile_text(int id) {
    if (!guest_text[id]) {
        size_t n = strlen(TEXTS[id]) + 1;
        guest_text[id] = heap_alloc((uint32_t)n);
        memcpy(cpu.m + guest_text[id], TEXTS[id], n);
    }
    return guest_text[id];
}

static void message(int id) {
    uint32_t a[] = {savefile_text(id)};
    guest_call(FN_SHOW_MESSAGE, a, 1);
}

typedef struct { const char *name; const uint8_t *data; uint32_t size; } entry_t;
static uint8_t *loaded;
static entry_t entries[NFILES];
static int nentries;
static bool quit_after_message;

static uint64_t fnv(const uint8_t *p, size_t n) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001b3ull;
    return h;
}

static void path_of(char *out, size_t cap, const char *dir, const char *name) { snprintf(out, cap, "%s%s/%s", save_dir, dir, name); }

static void export_to(const char *path) {
    size_t cap = 16, len = 0;
    uint8_t *out = malloc(cap);
    uint32_t count = 0;
    memcpy(out, MAGIC, 8);
    len = 12;
    bool has_hero = false;
    for (int i = 0; i < NFILES; i++) {
        char p[1200];
        path_of(p, sizeof p, "", FILES[i]);
        size_t n;
        void *d = SDL_LoadFile(p, &n);
        if (!d) continue;
        if (!strcmp(FILES[i], "HERODATA")) has_hero = true;
        out = realloc(out, len + NAME_LEN + 4 + n + 8);
        memset(out + len, 0, NAME_LEN);
        memcpy(out + len, FILES[i], strlen(FILES[i]));
        len += NAME_LEN;
        uint32_t size = (uint32_t)n;
        memcpy(out + len, &size, 4);
        len += 4;
        memcpy(out + len, d, n);
        len += n;
        count++;
        SDL_free(d);
    }
    if (!has_hero) { free(out); message(SAVE_TEXT_NO_SAVE); return; }
    memcpy(out + 8, &count, 4);
    uint64_t h = fnv(out, len);
    memcpy(out + len, &h, 8);
    len += 8;
    bool ok = SDL_SaveFile(path, out, len);
    free(out);
    message(ok ? SAVE_TEXT_EXPORTED : SAVE_TEXT_EXPORT_FAILED);
}

// 게임이 읽을 때와 같이 앞 16바이트 해시를 복호화한 내용과 비교한다(0x184b54).
static bool game_accepts(const uint8_t *d, uint32_t n) {
    if (n <= 16) return false;
    uint32_t body = n - 16, buf = heap_alloc(body + 16), hash = buf + body;
    memcpy(cpu.m + buf, d + 16, body);
    uint32_t a[] = {buf, body};
    guest_call(FN_DECRYPT, a, 2);
    uint32_t b[] = {buf, body, hash};
    guest_call(FN_HASH, b, 3);
    bool same = !memcmp(cpu.m + hash, d, 16);
    heap_free(buf);
    return same;
}

// 저장 파일을 해시로 검증하고 복호화한 내용을 돌려준다. 앞 16바이트 해시는 뺀다. 실패하면 -1이다.
int savefile_read_plain(const char *name, uint8_t *out, int cap) {
    char path[1200];
    snprintf(path, sizeof path, "%s/%s", save_dir, name);
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    uint8_t *d = malloc((size_t)cap + 16);
    int n = d ? (int)fread(d, 1, (size_t)cap + 16, f) : 0;
    fclose(f);
    if (n <= 16 || !game_accepts(d, (uint32_t)n)) { free(d); return -1; }
    int body = n - 16;
    uint32_t buf = heap_alloc((uint32_t)body);
    memcpy(cpu.m + buf, d + 16, (size_t)body);
    uint32_t a[] = {buf, (uint32_t)body};
    guest_call(FN_DECRYPT, a, 2);
    memcpy(out, cpu.m + buf, (size_t)body);
    heap_free(buf);
    free(d);
    return body;
}

static int known_file(const char *name) {
    for (int i = 0; i < NFILES; i++) if (!strcmp(FILES[i], name)) return i;
    return -1;
}

static bool parse_bundle(const uint8_t *d, size_t n) {
    nentries = 0;
    if (n < 20 || memcmp(d, MAGIC, 8)) return false;
    uint64_t want;
    memcpy(&want, d + n - 8, 8);
    if (fnv(d, n - 8) != want) return false;
    uint32_t count;
    memcpy(&count, d + 8, 4);
    if (count == 0 || count > NFILES) return false;
    size_t p = 12, end = n - 8;
    for (uint32_t i = 0; i < count; i++) {
        if (end - p < NAME_LEN + 4) return false;
        char name[NAME_LEN + 1] = {0};
        memcpy(name, d + p, NAME_LEN);
        int known = known_file(name);
        if (known < 0) return false;
        for (int k = 0; k < nentries; k++) if (!strcmp(entries[k].name, name)) return false;
        uint32_t size;
        memcpy(&size, d + p + NAME_LEN, 4);
        p += NAME_LEN + 4;
        if (size > MAX_FILE || size > end - p || !game_accepts(d + p, size)) return false;
        entries[nentries++] = (entry_t){FILES[known], d + p, size};
        p += size;
    }
    if (p != end) return false;
    for (size_t r = 0; r < sizeof REQUIRED / sizeof REQUIRED[0]; r++) {
        bool found = false;
        for (int k = 0; k < nentries; k++) if (!strcmp(entries[k].name, REQUIRED[r])) found = true;
        if (!found) return false;
    }
    return true;
}

static void import_from(const char *path) {
    size_t n;
    SDL_free(loaded);
    loaded = SDL_LoadFile(path, &n);
    if (!loaded || !parse_bundle(loaded, n)) {
        SDL_free(loaded);
        loaded = NULL;
        message(SAVE_TEXT_INVALID);
        return;
    }
    savefile_ask_overwrite();
}

// 새 파일을 모두 쓴 뒤에야 기존 세이브를 옮긴다. 중간에 실패하면 되돌린다.
static bool apply_import(void) {
    char a[1200], b[1200], dir[1100];
    snprintf(dir, sizeof dir, "%s/import", save_dir);
    SDL_CreateDirectory(dir);
    for (int i = 0; i < nentries; i++) {
        path_of(a, sizeof a, "/import", entries[i].name);
        if (!SDL_SaveFile(a, entries[i].data, entries[i].size)) return false;
    }
    snprintf(dir, sizeof dir, "%s/backup", save_dir);
    SDL_CreateDirectory(dir);
    bool moved[NFILES] = {0}, ok = true;
    for (int i = 0; i < NFILES; i++) {
        path_of(a, sizeof a, "", FILES[i]);
        path_of(b, sizeof b, "/backup", FILES[i]);
        remove(b);
        if (SDL_GetPathInfo(a, NULL) && !(moved[i] = SDL_RenamePath(a, b))) ok = false;
    }
    for (int i = 0; ok && i < nentries; i++) {
        path_of(a, sizeof a, "/import", entries[i].name);
        path_of(b, sizeof b, "", entries[i].name);
        if (!SDL_RenamePath(a, b)) ok = false;
    }
    if (ok) {
        snprintf(dir, sizeof dir, "%s/import", save_dir);
        SDL_RemovePath(dir);
    } else
        for (int i = 0; i < NFILES; i++) if (moved[i]) {
            path_of(a, sizeof a, "/backup", FILES[i]);
            path_of(b, sizeof b, "", FILES[i]);
            SDL_RenamePath(a, b);
        }
    return ok;
}

void savefile_overwrite_answer(bool yes) {
    if (yes && loaded) {
        if (apply_import()) { quit_after_message = true; message(SAVE_TEXT_IMPORTED); }
        else message(SAVE_TEXT_IMPORT_FAILED);
    }
    SDL_free(loaded);
    loaded = NULL;
}

// 파일 창의 응답은 다른 스레드에서 올 수 있어 메인 루프에서 처리한다.
static SDL_AtomicInt dialog_state; // 0 없음, 1 열림, 2 응답 도착
static bool dialog_import;
static char picked[2048];

static void SDLCALL on_pick(void *ud, const char *const *files, int filter) {
    (void)ud; (void)filter;
    picked[0] = 0;
    if (files && files[0]) snprintf(picked, sizeof picked, "%s", files[0]);
    SDL_SetAtomicInt(&dialog_state, 2);
}

bool savefile_dialog_open(void) { return SDL_GetAtomicInt(&dialog_state) != 0; }

// 내보내기 기본값: 문서 폴더의 chronos-wing-YYYY-MM-DD.cwsave. 문서 폴더를 모르면 이름만 넘긴다.
static void default_export_path(char *out, size_t cap) {
    SDL_Time now = 0;
    SDL_DateTime t = {0};
    SDL_GetCurrentTime(&now);
    SDL_TimeToDateTime(now, &t, true);
    const char *dir = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
    snprintf(out, cap, "%schronos-wing-%04d-%02d-%02d.cwsave", dir ? dir : "", t.year, t.month, t.day);
}

void savefile_choose(bool import) {
    static const SDL_DialogFileFilter filter[] = {{"Chronos Wing save", "cwsave"}};
    dialog_import = import;
    SDL_SetAtomicInt(&dialog_state, 1);
    const char *test = getenv("CHRONOS_SAVEFILE_TEST"); // 자동 시험용
    if (test) { const char *f[] = {test, NULL}; on_pick(NULL, f, 0); return; }
    if (import) SDL_ShowOpenFileDialog(on_pick, NULL, host_window(), filter, 1, NULL, false);
    else {
        char path[1200];
        default_export_path(path, sizeof path);
        SDL_ShowSaveFileDialog(on_pick, NULL, host_window(), filter, 1, path);
    }
}

void savefile_poll(void) {
    if (SDL_GetAtomicInt(&dialog_state) == 2) {
        SDL_SetAtomicInt(&dialog_state, 0);
        if (picked[0]) { if (dialog_import) import_from(picked); else export_to(picked); }
    }
    if (quit_after_message && !cpu.m[NET_UI + MESSAGE_OPEN]) request_exit();
}
