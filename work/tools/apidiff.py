#!/usr/bin/env python3
"""두 API 호출 기록(id lr 줄)을 비교해 처음 갈라지는 곳을 보여 준다. apidiff.py <wie> <native> [건너뛸 id...]"""
import sys
skip = {int(x, 16) for x in sys.argv[3:]}
def load(p):
    return [(int(a, 16), int(b, 16)) for a, b in (l.split() for l in open(p)) if int(a, 16) not in skip]
a, b = load(sys.argv[1]), load(sys.argv[2])
n = min(len(a), len(b))
i = 0
while i < n and a[i] == b[i]:
    i += 1
print('len', len(a), len(b), 'same prefix', i)
for k in range(max(0, i - 6), min(i + 12, n)):
    print(k, f'{a[k][0]:#x} {a[k][1]:#x}', '|', f'{b[k][0]:#x} {b[k][1]:#x}', '' if a[k] == b[k] else '<<')
