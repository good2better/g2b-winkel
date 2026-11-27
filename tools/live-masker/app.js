/* G2B Live-Masker browserlijn v2 — licht, local-first en bewust géén kopie van het app-prototype. */
const $ = id => document.getElementById(id);

const els = {
  cv: $('cv'), stage: $('stage'), laden: $('laden'), trackPill: $('trackPill'), status: $('status'), perf: $('perf'),
  start: $('start'), snap: $('snap'), rec: $('rec'), kars: $('kars'), up: $('up'), upBg: $('upBg'),
  sc: $('sc'), ox: $('ox'), oy: $('oy'), scOut: $('scOut'), oxOut: $('oxOut'), oyOut: $('oyOut'), resetFit: $('resetFit'),
  quality: $('quality'), cameraSelect: $('cameraSelect'), micSelect: $('micSelect'), micOn: $('micOn'), mirror: $('mirror'),
  lightMatch: $('lightMatch'), privacyMode: $('privacyMode'), privacyBadge: $('privacyBadge'), offlineBadge: $('offlineBadge'), modelBadge: $('modelBadge')
};

const cx = els.cv.getContext('2d', { alpha: false, desynchronized: true });
cx.imageSmoothingEnabled = true;
cx.imageSmoothingQuality = 'high';
const vid = document.createElement('video');
vid.playsInline = true;
vid.muted = true;

const QUALITY = {
  eco: { longSide: 768, trackLongSide: 384, detectMs: 55, recordFps: 24, bitrate: 2_200_000, label: '18 Hz' },
  balanced: { longSide: 960, trackLongSide: 480, detectMs: 40, recordFps: 30, bitrate: 4_000_000, label: '25 Hz' },
  sharp: { longSide: 1280, trackLongSide: 640, detectMs: 33, recordFps: 30, bitrate: 6_000_000, label: '30 Hz' }
};

/* Posities zijn gemeten in de bestaande browserlijn-assets. Zo blijft de eigen PNG-identiteit behouden,
   maar wordt wél op de ogen/irissen uitgelijnd in plaats van grof op het midden van het gezicht. */
const CHARACTERS = {
  kat: {
    label: 'Kat', src: './kat.png', scale: 0.82,
    eyeCX: 0.4955108514421094, eyeY: 0.31418795356705087, eyeDist: 0.1917736940077595,
    mouth: { x: 0.50, y: 0.415, w: 0.13 }
  },
  robot: {
    label: 'Robot', src: './robot.png', scale: 0.88,
    eyeCX: 0.5, eyeY: 0.310546875, eyeDist: 0.205078125,
    mouth: { x: 0.50, y: 0.380, w: 0.12 }
  },
  uil: {
    label: 'Uil', src: './uil.png', scale: 0.90,
    eyeCX: 0.49928385779712847, eyeY: 0.35662746598720185, eyeDist: 0.22725315627328485,
    mouth: { x: 0.50, y: 0.450, w: 0.10 }
  }
};
// eigen uploads hebben geen gemeten mondpositie — generieke gezichtsverhouding als aardige default
const DEFAULT_MOUTH = { x: 0.5, y: 0.60, w: 0.15 };
for (const def of Object.values(CHARACTERS)) {
  def.img = new Image();
  def.img.decoding = 'async';
  def.img.src = def.src;
}

const defaultFit = () => ({ sc: 1, ox: 0, oy: 0 });
let fitStore = {};
try { fitStore = JSON.parse(localStorage.getItem('g2b-livemasker-browser-fit-v2') || '{}') || {}; } catch (_) { fitStore = {}; }
let settings = {};
try { settings = JSON.parse(localStorage.getItem('g2b-livemasker-browser-settings-v2') || '{}') || {}; } catch (_) { settings = {}; }

let kar = 'kat';
let stream = null;
let running = false;
let restartingMedia = false;
let runToken = 0;
let worker = null;
let workerReady = false;
let workerError = '';
let workerDelegate = '';
let workerInitPromise = null;
let detectionPending = false;
let detectionTimeouts = 0;
let workerRestarting = false;
let detectionSeq = 0;
let lastDetectSent = 0;
let inferenceAvg = 0;
let effectiveDetectMs = 0;
let lastTrackInput = { width: 0, height: 0 };
let audioContext = null;
let micLevel = 0;
let recorder = null;
let chunks = [];
let recordStartedAt = 0;
let lastRecordTimerUpdate = 0;
let hiddenCaptureTimer = null;
let frameCount = 0;
let detectionCount = 0;
let lastPerfUpdate = performance.now();
let lastTrackUiUpdate = 0;
let noFaceSince = 0;
let lastFaceTime = 0;
let lastLightSample = 0;
let objectUrls = [];
let lastMaskMetrics = null;
let bgKind = 'camera';
let fxKind = 'uit';
let bgImage = null;
let bgImageLabel = '';
let segWorker = null;
let segReady = false;
let segError = '';
let segDelegate = '';
let segInitPromise = null;
let segPending = false;
let segSeq = 0;
let lastSegSent = 0;
let segInferenceAvg = 0;
let personMask = null;
let personCoverage = 0;
// zelfherstellende segmentatie: op sommige mobiele GPU's levert de GPU-delegate
// stilletjes lege maskers (persoon onzichtbaar ondanks werkende tracking).
// De watchdog gebruikt gezichtsdetectie als waarheid: is er een gezicht maar blijft
// de dekking vrijwel nul, dan proberen we eenmalig de andere delegate.
let segLowCoverageStreak = 0;
let segReinitAttempts = 0;
let segReiniting = false;
let segDelegateForced = '';
const personMaskCanvas = document.createElement('canvas');
const personCanvas = document.createElement('canvas');
const bgCacheCanvas = document.createElement('canvas');
let bgCacheKey = '';
let particles = [];
const reducedMotionQuery = window.matchMedia ? window.matchMedia('(prefers-reduced-motion: reduce)') : null;
let reducedMotion = !!(reducedMotionQuery && reducedMotionQuery.matches);
if (reducedMotionQuery && reducedMotionQuery.addEventListener) {
  reducedMotionQuery.addEventListener('change', event => { reducedMotion = !!event.matches; });
}

function clamp(v, lo = 0, hi = 1) { return Math.max(lo, Math.min(hi, v)); }
function lerp(a, b, t) { return a + (b - a) * t; }
function profile() { return QUALITY[els.quality.value] || QUALITY.balanced; }
function fit() { return { ...defaultFit(), ...(fitStore[kar] || {}) }; }
function saveSettings() {
  settings = {
    quality: els.quality.value,
    mirror: els.mirror.checked,
    micOn: els.micOn.checked,
    lightMatch: els.lightMatch.checked,
    privacyMode: els.privacyMode.value,
    bg: bgKind,
    fx: fxKind
  };
  try { localStorage.setItem('g2b-livemasker-browser-settings-v2', JSON.stringify(settings)); } catch (_) {}
}
function applySettings() {
  if (settings.quality && QUALITY[settings.quality]) els.quality.value = settings.quality;
  if (typeof settings.mirror === 'boolean') els.mirror.checked = settings.mirror;
  if (typeof settings.micOn === 'boolean') els.micOn.checked = settings.micOn;
  if (typeof settings.lightMatch === 'boolean') els.lightMatch.checked = settings.lightMatch;
  if (['freeze', 'blur', 'off'].includes(settings.privacyMode)) els.privacyMode.value = settings.privacyMode;
  if (settings.bg && ['camera', 'ruimte', 'onderwater', 'zonsondergang', 'gamewereld'].includes(settings.bg)) {
    bgKind = settings.bg;
  }
  if (settings.fx && (settings.fx === 'uit' || FX_SPECS[settings.fx])) {
    fxKind = settings.fx;
  }
}
function saveFit() {
  fitStore[kar] = {
    sc: parseFloat(els.sc.value),
    ox: parseFloat(els.ox.value),
    oy: parseFloat(els.oy.value)
  };
  try { localStorage.setItem('g2b-livemasker-browser-fit-v2', JSON.stringify(fitStore)); } catch (_) {}
}
function applyFitToControls() {
  const f = fit();
  els.sc.value = f.sc;
  els.ox.value = f.ox;
  els.oy.value = f.oy;
  updateFitOutputs();
}
function updateFitOutputs() {
  els.scOut.textContent = Math.round(parseFloat(els.sc.value) * 100) + '%';
  els.oxOut.textContent = Number(parseFloat(els.ox.value).toFixed(2));
  els.oyOut.textContent = Number(parseFloat(els.oy.value).toFixed(2));
}
for (const input of [els.sc, els.ox, els.oy]) {
  input.addEventListener('input', () => { updateFitOutputs(); saveFit(); });
}

function setStatus(text) { els.status.textContent = text; }
function setOverlay(visible, title, text, mode = 'loading') {
  els.laden.classList.toggle('hidden', !visible);
  const loader = els.laden.querySelector('.loader');
  loader.classList.toggle('done', mode === 'done');
  loader.classList.toggle('error', mode === 'error');
  if (visible) {
    const box = els.laden.querySelector('div:last-child');
    box.innerHTML = '';
    const strong = document.createElement('strong');
    strong.textContent = title;
    const br = document.createElement('br');
    const span = document.createElement('span');
    span.textContent = text;
    box.append(strong, br, span);
  }
}
function setBadge(el, text, state) {
  el.textContent = text;
  el.classList.toggle('ok', state === 'ok');
  el.classList.toggle('warn', state === 'warn');
}
function updatePrivacyBadge() {
  let external = [];
  try {
    external = performance.getEntriesByType('resource').filter(r => {
      try { return new URL(r.name, location.href).origin !== location.origin; } catch (_) { return false; }
    });
  } catch (_) {}
  setBadge(els.privacyBadge, external.length ? `privacy: ${external.length} extern` : 'privacy: 0 externe bronnen', external.length ? 'warn' : 'ok');
  els.privacyBadge.title = external.map(r => r.name).join('\n');
}
function updateTrackPill(forceState) {
  if (forceState) els.trackPill.dataset.state = forceState;
  const state = els.trackPill.dataset.state || 'off';
  els.trackPill.classList.remove('on', 'search', 'off');
  if (state === 'on') {
    els.trackPill.classList.add('on');
    els.trackPill.textContent = 'gezicht gevonden';
  } else if (state === 'search') {
    els.trackPill.classList.add('search');
    els.trackPill.textContent = noFaceSince ? 'gezicht even kwijt' : 'gezicht zoeken…';
  } else {
    els.trackPill.classList.add('off');
    els.trackPill.textContent = 'tracking uit';
  }
}
function updateModelBadge() {
  if (workerReady) setBadge(els.modelBadge, `model lokaal · ${workerDelegate || 'auto'}`, 'ok');
  else if (workerError) setBadge(els.modelBadge, 'model fout', 'warn');
  else setBadge(els.modelBadge, 'model laden…', '');
}

function smoothingAlpha(cutoff, dt) {
  const safeCutoff = Math.max(0.00001, cutoff);
  const tau = 1 / (2 * Math.PI * safeCutoff);
  return 1 / (1 + tau / Math.max(0.00001, dt));
}
class LowPassFilter {
  constructor() { this.y = null; }
  filter(x, alpha) {
    if (this.y === null) this.y = x;
    else this.y += alpha * (x - this.y);
    return this.y;
  }
  reset(x = null) { this.y = x; }
}
class OneEuroFilter {
  constructor(minCutoff = 1.25, beta = 0.02, dCutoff = 1.0) {
    this.minCutoff = minCutoff;
    this.beta = beta;
    this.dCutoff = dCutoff;
    this.xFilter = new LowPassFilter();
    this.dxFilter = new LowPassFilter();
    this.lastTime = null;
  }
  filter(x, timeSeconds) {
    if (this.lastTime === null) {
      this.lastTime = timeSeconds;
      this.xFilter.reset(x);
      this.dxFilter.reset(0);
      return x;
    }
    const dt = Math.max(1 / 240, timeSeconds - this.lastTime);
    this.lastTime = timeSeconds;
    const dx = (x - this.xFilter.y) / dt;
    const edx = this.dxFilter.filter(dx, smoothingAlpha(this.dCutoff, dt));
    const cutoff = this.minCutoff + this.beta * Math.abs(edx);
    return this.xFilter.filter(x, smoothingAlpha(cutoff, dt));
  }
  reset(x, timeSeconds) {
    this.xFilter.reset(x);
    this.dxFilter.reset(0);
    this.lastTime = timeSeconds;
  }
}
function makeFaceState() {
  return {
    x: new OneEuroFilter(1.35, 0.022), y: new OneEuroFilter(1.35, 0.022), d: new OneEuroFilter(1.1, 0.018),
    rs: new OneEuroFilter(1.6, 0.015), rc: new OneEuroFilter(1.6, 0.015),
    x0: 0, y0: 0, d0: 1, r0: 0, lastSeen: 0, ready: false, blend: {}, matrix: [], yaw: 0, pitch: 0, light: 1
  };
}
let face = makeFaceState();
function resetFace() {
  face = makeFaceState();
  lastMaskMetrics = null;
  lastFaceTime = 0;
  noFaceSince = 0;
}

function landmarkPoint(lm, index, width, height, mirror) {
  const p = lm[index];
  return { x: (mirror ? 1 - p.x : p.x) * width, y: p.y * height, z: p.z };
}
function computeFaceTarget(msg) {
  const lm = msg.landmarks || [];
  const W = els.cv.width;
  const H = els.cv.height;
  const mirror = els.mirror.checked;
  const leftIris = lm[468] ? landmarkPoint(lm, 468, W, H, mirror) : landmarkPoint(lm, 33, W, H, mirror);
  const rightIris = lm[473] ? landmarkPoint(lm, 473, W, H, mirror) : landmarkPoint(lm, 263, W, H, mirror);
  const x = (leftIris.x + rightIris.x) / 2;
  const y = (leftIris.y + rightIris.y) / 2;
  let dx = rightIris.x - leftIris.x;
  let dy = rightIris.y - leftIris.y;
  if (dx < 0) { dx = -dx; dy = -dy; }
  const d = Math.max(10, Math.hypot(dx, dy));
  const r = Math.atan2(dy, dx);
  return { x, y, d, r, blend: msg.blend || {}, matrix: msg.matrix || [] };
}
function smoothMap(previous, next, alpha = 0.44) {
  const out = { ...previous };
  for (const [key, value] of Object.entries(next || {})) {
    out[key] = previous[key] === undefined ? value : previous[key] + (value - previous[key]) * alpha;
  }
  return out;
}
function rotationsFromMatrix(m) {
  if (!m || m.length < 16) return { yaw: 0, pitch: 0 };
  const r00 = m[0], r01 = m[4], r02 = m[8], r12 = m[9], r22 = m[10];
  const yaw = Math.asin(clamp(r02, -1, 1));
  const pitch = Math.atan2(-r12, r22 || 0.00001);
  return { yaw: clamp(yaw, -0.75, 0.75), pitch: clamp(pitch, -0.65, 0.65) };
}
function yawPitchCoefficients(yaw, pitch) {
  const cy = Math.cos(yaw), sy = Math.sin(yaw), cxp = Math.cos(pitch), sxp = Math.sin(pitch);
  return {
    r00: cy, r01: sy * sxp,
    r10: 0, r11: cxp
  };
}
function updateFace(target, now) {
  const seconds = now / 1000;
  if (!face.ready || now - face.lastSeen > 650) {
    face.x.reset(target.x, seconds);
    face.y.reset(target.y, seconds);
    face.d.reset(target.d, seconds);
    face.rs.reset(Math.sin(target.r), seconds);
    face.rc.reset(Math.cos(target.r), seconds);
    face.blend = target.blend;
    const rot = rotationsFromMatrix(target.matrix);
    face.yaw = rot.yaw;
    face.pitch = rot.pitch;
    face.ready = true;
  } else {
    face.x.filter(target.x, seconds);
    face.y.filter(target.y, seconds);
    face.d.filter(target.d, seconds);
    face.rs.filter(Math.sin(target.r), seconds);
    face.rc.filter(Math.cos(target.r), seconds);
    face.blend = smoothMap(face.blend, target.blend, 0.48);
    const rot = rotationsFromMatrix(target.matrix);
    face.yaw += (rot.yaw - face.yaw) * 0.38;
    face.pitch += (rot.pitch - face.pitch) * 0.38;
  }
  face.matrix = target.matrix || [];
  face.x0 = face.x.y ?? target.x;
  face.y0 = face.y.y ?? target.y;
  face.d0 = face.d.y ?? target.d;
  face.r0 = Math.atan2(face.rs.y ?? Math.sin(target.r), face.rc.y ?? Math.cos(target.r));
  face.lastSeen = now;
  lastFaceTime = now;
  noFaceSince = 0;
  updateTrackPill('on');
}
function handleDetectionResult(msg) {
  detectionPending = false;
  if (!msg.ok) return;
  detectionTimeouts = 0;
  detectionCount++;
  inferenceAvg = inferenceAvg ? inferenceAvg * 0.82 + msg.inferenceMs * 0.18 : msg.inferenceMs;
  const now = performance.now();
  if (msg.hasFace) updateFace(computeFaceTarget(msg), now);
  else if (!noFaceSince) {
    noFaceSince = now;
    updateTrackPill('search');
  }
}

function scaleOneEuro(filter, scale) {
  if (!filter || !Number.isFinite(scale) || scale <= 0) return;
  for (const part of [filter.xFilter, filter.dxFilter]) {
    if (part && part.y !== null && part.y !== undefined) part.y *= scale;
  }
}
function rescaleFaceForCanvas(oldW, oldH, newW, newH) {
  if (!face.ready || !oldW || !oldH || !newW || !newH || (oldW === newW && oldH === newH)) return;
  const sx = newW / oldW;
  const sy = newH / oldH;
  const sd = (sx + sy) / 2;
  scaleOneEuro(face.x, sx);
  scaleOneEuro(face.y, sy);
  scaleOneEuro(face.d, sd);
  face.x0 *= sx;
  face.y0 *= sy;
  face.d0 *= sd;
}
function configureCanvasForVideo() {
  const p = profile();
  const oldW = els.cv.width;
  const oldH = els.cv.height;
  const vw = vid.videoWidth || 960;
  const vh = vid.videoHeight || 720;
  if (vw >= vh) {
    els.cv.width = p.longSide;
    els.cv.height = Math.round(p.longSide * vh / vw);
  } else {
    els.cv.height = p.longSide;
    els.cv.width = Math.round(p.longSide * vw / vh);
  }
  rescaleFaceForCanvas(oldW, oldH, els.cv.width, els.cv.height);
  if (els.stage) els.stage.style.aspectRatio = `${els.cv.width} / ${els.cv.height}`;
}
function privacyAlpha(now) {
  if (!face.ready) return 0;
  const age = now - face.lastSeen;
  const mode = els.privacyMode.value;
  const hold = mode === 'off' ? 80 : 430;
  if (age <= hold) return 1;
  return clamp(1 - (age - hold) / 430, 0, 1);
}
function updateLight(now) {
  if (!els.lightMatch.checked || !face.ready || vid.readyState < 2) {
    face.light += (1 - face.light) * 0.2;
    return;
  }
  const interval = els.quality.value === 'eco' ? 650 : 480;
  if (now - lastLightSample < interval) return;
  lastLightSample = now;
  const d = Math.max(24, face.d0 * 1.25);
  const x = Math.max(0, Math.min(els.cv.width - 2, Math.round(face.x0 - d / 2)));
  const y = Math.max(0, Math.min(els.cv.height - 2, Math.round(face.y0 - d / 2)));
  const w = Math.max(2, Math.min(els.cv.width - x, Math.round(d)));
  const h = Math.max(2, Math.min(els.cv.height - y, Math.round(d)));
  try {
    const data = cx.getImageData(x, y, w, h).data;
    let sum = 0;
    let count = 0;
    for (let i = 0; i < data.length; i += 16) {
      sum += data[i] * 0.2126 + data[i + 1] * 0.7152 + data[i + 2] * 0.0722;
      count++;
    }
    const avg = sum / Math.max(1, count) / 255;
    face.light += (avg - face.light) * 0.32;
  } catch (_) {}
}
/* ---- achtergrondmagie en beeldeffecten ----
   Lichte procedurele canvas-engine: geen engine, geen cloud, geen zware modellen behalve het
   250 KB selfie-segmentatiemodeltje dat alleen draait als een niet-camera-achtergrond actief is. */
function seededRandom(seed) {
  let a = seed >>> 0;
  return () => {
    a += 0x6D2B79F5;
    let x = a;
    x = Math.imul(x ^ x >>> 15, x | 1);
    x ^= x + Math.imul(x ^ x >>> 7, x | 61);
    return ((x ^ x >>> 14) >>> 0) / 4294967296;
  };
}
function ensureBgCache(kind, W, H) {
  const key = `${kind}:${W}x${H}`;
  if (bgCacheKey === key && bgCacheCanvas.width) return true;
  bgCacheKey = key;
  bgCacheCanvas.width = W;
  bgCacheCanvas.height = H;
  const g = bgCacheCanvas.getContext('2d');
  const rnd = seededRandom(kind === 'ruimte' ? 421337 : kind === 'onderwater' ? 90210 : 77123);
  if (kind === 'ruimte') {
    const grad = g.createLinearGradient(0, 0, W * 0.3, H);
    grad.addColorStop(0, '#040617');
    grad.addColorStop(0.45, '#101238');
    grad.addColorStop(0.75, '#241040');
    grad.addColorStop(1, '#040617');
    g.fillStyle = grad;
    g.fillRect(0, 0, W, H);
    for (let i = 0; i < 230; i++) {
      const x = rnd() * W, y = rnd() * H, r = 0.4 + rnd() * 1.7;
      g.fillStyle = `rgba(255,255,255,${(0.25 + rnd() * 0.7).toFixed(3)})`;
      g.beginPath();
      g.arc(x, y, r, 0, Math.PI * 2);
      g.fill();
    }
    for (const neb of [[0.24, 0.32, 0.5, 'rgba(41,203,255,.20)'], [0.74, 0.55, 0.55, 'rgba(181,63,255,.18)'], [0.5, 0.82, 0.4, 'rgba(255,111,145,.12)']]) {
      const rg = g.createRadialGradient(W * neb[0], H * neb[1], 0, W * neb[0], H * neb[1], Math.max(W, H) * neb[2]);
      rg.addColorStop(0, neb[3]);
      rg.addColorStop(1, 'rgba(0,0,0,0)');
      g.fillStyle = rg;
      g.fillRect(0, 0, W, H);
    }
    return true;
  }
  if (kind === 'onderwater') {
    const grad = g.createLinearGradient(0, 0, 0, H);
    grad.addColorStop(0, '#0a6f9e');
    grad.addColorStop(0.45, '#0a4d7c');
    grad.addColorStop(1, '#052642');
    g.fillStyle = grad;
    g.fillRect(0, 0, W, H);
    for (let i = 0; i < 90; i++) {
      const x = rnd() * W, y = rnd() * H, r = 0.5 + rnd() * 1.6;
      g.fillStyle = `rgba(190,235,255,${(0.05 + rnd() * 0.12).toFixed(3)})`;
      g.beginPath();
      g.arc(x, y, r, 0, Math.PI * 2);
      g.fill();
    }
    const sand = g.createLinearGradient(0, H * 0.9, 0, H);
    sand.addColorStop(0, 'rgba(214,192,138,0)');
    sand.addColorStop(1, 'rgba(214,192,138,.30)');
    g.fillStyle = sand;
    g.fillRect(0, H * 0.85, W, H * 0.15);
    return true;
  }
  if (kind === 'zonsondergang') {
    const grad = g.createLinearGradient(0, 0, 0, H);
    grad.addColorStop(0, '#2b1055');
    grad.addColorStop(0.42, '#8f3d6e');
    grad.addColorStop(0.68, '#ff7e5f');
    grad.addColorStop(1, '#feb47b');
    g.fillStyle = grad;
    g.fillRect(0, 0, W, H);
    const sunX = W * 0.5, sunY = H * 0.66, sunR = Math.min(W, H) * 0.16;
    const glow = g.createRadialGradient(sunX, sunY, 0, sunX, sunY, sunR * 3.1);
    glow.addColorStop(0, 'rgba(255,236,170,.85)');
    glow.addColorStop(0.25, 'rgba(255,190,110,.38)');
    glow.addColorStop(1, 'rgba(255,150,80,0)');
    g.fillStyle = glow;
    g.fillRect(0, 0, W, H);
    g.fillStyle = '#ffdf9e';
    g.beginPath();
    g.arc(sunX, sunY, sunR, 0, Math.PI * 2);
    g.fill();
    for (let i = 0; i < 40; i++) {
      const x = rnd() * W, y = rnd() * H * 0.5, r = 0.4 + rnd() * 1.3;
      g.fillStyle = `rgba(255,255,255,${(0.12 + rnd() * 0.3).toFixed(3)})`;
      g.beginPath();
      g.arc(x, y, r, 0, Math.PI * 2);
      g.fill();
    }
    return true;
  }
  if (kind === 'gamewereld') {
    const sky = g.createLinearGradient(0, 0, 0, H);
    sky.addColorStop(0, '#06142f'); sky.addColorStop(0.42, '#12285b'); sky.addColorStop(0.75, '#26184d'); sky.addColorStop(1, '#070b1e');
    g.fillStyle = sky; g.fillRect(0, 0, W, H);
    for (const neb of [[.22,.22,.45,'rgba(38,230,190,.20)'],[.74,.28,.42,'rgba(145,80,255,.22)'],[.52,.66,.52,'rgba(255,85,180,.10)']]) {
      const rg = g.createRadialGradient(W*neb[0],H*neb[1],0,W*neb[0],H*neb[1],Math.max(W,H)*neb[2]);rg.addColorStop(0,neb[3]);rg.addColorStop(1,'rgba(0,0,0,0)');g.fillStyle=rg;g.fillRect(0,0,W,H);
    }
    for (let i=0;i<180;i++) { const x=rnd()*W,y=rnd()*H*.62,r=.5+rnd()*1.6;g.fillStyle=`rgba(255,255,255,${(.18+rnd()*.62).toFixed(3)})`;g.beginPath();g.arc(x,y,r,0,Math.PI*2);g.fill(); }
    g.fillStyle='rgba(5,8,24,.88)';g.beginPath();g.moveTo(0,H*.72);for(let x=0;x<=W;x+=W/12)g.lineTo(x,H*(.68+rnd()*.09));g.lineTo(W,H);g.lineTo(0,H);g.closePath();g.fill();
    g.strokeStyle='rgba(32,230,139,.34)';g.lineWidth=2;for(let i=0;i<13;i++){const y=H*(.74+i*.022);g.beginPath();g.moveTo(0,y);g.lineTo(W,y);g.stroke()}
    for(let i=-8;i<18;i++){g.beginPath();g.moveTo(W*.5+i*W*.055,H*.73);g.lineTo(W*.5+i*W*.16,H);g.stroke()}
    for(const isl of [[.16,.48,.10],[.82,.42,.12],[.64,.57,.075]]){g.fillStyle='rgba(28,47,88,.92)';g.beginPath();g.ellipse(W*isl[0],H*isl[1],W*isl[2],H*.035,0,0,Math.PI*2);g.fill();g.fillStyle='rgba(32,230,139,.30)';g.fillRect(W*(isl[0]-isl[2]*.55),H*(isl[1]-.012),W*isl[2]*1.1,3)}
    return true;
  }
  return false;
}
function drawCoverImage(img, W, H, drift) {
  const iw = img.naturalWidth || img.width || 1;
  const ih = img.naturalHeight || img.height || 1;
  const scale = Math.max(W / iw, H / ih);
  const dw = iw * scale;
  const dh = ih * scale;
  const panX = drift ? Math.sin(drift / 9000) * 0.012 : 0;
  const panY = drift ? Math.cos(drift / 11000) * 0.010 : 0;
  const dx = (W - dw) / 2 + panX * dw;
  const dy = (H - dh) / 2 + panY * dh;
  cx.drawImage(img, dx, dy, dw, dh);
}
let camSmooth={x:0,y:0,scale:1,mode:''};
function smoothDirectorCam(mode,raw){
 if(mode!=='regisseur'){camSmooth={x:raw.x,y:raw.y,scale:raw.scale,mode};return raw}
 if(camSmooth.mode!==mode)camSmooth={x:raw.x,y:raw.y,scale:raw.scale,mode};
 camSmooth.x+=(raw.x-camSmooth.x)*.10;camSmooth.y+=(raw.y-camSmooth.y)*.10;camSmooth.scale+=(raw.scale-camSmooth.scale)*.075;
 return{...raw,x:camSmooth.x,y:camSmooth.y,scale:camSmooth.scale,smoothed:true}
}
function virtualCam(now){
 const mode=($('camMode')&&$('camMode').value)||'statisch',t=now/1000;
 if(mode==='push')return{scale:1.035+Math.sin(t*.28)*.018,x:Math.sin(t*.11)*10,y:Math.cos(t*.13)*5,shot:'push-in'};
 if(mode==='orbit')return{scale:1.045,x:Math.sin(t*.32)*24,y:Math.cos(t*.24)*10,shot:'orbit'};
 if(mode==='multicam'){const shot=Math.floor(t/5)%3;return[ {scale:.98,x:-18,y:4,shot:'wide links'}, {scale:1.075,x:22,y:-5,shot:'close rechts'}, {scale:1.03,x:0,y:8,shot:'center detail'} ][shot]}
 if(mode==='regisseur'){
  const W=els.cv.width,H=els.cv.height,fresh=face.ready&&(now-face.lastSeen)<1400;
  const cxn=fresh?face.x0/W-.5:0,cyn=fresh?face.y0/H-.48:0;
  const phase=Math.floor(t/6)%3,energy=clamp(micLevel*1.8+(fresh?.12:0),0,1);
  const closeBoost=energy>.45?.045:0,base=[.985,1.055+closeBoost,1.015][phase];
  const faceScale=fresh?clamp(face.d0/(W*.16),.90,1.12):1;
  const orbit=phase===2?Math.sin(t*.7)*8:0;
  return smoothDirectorCam(mode,{scale:clamp(base*faceScale,.94,1.13),x:clamp(-cxn*W*.15,-38,38)+orbit,y:clamp(-cyn*H*.10,-18,18),shot:energy>.45?'auto close op energie':['auto wide','auto close','auto detail'][phase]})
 }
 return{scale:1,x:0,y:0,shot:'statisch'}
}
function drawSceneBackground(now) {
  const W = els.cv.width;
  const H = els.cv.height;
  cx.save();
  cx.fillStyle = '#000';
  cx.fillRect(0, 0, W, H);
  if (bgKind === 'eigen') {
    if (bgImage && (bgImage.naturalWidth || bgImage.width)) {
      drawCoverImage(bgImage, W, H, reducedMotion ? 0 : now);
    } else {
      const grad = cx.createLinearGradient(0, 0, 0, H);
      grad.addColorStop(0, '#12324a');
      grad.addColorStop(1, '#071722');
      cx.fillStyle = grad;
      cx.fillRect(0, 0, W, H);
    }
    cx.restore();
    return;
  }
  if (!ensureBgCache(bgKind, W, H)) {
    cx.restore();
    return;
  }
  if(bgKind==='gamewereld'){const vc=virtualCam(now);cx.save();cx.translate(W/2+vc.x*.38,H/2+vc.y*.32);cx.scale(1+(vc.scale-1)*.62,1+(vc.scale-1)*.62);cx.translate(-W/2,-H/2);cx.drawImage(bgCacheCanvas,0,0);cx.restore()}else cx.drawImage(bgCacheCanvas, 0, 0);
  const t = now / 1000;
  const motion = reducedMotion ? 0.25 : 1;
  cx.globalCompositeOperation = 'screen';
  if (bgKind === 'ruimte') {
    for (const orb of [
      [0.5 + 0.16 * Math.sin(t * 0.21 * motion), 0.38 + 0.07 * Math.cos(t * 0.27 * motion), 0.34, 'rgba(53,219,255,.10)'],
      [0.5 + 0.19 * Math.cos(t * 0.17 * motion), 0.58 + 0.09 * Math.sin(t * 0.23 * motion), 0.42, 'rgba(204,83,255,.09)']
    ]) {
      const rg = cx.createRadialGradient(W * orb[0], H * orb[1], 0, W * orb[0], H * orb[1], Math.max(W, H) * orb[2]);
      rg.addColorStop(0, orb[3]);
      rg.addColorStop(1, 'rgba(0,0,0,0)');
      cx.fillStyle = rg;
      cx.fillRect(0, 0, W, H);
    }
    const rnd = seededRandom(555);
    for (let i = 0; i < 16; i++) {
      const sx = rnd() * W, sy = rnd() * H, phase = rnd() * Math.PI * 2;
      const tw = 0.35 + 0.65 * Math.abs(Math.sin(t * (0.9 + rnd() * 1.6) * motion + phase));
      cx.fillStyle = `rgba(255,255,255,${(tw * 0.85).toFixed(3)})`;
      cx.beginPath();
      cx.arc(sx, sy, 0.8 + rnd() * 1.3, 0, Math.PI * 2);
      cx.fill();
    }
  } else if (bgKind === 'onderwater') {
    for (const ray of [[0.28, 0.9], [0.52, 1.1], [0.76, 0.8]]) {
      const sway = Math.sin(t * 0.3 * motion + ray[0] * 9) * 0.05;
      const rg = cx.createLinearGradient(W * (ray[0] + sway), 0, W * (ray[0] + sway + 0.13), H);
      rg.addColorStop(0, `rgba(190,240,255,${(0.14 * ray[1]).toFixed(3)})`);
      rg.addColorStop(1, 'rgba(190,240,255,0)');
      cx.fillStyle = rg;
      cx.beginPath();
      cx.moveTo(W * (ray[0] + sway) - W * 0.03, 0);
      cx.lineTo(W * (ray[0] + sway) + W * 0.08, 0);
      cx.lineTo(W * (ray[0] + sway) + W * 0.22, H);
      cx.lineTo(W * (ray[0] + sway) + W * 0.02, H);
      cx.closePath();
      cx.fill();
    }
    const rnd = seededRandom(313);
    for (let i = 0; i < 14; i++) {
      const bx = (rnd() + 0.05 * Math.sin(t * (0.5 + rnd()) * motion + i)) * W;
      const rise = ((t * (0.05 + rnd() * 0.05) * motion + rnd()) % 1);
      const by = (1 - rise) * H;
      const br = 2 + rnd() * 5;
      cx.strokeStyle = 'rgba(210,240,255,.4)';
      cx.lineWidth = 1.2;
      cx.beginPath();
      cx.arc(bx, by, br, 0, Math.PI * 2);
      cx.stroke();
    }
  } else if (bgKind === 'zonsondergang') {
    for (const band of [[0.18, 0.22, 90], [0.35, 0.30, 130], [0.55, 0.18, 70]]) {
      const drift = reducedMotion ? 0 : ((t * 6) % (W + 600)) - 300;
      const y = H * band[0];
      const rg = cx.createRadialGradient(W * 0.5 + drift - W * 0.3, y, 0, W * 0.5 + drift - W * 0.3, y, W * band[1]);
      rg.addColorStop(0, `rgba(255,220,190,${(band[2] / 900).toFixed(3)})`);
      rg.addColorStop(1, 'rgba(255,220,190,0)');
      cx.fillStyle = rg;
      cx.fillRect(0, 0, W, H);
    }
  } else if (bgKind === 'gamewereld') {
    for (const rib of [[.18,.12,'rgba(32,230,139,.16)'],[.45,.18,'rgba(110,120,255,.13)'],[.72,.15,'rgba(255,100,210,.10)']]) {
      cx.strokeStyle=rib[2];cx.lineWidth=Math.max(10,W*.018);cx.beginPath();
      for(let x=0;x<=W;x+=W/28){const y=H*(rib[0]+Math.sin(t*.7+x*.003+rib[1]*8)*.035+x/W*.08);if(x===0)cx.moveTo(x,y);else cx.lineTo(x,y)}cx.stroke();
    }
    const rnd2=seededRandom(991);for(let i=0;i<34;i++){const sp=.025+rnd2()*.055,yy=(rnd2()+t*sp)%1,x=(rnd2()+Math.sin(t+i)*.015)*W,y=yy*H*.75;cx.fillStyle=i%3?'rgba(32,230,139,.55)':'rgba(255,214,107,.55)';cx.beginPath();cx.arc(x,y,1+rnd2()*2.8,0,Math.PI*2);cx.fill()}
    const glow=cx.createRadialGradient(W*.5,H*.78,0,W*.5,H*.78,W*.42);glow.addColorStop(0,'rgba(32,230,139,.12)');glow.addColorStop(1,'rgba(0,0,0,0)');cx.fillStyle=glow;cx.fillRect(0,0,W,H);
  }
  cx.globalCompositeOperation = 'source-over';
  cx.restore();
}
function initSegWorker() {
  if (segReady) return Promise.resolve(true);
  if (segInitPromise) return segInitPromise;
  segInitPromise = new Promise(resolve => {
    let settled = false;
    const finish = ok => {
      if (settled) return;
      settled = true;
      clearTimeout(timeout);
      resolve(ok);
    };
    let timeout;
    try {
      segWorker = new Worker('./segment-worker.js');
    } catch (error) {
      segError = String(error && error.message || error);
      resolve(false);
      return;
    }
    timeout = setTimeout(() => {
      segError = 'segmentatie laadt te langzaam';
      finish(false);
    }, 20000);
    segWorker.onmessage = event => {
      const msg = event.data || {};
      if (msg.type === 'ready') {
        segReady = true;
        segDelegate = msg.delegate || '';
        segError = '';
        finish(true);
        return;
      }
      if (msg.type === 'init-error') {
        segError = msg.error || 'segmentatie-startfout';
        finish(false);
        return;
      }
      if (msg.type !== 'result') return;
      segPending = false;
      if (!msg.ok || !msg.mask) return;
      segInferenceAvg = segInferenceAvg ? segInferenceAvg * 0.82 + msg.inferenceMs * 0.18 : msg.inferenceMs;
      personCoverage = msg.personCoverage || 0;
      const facePresent = face.ready && (performance.now() - (face.lastSeen || 0) < 1500);
      segWatchdogTick(personCoverage, facePresent);
      personMask = {
        data: new Uint8Array(msg.mask),
        width: msg.width,
        height: msg.height,
        timestamp: performance.now()
      };
      renderPersonMaskCanvas();
    };
    segWorker.onerror = event => {
      segError = String(event.message || 'segmentworker-fout');
      segPending = false;
      finish(false);
    };
    try {
      segWorker.postMessage({ type: 'init', delegate: segDelegateForced || 'AUTO' });
    } catch (error) {
      segError = String(error && error.message || error);
      finish(false);
    }
  });
  return segInitPromise;
}
function segWatchdogTick(coverage, facePresent) {
  if (facePresent && coverage < 0.015) segLowCoverageStreak++;
  else segLowCoverageStreak = 0;
  if (segLowCoverageStreak >= 6 && segReinitAttempts < 2 && !segReiniting) {
    segReinitAttempts++;
    const alternate = segDelegate === 'GPU' ? 'CPU' : 'GPU';
    reinitSegWorker(alternate);
    return true;
  }
  return false;
}
function reinitSegWorker(delegate) {
  if (segReiniting) return;
  segReiniting = true;
  segDelegateForced = delegate;
  try { if (segWorker) segWorker.postMessage({ type: 'close' }); } catch (_) {}
  try { if (segWorker) segWorker.terminate(); } catch (_) {}
  segWorker = null;
  segReady = false;
  segPending = false;
  segInitPromise = null;
  personMask = null;
  personCoverage = 0;
  segLowCoverageStreak = 0;
  initSegWorker().finally(() => { segReiniting = false; });
}
function renderPersonMaskCanvas() {
  if (!personMask || !personMask.data || !personMask.width || !personMask.height) return;
  const w = personMask.width;
  const h = personMask.height;
  if (personMaskCanvas.width !== w) personMaskCanvas.width = w;
  if (personMaskCanvas.height !== h) personMaskCanvas.height = h;
  const mctx = personMaskCanvas.getContext('2d');
  const img = mctx.createImageData(w, h);
  const src = personMask.data;
  for (let i = 0; i < src.length; i++) {
    const shaped = clamp((src[i] - 28) / 170);
    const o = i * 4;
    img.data[o] = 255;
    img.data[o + 1] = 255;
    img.data[o + 2] = 255;
    img.data[o + 3] = Math.round(Math.pow(shaped, 0.82) * 255);
  }
  mctx.putImageData(img, 0, 0);
}
function sendSegmentation(now) {
  if (bgKind === 'camera' || !segReady || segPending || vid.readyState < 2 || !running) return;
  const interval = Math.max(140, segInferenceAvg ? segInferenceAvg * 1.8 : 0);
  if (now - lastSegSent < interval) return;
  lastSegSent = now;
  segPending = true;
  const seq = ++segSeq;
  const scale = Math.min(1, 256 / Math.max(vid.videoWidth || 1, vid.videoHeight || 1));
  const resizeWidth = Math.max(2, Math.round((vid.videoWidth || 2) * scale));
  const resizeHeight = Math.max(2, Math.round((vid.videoHeight || 2) * scale));
  let bitmapPromise;
  try {
    bitmapPromise = createImageBitmap(vid, { resizeWidth, resizeHeight, resizeQuality: 'low' }).catch(() => createImageBitmap(vid));
  } catch (_) {
    bitmapPromise = createImageBitmap(vid);
  }
  bitmapPromise.then(bitmap => {
    try {
      segWorker.postMessage({ type: 'segment', image: bitmap, timestamp: now, seq }, [bitmap]);
    } catch (_) {
      if (bitmap.close) bitmap.close();
      segPending = false;
    }
  }).catch(() => { segPending = false; });
}
function paintPersonCutout(now) {
  const W = els.cv.width;
  const H = els.cv.height;
  if (!personMask || !personMaskCanvas.width || now - personMask.timestamp > 900 || vid.readyState < 2) return false;
  if (personCanvas.width !== W) personCanvas.width = W;
  if (personCanvas.height !== H) personCanvas.height = H;
  const pc = personCanvas.getContext('2d');
  pc.clearRect(0, 0, W, H);
  pc.globalCompositeOperation = 'source-over';
  pc.save();
  if (els.mirror.checked) {
    pc.translate(W, 0);
    pc.scale(-1, 1);
  }
  pc.drawImage(vid, 0, 0, W, H);
  pc.restore();
  pc.globalCompositeOperation = 'destination-in';
  pc.save();
  if (els.mirror.checked) {
    pc.translate(W, 0);
    pc.scale(-1, 1);
  }
  pc.drawImage(personMaskCanvas, 0, 0, W, H);
  pc.restore();
  pc.globalCompositeOperation = 'source-over';
  const age = now - personMask.timestamp;
  const alpha = age < 520 ? 1 : Math.max(0.35, 1 - (age - 520) / 380);
  cx.save();
  cx.globalAlpha = alpha;
  if ($('regieAuto') && $('regieAuto').checked) {
    const light = face.light || .55;
    const bright = clamp(1 + (.56 - light) * .62, .90, 1.24);
    const contr = clamp(1.04 + (.50 - light) * .14, .98, 1.12);
    cx.filter = `brightness(${bright.toFixed(3)}) contrast(${contr.toFixed(3)}) saturate(1.08)`;
  }
  if(bgKind==='gamewereld'){const vc=virtualCam(now);cx.translate(W/2+vc.x,H/2+vc.y);cx.scale(vc.scale,vc.scale);cx.translate(-W/2,-H/2)}
  cx.drawImage(personCanvas, 0, 0);
  cx.filter = 'none';
  cx.restore();
  return true;
}
const FX_SPECS = {
  sterren: { count: 45, dir: 1, speed: [0.03, 0.09], size: [1.4, 3.4], colors: ['#fff7c2', '#cdeaff', '#ffffff'], shape: 'star', sway: 0.010 },
  confetti: { count: 70, dir: 1, speed: [0.10, 0.22], size: [3.5, 7], colors: ['#ff5b9a', '#ffd23f', '#3fd68f', '#29cbff', '#b53fff', '#ff8a3d'], shape: 'rect', sway: 0.02 },
  sneeuw: { count: 60, dir: 1, speed: [0.05, 0.12], size: [1.6, 4.2], colors: ['#ffffff'], shape: 'circle', sway: 0.03 },
  bubbels: { count: 40, dir: -1, speed: [0.06, 0.15], size: [2.5, 6.5], colors: ['#bfeaff'], shape: 'bubble', sway: 0.014 }
};
let lastFxTime = 0;
function spawnParticle(spec, randomY) {
  return {
    x: Math.random(),
    y: randomY ? Math.random() : (spec.dir === 1 ? -0.05 : 1.05),
    vy: (spec.speed[0] + Math.random() * (spec.speed[1] - spec.speed[0])) * spec.dir,
    size: spec.size[0] + Math.random() * (spec.size[1] - spec.size[0]),
    color: spec.colors[Math.floor(Math.random() * spec.colors.length)],
    phase: Math.random() * Math.PI * 2,
    spin: Math.random() * Math.PI * 2,
    spinV: (Math.random() - 0.5) * 5
  };
}
function drawEffects(now) {
  if (fxKind === 'uit' || !FX_SPECS[fxKind]) {
    particles = [];
    lastFxTime = now;
    return;
  }
  const spec = FX_SPECS[fxKind];
  const W = els.cv.width;
  const H = els.cv.height;
  const dt = Math.min(0.1, Math.max(0.001, (now - lastFxTime) / 1000));
  lastFxTime = now;
  const motionScale = reducedMotion ? 0.4 : 1;
  const target = Math.round(spec.count * (reducedMotion ? 0.4 : 1));
  while (particles.length < target) particles.push(spawnParticle(spec, true));
  if (particles.length > target) particles = particles.slice(0, target);
  cx.save();
  for (const p of particles) {
    p.y += p.vy * dt * motionScale;
    p.phase += dt * 2;
    p.spin += p.spinV * dt;
    const x = (p.x + Math.sin(p.phase) * spec.sway) * W;
    const y = p.y * H;
    if ((spec.dir === 1 && p.y > 1.08) || (spec.dir === -1 && p.y < -0.08)) {
      Object.assign(p, spawnParticle(spec, false));
      continue;
    }
    if (spec.shape === 'rect') {
      cx.save();
      cx.translate(x, y);
      cx.rotate(p.spin);
      cx.fillStyle = p.color;
      cx.globalAlpha = 0.9;
      cx.fillRect(-p.size / 2, -p.size / 3, p.size, p.size * 0.66);
      cx.restore();
    } else if (spec.shape === 'bubble') {
      cx.strokeStyle = p.color;
      cx.globalAlpha = 0.55;
      cx.lineWidth = 1.4;
      cx.beginPath();
      cx.arc(x, y, p.size, 0, Math.PI * 2);
      cx.stroke();
      cx.globalAlpha = 0.8;
      cx.beginPath();
      cx.arc(x - p.size * 0.3, y - p.size * 0.3, p.size * 0.22, 0, Math.PI * 2);
      cx.fillStyle = '#ffffff';
      cx.fill();
    } else if (spec.shape === 'star') {
      const tw = 0.45 + 0.55 * Math.abs(Math.sin(p.phase * 1.7));
      cx.globalAlpha = tw;
      cx.fillStyle = p.color;
      cx.beginPath();
      cx.arc(x, y, p.size * 0.6, 0, Math.PI * 2);
      cx.fill();
      cx.globalAlpha = tw * 0.5;
      cx.fillRect(x - p.size * 1.6, y - 0.6, p.size * 3.2, 1.2);
      cx.fillRect(x - 0.6, y - p.size * 1.6, 1.2, p.size * 3.2);
    } else {
      cx.globalAlpha = 0.85;
      cx.fillStyle = p.color;
      cx.beginPath();
      cx.arc(x, y, p.size, 0, Math.PI * 2);
      cx.fill();
    }
  }
  cx.restore();
  cx.globalAlpha = 1;
}
function selectBackground(key, button) {
  if (!['camera', 'ruimte', 'onderwater', 'zonsondergang', 'gamewereld', 'eigen'].includes(key)) return;
  bgKind = key;
  document.querySelectorAll('.bg').forEach(x => x.classList.remove('act'));
  if (button) button.classList.add('act');
  const bgButton = document.querySelector(`.bg[data-b="${key}"]`);
  if (bgButton) bgButton.classList.add('act');
  if (key === 'gamewereld' && kar !== 'zelf') selectCharacter('zelf', document.querySelector('.kar[data-k="zelf"]'));
  if (key !== 'camera') {
    setStatus('achtergrondmagie wordt geladen…');
    initSegWorker().then(ok => {
      if (bgKind === 'camera') return;
      setStatus(ok ? `achtergrond “${key}” actief — jouw kamer blijft onzichtbaar` : 'achtergrondmagie lukte niet; camera valt terug');
    });
  } else {
    setStatus('gewone camera-achtergrond');
  }
  saveSettings();
}
function selectEffect(key, button) {
  if (key !== 'uit' && !FX_SPECS[key]) return;
  fxKind = key;
  particles = [];
  document.querySelectorAll('.fx').forEach(x => x.classList.remove('act'));
  if (button) button.classList.add('act');
  const fxButton = document.querySelector(`.fx[data-f="${key}"]`);
  if (fxButton) fxButton.classList.add('act');
  saveSettings();
}
async function handleBgUploadFile(file) {
  if (!file) return false;
  const isSvg = /\.svg$/i.test(file.name) || file.type === 'image/svg+xml';
  const okType = /^image\/(png|webp|jpeg|jpg|gif|svg\+xml)$/.test(file.type) || /\.(png|webp|jpe?g|gif|svg)$/i.test(file.name);
  if (!okType || file.size > 10 * 1024 * 1024) {
    setStatus('dit bestand kan niet als achtergrond worden gebruikt');
    return false;
  }
  let url = '';
  try {
    if (isSvg) {
      const clean = sanitizeSvgText(await file.text());
      if (!clean) {
        setStatus('deze SVG is niet veilig genoeg voor een achtergrond');
        return false;
      }
      url = URL.createObjectURL(new Blob([clean], { type: 'image/svg+xml' }));
    } else {
      url = URL.createObjectURL(file);
    }
  } catch (_) {
    setStatus('dit bestand kan niet als achtergrond worden gebruikt');
    return false;
  }
  objectUrls.push(url);
  return new Promise(resolve => {
    const img = new Image();
    img.onload = () => {
      if (!img.naturalWidth || !img.naturalHeight || img.naturalWidth > 4096 || img.naturalHeight > 4096) {
        setStatus('dit beeld is te groot of ongeldig voor een achtergrond');
        resolve(false);
        return;
      }
      bgImage = img;
      bgImageLabel = file.name.replace(/\.[^.]+$/, '').slice(0, 24);
      let eigBtn = document.querySelector('.bg[data-b="eigen"]');
      if (!eigBtn) {
        eigBtn = document.createElement('button');
        eigBtn.className = 'bg';
        eigBtn.dataset.b = 'eigen';
        eigBtn.type = 'button';
        const icon = document.createElement('span');
        icon.textContent = '🖼';
        eigBtn.append(icon, document.createTextNode(' eigen'));
        eigBtn.onclick = () => selectBackground('eigen', eigBtn);
        const row = document.getElementById('bgs');
        const uploadLabel = row ? row.querySelector('label.upload') : null;
        if (row) row.insertBefore(eigBtn, uploadLabel || null);
      }
      selectBackground('eigen');
      setStatus('eigen achtergrond actief — jij staat er nu echt in');
      resolve(true);
    };
    img.onerror = () => {
      setStatus('dit bestand kan niet als achtergrond worden gebruikt');
      resolve(false);
    };
    img.src = url;
  });
}
function paintVideo(blur = false) {
  const W = els.cv.width;
  const H = els.cv.height;
  cx.save();
  cx.clearRect(0, 0, W, H);
  cx.fillStyle = '#000';
  cx.fillRect(0, 0, W, H);
  if (vid.readyState >= 2) {
    if (blur) cx.filter = 'blur(15px) saturate(.68)';
    if (els.mirror.checked) {
      cx.translate(W, 0);
      cx.scale(-1, 1);
      cx.drawImage(vid, 0, 0, W, H);
    } else {
      cx.drawImage(vid, 0, 0, W, H);
    }
    cx.filter = 'none';
  }
  cx.restore();
}
function characterImageReady(def) {
  return def && def.img && def.img.complete && def.img.naturalWidth > 0 && def.img.naturalHeight > 0;
}
function maskMetrics() {
  const def = CHARACTERS[kar];
  if (!def || !face.ready) return null;
  const f = fit();
  const eyeD = Math.max(10, face.d0);
  const effectiveEyeDist = Math.max(0.04, def.eyeDist);
  const spriteW = eyeD / effectiveEyeDist * (def.scale || 1) * f.sc;
  const aspect = characterImageReady(def) ? def.img.naturalHeight / def.img.naturalWidth : 1;
  const spriteH = spriteW * aspect;
  const cxp = face.x0 + f.ox * eyeD;
  const cyp = face.y0 + f.oy * eyeD;
  return { def, f, eyeD, spriteW, spriteH, cxp, cyp };
}
let blendOverride = null;
function expressionValues() {
  const blend = { ...(face.blend || {}) };
  if (blendOverride && performance.now() < blendOverride.until) Object.assign(blend, blendOverride.vals);
  const jaw = clamp(blend.jawOpen || 0);
  const smile = clamp(((blend.mouthSmileLeft || 0) + (blend.mouthSmileRight || 0)) / 2);
  const blink = clamp(((blend.eyeBlinkLeft || 0) + (blend.eyeBlinkRight || 0)) / 2);
  const brow = clamp(blend.browInnerUp || 0);
  return { jaw, smile, blink, brow };
}
function drawMask(now) {
  if (kar === 'zelf') return;
  const alpha = privacyAlpha(now);
  if (alpha <= 0.01) {
    if (lastMaskMetrics) lastMaskMetrics = { ...lastMaskMetrics, alpha: 0 };
    return;
  }
  const m = maskMetrics();
  if (!m || !characterImageReady(m.def)) return;
  lastMaskMetrics = {
    x: m.cxp, y: m.cyp, width: m.spriteW, height: m.spriteH,
    eyeDistance: m.eyeD, anchorX: m.def.eyeCX * m.spriteW, anchorY: m.def.eyeY * m.spriteH,
    character: kar, alpha, yaw: face.yaw, pitch: face.pitch
  };
  updateLight(now);
  const exv = expressionValues();
  const voice = exv.jaw > 0.025 ? exv.jaw * 0.58 : micLevel * 0.38;
  const talk = clamp(voice, 0, 0.8);
  const motionAmount = reducedMotion ? 0 : 1;
  const breathe = Math.sin(now / 640) * 0.0075 * motionAmount;
  const sway = Math.sin(now / 940) * 0.005 * motionAmount;
  const squashX = 1 + breathe + (exv.smile * 0.035 + talk * 0.012) * motionAmount;
  const squashY = 1 - breathe * 0.7 + (talk * 0.07 + exv.brow * 0.012 - exv.blink * 0.006) * motionAmount;
  const playfulTilt = sway + (talk * Math.sin(now / 58) * 0.012 + exv.smile * 0.008) * motionAmount;
  const brightness = (0.84 + face.light * 0.30).toFixed(3);
  const contrast = (0.93 + face.light * 0.13).toFixed(3);

  cx.save();
  cx.globalAlpha = alpha;
  cx.translate(m.cxp, m.cyp);
  cx.rotate(face.r0 + playfulTilt);
  const yaw = els.mirror.checked ? -face.yaw : face.yaw;
  const pose = yawPitchCoefficients(yaw, face.pitch);
  cx.transform(pose.r00, pose.r10, pose.r01, pose.r11, 0, 0);
  cx.scale(squashX, squashY);
  if (els.lightMatch.checked) cx.filter = `brightness(${brightness}) contrast(${contrast})`;
  const anchorX = m.spriteW * m.def.eyeCX;
  const anchorY = m.spriteH * m.def.eyeY;
  cx.drawImage(m.def.img, -anchorX, -anchorY, m.spriteW, m.spriteH);
  cx.filter = 'none';
  drawMouthCavity(m, anchorX, anchorY, exv.jaw);
  cx.restore();
}
// pratende mond: zichtbare mondholte die mee-opent met de kaak van de gebruiker.
// De kale squash-vervorming was in de praktijk onzichtbaar (max ~2-5%) — deze poppenmond
// is de eerlijke lichte cartoon-oplossing binnen statische PNG-sprites.
let jawSmoothed = 0;
function drawMouthCavity(m, anchorX, anchorY, jawRaw) {
  const target = clamp(jawRaw || 0, 0, 1);
  jawSmoothed += (target - jawSmoothed) * (target > jawSmoothed ? 0.5 : 0.22);
  const jaw = jawSmoothed;
  if (jaw < 0.045) return;
  const mouth = (m.def && m.def.mouth) || DEFAULT_MOUTH;
  const mxp = mouth.x * m.spriteW - anchorX;
  const myp = mouth.y * m.spriteH - anchorY;
  const halfW = mouth.w * m.spriteW * (0.85 + jaw * 0.30) * 0.5;
  const halfH = Math.max(halfW * 0.25, mouth.w * m.spriteW * 0.62 * jaw * 0.5);
  cx.save();
  cx.translate(mxp, myp);
  const grad = cx.createRadialGradient(0, -halfH * 0.1, halfH * 0.15, 0, 0, Math.max(halfW, halfH) * 1.15);
  grad.addColorStop(0, '#6e1626');
  grad.addColorStop(0.55, '#420a16');
  grad.addColorStop(1, '#24040c');
  cx.fillStyle = grad;
  cx.beginPath();
  cx.ellipse(0, 0, halfW, halfH, 0, 0, Math.PI * 2);
  cx.fill();
  if (jaw > 0.30) {
    cx.fillStyle = 'rgba(196,64,79,0.9)';
    cx.beginPath();
    cx.ellipse(0, halfH * 0.42, halfW * 0.52, halfH * 0.38, 0, 0, Math.PI * 2);
    cx.fill();
  }
  cx.restore();
}
let hostMsg='Klaar voor een sterke stream!',hostUntil=performance.now()+5000,worldBursts=[],lastAutoHost=0;
function triggerCohost(type='hype'){
 const texts={hype:['🔥 Dit is een sterk moment!','Laten we gaan, G2B!','Energie omhoog — mooi werk!'],vraag:['❓ Chat: hoe zou jij dit gebruiken?','❓ Welke tool wil je nu live zien?','❓ Durf jij deze workflow te proberen?'],event:['🎉 Wereld-event actief!','✨ De wereld reageert op je stream!','🚀 Nieuwe energie in de wereld!']};
 const arr=texts[type]||texts.hype;hostMsg=arr[Math.floor(Math.random()*arr.length)];hostUntil=performance.now()+5200;
 if(type==='event'){selectEffect('confetti');for(let i=0;i<8;i++)worldBursts.push({x:.18+Math.random()*.64,y:.2+Math.random()*.5,t:performance.now(),hue:120+Math.random()*160})}
 drawFrame();return {type,message:hostMsg}
}
function drawWorldBursts(now){worldBursts=worldBursts.filter(b=>now-b.t<1800);for(const b of worldBursts){const a=1-(now-b.t)/1800,r=(1-a)*90+8,x=b.x*els.cv.width,y=b.y*els.cv.height;cx.save();cx.globalAlpha=a;cx.strokeStyle=`hsla(${b.hue},95%,62%,.9)`;cx.lineWidth=3;cx.beginPath();cx.arc(x,y,r,0,Math.PI*2);cx.stroke();cx.fillStyle=`hsla(${b.hue},95%,72%,.7)`;for(let i=0;i<7;i++){const ang=i/7*Math.PI*2+now/300;cx.beginPath();cx.arc(x+Math.cos(ang)*r,y+Math.sin(ang)*r,3,0,Math.PI*2);cx.fill()}cx.restore()}}
function roundRect(g,x,y,w,h,r){g.beginPath();g.moveTo(x+r,y);g.arcTo(x+w,y,x+w,y+h,r);g.arcTo(x+w,y+h,x,y+h,r);g.arcTo(x,y+h,x,y,r);g.arcTo(x,y,x+w,y,r);g.closePath()}
function drawCohost(now){
 if(bgKind!=='gamewereld'||!($('cohostOn')&&$('cohostOn').checked))return;
 const W=els.cv.width,H=els.cv.height,x=W*.82,y=H*.76+Math.sin(now/380)*H*.008,s=Math.min(W,H)*.105;cx.save();
 const glow=cx.createRadialGradient(x,y,0,x,y,s*2.1);glow.addColorStop(0,'rgba(32,230,139,.30)');glow.addColorStop(1,'rgba(32,230,139,0)');cx.fillStyle=glow;cx.fillRect(x-s*2,y-s*2,s*4,s*4);
 cx.translate(x,y);cx.fillStyle='#12253f';roundRect(cx,-s*.62,-s*.72,s*1.24,s*1.42,s*.28);cx.fill();cx.strokeStyle='#20e68b';cx.lineWidth=3;cx.stroke();
 cx.fillStyle='#dffef0';cx.beginPath();cx.arc(-s*.24,-s*.26,s*.09,0,Math.PI*2);cx.arc(s*.24,-s*.26,s*.09,0,Math.PI*2);cx.fill();cx.fillStyle='#20e68b';cx.fillRect(-s*.30,s*.08,s*.6,Math.max(3,s*.06+Math.sin(now/90)*s*.025));
 cx.strokeStyle='#ffd66b';cx.lineWidth=4;cx.beginPath();cx.moveTo(-s*.62,-s*.1);cx.lineTo(-s*1.05,-s*.44+Math.sin(now/180)*s*.12);cx.moveTo(s*.62,-s*.1);cx.lineTo(s*1.05,-s*.40+Math.cos(now/190)*s*.12);cx.stroke();
 cx.restore();
 if(now<hostUntil){const fs=Math.max(14,Math.round(W*.018));cx.font='900 '+fs+'px system-ui';const tw=Math.min(W*.43,cx.measureText(hostMsg).width+34),bx=W*.56-tw/2,by=H*.12;cx.fillStyle='rgba(3,15,10,.82)';roundRect(cx,bx,by,tw,fs*2.15,14);cx.fill();cx.strokeStyle='rgba(32,230,139,.8)';cx.stroke();cx.fillStyle='#eafff4';cx.textAlign='center';cx.fillText(hostMsg,W*.56,by+fs*1.42)}
}
function drawFrame() {
  const now = performance.now();
  updateLight(now);
  if(bgKind==='gamewereld'&&($('camMode')&&$('camMode').value)==='regisseur'&&($('cohostOn')&&$('cohostOn').checked)&&micLevel>.55&&now-lastAutoHost>18000){lastAutoHost=now;triggerCohost('hype')}
  const faceAge = face.ready ? now - face.lastSeen : Infinity;
  const blurForPrivacy = els.privacyMode.value === 'blur' && faceAge > 170;
  if (bgKind === 'camera') {
    paintVideo(blurForPrivacy);
  } else {
    drawSceneBackground(now);
    if (!paintPersonCutout(now) && vid.readyState >= 2) {
      paintVideo(blurForPrivacy);
    }
  }
  drawMask(now);
  drawWorldBursts(now);
  drawCohost(now);
  drawEffects(now);
}

function initWorker() {
  if (workerReady) return Promise.resolve();
  if (workerInitPromise) return workerInitPromise;
  workerInitPromise = new Promise((resolve, reject) => {
    let settled = false;
    workerError = '';
    updateModelBadge();
    try { worker = new Worker('./face-worker.js'); } catch (error) {
      workerError = String(error && error.message || error);
      updateModelBadge();
      reject(error);
      return;
    }
    const timeout = setTimeout(() => {
      if (!settled) {
        settled = true;
        workerError = 'timeout bij laden trackingmodel';
        updateModelBadge();
        reject(new Error(workerError));
      }
    }, 25000);
    worker.onerror = event => {
      workerError = event.message || 'workerfout';
      if (!settled) {
        settled = true;
        clearTimeout(timeout);
        updateModelBadge();
        reject(new Error(workerError));
      }
    };
    worker.onmessage = event => {
      const msg = event.data || {};
      if (msg.type === 'ready') {
        workerReady = true;
        workerDelegate = msg.delegate || '';
        workerError = '';
        updateModelBadge();
        if (!settled) {
          settled = true;
          clearTimeout(timeout);
          resolve();
        }
        return;
      }
      if (msg.type === 'init-error') {
        workerError = msg.error || 'init-error';
        updateModelBadge();
        if (!settled) {
          settled = true;
          clearTimeout(timeout);
          reject(new Error(workerError));
        }
        return;
      }
      if (msg.type === 'result') handleDetectionResult(msg);
    };
    worker.postMessage({ type: 'init', delegate: 'AUTO' });
  }).catch(error => {
    workerInitPromise = null;
    throw error;
  });
  return workerInitPromise;
}
function restartTrackingWorker() {
  if (workerRestarting) return;
  workerRestarting = true;
  workerReady = false;
  detectionPending = false;
  updateModelBadge();
  try { if (worker) worker.terminate(); } catch (_) {}
  worker = null;
  workerInitPromise = null;
  initWorker().catch(() => {}).finally(() => { workerRestarting = false; });
}
function sendDetection(now) {
  if (!workerReady || vid.readyState < 2) return;
  if (detectionPending) {
    if (now - lastDetectSent < 1500) return;
    detectionPending = false;
    detectionTimeouts++;
    if (detectionTimeouts >= 2) {
      detectionTimeouts = 0;
      restartTrackingWorker();
      return;
    }
  }
  const p = profile();
  effectiveDetectMs = Math.max(p.detectMs, inferenceAvg > 55 ? inferenceAvg * 1.22 : 0);
  if (now - lastDetectSent < effectiveDetectMs) return;
  lastDetectSent = now;
  detectionPending = true;
  const seq = ++detectionSeq;
  const scale = Math.min(1, p.trackLongSide / Math.max(vid.videoWidth || 1, vid.videoHeight || 1));
  const resizeWidth = Math.max(2, Math.round((vid.videoWidth || 2) * scale));
  const resizeHeight = Math.max(2, Math.round((vid.videoHeight || 2) * scale));
  lastTrackInput = { width: resizeWidth, height: resizeHeight };
  let bitmapPromise;
  try {
    bitmapPromise = createImageBitmap(vid, { resizeWidth, resizeHeight, resizeQuality: 'low' }).catch(() => createImageBitmap(vid));
  } catch (_) {
    bitmapPromise = createImageBitmap(vid);
  }
  bitmapPromise.then(bitmap => {
    try { worker.postMessage({ type: 'detect', image: bitmap, timestamp: now, seq }, [bitmap]); }
    catch (_) {
      if (bitmap.close) bitmap.close();
      detectionPending = false;
    }
  }).catch(() => { detectionPending = false; });
}
function updatePerf(now) {
  frameCount++;
  if (now - lastPerfUpdate < 1000) return;
  const fps = frameCount * 1000 / (now - lastPerfUpdate);
  const detFps = detectionCount * 1000 / (now - lastPerfUpdate);
  let perfText = running ? `${fps.toFixed(0)} fps · ${detFps.toFixed(0)} Hz · ${inferenceAvg ? inferenceAvg.toFixed(1) : '–'} ms` : '';
  if (running && bgKind !== 'camera') {
    perfText += ` · seg ${(personCoverage * 100).toFixed(0)}% ${segInferenceAvg ? segInferenceAvg.toFixed(0) + 'ms' : '–'} ${segDelegate || '…'}${segReinitAttempts ? ' h' + segReinitAttempts : ''}`;
  }
  els.perf.textContent = perfText;
  frameCount = 0;
  detectionCount = 0;
  lastPerfUpdate = now;
}
let frameTimeAvg = 0;
function renderTick(now) {
  if (bgKind !== 'camera' && !segReady && !segInitPromise) initSegWorker();
  const frameStart = performance.now();
  drawFrame();
  const frameMs = performance.now() - frameStart;
  frameTimeAvg = frameTimeAvg ? frameTimeAvg * 0.9 + frameMs * 0.1 : frameMs;
  sendDetection(now);
  sendSegmentation(now);
  updatePerf(now);
  if (recorder && recorder.state === 'recording' && recordStartedAt && now - lastRecordTimerUpdate > 500) {
    const total = Math.max(0, Math.floor((now - recordStartedAt) / 1000));
    els.rec.textContent = `⏹ stop ${Math.floor(total / 60)}:${String(total % 60).padStart(2, '0')}`;
    lastRecordTimerUpdate = now;
  }
}
function loop(now, token = runToken) {
  if (!running || token !== runToken) return;
  renderTick(now);
  if (now - lastTrackUiUpdate > 350) {
    if (lastFaceTime && now - lastFaceTime > 500) updateTrackPill('search');
    lastTrackUiUpdate = now;
  }
  requestAnimationFrame(next => loop(next, token));
}
function syncHiddenCapturePump() {
  const needed = document.hidden && running && recorder && recorder.state === 'recording';
  if (needed && hiddenCaptureTimer === null) {
    hiddenCaptureTimer = setInterval(() => {
      if (!document.hidden || !running || !recorder || recorder.state !== 'recording') {
        syncHiddenCapturePump();
        return;
      }
      renderTick(performance.now());
    }, 33);
  } else if (!needed && hiddenCaptureTimer !== null) {
    clearInterval(hiddenCaptureTimer);
    hiddenCaptureTimer = null;
  }
}
document.addEventListener('visibilitychange', syncHiddenCapturePump);

function fillDeviceSelect(select, devices) {
  if (!select) return;
  const previous = select.value;
  select.innerHTML = '';
  const auto = document.createElement('option');
  auto.value = '';
  auto.textContent = 'automatisch / standaard';
  select.appendChild(auto);
  devices.forEach((device, index) => {
    const option = document.createElement('option');
    option.value = device.deviceId || '';
    option.textContent = device.label || `naam wordt zichtbaar na toestemming ${index + 1}`;
    select.appendChild(option);
  });
  if ([...select.options].some(o => o.value === previous)) select.value = previous;
}
async function refreshDeviceSelectors() {
  if (!navigator.mediaDevices || !navigator.mediaDevices.enumerateDevices) return { video: [], audio: [] };
  try {
    const devices = await navigator.mediaDevices.enumerateDevices();
    const video = devices.filter(d => d.kind === 'videoinput');
    const audio = devices.filter(d => d.kind === 'audioinput');
    fillDeviceSelect(els.cameraSelect, video);
    fillDeviceSelect(els.micSelect, audio);
    return { video, audio };
  } catch (_) { return { video: [], audio: [] }; }
}
if (navigator.mediaDevices && navigator.mediaDevices.addEventListener) {
  navigator.mediaDevices.addEventListener('devicechange', () => { refreshDeviceSelectors(); });
}
function buildMediaConstraints() {
  const p = profile();
  const video = {
    width: { ideal: Math.min(1280, p.longSide) },
    height: { ideal: 720 },
    frameRate: { ideal: 30, max: 30 },
    facingMode: { ideal: 'user' }
  };
  if (els.cameraSelect.value) video.deviceId = { exact: els.cameraSelect.value };
  let audio = false;
  if (els.micOn.checked) {
    audio = { echoCancellation: true, noiseSuppression: true, autoGainControl: true };
    if (els.micSelect.value) audio.deviceId = { exact: els.micSelect.value };
  }
  return { video, audio };
}
async function getMediaWithOptionalMic() {
  const constraints = buildMediaConstraints();
  if (!constraints.audio) {
    return { stream: await navigator.mediaDevices.getUserMedia(constraints), micFallback: false };
  }
  try {
    return { stream: await navigator.mediaDevices.getUserMedia(constraints), micFallback: false };
  } catch (firstError) {
    try {
      const videoOnly = await navigator.mediaDevices.getUserMedia({ video: constraints.video, audio: false });
      return { stream: videoOnly, micFallback: true, firstError };
    } catch (_) {
      throw firstError;
    }
  }
}
function setupAudio(inputStream) {
  teardownAudio();
  if (!els.micOn.checked || !inputStream || !inputStream.getAudioTracks().length) return;
  try {
    audioContext = new (window.AudioContext || window.webkitAudioContext)();
    if (audioContext.state === 'suspended') audioContext.resume().catch(() => {});
    const analyser = audioContext.createAnalyser();
    analyser.fftSize = 256;
    audioContext.createMediaStreamSource(inputStream).connect(analyser);
    const buf = new Uint8Array(analyser.fftSize);
    const ownedContext = audioContext;
    const tick = () => {
      if (!running || audioContext !== ownedContext) return;
      analyser.getByteTimeDomainData(buf);
      let peak = 0;
      for (let i = 0; i < buf.length; i++) peak = Math.max(peak, Math.abs(buf[i] - 128));
      micLevel += (Math.min(1, peak / 55) - micLevel) * 0.22;
      requestAnimationFrame(tick);
    };
    tick();
  } catch (_) { micLevel = 0; }
}
function teardownAudio() {
  if (audioContext) {
    try { audioContext.close(); } catch (_) {}
  }
  audioContext = null;
  micLevel = 0;
}
function stopActiveStream() {
  if (stream) {
    for (const track of stream.getTracks()) {
      try { track.stop(); } catch (_) {}
    }
  }
  stream = null;
  vid.srcObject = null;
}
function updateStartButton() {
  els.start.classList.toggle('stop', running);
  els.start.textContent = running ? '⏹ STOP WEBCAM' : '📷 START JE WEBCAM';
}
async function openCamera(forceRestart = false) {
  if (running || (restartingMedia && !forceRestart)) return;
  els.start.disabled = true;
  setOverlay(true, 'Camera starten', 'het lokale trackingmodel en je webcam worden voorbereid…', 'loading');
  setStatus('camera starten…');
  try {
    if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) throw new Error('NO_SUPPORT');
    await initWorker().catch(error => { throw new Error('MODEL:' + error.message); });
    const media = await getMediaWithOptionalMic();
    stream = media.stream;
    const videoTrack = stream.getVideoTracks()[0];
    if (videoTrack) {
      videoTrack.addEventListener('ended', () => { if (running) stopCamera(); });
    }
    vid.srcObject = stream;
    await vid.play();
    configureCanvasForVideo();
    setupAudio(stream);
    refreshDeviceSelectors();
    running = true;
    const token = ++runToken;
    resetFace();
    frameCount = 0;
    detectionCount = 0;
    inferenceAvg = 0;
    lastPerfUpdate = performance.now();
    els.snap.disabled = false;
    els.rec.disabled = false;
    updateStartButton();
    setOverlay(false);
    setStatus(media.micFallback ? 'live zonder microfoon — camera werkt, alles blijft lokaal' : 'live — alles blijft lokaal');
    updateTrackPill('search');
    requestAnimationFrame(now => loop(now, token));
  } catch (error) {
    stopActiveStream();
    teardownAudio();
    running = false;
    updateStartButton();
    els.snap.disabled = true;
    els.rec.disabled = true;
    let message = 'camera kon niet starten';
    if (String(error.message || '').startsWith('MODEL:')) message = 'trackingmodel kon niet laden: ' + String(error.message).slice(6);
    else if (error && (error.name === 'NotAllowedError' || error.name === 'SecurityError')) message = 'camera of microfoon niet toegestaan — controleer de browsermelding';
    else if (error && (error.name === 'NotFoundError' || error.name === 'DevicesNotFoundError' || error.name === 'OverconstrainedError')) message = 'geen passende camera gevonden';
    else if (error && error.message === 'NO_SUPPORT') message = 'deze browser ondersteunt geen camera/media-opname';
    else if (error && error.message) message = error.message;
    setOverlay(true, 'Camera niet gestart', message, 'error');
    setStatus(message);
    updateTrackPill('off');
  } finally {
    els.start.disabled = false;
  }
}
function stopCamera() {
  runToken++;
  running = false;
  syncHiddenCapturePump();
  if (recorder && recorder.state === 'recording') {
    try { recorder.stop(); } catch (_) {}
  }
  stopActiveStream();
  teardownAudio();
  resetFace();
  els.snap.disabled = true;
  els.rec.disabled = true;
  cx.save();
  cx.clearRect(0, 0, els.cv.width, els.cv.height);
  cx.fillStyle = '#000';
  cx.fillRect(0, 0, els.cv.width, els.cv.height);
  cx.restore();
  updateStartButton();
  updateTrackPill('off');
  setOverlay(true, 'Webcam gestopt', 'je camera en microfoon zijn losgelaten; start opnieuw wanneer je wilt', 'done');
  setStatus('webcam gestopt — er blijft niets lokaal draaien');
  els.perf.textContent = '';
}
els.start.onclick = () => { if (running) stopCamera(); else openCamera(); };
async function restartMediaForDeviceChange() {
  saveSettings();
  if (!running || restartingMedia) return;
  restartingMedia = true;
  try {
    runToken++;
    running = false;
    stopActiveStream();
    teardownAudio();
    await openCamera(true);
  } finally { restartingMedia = false; }
}
els.cameraSelect.onchange = restartMediaForDeviceChange;
els.micSelect.onchange = restartMediaForDeviceChange;
els.micOn.onchange = restartMediaForDeviceChange;
els.quality.onchange = () => {
  saveSettings();
  configureCanvasForVideo();
  setStatus('kwaliteit aangepast');
};
els.mirror.onchange = () => {
  saveSettings();
  resetFace();
  updateTrackPill(running ? 'search' : 'off');
};
els.lightMatch.onchange = saveSettings;
els.privacyMode.onchange = saveSettings;

function selectCharacter(key, button) {
  if (!CHARACTERS[key] && key !== 'zelf') return;
  kar = key;
  document.querySelectorAll('.kar').forEach(x => x.classList.remove('act'));
  if (button) button.classList.add('act');
  applyFitToControls();
}
document.querySelectorAll('.kar').forEach(button => { button.onclick = () => selectCharacter(button.dataset.k, button); });
document.querySelectorAll('.bg').forEach(button => { button.onclick = () => selectBackground(button.dataset.b, button); });
document.querySelectorAll('.fx').forEach(button => { button.onclick = () => selectEffect(button.dataset.f, button); });
if (els.upBg) {
  els.upBg.onchange = event => {
    const file = event.target.files && event.target.files[0];
    handleBgUploadFile(file);
    event.target.value = '';
  };
  const upBgLabel = document.querySelector('label.upload[for="upBg"]');
  if (upBgLabel) {
    upBgLabel.addEventListener('keydown', event => {
      if (event.key !== 'Enter' && event.key !== ' ') return;
      event.preventDefault();
      els.upBg.click();
    });
  }
}
for (const key of ['camera', 'ruimte', 'onderwater', 'zonsondergang', 'gamewereld']) {
  if (key === bgKind) {
    document.querySelectorAll('.bg').forEach(x => x.classList.toggle('act', x.dataset.b === key));
  }
}
document.querySelectorAll('.fx').forEach(x => x.classList.toggle('act', x.dataset.f === fxKind));
if ($('hostHype')) $('hostHype').onclick=()=>triggerCohost('hype');
if ($('hostVraag')) $('hostVraag').onclick=()=>triggerCohost('vraag');
if ($('hostConfetti')) $('hostConfetti').onclick=()=>triggerCohost('event');
els.cv.addEventListener('pointerdown',ev=>{const r=els.cv.getBoundingClientRect(),x=(ev.clientX-r.left)/r.width,y=(ev.clientY-r.top)/r.height;worldBursts.push({x,y,t:performance.now(),hue:120+Math.random()*160});if(bgKind==='gamewereld'&&Math.random()<.55)triggerCohost('event')});
window.addEventListener('keydown',ev=>{if(ev.key.toLowerCase()==='h')triggerCohost('hype')});
els.resetFit.onclick = () => {
  fitStore[kar] = defaultFit();
  try { localStorage.setItem('g2b-livemasker-browser-fit-v2', JSON.stringify(fitStore)); } catch (_) {}
  applyFitToControls();
  setStatus('passing gereset');
};

function sanitizeSvgText(text) {
  if (/<!doctype|<!entity/i.test(text)) return '';
  const preparseText = String(text).replace(/\s+on[a-zA-Z0-9:_-]+\s*=\s*("[^"]*"|'[^']*'|[^\s>]+)/gi, '');
  const doc = new DOMParser().parseFromString(preparseText, 'image/svg+xml');
  if (doc.querySelector('parsererror')) return '';
  doc.querySelectorAll('script,foreignObject,iframe,video,audio,canvas,object,embed,link,meta,a,style,set,animate,animateTransform,animateMotion').forEach(el => el.remove());
  const safeFragmentUrl = value => !/url\s*\(/i.test(value) || /^([\w-]+\s*:\s*)?url\((['"]?)#[^)'"]+\2\)\s*(!important)?$/i.test(value.trim());
  const safeDataRaster = value => /^data:image\/(png|jpeg|jpg|webp|gif);base64,[a-z0-9+/=\s]+$/i.test(value);
  const sanitizeCss = style => {
    const out = [];
    for (const part of String(style || '').split(';')) {
      const bits = part.split(':');
      if (bits.length < 2) continue;
      const prop = bits.shift().trim();
      const value = bits.join(':').trim();
      const compact = value.replace(/[\s\u0000-\u001f]+/g, '').toLowerCase();
      if (!prop || /expression|behavior|import|javascript:|vbscript:|data:text\/html|data:image\/svg/i.test(compact)) continue;
      if (/url\s*\(/i.test(value) && !safeFragmentUrl(value)) continue;
      out.push(`${prop}:${value}`);
    }
    return out.join(';');
  };
  doc.querySelectorAll('*').forEach(el => {
    for (const attr of [...el.attributes]) {
      const name = attr.name.toLowerCase();
      const raw = String(attr.value || '');
      const compact = raw.replace(/[\s\u0000-\u001f]+/g, '').toLowerCase();
      const isHref = name === 'href' || name.endsWith(':href');
      if (name.startsWith('on') || name === 'srcset' || compact.startsWith('javascript:') || compact.startsWith('vbscript:')) {
        el.removeAttribute(attr.name);
        continue;
      }
      if (name === 'style') {
        const clean = sanitizeCss(raw);
        if (clean) el.setAttribute(attr.name, clean); else el.removeAttribute(attr.name);
        continue;
      }
      if (/url\s*\(/i.test(raw) && !safeFragmentUrl(raw)) {
        el.removeAttribute(attr.name);
        continue;
      }
      if (isHref) {
        if (compact.startsWith('#')) continue;
        if (el.tagName.toLowerCase() === 'image' && safeDataRaster(raw)) continue;
        el.removeAttribute(attr.name);
      }
    }
  });
  return new XMLSerializer().serializeToString(doc.documentElement);
}
async function handleUploadFile(file) {
  if (!file) return false;
  const isSvg = /\.svg$/i.test(file.name) || file.type === 'image/svg+xml';
  const okType = /^image\/(png|webp|gif|svg\+xml)$/.test(file.type) || /\.(png|webp|gif|svg)$/i.test(file.name);
  if (!okType || file.size > 8 * 1024 * 1024) {
    setStatus('dit bestand kan niet als karakter worden gebruikt');
    return false;
  }
  let url = '';
  try {
    if (isSvg) {
      const clean = sanitizeSvgText(await file.text());
      if (!clean) {
        setStatus('deze SVG is niet veilig genoeg voor een karakter');
        return false;
      }
      url = URL.createObjectURL(new Blob([clean], { type: 'image/svg+xml' }));
    } else {
      url = URL.createObjectURL(file);
    }
  } catch (_) {
    setStatus('dit bestand kan niet als karakter worden gebruikt');
    return false;
  }
  objectUrls.push(url);
  return new Promise(resolve => {
    const img = new Image();
    img.onload = () => {
      if (!img.naturalWidth || !img.naturalHeight || img.naturalWidth > 4096 || img.naturalHeight > 4096) {
        setStatus('dit beeld is te groot of ongeldig voor een karakter');
        resolve(false);
        return;
      }
      const base = file.name.replace(/\.[^.]+$/, '').replace(/[^\p{L}\p{N}_-]+/gu, '-').slice(0, 18) || 'eigen';
      const key = 'custom-' + Date.now().toString(36);
      CHARACTERS[key] = {
        label: base, src: url, img, scale: 1,
        eyeCX: 0.5, eyeY: 0.43, eyeDist: 0.28
      };
      const b = document.createElement('button');
      b.className = 'kar';
      b.dataset.k = key;
      b.type = 'button';
      const icon = document.createElement('span');
      icon.textContent = '⭐';
      const label = document.createTextNode(' ' + base);
      b.append(icon, label);
      b.onclick = () => selectCharacter(key, b);
      els.kars.appendChild(b);
      selectCharacter(key, b);
      setStatus('eigen karakter toegevoegd — pas eventueel grootte en positie aan');
      resolve(true);
    };
    img.onerror = () => {
      setStatus('dit bestand kan niet als karakter worden gebruikt');
      resolve(false);
    };
    img.src = url;
  });
}
els.up.onchange = event => {
  const file = event.target.files && event.target.files[0];
  handleUploadFile(file);
  event.target.value = '';
};
const uploadLabel = document.querySelector('label.upload[for="up"]');
if (uploadLabel) {
  uploadLabel.addEventListener('keydown', event => {
    if (event.key !== 'Enter' && event.key !== ' ') return;
    event.preventDefault();
    els.up.click();
  });
}

function downloadBlob(blob, filename) {
  const a = document.createElement('a');
  const url = URL.createObjectURL(blob);
  objectUrls.push(url);
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 5000);
}
function stamp() { return new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19); }
els.snap.onclick = () => {
  if (!running) return;
  drawFrame();
  els.cv.toBlob(blob => {
    if (!blob) return;
    downloadBlob(blob, `g2b-live-masker-${stamp()}.png`);
    setStatus('foto gedownload');
  }, 'image/png');
};
function pickRecorderMime() {
  if (typeof MediaRecorder === 'undefined') return '';
  const options = ['video/webm;codecs=vp9,opus', 'video/webm;codecs=vp8,opus', 'video/webm', 'video/mp4'];
  return options.find(type => MediaRecorder.isTypeSupported(type)) || '';
}
els.rec.onclick = () => {
  if (!running) return;
  if (recorder && recorder.state === 'recording') {
    recorder.stop();
    return;
  }
  const mime = pickRecorderMime();
  if (!mime) {
    setStatus('video-opname wordt niet ondersteund in deze browser');
    return;
  }
  chunks = [];
  drawFrame();
  const p = profile();
  if (typeof els.cv.captureStream !== 'function') {
    setStatus('video-opname wordt niet ondersteund in deze browser');
    return;
  }
  const canvasStream = els.cv.captureStream(p.recordFps);
  const tracks = [...canvasStream.getVideoTracks()];
  if (stream && els.micOn.checked) tracks.push(...stream.getAudioTracks());
  try {
    recorder = new MediaRecorder(new MediaStream(tracks), {
      mimeType: mime,
      videoBitsPerSecond: p.bitrate,
      audioBitsPerSecond: 128_000
    });
  } catch (_) {
    setStatus('video-opname kon in deze browser niet starten');
    return;
  }
  recorder.ondataavailable = event => { if (event.data && event.data.size) chunks.push(event.data); };
  recorder.onerror = () => {
    recordStartedAt = 0;
    els.rec.classList.remove('recording');
    els.rec.textContent = '⏺ neem op';
    setStatus('opname kon niet worden geschreven');
  };
  recorder.onstop = () => {
    const type = recorder.mimeType || mime;
    const blob = new Blob(chunks, { type });
    if (blob.size > 0) {
      const ext = type.includes('mp4') ? 'mp4' : 'webm';
      downloadBlob(blob, `g2b-live-masker-${stamp()}.${ext}`);
      setStatus(`filmpje gedownload · ${(blob.size / 1024 / 1024).toFixed(1)} MB`);
    }
    recordStartedAt = 0;
    els.rec.classList.remove('recording');
    els.rec.textContent = '⏺ neem op';
  };
  recorder.start(250);
  recordStartedAt = performance.now();
  lastRecordTimerUpdate = 0;
  els.rec.classList.add('recording');
  els.rec.textContent = '⏹ stop 0:00';
  setStatus('opname loopt…');
};

window.addEventListener('keydown', event => {
  const tag = (event.target && event.target.tagName || '').toLowerCase();
  if (tag === 'input' || tag === 'select' || tag === 'textarea') return;
  if (event.key === 'Escape' && running) {
    stopCamera();
    return;
  }
  if (event.key.toLowerCase() === 's' && !els.snap.disabled) els.snap.click();
  if (event.key.toLowerCase() === 'r' && !els.rec.disabled) els.rec.click();
});
window.addEventListener('beforeunload', () => {
  stopActiveStream();
  for (const url of objectUrls.splice(0)) {
    try { URL.revokeObjectURL(url); } catch (_) {}
  }
});

function registerServiceWorker() {
  if (!('serviceWorker' in navigator) || !/^https?:$/.test(location.protocol)) {
    setBadge(els.offlineBadge, 'offline niet actief', 'warn');
    return;
  }
  setBadge(els.offlineBadge, 'offline installeren…', '');
  navigator.serviceWorker.register('./sw.js').then(async reg => {
    const mark = () => setBadge(els.offlineBadge, 'offline gereed', 'ok');
    if (reg.active) mark();
    reg.addEventListener('updatefound', () => {
      const sw = reg.installing;
      if (!sw) return;
      sw.addEventListener('statechange', () => { if (sw.state === 'activated') mark(); });
    });
    if (!reg.active) setTimeout(() => { if (reg.active) mark(); }, 5000);
  }).catch(() => setBadge(els.offlineBadge, 'offline niet actief', 'warn'));
}
function imageStatsForCanvas() {
  const w = els.cv.width;
  const h = els.cv.height;
  if (!w || !h) return { width: 0, height: 0, mean: 0, variance: 0 };
  const data = cx.getImageData(0, 0, w, h).data;
  let mean = 0;
  let sq = 0;
  let black = 0;
  for (let i = 0; i < data.length; i += 64) {
    const lum = data[i] * 0.2126 + data[i + 1] * 0.7152 + data[i + 2] * 0.0722;
    mean += lum;
    sq += lum * lum;
    if (lum < 5) black++;
  }
  const n = Math.max(1, Math.ceil(data.length / 64));
  mean /= n;
  return { width: w, height: h, mean, variance: sq / n - mean * mean, blackRatio: black / n };
}
async function mediaDiagnostics() {
  const videoTrack = stream && stream.getVideoTracks ? stream.getVideoTracks()[0] : null;
  const audioTrack = stream && stream.getAudioTracks ? stream.getAudioTracks()[0] : null;
  let devices = [];
  try {
    if (navigator.mediaDevices && navigator.mediaDevices.enumerateDevices) devices = await navigator.mediaDevices.enumerateDevices();
  } catch (_) {}
  const trackInfo = track => track ? {
    label: track.label || '', readyState: track.readyState || '', enabled: !!track.enabled, muted: !!track.muted,
    settings: track.getSettings ? track.getSettings() : {}
  } : null;
  return {
    video: trackInfo(videoTrack), audio: trackInfo(audioTrack),
    videoWidth: vid.videoWidth || 0, videoHeight: vid.videoHeight || 0,
    devices: devices.map(d => ({ kind: d.kind, label: d.label || '', deviceId: d.deviceId || '' }))
  };
}

applySettings();
applyFitToControls();
updateStartButton();
updateTrackPill('off');
updateModelBadge();
setOverlay(true, 'Klaar voor je masker', 'Klik op “Start je webcam”. Je beeld wordt niet geüpload.', 'done');
registerServiceWorker();
setTimeout(updatePrivacyBadge, 1200);
setTimeout(updatePrivacyBadge, 4500);
setTimeout(() => {
  initWorker().catch(() => {});
}, 500);

window.G2BMaskerBrowser = {
  getState() {
    const now = performance.now();
    let externalResources = [];
    try {
      externalResources = performance.getEntriesByType('resource')
        .filter(r => new URL(r.name, location.href).origin !== location.origin)
        .map(r => r.name);
    } catch (_) {}
    return {
      running, workerReady, workerError, workerDelegate, karakter: kar,
      canvas: { width: els.cv.width, height: els.cv.height },
      trackerReady: face.ready && now - face.lastSeen < 600,
      faceAge: face.ready ? now - face.lastSeen : null,
      privacyAlpha: privacyAlpha(now),
      x: face.x0, y: face.y0, eyeDistance: face.d0, rotation: face.r0,
      yaw: face.yaw, pitch: face.pitch, matrix: [...(face.matrix || [])],
      blend: { ...(face.blend || {}) }, micLevel, light: face.light,
      inferenceAvg, effectiveDetectMs, detectionTimeouts, lastTrackInput: { ...lastTrackInput },
      quality: els.quality.value, mirror: els.mirror.checked, micOn: els.micOn.checked,
      micActive: !!(stream && stream.getAudioTracks && stream.getAudioTracks().length),
      privacyMode: els.privacyMode.value, lightMatch: els.lightMatch.checked,
      status: els.status.textContent, perf: els.perf.textContent,
      privacyBadge: els.privacyBadge.textContent, offlineBadge: els.offlineBadge.textContent, modelBadge: els.modelBadge.textContent,
      externalResources, lastMaskMetrics: lastMaskMetrics ? { ...lastMaskMetrics } : null,
      bg: bgKind, fx: fxKind, bgImageLabel, camMode:($('camMode')&&$('camMode').value)||'statisch',camShot:virtualCam(performance.now()).shot,directorCam:virtualCam(performance.now()),regieAuto: !!($('regieAuto') && $('regieAuto').checked), cohostOn: !!($('cohostOn') && $('cohostOn').checked), hostMsg, worldBursts: worldBursts.length,
      segReady, segError, segDelegate, segInferenceAvg, personCoverage,
      segReinitAttempts, segLowCoverageStreak, jawSmoothed,
      personMaskAge: personMask ? now - personMask.timestamp : null,
      particleCount: particles.length, frameTimeAvg,
      activeCharacter: CHARACTERS[kar] ? {
        label: CHARACTERS[kar].label, src: CHARACTERS[kar].src,
        eyeCX: CHARACTERS[kar].eyeCX, eyeY: CHARACTERS[kar].eyeY, eyeDist: CHARACTERS[kar].eyeDist,
        scale: CHARACTERS[kar].scale, mouth: CHARACTERS[kar].mouth || DEFAULT_MOUTH
      } : null,
      characterCount: Object.keys(CHARACTERS).length,
      snapDisabled: els.snap.disabled, recDisabled: els.rec.disabled
    };
  },
  start: openCamera,
  stop: stopCamera,
  selectCharacter,
  selectBackground,
  selectEffect,
  triggerCohost,
  uploadBgFile: handleBgUploadFile,
  initSeg: initSegWorker,
  setFit(values) {
    for (const [key, input] of Object.entries({ sc: els.sc, ox: els.ox, oy: els.oy })) {
      if (values[key] !== undefined) {
        input.value = values[key];
        input.dispatchEvent(new Event('input', { bubbles: true }));
      }
    }
  },
  resetFit: () => els.resetFit.click(),
  uploadFile: handleUploadFile,
  snapshot: () => els.snap.click(),
  record: () => els.rec.click(),
  drawOnce: drawFrame,
  streamInfo: mediaDiagnostics,
  imageStats: imageStatsForCanvas,
  debugBlend(values) {
    face.blend = { ...(face.blend || {}), ...(values || {}) };
    drawFrame();
    return { blend: { ...face.blend }, metrics: lastMaskMetrics ? { ...lastMaskMetrics } : null };
  },
  debugBlendOverride(values, ms) {
    blendOverride = values ? { vals: { ...values }, until: performance.now() + (ms || 2000) } : null;
    return !!blendOverride;
  },
  debugFace(values = {}) {
    const target = {
      x: values.x ?? els.cv.width / 2,
      y: values.y ?? els.cv.height * 0.42,
      d: values.d ?? els.cv.width * 0.16,
      r: values.r ?? 0,
      blend: values.blend || {}
    };
    updateFace(target, performance.now());
    drawFrame();
    return { target, state: this.getState() };
  },
  debugPose(values = {}) {
    if (values.yaw !== undefined) face.yaw = Number(values.yaw) || 0;
    if (values.pitch !== undefined) face.pitch = Number(values.pitch) || 0;
    drawFrame();
    return { yaw: face.yaw, pitch: face.pitch, metrics: lastMaskMetrics ? { ...lastMaskMetrics } : null };
  },
  debugAge(ms) {
    if (face.ready) face.lastSeen -= Number(ms || 0);
    drawFrame();
    return this.getState();
  },
  exportStats() {
    drawFrame();
    return imageStatsForCanvas();
  },
  mouthRegionStats() {
    const mm = lastMaskMetrics;
    if (!mm || !mm.width) return null;
    const def = CHARACTERS[kar];
    const mouth = (def && def.mouth) || DEFAULT_MOUTH;
    const mx = mm.x + (mouth.x * mm.width - mm.anchorX);
    const my = mm.y + (mouth.y * mm.height - mm.anchorY);
    const r = Math.max(6, mouth.w * mm.width * 0.5);
    const x0 = Math.max(0, Math.round(mx - r)), x1 = Math.min(els.cv.width, Math.round(mx + r));
    const y0 = Math.max(0, Math.round(my - r)), y1 = Math.min(els.cv.height, Math.round(my + r));
    if (x1 <= x0 || y1 <= y0) return null;
    const data = cx.getImageData(x0, y0, x1 - x0, y1 - y0).data;
    let dark = 0, n = 0, sumR = 0, sumG = 0, sumB = 0;
    for (let i = 0; i < data.length; i += 4) {
      const R = data[i], G = data[i + 1], B = data[i + 2];
      sumR += R; sumG += G; sumB += B; n++;
      const lum = R * 0.2126 + G * 0.7152 + B * 0.0722;
      if (lum < 70 && R > B * 0.8) dark++;
    }
    return { cx: mx, cy: my, r, darkRatio: dark / n, meanR: sumR / n, meanG: sumG / n, meanB: sumB / n };
  },
  debugWatchdog(coverage, facePresent) {
    const before = { attempts: segReinitAttempts, streak: segLowCoverageStreak };
    const triggered = segWatchdogTick(Number(coverage) || 0, !!facePresent);
    return {
      triggered,
      before,
      after: { attempts: segReinitAttempts, streak: segLowCoverageStreak, forced: segDelegateForced }
    };
  }
};
