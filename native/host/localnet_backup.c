// 내장 서버: 4 백업과 복구.
#include "localnet.h"

bool ln_backup(ln_req *q) {
    switch (q->minor) {
    case 1: // 0x154884. 결과가 0이든 -1이든 다음은 공지 요청이다(0x154b88).
        ln_upload_set(q->p, (size_t)(q->end - q->p));
        ln_begin(4, 1, 0);
        ln_send();
        return true;
    }
    return false;
}
