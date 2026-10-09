// GS Books service worker — offline-leesmodus voor kinderen
const CACHE = "gsbooks-v5";
const CORE = [
  "./index.html",
  "./manifest.webmanifest",
  "./boeken.json",
  "./quiz/quiz.json",
  "./kleur/platen.json",
  "./img/favicon.svg",
  "./img/icon-192.png",
  "./img/icon-512.png",
  "./kleur/",
  "./quiz/",
  "./paspoort/"
];

self.addEventListener("install", e => {
  e.waitUntil(caches.open(CACHE).then(c => c.addAll(CORE)).then(() => self.skipWaiting()));
});

self.addEventListener("activate", e => {
  e.waitUntil(caches.keys().then(ks =>
    Promise.all(ks.filter(k => k !== CACHE).map(k => caches.delete(k)))
  ).then(() => self.clients.claim()));
});

// Pagina's + JSON altijd vers proberen (netwerk-eerst, offline → cache);
// losse assets (plaatjes/audio/fonts) cache-first — die veranderen nooit.
self.addEventListener("fetch", e => {
  const u = new URL(e.request.url);
  if (e.request.method !== "GET" || u.origin !== location.origin) return;
  // Grote media (video/audio) nooit cachen: te groot voor cache-opslag én
  // de browser streamt ze beter zelf (range-requests, geen vaste caches).
  if (/\.(mp4|mp3|m4a|m4b|wav|webm|ogg|mov|mpg|m4v)(\?.*)?$/i.test(u.pathname)) {
    e.respondWith(fetch(e.request));
    return;
  }
  const vers = e.request.mode === "navigate" || u.pathname.endsWith(".html") || u.pathname.endsWith(".json") || u.pathname.endsWith("/");
  e.respondWith(
    vers
      ? fetch(e.request).then(res => {
          if (res.ok) {
            const kloon = res.clone();
            caches.open(CACHE).then(c => c.put(e.request, kloon));
          }
          return res;
        }).catch(() => caches.match(e.request))
      : caches.match(e.request).then(hit => hit ||
          fetch(e.request).then(res => {
            if (res.ok) {
              const kloon = res.clone();
              caches.open(CACHE).then(c => c.put(e.request, kloon));
            }
            return res;
          })
        )
  );
});
