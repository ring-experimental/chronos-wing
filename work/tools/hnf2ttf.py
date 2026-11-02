# hnf2ttf.py <게임 데이터 폴더> <출력.ttf>: 게임의 비트맵 폰트(ft/)를 픽셀 모양 그대로의 TTF로 바꾼다. 결과는 원본 파생물이라 build/에만 둔다.
# 형식: "HNF", 종류(0 한글, 1 영문), 줄당 폭(비트), 높이. 한글은 머리글 9바이트 뒤에 초성 8벌×20, 중성 4벌×22, 종성 4벌×28 조각이 있고,
# 한 줄은 [오른쪽 8픽셀, 왼쪽 8픽셀] 2바이트다.
# 글자 본체는 kor9x14, eng5x14다. kor11x16, eng7x10은 본체를 감싸는 외곽선 마스크라 쓰지 않는다(외곽선은 RmlUi font-effect로 낸다).
# 조합 규칙은 원본 0x12c980 부근과 같다.
import sys
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

# 1 em = 게임 화면 11픽셀. 글자 폭은 가변이다(오른쪽 끝 + 2, 공백 4). 게임 화면의 글자 위치에서 잰 값이다.
UNIT, ASCENT, DESCENT = 64, 10, 1
SPACE_ADVANCE = 4
KOR_H, ENG_H = 14, 8
SQUARE_ROWS = [0, 0, 0xf8, 0x88, 0x88, 0x88, 0xf8, 0]
TRIANGLE_ROWS = [0, 0, 0x20, 0x50, 0x50, 0x88, 0xf8, 0]


def rows_kor(d, index):
    size = KOR_H * 2
    base = 9 + index * size
    return [d[base + y * 2 + 1] << 8 | d[base + y * 2] for y in range(KOR_H)]


def compose(code, kor, hancode, t):
    cho = hancode[code >> 10 & 0x1f]
    jung = hancode[31 + (code >> 5 & 0x1f)]
    jong = hancode[62 + (code & 0x1f)]
    has_jong = jong != 0
    cho_set = t[(22 if has_jong else 0) + jung]
    jung_set = t[44 + (20 if has_jong else 0) + cho]
    jong_set = t[84 + jung]
    rows = rows_kor(kor, cho_set * 20 + cho)
    if jung:
        rows = [a | b for a, b in zip(rows, rows_kor(kor, 160 + jung_set * 22 + jung))]
    if jong:
        rows = [a | b for a, b in zip(rows, rows_kor(kor, 248 + jong_set * 28 + jong))]
    return rows


def advance(rows, width):
    ink = [x for r in rows for x in range(width) if r >> (width - 1 - x) & 1]
    return max(ink) + 2 if ink else SPACE_ADVANCE


def outline(rows, width, top):
    pen = TTGlyphPen(None)
    for y, row in enumerate(rows):
        x = 0
        while x < width:
            if row >> (width - 1 - x) & 1:
                start = x
                while x < width and row >> (width - 1 - x) & 1:
                    x += 1
                y0, y1 = (ASCENT - top - y) * UNIT, (ASCENT - top - y - 1) * UNIT
                pen.moveTo((start * UNIT, y1)); pen.lineTo((start * UNIT, y0)); pen.lineTo((x * UNIT, y0)); pen.lineTo((x * UNIT, y1)); pen.closePath()
            else:
                x += 1
    return pen.glyph()


def main(game, out):
    read = lambda n: open(f'{game}/ft/{n}', 'rb').read()
    kor, eng, hancode, t, info = read('kor9x14'), read('eng5x14'), read('hancode'), read('type844'), read('haninfo')
    glyphs = {'.notdef': TTGlyphPen(None).glyph()}
    cmap, metrics = {}, {'.notdef': (SPACE_ADVANCE * UNIT, 0)}
    for i in range(95):  # 영문: 머리글 6바이트 뒤 0x20부터 95자
        ch = 0x20 + i
        g = eng[6 + i * ENG_H:6 + (i + 1) * ENG_H]
        name = f'u{ch:04X}'
        glyphs[name] = outline(list(g), 8, 1)
        cmap[ch] = name
        metrics[name] = (advance(list(g), 8) * UNIT, 0)
    # 패드 표시용 기호. 비트맵은 host/gamepad.c와 같고 ○은 영문 O를 쓴다.
    for ch, rows in ((0x25A1, SQUARE_ROWS), (0x25B3, TRIANGLE_ROWS), (0x25CB, list(eng[6 + (ord('O') - 0x20) * ENG_H:6 + (ord('O') - 0x1f) * ENG_H]))):
        name = f'u{ch:04X}'
        glyphs[name] = outline(rows, 8, 1)
        cmap[ch] = name
        metrics[name] = (advance(rows, 8) * UNIT, 0)
    for i in range(len(info) // 2):  # 한글 2350자: haninfo는 KS X 1001 순서의 조합형 코드
        hi, lo = 0xb0 + i // 94, 0xa1 + i % 94
        ch = ord(bytes((hi, lo)).decode('euc-kr'))
        name = f'u{ch:04X}'
        rows = compose(info[i * 2] << 8 | info[i * 2 + 1], kor, hancode, t)
        glyphs[name] = outline(rows, 16, 1)
        cmap[ch] = name
        metrics[name] = (advance(rows, 16) * UNIT, 0)
    order = list(glyphs)
    fb = FontBuilder((ASCENT + DESCENT) * UNIT, isTTF=True)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap(cmap)
    fb.setupGlyf(glyphs)
    fb.setupHorizontalMetrics({n: (metrics[n][0], (glyphs[n].xMin if glyphs[n].numberOfContours else 0)) for n in order})
    fb.setupHorizontalHeader(ascent=ASCENT * UNIT, descent=-DESCENT * UNIT)
    fb.setupNameTable({'familyName': 'ChronosGame', 'styleName': 'Regular'})
    fb.setupOS2(sTypoAscender=ASCENT * UNIT, sTypoDescender=-DESCENT * UNIT, usWinAscent=ASCENT * UNIT, usWinDescent=DESCENT * UNIT)
    fb.setupPost()
    fb.save(out)
    print(len(cmap), 'chars')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
