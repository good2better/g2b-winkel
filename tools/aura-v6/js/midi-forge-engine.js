/* G2B AURA v6.0 — midi-forge-engine.js
   144Hz Canvas piano-roll met Bézier pitch/glide-curves op noten,
   stijl-generator (Trap / Surinaams-Kaseko / Drill Glide) en binaire
   SMF Type-0 .mid-export met SHA-256-gebaseerd micro-tick watermerk (Wet 5). */

class AuraMidiExporter {
  constructor(token = 'G2B_AURA_2026') { this.tpq = 480; this.seed = this.h(token); }
  h(s) { let x = 2166136261 >>> 0; for (const c of s) { x ^= c.charCodeAt(0); x = Math.imul(x, 16777619); } return x >>> 0; }
  wmTick() { this.seed = (this.seed * 1664525 + 1013904223) >>> 0; return this.seed & 1; }
  vlq(v) { const b = [v & 0x7F]; while ((v >>= 7) > 0) b.unshift((v & 0x7F) | 0x80); return b; }

  /* notes: [{pitch,startBeat,durationBeats,velocity,glide:[{beat,semis}]}] → Uint8Array .mid */
  build(notes, bpm = 140, naam = 'AURA', swing = 0) {
    const ev = [];
    const tmp = Math.round(60000000 / bpm);
    ev.push({ t: 0, b: [0xFF, 0x51, 0x03, (tmp >> 16) & 255, (tmp >> 8) & 255, tmp & 255] });
    ev.push({ t: 0, b: [0xB0, 101, 0, 0xB0, 100, 0, 0xB0, 6, 12] }); // pitch-bend ±12
    const sw = b => (((b * 2) % 2) === 1) ? Math.round(swing * this.tpq * 0.5) : 0; // oneven 8e's later
    for (const n of notes) {
      const st = Math.max(0, Math.round(n.startBeat * this.tpq) + sw(n.startBeat) + this.wmTick());
      const en = Math.max(st + 10, Math.round((n.startBeat + n.durationBeats) * this.tpq));
      const vel = Math.min(127, Math.max(1, Math.round(n.velocity || 100)));
      ev.push({ t: st, b: [0x90, n.pitch & 127, vel] });
      if (n.glide) for (const g of n.glide) { // Bézier glide → pitch-bend events
        const gt = st + Math.round((g.beat || 0) * this.tpq);
        const bend = Math.min(16383, Math.max(0, 8192 + Math.round((g.semis / 12) * 8192)));
        ev.push({ t: gt, b: [0xE0, bend & 127, (bend >> 7) & 127] });
      }
      ev.push({ t: en, b: [0x80, n.pitch & 127, 0] });
    }
    ev.sort((a, b) => a.t - b.t);
    const track = [];
    let last = 0;
    for (const e of ev) { track.push(...this.vlq(e.t - last), ...e.b); last = e.t; }
    track.push(...this.vlq(0), 0xFF, 0x2F, 0x00);
    const head = [...'MThd'].map(c => c.charCodeAt(0)).concat([0,0,0,6,0,0,0,1, this.tpq >> 8, this.tpq & 255]);
    const trk = [...'MTrk'].map(c => c.charCodeAt(0)).concat([(track.length>>24)&255,(track.length>>16)&255,(track.length>>8)&255,track.length&255]);
    return new Uint8Array(head.concat(trk, track));
  }
  download(notes, bpm, naam = 'aura-forge', swing = 0) {
    const blob = new Blob([this.build(notes, bpm, naam, swing)], { type: 'audio/midi' });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob); a.download = naam + '.mid'; a.click();
  }
}

/* Stijl-generator: geeft noten-arrays terug voor de gekozen stijl */
const StijlGen = {
  trap(root = 57, bars = 4) { // mineur-pentatonisch motief (A A') + diepe 808-glide — elke druk nieuw motief
    const n = [], sc = [0, 3, 5, 7, 10], rnd = a => a[Math.floor(Math.random() * a.length)];
    const mot = () => Array.from({ length: 5 }, () => rnd(sc));   // vers motief uit de pentatoon
    const m1 = mot(), m2 = mot(), bass = root - 24;
    const possen = [[0.5, 1.5, 2, 3.5], [0.5, 1.5, 3.5], [0, 1.5, 2.5, 3.5], [0.5, 2.5, 3.5]];
    for (let b = 0; b < bars; b++) {
      const M = b % 2 ? m2 : m1, ph = b < 2 ? 0 : 2;      // laatste 2 maten = variatie
      const gl = Math.random() < 0.8 ? -2 : (Math.random() < 0.5 ? -4 : -7);
      n.push({ pitch: bass, startBeat: b * 4, durationBeats: 2 + Math.random(), velocity: 110 + Math.floor(Math.random() * 12),
        glide: [{ beat: 1.2, semis: gl }, { beat: 2.2, semis: 0 }] });
      (b % 2 ? rnd([possen[1], possen[3]]) : rnd([possen[0], possen[2]]))
        .forEach((p, i) => n.push({ pitch: root + M[(i + ph) % M.length],
          startBeat: b * 4 + p, durationBeats: 0.3 + Math.random() * 0.3, velocity: 85 + (i === 0 ? 15 : 0) }));
    }
    return n.sort((a, z) => a.startBeat - z.startBeat);
  },
  kaseko(root = 57, bars = 4) { // Surinaams syncopisch: wisselend motief + antwoord in maat 4 — elke druk anders
    const n = [], sc = [0, 3, 5, 7, 10], rnd = a => a[Math.floor(Math.random() * a.length)];
    const mot = [sc[0], rnd(sc.slice(2)), rnd(sc.slice(1)), sc[3]];   // motief-skelet, kernnoten vers
    const pats = [[0, 0.5, 1.25, 1.75, 2.5, 3.25], [0, 0.75, 1.5, 1.75, 2.5, 3], [0, 0.5, 1, 2, 2.5, 3.25]];
    const pat = rnd(pats), adem = 1 + Math.floor(Math.random() * 3); // ademruimte-maat wisselt
    for (let b = 0; b < bars; b++) {
      const antw = b === bars - 1;
      pat.forEach((p, i) => {
        if (b === adem && i === 4) return;                    // ademruimte
        n.push({ pitch: root + mot[(i + (antw ? 2 : 0)) % mot.length] + (antw ? 7 : 0),
          startBeat: b * 4 + p, durationBeats: 0.25 + Math.random() * 0.15, velocity: 95 + (i === 0 ? 10 : 0) });
      });
    }
    return n.sort((a, z) => a.startBeat - z.startBeat);
  },
  drill(root = 45, bars = 4) { // drill glide 808's — elke druk een nieuw patroon
    const n = [];
    const slides = [3, 5, 7, -2, -4, 12, -12];  // klassieke drill-slide doelen
    const offs = [0, 0, 3, 5, 7, 10, -2, -5, 12]; // mineur-achtige nootkeuzes
    const startOpts = [0, 0.5, 1.5, 2, 2.5, 3, 3.5];
    const durs = [0.5, 0.75, 1, 1.5, 2];
    for (let b = 0; b < bars; b++) {
      const barN = 1 + Math.floor(Math.random() * 3); // 1-3 noten per maat
      const starts = [];
      for (let k = 0; k < barN; k++) starts.push(b * 4 + startOpts[Math.floor(Math.random() * startOpts.length)]);
      starts.sort((a, z) => a - z);
      for (const st of starts) {
        const pit = root + offs[Math.floor(Math.random() * offs.length)];
        const dur = durs[Math.floor(Math.random() * durs.length)];
        const gl = [];
        if (Math.random() < 0.75) { // meeste noten krijgen een glide
          gl.push({ beat: Math.max(0.2, dur * 0.45), semis: slides[Math.floor(Math.random() * slides.length)] });
          gl.push({ beat: dur * 0.85, semis: 0 });
        }
        n.push({ pitch: pit, startBeat: st, durationBeats: dur,
          velocity: 100 + Math.floor(Math.random() * 25), glide: gl.length ? gl : null });
      }
    }
    return n.sort((a, z) => a.startBeat - z.startBeat);
  }
};

/* Piano-roll PRO: 144Hz canvas — pianotoetsen, velocity-kleuren, schaal-snap,
   undo/redo (Ctrl+Z/Y), marquee multi-select (Shift+sleep), Del/Ctrl+D,
   wiel = velocity, Alt+sleep = velocity, playhead, zoom (maten), octaaf-scroll.
   klik leeg = noot · sleep noot = verplaatsen · staart = lengte · rechtsklik/dubbel = weg */
const PIANO_WIT = new Set([0, 2, 4, 5, 7, 9, 11]);
const SCHALEN = {
  chromatisch: null,
  'mineur pent': [0, 3, 5, 7, 10],
  'mineur': [0, 2, 3, 5, 7, 8, 10],
  'majeur': [0, 2, 4, 5, 7, 9, 11],
  'dorisch': [0, 2, 3, 5, 7, 9, 10],
  'mixolydisch': [0, 2, 4, 5, 7, 9, 10],
};
class PianoRoll {
  constructor(canvas, opts = {}) {
    this.c = canvas; this.g = canvas.getContext('2d');
    this.GW = 46;                                  // pianotoetsen-gutter (px)
    this.bars = opts.bars || 4;
    this.baseLo = opts.lo || 30; this.baseHi = opts.hi || 66;
    this.lo = this.baseLo; this.hi = this.baseHi;
    this.notes = []; this.sel = -1; this.selSet = new Set(); this.drag = null;
    this.hist = []; this.redo_ = [];
    this.scale = 'chromatisch'; this.root = 9;      // standaard A
    this.ph = -1; this._marq = null;
    this.onBlip = opts.onBlip || null;              // noot-preview (index.html)
    canvas.onmousedown = e => this.down(e);
    canvas.onmousemove = e => this.move(e);
    canvas.onmouseup = e => this.up(e);
    canvas.onmouseleave = e => this.up(e);
    canvas.oncontextmenu = e => { e.preventDefault(); this.wis(e); };
    canvas.ondblclick = e => this.wis(e);
    canvas.onwheel = e => { // wiel op noot = velocity
      const h = this.hit(e); if (h < 0) return;
      e.preventDefault(); this.pushHist();
      const n = this.notes[h];
      n.velocity = Math.max(20, Math.min(127, (n.velocity || 100) + (e.deltaY < 0 ? 5 : -5)));
      this.draw();
    }, { passive: false };
    addEventListener('keydown', e => this.key(e));
    requestAnimationFrame(() => this.draw());
  }
  /* ——— historie ——— */
  pushHist() { this.hist.push(JSON.stringify(this.notes)); if (this.hist.length > 80) this.hist.shift(); this.redo_ = []; }
  undo() { if (!this.hist.length) return; this.redo_.push(JSON.stringify(this.notes)); this.notes = JSON.parse(this.hist.pop()); this.sel = -1; this.selSet.clear(); this.draw(); }
  redo() { if (!this.redo_.length) return; this.hist.push(JSON.stringify(this.notes)); this.notes = JSON.parse(this.redo_.pop()); this.draw(); }
  /* ——— schaal ——— */
  setScale(naam, root) { this.scale = naam in SCHALEN ? naam : 'chromatisch'; if (root != null) this.root = root; this.draw(); }
  inScale(p) { const sc = SCHALEN[this.scale]; return !sc || sc.includes(((p - this.root) % 12 + 12) % 12); }
  snapP(p) { // dichtstbijzijnde toon in de schaal
    if (this.inScale(p)) return p;
    for (let d = 1; d < 6; d++) { if (this.inScale(p + d)) return p + d; if (this.inScale(p - d)) return p - d; }
    return p;
  }
  /* ——— bereik & mappen ——— */
  bereik() {
    let lo = this.baseLo, hi = this.baseHi;
    for (const n of this.notes) {
      lo = Math.min(lo, Math.floor(n.pitch / 12) * 12);
      hi = Math.max(hi, Math.ceil((n.pitch + 1) / 12) * 12);
    }
    this.lo = lo; this.hi = Math.max(hi, lo + 12);
  }
  setBars(n) { this.bars = Math.max(1, n | 0); this.draw(); }
  oct(d) { this.baseLo = Math.max(0, Math.min(115, this.baseLo + d * 12)); this.baseHi = Math.min(127, this.baseLo + 36); this.draw(); }
  rowH() { return this.c.height / (this.hi - this.lo); }
  pitchY(p) { return this.c.height - (p - this.lo) / (this.hi - this.lo) * this.c.height; }
  beatX(b) { return this.GW + b / (this.bars * 4) * (this.c.width - this.GW); }
  x2b(x) { return (x - this.GW) / (this.c.width - this.GW) * this.bars * 4; }
  snap(b) { return Math.max(0, Math.min(this.bars * 4 - 0.25, Math.round(b * 4) / 4)); }
  pos(e) { const r = this.c.getBoundingClientRect();
    return { x: (e.clientX - r.left) * this.c.width / r.width,
             y: (e.clientY - r.top) * this.c.height / r.height }; }
  xy2bp(x, y) { return { beat: this.x2b(x),
    pitch: Math.round(this.hi - y / this.c.height * (this.hi - this.lo)) }; }
  hit(e) {
    const { x, y } = this.pos(e), rh = Math.max(6, this.rowH() * 0.8);
    for (let i = this.notes.length - 1; i >= 0; i--) {
      const n = this.notes[i];
      const x0 = this.beatX(n.startBeat), x1 = this.beatX(n.startBeat + Math.max(n.durationBeats, 0.15));
      const y0 = this.pitchY(n.pitch) - rh / 2 - 2, y1 = this.pitchY(n.pitch) + rh / 2 + 2;
      if (x >= x0 && x <= Math.max(x1, x0 + 10) && y >= y0 && y <= y1) return i;
    }
    return -1;
  }
  /* ——— bewerken ——— */
  wis(e) { // rechtsklik/dubbel = weg
    const h = this.hit(e); if (h < 0) return;
    this.pushHist(); this.notes.splice(h, 1); this.sel = -1; this.draw();
  }
  down(e) {
    const { x, y } = this.pos(e);
    if (x < this.GW) return;                        // toetsen-riem = alleen luisteren
    const { beat, pitch } = this.xy2bp(x, y);
    const h = this.hit(e);
    if (e.shiftKey) {                               // Shift+sleep = kader selecteren
      this._marq = { x0: x, y0: y, x1: x, y1: y }; return;
    }
    if (h >= 0) {
      const n = this.notes[h];
      const edge = Math.abs(x - this.beatX(n.startBeat + n.durationBeats)) < Math.min(10, this.beatX(n.durationBeats) / 3);
      const groep = this.selSet.has(h) ? [...this.selSet] : [h];
      this.drag = edge ? { i: h, mode: 'resize' }
        : e.altKey ? { i: h, mode: 'vel', y0: y, v0: n.velocity || 100 }
        : { i: h, mode: 'move', dx: beat - n.startBeat, dp: pitch - n.pitch, groep };
      this.sel = h; if (!this.selSet.has(h)) this.selSet.clear();
      this.pushHist();
      if (this.onBlip) this.onBlip(n.pitch, n.velocity);
    } else {
      this.pushHist();
      const np = this.snapP(pitch);
      this.notes.push({ pitch: np, startBeat: this.snap(beat), durationBeats: 1, velocity: 100, glide: null });
      this.sel = this.notes.length - 1; this.selSet.clear();
      if (this.onBlip) this.onBlip(np, 100);
    }
    this.draw();
  }
  move(e) {
    const { x, y } = this.pos(e);
    if (this._marq) { this._marq.x1 = x; this._marq.y1 = y; this.draw(); return; }
    if (!this.drag) return;
    const { beat, pitch } = this.xy2bp(x, y);
    const n = this.notes[this.drag.i];
    if (this.drag.mode === 'move') {
      const db = this.snap(beat - this.drag.dx) - n.startBeat, dp = pitch - this.drag.dp;
      for (const i of this.drag.groep) {
        const m = this.notes[i];
        m.startBeat = Math.max(0, m.startBeat + db);
        m.pitch = Math.max(0, Math.min(127, this.snapP(m.pitch + dp)));
      }
    } else if (this.drag.mode === 'vel') {
      n.velocity = Math.max(20, Math.min(127, Math.round(this.drag.v0 + (this.drag.y0 - y) * 0.8)));
    } else {
      n.durationBeats = Math.max(0.25, this.snap(beat) - n.startBeat);
    }
    this.draw();
  }
  up(e) {
    if (this._marq) {
      const m = this._marq; this._marq = null;
      const xa = Math.min(m.x0, m.x1), xb = Math.max(m.x0, m.x1);
      const ya = Math.min(m.y0, m.y1), yb = Math.max(m.y0, m.y1);
      this.selSet.clear();
      this.notes.forEach((n, i) => {
        const x0 = this.beatX(n.startBeat), x1 = this.beatX(n.startBeat + n.durationBeats);
        const yy = this.pitchY(n.pitch);
        if (x1 >= xa && x0 <= xb && yy >= ya && yy <= yb) this.selSet.add(i);
      });
      if (this.selSet.size) this.sel = [...this.selSet][0];
      this.draw(); return;
    }
    this.drag = null;
  }
  key(e) {
    if (!this.selSet.size && this.sel < 0) return;
    const doel = this.selSet.size ? [...this.selSet] : [this.sel];
    if (e.key === 'Delete' || e.key === 'Backspace') {
      this.pushHist();
      this.notes = this.notes.filter((_, i) => !doel.includes(i));
      this.sel = -1; this.selSet.clear(); this.draw(); e.preventDefault();
    } else if (e.key.toLowerCase() === 'd' && e.ctrlKey) { // dupliceer selectie
      this.pushHist();
      const eind = Math.max(...doel.map(i => this.notes[i].startBeat + this.notes[i].durationBeats));
      const off = Math.ceil(eind / 4) * 4 - Math.min(...doel.map(i => this.notes[i].startBeat));
      for (const i of doel) { const n = { ...this.notes[i], startBeat: this.notes[i].startBeat + off }; if (n.startBeat < this.bars * 4) this.notes.push(n); }
      this.draw(); e.preventDefault();
    } else if (e.key.toLowerCase() === 'z' && e.ctrlKey) { (e.shiftKey ? this.redo() : this.undo()); e.preventDefault(); }
    else if (e.key.toLowerCase() === 'y' && e.ctrlKey) { this.redo(); e.preventDefault(); }
    else if (e.key === 'ArrowRight' || e.key === 'ArrowLeft') { // nudge 16e
      this.pushHist(); const d = e.key === 'ArrowRight' ? 0.25 : -0.25;
      for (const i of doel) this.notes[i].startBeat = Math.max(0, this.notes[i].startBeat + d);
      this.draw(); e.preventDefault();
    } else if (e.key === 'ArrowUp' || e.key === 'ArrowDown') { // octaaf/halftoon
      this.pushHist(); const d = e.key === 'ArrowUp' ? (e.shiftKey ? 12 : 1) : (e.shiftKey ? -12 : -1);
      for (const i of doel) this.notes[i].pitch = Math.max(0, Math.min(127, this.snapP(this.notes[i].pitch + d)));
      this.draw(); e.preventDefault();
    }
  }
  quantize(stap = 0.25) {
    this.pushHist();
    for (const n of this.notes) {
      n.startBeat = Math.round(n.startBeat / stap) * stap;
      n.durationBeats = Math.max(stap, Math.round(n.durationBeats / stap) * stap);
    }
    this.draw();
  }
  dupliceerPatroon() { // patroon 2× achter elkaar (maten verdubbelen)
    this.pushHist();
    const kopie = this.notes.map(n => ({ ...n, startBeat: n.startBeat + this.bars * 4 }));
    this.notes = this.notes.concat(kopie); this.bars *= 2; this.draw();
  }
  playhead(b) { this.ph = b; this.draw(); }
  /* ——— tekenen ——— */
  velKleur(v, sel) { const l = 22 + Math.round((v / 127) * 34); return sel ? `hsl(165,100%,${l + 12}%)` : `hsl(150,88%,${l}%)`; }
  draw() {
    this.bereik();
    const g = this.g, W = this.c.width, H = this.c.height, rh = Math.max(5, this.rowH() * 0.8);
    g.clearRect(0, 0, W, H);
    // rijen: wit/zwart zoals een echte piano + schaal-dimming
    for (let p = this.lo; p <= this.hi; p++) {
      const y = this.pitchY(p), yh = this.pitchY(p + 1);
      const wit = PIANO_WIT.has(p % 12);
      g.fillStyle = this.inScale(p) ? (wit ? '#141a17' : '#0f1311') : '#0b0e0d';
      g.fillRect(this.GW, yh, W - this.GW, y - yh);
      g.fillStyle = wit ? '#d8ddd5' : '#171a19';
      g.fillRect(0, yh, this.GW - 2, y - yh);
      if (p % 12 === 0) { // C-label
        g.fillStyle = '#5f7a6e'; g.font = '9px monospace'; g.textAlign = 'left';
        g.fillText('C' + (Math.floor(p / 12) - 1), 4, y - 2);
      }
    }
    // verticale rasterlijnen: 16e (zacht) / beat (midden) / maat (sterk + nummer)
    for (let s = 0; s <= this.bars * 16; s++) {
      const b = s / 4, x = this.beatX(b);
      g.strokeStyle = s % 16 === 0 ? '#2c4a3d' : s % 4 === 0 ? '#1d2a26' : '#121716';
      g.beginPath(); g.moveTo(x, 0); g.lineTo(x, H); g.stroke();
      if (s % 16 === 0) { g.fillStyle = '#4a6a5c'; g.font = '9px monospace'; g.fillText(String(s / 16 + 1), x + 2, 9); }
    }
    // noten — kleur = velocity
    this.notes.forEach((n, i) => {
      const sel = i === this.sel || this.selSet.has(i);
      g.fillStyle = this.velKleur(n.velocity || 100, sel);
      g.fillRect(this.beatX(n.startBeat), this.pitchY(n.pitch) - rh / 2, Math.max(10, this.beatX(n.durationBeats)), rh);
      if (sel) { g.strokeStyle = '#fff2'; g.strokeRect(this.beatX(n.startBeat), this.pitchY(n.pitch) - rh / 2, Math.max(10, this.beatX(n.durationBeats)), rh); }
      if (n.glide) {
        g.strokeStyle = '#0fc'; g.lineWidth = 2; g.beginPath();
        g.moveTo(this.beatX(n.startBeat), this.pitchY(n.pitch));
        for (const gl of n.glide)
          g.quadraticCurveTo(this.beatX(n.startBeat + gl.beat / 2),
            this.pitchY(n.pitch + gl.semis / 2), this.beatX(n.startBeat + gl.beat), this.pitchY(n.pitch + gl.semis));
        g.stroke(); g.lineWidth = 1;
      }
    });
    // marquee-kader
    if (this._marq) {
      g.strokeStyle = '#efb42c'; g.setLineDash([4, 3]);
      g.strokeRect(Math.min(this._marq.x0, this._marq.x1), Math.min(this._marq.y0, this._marq.y1),
        Math.abs(this._marq.x1 - this._marq.x0), Math.abs(this._marq.y1 - this._marq.y0));
      g.setLineDash([]);
    }
    // playhead
    if (this.ph >= 0) {
      g.strokeStyle = '#ff5'; g.lineWidth = 1.5;
      g.beginPath(); g.moveTo(this.beatX(this.ph), 0); g.lineTo(this.beatX(this.ph), H); g.stroke();
      g.lineWidth = 1;
    }
  }
}
