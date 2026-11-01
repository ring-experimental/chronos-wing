// 입력기(표 4)와 TextComponent(표 9). 닉네임 입력(0x156ffc, 0x104eec)이 쓴다. 표시와 커서는 게임이 맡고 여기서는 키를 글자로 바꾼다.
// 모드 1 영문, 2 숫자, 3 한글(0x104d3e 표시 문자열 "A", "1", "한").
// 키: 영문은 2~9 멀티탭. 한글은 1 ㄱㅋㄲ 2 ㄴㄹ 3 ㄷㅌㄸ 4 ㅂㅍㅃ 5 ㅅㅎㅆ 6 ㅈㅊㅉ 7 ㅇㅁ 8 ㅏㅓㅑㅕㅐㅔㅒㅖ 9 ㅗㅜㅛㅠ 0 ㅡㅣ.
// 같은 키를 1초 안에 다시 누르면 순환한다. 원본 폰의 배열은 확인할 수 없어 나랏글 자음 배치를 따랐다.
#include "host.h"
#include <string.h>

enum { FLUSH_KEY = 0x9d, MODE_ALPHA = 1, MODE_DIGIT = 2, MODE_HANGUL = 3, TAP_MS = 1000, FIELD_MAX = 8 };

// 유니코드 음절(AC00~) 순서로 KS X 1001 수록 여부. 수록 음절은 유니코드 순으로 EUC-KR B0A1부터 놓인다.
static const uint8_t KS_SYLLABLES[] = {
    147,7,255,62,17,176,3,19,1,40,16,17,0,0,147,5,123,30,17,176,3,151,1,59,18,17,160,0,147,149,107,48,
    81,176,2,17,1,50,48,17,176,2,17,1,10,48,121,184,6,19,1,48,16,0,128,0,19,1,11,16,17,0,0,147,
    3,43,16,0,0,0,147,5,107,116,81,176,35,19,1,59,48,16,0,0,0,0,0,112,17,176,3,19,0,41,16,17,
    128,33,1,0,0,48,21,176,14,3,1,48,48,0,0,2,17,1,35,16,0,0,0,19,129,107,16,16,0,3,19,1,
    19,16,17,48,0,1,0,0,48,85,184,34,0,0,0,48,17,176,2,151,7,251,58,17,176,3,19,1,33,0,0,0,
    0,27,13,59,56,17,176,3,19,1,51,17,1,0,0,19,5,43,28,17,0,1,0,0,0,16,17,176,0,19,1,42,
    48,25,176,2,1,0,16,16,0,0,0,17,1,3,48,16,48,2,19,7,107,20,17,0,0,19,5,43,116,249,184,143,
    19,1,59,16,0,0,0,0,0,0,112,217,176,74,19,1,59,16,17,0,3,17,0,0,48,89,177,42,17,1,0,16,
    0,0,1,17,1,11,16,0,0,0,19,1,43,16,0,0,1,1,0,32,16,17,160,2,17,1,33,48,89,176,2,1,
    0,0,48,25,176,7,19,1,59,56,17,176,3,0,0,0,0,0,0,0,19,13,59,56,17,176,3,1,0,16,0,0,
    0,0,19,1,32,16,16,0,0,1,0,0,16,1,0,0,0,0,0,48,17,24,2,0,0,0,16,0,0,0,17,1,
    35,0,0,0,0,147,1,11,16,17,48,0,17,1,43,48,17,176,199,19,1,59,48,1,128,2,0,0,0,48,17,176,
    131,19,1,43,48,17,176,3,17,0,10,48,17,176,2,17,0,32,0,0,0,1,17,1,43,16,17,160,2,19,1,43,
    16,0,0,1,1,0,0,48,17,144,2,19,1,43,48,17,176,102,0,0,0,48,17,176,2,211,7,107,58,17,176,7,
    3,1,32,0,0,0,0,19,5,107,56,17,176,3,19,1,184,16,0,0,0,27,5,43,16,1,0,3,0,0,0,16,
    17,160,2,17,1,10,112,121,176,162,17,1,10,16,0,0,0,17,1,0,16,17,144,0,17,1,9,0,0,0,0,147,
    5,187,242,249,176,34,19,1,59,50,1,32,0,0,0,0,48,89,176,6,147,1,59,48,17,160,35,17,0,0,112,17,
    176,2,17,0,16,16,0,0,1,19,1,3,16,1,0,0,147,7,43,22,16,0,1,1,0,0,48,17,0,2,17,1,
    41,48,17,176,0,0,0,0,48,81,176,14,19,5,59,56,17,176,3,3,0,1,0,0,0,0,147,1,57,16,0,0,
    2,3,0,59,0,0,0,0,19,1,35,0,0,0,0,0,0,0,16,0,0,0,1,0,32,48,17,144,2,0,0,0,
    0,0,0,0,0,0,0,16,0,0,2,17,1,3,0,0,0,0,19,1,43,176,121,176,35,19,1,59,48,17,176,2,
    17,1,33,240,217,176,67,19,1,59,48,17,176,3,17,1,32,112,81,176,34,19,1,32,16,17,144,1,17,1,11,48,
    17,176,2,147,1,171,22,0,0,1,19,1,33,48,17,176,2,3,1,41,48,49,176,2,0,0,0,48,25,184,66,27,
    1,51,56,17,48,3,0,0,32,0,0,0,0,19,5,51,16,17,0,0,0,0,0,0,1,0,0,147,5,35,48,1,
    0,1,1,0,16,16,17,48,0,1,0,0,48,17,48,2,1,0,16,16,0,0,0,17,0,0,0,0,0,2,19,133,
    3,16,17,16,0,19,1,43,48,119,184,99,19,1,59,48,145,176,162,17,1,2,48,123,240,87,19,1,43,112,209,240,
    227,17,1,27,48,113,185,10,19,1,59,48,1,144,2,19,1,43,48,17,176,2,19,7,43,48,17,48,3,19,1,35,
    48,17,176,2,19,1,171,48,17,180,254,17,1,9,48,113,184,71,211,5,123,48,17,176,3,83,1,33,16,17,0,0,
    19,5,107,48,17,176,2,17,1,51,16,0,0,0,19,5,235,56,16,160,2,1,0,48,16,17,176,2,19,0,32,48,
    113,176,2,1,0,16,16,0,0,0,19,1,11,16,17,16,0,19,1,43,0,0,0,0,147,5,107,54,149,176,3,19,
    1,59,16,1,0,2,0,0,0,48,17,176,3,1,0,32,16,0,0,1,0,0,0,48,17,176,10,3,1,16,16,0,
    0,1,17,1,3,0,0,0,2,19,1,35,16,0,0,3,0,0,0,16,0,0,0,1,0,0,16,0,144,2,0,0,
    0,48,17,48,134,83,1,123,48,17,176,3,81,1,33,0,0,0,0,19,1,59,48,17,176,2,17,0,16,16,1,0,
    2,19,1,43,16,17,0,2,0,0,0,16,17,176,2,1,0,1,48,17,176,2,1,0,16,16,1,0,0,17,1,43,
    16,17,16,2,19,1,43,0,0,0,0,147,3,43,48,17,176,2,19,1,59,48,0,0,2,0,0,0,48,25,176,3,
    19,1,43,16,17,176,3,1,0,0,48,17,176,2,19,1,33,16,0,0,2,1,1,0,16,0,0,0,19,1,43,16,
    17,0,2,1,0,32,48,17,176,2,17,1,1,48,17,48,2,0,0,0,48,17,176,2,19,3,59,48,17,176,3,1,
    0,32,0,0,0,0,19,5,59,48,17,176,2,17,0,16,16,1,0,0,19,1,43,20,1,0,0,1,0,0,16,1,
    128,2,1,0,0,48,17,176,2,1,0,16,16,0,0,0,19,1,35,16,17,16,2,147,5,11,16,17,48,0,19,1,
    43,112,81,176,35,19,1,59,48,0,0,0,0,0,0,48,17,176,3,19,1,43,16,17,48,3,1,1,10,48,17,176,
    2,1,0,32,0,0,0,0,17,0,0,16,17,160,0,147,5,43,16,0,0,2,0,0,0,16,17,144,0,17,1,41,
    16,17,176,0,0,0,0,48,17,176,2,19,33,43,48,17,176,3,1,0,32,0,0,0,0,19,5,43,48,17,176,2,
    19,1,59,16,17,32,0,19,33,43,50,17,128,2,19,0,40,48,17,160,2,17,1,10,48,17,146,2,17,1,33,48,
    17,0,2,19,1,43,48,17,144,2,211,3,43,18,17,48,2,19,1,43,0,
};

static int mode = MODE_HANGUL;
static int last_key = -1, tap;
static uint64_t last_ms;
static char alpha_comp; // 영문 조합 중인 글자

// 자모는 호환 자모 번호(EUC-KR A4A1 + n, 유니코드 3131 + n)로 다룬다.
#define J(u) ((u) - 0x3131)
static const int CHO_OF[] = { // 호환 자음 -> 초성 번호, 없으면 -1
    0, 1, -1, 2, -1, -1, 3, 4, 5, -1, -1, -1, -1, -1, -1, -1, 6, 7, 8, -1, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
};
static const int JONG_OF[] = { // 호환 자음 -> 종성 번호. 겹받침은 만들지 않는다.
    1, 2, -1, 4, -1, -1, 7, -1, 8, -1, -1, -1, -1, -1, -1, -1, 16, 17, -1, 19, 20, 21, 22, -1, 23, 24, 25, 26, 27, -1,
};
static const uint16_t CONS[10][4] = { // 키 0~9, 0은 모음
    {0}, {0x3131, 0x314b, 0x3132}, {0x3134, 0x3139}, {0x3137, 0x314c, 0x3138}, {0x3142, 0x314d, 0x3143},
    {0x3145, 0x314e, 0x3146}, {0x3148, 0x314a, 0x3149}, {0x3147, 0x3141},
};
static const uint16_t VOWEL[10][9] = {
    [0] = {0x3161, 0x3163}, [8] = {0x314f, 0x3153, 0x3151, 0x3155, 0x3150, 0x3154, 0x3152, 0x3156}, [9] = {0x3157, 0x315c, 0x315b, 0x3160},
};

// 조합 상태. cho, jung, jong은 호환 자모(0이면 없음)다. prev_jung은 겹모음을 만들기 전 모음이다.
static uint16_t h_cho, h_jung, h_jong;

static int emit_jamo(char *o, uint16_t u) { o[0] = (char)0xa4; o[1] = (char)(0xa1 + u - 0x3131); o[2] = 0; return 2; }

static int emit_syllable(char *o, int cho, int jung, int jong) {
    int idx = (cho * 21 + jung) * 28 + jong, rank = 0;
    if (!(KS_SYLLABLES[idx / 8] >> (idx % 8) & 1)) return 0;
    for (int i = 0; i < idx / 8; i++) rank += __builtin_popcount(KS_SYLLABLES[i]);
    rank += __builtin_popcount(KS_SYLLABLES[idx / 8] & ((1 << (idx % 8)) - 1));
    o[0] = (char)(0xb0 + rank / 94);
    o[1] = (char)(0xa1 + rank % 94);
    o[2] = 0;
    return 2;
}

// 조합 중인 글자를 EUC-KR로. 없으면 0.
static int hangul_compose(char *o) {
    o[0] = 0;
    if (h_cho && h_jung) {
        int n = emit_syllable(o, CHO_OF[J(h_cho)], h_jung - 0x314f, h_jong ? JONG_OF[J(h_jong)] : 0);
        if (n) return n;
    }
    if (h_cho) return emit_jamo(o, h_cho);
    if (h_jung) return emit_jamo(o, h_jung);
    return 0;
}

static uint16_t compound(uint16_t a, uint16_t b) {
    static const uint16_t T[][3] = {{0x3157, 0x314f, 0x3158}, {0x3157, 0x3150, 0x3159}, {0x3157, 0x3163, 0x315a}, {0x315c, 0x3153, 0x315d},
                                    {0x315c, 0x3154, 0x315e}, {0x315c, 0x3163, 0x315f}, {0x3161, 0x3163, 0x3162}};
    for (size_t i = 0; i < sizeof T / sizeof T[0]; i++)
        if (T[i][0] == a && T[i][1] == b) return T[i][2];
    return 0;
}

// 한글 키 처리. 끝난 글자는 commit에 쓰고 길이를 돌려준다.
static int hangul_key(int d, bool same, char *commit) {
    int out = 0;
    commit[0] = 0;
    if (VOWEL[d][0]) {
        int n = 0;
        while (n < 9 && VOWEL[d][n]) n++;
        uint16_t v = VOWEL[d][same ? tap % n : 0];
        if (h_jong && !same) { // 받침이 다음 글자 초성으로 넘어간다.
            uint16_t c = h_jong;
            h_jong = 0;
            out = hangul_compose(commit);
            h_cho = c; h_jung = v;
            return out;
        }
        if (same && h_jung) { h_jung = v; return 0; }
        if (h_jung) {
            uint16_t m = compound(h_jung, v);
            if (m) { h_jung = m; return 0; }
            out = hangul_compose(commit);
            h_cho = 0; h_jong = 0; h_jung = v;
            return out;
        }
        h_jung = v;
        return 0;
    }
    if (!CONS[d][0]) return 0;
    int n = 0;
    while (n < 4 && CONS[d][n]) n++;
    uint16_t c = CONS[d][same ? tap % n : 0];
    if (same && h_jong) { // 받침 순환. 받침이 될 수 없으면 건너뛴다.
        if (JONG_OF[J(c)] >= 0) h_jong = c;
        return 0;
    }
    if (same && h_cho && !h_jung) { h_cho = c; return 0; }
    if (h_cho && h_jung && !h_jong && JONG_OF[J(c)] >= 0) { h_jong = c; return 0; }
    out = hangul_compose(commit);
    h_cho = c; h_jung = 0; h_jong = 0;
    return out;
}

static char alpha_letter(int d, int t) {
    static const char *const L[10] = {"", "", "abc", "def", "ghi", "jkl", "mno", "pqrs", "tuv", "wxyz"};
    int n = (int)strlen(L[d]);
    return n ? L[d][t % n] : 0;
}

static void reset_compose(void) { h_cho = h_jung = h_jong = 0; alpha_comp = 0; last_key = -1; tap = 0; }

// MC_imHandleInput(key, 2, commit, &commit_len, compose, &compose_len). 버퍼는 5바이트다(0x104f86).
static void handle_input(cpu_t *c) {
    uint32_t key = c->r[0] & 0xff, commit_a = c->r[2], commit_len_a = c->r[3], compose_a = arg(c, 4), compose_len_a = arg(c, 5);
    char commit[5] = {0}, compose[5] = {0};
    uint64_t now = now_ms();
    if (key == FLUSH_KEY) { // 커서 이동, 지우기 전에 조합을 끝낸다.
        if (alpha_comp) commit[0] = alpha_comp;
        else hangul_compose(commit);
        reset_compose();
    } else if (key >= '0' && key <= '9') {
        int d = (int)key - '0';
        bool same = last_key == (int)key && now - last_ms < TAP_MS;
        tap = same ? tap + 1 : 0;
        if (mode == MODE_DIGIT) {
            commit[0] = (char)key;
        } else if (mode == MODE_ALPHA) {
            char l = alpha_letter(d, tap);
            if (!l) { // 1, 0은 숫자를 그대로 넣는다.
                commit[0] = alpha_comp ? alpha_comp : (char)key;
                commit[1] = alpha_comp ? (char)key : 0;
                alpha_comp = 0;
            } else {
                if (!same && alpha_comp) commit[0] = alpha_comp;
                alpha_comp = l;
                compose[0] = l;
            }
        } else {
            hangul_key(d, same, commit);
            hangul_compose(compose);
        }
        last_key = (int)key;
        last_ms = now;
    }
    int cl = (int)strlen(commit), pl = (int)strlen(compose);
    memcpy(c->m + commit_a, commit, 5);
    memcpy(c->m + compose_a, compose, 5);
    W32_(c, commit_len_a, (uint32_t)cl);
    W32_(c, compose_len_a, (uint32_t)pl);
    c->r[0] = cl + pl > 0;
}

void ime_api(cpu_t *c, uint32_t i) {
    switch (i) {
    case 0: handle_input(c); break;
    case 1:
        if (mode != (int)c->r[0]) reset_compose();
        mode = (int)c->r[0];
        c->r[0] = 0;
        break;
    case 2: c->r[0] = (uint32_t)mode; break;
    default: c->r[0] = 0;
    }
}

// TextComponent 위치만 기억한다. 그리기는 게임이 한다(0x104d3e).
static struct { int x, y, w, h; bool used; } fields[FIELD_MAX];

void uic_api(cpu_t *c, uint32_t i) {
    uint32_t *r = c->r;
    switch (i) {
    case 2: // create: 0은 게임이 빈 값으로 볼 수 있어 1부터 준다.
        r[0] = 0;
        for (int k = 0; k < FIELD_MAX; k++)
            if (!fields[k].used) { fields[k].used = true; r[0] = (uint32_t)k + 1; break; }
        break;
    case 3:
        if (r[0] >= 1 && r[0] <= FIELD_MAX) fields[r[0] - 1].used = false;
        r[0] = 0;
        break;
    case 9: // configure(handle, x, y, w, h, ...)
        if (r[0] >= 1 && r[0] <= FIELD_MAX) {
            fields[r[0] - 1].x = (int)r[1]; fields[r[0] - 1].y = (int)r[2]; fields[r[0] - 1].w = (int)r[3]; fields[r[0] - 1].h = (int)arg(c, 4);
        }
        r[0] = 0;
        break;
    case 10: { // MC_uicGetGeometry(handle, &x, &y, &w, &h)
        int k = r[0] >= 1 && r[0] <= FIELD_MAX ? (int)r[0] - 1 : -1;
        W32_(c, r[1], k < 0 ? 0 : (uint32_t)fields[k].x);
        W32_(c, r[2], k < 0 ? 0 : (uint32_t)fields[k].y);
        W32_(c, r[3], k < 0 ? 0 : (uint32_t)fields[k].w);
        W32_(c, arg(c, 4), k < 0 ? 0 : (uint32_t)fields[k].h);
        r[0] = 0;
        break;
    }
    default: r[0] = 0;
    }
}
