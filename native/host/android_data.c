// Android: APK의 game.pak을 앱 내부 저장소에 풀고 게임 폴더 위치를 정한다. 세이브는 다른 폴더라 건드리지 않는다.
#ifdef __ANDROID__
#include "host.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t read_u32(SDL_IOStream *io) { Uint32 v = 0; SDL_ReadU32LE(io, &v); return v; }

static bool extract(SDL_IOStream *io, const char *root) {
    char magic[4];
    if (SDL_ReadIO(io, magic, 4) != 4 || memcmp(magic, "CWPK", 4)) return false;
    uint32_t count = read_u32(io);
    char name[512], path[1200];
    for (uint32_t i = 0; i < count; i++) {
        Uint16 len = 0;
        SDL_ReadU16LE(io, &len);
        if (len >= sizeof name || SDL_ReadIO(io, name, len) != len) return false;
        name[len] = 0;
        uint32_t size = read_u32(io);
        snprintf(path, sizeof path, "%s/%s", root, name);
        char *slash = strrchr(path, '/');
        *slash = 0;
        SDL_CreateDirectory(path);
        *slash = '/';
        void *data = malloc(size ? size : 1);
        if (SDL_ReadIO(io, data, size) != size) { free(data); return false; }
        FILE *f = fopen(path, "wb");
        if (!f) { free(data); return false; }
        fwrite(data, 1, size, f);
        fclose(f);
        free(data);
    }
    return true;
}

bool android_prepare_data(void) {
    char root[1024], marker[1100], want[32], have[32] = "";
    snprintf(root, sizeof root, "%s/data", SDL_GetAndroidInternalStoragePath());
    SDL_IOStream *io = SDL_IOFromFile("game.pak", "rb");
    if (!io) return false;
    snprintf(want, sizeof want, "%lld", (long long)SDL_GetIOSize(io));
    snprintf(marker, sizeof marker, "%s/.pak", root);
    FILE *f = fopen(marker, "r");
    if (f) { if (!fgets(have, sizeof have, f)) have[0] = 0; fclose(f); }
    bool ok = !strcmp(want, have);
    if (!ok) {
        SDL_CreateDirectory(root);
        remove(marker);
        ok = extract(io, root);
        if (ok && (f = fopen(marker, "w"))) { fputs(want, f); fclose(f); }
    }
    SDL_CloseIO(io);
    snprintf(game_dir, sizeof game_dir, "%s/game", root);
    return ok;
}
#endif
