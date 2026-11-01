// 소리. MMF를 미리 바꾼 이벤트 파일(smaf2ev)을 내용 해시로 찾아 재생한다. MIDI는 TinySoundFont(MIT), 파형은 직접 섞는다.
#include "host.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#define TSF_IMPLEMENTATION
#include "../third_party/tsf.h"

#define RATE 44100
typedef struct { uint32_t time; uint8_t kind, ch; uint32_t rate, len; const uint8_t *data; } ev_t;
typedef struct { uint64_t hash; uint32_t n; ev_t *ev; uint32_t duration; bool has_wave; } seq_t;
typedef struct { bool on, repeat; seq_t *s; uint32_t next; uint64_t pos; uint32_t handle; } play_t;
typedef struct { bool on; const int16_t *s; uint32_t n; double pos, step; uint32_t handle; } wave_t;

static uint8_t *blob;
static seq_t *seqs;
static uint32_t nseq;
static tsf *synth;
static SDL_Mutex *lock;
static SDL_AudioStream *stream;
static play_t plays[16];
static wave_t waves[16];
static seq_t **handles;
static uint32_t nhandles;
float audio_peak;

static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

static void load_events(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "소리 이벤트 파일 없음: %s\n", path); return; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    blob = malloc((size_t)len);
    if (fread(blob, 1, (size_t)len, f) != (size_t)len) { fclose(f); return; }
    fclose(f);
    const uint8_t *p = blob + 4;
    nseq = rd32(p); p += 4;
    seqs = calloc(nseq, sizeof *seqs);
    for (uint32_t i = 0; i < nseq; i++) {
        seq_t *s = &seqs[i];
        memcpy(&s->hash, p, 8); p += 8;
        s->n = rd32(p); p += 4;
        s->ev = calloc(s->n ? s->n : 1, sizeof(ev_t));
        for (uint32_t k = 0; k < s->n; k++) {
            ev_t *e = &s->ev[k];
            e->time = rd32(p); p += 4;
            e->kind = *p++;
            if (e->time > s->duration) s->duration = e->time;
            if (e->kind == 0) { e->len = rd32(p); p += 4; e->data = p; p += e->len; }
            else { s->has_wave = true; e->ch = *p++; e->rate = rd32(p); p += 4; e->len = rd32(p); p += 4; e->data = p; p += (size_t)e->len * 2; }
        }
    }
}

static void midi(const uint8_t *m, uint32_t n) {
    if (n < 2) return;
    int ch = m[0] & 15;
    switch (m[0] & 0xf0) {
    case 0x90: if (n >= 3) { if (m[2]) tsf_channel_note_on(synth, ch, m[1], m[2] / 127.0f); else tsf_channel_note_off(synth, ch, m[1]); } break;
    case 0x80: if (n >= 3) tsf_channel_note_off(synth, ch, m[1]); break;
    case 0xc0: tsf_channel_set_presetnumber(synth, ch, m[1], ch == 9); break;
    case 0xb0: if (n >= 3) tsf_channel_midi_control(synth, ch, m[1], m[2]); break;
    case 0xe0: if (n >= 3) tsf_channel_set_pitchwheel(synth, ch, m[1] | m[2] << 7); break;
    }
}

// 곡 사이에 서스테인 페달 같은 채널 상태가 남으면 다음 곡의 음이 끝나지 않고 쌓인다.
static void synth_reset(void) {
    if (!synth) return;
    // TinySoundFont의 CC121은 페달을 풀지 않아 CC64를 따로 끈다.
    for (int ch = 0; ch < 16; ch++) {
        tsf_channel_midi_control(synth, ch, 64, 0);
        tsf_channel_midi_control(synth, ch, 121, 0);
    }
    tsf_note_off_all(synth);
}

static void run_events(play_t *p) {
    uint64_t now_ms = p->pos * 1000 / RATE;
    while (p->next < p->s->n && p->s->ev[p->next].time <= now_ms) {
        ev_t *e = &p->s->ev[p->next++];
        if (e->kind == 0) { if (synth && !p->s->has_wave) midi(e->data, e->len); }
        else {
            for (int w = 0; w < 16; w++)
                if (!waves[w].on) {
                    waves[w] = (wave_t){true, (const int16_t *)e->data, e->len, 0, (double)e->rate / RATE, p->handle};
                    break;
                }
        }
    }
    if (p->next >= p->s->n && p->pos * 1000 / RATE >= p->s->duration) {
        if (!p->s->has_wave) synth_reset();
        if (p->repeat && p->s->duration) { p->pos = 0; p->next = 0; }
        else p->on = false;
    }
}

// 원본은 소리를 하나만 내는 기기용이라 효과음마다 "배경음 정지, 효과음 재생, 효과음 정지, 배경음 처음부터 재생"을 호출한다.
// 함께 들리도록 이 순서를 해석한다. 효과음이 끼어든 정지는 무시하고, 같은 곡 재생은 이어서 튼다.
// 효과음이 끝난 뒤에도 재생 요청이 없으면 그때 실제로 멈춘다.
#define MUSIC_STOP_WAIT_MS 100
#define MUSIC_INTERRUPT_MAX_MS 6000
static uint64_t music_stop_at, music_interrupt_at;
static uint32_t interrupting_sfx;

static play_t *music_play(void) {
    for (int i = 0; i < 16; i++) if (plays[i].on && !plays[i].s->has_wave) return &plays[i];
    return NULL;
}

static void music_clear_stop(void) { music_stop_at = music_interrupt_at = 0; interrupting_sfx = 0; }

static void music_stop_now(void) {
    play_t *m = music_play();
    if (m) { m->on = false; synth_reset(); }
    music_clear_stop();
}

static void music_check_stop(void) {
    uint64_t now = SDL_GetTicks();
    if ((music_stop_at && now >= music_stop_at) || (music_interrupt_at && now - music_interrupt_at >= MUSIC_INTERRUPT_MAX_MS)) music_stop_now();
}

static void SDLCALL audio_cb(void *ud, SDL_AudioStream *st, int additional, int total) {
    static float buf[8192];
    (void)ud; (void)total;
    int frames = additional / (int)(sizeof(float) * 2);
    while (frames > 0) {
        int n = frames > 256 ? 256 : frames; // 이벤트 시각 해상도 약 6ms
        SDL_LockMutex(lock);
        music_check_stop();
        memset(buf, 0, sizeof(float) * 2 * n);
        for (int i = 0; i < 16; i++) if (plays[i].on) { run_events(&plays[i]); plays[i].pos += n; }
        if (synth) tsf_render_float(synth, buf, n, 0);
        for (int w = 0; w < 16; w++) {
            wave_t *v = &waves[w];
            if (!v->on) continue;
            for (int k = 0; k < n; k++) {
                uint32_t idx = (uint32_t)v->pos;
                if (idx >= v->n) { v->on = false; break; }
                float s = v->s[idx] / 65536.0f; // 효과음 여러 개가 겹쳐도 클리핑이 덜 나도록 절반으로 섞는다.
                buf[k * 2] += s;
                buf[k * 2 + 1] += s;
                v->pos += v->step;
            }
        }
        SDL_UnlockMutex(lock);
        for (int k = 0; k < n * 2; k++) {
            float a = buf[k] < 0 ? -buf[k] : buf[k];
            if (a > audio_peak) audio_peak = a;
            if (buf[k] > 1) buf[k] = 1;
            if (buf[k] < -1) buf[k] = -1;
        }
        SDL_PutAudioStreamData(st, buf, n * 2 * (int)sizeof(float));
        frames -= n;
    }
}

void audio_pause(bool paused) {
    if (!stream) return;
    if (paused) SDL_PauseAudioStreamDevice(stream); else SDL_ResumeAudioStreamDevice(stream);
}

void audio_init(const char *soundfont) {
    char path[1200];
    snprintf(path, sizeof path, "%s/../game.ev", game_dir);
    load_events(path);
    snprintf(path, sizeof path, "%s/../1mgm.sf2", game_dir);
    synth = tsf_load_filename(soundfont ? soundfont : path);
    if (synth) {
        tsf_set_output(synth, TSF_STEREO_INTERLEAVED, RATE, -6.0f);
        for (int c = 0; c < 16; c++) tsf_channel_set_presetnumber(synth, c, 0, c == 9);
    } else {
        fprintf(stderr, "사운드폰트를 열지 못함: BGM 없이 진행\n");
    }
    lock = SDL_CreateMutex();
    SDL_AudioSpec spec = {SDL_AUDIO_F32, 2, RATE};
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_cb, NULL);
    if (stream) SDL_ResumeAudioStreamDevice(stream);
    else fprintf(stderr, "오디오 장치 실패: %s\n", SDL_GetError());
}

uint32_t audio_load(const uint8_t *data, uint32_t size) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (uint32_t i = 0; i < size; i++) h = (h ^ data[i]) * 0x100000001b3ull;
    for (uint32_t i = 0; i < nseq; i++)
        if (seqs[i].hash == h) {
            handles = realloc(handles, sizeof *handles * (nhandles + 1));
            handles[nhandles++] = &seqs[i];
            return nhandles;
        }
    fprintf(stderr, "변환되지 않은 소리 데이터: %u바이트\n", size);
    return 0;
}

void audio_stop(uint32_t handle) {
    if (!handle || !lock) return;
    SDL_LockMutex(lock);
    play_t *m = music_play();
    if (m && m->handle == handle) {
        if (!music_interrupt_at && !music_stop_at) music_stop_at = SDL_GetTicks() + MUSIC_STOP_WAIT_MS;
    } else if (handle == interrupting_sfx) {
        // 효과음은 끝까지 울린다. 이 뒤로 배경음 요청이 곧 오지 않으면 실제 정지다.
        music_stop_at = SDL_GetTicks() + MUSIC_STOP_WAIT_MS;
    }
    SDL_UnlockMutex(lock);
}

void audio_play(uint32_t handle, bool repeat) {
    if (!handle || handle > nhandles) return;
    seq_t *seq = handles[handle - 1];
    SDL_LockMutex(lock);
    play_t *m = music_play();
    if (seq->has_wave) {
        if (m && (music_stop_at || music_interrupt_at)) {
            music_stop_at = 0;
            if (!music_interrupt_at) music_interrupt_at = SDL_GetTicks();
            interrupting_sfx = handle;
        }
    } else if (m && m->s == seq) {
        // 같은 곡이면 처음으로 돌리지 않고 핸들만 넘긴다.
        m->handle = handle;
        m->repeat = repeat;
        music_clear_stop();
        SDL_UnlockMutex(lock);
        return;
    } else {
        music_stop_now();
    }
    for (int i = 0; i < 16; i++) if (!plays[i].on) {
        plays[i] = (play_t){true, repeat, seq, 0, 0, handle};
        break;
    }
    SDL_UnlockMutex(lock);
}

void audio_free(uint32_t handle) { audio_stop(handle); }
