// SPEEL-MODUS: normaal / 1-toets akkoord / octaaf stack / arpeggiator / scale lock
// Werkt voor muis, computertoetsenbord én USB MIDI-keyboard (alles loopt via InstrumentMatrix.play).
const SpeelModus = {
  mode: 'norm',            // legacy — wordt afgeleid uit de vlaggen hieronder
  chordOn: false,          // 1-toets akkoord aan/uit
  octOn: false,            // octaaf-stapel aan/uit
  arpOn: false,            // arpeggiator aan/uit — combineerbaar met chord + oct
  arpTight: true,          // STRAK: elke arp-noot stopt vóór de volgende — geen overlap (piano-gevoel)
  arpGate: 80,             // GATE %: 10=vlijmscherp pluck, 100=los/dromerig
  bassHold: false,         // BASHOLD: grondtoon klinkt door, top-noten kapten elkaar (lo-fi/R&B)
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
  // combinaties: chord+oct+arp stapelen (arp over akkoord en/of octaaf-laag)
  expand(p) {
    p = this.snap(p);
    let notes = [p];
    if (this.chordOn) {
      const iv = this.CHORDS[this.chordType] || this.CHORDS.min;
      notes = iv.map(i => this.snap(p + i));
    }
    if (this.octOn) notes = notes.flatMap(n => [n - 12, n, n + 12]);
    if (!this.arpOn) return notes.map(n => ({ p: n, at: 0 }));
    const st = this.arpRate === 16 ? 1 : 2;
    let seq = notes.slice();
    if (this.arpDir === 'down') seq.reverse();
    else if (this.arpDir === 'updown') seq = [...seq, ...notes.slice(1, -1).reverse()];
    else if (this.arpDir === 'conv') { // converge: buiten→binnen (laag,hoog,2e-laag,2e-hoog,...)
      const s = notes.slice().sort((a, b) => a - b), out = [];
      for (let i = 0, j = s.length - 1; i <= j;) { out.push(s[i++]); if (i <= j) out.push(s[j--]); }
      seq = out;
    } else if (this.arpDir === 'div') { // diverge: binnen→buiten (harp-achtig, strak met STRAK-gate)
      const s = notes.slice().sort((a, b) => a - b), out = [];
      let i = (s.length - 1) >> 1, j = i + 1;
      while (i >= 0 || j < s.length) { if (i >= 0) out.push(s[i--]); if (j < s.length) out.push(s[j++]); }
      seq = out;
    } else if (this.arpDir === 'oct') { // octave-jump: elke 2e noot +12 — hoog krijgt extra korte gate
      seq = notes.flatMap((n, i) => i % 2 ? [{ p: n + 12, sh: 1 }] : [{ p: n }]);
      return seq.map((n, i) => ({ p: n.p, at: i * st, sh: n.sh }));
    } else if (this.arpDir === 'rand') { // random volgorde — elke aanslag anders
      seq = notes.slice();
      for (let i = seq.length - 1; i > 0; i--) { const j = (Math.random() * (i + 1)) | 0; [seq[i], seq[j]] = [seq[j], seq[i]]; }
    } else if (this.arpDir === 'ratchet') { // ratchet: 1e noot dubbelt 2x snel — alleen nuttig met afkapping
      const out = [];
      notes.forEach((n, i) => { if (i === 0) { out.push({ p: n, at: 0, sh: 1 }, { p: n, at: st / 2, sh: 1 }); } else out.push({ p: n, at: st * i }); });
      return out;
    }
    return seq.map((n, i) => ({ p: n, at: i * st }));
  },

  // maakt een InstrumentMatrix.play die via de modi expandeert + het paneel voedt
  wire(baseFn, panel) {
    const stepMs = 60000 / 120 / 4; // 16e stap @120bpm — zelfde raster als StudioPanel
    return (p, o = {}) => {
      const v = o.velocity || 100;
      const st = SpeelModus.arpRate === 16 ? 1 : 2;
      SpeelModus.expand(p).forEach((n, i) => {
        // STRAK + GATE-schaal + groove: tel volle gate, tussen 60% ervan; sh = extra kort
        const g = SpeelModus.arpGate / 100;
        // BASHOLD: laagste noot (i===0 na sorteren op p) houdt de grond vast
        const isBassHold = SpeelModus.bassHold && n.p === Math.min(...SpeelModus.expand(p).map(x => x.p));
        const gateMs = (SpeelModus.arpOn && SpeelModus.arpTight && !isBassHold)
          ? st * stepMs * (n.sh ? g * 0.4 : (i % 2 ? g * 0.6 : g)) : 0;
        const go = () => {
          if (panel) panel.hit(n.p, v);
          baseFn(n.p, gateMs ? { ...o, hold: gateMs / 1000 } : o);
          if (gateMs && typeof Aura !== 'undefined' && Aura.noteOff)
            setTimeout(() => Aura.noteOff(n.p), gateMs);
        };
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
      <button data-m="oct" class="sm">OCTAAF-STAPEL</button>
      <button data-m="arp" class="sm">ARP</button>
      <select id="smArp" class="sm-sel"><option value="8">1/8</option><option value="16">1/16</option></select>
      <select id="smArpDir" class="sm-sel"><option value="up">↑</option><option value="down">↓</option><option value="updown">↑↓</option><option value="conv">→←</option><option value="div">←→</option><option value="oct">OCT+</option><option value="rand">RND</option><option value="ratchet">RAT</option></select>
      <button id="smTight" class="sm on">STRAK: AAN</button>
      <label class="sm-lbl" style="display:flex;align-items:center;gap:4px">GATE <input type="range" id="smGate" min="10" max="100" value="80" style="width:70px"></label>
      <button id="smBassHold" class="sm">BASHOLD: UIT</button>
      <button id="smDice" class="sm">🎲 DICE</button>
      <button id="smScale" class="sm">NOOIT VALS: UIT</button>
      <select id="smKey" class="sm-sel"></select>`;
    el.appendChild(bar);

    bar.querySelectorAll('button.sm[data-m]').forEach(b => b.onclick = () => {
      const m = b.dataset.m;
      if (m === 'norm') { S.chordOn = S.octOn = S.arpOn = false; }
      else if (m === 'chord') S.chordOn = !S.chordOn;
      else if (m === 'oct') S.octOn = !S.octOn;
      else if (m === 'arp') S.arpOn = !S.arpOn;
      S.mode = S.arpOn ? 'arp' : S.chordOn ? 'chord' : S.octOn ? 'oct' : 'norm';
      bar.querySelector('button.sm[data-m=norm]').classList.toggle('on', !S.chordOn && !S.octOn && !S.arpOn);
      bar.querySelector('button.sm[data-m=chord]').classList.toggle('on', S.chordOn);
      bar.querySelector('button.sm[data-m=oct]').classList.toggle('on', S.octOn);
      bar.querySelector('button.sm[data-m=arp]').classList.toggle('on', S.arpOn);
    });
    bar.querySelector('#smChord').onchange = e => S.chordType = e.target.value;
    bar.querySelector('#smArp').onchange = e => S.arpRate = +e.target.value;
    bar.querySelector('#smArpDir').onchange = e => S.arpDir = e.target.value;
    const tightBtn = bar.querySelector('#smTight');
    const tightPaint = () => { tightBtn.textContent = 'STRAK: ' + (S.arpTight ? 'AAN' : 'UIT'); tightBtn.classList.toggle('on', S.arpTight); };
    tightBtn.onclick = () => { S.arpTight = !S.arpTight; tightPaint(); };
    tightPaint();
    bar.querySelector('#smGate').oninput = e => S.arpGate = +e.target.value;
    const bhBtn = bar.querySelector('#smBassHold');
    bhBtn.onclick = () => { S.bassHold = !S.bassHold; bhBtn.textContent = 'BASHOLD: ' + (S.bassHold ? 'AAN' : 'UIT'); bhBtn.classList.toggle('on', S.bassHold); };
    const sync = () => {
      bar.querySelector('button.sm[data-m=norm]').classList.toggle('on', !S.chordOn && !S.octOn && !S.arpOn);
      bar.querySelector('button.sm[data-m=chord]').classList.toggle('on', S.chordOn);
      bar.querySelector('button.sm[data-m=oct]').classList.toggle('on', S.octOn);
      bar.querySelector('button.sm[data-m=arp]').classList.toggle('on', S.arpOn);
    };
    // 🎲 DICE: rol een random preset-combinatie
    bar.querySelector('#smDice').onclick = () => {
      const r = Math.random;
      S.chordOn = r() < 0.55;
      S.octOn = r() < 0.4;
      S.arpOn = r() < 0.6;
      if (!S.chordOn && !S.octOn && !S.arpOn) S.arpOn = true; // altijd iets
      const ct = ['min','maj','min7','maj7','kaseko','sus','trap'];
      S.chordType = ct[(r() * ct.length) | 0];
      bar.querySelector('#smChord').value = S.chordType;
      S.arpRate = r() < 0.5 ? 8 : 16;
      bar.querySelector('#smArp').value = S.arpRate;
      const dirs = ['up','down','updown','conv','div','oct','rand','ratchet'];
      S.arpDir = dirs[(r() * 3) | 0];
      bar.querySelector('#smArpDir').value = S.arpDir;
      sync();
      const d = bar.querySelector('#smDice');
      d.classList.add('on'); setTimeout(() => d.classList.remove('on'), 350);
    };
    const keySel = bar.querySelector('#smKey');
    this.ROOTS.forEach((r, i) => keySel.add(new Option(r + ' min', i + ':min')));
    this.ROOTS.forEach((r, i) => keySel.add(new Option(r + ' maj', i + ':maj')));
    keySel.add(new Option('C penta', '0:penta'));
    keySel.value = '0:min';
    keySel.onchange = e => { const [r, t] = e.target.value.split(':'); S.scaleRoot = +r; S.scaleType = t; };
    bar.querySelector('#smScale').onclick = e => {
      S.scaleOn = !S.scaleOn;
      e.target.textContent = 'NOOIT VALS: ' + (S.scaleOn ? 'AAN' : 'UIT');
      e.target.classList.toggle('on', S.scaleOn);
    };
    return this;
  },
};
