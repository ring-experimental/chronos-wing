// 페이지 쪽 진행: 저장소 연결, 원본 파일 가져오기, 게임 시작, 세이브 파일 주고받기.
// 저장소(/persist, IndexedDB): game/ 원본에서 푼 데이터, game.ev와 ChronosGame.ttf 파생물, save/ 세이브, ui.ini 설정.
'use strict';
const $ = id => document.getElementById(id);
const PERSIST = '/persist', DATA_VERSION = '1', IMPORT_PATH = '/tmp/import.cwsave';
let store;

function status(text, error) {
  $('status').textContent = text;
  $('status').classList.toggle('error', !!error);
}

var Module = {
  noInitialRun: true,
  canvas: $('canvas'),
  printErr: text => console.warn(text),
  onRuntimeInitialized: boot,
  onAbort: what => { $('setup').hidden = false; $('need').hidden = $('ready').hidden = true; status('오류로 멈췄습니다: ' + what, true); },
};

// 바뀐 파일만 IndexedDB에 쓴다. 쓰는 중에 또 부르면 끝난 뒤 한 번 더 돈다.
let syncing = false, again = false;
Module.saveSync = () => {
  if (!store) return;
  if (syncing) { again = true; return; }
  syncing = true;
  store.syncfs(false, err => {
    if (err) console.warn('저장소 쓰기 실패', err);
    syncing = false;
    if (again) { again = false; Module.saveSync(); }
  });
};
const syncNow = () => new Promise(done => store.syncfs(false, done));

const exists = path => store.analyzePath(path).exists;
const dataReady = () => exists(PERSIST + '/game/client.bin') && exists(PERSIST + '/game.ev') && exists(PERSIST + '/ChronosGame.ttf')
  && exists(PERSIST + '/data.version') && store.readFile(PERSIST + '/data.version', { encoding: 'utf8' }) === DATA_VERSION;

function show() {
  const ok = dataReady();
  $('need').hidden = ok;
  $('ready').hidden = !ok;
  status('');
}

function boot() {
  store = Module.FS;
  store.mkdir(PERSIST);
  store.mount(Module.IDBFS, {}, PERSIST);
  store.syncfs(true, err => {
    if (err) { status('브라우저 저장소를 열지 못했습니다. 사생활 보호 창에서는 실행할 수 없습니다.', true); return; }
    show();
  });
}

function removeTree(path) {
  if (!exists(path)) return;
  for (const name of store.readdir(path)) {
    if (name === '.' || name === '..') continue;
    const child = path + '/' + name;
    if (store.isDir(store.stat(child).mode)) removeTree(child); else store.unlink(child);
  }
  store.rmdir(path);
}

async function install(file) {
  $('need').hidden = true;
  try {
    status('원본 파일을 푸는 중…');
    const game = await CWGameData.extractGame(new Uint8Array(await file.arrayBuffer()));
    status('소리 데이터를 만드는 중…');
    const events = await CWGameData.buildEvents(game, await (await fetch('smaf2ev.wasm')).arrayBuffer());
    status('글꼴을 만드는 중…');
    const font = CWGameData.buildFont(game);
    status('저장하는 중…');
    removeTree(PERSIST + '/game');
    for (const [name, data] of game) {
      const path = PERSIST + '/game/' + name;
      store.mkdirTree(path.slice(0, path.lastIndexOf('/')));
      store.writeFile(path, data);
    }
    store.writeFile(PERSIST + '/game.ev', events);
    store.writeFile(PERSIST + '/ChronosGame.ttf', font);
    store.writeFile(PERSIST + '/data.version', DATA_VERSION);
    const err = await syncNow();
    if (err) throw new Error('브라우저 저장소에 쓰지 못했습니다. 남은 공간을 확인하세요.');
    show();
  } catch (e) {
    console.error(e);
    show();
    status(e.message || String(e), true);
  }
}

function start() {
  $('setup').hidden = true;
  if (navigator.storage && navigator.storage.persist) navigator.storage.persist();
  const args = [PERSIST + '/game', '--save', PERSIST + '/save', '--soundfont', '/1mgm.sf2'];
  const extra = new URLSearchParams(location.search); // 시험용: ?touch=1 화면 패드, ?width=<폭>
  if (navigator.maxTouchPoints > 0 || extra.get('touch')) args.push('--touch');
  if (extra.get('width')) args.push('--width', extra.get('width'));
  $('canvas').focus();
  Module.callMain(args);
}

// 게임이 부른다(host/savefile.c).
Module.pickSave = () => { $('savefile').value = ''; $('picker').hidden = false; };
function picked(path) {
  $('picker').hidden = true;
  $('canvas').focus();
  Module.ccall('web_save_picked', null, ['string'], [path]);
}
Module.downloadFile = path => {
  if (!exists(path)) return;
  const url = URL.createObjectURL(new Blob([store.readFile(path)], { type: 'application/octet-stream' }));
  const a = document.createElement('a');
  a.href = url;
  a.download = path.slice(path.lastIndexOf('/') + 1);
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 60000);
  store.unlink(path);
};
Module.onGameExit = () => { $('ended').hidden = false; };

$('source').addEventListener('change', e => { if (e.target.files[0]) install(e.target.files[0]); });
$('start').addEventListener('click', start);
$('forget').addEventListener('click', async () => {
  removeTree(PERSIST + '/game');
  for (const f of ['/game.ev', '/ChronosGame.ttf', '/data.version']) if (exists(PERSIST + f)) store.unlink(PERSIST + f);
  await syncNow();
  show();
});
$('savefile').addEventListener('change', async e => {
  const file = e.target.files[0];
  if (!file) return;
  store.writeFile(IMPORT_PATH, new Uint8Array(await file.arrayBuffer()));
  picked(IMPORT_PATH);
});
$('cancel').addEventListener('click', () => picked(''));
$('again').addEventListener('click', () => location.reload());
document.addEventListener('visibilitychange', () => { if (document.hidden) Module.saveSync(); });
addEventListener('pagehide', () => Module.saveSync());
addEventListener('contextmenu', e => e.preventDefault());

if ('serviceWorker' in navigator && (location.protocol === 'https:' || location.hostname === 'localhost')) navigator.serviceWorker.register('sw.js');
