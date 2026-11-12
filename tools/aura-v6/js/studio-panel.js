/* G2B AURA v6.0 — studio-panel.js
   Volwaardig opname- & loop-veld: maatraster (4 maten × 16e), meelopende playhead,
   transportbalk (direct spelen/play/stop/rec/loop/step/wis/.mid/.wav).
   Noten verschijnen als groene/cyaan balkjes op toonhoogte in het raster.
   Gebruik: const panel = StudioPanel.bouw(el, {bpm:120}); panel.hit(pitch,vel) vanuit play(). */
const StudioPanel = {
  bouw(el, opts = {}) {
    const bpm = opts.bpm || 120, bars = 4, steps = 16, N = bars * steps;
    const P = { rec: false, loop: true, stepRec: false, pos: -1, timer: null,
                notes: [], _pb: false, playing: false };
    /* notes: {p: midi, s: stap, d: stappen-duur, v: vel} */

    el.innerHTML = '';
    const box = document.createElement('div'); box.className = 'studio';
    box.innerHTML = `
      <div class="sp-trans">
        <button id="spDirect" class="sp-main">⚡ DIRECT SPELEN</button>
        <button id="spPlay">▶ SPEEL</button>
        <button id="spStop">■ STOP</button>
        <button id="spRec">● OPN</button>
        <button id="spLoop" class="on">↻ LOOP: AAN</button>
        <button id="spStep">STAP-INSPELEN</button>
        <button id="spWis">WIS RASTER</button>
        <button id="spMid">↓ .MID</button>
        <button id="spWav">↓ .WAV</button>
        <span id="spStat" class="hint"></span>
      </div>
      <canvas class="sp-grid" width="720" height="110"></canvas>`;
    el.appendChild(box);
    const cvs = box.querySelector('canvas'), g = cvs.getContext('2d');
    const st = box.querySelector('#spStat');

    /* ---------- raster tekenen ---------- */
    const PITCH_LO = 36, PITCH_HI = 84;
    const draw = () => {
      const W = cvs.width, w = W / N, H = cvs.height, top = 18;
      g.fillStyle = '#0d1512'; g.fillRect(0, 0, W, H);
      // maat-nummers + beat-lijnen
      for (let b = 0; b <= bars; b++) {
        const x = b * 16 * w;
        g.strokeStyle = '#3a5'; g.lineWidth = b < bars ? 1.5 : 0;
        if (b < bars) { g.beginPath(); g.moveTo(x, 0); g.lineTo(x, H); g.stroke();
          g.fillStyle = '#7c9'; g.font = 'bold 11px sans-serif'; g.fillText('Maat ' + (b + 1), x + 4, 12); }
      }
      for (let i = 0; i < N; i++) {
        const x = i * w;
        g.strokeStyle = i % 4 === 0 ? '#243' : '#1a2a20'; g.lineWidth = 1;
        g.beginPath(); g.moveTo(x, top); g.lineTo(x, H); g.stroke();
      }
      // opgenomen noten als balkjes op toonhoogte
      for (const n of P.notes) {
        const y = top + (1 - (n.p - PITCH_LO) / (PITCH_HI - PITCH_LO)) * (H - top - 6);
        g.fillStyle = P.rec ? '#0f6' : '#0cf';
        g.fillRect(n.s * w + 1, Math.max(top + 1, y - 2), Math.max(3, (n.d || 1) * w - 2), 4);
      }
      // playhead
      if (P.pos >= 0) {
        g.fillStyle = '#0fc'; g.fillRect(P.pos * w, 0, 2, H);
      }
    };
    cvs.onclick = e => {  // klik = stap wissen/plaatsen (losse klik = wis noten op stap)
      const r = cvs.getBoundingClientRect();
      const i = Math.floor((e.clientX - r.left) / r.width * N);
      const before = P.notes.length;
      P.notes = P.notes.filter(n => n.s !== i);
      if (P.notes.length === before) P.notes.push({ p: 60, s: i, d: 1, v: 100 });
      draw();
    };

    /* ---------- transport ---------- */
    const tick = () => {
      P.pos = (P.pos + 1) % N;
      // playback van opgenomen noten
      for (const n of P.notes.filter(n => n.s === P.pos)) {
        P._pb = true;
        try { InstrumentMatrix.play(n.p, { velocity: n.v }); } catch (e) {}
        P._pb = false;
      }
      draw();
    };
    const start = () => { if (!P.timer) { P.timer = setInterval(tick, 60000 / bpm / 4); P.playing = true; } };
    const halt = () => { clearInterval(P.timer); P.timer = null; P.pos = -1; P.playing = false; draw(); };

    box.querySelector('#spPlay').onclick = () => { Aura.resume(); start(); };
    box.querySelector('#spStop').onclick = () => { halt(); P.rec = false; updRec(); Aura.panic(); };
    box.querySelector('#spLoop').onclick = e => { P.loop = !P.loop;
      e.target.textContent = P.loop ? '↻ LOOP: AAN' : '↻ LOOP: UIT';
      e.target.classList.toggle('on', P.loop); };
    box.querySelector('#spStep').onclick = e => { P.stepRec = !P.stepRec;
      e.target.classList.toggle('on', P.stepRec);
      st.textContent = P.stepRec ? 'stap-inspelen aan — elke noot = volgende stap' : ''; };
    box.querySelector('#spWis').onclick = () => { P.notes = []; draw(); st.textContent = 'raster leeg'; };
    const updRec = () => { const b = box.querySelector('#spRec');
      b.classList.toggle('on', P.rec); b.textContent = P.rec ? '● OPN (aan)' : '● OPN'; };
    box.querySelector('#spRec').onclick = () => { P.rec = !P.rec; updRec();
      if (P.rec) { Aura.resume(); start(); st.textContent = 'opnemen — speel noten in'; } };

    /* ⚡ DIRECT SPELEN: selecteer eerste geluid + speel direct een noot */
    box.querySelector('#spDirect').onclick = () => {
      Aura.resume();
      if (InstrumentMatrix.sounds.length && InstrumentMatrix.selected.size === 0) {
        const first = InstrumentMatrix.el.querySelector('.slot');
        if (first) first.click();
      }
      Aura.setVolume(1);
      InstrumentMatrix.play(60, { velocity: 110 });
      st.textContent = 'speelt — eerste geluid actief';
    };

    /* ---------- .MID export (echte SMF) ---------- */
    const vlq = n => { const b = [n & 127]; while (n >>= 7) b.unshift((n & 127) | 128); return b; };
    const exportMid = () => {
      const ppq = 96, tp16 = ppq / 4;
      const ev = [];
      for (const n of P.notes) {
        const t0 = n.s * tp16, t1 = t0 + (n.d || 1) * tp16;
        ev.push([t0, [0x90, n.p, n.v || 100]], [t1, [0x80, n.p, 64]]);
      }
      ev.sort((a, b) => a[0] - b[0]);
      const trk = []; let last = 0;
      for (const [t, msg] of ev) { trk.push(...vlq(t - last), ...msg); last = t; }
      trk.push(0, 0xFF, 0x2F, 0);
      const hdr = [0x4D,0x54,0x68,0x64,0,0,0,6,0,0,0,1,0,ppq,
                   0x4D,0x54,0x72,0x6B,(trk.length>>>24)&255,(trk.length>>>16)&255,(trk.length>>>8)&255,trk.length&255];
      const a = document.createElement('a');
      a.href = URL.createObjectURL(new Blob([new Uint8Array([...hdr, ...trk])], { type: 'audio/mid' }));
      a.download = 'g2b-opname.mid'; a.click();
      st.textContent = P.notes.length + ' noten → g2b-opname.mid';
    };
    box.querySelector('#spMid').onclick = exportMid;

    /* ---------- .WAV export (echte opname van de master-bus) ---------- */
    let wavRec = null;
    const exportWav = async () => {
      if (!wavRec) { // start opname
        const sr = Aura.ctx.sampleRate;
        const scr = Aura.ctx.createScriptProcessor(4096, 2, 2);
        wavRec = { scr, L: [], R: [], sr };
        scr.onaudioprocess = e => {
          wavRec.L.push(e.inputBuffer.getChannelData(0).slice());
          wavRec.R.push(e.inputBuffer.getChannelData(1).slice());
        };
        Aura.master.connect(scr); scr.connect(Aura.ctx.destination);
        st.textContent = 'wav-opname loopt — klik nogmaals om te stoppen & downloaden';
        box.querySelector('#spWav').classList.add('on');
      } else { // stop + encode
        wavRec.scr.disconnect(); Aura.master.disconnect(wavRec.scr);
        const cat = a => { const n = a.reduce((s, c) => s + c.length, 0), o = new Float32Array(n);
          let i = 0; for (const c of a) { o.set(c, i); i += c.length; } return o; };
        const L = cat(wavRec.L), R = cat(wavRec.R), n = L.length;
        const buf = new ArrayBuffer(44 + n * 4), v = new DataView(buf);
        const wstr = (o, s) => [...s].forEach((c, i) => v.setUint8(o + i, c.charCodeAt(0)));
        wstr(0, 'RIFF'); v.setUint32(4, 36 + n * 4, true); wstr(8, 'WAVEfmt ');
        v.setUint32(16, 16, true); v.setUint16(20, 1, true); v.setUint16(22, 2, true);
        v.setUint32(24, wavRec.sr, true); v.setUint32(28, wavRec.sr * 4, true);
        v.setUint16(32, 4, true); v.setUint16(34, 16, true); wstr(36, 'data');
        v.setUint32(40, n * 4, true);
        for (let i = 0; i < n; i++) {
          const l = Math.max(-1, Math.min(1, L[i])), r = Math.max(-1, Math.min(1, R[i]));
          v.setInt16(44 + i * 4, l * 32767, true); v.setInt16(44 + i * 4 + 2, r * 32767, true);
        }
        const a = document.createElement('a');
        a.href = URL.createObjectURL(new Blob([buf], { type: 'audio/wav' }));
        a.download = 'g2b-opname.wav'; a.click();
        st.textContent = (n / wavRec.sr).toFixed(1) + 's → g2b-opname.wav';
        wavRec = null; box.querySelector('#spWav').classList.remove('on');
      }
    };
    box.querySelector('#spWav').onclick = exportWav;

    draw();
    /* API — zelfde interface als oude loopRaster + hit met pitch */
    return {
      hit(pitch, vel = 100) {
        if (P._pb) return;                       // playback-noten niet opnieuw opnemen
        if (P.stepRec) {
          const i = P.pos < 0 ? 0 : P.pos;
          P.notes.push({ p: pitch, s: i, d: 1, v: vel });
          P.pos = (i + 1) % N; draw(); return;
        }
        if (P.rec && P.pos >= 0) { P.notes.push({ p: pitch, s: P.pos, d: 1, v: vel }); draw(); }
      },
      record(on) { P.rec = on; updRec(); },
      start, stop: halt, grid: () => P.notes.map(n => n.s), canvas: cvs,
      _p: P
    };
  }
};
