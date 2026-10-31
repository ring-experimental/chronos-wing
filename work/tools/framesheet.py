# framesheet.py <프레임폴더> <out.png> [칸수]: log.txt 전체에서 고르게 뽑은 프레임을 절반 크기 시트로 만든다.
import sys, zlib, struct
d, out = sys.argv[1], sys.argv[2]; n = int(sys.argv[3]) if len(sys.argv) > 3 else 24
rows = [l.split() for l in open(d + '/log.txt')]
idx = [i * (len(rows) - 1) // (n - 1) for i in range(n)]
cols = 8; lines = (n + cols - 1) // cols
W, H = cols * 120, lines * 160
buf = bytearray(W * H * 3)
for k, i in enumerate(idx):
    f = open(f'{d}/{rows[i][1]}.raw', 'rb').read()
    ox, oy = k % cols * 120, k // cols * 160
    for y in range(160):
        for x in range(120):
            s = (y * 2 * 240 + x * 2) * 2; p = f[s] | f[s + 1] << 8; t = ((oy + y) * W + ox + x) * 3
            buf[t:t + 3] = bytes(((p >> 11 & 31) * 8, (p >> 5 & 63) * 4, (p & 31) * 8))
raw = b''.join(b'\0' + bytes(buf[y * W * 3:(y + 1) * W * 3]) for y in range(H))
chunk = lambda t, b: struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b))
open(out, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
print([rows[i][0] for i in idx])
