# mkskin.py <출력 폴더>: 터치 패드 스킨을 만든다. 색은 게임 메뉴(남색, 파랑, 금색), 톱니 테는 타이틀 로고에서 가져왔다. 게임 원본 그림은 쓰지 않는다.
import math, sys
from PIL import Image, ImageDraw
out = sys.argv[1]
SS = 4
NAVY = (24, 60, 112); BLUE = (48, 92, 168); DEEP = (10, 22, 46); GOLD = (248, 196, 0); GOLD_D = (150, 104, 20); CREAM = (250, 240, 205)
EMBER = (150, 62, 24); EMBER_L = (214, 110, 40)

def canvas(size): return Image.new('RGBA', (size * SS, size * SS), (0, 0, 0, 0))
def done(img, name, size): img.resize((size, size), Image.LANCZOS).save(f'{out}/{name}.png')

def disc(d, c, r, fill, outline=None, width=0):
    d.ellipse((c - r, c - r, c + r, c + r), fill=fill, outline=outline, width=width)

def gear_ring(d, c, r, teeth, depth, fill):
    pts = []
    for i in range(teeth * 4):
        a = 2 * math.pi * i / (teeth * 4)
        rr = r + depth if i % 4 in (1, 2) else r
        pts.append((c + rr * math.cos(a), c + rr * math.sin(a)))
    d.polygon(pts, fill=fill)

def medal(name, size, top, bottom, teeth=0, glow=False):
    img = canvas(size); d = ImageDraw.Draw(img); s = size * SS; c = s / 2; r = s * 0.46
    if glow: disc(d, c, s * 0.5 - 1, (255, 225, 120, 110))
    if teeth: gear_ring(d, c, r * 0.97, teeth, s * 0.022, (*GOLD_D, 255))
    disc(d, c, r, (*DEEP, 255))
    disc(d, c, r * 0.95, (*GOLD, 255))
    disc(d, c, r * 0.86, (*DEEP, 255))
    for y in range(int(c - r * 0.82), int(c + r * 0.82)):   # 위가 밝은 금속 면
        t = (y - (c - r * 0.82)) / (r * 1.64)
        col = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        w = math.sqrt(max(0, (r * 0.82) ** 2 - (y - c) ** 2))
        d.line((c - w, y, c + w, y), fill=(*col, 245))
    d.arc((c - r * 0.72, c - r * 0.74, c + r * 0.72, c + r * 0.7), 200, 340, fill=(255, 255, 255, 110), width=max(2, int(s * 0.012)))
    done(img, name, size)

def dial():
    size = 384; img = canvas(size); d = ImageDraw.Draw(img); s = size * SS; c = s / 2; r = s * 0.45
    gear_ring(d, c, r, 24, s * 0.022, (*GOLD_D, 235))
    disc(d, c, r, (*DEEP, 235))
    disc(d, c, r * 0.965, (*GOLD, 255))
    disc(d, c, r * 0.92, (*DEEP, 170))
    disc(d, c, r * 0.89, (*NAVY, 120))
    disc(d, c, r * 0.24, (*DEEP, 200))
    disc(d, c, r * 0.21, (*GOLD_D, 230))
    disc(d, c, r * 0.16, (*BLUE, 230))
    done(img, 'dial', size)

def arrow(name, angle, on):
    size = 128; img = canvas(size); d = ImageDraw.Draw(img); s = size * SS; c = s / 2
    def rot(x, y):
        a = math.radians(angle); return (c + x * math.cos(a) - y * math.sin(a), c + x * math.sin(a) + y * math.cos(a))
    if on: disc(d, c, s * 0.44, (*GOLD, 190))
    h = s * 0.25
    d.polygon([rot(0, -h), rot(h * 0.95, h * 0.62), rot(-h * 0.95, h * 0.62)], fill=(*DEEP, 255))
    h2 = h * 0.72
    d.polygon([rot(0, -h2), rot(h2 * 0.95, h2 * 0.62), rot(-h2 * 0.95, h2 * 0.62)], fill=(*(DEEP if on else CREAM), 255))
    done(img, name, size)

def pill(name, top, bottom, glow=False):
    w, h = 256, 128
    img = Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0)); d = ImageDraw.Draw(img); W, H = w * SS, h * SS; m = H * 0.05
    if glow: d.rounded_rectangle((0, 0, W - 1, H - 1), H / 2, fill=(255, 225, 120, 110))
    d.rounded_rectangle((m, m, W - m, H - m), (H - 2 * m) / 2, fill=(*DEEP, 255))
    d.rounded_rectangle((m * 1.7, m * 1.7, W - m * 1.7, H - m * 1.7), (H - 3.4 * m) / 2, fill=(*GOLD, 255))
    k = m * 2.7
    d.rounded_rectangle((k, k, W - k, H - k), (H - 2 * k) / 2, fill=(*top, 245))
    img.resize((w, h), Image.LANCZOS).save(f'{out}/{name}.png')

def icon(name, draw):
    size = 96; img = canvas(size); d = ImageDraw.Draw(img); s = size * SS
    draw(d, s); done(img, name, size)

def bag(d, s):  # 끈으로 묶은 주머니
    d.ellipse((s*.18, s*.34, s*.82, s*.9), fill=CREAM, outline=DEEP, width=int(s*.025))
    d.polygon([(s*.36,s*.12),(s*.64,s*.12),(s*.57,s*.36),(s*.43,s*.36)], fill=CREAM, outline=DEEP)
    d.rectangle((s*.38, s*.33, s*.62, s*.39), fill=GOLD_D)

def map_icon(d, s):
    d.polygon([(s*.14,s*.26),(s*.38,s*.16),(s*.62,s*.26),(s*.86,s*.16),(s*.86,s*.74),(s*.62,s*.84),(s*.38,s*.74),(s*.14,s*.84)], fill=CREAM, outline=DEEP)
    d.line((s*.38,s*.16,s*.38,s*.74), fill=GOLD_D, width=int(s*.035)); d.line((s*.62,s*.26,s*.62,s*.84), fill=GOLD_D, width=int(s*.035))

def save_icon(d, s):  # 플로피 디스크
    d.polygon([(s*.16,s*.14),(s*.72,s*.14),(s*.86,s*.28),(s*.86,s*.86),(s*.16,s*.86)], fill=CREAM, outline=DEEP)
    d.rectangle((s*.3, s*.14, s*.66, s*.38), fill=GOLD_D, outline=DEEP)
    d.rectangle((s*.52, s*.19, s*.6, s*.33), fill=CREAM)
    d.rectangle((s*.28, s*.54, s*.74, s*.86), fill=NAVY, outline=DEEP)

def menu_icon(d, s):
    for y in (.28, .5, .72):
        d.rounded_rectangle((s*.18, s*(y-.06), s*.82, s*(y+.06)), s*.05, fill=CREAM, outline=DEEP, width=int(s*.012))

def gear_icon(d, s):
    gear_ring(d, s/2, s*.27, 8, s*.09, CREAM)
    disc(d, s/2, s*.13, (*DEEP, 255))

dial()
medal('orb', 256, BLUE, NAVY, teeth=20)
medal('orb_on', 256, (120, 170, 240), BLUE, teeth=20, glow=True)
medal('skill', 192, EMBER_L, EMBER, teeth=14)
medal('skill_on', 192, (255, 180, 90), EMBER_L, teeth=14, glow=True)
medal('small', 128, BLUE, NAVY)
medal('small_on', 128, (120, 170, 240), BLUE, glow=True)
pill('pill', NAVY, NAVY)
pill('pill_on', BLUE, BLUE, glow=True)
for n, a in (('up', 0), ('right', 90), ('down', 180), ('left', 270)):
    arrow(f'a_{n}', a, False); arrow(f'a_{n}_on', a, True)
icon('i_bag', bag); icon('i_map', map_icon); icon('i_menu', menu_icon); icon('i_gear', gear_icon); icon('i_save', save_icon)
