/* G2B AIRDRUMS v2 — gedeelde kern voor gratis- en clubversie.
   Eigen productlijn: drum/melodie/fx in de lucht, MIDI naar Reason, 100% lokaal.
   v2: echte handtracking-slagengine (MediaPipe HandLandmarker, Apache-2.0) bovenop
   de bewezen versie 1-functies (classieke pixeldetectie blijft als reserve). */
'use strict';
(function () {
const $ = id => document.getElementById(id);
const FREE = !!window.G2B_AIRDRUMS_FREE;

const PAD4 = [
  { n: 'KICK', x: .25, y: .75, note: 36, col: '#35d07f' }, { n: 'SNARE', x: .75, y: .75, note: 38, col: '#ff9a3c' },
  { n: 'HAT', x: .25, y: .28, note: 42, col: '#b98cff' }, { n: 'CLAP', x: .75, y: .28, note: 39, col: '#f5e9c8' }];
const PAD8 = [
  { n: 'HAT', x: .14, y: .3, note: 42, col: '#b98cff' }, { n: 'OHAT', x: .38, y: .3, note: 46, col: '#9a7fd0' },
  { n: 'CYM', x: .62, y: .3, note: 49, col: '#f5e9c8' }, { n: 'CLAP', x: .86, y: .3, note: 39, col: '#ffd166' },
  { n: 'KICK', x: .14, y: .78, note: 36, col: '#35d07f' }, { n: 'SNARE', x: .38, y: .78, note: 38, col: '#ff9a3c' },
  { n: 'TOM', x: .62, y: .78, note: 47, col: '#ef8354' }, { n: 'PERC', x: .86, y: .78, note: 70, col: '#8ecae6' }];
const PAD4SYNTH = ['kick', 'snare', 'hat', 'clap'], PAD8SYNTH = ['hat', 'hat', 'cym', 'clap', 'kick', 'snare', 'tom', 'perc'];
const PW4 = .30, PH4 = .34, PW8 = .19, PH8 = .30;
const SCALES = [[45, 48, 50, 52, 55, 57, 60, 62, 64, 67, 69], [48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67], [42, 45, 47, 49, 52, 54, 57, 59, 61, 64, 66]];
let ac = null, midiOut = null, prev = null, base = new Float32Array(8), cool = new Float64Array(8), run = false;
let ownSamp = [];
let padPick = -1, metrIv = null, beatN = 0;
let lastNote = -1, recT0 = 0, recLog = [], fxCool = 0;
const LW = 160, LH = 120;
const off = document.createElement('canvas'); off.width = LW; off.height = LH;
const octx = off.getContext('2d', { willReadFrequently: true });

// --- eigen synth ---
function ensureAC() { if (!ac) ac = new (window.AudioContext || window.webkitAudioContext)(); if (ac.state === 'suspended') ac.resume(); }
function noiseBuf() { if (!noiseBuf.b) { const b = ac.createBuffer(1, ac.sampleRate, ac.sampleRate), d = b.getChannelData(0); for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1; noiseBuf.b = b; } return noiseBuf.b; }
function g(t, v, dur) { const gn = ac.createGain(); gn.gain.setValueAtTime(v, t); gn.gain.exponentialRampToValueAtTime(.001, t + dur); return gn; }
function nz(t, v, dur, fq, type, q) { const n = ac.createBufferSource(); n.buffer = noiseBuf(); const f = ac.createBiquadFilter(); f.type = type; f.frequency.value = fq; f.Q.value = q || 1; n.connect(f).connect(g(t, v, dur)).connect(ac.destination); n.start(t); n.stop(t + dur + .02); }
function sn(t, f0, f1, dur, v) { const o = ac.createOscillator(); o.frequency.setValueAtTime(f0, t); if (f1) o.frequency.exponentialRampToValueAtTime(f1, t + dur * .6); o.connect(g(t, v, dur)).connect(ac.destination); o.start(t); o.stop(t + dur + .02); }
const SYN = {
  kick: v => { const t = ac.currentTime; sn(t, 150, 48, .3, v); nz(t, v * .4, .02, 4000, 'highpass'); },
  snare: v => { const t = ac.currentTime; nz(t, v * .8, .16, 1800, 'bandpass', .8); sn(t, 190, 0, .09, v * .5); },
  hat: v => nz(ac.currentTime, v * .5, .05, 8000, 'highpass'),
  clap: v => { const t = ac.currentTime; for (let i = 0; i < 3; i++) nz(t + i * .012, v * .6, .09, 1400, 'bandpass', 1.5); },
  cym: v => nz(ac.currentTime, v * .5, .5, 6000, 'highpass'),
  tom: v => { const t = ac.currentTime; sn(t, 120, 80, .25, v); },
  perc: v => { const t = ac.currentTime; sn(t, 900, 600, .05, v * .7); },
  mel: (v, note) => { const t = ac.currentTime, fq = 440 * Math.pow(2, (note - 69) / 12); sn(t, fq, fq, .25, v * .6); nz(t, v * .2, .03, 5000, 'highpass'); }
};

// --- MIDI ---
async function setupMIDI() {
  try {
    const m = await navigator.requestMIDIAccess();
    const sel = $('midi'); sel.innerHTML = '<option value="">— geen —</option>';
    for (const o of m.outputs.values()) { const op = document.createElement('option'); op.value = o.id; op.textContent = o.name; sel.appendChild(op); }
    if (sel.options.length > 1) sel.selectedIndex = 1;
    sel.onchange = () => { midiOut = sel.value ? m.outputs.get(sel.value) : null; };
    sel.onchange();
    for (const inp of m.inputs.values()) {
      inp.onmidimessage = ev => {
        const [st, n, v] = ev.data;
        const typ = st & 0xF0;
        if (typ === 0x90 && v > 0) {
          ensureAC();
          const ps = pads(), z = ps.findIndex(p => p.note === n);
          if (z >= 0) { padHit(z, Math.max(.3, v / 127)); }
          else {
            const vv = Math.max(.25, v / 127), vel = Math.min(127, 40 + Math.round(v * .7) + Math.round((Math.random() - .5) * 10));
            const chord = $('chord').value === 'aan' ? chordNotes(n) : [snap(n)];
            for (const nn of chord) { if ($('snd').value === 'aan') SYN.mel(vv, nn); noteOn(nn, vel, 0); logNote(nn, vel, 0); }
          }
        } else if (typ === 0x80 || (typ === 0x90 && v === 0)) {
          const ps = pads(); if (!ps.find(p => p.note === n)) noteOff(snap(n), 0);
        }
      };
    }
  } catch (e) { }
}
function noteOn(n, v, ch) { if (midiOut) { midiOut.send([0x90 | (ch === undefined ? 9 : ch), n, v]); if (ch === undefined || ch === 9) midiOut.send([0x89, n, 0], performance.now() + 120); } }
function noteOff(n, ch) { if (midiOut) midiOut.send([0x80 | (ch === undefined ? 0 : ch), n, 0]); }
function sendCC(cc, v, ch) { if (midiOut) midiOut.send([0xB0 | (ch === undefined ? 9 : ch), cc, Math.max(0, Math.min(127, Math.round(v)))]); }
function snap(n) {
  const sc = SCALES[$('toon').value | 0], pcs = sc.map(s => s % 12);
  for (let d = 0; d <= 5; d++) { if (pcs.includes((n - d) % 12)) return n - d; if (pcs.includes((n + d) % 12)) return n + d; }
  return n;
}
function chordNotes(root) {
  const sc = SCALES[$('toon').value | 0].slice().sort((a, b) => a - b);
  const i = sc.reduce((b, _, j) => Math.abs(sc[j] - root) < Math.abs(sc[b] - root) ? j : b, 0);
  return [sc[i], sc[(i + 2) % sc.length] + (i + 2 >= sc.length ? 12 : 0), sc[(i + 4) % sc.length] + (i + 4 >= sc.length ? 12 : 0)];
}
function human(v) { return Math.max(1, Math.min(127, v + Math.round((Math.random() - .5) * 12))); }

// --- opname → .mid ---
let recOn = false;
let recCnt = false;
function startRec() { recOn = true; $('rec').classList.add('on'); $('stat').textContent = '● OPNAME LOOPT'; recLog = []; recT0 = performance.now(); $('dl').style.display = 'none'; }
function wireRecButton() {
  $('rec').onclick = () => {
    if (FREE) { $('stat').innerHTML = '🔒 OPNAME + .mid-export zit in de <b>CLUB</b>-versie — <a href="bestel.html?p=g2bclub" style="color:#35d07f">word lid (€4,95/mnd)</a>'; return; }
    if (recOn) {
      recOn = false; $('rec').classList.remove('on'); $('stat').textContent = '';
      if (recLog.length) { const b = writeMid(); $('dl').href = URL.createObjectURL(b); $('dl').download = 'G2B-AIRDRUMS.mid'; $('dl').style.display = 'inline'; $('dl').onclick = e => { if (window.G2BExport && !G2BExport.ok('airdrums')) e.preventDefault(); }; }
      return;
    }
    if (recCnt) { return; }
    recCnt = true; ensureAC(); let n = 3;
    const iv = setInterval(() => {
      if (n > 0) { $('stat').textContent = 'OPNAME START IN ' + n + '…'; const t = ac.currentTime; sn(t, 1200, 0, .06, .5); n--; }
      else { clearInterval(iv); recCnt = false; const t = ac.currentTime; sn(t, 1800, 0, .1, .6); startRec(); }
    }, 60000 / (+$('bpm').value || 120) / 2);
  };
}
function logNote(n, v, ch) {
  if (recOn) {
    let ms = performance.now() - recT0;
    if ($('quant').value === '16') { const q = 60000 / (+$('bpm').value) / 4; ms = Math.round(ms / q) * q; }
    recLog.push([ms, n, v, ch === undefined ? 9 : ch]);
  }
}
function vlq(a, v) { do { let b = v & 127; v >>= 7; if (v) b |= 128; a.push(b); } while (v); }
function writeMid() {
  const tpq = 480, mpq = Math.round(60000000 / (+$('bpm').value || 120)), bpm = +$('bpm').value || 120;
  recLog.sort((a, b) => a[0] - b[0]);
  const byCh = [[], [], []];
  for (const [ms, n, v, ch] of recLog) { byCh[ch === 9 ? 2 : ch === 1 ? 1 : 0].push([ms, n, v, ch]); }
  function noteTrack(list) {
    const evs = [];
    for (const [ms, n, v, ch] of list) {
      const tk = Math.round(ms / 1000 * bpm / 60 * tpq);
      evs.push([tk, 0x90 | ch, n, v]); evs.push([tk + Math.round(tpq / 4), 0x80 | ch, n, 0]);
    }
    return evs;
  }
  const tracks = [[0x00, 0xFF, 0x51, 0x03, mpq >> 16, (mpq >> 8) & 255, mpq & 255]];
  const names = ['G2B-MELODIE', 'G2B-PITCH', 'G2B-DRUMS'];
  byCh.forEach((list, i) => {
    if (!list.length) return;
    const evs = [[0, 0xFF, 0x03, names[i].length, ...names[i].split('').map(c => c.charCodeAt(0))], ...noteTrack(list)];
    evs.sort((a, b) => a[0] - b[0]);
    const b = []; let last = 0;
    for (const e of evs) { vlq(b, e[0] - last); last = e[0]; b.push(e[1], e[2], e[3]); }
    vlq(b, 0); b.push(0xFF, 0x2F, 0x00);
    tracks.push([0x4D, 0x54, 0x72, 0x6B, (b.length >>> 24) & 255, (b.length >>> 16) & 255, (b.length >>> 8) & 255, b.length & 255, ...b]);
  });
  const head = [0x4D, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 0, 0, 1, 0, 0, tpq >> 8, tpq & 255];
  head[9] = 1; head[10] = tracks.length;
  return new Blob([new Uint8Array([...head, ...tracks.flat()])], { type: 'audio/midi' });
}

// --- klassieke pixeldetectie (reservepad, bewezen in v1) ---
function pads() { const n = $('npads').value === '8' ? PAD8 : PAD4; return n; }
function zoneEnergies(ps) {
  octx.drawImage($('cam'), 0, 0, LW, LH);
  const cur = octx.getImageData(0, 0, LW, LH).data;
  if (!prev) { prev = new Uint8ClampedArray(cur); return null; }
  const out = new Float32Array(ps.length);
  const pw = (ps.length === 8 ? PW8 : PW4), ph = (ps.length === 8 ? PH8 : PH4);
  for (let z = 0; z < ps.length; z++) {
    const zx = 1 - ps[z].x;
    const x0 = Math.max(0, Math.floor((zx - pw / 2) * LW)), x1 = Math.min(LW, Math.ceil((zx + pw / 2) * LW));
    const y0 = Math.max(0, Math.floor((ps[z].y - ph / 2) * LH)), y1 = Math.min(LH, Math.ceil((ps[z].y + ph / 2) * LH));
    let s = 0, c = 0;
    for (let y = y0; y < y1; y += 2) for (let x = x0; x < x1; x += 2) {
      const i = (y * LW + x) * 4;
      s += Math.abs(cur[i] - prev[i]) + Math.abs(cur[i + 1] - prev[i + 1]) + Math.abs(cur[i + 2] - prev[i + 2]); c++;
    }
    out[z] = c ? s / c / 255 : 0;
  }
  prev.set(cur); return out;
}
function centroid() {
  octx.drawImage($('cam'), 0, 0, LW, LH);
  const cur = octx.getImageData(0, 0, LW, LH).data;
  if (!prev) { prev = new Uint8ClampedArray(cur); return null; }
  let sx = 0, sy = 0, sc = 0;
  for (let y = 0; y < LH; y += 3) for (let x = 0; x < LW; x += 3) {
    const i = (y * LW + x) * 4;
    const d = Math.abs(cur[i] - prev[i]) + Math.abs(cur[i + 1] - prev[i + 1]) + Math.abs(cur[i + 2] - prev[i + 2]);
    if (d > 60) { sx += x; sy += y; sc++; }
  }
  prev.set(cur);
  return sc ? { x: sx / sc / LW, y: sy / sc / LH, e: sc / (LW * LH / 9) } : null;
}

// =========================================================================
// HANDTRACKING-SLAGENGINE — het hart van v2
// Een slag is een neerwaartse beweging die afremt/omkeert, net als een drumstok
// die het vel raakt en terugveert. Velocity komt uit de echte pieksnelheid.
// =========================================================================
const STRIKE_DOWN_MIN = 0.85;  // genormaliseerde beeldhoogtes/sec neerwaarts
const STRIKE_END_VEL = 0.18;   // slag eindigt als de neerwaartse snelheid instort
const HAND_REFRACT_MS = 110;   // minimale rust voor dezelfde hand op hetzelfde pad

function newHandState() {
  return { phase: 'idle', peakVy: 0, prevX: 0, prevY: 0, prevT: 0, smVy: 0, fastFrames: 0, lastHitMs: 0, seen: 0, x: 0.5, y: 0.5, label: '', present: false };
}
const handStates = [newHandState(), newHandState()];

function padAt(ps, x, y) {
  // x,y in gespiegelde (gebruikers) coördinaten 0..1
  const pw = (ps.length === 8 ? PW8 : PW4), ph = (ps.length === 8 ? PH8 : PH4);
  for (let z = 0; z < ps.length; z++) {
    const p = ps[z];
    if (Math.abs(x - p.x) <= pw / 2 && Math.abs(y - p.y) <= ph / 2) return z;
  }
  return -1;
}
function velFromPeak(peakVy) {
  const norm = Math.max(0, Math.min(1, (peakVy - STRIKE_DOWN_MIN) / (3.4 - STRIKE_DOWN_MIN)));
  return 0.25 + norm * 0.75; // audio-gain 0.25..1
}
function midiVelFromPeak(peakVy) {
  const norm = Math.max(0, Math.min(1, (peakVy - STRIKE_DOWN_MIN) / (3.4 - STRIKE_DOWN_MIN)));
  return Math.round(45 + norm * 82); // 45..127
}
// één hand per frame bijwerken; geeft hit terug of null. Zuiver testbaar.
function updateHandStrike(hs, x, y, tMs, ps) {
  hs.x = x; hs.y = y;
  if (!hs.prevT) { hs.prevX = x; hs.prevY = y; hs.prevT = tMs; return null; }
  const dt = (tMs - hs.prevT) / 1000;
  if (dt <= 0 || dt > 0.5) { hs.prevX = x; hs.prevY = y; hs.prevT = tMs; hs.smVy = 0; return null; }
  const vy = (y - hs.prevY) / dt;
  hs.smVy = hs.smVy * 0.35 + vy * 0.65;
  hs.prevX = x; hs.prevY = y; hs.prevT = tMs;
  let hit = null;
  if (hs.phase === 'idle') {
    hs.fastFrames = vy > STRIKE_DOWN_MIN ? hs.fastFrames + 1 : 0;
    if (hs.fastFrames >= 2) { hs.phase = 'striking'; hs.peakVy = vy; }
  } else if (hs.phase === 'striking') {
    hs.peakVy = Math.max(hs.peakVy, vy);
    if (vy < STRIKE_END_VEL) {
      const z = padAt(ps, x, y);
      if (z >= 0 && tMs - hs.lastHitMs > HAND_REFRACT_MS) {
        hs.lastHitMs = tMs;
        hit = { zone: z, vel: velFromPeak(hs.peakVy), midiVel: midiVelFromPeak(hs.peakVy), peakVy: hs.peakVy };
      }
      hs.phase = 'refract';
      hs.fastFrames = 0;
    }
  } else if (hs.phase === 'refract') {
    if (vy < 0.3) hs.phase = 'idle';
    hs.fastFrames = 0;
  }
  return hit;
}

// --- handworker-beheer ---
let handWorker = null, handReady = false, handError = '', handDelegate = '';
let handPending = false, handInferenceAvg = 0, handLastSent = 0, handSeq = 0;
let handResultsSeen = 0, lastHandSeenMs = 0;
function initHandWorker(forceDelegate) {
  if (handReady) return Promise.resolve(true);
  return new Promise(resolve => {
    let settled = false;
    const finish = ok => { if (!settled) { settled = true; resolve(ok); } };
    try { handWorker = new Worker('/js/airdrums-hand-worker.js'); }
    catch (e) { handError = String(e && e.message || e); finish(false); return; }
    const timeout = setTimeout(() => { handError = 'handmodel laadt te langzaam'; finish(false); }, 20000);
    handWorker.onmessage = event => {
      const msg = event.data || {};
      if (msg.type === 'ready') {
        clearTimeout(timeout);
        handReady = true; handDelegate = msg.delegate || ''; handError = '';
        finish(true);
        return;
      }
      if (msg.type === 'init-error') { clearTimeout(timeout); handError = msg.error || 'handmodel-startfout'; finish(false); return; }
      if (msg.type !== 'result') return;
      handPending = false;
      if (!msg.ok) return;
      handInferenceAvg = handInferenceAvg ? handInferenceAvg * 0.8 + msg.inferenceMs * 0.2 : msg.inferenceMs;
      handResultsSeen++;
      processHandResult(msg);
    };
    handWorker.onerror = e => { clearTimeout(timeout); handError = String(e.message || 'handworker-fout'); handPending = false; finish(false); };
    handWorker.postMessage({ type: 'init', delegate: forceDelegate || 'AUTO' });
  });
}
function processHandResult(msg) {
  const now = performance.now();
  const mode = $('mode').value;
  const ps = pads();
  const present = new Set();
  for (const h of msg.hands || []) {
    const pts = new Float32Array(h.pts);
    // vingertoppen index(8)+middel(12) gemiddeld = slagpunt; spiegelen naar gebruikersbeeld
    const xImg = (pts[8 * 2] + pts[12 * 2]) / 2;
    const yImg = (pts[8 * 2 + 1] + pts[12 * 2 + 1]) / 2;
    const x = 1 - xImg, y = yImg;
    let hi = h.handedness === 'Left' ? 0 : h.handedness === 'Right' ? 1 : -1;
    if (hi < 0) hi = handStates[0].seen <= handStates[1].seen ? 0 : 1;
    const hs = handStates[hi];
    hs.present = true; hs.seen++; hs.label = h.handedness || (hi === 0 ? 'L' : 'R');
    present.add(hi);
    lastHandSeenMs = now;
    const hit = updateHandStrike(hs, x, y, now, ps);
    if (hit && mode === 'drums') {
      const z = hit.zone;
      const gate = gateMs();
      if (!gate || now - cool[z] >= gate) {
        cool[z] = now;
        if (ps[z].note === 46 && midiOut) midiOut.send([0x89, 42, 0]);
        triggerPad(z, hit.vel, hit.midiVel);
      }
    }
  }
  for (let i = 0; i < 2; i++) if (!present.has(i)) handStates[i].present = false;
  // mel/fx/gtr sturen continu op handpositie van de eerste aanwezige hand
  const first = handStates.find(h => h.present);
  if (first && mode !== 'drums') handContinuous(first, now, mode);
}
function handContinuous(hs, now, mode) {
  if (mode === 'mel') {
    const scale = SCALES[$('toon').value | 0];
    const idx = Math.max(0, Math.min(scale.length - 1, Math.floor((1 - hs.y) * scale.length)));
    const note = scale[idx];
    if (note !== lastNote) {
      lastNote = note;
      const vel = 85;
      if ($('snd').value === 'aan') SYN.mel(.55, note);
      noteOn(note, vel); logNote(note, vel);
    }
    const bars = document.querySelectorAll('#meters .f')[0]; if (bars) bars.style.height = (1 - hs.y) * 100 + '%';
  } else if (mode === 'gtr') {
    if (gtrSrc) gtrCtl(1 - hs.x, hs.y);
  } else if (mode === 'fx') {
    if (now - fxCool > 40) {
      fxCool = now;
      sendCC(74, (1 - hs.x) * 127); sendCC(11, (1 - hs.y) * 127);
    }
  }
}
function sendHandDetection(now) {
  if (!handReady || handPending || !run) return;
  const vid = $('cam');
  if (vid.readyState < 2) return;
  const interval = Math.max(33, handInferenceAvg ? handInferenceAvg * 1.5 : 0);
  if (now - handLastSent < interval) return;
  handLastSent = now;
  handPending = true;
  const seq = ++handSeq;
  const scale = Math.min(1, 320 / Math.max(vid.videoWidth || 1, vid.videoHeight || 1));
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
      handWorker.postMessage({ type: 'detect', image: bitmap, timestamp: now, seq }, [bitmap]);
    } catch (_) {
      if (bitmap.close) bitmap.close();
      handPending = false;
    }
  }).catch(() => { handPending = false; });
}

// --- gedeelde padtrigger ---
function triggerPad(z, gainV, midiVel) {
  const ps = pads();
  if (z >= ps.length) return;
  if ($('snd').value === 'aan') {
    if (ownSamp[z]) { const b = ac.createBufferSource(); b.buffer = ownSamp[z]; const gg = ac.createGain(); gg.gain.value = gainV; b.connect(gg).connect(ac.destination); b.start(); }
    else { const synth = ps.length === 8 ? PAD8SYNTH[z] : PAD4SYNTH[z]; SYN[synth](gainV); }
  }
  const vel = midiVel || human(Math.min(127, Math.round(40 + gainV * 80)));
  noteOn(ps[z].note, vel); logNote(ps[z].note, vel); fl[z] = performance.now();
  const bars = document.querySelectorAll('#meters .f')[z]; if (bars) bars.style.height = gainV * 100 + '%';
}
function padHit(z, v) { // klassieke/muis/toetsenbord/midi-in trigger (ongewijzigd gedrag)
  const ps = pads(); if (z >= ps.length) return;
  const gate = gateMs(), now = performance.now(); if (gate && now - cool[z] < gate) return; cool[z] = now;
  triggerPad(z, v, 0);
}

// --- gitaar-pedaal + pitch (ongewijzigd uit v1) ---
let gtrSrc = null, gtrFil = null, gtrGain = null, mouseP = { x: .5, y: .5 };
async function gtrSetup() {
  try {
    ensureAC();
    const s = await navigator.mediaDevices.getUserMedia({ audio: true });
    gtrSrc = ac.createMediaStreamSource(s);
    gtrFil = ac.createBiquadFilter(); gtrFil.type = 'lowpass'; gtrFil.frequency.value = 800; gtrFil.Q.value = 6;
    gtrGain = ac.createGain(); gtrGain.gain.value = .9;
    gtrSrc.connect(gtrFil).connect(gtrGain).connect(ac.destination);
    $('stat').textContent = '✓ GITAAR aan — hand/muis: X=wah-filter, Y=volume';
  } catch (e) { $('stat').textContent = '✗ Geen audio-ingang — sluit gitaar/mic aan (line-in).'; }
}
function gtrCtl(x, y) {
  if (!gtrFil) return;
  const t = ac.currentTime;
  gtrFil.frequency.setTargetAtTime(150 + Math.pow(x, 2) * 8000, t, .02);
  gtrGain.gain.setTargetAtTime(Math.max(0, 1 - y), t, .03);
  sendCC(74, x * 127, 0); sendCC(11, y * 127, 0);
}
let pitchSrc = null, pitchAn = null, pitchNote = -1, pitchCand = -1, pitchCandN = 0;
function pitchStep() {
  const f = detectPitch();
  if (f > 0) {
    let n = snap(Math.round(69 + 12 * Math.log2(f / 440)));
    if (n === pitchCand) pitchCandN++; else { pitchCand = n; pitchCandN = 1; }
    if (pitchCandN >= 2 && n !== pitchNote) {
      if (pitchNote >= 0) noteOff(pitchNote, 1);
      pitchNote = n;
      const vel = 85; if ($('snd').value === 'aan') SYN.mel(.6, n); noteOn(n, vel, 1); logNote(n, vel, 1);
      const bars = document.querySelectorAll('#meters .f')[0]; if (bars) bars.style.height = '80%';
    }
  } else {
    pitchCand = -1; pitchCandN = 0;
    if (pitchNote >= 0) { noteOff(pitchNote, 1); pitchNote = -1; }
    const bars = document.querySelectorAll('#meters .f')[0]; if (bars) bars.style.height = '0%';
  }
}
async function pitchSetup() {
  try {
    ensureAC();
    const s = await navigator.mediaDevices.getUserMedia({ audio: true });
    pitchSrc = ac.createMediaStreamSource(s);
    pitchAn = ac.createAnalyser(); pitchAn.fftSize = 2048;
    pitchSrc.connect(pitchAn);
    $('stat').textContent = '✓ PITCH aan — zing of speel, noten gaan naar MIDI-uit';
  } catch (e) { $('stat').textContent = '✗ Geen audio-ingang voor PITCH.'; }
}
const pitchBuf = new Float32Array(2048);
function detectPitch() {
  pitchAn.getFloatTimeDomainData(pitchBuf);
  let rms = 0; for (const v of pitchBuf) rms += v * v; rms = Math.sqrt(rms / pitchBuf.length);
  if (rms < .012) return 0;
  const sr = ac.sampleRate, N = pitchBuf.length;
  let best = -1, bestC = 0;
  const lag0 = Math.floor(sr / 1000), lag1 = Math.min(N - 1, Math.floor(sr / 70));
  for (let lag = lag0; lag < lag1; lag++) {
    let s = 0; for (let i = 0; i < N - lag; i++) s += pitchBuf[i] * pitchBuf[i + lag];
    if (s > bestC) { bestC = s; best = lag; }
  }
  return best > 0 ? sr / best : 0;
}

// --- render ---
const cv = $('cv'), ctx = cv.getContext('2d');
let fl = new Float64Array(8);
const HAND_COLS = ['#35d07f', '#ff9a3c'];
function teken() {
  const W = cv.width = cv.clientWidth, H = cv.height = cv.clientHeight;
  ctx.clearRect(0, 0, W, H);
  const mode = $('mode').value;
  if (mode === 'drums' || mode === 'muis') {
    const ps = pads(), pw = (ps.length === 8 ? PW8 : PW4) * W, ph = (ps.length === 8 ? PH8 : PH4) * H;
    ps.forEach((p, z) => {
      const x = (1 - p.x) * W - pw / 2, y = p.y * H - ph / 2;
      const hit = performance.now() - fl[z] < 150;
      ctx.strokeStyle = p.col; ctx.lineWidth = hit ? 5 : 2;
      ctx.fillStyle = p.col + (hit ? '66' : '22');
      ctx.beginPath(); if (ctx.roundRect) ctx.roundRect(x, y, pw, ph, 14); else ctx.rect(x, y, pw, ph); ctx.fill(); ctx.stroke();
      ctx.fillStyle = hit ? '#fff' : p.col; ctx.font = 'bold ' + (ps.length === 8 ? 14 : 18) + 'px system-ui'; ctx.textAlign = 'center';
      ctx.fillText(p.n, x + pw / 2, y + ph / 2 + 6);
    });
    drawHandCursors(W, H);
  } else if (mode === 'mel') {
    const scale = SCALES[$('toon').value | 0];
    ctx.strokeStyle = '#35d07f44'; ctx.lineWidth = 1;
    for (let i = 0; i < scale.length; i++) { const y = i / scale.length * H; ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W, y); ctx.stroke(); }
    ctx.fillStyle = '#f5e9c8'; ctx.font = 'bold 16px system-ui'; ctx.textAlign = 'center';
    ctx.fillText('HOOG ↑', W / 2, 22); ctx.fillText('LAAG ↓', W / 2, H - 10);
    drawHandCursors(W, H);
  } else {
    const gr = ctx.createLinearGradient(0, 0, W, H);
    gr.addColorStop(0, '#35d07f22'); gr.addColorStop(1, '#b98cff22');
    ctx.fillStyle = gr; ctx.fillRect(0, 0, W, H);
    ctx.strokeStyle = '#35d07f55';
    ctx.beginPath(); ctx.moveTo(W / 2, 0); ctx.lineTo(W / 2, H); ctx.moveTo(0, H / 2); ctx.lineTo(W, H / 2); ctx.stroke();
    ctx.fillStyle = '#f5e9c8'; ctx.font = 'bold 13px system-ui'; ctx.textAlign = 'center';
    ctx.fillText('← FILTER (CC74 / wah) →', W / 2, 20); ctx.fillText('↕ EXPRESSIE (CC11 / vol)', W / 2, H - 12);
    drawHandCursors(W, H);
  }
}
function drawHandCursors(W, H) {
  if (invoer() !== 'handen') return;
  for (let i = 0; i < 2; i++) {
    const hs = handStates[i];
    if (!hs.present) continue;
    const x = hs.x * W, y = hs.y * H;
    const col = HAND_COLS[i];
    ctx.save();
    ctx.strokeStyle = col; ctx.fillStyle = col;
    ctx.lineWidth = 2.5;
    ctx.beginPath(); ctx.arc(x, y, 13, 0, Math.PI * 2); ctx.stroke();
    ctx.globalAlpha = .85;
    ctx.beginPath(); ctx.arc(x, y, 4.5, 0, Math.PI * 2); ctx.fill();
    if (hs.phase === 'striking') {
      ctx.globalAlpha = .5;
      ctx.beginPath(); ctx.arc(x, y, 20, 0, Math.PI * 2); ctx.stroke();
    }
    ctx.globalAlpha = 1;
    ctx.font = 'bold 10px system-ui'; ctx.textAlign = 'center';
    ctx.fillText(i === 0 ? 'L' : 'R', x, y - 18);
    ctx.restore();
  }
}
function invoer() { return ($('invoer') && $('invoer').value) || 'handen'; }

// --- meters ---
function buildMeters() {
  const el = $('meters'); el.innerHTML = '';
  const mode = $('mode').value;
  const labels = mode === 'drums' ? pads().map(p => p.n) : mode === 'mel' ? ['TOON'] : mode === 'pitch' ? ['NOOT'] : ['XY'];
  labels.forEach(n => {
    const d = document.createElement('div'); d.className = 'met';
    d.innerHTML = '<div class="b"><div class="f" style="height:0%"></div></div>' + n; el.appendChild(d);
  });
}
function gateMs() { const gv = $('gate').value; if (gv === 'uit') return 0; return 60000 / (+$('bpm').value || 120) * (4 / +gv); }

// --- muis-invoer ---
let muisDown = false;
function muisHit(e) {
  const r = cv.getBoundingClientRect(), mx = e.clientX - r.left, my = e.clientY - r.top;
  const ps = pads(), pw = (ps.length === 8 ? PW8 : PW4) * r.width, ph = (ps.length === 8 ? PH8 : PH4) * r.height;
  ps.forEach((p, z) => {
    const x = (1 - p.x) * r.width - pw / 2, y = p.y * r.height - ph / 2;
    if (mx >= x && mx <= x + pw && my >= y && my <= y + ph) padHit(z, .5 + my / r.height * .5);
  });
}

// --- camera + hoofdloop ---
async function start() {
  const st = $('stat');
  try {
    if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) { st.textContent = 'Geen camera-ondersteuning in deze browser — gebruik Chrome/Edge.'; return; }
    st.textContent = 'Camera opstarten…'; $('start').disabled = true;
    let s = null;
    try { s = await navigator.mediaDevices.getUserMedia({ video: { width: 640, height: 480 } }); }
    catch (e) { s = await navigator.mediaDevices.getUserMedia({ video: true }); }
    $('cam').srcObject = s; run = true; ensureAC(); requestAnimationFrame(tick);
    setTimeout(() => { if (run) { const ps = pads(), E = zoneEnergies(ps); if (E) for (let z = 0; z < ps.length; z++) base[z] = E[z] * 1.4; } }, 1500);
    if (invoer() === 'handen') {
      st.textContent = 'Handmodel laden…';
      const ok = await initHandWorker();
      if (!ok) {
        $('invoer').value = 'classiek';
        st.textContent = '⚠ handmodel niet geladen (' + (handError || 'onbekend') + ') — CLASSIEK-pixeldetectie actief';
      }
    }
    if ($('invoer').value !== 'classiek' || !st.textContent.startsWith('⚠')) st.textContent = '✓ Camera aan — beweeg voor de pads!';
    $('start').textContent = '⏹ STOP';
    $('start').onclick = () => { s.getTracks().forEach(t => t.stop()); run = false; $('start').textContent = '▶ START CAMERA'; $('start').onclick = start; st.textContent = 'Camera uit.'; $('start').disabled = false; };
    $('start').disabled = false;
  } catch (e) {
    $('start').disabled = false;
    st.textContent = e.name === 'NotAllowedError' ? '✗ Camera geblokkeerd — klik op het slotje in de adresbalk en sta camera toe.' : e.name === 'NotFoundError' ? '✗ Geen camera gevonden.' : '✗ Camera-fout: ' + e.message;
  }
}
function tick() {
  if (!run) return;
  const now = performance.now(), mode = $('mode').value, inp = invoer();
  if (inp === 'handen' && handReady) sendHandDetection(now);
  if (mode === 'drums') {
    if (inp === 'classiek') {
      const ps = pads(), E = zoneEnergies(ps);
      if (E) {
        const th = +$('sens').value / 100;
        for (let z = 0; z < ps.length; z++) {
          const v = E[z] - base[z];
          const bars = document.querySelectorAll('#meters .f')[z]; if (bars) bars.style.height = Math.min(100, Math.max(0, v) * 300) + '%';
          if (v > th && now - cool[z] > Math.max(gateMs(), 60)) {
            cool[z] = now;
            if (ps[z].note === 46 && midiOut) midiOut.send([0x89, 42, 0]);
            triggerPad(z, Math.min(1, v * 3), Math.min(127, Math.round(40 + v * 300)));
          }
        }
      }
    }
  } else if (inp === 'classiek') {
    // mel/fx/gtr via pixel-centroid in classieke modus
    if (mode === 'mel') {
      const c = centroid();
      if (c && c.e > .01) {
        const scale = SCALES[$('toon').value | 0];
        const idx = Math.max(0, Math.min(scale.length - 1, Math.floor((1 - c.y) * scale.length)));
        const note = scale[idx], vel = Math.min(127, 50 + Math.round(c.e * 800));
        if (note !== lastNote) {
          lastNote = note;
          if ($('snd').value === 'aan') SYN.mel(Math.min(1, c.e * 5), note);
          noteOn(note, vel); logNote(note, vel);
        }
        const bars = document.querySelectorAll('#meters .f')[0]; if (bars) bars.style.height = (1 - c.y) * 100 + '%';
      } else lastNote = -1;
    } else if (mode === 'gtr') {
      if (!gtrSrc) gtrSetup();
      else { const c = centroid(); if (c && c.e > .005) { gtrCtl(c.x, c.y); const bars = document.querySelectorAll('#meters .f')[0]; if (bars) bars.style.height = c.e * 400 + '%'; } }
    } else if (mode === 'pitch') {
      if (!pitchSrc) { pitchSetup(); } else pitchStep();
    } else if (mode === 'fx') {
      const c = centroid();
      if (c && c.e > .008 && now - fxCool > 40) {
        fxCool = now;
        sendCC(74, (1 - c.x) * 127); sendCC(11, (1 - c.y) * 127);
        const bars = document.querySelectorAll('#meters .f')[0]; if (bars) bars.style.height = c.e * 400 + '%';
      }
    }
  } else {
    if (mode === 'gtr' && !gtrSrc) gtrSetup();
    if (mode === 'pitch' && !pitchSrc) pitchSetup();
    else if (mode === 'pitch') pitchStep();
  }
  teken();
  requestAnimationFrame(tick);
}

// --- UI-bedrading ---
function buildPadCfg() {
  const el = $('padcfg'); el.innerHTML = '';
  pads().forEach((p, z) => {
    const d = document.createElement('button'); d.className = 'sec';
    d.style.cssText = 'padding:6px 10px;font-size:11px;border-color:' + p.col + '66;color:' + p.col;
    d.textContent = p.n + (ownSamp[z] ? ' ●' : '');
    d.onclick = () => { padPick = z; $('padf').click(); };
    el.appendChild(d);
  });
}
function init() {
  $('mode').onchange = () => {
    buildMeters();
    const m = $('mode').value;
    $('stat').textContent = m === 'muis' ? '' : m === 'gtr' ? 'GITAAR-modus: klik in het vak → sluit gitaar/mic aan, beweeg X=wah Y=volume.' : m === 'pitch' ? 'PITCH-modus: klik in het vak → zing/speel in je mic — noten gaan naar MIDI + opname.' : '';
  };
  $('npads').onchange = () => { buildMeters(); buildPadCfg(); };
  buildMeters();
  wireRecButton();
  $('start').onclick = start;
  $('padf').onchange = async () => {
    const f = $('padf').files[0]; if (!f || padPick < 0) return;
    ensureAC();
    try {
      ownSamp[padPick] = await ac.decodeAudioData(await f.arrayBuffer());
      $('ownlist').textContent = '✓ ' + pads()[padPick].n + ' = ' + f.name; buildPadCfg();
    }
    catch (e) { $('ownlist').textContent = 'Kon ' + f.name + ' niet lezen.'; }
  };
  $('own').onclick = () => { if (FREE) { $('stat').innerHTML = '🔒 Eigen sounds laden zit in de <b>CLUB</b>-versie — <a href="bestel.html?p=g2bclub" style="color:#35d07f">word lid (€4,95/mnd)</a>'; return; } $('ownf').click(); };
  if (FREE) { $('dl').style.display = 'none'; }
  $('ownf').onchange = async () => {
    ensureAC(); const fs = [...$('ownf').files]; if (!fs.length) return;
    ownSamp = []; let names = [];
    for (const f of fs.slice(0, 8)) {
      try { ownSamp.push(await ac.decodeAudioData(await f.arrayBuffer())); names.push(f.name); }
      catch (e) { ownSamp.push(null); }
    }
    $('ownlist').textContent = '✓ Geladen: ' + names.join(', '); buildPadCfg();
  };
  buildPadCfg();
  $('bpm').oninput = () => { $('bpmv').textContent = $('bpm').value; };
  $('metr').onchange = () => {
    if (metrIv) { clearInterval(metrIv); metrIv = null; }
    if ($('metr').value === 'aan') {
      ensureAC(); beatN = 0;
      const step = () => {
        const t = ac.currentTime;
        sn(t, beatN % 4 === 0 ? 1600 : 1000, 0, .05, beatN % 4 === 0 ? .5 : .25); beatN++;
        metrIv = setTimeout(step, 60000 / +$('bpm').value);
      };
      step();
    }
  };
  $('cal').onclick = () => {
    const ps = pads(), E = zoneEnergies(ps);
    if (E) for (let z = 0; z < ps.length; z++) base[z] = E[z] * 1.4;
  };
  cv.addEventListener('pointerdown', e => {
    const m = $('mode').value; const r = cv.getBoundingClientRect(); mouseP = { x: (e.clientX - r.left) / r.width, y: (e.clientY - r.top) / r.height };
    if (m === 'gtr') { ensureAC(); if (!gtrSrc) gtrSetup(); else gtrCtl(mouseP.x, mouseP.y); return; }
    if (m === 'pitch') { ensureAC(); if (!pitchSrc) pitchSetup(); return; }
    if (invoer() !== 'muis' && $('mode').value !== 'drums') return;
    if (invoer() !== 'muis') return;
    ensureAC(); muisDown = true; muisHit(e);
  });
  cv.addEventListener('pointermove', e => {
    const m = $('mode').value; const r = cv.getBoundingClientRect(); mouseP = { x: (e.clientX - r.left) / r.width, y: (e.clientY - r.top) / r.height };
    if (m === 'gtr') { if (gtrSrc) gtrCtl(mouseP.x, mouseP.y); return; }
    if (invoer() !== 'muis' || !muisDown) return; muisHit(e);
  });
  addEventListener('pointerup', () => muisDown = false);
  const KEYS = 'asdfjkl;';
  addEventListener('keydown', e => {
    if (e.repeat || e.target.tagName === 'SELECT' || e.target.tagName === 'INPUT') return;
    const m = $('mode').value; if (m !== 'drums') return;
    const z = KEYS.indexOf(e.key.toLowerCase()); if (z < 0) return;
    ensureAC(); padHit(z, e.shiftKey ? 1 : .7);
  });
  ['pointerdown', 'keydown'].forEach(ev => addEventListener(ev, function _mi() { setupMIDI(); }, { once: true }));
  (function drawLoop() { try { const m = $('mode').value; if (m !== 'mel' && !run) teken(); } catch (e) { } requestAnimationFrame(drawLoop); })();
  (function pitchLoop() { try { if ($('mode').value === 'pitch' && pitchSrc && !run) pitchStep(); } catch (e) { } setTimeout(pitchLoop, 60); })();
}
init();

// --- testhaken ---
window.G2BAirdrums = {
  version: 2,
  newHandState, updateHandStrike, padAt,
  constants: { STRIKE_DOWN_MIN, STRIKE_END_VEL, HAND_REFRACT_MS, PAD4, PAD8, PW4, PH4, PW8, PH8 },
  handStates,
  getState() {
    return {
      run, invoer: invoer(), mode: $('mode').value,
      handReady, handError, handDelegate, handInferenceAvg, handResultsSeen,
      hands: handStates.map(h => ({ present: h.present, x: h.x, y: h.y, phase: h.phase, smVy: h.smVy, label: h.label })),
      recOn, recLogLen: recLog.length, free: FREE
    };
  },
  triggerPad, padHit, writeMid, logNote,
  startRecForTest() { recLog = []; recT0 = performance.now(); recOn = true; },
  stopRecForTest() { recOn = false; return writeMid(); },
  start, initHandWorker,
  simulateHand(handIndex, x, y, tMs) {
    const hs = handStates[handIndex];
    hs.present = true;
    const hit = updateHandStrike(hs, x, y, tMs === undefined ? performance.now() : tMs, pads());
    return hit;
  },
  releaseHand(handIndex) { handStates[handIndex].present = false; }
};
})();
