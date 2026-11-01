#!/usr/bin/env python3
"""netxref.py <native/build> <문자열 정규식>: 그 문자열을 쓰는 함수와 호출자를 찾는다. 주소는 게스트 주소(파일 오프셋 + 0x100000)다."""
import json,struct,re,pickle,sys
from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB
R=sys.argv[1].rstrip('/')+'/'  # native/build
d=open(R+'game/client.bin','rb').read()
cm=json.load(open(R+'codemap.json'))
funcs={int(k,16):v for k,v in cm['roots'].items() if v['thumb'] and v.get('addrs')}
GOT=cm['summary']['GOT'][0]; GOTEND=cm['summary']['GOT'][1]
md=Cs(CS_ARCH_ARM,CS_MODE_THUMB)
u32=lambda o:struct.unpack_from('<I',d,o)[0]
strs={}
for m in re.finditer(rb'[\x20-\x7e\xa1-\xfe]{2,}\x00',d[0x8ef74:0x9a340]):
    a=0x8ef74+m.start()
    try:strs[a]=m.group()[:-1].decode('euc-kr')
    except:pass
X={}
for f,v in funcs.items():
    S=[];G=[];C=[]
    for a in v['addrs']:
        ins=next(md.disasm(d[a:a+4],a),None)
        if not ins: continue
        if ins.mnemonic=='bl':
            C.append(int(ins.op_str[1:],0))
        m=re.match(r'r\d, \[pc, #(0x[0-9a-f]+|\d+)\]',ins.op_str)
        if ins.mnemonic=='ldr' and m:
            lit=((a+4)&~3)+int(m.group(1),0)
            if lit+4>len(d):continue
            L=u32(lit); sL=L-(1<<32) if L>>31 else L
            t=GOT+sL
            if t in strs: S.append((a,t,strs[t]))
            elif 0<=sL<GOTEND-GOT and sL%4==0:
                tv=u32(GOT+sL)+0x1398
                if tv in strs: S.append((a,tv,strs[tv]))
                else: G.append((a,sL,tv+0x100000))
            elif 0<t<len(d)+500000: G.append((a,None,t))
    X[f]=dict(S=S,G=G,C=C,end=v.get('end'))
callers={}
for f,x in X.items():
    for c in x['C']: callers.setdefault(c,[]).append(f)
if len(sys.argv)<3:
    print(__doc__); sys.exit()
pat=re.compile(sys.argv[2])
for f,v in sorted(X.items()):
    h=[s[2] for s in v['S'] if pat.search(s[2])]
    if h: print(hex(f+0x100000),h,'호출자',[hex(c+0x100000) for c in callers.get(f,[])][:6])
