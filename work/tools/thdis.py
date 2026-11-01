#!/usr/bin/env python3
"""thdis.py <게스트 주소 hex> <바이트 수> [client.bin]: Thumb 역어셈블."""
import sys,re
from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB
d=open(sys.argv[3] if len(sys.argv)>3 else 'build/game/client.bin','rb').read()
md=Cs(CS_ARCH_ARM,CS_MODE_THUMB)
a=int(sys.argv[1],16)-0x100000;n=int(sys.argv[2])
for x in md.disasm(d[a:a+n],a): print(hex(x.address+0x100000),x.mnemonic,x.op_str)
