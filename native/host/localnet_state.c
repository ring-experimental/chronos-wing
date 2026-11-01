// 내장 서버의 공통 상태. 프로필, 동료 목록, 마지막 4.1 요청을 net/에 둔다.
#include "host.h"
#include "localnet.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

enum { PROFILE_MAGIC = 0x4c4e5031, UPLOAD_MAX = 0x800, UPLOAD_COMPANIONS = 53, COMPANION_SIZE = 10 };

static ln_profile me;
static bool me_loaded;
static ln_member members[LN_MEMBER_MAX];
static int member_count = -1;
static uint8_t upload[UPLOAD_MAX];
static long upload_len = -2;

// 서버에만 있던 값이라 정한 초기값이다. 근거는 work/net-offline.md의 세션 절.
static void profile_defaults(void) {
    memset(&me, 0, sizeof me);
    me.magic = PROFILE_MAGIC;
    me.user_id = 1;
    me.np = 1000;
    me.level = 1;
    me.fame_max = 100;
    me.rank = 1;
    me.limit_attack = 180;
    me.limit_counter = 120;
    me.limit_double = 60;
    strcpy(me.grade, "\xc3\xca\xba\xb8\x20\xc7\xd4\xc0\xe5"); // 초보 함장
    me.sleep_min = me.shield_min = me.np_regen_min = me.np_bonus_min = -1;
}

ln_profile *ln_me(void) {
    if (!me_loaded) {
        me_loaded = true;
        if (ln_load("profile", &me, sizeof me) != (long)sizeof me || me.magic != PROFILE_MAGIC) profile_defaults();
    }
    return &me;
}

void ln_me_save(void) {
    ln_me();
    if (!ln_save("profile", &me, sizeof me)) fprintf(stderr, "[net] 프로필 저장 실패\n");
}

bool ln_has_account(void) { return ln_me()->nick[0] != 0; }

int32_t ln_np_add(int32_t delta) {
    ln_profile *p = ln_me();
    int64_t v = (int64_t)p->np + delta;
    p->np = v < 0 ? 0 : v > INT32_MAX ? INT32_MAX : (int32_t)v;
    ln_me_save();
    return p->np;
}

bool ln_np_spend(int32_t cost) {
    if (ln_me()->np < cost) return false;
    ln_np_add(-cost);
    return true;
}

void ln_fame_add(int32_t delta) {
    ln_profile *p = ln_me();
    p->fame += delta;
    p->fame_today += delta;
    if (p->fame < 0) p->fame = 0;
    while (p->fame >= p->fame_max && p->level < 127) {
        p->fame -= p->fame_max;
        p->level++;
        p->fame_max = p->level * 100;
    }
    ln_me_save();
}

void ln_write_profile(void) {
    ln_sleep_tick();
    const ln_profile *p = ln_me();
    ln_w32((uint32_t)p->user_id);
    ln_wbytes(p->nick, 10);
    ln_w32((uint32_t)p->np);
    ln_w32((uint32_t)p->level);
    ln_w32((uint32_t)p->fame);
    ln_w32((uint32_t)p->fame_today);
    ln_w32((uint32_t)p->rank);
    ln_w32((uint32_t)p->rank_today);
    ln_w32((uint32_t)p->hit);
    ln_w32((uint32_t)p->defend);
    ln_w32((uint32_t)p->hit_month);
    ln_w32((uint32_t)p->defend_month);
    ln_w32((uint32_t)p->limit_attack);
    ln_w32((uint32_t)p->limit_counter);
    ln_w32((uint32_t)p->limit_double);
    ln_w8(p->sleeping);
    ln_wbytes(p->grade, 10);
    ln_w32((uint32_t)p->fame_max);
    ln_w8(p->sms);
    ln_w8(p->auto_protect);
    ln_w32((uint32_t)p->sleep_min);
    ln_w32((uint32_t)p->shield_min);
    ln_w32((uint32_t)p->np_regen_min);
    ln_w32((uint32_t)p->np_bonus_min);
    ln_w8(p->double_counter);
    ln_wbytes(p->guild, 12);
}

static void members_load(void) {
    if (member_count >= 0) return;
    long n = ln_load("members", members, sizeof members);
    member_count = n > 0 ? (int)(n / (long)sizeof(ln_member)) : 0;
}

ln_member *ln_members(int *n) {
    members_load();
    if (n) *n = member_count;
    return members;
}

void ln_members_save(void) {
    members_load();
    if (!ln_save("members", members, (size_t)member_count * sizeof(ln_member))) fprintf(stderr, "[net] 동료 저장 실패\n");
}

// 0x155210, 0x152040, 0x155398, 0x1542a4 공통 항목. 동료는 받는 쪽이 계약기간을 버리고 1로 고정한다.
void ln_write_unit(const ln_member *m) {
    size_t len = strnlen(m->name, sizeof m->name);
    int64_t left = m->expire - (int64_t)time(NULL);
    ln_w64((uint64_t)m->uid);
    ln_w16((uint16_t)m->id);
    ln_w8((uint8_t)m->level);
    ln_w8((uint8_t)len);
    ln_wbytes(m->name, len);
    ln_w16((uint16_t)m->recover_cost);
    ln_w16((uint16_t)m->floor);
    ln_w16((uint16_t)m->room);
    ln_w16((uint16_t)m->slot);
    ln_w8(m->fatigue);
    // +0x54 남은 일, +0x58 남은 시간. 둘 다 1보다 작으면 계약만료(0x162c76, 0x1641fc)
    ln_w32(m->uid < 0 && left > 0 ? (uint32_t)(left / 86400) : 0);
    ln_w8(m->uid < 0 && left > 0 ? (uint8_t)(left % 86400 / 3600) : 0);
    ln_w32((uint32_t)m->hp);
    ln_w8(m->attack_count);
    ln_w32((uint32_t)m->attack);
    for (int i = 1; i < m->attack_count; i++) ln_w32((uint32_t)m->attack_more[i - 1]);
    ln_w16((uint16_t)m->pdef);
    ln_w16((uint16_t)m->mdef);
    ln_w8((uint8_t)m->crit);
    ln_w8(m->grade);
}

void ln_write_members(void) {
    members_load();
    ln_w16((uint16_t)member_count);
    for (int i = 0; i < member_count; i++) ln_write_unit(&members[i]);
}

// 표 앞 8칸이 모두 슬롯 순서로 덮이므로 응답하지 않은 칸의 /DAT/SL 이름이나 +0x44, +0x46이 어긋날 일이 없다.
// 합류 안 한 동료는 피로도 100으로 보내 원본 화면이 회색, 전투불능으로 그리게 한다(정한 값).
void ln_members_sync(const int16_t *ids, const int8_t *levels, int n) {
    enum { TABLE = 0x1a9e58, ENTRY = 0x74, NAME = 4 }; // 이름은 요청 직전 0x128278이 /DAT/SL에서 다시 읽어 둔 표에서 가져온다.
    ln_member old[LN_MEMBER_MAX];
    int old_n;
    memcpy(old, ln_members(&old_n), sizeof old);
    member_count = 0;
    for (int i = 0; i < n && i < LN_MEMBER_MAX; i++) {
        ln_member *m = &members[member_count++];
        memset(m, 0, sizeof *m);
        m->uid = i + 1;
        m->floor = m->room = m->slot = -1;
        for (int k = 0; k < old_n; k++)
            if (old[k].uid == m->uid) *m = old[k];
        m->id = ids[i];
        m->level = levels[i];
        memcpy(m->name, gstr(TABLE + (uint32_t)i * ENTRY + NAME), sizeof m->name);
        m->name[sizeof m->name - 1] = 0;
        bool joined = ln_upload_joined(i);
        if (!joined) {
            m->floor = m->room = m->slot = -1;
            m->fatigue = 100;
        } else if (!m->joined) {
            m->fatigue = 0;
        }
        m->joined = joined;
        ln_unit_stats(m);
    }
    ln_members_save();
}

const uint8_t *ln_upload(size_t *n) {
    if (upload_len == -2) upload_len = ln_load("upload", upload, sizeof upload);
    if (upload_len < 0) return NULL;
    if (n) *n = (size_t)upload_len;
    return upload;
}

void ln_upload_set(const uint8_t *body, size_t n) {
    if (n > sizeof upload) n = sizeof upload;
    memcpy(upload, body, n);
    upload_len = (long)n;
    if (!ln_save("upload", upload, n)) fprintf(stderr, "[net] 4.1 저장 실패\n");
}

bool ln_upload_joined(int slot) {
    size_t n = 0;
    const uint8_t *u = ln_upload(&n);
    size_t at = UPLOAD_COMPANIONS + (size_t)slot * COMPANION_SIZE;
    return u && at < n && u[at] != 0;
}
