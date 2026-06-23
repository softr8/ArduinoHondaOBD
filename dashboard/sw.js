// Caches the app shell so the PWA installs and opens full-screen offline.
// Live data is NEVER cached — it always comes over the WebSocket.
const CACHE = "hobd-v2";
const SHELL = [
  "./",
  "./index.html",
  "./style.css",
  "./app.js",
  "./dtc-codes.js",
  "./manifest.webmanifest",
];

self.addEventListener("install", (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(SHELL)).then(() => self.skipWaiting()));
});

self.addEventListener("activate", (e) => {
  e.waitUntil(
    caches.keys().then((keys) =>
      Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k)))
    ).then(() => self.clients.claim())
  );
});

// Cache-first for the shell; ignore non-GET and WebSocket traffic.
self.addEventListener("fetch", (e) => {
  if (e.request.method !== "GET") return;
  e.respondWith(
    caches.match(e.request).then((hit) => hit || fetch(e.request).catch(() => caches.match("./index.html")))
  );
});
