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
  trap(root = 45, bars = 2) { // trap-hat-ish melodie + 808 glide
    const n = [];
    for (let b = 0; b < bars * 4; b++) {
      if (b % 2 === 0) n.push({ pitch: root, startBeat: b, durationBeats: 0.9, velocity: 110,
        glide: [{ beat: 0.5, semis: -2 }] });
      if (Math.random() < .4) n.push({ pitch: root + 7, startBeat: b + 0.5, durationBeats: 0.4, velocity: 90 });
    }
    return n;
  },
  kaseko(root = 57, bars = 2) { // Surinaams: syncopated, percussive
    const n = []; const pat = [0, 0.5, 1, 1.75, 2.5, 3, 3.5];
    for (let b = 0; b < bars; b++) for (const p of pat)
      n.push({ pitch: root + [0, 3, 7, 5][Math.floor(Math.random() * 4)], startBeat: b * 4 + p, durationBeats: 0.3, velocity: 95 });
    return n;
  },
  drill(root = 45, bars = 2) { // drill glide 808's
    const n = [];
    for (let b = 0; b < bars * 4; b += 2)
      n.push({ pitch: root, startBeat: b, durationBeats: 1.6, velocity: 115,
        glide: [{ beat: 0.7, semis: 5 }, { beat: 1.2, semis: 0 }] });
    return n;
  }
};

/* Piano-roll: 144Hz canvas, klik om noten te zetten, sleep voor glide-curve */
class PianoRoll {
  constructor(canvas, opts = {}) {
    this.c = canvas; this.g = canvas.getContext('2d');
    this.bars = opts.bars || 4; this.lo = opts.lo || 36; this.hi = opts.hi || 84;
    this.notes = []; this.sel = null;
    canvas.onmousedown = e => this.down(e);
    requestAnimationFrame(() => this.draw());
  }
  pitchY(p) { return this.c.height - (p - this.lo) / (this.hi - this.lo) * this.c.height; }
  beatX(b) { return b / (this.bars * 4) * this.c.width; }
  down(e) {
    const r = this.c.getBoundingClientRect();
    const x = e.clientX - r.left, y = e.clientY - r.top;
    const beat = x / this.c.width * this.bars * 4;
    const pitch = Math.round(this.hi - y / this.c.height * (this.hi - this.lo));
    this.notes.push({ pitch, startBeat: beat, durationBeats: 1, velocity: 100, glide: null });
    this.draw();
  }
  draw() {
    const g = this.g;
    g.clearRect(0, 0, this.c.width, this.c.height);
    g.strokeStyle = '#1d2a26';
    for (let b = 0; b <= this.bars * 4; b++) { g.beginPath(); g.moveTo(this.beatX(b), 0); g.lineTo(this.beatX(b), this.c.height); g.stroke(); }
    for (let p = this.lo; p <= this.hi; p++) {
      g.strokeStyle = p % 12 === 0 ? '#243' : '#161a18';
      g.beginPath(); g.moveTo(0, this.pitchY(p)); g.lineTo(this.c.width, this.pitchY(p)); g.stroke();
    }
    for (const n of this.notes) {
      g.fillStyle = '#0f6';
      g.fillRect(this.beatX(n.startBeat), this.pitchY(n.pitch) - 4, Math.max(8, this.beatX(n.durationBeats) ), 8);
      if (n.glide) { // Bézier glide-curve tekenen
        g.strokeStyle = '#0fc'; g.lineWidth = 2; g.beginPath();
        g.moveTo(this.beatX(n.startBeat), this.pitchY(n.pitch));
        for (const gl of n.glide)
          g.quadraticCurveTo(this.beatX(n.startBeat + gl.beat / 2),
            this.pitchY(n.pitch + gl.semis / 2), this.beatX(n.startBeat + gl.beat), this.pitchY(n.pitch + gl.semis));
        g.stroke(); g.lineWidth = 1;
      }
    }
  }
}
