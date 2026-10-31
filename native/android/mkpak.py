# mkpak.py <빌드 폴더> <출력>: 게임 데이터, game.ev, 사운드폰트를 앱이 첫 실행 때 푸는 단일 파일로 묶는다.
import os, struct, sys
build, out = sys.argv[1], sys.argv[2]
items = [('game.ev', f'{build}/game.ev'), ('1mgm.sf2', f'{build}/1mgm.sf2')]
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
