#!/usr/bin/env python3
"""sheet.py <폴더> <접두사> <출력.png> [개수]: 덤프한 PPM을 한 장으로 모은다."""
import glob, re, sys
from PIL import Image
d, pre, out = sys.argv[1:4]
n = int(sys.argv[4]) if len(sys.argv) > 4 else 12
fs = sorted(glob.glob(f'{d}/{pre}*.ppm'), key=lambda f: int(re.findall(r'(\d+)\.ppm', f)[0]))
pick = [fs[int(i * (len(fs) - 1) / (n - 1))] for i in range(n)] if len(fs) >= n else fs
c = Image.new('RGB', (240 * 6, 320 * ((len(pick) + 5) // 6)))
for k, f in enumerate(pick):
    c.paste(Image.open(f).convert('RGB'), ((k % 6) * 240, (k // 6) * 320))
c.save(out)
print(len(fs))
