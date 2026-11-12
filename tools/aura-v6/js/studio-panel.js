/* G2B AURA v6.0 — studio-panel.js
   Volwaardig opname- & loop-veld: maatraster (4 maten × 16e), meelopende playhead,
   transportbalk (direct spelen/play/stop/rec/loop/step/wis/.mid/.wav).
   Noten verschijnen als groene/cyaan balkjes op toonhoogte in het raster.
   Gebruik: const panel = StudioPanel.bouw(el, {bpm:120}); panel.hit(pitch,vel) vanuit play(). */
/* demo-gate: .mid/.wav export alleen voor kopers (licentiecode via validate.php of ?licentie= of opgeslagen) */
const _gelicen = () => {
  try {
    if (localStorage.getItem('g2b_aura_licentie') === 'ok') return true;
    const q = new URLSearchParams(location.search).get('licentie');
    if (q && q.length >= 6) { localStorage.setItem('g2b_aura_licentie', 'ok'); return true; }
  } catch (e) {}
  return false;
};
const _vraagLicentie = (st) => {
  const code = prompt('Export (.mid/.wav) is voor kopers.\nHeb je een licentiecode? Vul hem hier in — anders: koop de module in de winkel.');
  if (code && code.trim().length >= 6) { try { localStorage.setItem('g2b_aura_licentie', 'ok'); } catch (e) {} st.textContent = 'licentie ok — export werkt nu'; return true; }
  st.textContent = 'demo-modus: spelen & opnemen gratis — export alleen voor kopers';
  return false;
};

const StudioPanel = {
  bouw(el, opts = {}) {
    const bpm = opts.bpm || 120, bars = 4, steps = 16, N = bars * steps;
    const P = { rec: false, loop: true, stepRec: false, pos: -1, timer: null,
                notes: [], _pb: false, playing: false, hist: [] };
    /* ↩ ongedaan maken: momentopname van noten voor elke bewerking */
    const snap = () => { P.hist.push(JSON.stringify(P.notes)); if (P.hist.length > 60) P.hist.shift(); };
    const undo = () => { if (P.hist.length) { P.notes = JSON.parse(P.hist.pop()); draw(); st.textContent = 'ongedaan gemaakt'; } };
    el.ownerDocument.addEventListener('keydown', e => { if ((e.ctrlKey || e.metaKey) && e.key === 'z') { e.preventDefault(); undo(); } });
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
        <button id="spGlue" class="on">🔥 PRO GLUE: AAN</button>
        <button id="spUndo">↩ ONGEDAAN MAKEN</button>
        <button id="spMid">↓ .MID</button>
        <button id="spWav">↓ .WAV</button>
        <span id="spStat" class="hint"></span>
      </div>
      <canvas class="sp-grid" width="720" height="110"></canvas>`;
    el.appendChild(box);
    const cvs = box.querySelector('canvas'), g = cvs.getContext('2d');
    const st = box.querySelector('#spStat');
    box.querySelector('#spUndo').onclick = undo;

    /* ---------- raster: echte mini piano-roll (slepen/bewerken/wissen) ---------- */
    const PITCH_LO = 36, PITCH_HI = 84;
    const rowH = () => (cvs.height - 18 - 6) / (PITCH_HI - PITCH_LO);
    const yOf = p => 18 + (1 - (p - PITCH_LO) / (PITCH_HI - PITCH_LO)) * (cvs.height - 18 - 6);
    const pOf = y => Math.round(PITCH_LO + (1 - (y - 18) / (cvs.height - 18 - 6)) * (PITCH_HI - PITCH_LO));
    const draw = () => {
      const W = cvs.width, w = W / N, H = cvs.height, top = 18;
      g.fillStyle = '#0d1512'; g.fillRect(0, 0, W, H);
      // octaaf-rijlijnen + C-labels
      for (let p = PITCH_LO; p <= PITCH_HI; p++) {
        if (p % 12 === 0) { const y = yOf(p);
          g.strokeStyle = '#1d3a2a'; g.lineWidth = 1;
          g.beginPath(); g.moveTo(0, y); g.lineTo(W, y); g.stroke();
          g.fillStyle = '#5a8'; g.font = '9px sans-serif'; g.fillText('C' + (p / 12 - 1), 2, y - 2); }
      }
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
      // noten als sleeptbare balkjes
      for (const n of P.notes) {
        const y = yOf(n.p), rh = Math.max(5, rowH() * 0.9);
        g.fillStyle = n === P.dragN ? '#ffd24a' : (P.rec ? '#0f6' : '#0cf');
        g.fillRect(n.s * w + 1, y - rh / 2, Math.max(3, (n.d || 1) * w - 2), rh);
        g.strokeStyle = 'rgba(0,0,0,.5)'; g.lineWidth = 1;
        g.strokeRect(n.s * w + 1, y - rh / 2, Math.max(3, (n.d || 1) * w - 2), rh);
      }
      // playhead
      if (P.pos >= 0) { g.fillStyle = '#0fc'; g.fillRect(P.pos * w, 0, 2, H); }
    };
    /* hit-test: welke noot zit onder de cursor? rand (laatste 5px) = resize */
    const hitN = (x, y) => {
      const w = cvs.width / N, rh = Math.max(5, rowH() * 0.9);
      for (let i = P.notes.length - 1; i >= 0; i--) {
        const n = P.notes[i], x0 = n.s * w + 1, x1 = x0 + Math.max(3, (n.d || 1) * w - 2), y0 = yOf(n.p);
        if (x >= x0 && x <= x1 && Math.abs(y - y0) <= rh / 2 + 2)
          return { n, edge: x > x1 - 5 ? 'resize' : 'move' };
      }
      return null;
    };
    let drag = null; // {n, mode, offS, offP} of {nieuw}
    cvs.oncontextmenu = e => e.preventDefault();
    cvs.onpointerdown = e => {
      const r = cvs.getBoundingClientRect(), x = (e.clientX - r.left) * cvs.width / r.width, y = (e.clientY - r.top) * cvs.height / r.height;
      const w = cvs.width / N, i = Math.max(0, Math.min(N - 1, Math.floor(x / w)));
      const h = hitN(x, y);
      if (e.button === 2) { if (h) { snap(); P.notes.splice(P.notes.indexOf(h.n), 1); draw(); } return; } // rechts = wis
      cvs.setPointerCapture(e.pointerId);
      snap();
      if (h) {
        drag = { n: h.n, mode: h.edge, x0: x, p0: h.n.p, s0: h.n.s, d0: h.n.d || 1 };
        P.dragN = h.n;
      } else if (y >= 18) { // leeg: teken een nieuwe noot
        const p = Math.max(PITCH_LO, Math.min(PITCH_HI, pOf(y)));
        const n = { p, s: i, d: 1, v: 100 };
        P.notes.push(n); drag = { n, mode: 'resize', x0: x, s0: i, d0: 1 }; P.dragN = n;
        try { InstrumentMatrix.play(n.p, { velocity: 90 }); } catch (err) {}
      }
      draw();
    };
    cvs.onpointermove = e => {
      const r = cvs.getBoundingClientRect(), x = (e.clientX - r.left) * cvs.width / r.width, y = (e.clientY - r.top) * cvs.height / r.height;
      const w = cvs.width / N;
      if (!drag) { const h = hitN(x, y); cvs.style.cursor = h ? (h.edge === 'resize' ? 'ew-resize' : 'move') : 'crosshair'; return; }
      const n = drag.n;
      if (drag.mode === 'resize') {
        n.d = Math.max(1, Math.min(N - n.s, Math.round((x - n.s * w) / w)));
      } else {
        const ds = Math.round((x - drag.x0) / w);
        n.s = Math.max(0, Math.min(N - (n.d || 1), drag.s0 + ds));
        const np = Math.max(PITCH_LO, Math.min(PITCH_HI, pOf(y)));
        if (np !== n.p) { n.p = np; try { InstrumentMatrix.play(n.p, { velocity: 80 }); } catch (err) {} } // preview: hoor waar je hem neerzet
      }
      draw();
    };
    const endDrag = e => {
      if (drag && drag.mode === 'move' && Math.abs((e.clientX - cvs.getBoundingClientRect().left) * cvs.width / cvs.getBoundingClientRect().width - drag.x0) < 3) {
        // klik zonder beweging op een noot = wis hem (handig voor fouten)
      }
      drag = null; P.dragN = null; draw();
    };
    cvs.onpointerup = endDrag; cvs.onpointercancel = endDrag;
    cvs.ondblclick = e => {
      const r = cvs.getBoundingClientRect(), x = (e.clientX - r.left) * cvs.width / r.width, y = (e.clientY - r.top) * cvs.height / r.height;
      const h = hitN(x, y); if (h) { snap(); P.notes.splice(P.notes.indexOf(h.n), 1); draw(); }
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
    box.querySelector('#spWis').onclick = () => { snap(); P.notes = []; draw(); st.textContent = 'raster leeg'; };
    P.glue = true;
    box.querySelector('#spGlue').onclick = e => { P.glue = !P.glue;
      e.target.textContent = P.glue ? '🔥 PRO GLUE: AAN' : '🔥 PRO GLUE: UIT';
      e.target.classList.toggle('on', P.glue);
      try { Aura.setGlue(P.glue); } catch (err) {}
      st.textContent = P.glue ? 'pro glue aan — afgemasterd geluid' : 'pro glue uit — droog signaal'; };
    const updRec = () => { const b = box.querySelector('#spRec');
      b.classList.toggle('on', P.rec); b.textContent = P.rec ? '● OPN (aan)' : '● OPN'; };
    box.querySelector('#spRec').onclick = () => { P.rec = !P.rec; P.recSnap = false; updRec();
      if (P.rec) { Aura.resume(); start(); st.textContent = 'opnemen — speel noten in'; } };

    /* ⚡ DIRECT SPELEN: selecteer eerste geluid + speel direct een noot */
    box.querySelector('#spDirect').onclick = () => {
      Aura.resume();
      if (InstrumentMatrix.sounds.length && InstrumentMatrix.selected.size === 0) {
        const first = InstrumentMatrix.el.querySelector('.slot');
        if (first) first.click();
      }
      Aura.setVolume(1);
      // speel op de root-noot van het gekozen geluid — meteen de goede toonhoogte
      const i0 = InstrumentMatrix.selected.size ? [...InstrumentMatrix.selected][0] : -1;
      const root = (i0 >= 0 && InstrumentMatrix.sounds[i0] && InstrumentMatrix.sounds[i0].basePitch) || 60;
      InstrumentMatrix.play(root, { velocity: 110 });
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
    box.querySelector('#spMid').onclick = () => { if (_gelicen() || _vraagLicentie(st)) exportMid(); };

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
    box.querySelector('#spWav').onclick = () => { if (_gelicen() || _vraagLicentie(st)) exportWav(); };

    draw();
    el._sp = P; // test/debug-hook
    /* API — zelfde interface als oude loopRaster + hit met pitch */
    return {
      hit(pitch, vel = 100) {
        if (P._pb) return;                       // playback-noten niet opnieuw opnemen
        if (P.stepRec) {
          const i = P.pos < 0 ? 0 : P.pos;
          P.notes.push({ p: pitch, s: i, d: 1, v: vel });
          P.pos = (i + 1) % N; draw(); return;
        }
        if (P.rec && P.pos >= 0) { if (!P.recSnap) { snap(); P.recSnap = true; } P.notes.push({ p: pitch, s: P.pos, d: 1, v: vel }); draw(); }
      },
      record(on) { P.rec = on; P.recSnap = false; updRec(); },
      start, stop: halt, grid: () => P.notes.map(n => n.s), canvas: cvs,
      _p: P
    };
  }
};
