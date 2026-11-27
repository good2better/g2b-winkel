const CACHE = 'g2b-livemasker-browser-v2.1.1';
const ASSETS = [
  './',
  './index.html',
  './styles.css',
  './app.js',
  './face-worker.js',
  './segment-worker.js',
  './manifest.webmanifest',
  './LICENTIES.txt',
  './kat.png',
  './robot.png',
  './icon-192.png',
  './icon-512.png',
  './uil.png',
  '/js/g2b-gate.js',
  './vendor/mediapipe/vision_bundle.cjs.js',
  './vendor/mediapipe/face_landmarker.task',
  './vendor/mediapipe/selfie_segmenter.tflite',
  './vendor/mediapipe/LICENSE-APACHE-2.0.txt',
  './vendor/mediapipe/PROVENANCE.json',
  './vendor/mediapipe/wasm/vision_wasm_internal.js',
  './vendor/mediapipe/wasm/vision_wasm_internal.wasm',
  './vendor/mediapipe/wasm/vision_wasm_nosimd_internal.js',
  './vendor/mediapipe/wasm/vision_wasm_nosimd_internal.wasm'
];

self.addEventListener('install', event => {
  event.waitUntil(
    caches.open(CACHE)
      .then(cache => cache.addAll(ASSETS))
      .then(() => self.skipWaiting())
  );
});

self.addEventListener('activate', event => {
  event.waitUntil(
    caches.keys()
      .then(keys => Promise.all(keys.filter(key => key !== CACHE).map(key => caches.delete(key))))
      .then(() => self.clients.claim())
  );
});

self.addEventListener('fetch', event => {
  if (event.request.method !== 'GET') return;
  const url = new URL(event.request.url);
  if (url.origin !== location.origin) return;

  if (event.request.mode === 'navigate') {
    event.respondWith(
      fetch(event.request).then(response => {
        if (response.ok) {
          const copy = response.clone();
          caches.open(CACHE).then(cache => cache.put('./index.html', copy));
        }
        return response;
      }).catch(() => caches.match('./index.html'))
    );
    return;
  }

  const pathname = url.pathname;
  const longLived = pathname.includes('/vendor/mediapipe/') || /\.(png|webp|gif)$/i.test(pathname);
  if (longLived) {
    event.respondWith(
      caches.match(event.request).then(cached => {
        if (cached) return cached;
        return fetch(event.request).then(response => {
          if (response.ok) {
            const copy = response.clone();
            caches.open(CACHE).then(cache => cache.put(event.request, copy));
          }
          return response;
        });
      })
    );
    return;
  }

  event.respondWith(
    fetch(event.request).then(response => {
      if (response.ok) {
        const copy = response.clone();
        caches.open(CACHE).then(cache => cache.put(event.request, copy));
      }
      return response;
    }).catch(() => caches.match(event.request))
  );
});
