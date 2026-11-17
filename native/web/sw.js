// 앱 껍데기만 캐시해 오프라인에서도 열리게 한다. 게임 데이터와 세이브는 IndexedDB에 있고 여기서 다루지 않는다.
const CACHE = 'chronos-wing-__BUILD__';
const FILES = ['./', 'index.html', 'app.js', 'gamedata.js', 'chronos.js', 'chronos.wasm', 'chronos.data', 'smaf2ev.wasm', 'manifest.webmanifest', 'icon.svg', 'icon-192.png', 'icon-512.png'];

self.addEventListener('install', e => e.waitUntil(caches.open(CACHE).then(c => c.addAll(FILES)).then(() => self.skipWaiting())));
self.addEventListener('activate', e => e.waitUntil(
  caches.keys().then(keys => Promise.all(keys.filter(k => k !== CACHE).map(k => caches.delete(k)))).then(() => self.clients.claim())));
self.addEventListener('fetch', e => {
  if (e.request.method !== 'GET') return;
  e.respondWith(caches.match(e.request, { ignoreSearch: true }).then(hit => hit || fetch(e.request)));
});
