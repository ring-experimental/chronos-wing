#!/usr/bin/env python3
"""복원 방식 시험. 같은 입력에서 원본 Thumb(Unicorn) 대 기계 번역 C, 대 Ghidra C를 비교한다.
trial.py <client.bin> <codemap.json> <all.c> <tr.dylib> <표본 수> <out.json>"""
import ctypes, json, random, re, struct, subprocess, sys, tempfile, os
from unicorn import *
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile

data = open(sys.argv[1], 'rb').read()
cm = json.load(open(sys.argv[2])); S = cm['summary']
allc = open(sys.argv[3]).read()
lib = ctypes.CDLL(sys.argv[4])
SAMPLE = int(sys.argv[5]); OUT = sys.argv[6]
LOAD, MEMSZ, ARENA, STACK = 0x100000, 0x420000, 0x300000, 0x410000
MAGIC = 0x7f0000
BASE = S['BASE']
img = bytearray(MEMSZ)
img[LOAD:LOAD + len(data)] = data
u32 = lambda o: struct.unpack_from('<I', data, o)[0]
b0 = LOAD + BASE
for i in range(u32(0x70)):
    o = b0 + u32(0x74 + i * 4)
    struct.pack_into('<I', img, o, (struct.unpack_from('<I', img, o)[0] + b0) & 0xffffffff)
for o in range(LOAD + S['GOT'][0], LOAD + S['GOT'][1], 4):
    struct.pack_into('<I', img, o, (struct.unpack_from('<I', img, o)[0] + b0) & 0xffffffff)
REGS = [UC_ARM_REG_R0 + i for i in range(13)] + [UC_ARM_REG_SP, UC_ARM_REG_LR]

def new_uc():
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    # 번역 런타임의 경계 검사와 같은 범위만 연다. 실행은 이미지와 시험 코드, 복귀 지점만 허용한다.
    uc.mem_map(LOAD, 0x100000, UC_PROT_ALL)
    uc.mem_map(0x200000, MEMSZ - 0x200000, UC_PROT_READ | UC_PROT_WRITE)
    uc.mem_map(0x500000, 0x10000, UC_PROT_ALL)
    uc.mem_map(MAGIC, 0x1000, UC_PROT_ALL)
    return uc
UC = new_uc()

def inputs(seed):
    r = random.Random(seed)
    mem = bytearray(img)
    for o in range(ARENA, ARENA + 0x8000, 4):
        k = r.random()
        v = ARENA + r.randrange(0x7000) * 4 // 4 * 4 % 0x7000 if k < 0.5 else r.randrange(256) if k < 0.9 else r.getrandbits(32)
        struct.pack_into('<I', mem, o, v & 0xffffffff)
    regs = [0] * 16
    for i in range(4):
        regs[i] = (ARENA + r.randrange(0x1000) * 4) if r.random() < 0.6 else r.randrange(64)
    for i in range(4, 13):
        regs[i] = r.getrandbits(16)
    regs[13] = STACK - 0x100
    regs[14] = MAGIC | 1
    return regs, mem

def run_uc(addr, regs, mem, code=None, thumb=True):
    global UC
    # 0x500000에 함수마다 다른 코드를 올리므로 번역 캐시가 남지 않게 새 엔진을 쓴다.
    if code is not None:
        UC = new_uc()
    UC.mem_write(LOAD, bytes(mem[LOAD:MEMSZ]))
    for rn, v in zip(REGS, regs[:15]):
        UC.reg_write(rn, v)
    if code is not None:
        UC.mem_write(0x500000, code)
    try:
        UC.emu_start(addr | 1, MAGIC, count=2000000)
    except UcError:
        if code is not None:
            UC = new_uc()
        return None
    if UC.reg_read(UC_ARM_REG_PC) & ~1 != MAGIC:
        return None
    res = [UC.reg_read(r) for r in REGS], bytes(UC.mem_read(LOAD, MEMSZ - LOAD))
    if code is not None:
        UC = new_uc()
    return res

HDR = r'''
typedef unsigned char undefined1,byte,uchar,bool_,undefined; typedef unsigned short undefined2,ushort,word; typedef unsigned int undefined4,uint,dword,ulong_;
typedef unsigned long long undefined8,ulonglong; typedef long long longlong; typedef signed char sbyte; typedef void (code)();
#define bool unsigned char
#define true 1
#define false 0
#define ulong uint
#define long int
#define CONCAT44(a,b) (((ulonglong)(uint)(a)<<32)|(uint)(b))
#define CONCAT22(a,b) (((uint)(ushort)(a)<<16)|(ushort)(b))
#define CONCAT31(a,b) (((uint)(a)<<8)|(byte)(b))
#define CONCAT13(a,b) (((uint)(a)<<24)|((uint)(b)&0xffffff))
#define CONCAT11(a,b) (((ushort)(a)<<8)|(byte)(b))
#define SUB41(a,b) ((byte)((uint)(a)>>((b)*8)))
#define SUB42(a,b) ((ushort)((uint)(a)>>((b)*8)))
#define SUB84(a,b) ((uint)((ulonglong)(a)>>((b)*8)))
#define CARRY4(a,b) (((uint)(a)+(uint)(b))<(uint)(a))
#define SCARRY4(a,b) (((int)((uint)(a)+(uint)(b))<0)!=((int)(a)<0)&&((int)(a)<0)==((int)(b)<0))
#define SBORROW4(a,b) (((int)(a)<0)!=((int)(b)<0)&&((int)((uint)(a)-(uint)(b))<0)!=((int)(a)<0))
#define ZEXT14(a) ((uint)(byte)(a))
#define ZEXT24(a) ((uint)(ushort)(a))
#define SEXT14(a) ((int)(sbyte)(a))
#define SEXT24(a) ((int)(short)(a))
#define ZEXT48(a) ((ulonglong)(uint)(a))
#define SEXT48(a) ((longlong)(int)(a))
'''
funcs = {}
for m in re.finditer(r'// @([0-9a-f]+)\n(.*?)(?=// @[0-9a-f]+\n|\Z)', allc, re.S):
    funcs[int(m.group(1), 16) - LOAD] = m.group(2)

def compile_ghidra(addr, src):
    """Ghidra C를 ARMv4T Thumb로 컴파일. 전역과 다른 함수는 원본 주소로 묶는다. 성공 시 기계어와 진입 오프셋."""
    s = re.sub(r'\b_?(s_\w+_|PTR_\w*?_|DAT_|_DAT_)([0-9a-f]{8})\b', lambda m: f'(*(volatile uint*)0x{m.group(2)})', src)
    s = re.sub(r'\bFUN_([0-9a-f]{8})\b(?=\s*\()', lambda m: '((int(*)())0x%x)' % (int(m.group(1), 16) | 1) if int(m.group(1), 16) != addr + LOAD else m.group(0), s)
    s = re.sub(r'\bfunc_0x([0-9a-f]{8})\b', lambda m: '((int(*)())0x%x)' % (int(m.group(1), 16) | 1), s)
    with tempfile.TemporaryDirectory() as d:
        c = os.path.join(d, 'f.c'); o = os.path.join(d, 'f.o')
        open(c, 'w').write(HDR + s)
        p = subprocess.run(['clang', '--target=armv4t-none-eabi', '-mthumb', '-O1', '-w', '-fno-builtin', '-fsigned-char', '-std=gnu89', '-c', c, '-o', o], capture_output=True, text=True)
        if p.returncode:
            return None, (p.stderr.split('\n')[0] if p.stderr else 'err')
        elf = ELFFile(open(o, 'rb'))
        text = elf.get_section_by_name('.text').data()
        sym = next((x for x in elf.get_section_by_name('.symtab').iter_symbols() if x.name.startswith('FUN_') and x['st_info']['type'] == 'STT_FUNC'), None)
        rel = elf.get_section_by_name('.rel.text')
        if rel is not None and rel.num_relocations():
            names = {elf.get_section_by_name('.symtab').get_symbol(x['r_info_sym']).name for x in rel.iter_relocations()}
            return None, 'extern ' + ','.join(sorted(names))[:40]
        return (text, sym['st_value'] & ~1), None

candidates = [int(k, 16) for k, v in cm['roots'].items() if v['thumb'] and v.get('addrs') and int(k, 16) in funcs]
random.Random(7).shuffle(candidates)
sample = sorted(candidates[:SAMPLE])
res = {'n': len(sample), 'tr': {'match': 0, 'mismatch': 0, 'none': 0}, 'gh': {'compile_fail': 0, 'match': 0, 'mismatch': 0, 'none': 0}, 'tr_bad': [], 'gh_bad': [], 'gh_err': {}, 'tr_runs': 0, 'gh_runs': 0, 'fn_used': 0}
run = lib.run_fn; run.argtypes = [ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32), ctypes.c_char_p, ctypes.c_uint32]
for n, addr in enumerate(sample):
    a = addr + LOAD
    code, err = compile_ghidra(addr, funcs[addr])
    if code is None:
        res['gh']['compile_fail'] += 1
        k = re.sub(r'.*error: ', '', err)[:50]; res['gh_err'][k] = res['gh_err'].get(k, 0) + 1
    tr_state, gh_state = 'none', 'none'
    for seed in range(4):
        regs, mem = inputs(addr * 31 + seed)
        orig = run_uc(a, regs, mem)
        if orig is None:
            continue
        o_regs, o_mem = orig
        # 번역
        buf = ctypes.create_string_buffer(bytes(mem), len(mem))
        rr = (ctypes.c_uint32 * 16)(*regs)
        f = lib.run_fn(ctypes.c_uint32(a), rr, buf, ctypes.c_uint32(MEMSZ))
        res['tr_runs'] += 1
        # 호출 규약상 보존되는 값과 스택 밖 메모리만 비교한다(ARM 도우미를 C로 바꿔 임시 레지스터는 달라질 수 있다).
        keep = [0, 4, 5, 6, 7, 8, 9, 10, 11, 13]
        if f == 0 and all(rr[i] == o_regs[i] for i in keep) and buf.raw[LOAD:0x400000] == o_mem[:0x300000]:
            tr_state = 'match' if tr_state != 'mismatch' else tr_state
        else:
            tr_state = 'mismatch'
            res['tr_bad'].append((hex(addr), f, seed))
        # Ghidra
        if code is not None:
            tt, ent = code
            g = run_uc(0x500000 + ent, regs, mem, tt)
            res['gh_runs'] += 1
            ok = False
            if g is not None:
                gr, gm = g
                nov = gm[:0x300000] == o_mem[:0x300000]
                void = re.search(r'^\s*void\s+FUN_', funcs[addr], re.M) is not None
                ok = nov and (void or gr[0] == o_regs[0])
            if ok:
                gh_state = 'match' if gh_state != 'mismatch' else gh_state
            else:
                gh_state = 'mismatch'
                res['gh_bad'].append((hex(addr), seed, g is None))
    res['tr'][tr_state] += 1
    if code is not None:
        res['gh'][gh_state] += 1
    if n % 50 == 0:
        print(n, res['tr'], res['gh'], flush=True)
res['tr_bad'] = res['tr_bad'][:50]; res['gh_bad'] = res['gh_bad'][:50]
json.dump(res, open(OUT, 'w'), indent=1)
print(json.dumps({k: res[k] for k in ('n','tr','gh','gh_err','tr_runs','gh_runs')}, indent=1)); print(res['tr_bad'][:20])
