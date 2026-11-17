# mkpak.py <빌드 폴더> <출력> <ui 폴더>: 게임 데이터, game.ev, 사운드폰트, UI를 앱이 첫 실행 때 푸는 단일 파일로 묶는다.
# 묶음에는 원본 게임 데이터가 들어간다. build/에만 두며 저장소와 공개 배포에 올리지 않는다.
import os, struct, sys
build, out, ui = sys.argv[1], sys.argv[2], sys.argv[3]
items = [('game.ev', f'{build}/game.ev'), ('1mgm.sf2', f'{build}/1mgm.sf2')]
items += [(f'ui/{n}', f'{ui}/{n}') for n in sorted(os.listdir(ui)) if not n.startswith('.')]
game = f'{build}/game'
for d, _, names in sorted(os.walk(game)):
    rel = os.path.relpath(d, game)
    if rel.startswith('META-INF'):
        continue
    for n in sorted(names):
        if n != 'client.bin474328':
            items.append((os.path.normpath(f'game/{rel}/{n}'), f'{d}/{n}'))
with open(out, 'wb') as w:
    w.write(b'CWPK' + struct.pack('<I', len(items)))
    for name, path in items:
        data = open(path, 'rb').read()
        raw = name.encode()
        w.write(struct.pack('<H', len(raw)) + raw + struct.pack('<I', len(data)) + data)
print(len(items), 'files', os.path.getsize(out), 'bytes')
