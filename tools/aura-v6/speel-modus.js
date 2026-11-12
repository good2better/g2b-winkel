// SPEEL-MODUS: normaal / 1-toets akkoord / octaaf stack / arpeggiator / scale lock
// Werkt voor muis, computertoetsenbord én USB MIDI-keyboard (alles loopt via InstrumentMatrix.play).
const SpeelModus = {
  mode: 'norm',            // norm | chord | oct | arp
  chordType: 'min',        // min | maj | kaseko | trap | sus
  arpRate: 8,              // 8 | 16
  arpDir: 'up',            // up | updown | down
  scaleOn: false,
  scaleRoot: 0,            // 0=C
  scaleType: 'min',        // min | maj

  CHORDS: {
    min:    [0, 3, 7],        // mineur drieklank
    min7:   [0, 3, 7, 10],    // mineur 7 (trap/R&B)
    maj:    [0, 4, 7],        // majeur drieklank
    maj7:   [0, 4, 7, 11],    // majeur 7
    kaseko: [0, 4, 7, 10],    // dominante 7 — Surinaams/kaseko voicing
    sus:    [0, 5, 7],        // sus4
    trap:   [0, 3, 6, 10],    // donker trap (m + b5 + b7)
  },
  SCALES: {
    maj: [0, 2, 4, 5, 7, 9, 11],
    min: [0, 2, 3, 5, 7, 8, 10],
    penta: [0, 3, 5, 7, 10],
  },
  ROOTS: ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'],

  snap(p) {
    if (!this.scaleOn) return p;
    const sc = this.SCALES[this.scaleType];
    let best = p, bd = 99;
    for (let d = -3; d <= 3; d++) {
      const n = p + d;
      if (sc.includes(((n - this.scaleRoot) % 12 + 12) % 12) && Math.abs(d) < bd) { bd = Math.abs(d); best = n; }
    }
    return best;
  },

  // geeft [{p, at}] — at = 16e-stappen vertraging (0 = nu)
  expand(p) {
    p = this.snap(p);
    if (this.mode === 'oct') return [{ p, at: 0 }, { p: p - 12, at: 0 }, { p: p + 12, at: 0 }];
    if (this.mode === 'chord' || this.mode === 'arp') {
      const iv = this.CHORDS[this.chordType] || this.CHORDS.min;
      const notes = iv.map(i => this.snap(p + i));
      if (this.mode === 'chord') return notes.map(n => ({ p: n, at: 0 }));
      // arp
      const st = this.arpRate === 16 ? 1 : 2;
      let seq = notes.slice();
      if (this.arpDir === 'down') seq.reverse();
      if (this.arpDir === 'updown') seq = [...seq, ...notes.slice(1, -1).reverse()];
      return seq.map((n, i) => ({ p: n, at: i * st }));
    }
    return [{ p, at: 0 }];
  },

  // maakt een InstrumentMatrix.play die via de modi expandeert + het paneel voedt
  wire(baseFn, panel) {
    const stepMs = 60000 / 120 / 4; // 16e stap @120bpm — zelfde raster als StudioPanel
    return (p, o = {}) => {
      const v = o.velocity || 100;
      SpeelModus.expand(p).forEach(n => {
        const go = () => { if (panel) panel.hit(n.p, v); baseFn(n.p, o); };
        if (n.at) setTimeout(go, n.at * stepMs); else go();
      });
    };
  },

  bouw(el) {
    const S = this;
    el.innerHTML = '';
    const bar = document.createElement('div');
    bar.className = 'sm-bar';
    bar.innerHTML = `
      <span class="sm-lbl">SPEEL-MODUS</span>
      <button data-m="norm" class="sm on">NORMAAL (1 NOOT)</button>
      <button data-m="chord" class="sm">1-TOETS AKKOORD</button>
      <select id="smChord" class="sm-sel">
        <option value="min">MINEUR</option><option value="maj">MAJEUR</option>
        <option value="min7">MINEUR 7 (R&B)</option><option value="maj7">MAJEUR 7</option>
        <option value="kaseko">KASEKO/SURI</option><option value="sus">SUS4</option>
        <option value="trap">TRAP DONKER</option>
      </select>
      <button data-m="oct" class="sm">OCTAAF STACK</button>
      <button data-m="arp" class="sm">ARP</button>
      <select id="smArp" class="sm-sel"><option value="8">1/8</option><option value="16">1/16</option></select>
      <select id="smArpDir" class="sm-sel"><option value="up">↑</option><option value="down">↓</option><option value="updown">↑↓</option></select>
      <button id="smScale" class="sm">SCALE LOCK: UIT</button>
      <select id="smKey" class="sm-sel"></select>`;
    el.appendChild(bar);

    bar.querySelectorAll('button.sm[data-m]').forEach(b => b.onclick = () => {
      S.mode = b.dataset.m;
      bar.querySelectorAll('button.sm[data-m]').forEach(x => x.classList.toggle('on', x === b));
    });
    bar.querySelector('#smChord').onchange = e => S.chordType = e.target.value;
    bar.querySelector('#smArp').onchange = e => S.arpRate = +e.target.value;
    bar.querySelector('#smArpDir').onchange = e => S.arpDir = e.target.value;
    const keySel = bar.querySelector('#smKey');
    this.ROOTS.forEach((r, i) => keySel.add(new Option(r + ' min', i + ':min')));
    this.ROOTS.forEach((r, i) => keySel.add(new Option(r + ' maj', i + ':maj')));
    keySel.add(new Option('C penta', '0:penta'));
    keySel.value = '0:min';
    keySel.onchange = e => { const [r, t] = e.target.value.split(':'); S.scaleRoot = +r; S.scaleType = t; };
    bar.querySelector('#smScale').onclick = e => {
      S.scaleOn = !S.scaleOn;
      e.target.textContent = 'SCALE LOCK: ' + (S.scaleOn ? 'AAN' : 'UIT');
      e.target.classList.toggle('on', S.scaleOn);
    };
    return this;
  },
};
