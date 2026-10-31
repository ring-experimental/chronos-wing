# keys2frames.py <토큰...>: "ok down wait2" 같은 키 열을 framecmp용 프레임 스크립트로 바꾼다. START, STEP(프레임) 환경변수.
import os, sys
f = int(os.environ.get('START', 300)); step = int(os.environ.get('STEP', 30))
for t in sys.argv[1:]:
    if t.startswith('wait'): f += int(t[4:]) * 20; continue
    print(f, 'key', t); print(f + 3, 'up', t); f += step
print(f + 600, 'quit')
