// Strata service worker.
//
// 1. Offline play: the game files are cached when the worker installs, and
//    every same-origin file is fetched from the network when possible (so
//    updates arrive at once) and from the cache otherwise.
// 2. Threads: it adds the cross-origin isolation headers (COOP/COEP) that
//    WebAssembly threads require, so the faster multi-threaded build also
//    works on static hosts that cannot set headers. They apply from the next
//    page load after installation.
const CACHE = 'strata-v2';
const FILES = ['./', 'index.html', 'manifest.webmanifest', 'icon.svg', 'icon-192.png', 'icon-512.png',
               'icon-maskable-512.png', 'strata.js', 'strata.wasm', 'strata-mt.js', 'strata-mt.wasm',
               'strata-mt.worker.js'];

self.addEventListener('install', (event) => {
  self.skipWaiting();
  // Fetch each file on its own: a host without the threaded build (or a
  // newer Emscripten without a separate worker script) must not stop the rest.
  event.waitUntil(caches.open(CACHE).then((cache) => Promise.all(
      FILES.map((file) => cache.add(new Request(file, { cache: 'reload' })).catch(() => {})))));
});
self.addEventListener('activate', (event) => {
  event.waitUntil((async () => {
    for (const key of await caches.keys()) if (key !== CACHE) await caches.delete(key);
    await self.clients.claim();
  })());
});

function isolated(response) {
  if (!response || response.type === 'opaque' || response.status === 0) return response;
  const headers = new Headers(response.headers);
  headers.set('Cross-Origin-Opener-Policy', 'same-origin');
  headers.set('Cross-Origin-Embedder-Policy', 'require-corp');
  headers.set('Cross-Origin-Resource-Policy', 'same-origin');
  return new Response(response.body, { status: response.status, statusText: response.statusText, headers });
}

self.addEventListener('fetch', (event) => {
  const request = event.request;
  if (request.method !== 'GET' || new URL(request.url).origin !== self.location.origin) return;
  event.respondWith((async () => {
    try {
      const response = await fetch(request);
      if (response.ok) {
        const copy = response.clone();
        event.waitUntil(caches.open(CACHE).then((cache) => cache.put(request, copy)));
      }
      return isolated(response);
    } catch (err) {
      const cached = await caches.match(request, { ignoreSearch: request.mode === 'navigate' });
      if (cached) return isolated(cached);
      throw err;
    }
  })());
});
