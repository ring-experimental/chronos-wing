// 내장 서버: 1(30~33) 휴면과 SMS 설정.
#include "localnet.h"

bool ln_sleep(ln_req *q) {
    (void)q;
    return false;
}
