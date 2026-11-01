// NT 창고 저장소. 창고(3.1~6)와 경매(3.20~21)가 같이 쓴다. 근거는 work/net-offline.md의 창고와 경매 절.
#pragma once
#include "localnet.h"
#include <stdint.h>

enum { DEPOT_PAGES = 5, DEPOT_PAGE_ITEMS = 30, DEPOT_MAX = DEPOT_PAGES * DEPOT_PAGE_ITEMS, DEPOT_PAL_MAX = 85 };

// 응답 레코드(NS+0x146c, 0x94바이트)와 게임 아이템의 대응은 괄호 안 오프셋이다.
typedef struct {
    uint32_t uid;     // 판매번호를 겸한다
    int8_t kind;      // +0x20. /ITM/DAT 파일 번호
    int16_t id;       // +0x22
    uint8_t count;    // +0x16d
    uint8_t opt[5];   // +0x177, +0x178, +0x17a, +0x179, +0x17b 순서. 패킷 순서 그대로
    uint32_t serial;  // +0x170
    uint8_t refine[2]; // +0x17c, +0x17d
    uint8_t pal_len;  // 염색 팔레트 바이트 수. 목록에는 절반(+0x180)이 나간다
    uint8_t pal[DEPOT_PAL_MAX];
    uint8_t listed;   // 1이면 경매 중
    int32_t price;    // 판매가 NP
    int64_t listed_at; // 경매 등록 시각(초)
} depot_item;

depot_item *depot_items(int *n);
void depot_save(void);
// 끝에 새 항목을 잡아 uid를 채워 돌려준다. 가득 차면 NULL.
depot_item *depot_add(void);
depot_item *depot_find(uint32_t uid);
void depot_remove(depot_item *it);
// 비어 있는 공장 상태 값. 시장 물건과 구매품이 쓴다(0x1372dc 초기값).
void depot_item_defaults(depot_item *it);

// 경매장 목록에 분류가 있는 종류인지. 없으면 등록할 수 없다.
bool auction_kind_allowed(int kind);
// 등록한 물건 중 팔릴 조건이 찬 것을 처리한다. 창고 목록을 내려주기 전에 부른다.
void auction_settle(void);
