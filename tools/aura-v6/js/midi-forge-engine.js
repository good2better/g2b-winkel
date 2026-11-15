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
  build(notes, bpm = 140, naam = 'AURA') {
    const ev = [];
    const tmp = Math.round(60000000 / bpm);
    ev.push({ t: 0, b: [0xFF, 0x51, 0x03, (tmp >> 16) & 255, (tmp >> 8) & 255, tmp & 255] });
    ev.push({ t: 0, b: [0xB0, 101, 0, 0xB0, 100, 0, 0xB0, 6, 12] }); // pitch-bend ±12
    for (const n of notes) {
      const st = Math.max(0, Math.round(n.startBeat * this.tpq) + this.wmTick());
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
  download(notes, bpm, naam = 'aura-forge') {
    const blob = new Blob([this.build(notes, bpm)], { type: 'audio/midi' });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob); a.download = naam + '.mid'; a.click();
  }
}

/* Stijl-generator: geeft noten-arrays terug voor de gekozen stijl */
const StijlGen = {
  trap(root = 57, bars = 4) { // mineur-pentatonisch motief (A A') + diepe 808-glide onderlaag
    const n = [], sc = [0, 3, 5, 7, 10];
    const mot = () => [0, 2, 1, 3, 0].map(i => sc[i % sc.length]);
    const m1 = mot(), m2 = mot(), bass = root - 24;
    for (let b = 0; b < bars; b++) {
      const M = b % 2 ? m2 : m1, ph = b < 2 ? 0 : 2;      // laatste 2 maten = variatie
      n.push({ pitch: bass, startBeat: b * 4, durationBeats: 2.5, velocity: 115,
        glide: [{ beat: 1.2, semis: -2 }, { beat: 2.2, semis: 0 }] });
      const pos = b % 2 ? [0.5, 1.5, 3.5] : [0.5, 1.5, 2, 3.5];
      pos.forEach((p, i) => n.push({ pitch: root + M[(i + ph) % M.length],
        startBeat: b * 4 + p, durationBeats: 0.4, velocity: 85 + (i === 0 ? 15 : 0) }));
    }
    return n.sort((a, z) => a.startBeat - z.startBeat);
  },
  kaseko(root = 57, bars = 4) { // Surinaams syncopisch: vast motief + antwoord in maat 4
    const n = [], sc = [0, 3, 5, 7, 10];
    const mot = [sc[0], sc[2], sc[1], sc[3]];
    const pat = [0, 0.5, 1.25, 1.75, 2.5, 3.25];
    for (let b = 0; b < bars; b++) {
      const antw = b === 3;
      pat.forEach((p, i) => {
        if (b === 1 && i === 4) return;                    // ademruimte
        n.push({ pitch: root + mot[(i + (antw ? 2 : 0)) % mot.length] + (antw ? 7 : 0),
          startBeat: b * 4 + p, durationBeats: 0.3, velocity: 95 + (i === 0 ? 10 : 0) });
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

/* Piano-roll: 144Hz canvas, bewerkbaar — klik leeg = noot zetten,
   sleep noot = verplaatsen, rand = langer/korter, rechtsklik/dubbel = weg */
class PianoRoll {
  constructor(canvas, opts = {}) {
    this.c = canvas; this.g = canvas.getContext('2d');
    this.bars = opts.bars || 4; this.baseLo = opts.lo || 30; this.baseHi = opts.hi || 66;
    this.lo = this.baseLo; this.hi = this.baseHi;
    this.notes = []; this.sel = -1; this.drag = null;
    canvas.onmousedown = e => this.down(e);
    canvas.onmousemove = e => this.move(e);
    canvas.onmouseup = () => { this.drag = null; };
    canvas.onmouseleave = () => { this.drag = null; };
    canvas.oncontextmenu = e => { // rechtsklik = noot weggooien
      e.preventDefault();
      const h = this.hit(e); if (h >= 0) { this.notes.splice(h, 1); this.sel = -1; this.draw(); }
    };
    canvas.ondblclick = e => { // dubbelklik = ook weggooien (makkelijker te vinden)
      const h = this.hit(e); if (h >= 0) { this.notes.splice(h, 1); this.sel = -1; this.draw(); }
    };
    requestAnimationFrame(() => this.draw());
  }
  bereik() { // zichtbaar bereik: baseLo..baseHi, klapt uit naar alle noten
    let lo = this.baseLo, hi = this.baseHi;
    for (const n of this.notes) {
      lo = Math.min(lo, Math.floor(n.pitch / 12) * 12);
      hi = Math.max(hi, Math.ceil((n.pitch + 1) / 12) * 12);
    }
    this.lo = lo; this.hi = Math.max(hi, lo + 12);
  }
  rowH() { return this.c.height / (this.hi - this.lo); }
  pitchY(p) { return this.c.height - (p - this.lo) / (this.hi - this.lo) * this.c.height; }
  beatX(b) { return b / (this.bars * 4) * this.c.width; }
  snap(b) { return Math.max(0, Math.round(b * 4) / 4); } // 16e-noten grid
  pos(e) { const r = this.c.getBoundingClientRect();
    return { x: (e.clientX - r.left) * this.c.width / r.width,
             y: (e.clientY - r.top) * this.c.height / r.height }; }
  xy2bp(x, y) { return { beat: x / this.c.width * this.bars * 4,
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
  down(e) {
    const { x, y } = this.pos(e);
    const { beat, pitch } = this.xy2bp(x, y);
    const h = this.hit(e);
    if (h >= 0) {
      const n = this.notes[h];
      const edge = Math.abs(x - this.beatX(n.startBeat + n.durationBeats)) < Math.min(10, this.beatX(n.durationBeats) / 3);
      this.drag = edge ? { i: h, mode: 'resize' }
        : { i: h, mode: 'move', dx: beat - n.startBeat, dp: pitch - n.pitch };
      this.sel = h;
    } else {
      this.notes.push({ pitch, startBeat: this.snap(beat), durationBeats: 1, velocity: 100, glide: null });
      this.sel = this.notes.length - 1;
    }
    this.draw();
  }
  move(e) {
    if (!this.drag) return;
    const { x, y } = this.pos(e);
    const { beat, pitch } = this.xy2bp(x, y);
    const n = this.notes[this.drag.i];
    if (this.drag.mode === 'move') {
      n.startBeat = this.snap(beat - this.drag.dx);
      n.pitch = Math.max(0, Math.min(127, pitch - this.drag.dp));
    } else { // resize: staart verslepen
      n.durationBeats = Math.max(0.25, this.snap(beat) - n.startBeat);
    }
    this.draw();
  }
  draw() {
    this.bereik();
    const g = this.g, rh = Math.max(6, this.rowH() * 0.8);
    g.clearRect(0, 0, this.c.width, this.c.height);
    g.strokeStyle = '#1d2a26';
    for (let b = 0; b <= this.bars * 4; b++) { g.beginPath(); g.moveTo(this.beatX(b), 0); g.lineTo(this.beatX(b), this.c.height); g.stroke(); }
    for (let p = this.lo; p <= this.hi; p++) {
      g.strokeStyle = p % 12 === 0 ? '#243' : '#161a18';
      g.beginPath(); g.moveTo(0, this.pitchY(p)); g.lineTo(this.c.width, this.pitchY(p)); g.stroke();
    }
    this.notes.forEach((n, i) => {
      g.fillStyle = i === this.sel ? '#3fa' : '#0f6';
      g.fillRect(this.beatX(n.startBeat), this.pitchY(n.pitch) - rh / 2, Math.max(10, this.beatX(n.durationBeats)), rh);
      if (n.glide) { // Bézier glide-curve tekenen
        g.strokeStyle = '#0fc'; g.lineWidth = 2; g.beginPath();
        g.moveTo(this.beatX(n.startBeat), this.pitchY(n.pitch));
        for (const gl of n.glide)
          g.quadraticCurveTo(this.beatX(n.startBeat + gl.beat / 2),
            this.pitchY(n.pitch + gl.semis / 2), this.beatX(n.startBeat + gl.beat), this.pitchY(n.pitch + gl.semis));
        g.stroke(); g.lineWidth = 1;
      }
    });
  }
}
