# framediff.py <출력폴더>: 연속 중복을 뺀 두 프레임 열을 정렬해 처음 다른 프레임과 픽셀 차이를 보여 준다.
import sys, difflib, os
out = sys.argv[1]
def load(side):
    rows = [l.split() for l in open(f'{out}/{side}/log.txt')]
    seq = []
    for r in rows:
        if not seq or seq[-1][1] != r[1]: seq.append((int(r[0]), r[1]))
    print(side, 'frames', len(rows), 'runs', len(seq), 'distinct', len({r[1] for r in rows}), 'size', {(r[2], r[3]) for r in rows})
    return seq
w, n = load('wie'), load('nat')
ws, ns = {h for _, h in w}, {h for _, h in n}
print('distinct common', len(ws & ns), 'wie only', len(ws - ns), 'nat only', len(ns - ws))
m = difflib.SequenceMatcher(None, [h for _, h in w], [h for _, h in n], autojunk=False)
ops = [o for o in m.get_opcodes()]
print('aligned equal', sum(o[2]-o[1] for o in ops if o[0] == 'equal'))
shown = 0
for tag, a, b, c, d in ops:
    if tag == 'equal': continue
    print(tag, 'wie', a, b, 'seq', w[a][0] if a < len(w) else '-', 'nat', c, d, 'seq', n[c][0] if c < len(n) else '-')
    if tag == 'replace' and shown < 8:
        pw, pn = f'{out}/wie/{w[a][1]}.raw', f'{out}/nat/{n[c][1]}.raw'
        A, B = open(pw,'rb').read(), open(pn,'rb').read()
        if len(A) == len(B):
            diff = [i//2 for i in range(0, len(A), 2) if A[i:i+2] != B[i:i+2]]
            if diff:
                xs = [i % 240 for i in diff]; ys = [i // 240 for i in diff]
                print(f'   {w[a][1]} vs {n[c][1]}: px {len(diff)} bbox x{min(xs)}-{max(xs)} y{min(ys)}-{max(ys)} first {A[diff[0]*2:diff[0]*2+2].hex()} vs {B[diff[0]*2:diff[0]*2+2].hex()}')
        shown += 1
