#!/usr/bin/env python3
"""codemap.json의 Thumb 함수를 C로 기계 번역한다(방식 시험용).

레지스터와 플래그는 cpu 구조체, 메모리는 m(게스트 주소 + 베이스)로 접근한다.
thumb2c.py <client.bin> <codemap.json> <out.c>
"""
import json, re, struct, sys
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

data = open(sys.argv[1], 'rb').read()
cm = json.load(open(sys.argv[2]))
LOAD = 0x100000
GOT0 = cm['summary']['GOT'][0]
u32 = lambda o: struct.unpack_from('<I', data, o)[0]
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
funcs = {int(k, 16): v for k, v in cm['roots'].items() if v['thumb'] and v.get('addrs')}
RN = {'sp': 'SP', 'lr': 'LR', 'sb': 'r[9]', 'sl': 'r[10]', 'fp': 'r[11]', 'ip': 'r[12]'}
def R(n):
    if n in RN: return RN[n]
    return f'r[{n[1:]}]'
COND = {'eq':'c->z','ne':'!c->z','hs':'c->cf','cs':'c->cf','lo':'!c->cf','cc':'!c->cf','mi':'c->n','pl':'!c->n','vs':'c->v','vc':'!c->v',
        'hi':'(c->cf&&!c->z)','ls':'(!c->cf||c->z)','ge':'(c->n==c->v)','lt':'(c->n!=c->v)','gt':'(!c->z&&c->n==c->v)','le':'(c->z||c->n!=c->v)'}
LOADS = {'ldr':'R32','ldrb':'R8','ldrh':'R16','ldrsb':'RS8','ldrsh':'RS16'}
STORES = {'str':'W32','strb':'W8','strh':'W16'}
ALU = {'ands':'{d}={a}&{b};NZ({d});','and':'{d}={a}&{b};NZ({d});','eors':'{d}={a}^{b};NZ({d});','orrs':'{d}={a}|{b};NZ({d});',
       'bics':'{d}={a}&~{b};NZ({d});','mvns':'{d}=~{b};NZ({d});','muls':'{d}={a}*{b};NZ({d});',
       'adcs':'{d}=addc(c,{a},{b},c->cf);','sbcs':'{d}=addc(c,{a},~{b},c->cf);',
       'lsls':'{d}=lsl_(c,{a},{b});','lsrs':'{d}=lsr_(c,{a},{b});','asrs':'{d}=asr_(c,{a},{b});','rors':'{d}=ror_(c,{a},{b});'}

def tr(ins, lits_pc, targets, far, fn):
    m, ops = ins.mnemonic, ins.op_str
    pc = ins.address
    p = [x.strip() for x in re.sub(r'[\[\]!]', '', ops.split('{')[0]).split(',') if x.strip()]
    def val(x):
        if x.startswith('#'):
            return f'{int(x[1:], 0) & 0xffffffff}u'
        if x == 'pc':
            return f'{(pc + 4 + LOAD) & 0xffffffff}u'
        return R(x)
    d = R(p[0]) if p and p[0] != 'pc' else ''
    if m in LOADS or m in STORES:
        base = p[1]
        if base == 'pc':
            ea = f'{((pc + 4) & ~3) + LOAD + int(p[2][1:], 0)}u'
        else:
            off = val(p[2]) if len(p) > 2 else '0u'
            ea = f'{val(base)}+{off}'
        return f'{d}={LOADS[m]}({ea});' if m in LOADS else f'{STORES[m]}({ea},{d});'
    if m in ('mov', 'movs'):
        if p[0] == 'pc':
            return None
        return f'{d}={val(p[1])};' + ('NZ(%s);' % d if m == 'movs' else '')
    if m in ('add', 'adds', 'sub', 'subs'):
        if len(p) == 2: a, b = p[0], p[1]
        else: a, b = p[1], p[2]
        sub = m.startswith('sub')
        if m.endswith('s'):
            return f'{d}=addc(c,{val(a)},{"~" if sub else ""}{val(b)},{1 if sub else 0});'
        if p[0] == 'pc':
            return None
        # add rd, pc, #i는 워드 정렬된 pc를 쓴다.
        av = f'{(pc + 4 + LOAD) & ~3 & 0xffffffff}u' if a == 'pc' and len(p) == 3 else val(a)
        return f'{d}={av}{"-" if sub else "+"}{val(b)};'
    if m == 'rsbs':
        return f'{d}=addc(c,~{val(p[1])},0u,1);'
    if m == 'negs':
        return f'{d}=addc(c,~{val(p[1])},0u,1);'
    if m == 'cmp':
        return f'addc(c,{val(p[0])},~{val(p[1])},1);'
    if m == 'cmn':
        return f'addc(c,{val(p[0])},{val(p[1])},0);'
    if m == 'tst':
        return f'{{uint32_t t={val(p[0])}&{val(p[1])};NZ(t);}}'
    if m in ALU:
        a, b = (p[0], p[1]) if len(p) == 2 else (p[1], p[2])
        return ALU[m].format(d=d, a=val(a), b=val(b))
    if m in ('push', 'pop', 'ldm', 'stm', 'ldmia', 'stmia'):
        regs = [x.strip() for x in ops.split('{')[1].rstrip('}').split(',')]
        if m == 'push':
            s = f'SP-={4 * len(regs)};{{uint32_t a=SP;'
            for r in regs: s += f'W32(a,{R(r)});a+=4;'
            return s + '}'
        if m == 'pop':
            s = '{uint32_t a=SP;'
            for r in regs:
                if r == 'pc': s += 'a+=4;'
                else: s += f'{R(r)}=R32(a);a+=4;'
            return s + f'SP=a;}}'
        s = f'{{uint32_t a={R(p[0])};'
        wb = p[0] not in regs
        for r in regs:
            s += (f'{R(r)}=R32(a);a+=4;' if m.startswith('ldm') else f'W32(a,{R(r)});a+=4;')
        return s + (f'{R(p[0])}=a;' if wb or not m.startswith('ldm') else '') + '}'
    raise ValueError(f'{pc:#x} {m} {ops}')

def popped_return(decoded, addrs, idx, reg):
    """pop {rN}; (add sp 등); bx rN 형태의 복귀인지 본다."""
    for k in range(idx - 1, max(idx - 5, -1), -1):
        i = decoded[addrs[k]]
        if i.mnemonic == 'pop' and re.search(rf'\b{reg}\b', i.op_str):
            return True
        if re.match(rf'{reg}\b', i.op_str) and i.mnemonic not in ('cmp', 'tst', 'str'):
            return False
    return False

def gen(entry, f):
    tabs = {int(t): n for t, n in f['tables']}
    addrs = f['addrs']
    aset = set(addrs)
    out = [f'void f_{entry:x}(cpu_t*c){{uint32_t*r=c->r;uint8_t*m=c->m;(void)r;(void)m;']
    decoded = {}
    for a in addrs:
        decoded[a] = next(md.disasm(data[a:a + 4], a, 1))
    # 라벨
    labels = set()
    for a, i in decoded.items():
        if i.mnemonic == 'b' or (i.mnemonic[0] == 'b' and i.mnemonic[1:] in COND):
            labels.add(int(i.op_str.split('#')[1], 0))
        if i.mnemonic == 'bl':
            if a in f['far']: labels.add(int(i.op_str.split('#')[1], 0))
    for t, n in tabs.items():
        for k in range(n):
            v = u32(t + k * 4)
            labels.add((t + (v - (1 << 32) if v >> 31 else v)) & 0xffffffff)
    popped = None
    body = []
    if addrs[0] != entry:
        body.append(f'goto L{entry:x};')
    for idx, a in enumerate(addrs):
        i = decoded[a]
        m, ops = i.mnemonic, i.op_str
        if a in labels or a == entry:
            body.append(f'L{a:x}:;')
        body.append(f'TICK(0x{a:x});')
        nxt = a + i.size
        fall = True
        if m == 'b':
            body.append(f'goto L{int(ops.split("#")[1], 0):x};'); fall = False
        elif m[0] == 'b' and m[1:] in COND:
            body.append(f'if({COND[m[1:]]})goto L{int(ops.split("#")[1], 0):x};')
        elif m == 'bl':
            t = int(ops.split('#')[1], 0)
            if a in f['far']:
                body.append(f'LR={(nxt + LOAD) | 1}u;goto L{t:x};'); fall = False
            else:
                body.append(f'LR={(nxt + LOAD) | 1}u;f_{t:x}(c);' if t in funcs else f'LR={(nxt + LOAD) | 1}u;call_ind(c,{t + LOAD | 1}u);')
        elif m == 'bx':
            if ops == 'lr':
                body.append('return;')
            elif ops == 'pc':
                body.append(f'call_ind(c,{(a + 4 & ~3) + LOAD}u);return;')
            elif popped_return(decoded, addrs, idx, ops):
                body.append('return;')
            else:
                body.append(f'call_ind(c,{R(ops)});return;')
            fall = False
        elif m == 'pop' and 'pc' in ops:
            body.append(tr(i, None, None, None, f)); body.append('return;'); fall = False
        elif m == 'mov' and ops.startswith('pc,'):
            src = ops.split(',')[1].strip()
            tab = None
            for t in tabs:
                # 이 함수의 점프표: 값이 라벨 중 하나면 switch로 보낸다.
                tab = t
            prev = decoded.get(addrs[idx - 1]) if idx else None
            if tabs and src != 'lr' and not popped_return(decoded, addrs, idx, src):
                cases = ''.join(f'case {(l + LOAD) & 0xffffffff}u:goto L{l:x};' for l in sorted(labels) if l in aset)
                body.append(f'switch({R(src)}){{{cases}default:call_ind(c,{R(src)});return;}}')
            elif src == 'lr' or popped_return(decoded, addrs, idx, src):
                body.append('return;')
            else:
                body.append(f'call_ind(c,{R(src)});return;')
            fall = False
        else:
            body.append(tr(i, None, None, None, f))
        if fall and idx + 1 < len(addrs) and addrs[idx + 1] != nxt:
            body.append(f'goto L{nxt:x};')
        elif fall and idx + 1 == len(addrs):
            body.append('return;')
        if a == addrs[-1] and fall and nxt not in aset:
            pass
    out.append('\n'.join(body))
    # 쓰이지 않은 라벨 경고 방지용 끝맺음
    out.append('}')
    return '\n'.join(out)

fails = []
with open(sys.argv[3], 'w') as w:
    w.write('#include "thumbrt.h"\n')
    for e in funcs:
        w.write(f'void f_{e:x}(cpu_t*c);\n')
    # 사람이 옮긴 함수(파일 오프셋 목록)는 건너뛴다. 정의는 native/restored/에 있다.
    restored = {int(l.split()[0], 16) for l in open(sys.argv[4]) if l.strip() and not l.startswith('#')} if len(sys.argv) > 4 else set()
    # `wrap`이 붙은 함수는 orig_f_<오프셋>으로 남겨 읽는 C가 원본을 호출할 수 있게 한다.
    rows = [l.split() for l in open(sys.argv[4]) if l.strip() and not l.startswith('#')] if len(sys.argv) > 4 else []
    wrapped = {int(x[0], 16) for x in rows if x[-1] == 'wrap'}
    restored -= wrapped
    for e, f in sorted(funcs.items()):
        if e in restored:
            continue
        try:
            code = gen(e, f)
            if e in wrapped:
                code = code.replace(f'void f_{e:x}(cpu_t*c){{', f'void orig_f_{e:x}(cpu_t*c){{', 1)
            w.write(code + '\n')
        except Exception as ex:
            fails.append((hex(e), str(ex)))
            w.write(f'void f_{e:x}(cpu_t*c){{c->fault=1;}}\n')
    w.write('const fn_entry_t fn_table[]={\n')
    for e in sorted(funcs):
        w.write(f'{{{e + LOAD}u,f_{e:x}}},\n')
    w.write('{0,0}};\n')
print('functions', len(funcs), 'translate failures', len(fails), fails[:20])
