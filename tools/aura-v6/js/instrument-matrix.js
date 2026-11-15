/* G2B AURA v6.0 — instrument-matrix.js
   Multi-select & Stacking grid: 1 groen vak = solo over heel keyboard,
   meerdere groene vakken = direct stapelen (layeren). Opnieuw klikken = uit.
   Bouwt een slot-grid in een element; speelt via Aura.noteOn. */
const InstrumentMatrix = {
  sounds: [], selected: new Set(), el: null,

  /* sounds: [{naam, cat, wave, tone, decay}] — bouw het grid */
  build(el, sounds) {
    this.el = el; this.sounds = sounds; this.selected = new Set();
    el.innerHTML = '';
    const grid = document.createElement('div');
    grid.className = 'slot-grid';
    sounds.forEach((s, i) => {
      const b = document.createElement('div');
      b.className = 'slot';
      b.textContent = s.naam;
      b.dataset.cat = s.cat || '';
      b.onclick = () => this.toggle(i, b);
      grid.appendChild(b);
    });
    el.appendChild(grid);
    return grid;
  },

  toggle(i, btn) {
    const s = this.sounds[i];
    if (this.selected.has(i)) { // UIT: stop die sound meteen — geen doorklinkende pad meer
      this.selected.delete(i); btn.classList.remove('green');
      if (s.src && Aura.killSrc) Aura.killSrc(s.src);
      return;
    }
    this.selected.add(i); btn.classList.add('green');
    if (s.src) Aura.loadSample(s.src);  // voorladen → eerste toetsaanslag speelt direct
    // klik = meteen horen (preview op grondtoon) — 2 aangeklikte = 2 die je hoort
    try { this.play(s.basePitch != null ? s.basePitch : 60, { velocity: 100 }); } catch (e) {}
    // 1 groen = solo; >1 = stapel — speelt sowieso alle groene
  },

  play(pitch, opts = {}) {
    const sel = this.selected.size ? [...this.selected] : [];
    if (sel.length === 0) { // niets aangeklikt: korte, zachte neven-toon (niet eeuwig doorklinken)
      Aura.noteOn(pitch, { ...opts, decay: 0.995, velocity: Math.min(opts.velocity || 100, 80) }); return; }
    sel.forEach(i => {
      const s = this.sounds[i];
      if (s.src) { Aura.sampleOn(pitch, { ...s, gain: (opts.velocity || 110) / 127 * 0.9, hold: opts.hold }); return; }
      Aura.noteOn(pitch, { wave: s.wave, tone: (opts.tone || 1) * (s.tone || 1),
        decay: opts.decay != null ? opts.decay : s.decay,
        velocity: opts.velocity || 100 });
    });
  },

  keys: () => [...this.selected]
};
