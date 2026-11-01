// 내장 서버: 1(1~11) 편지함과 개인공지. 근거와 정한 값은 work/net-offline.md의 우편 절.
#include "host.h"
#include "localnet.h"
#include <stdio.h>
#include <string.h>

enum {
    NS = 0x20d758,
    NS_ATTACH_OBJ = NS + 0x29c, // 고른 아이템 객체(0x13e8ce). 이름은 객체 +0x3f(0x13a2e4)
    OBJ_NAME = 0x3f,
    PAGE_ROWS = 8,              // 요청이 항상 8을 싣는다(0x151494, 0x15155c). 표도 8칸이다.
    FROM_LEN = 10,
    TITLE_MAX = 29,             // NS+0x2e4 30바이트 칸
    TEXT_MAX = 38,              // NS+0x3e9~0x410. 입력 칸은 36바이트까지(0x168ac4)
    ITEM_NAME_MAX = 26,         // NS+0x414~0x42f
    NOTICE_TITLE_MAX = 49,      // NS+0x47c 50바이트 칸
    NOTICE_BODY_MAX = 199,      // NS+0x60e 200바이트(0x151f00)
    LETTER_MAX = 64,
    NOTICE_MAX = 64,
    NAME_MAX = 32,
    MAILBOX_MAGIC = 0x4c4e4d31,
    ITEM_UID_BYTES = 8,         // 항목 앞 8바이트가 uid
    ITEM_CAT = 8, ITEM_ID = 9, ITEM_COUNT = 11, // 0x153d68: 종류 u8, 번호 be16, 개수 u8(0x13eb28)
    CAT_QUEST = 0x42,           // ITM/DAT/66 첫 항목이 접속확인증이다. 종류 0x42, 번호 0
};

static const char MSG_NO_RECIPIENT[] = "\xb9\xde\xb4\xc2\x20\xbb\xe7\xb6\xf7\xc0\xbb\x20\xc3\xa3\xc0\xbb\x20\xbc\xf6\x20\xbe\xf8\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 받는 사람을 찾을 수 없습니다.
static const char MSG_NO_NP[] = "\x4e\x50\xb0\xa1\x20\xba\xce\xc1\xb7\xc7\xd5\xb4\xcf\xb4\xd9\x2e"; // NP가 부족합니다.
static const char MSG_NO_ITEM[] = "\xc3\xb7\xba\xce\xc7\xd2\x20\xbe\xc6\xc0\xcc\xc5\xdb\xc0\xbb\x20\xc3\xa3\xc0\xbb\x20\xbc\xf6\x20\xbe\xf8\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 첨부할 아이템을 찾을 수 없습니다.
static const char MSG_BOX_FULL[] = "\xb9\xde\xb4\xc2\x20\xbb\xe7\xb6\xf7\xc0\xc7\x20\xc6\xed\xc1\xf6\xc7\xd4\xc0\xcc\x20\xb0\xa1\xb5\xe6\x20\xc3\xa1\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 받는 사람의 편지함이 가득 찼습니다.
static const char MSG_SENT[] = "\xb9\xdf\xbc\xdb\xb5\xc7\xbe\xfa\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 발송되었습니다.
static const char MSG_NO_LETTER[] = "\xc0\xcc\xb9\xcc\x20\xbb\xe7\xb6\xf3\xc1\xf8\x20\xc6\xed\xc1\xf6\xc0\xd4\xb4\xcf\xb4\xd9\x2e"; // 이미 사라진 편지입니다.
static const char MSG_DEPOT_FULL[] = "\xc3\xa2\xb0\xed\xbf\xa1\x20\xb3\xd6\xc0\xbb\x20\xbc\xf6\x20\xbe\xf8\xbd\xc0\xb4\xcf\xb4\xd9\x2e"; // 창고에 넣을 수 없습니다.
static const char MSG_NPC_NO_ATTACH[] = "\xb5\xb7\xb0\xfa\x20\xbe\xc6\xc0\xcc\xc5\xdb\xc0\xba\x20\xba\xb8\xb3\xbe\x20\xbc\xf6\x20\xbe\xf8\xb4\xc2\x20\xbb\xf3\xb4\xeb\xc0\xd4\xb4\xcf\xb4\xd9\x2e"; // 돈과 아이템은 보낼 수 없는 상대입니다.
static const char MSG_NO_NOTICE[] = "\xc0\xcc\xb9\xcc\x20\xbb\xe7\xb6\xf3\xc1\xf8\x20\xb0\xf8\xc1\xf6\xc0\xd4\xb4\xcf\xb4\xd9\x2e"; // 이미 사라진 공지입니다.
static const char WELCOME_FROM[] = "\xc5\xb0\xb3\xc9\xb3\xc9"; // 키냥냥
static const char WELCOME_TEXT[] = "\xc3\xb9\x20\xc1\xa2\xbc\xd3\x20\xc8\xae\xc0\xce\xc1\xf5\xc0\xbb\x20\xba\xb8\xb3\xbb\x20\xb5\xe5\xb8\xb3\xb4\xcf\xb4\xd9\x2e"; // 첫 접속 확인증을 보내 드립니다.
static const char WELCOME_ITEM[] = "\xc1\xa2\xbc\xd3\xc8\xae\xc0\xce\xc1\xf5"; // 접속확인증

typedef struct {
    uint64_t uid;
    char from[FROM_LEN + 1];
    char text[TEXT_MAX + 1];
    char item_name[ITEM_NAME_MAX + 1];
    int32_t np;
    uint8_t has_item;
    uint8_t item[LN_ITEM_SIZE];
} letter;

typedef struct {
    uint64_t uid;
    char title[NOTICE_TITLE_MAX + 1];
    char body[NOTICE_BODY_MAX + 1];
} notice;

static struct {
    uint32_t magic;
    uint64_t next_uid;
    int letter_count, notice_count, name_count;
    letter letters[LETTER_MAX];   // 오래된 것부터. 목록은 최신부터 보낸다.
    notice notices[NOTICE_MAX];
    char names[NAME_MAX][FROM_LEN + 1]; // 편지를 받아 주는 상대 이름
} box;
static bool box_loaded;

static void box_save(void) {
    if (!ln_save("mail", &box, sizeof box)) fprintf(stderr, "[net] 우편 저장 실패\n");
}

// EUC-KR 글자가 잘리지 않게 n바이트까지 복사한다.
static void copy_text(char *out, const char *in, size_t n) {
    size_t i = 0;
    while (i < n && in[i]) {
        size_t w = (in[i] & 0x80) ? 2 : 1;
        if (i + w > n || (w == 2 && !in[i + 1])) break;
        memcpy(out + i, in + i, w);
        i += w;
    }
    out[i] = 0;
}

static letter *letter_new(const char *from, const char *text) {
    if (box.letter_count >= LETTER_MAX) return NULL;
    letter *l = &box.letters[box.letter_count++];
    memset(l, 0, sizeof *l);
    l->uid = box.next_uid++;
    copy_text(l->from, from, FROM_LEN);
    copy_text(l->text, text, TEXT_MAX);
    return l;
}

// 품목 설명이 "나에게 온 첫번째 편지!"이고 퀘스트 네트워크 접속(/DAT/QST, MAP/SC/000)이 이것을 받아 오라고 한다. 첫 접속에 한 통 넣는다.
static void add_welcome(void) {
    letter *l = letter_new(WELCOME_FROM, WELCOME_TEXT);
    l->has_item = 1;
    l->item[ITEM_CAT] = CAT_QUEST;
    l->item[ITEM_ID] = 0;
    l->item[ITEM_ID + 1] = 0;
    l->item[ITEM_COUNT] = 1;
    copy_text(l->item_name, WELCOME_ITEM, ITEM_NAME_MAX);
}

static void box_load(void) {
    if (box_loaded) return;
    box_loaded = true;
    if (ln_load("mail", &box, sizeof box) == (long)sizeof box && box.magic == MAILBOX_MAGIC) return;
    memset(&box, 0, sizeof box);
    box.magic = MAILBOX_MAGIC;
    box.next_uid = 1;
    add_welcome();
    box_save();
}

static uint64_t read_u64(ln_req *q) {
    uint64_t hi = ln_u32(q);
    return hi << 32 | ln_u32(q);
}

static void write_title(const char *s) {
    size_t n = strlen(s);
    ln_w8((uint8_t)n);
    ln_wbytes(s, n);
}

// 1.1 0x151980: 개수, (uid be64, 종류 u8, 제목 길이 u8, 제목), 다음 쪽 여부 u8.
// 종류 1이면 1.2에 카운터가 붙는다(0x1514ec). 첨부 수령이 재시작 복구로 되풀이되지 않게 한 값으로 보인다. 첨부가 있으면 1로 둔다. 읽기만 하고 쓰지 않는다.
static void letter_list(ln_req *q) {
    int rows = ln_u8(q);
    int page = ln_u8(q);
    if (rows <= 0 || rows > PAGE_ROWS) rows = PAGE_ROWS;
    int skip = page * rows;
    int n = box.letter_count - skip;
    if (n < 0) n = 0;
    if (n > rows) n = rows;
    ln_begin(1, 1, 0);
    ln_w8((uint8_t)n);
    for (int i = 0; i < n; i++) {
        const letter *l = &box.letters[box.letter_count - 1 - skip - i];
        char title[TITLE_MAX + 1], joined[FROM_LEN + TEXT_MAX + 4];
        snprintf(joined, sizeof joined, l->text[0] ? "%s : %s" : "%s", l->from, l->text);
        copy_text(title, joined, TITLE_MAX);
        ln_w64(l->uid);
        ln_w8(l->np != 0 || l->has_item);
        write_title(title);
    }
    ln_w8(box.letter_count > skip + rows);
    ln_send();
}

static void letter_remove(letter *l) {
    int i = (int)(l - box.letters);
    memmove(l, l + 1, (size_t)(box.letter_count - i - 1) * sizeof *l);
    box.letter_count--;
}

// 1.2 0x151ae8: 보낸 이 10, 내용(u8 길이), 받은 NP be32, 아이템 이름(u8 길이), 받은 뒤 NP be32.
// 삭제 요청이 없고 돌아가면 게임이 목록을 다시 받으므로(0x169f90) 읽으면 첨부를 주고 지운다.
static void letter_read(ln_req *q) {
    uint64_t uid = read_u64(q);
    letter *l = NULL;
    for (int i = 0; i < box.letter_count; i++)
        if (box.letters[i].uid == uid) l = &box.letters[i];
    if (!l) { ln_fail(1, 2, MSG_NO_LETTER); return; }
    if (l->has_item && !ln_depot_put(l->item)) { ln_fail(1, 2, MSG_DEPOT_FULL); return; }
    if (l->np) ln_np_add(l->np);
    ln_begin(1, 2, 0);
    ln_wbytes(l->from, FROM_LEN);
    write_title(l->text);
    ln_w32((uint32_t)l->np);
    write_title(l->item_name);
    ln_w32((uint32_t)ln_me()->np);
    ln_send();
    letter_remove(l);
    box_save();
}

static bool is_npc(const char *name) {
    for (int i = 0; i < box.name_count; i++)
        if (!strncmp(box.names[i], name, FROM_LEN)) return true;
    return false;
}

// 1.3 0x151408, 0x151c5c: 성공이면 소지 NP be32, 안내문. 실패면 안내문이고 화면은 입력을 유지한다.
// 받는 사람 검증은 클라이언트에 없다. 플레이어가 혼자이므로 자기 자신에게는 진짜로 배달하고, 등록된 상대는 내용만 받으며, 그 밖은 실패다.
static void letter_send(ln_req *q) {
    char to[FROM_LEN + 1] = {0}, text[TEXT_MAX + 1] = {0}, raw[256];
    ln_bytes(q, to, FROM_LEN);
    int len = ln_u8(q);
    ln_bytes(q, raw, (size_t)len);
    raw[len] = 0;
    copy_text(text, raw, TEXT_MAX);
    int32_t np = (int32_t)ln_u32(q);
    uint64_t item_uid = read_u64(q);

    ln_profile *me = ln_me();
    bool self = !strncmp(to, me->nick, FROM_LEN);
    if (!self && !is_npc(to)) { ln_fail(1, 3, MSG_NO_RECIPIENT); return; }
    if (!self && (np || item_uid)) { ln_fail(1, 3, MSG_NPC_NO_ATTACH); return; }
    if (np < 0 || np > me->np) { ln_fail(1, 3, MSG_NO_NP); return; }
    if (self && box.letter_count >= LETTER_MAX) { ln_fail(1, 3, MSG_BOX_FULL); return; }

    uint8_t item[LN_ITEM_SIZE] = {0};
    if (item_uid && !ln_depot_take(item_uid, item)) { ln_fail(1, 3, MSG_NO_ITEM); return; }
    if (self) {
        letter *l = letter_new(me->nick, text);
        l->np = np;
        if (item_uid) {
            l->has_item = 1;
            memcpy(l->item, item, sizeof item);
            uint32_t obj = R32_(&cpu, NS_ATTACH_OBJ);
            if (obj) copy_text(l->item_name, gstr(obj + OBJ_NAME), ITEM_NAME_MAX);
        }
        if (np) ln_np_add(-np);
        box_save();
    }
    ln_begin(1, 3, 0);
    ln_w32((uint32_t)me->np);
    ln_wstr16(MSG_SENT);
    ln_send();
}

// 1.10 0x151d98: 개인공지 목록. 형식은 편지 목록과 같고 제목 칸이 50바이트다. 문구 "새로운 공지사항이 없습니다"대로 읽으면 지운다.
static void notice_list(ln_req *q) {
    int rows = ln_u8(q);
    int page = ln_u8(q);
    if (rows <= 0 || rows > PAGE_ROWS) rows = PAGE_ROWS;
    int skip = page * rows;
    int n = box.notice_count - skip;
    if (n < 0) n = 0;
    if (n > rows) n = rows;
    ln_begin(1, 10, 0);
    ln_w8((uint8_t)n);
    for (int i = 0; i < n; i++) {
        const notice *e = &box.notices[box.notice_count - 1 - skip - i];
        ln_w64(e->uid);
        write_title(e->title);
    }
    ln_w8(box.notice_count > skip + rows);
    ln_send();
}

// 1.11 0x151f00: 본문(be16 길이).
static void notice_read(ln_req *q) {
    uint64_t uid = read_u64(q);
    for (int i = 0; i < box.notice_count; i++) {
        notice *e = &box.notices[i];
        if (e->uid != uid) continue;
        ln_begin(1, 11, 0);
        ln_wstr16(e->body);
        ln_send();
        memmove(e, e + 1, (size_t)(box.notice_count - i - 1) * sizeof *e);
        box.notice_count--;
        box_save();
        return;
    }
    ln_fail(1, 11, MSG_NO_NOTICE);
}

void ln_notice_add(const char *title, const char *body) {
    box_load();
    if (box.notice_count >= NOTICE_MAX) {
        memmove(box.notices, box.notices + 1, (NOTICE_MAX - 1) * sizeof box.notices[0]);
        box.notice_count--;
    }
    notice *e = &box.notices[box.notice_count++];
    e->uid = box.next_uid++;
    copy_text(e->title, title, NOTICE_TITLE_MAX);
    copy_text(e->body, body, NOTICE_BODY_MAX);
    box_save();
}

void ln_mail_add_name(const char *name) {
    box_load();
    if (is_npc(name) || box.name_count >= NAME_MAX) return;
    copy_text(box.names[box.name_count++], name, FROM_LEN);
    box_save();
}

bool ln_mail(ln_req *q) {
    box_load();
    switch (q->minor) {
    case 1: letter_list(q); return true;
    case 2: letter_read(q); return true;
    case 3: letter_send(q); return true;
    case 10: notice_list(q); return true;
    case 11: notice_read(q); return true;
    }
    return false;
}
