// 사용자가 고른 원본 파일(크로노스윙.zip 또는 그 안의 jar)에서 게임 데이터를 꺼내고, 빌드 때 만들던 파생물을 브라우저에서 만든다.
// - 글꼴: 게임 비트맵 폰트(ft/) → TTF. work/tools/hnf2ttf.py와 같은 규칙이다.
// - 소리: snd/의 SMAF → 이벤트 파일. work/tools/smaf2ev(wasm)를 부른다.
// 원본에서 나온 내용은 이 파일에 없다. 브라우저와 node(시험)에서 함께 쓴다.
(function (root) {
'use strict';

const CLIENT_SHA256 = '46d3abafdbb1c7e9f11859b3166ddd23a4bd33c5558d74ac6a242d447c34f7e8';

// ---- zip ----

const u16 = (b, o) => b[o] | b[o + 1] << 8;
const u32 = (b, o) => (b[o] | b[o + 1] << 8 | b[o + 2] << 16 | b[o + 3] << 24) >>> 0;

async function inflate(bytes) {
  const stream = new Blob([bytes]).stream().pipeThrough(new DecompressionStream('deflate-raw'));
  return new Uint8Array(await new Response(stream).arrayBuffer());
}

// 이름 → 내용. 폴더 항목은 뺀다.
async function unzip(bytes) {
  let end = bytes.length - 22;
  while (end >= 0 && u32(bytes, end) !== 0x06054b50) end--;
  if (end < 0) throw new Error('zip 파일이 아닙니다.');
  const count = u16(bytes, end + 10);
  let at = u32(bytes, end + 16);
  const files = new Map();
  for (let i = 0; i < count; i++) {
    if (u32(bytes, at) !== 0x02014b50) throw new Error('zip 목록이 깨졌습니다.');
    const method = u16(bytes, at + 10), packed = u32(bytes, at + 20);
    const nameLen = u16(bytes, at + 28), extraLen = u16(bytes, at + 30), commentLen = u16(bytes, at + 32);
    const local = u32(bytes, at + 42);
    const name = new TextDecoder().decode(bytes.subarray(at + 46, at + 46 + nameLen));
    at += 46 + nameLen + extraLen + commentLen;
    if (name.endsWith('/')) continue;
    const start = local + 30 + u16(bytes, local + 26) + u16(bytes, local + 28);
    const body = bytes.subarray(start, start + packed);
    if (method === 0) files.set(name, body.slice());
    else if (method === 8) files.set(name, await inflate(body));
    else throw new Error('풀 수 없는 압축 방식: ' + name);
  }
  return files;
}

async function sha256(bytes) {
  const d = new Uint8Array(await crypto.subtle.digest('SHA-256', bytes));
  return Array.from(d, x => x.toString(16).padStart(2, '0')).join('');
}

// 원본 zip이면 안의 jar를 다시 푼다. 실행 이미지는 client.bin<BSS 크기>로 들어 있고 호스트는 client.bin으로 읽는다.
// zip에는 단말기 메뉴용 아이콘(big.icon, PNG)이 jar 옆에 있다. 있으면 돌려주는 Map의 icon에 담는다(jar만 고르면 없다).
async function extractGame(bytes) {
  let files = await unzip(bytes);
  const icon = files.get('big.icon');
  const jar = [...files.keys()].find(n => n.toLowerCase().endsWith('.jar'));
  if (jar) files = await unzip(files.get(jar));
  const image = [...files.keys()].find(n => /^client\.bin\d+$/.test(n));
  if (!image) throw new Error('게임 실행 파일(client.bin)이 들어 있지 않습니다.');
  const hash = await sha256(files.get(image));
  if (hash !== CLIENT_SHA256) throw new Error('지원하는 판본이 아닙니다(KTF 판만 됩니다).');
  const game = new Map();
  for (const [name, data] of files) if (!name.startsWith('META-INF/') && name !== image) game.set(name, data);
  game.set('client.bin', files.get(image));
  game.icon = icon || null;
  return game;
}

// ---- 글꼴 ----
// 1 em = 게임 화면 11픽셀. 글자 폭은 가변이다(오른쪽 끝 + 2, 공백 4).
const UNIT = 64, ASCENT = 10, DESCENT = 1, SPACE_ADVANCE = 4, KOR_H = 14, ENG_H = 8;
const SQUARE_ROWS = [0, 0, 0xf8, 0x88, 0x88, 0x88, 0xf8, 0];
const TRIANGLE_ROWS = [0, 0, 0x20, 0x50, 0x50, 0x88, 0xf8, 0];

function rowsKor(d, index) {
  const base = 9 + index * KOR_H * 2, rows = [];
  for (let y = 0; y < KOR_H; y++) rows.push(d[base + y * 2 + 1] << 8 | d[base + y * 2]);
  return rows;
}

// 조합형 코드 → 초성·중성·종성 조각을 겹친다(원본 0x12c980 부근과 같은 규칙).
function compose(code, kor, hancode, t) {
  const cho = hancode[code >> 10 & 0x1f], jung = hancode[31 + (code >> 5 & 0x1f)], jong = hancode[62 + (code & 0x1f)];
  const choSet = t[(jong ? 22 : 0) + jung], jungSet = t[44 + (jong ? 20 : 0) + cho], jongSet = t[84 + jung];
  let rows = rowsKor(kor, choSet * 20 + cho);
  if (jung) rows = rows.map((r, i) => r | rowsKor(kor, 160 + jungSet * 22 + jung)[i]);
  if (jong) rows = rows.map((r, i) => r | rowsKor(kor, 248 + jongSet * 28 + jong)[i]);
  return rows;
}

function advance(rows, width) {
  let max = -1;
  for (const r of rows) for (let x = 0; x < width; x++) if (r >> (width - 1 - x) & 1 && x > max) max = x;
  return max >= 0 ? max + 2 : SPACE_ADVANCE;
}

// 켜진 픽셀의 가로 줄마다 사각형 하나. 점 순서는 hnf2ttf.py와 같다.
function outline(rows, width, top) {
  const contours = [];
  rows.forEach((row, y) => {
    let x = 0;
    while (x < width) {
      if (row >> (width - 1 - x) & 1) {
        const start = x;
        while (x < width && row >> (width - 1 - x) & 1) x++;
        const y0 = (ASCENT - top - y) * UNIT, y1 = (ASCENT - top - y - 1) * UNIT;
        contours.push([[start * UNIT, y1], [start * UNIT, y0], [x * UNIT, y0], [x * UNIT, y1]]);
      } else x++;
    }
  });
  return contours;
}

class Writer {
  constructor() { this.b = []; }
  u8(v) { this.b.push(v & 255); return this; }
  u16(v) { this.b.push(v >> 8 & 255, v & 255); return this; }
  u32(v) { this.b.push(v >>> 24 & 255, v >>> 16 & 255, v >>> 8 & 255, v & 255); return this; }
  tag(s) { for (const ch of s) this.b.push(ch.charCodeAt(0)); return this; }
  pad() { while (this.b.length & 3) this.b.push(0); return this; }
  get length() { return this.b.length; }
}

function glyphRecord(contours) {
  const w = new Writer();
  if (!contours.length) return { bytes: [], xMin: 0, yMin: 0, xMax: 0, yMax: 0 };
  const pts = contours.flat();
  const xs = pts.map(p => p[0]), ys = pts.map(p => p[1]);
  const box = { xMin: Math.min(...xs), yMin: Math.min(...ys), xMax: Math.max(...xs), yMax: Math.max(...ys) };
  w.u16(contours.length).u16(box.xMin).u16(box.yMin).u16(box.xMax).u16(box.yMax);
  let end = -1;
  for (const c of contours) { end += c.length; w.u16(end); }
  w.u16(0);
  for (let i = 0; i < pts.length; i++) w.u8(1); // 모두 곡선 위의 점, 좌표는 16비트 차이
  let px = 0, py = 0;
  for (const p of pts) { w.u16(p[0] - px); px = p[0]; }
  for (const p of pts) { w.u16(p[1] - py); py = p[1]; }
  w.pad();
  return { bytes: w.b, ...box };
}

function cmapTable(codes) { // codes: [[유니코드, 글리프 번호]] 오름차순, BMP만
  const segs = [];
  for (const [code, gid] of codes) {
    const last = segs[segs.length - 1];
    if (last && code === last.end + 1 && gid === last.gid + (code - last.start)) last.end = code;
    else segs.push({ start: code, end: code, gid });
  }
  segs.push({ start: 0xffff, end: 0xffff, gid: 1 }); // 끝 표시: 0xffff + delta가 0이 되게
  const n = segs.length, pow = 2 ** Math.floor(Math.log2(n));
  const sub = new Writer();
  sub.u16(4).u16(16 + n * 8).u16(0).u16(n * 2).u16(pow * 2).u16(Math.log2(pow)).u16(n * 2 - pow * 2);
  for (const s of segs) sub.u16(s.end);
  sub.u16(0);
  for (const s of segs) sub.u16(s.start);
  for (const s of segs) sub.u16(s.gid - s.start);
  for (let i = 0; i < n; i++) sub.u16(0);
  const w = new Writer();
  w.u16(0).u16(1).u16(3).u16(1).u32(12);
  w.b.push(...sub.b);
  return w.b;
}

function nameTable(entries) { // [[nameID, 문자열]]
  const w = new Writer(), strings = [];
  w.u16(0).u16(entries.length).u16(6 + entries.length * 12);
  let off = 0;
  for (const [id, text] of entries) {
    w.u16(3).u16(1).u16(0x409).u16(id).u16(text.length * 2).u16(off);
    for (const ch of text) strings.push(0, ch.charCodeAt(0));
    off += text.length * 2;
  }
  w.b.push(...strings);
  return w.b;
}

function buildFont(game) {
  const read = n => { const f = game.get('ft/' + n); if (!f) throw new Error('글꼴 파일이 없습니다: ' + n); return f; };
  const kor = read('kor9x14'), eng = read('eng5x14'), hancode = read('hancode'), t = read('type844'), info = read('haninfo');
  const glyphs = [{ contours: [], advance: SPACE_ADVANCE }]; // 0번은 .notdef
  const codes = [];
  const add = (ch, rows, width) => { codes.push([ch, glyphs.length]); glyphs.push({ contours: outline(rows, width, 1), advance: advance(rows, width) }); };
  const engRows = ch => Array.from(eng.subarray(6 + (ch - 0x20) * ENG_H, 6 + (ch - 0x1f) * ENG_H));
  for (let ch = 0x20; ch < 0x20 + 95; ch++) add(ch, engRows(ch), 8);
  // 패드 표시용 기호. 비트맵은 host/gamepad.c와 같고 ○은 영문 O를 쓴다.
  add(0x25a1, SQUARE_ROWS, 8); add(0x25b3, TRIANGLE_ROWS, 8); add(0x25cb, engRows(0x4f), 8);
  const euckr = new TextDecoder('euc-kr');
  for (let i = 0; i < info.length >> 1; i++) { // 한글 2350자: haninfo는 KS X 1001 순서의 조합형 코드
    const ch = euckr.decode(new Uint8Array([0xb0 + Math.floor(i / 94), 0xa1 + i % 94])).codePointAt(0);
    add(ch, compose(info[i * 2] << 8 | info[i * 2 + 1], kor, hancode, t), 16);
  }
  codes.sort((a, b) => a[0] - b[0]);

  const glyf = [], loca = new Writer(), hmtx = new Writer();
  let xMin = 0, yMin = 0, xMax = 0, yMax = 0, maxPoints = 0, maxContours = 0, advMax = 0;
  for (const g of glyphs) {
    const rec = glyphRecord(g.contours);
    loca.u32(glyf.length);
    for (const b of rec.bytes) glyf.push(b);
    hmtx.u16(g.advance * UNIT).u16(rec.xMin);
    xMin = Math.min(xMin, rec.xMin); yMin = Math.min(yMin, rec.yMin); xMax = Math.max(xMax, rec.xMax); yMax = Math.max(yMax, rec.yMax);
    maxPoints = Math.max(maxPoints, g.contours.length * 4); maxContours = Math.max(maxContours, g.contours.length);
    advMax = Math.max(advMax, g.advance * UNIT);
  }
  loca.u32(glyf.length);
  const em = (ASCENT + DESCENT) * UNIT;
  const head = new Writer();
  head.u32(0x00010000).u32(0x00010000).u32(0).u32(0x5f0f3cf5).u16(3).u16(em).u32(0).u32(0).u32(0).u32(0)
    .u16(xMin).u16(yMin).u16(xMax).u16(yMax).u16(0).u16(8).u16(2).u16(1).u16(0);
  const hhea = new Writer();
  hhea.u32(0x00010000).u16(ASCENT * UNIT).u16(-DESCENT * UNIT).u16(0).u16(advMax).u16(xMin).u16(0).u16(xMax)
    .u16(1).u16(0).u16(0).u16(0).u16(0).u16(0).u16(0).u16(0).u16(glyphs.length);
  const maxp = new Writer();
  maxp.u32(0x00010000).u16(glyphs.length).u16(maxPoints).u16(maxContours).u16(0).u16(0).u16(1).u16(0).u16(0).u16(0).u16(0).u16(0).u16(0).u16(0).u16(0);
  const post = new Writer();
  post.u32(0x00030000).u32(0).u16(0).u16(0).u32(0).u32(0).u32(0).u32(0).u32(0);
  const tables = [
    ['cmap', cmapTable(codes)], ['glyf', glyf], ['head', head.b], ['hhea', hhea.b], ['hmtx', hmtx.b], ['loca', loca.b], ['maxp', maxp.b],
    ['name', nameTable([[1, 'ChronosGame'], [2, 'Regular'], [4, 'ChronosGame Regular'], [6, 'ChronosGame-Regular']])], ['post', post.b],
  ];
  const n = tables.length, pow = 2 ** Math.floor(Math.log2(n));
  const out = new Writer();
  out.u32(0x00010000).u16(n).u16(pow * 16).u16(Math.log2(pow)).u16(n * 16 - pow * 16);
  let off = 12 + n * 16;
  for (const [tag, data] of tables) { out.tag(tag).u32(0).u32(off).u32(data.length); off += (data.length + 3) & ~3; }
  for (const [, data] of tables) { for (let i = 0; i < data.length; i += 65536) out.b.push(...data.slice(i, i + 65536)); out.pad(); }
  return new Uint8Array(out.b);
}

// ---- 소리 ----

// "CWEV", 파일 수, 이름순으로 파일마다 기록 하나.
async function buildEvents(game, wasmBytes) {
  const { instance } = await WebAssembly.instantiate(wasmBytes, {});
  const ex = instance.exports;
  const names = [...game.keys()].filter(n => n.startsWith('snd/')).sort();
  for (const name of names) {
    const raw = game.get(name);
    const at = ex.ev_alloc(raw.length);
    new Uint8Array(ex.memory.buffer, at, raw.length).set(raw);
    ex.ev_add();
  }
  const body = new Uint8Array(ex.memory.buffer, ex.ev_ptr(), ex.ev_len());
  const out = new Uint8Array(8 + body.length);
  out.set([0x43, 0x57, 0x45, 0x56]);
  new DataView(out.buffer).setUint32(4, names.length, true);
  out.set(body, 8);
  return out;
}

root.CWGameData = { unzip, extractGame, buildFont, buildEvents, sha256 };
if (typeof module !== 'undefined') module.exports = root.CWGameData;
})(typeof globalThis !== 'undefined' ? globalThis : this);
