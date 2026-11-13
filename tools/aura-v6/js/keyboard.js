/* G2B AURA v6.0 — keyboard.js: piano-keyboard dat via InstrumentMatrix.play() speelt
   + Web MIDI: een echt MIDI-keyboard stuurt dezelfde play() aan (met velocity). */
let _kbdRef = null;
// OCTAAF-schuif: voor kleine keyboards — verschuift muis + MIDI + computertoets
let _oct = 0;                 // in octaven (-3..+3)
const _gespeeld = new Map();  // aangeslagen noot -> echt gespeelde noot (voor juiste noteOff)
const clampP = p => Math.max(0, Math.min(127, p));
const speelPitch = p => clampP(p + _oct * 12);
function octZet(n, lbl) {
  _oct = Math.max(-3, Math.min(3, n));
  if (lbl) lbl.textContent = 'OCT ' + (_oct > 0 ? '+' + _oct : _oct);
}
function midiInit() {
  if (midiInit.done || !navigator.requestMIDIAccess) return;
  midiInit.done = true;
  navigator.requestMIDIAccess().then(acc => {
    const onMsg = e => {
      const [st, note, vel] = e.data, cmd = st & 0xf0;
      const played = speelPitch(note);
      const k = _kbdRef && _kbdRef.querySelector(`[data-pitch="${played}"]`);
      if (cmd === 0x90 && vel > 0) {
        if (k) k.classList.add('dn');
        _gespeeld.set(note, played);
        InstrumentMatrix.play(played, { velocity: vel });
      } else if (cmd === 0x80 || (cmd === 0x90 && vel === 0)) {
        const off = _gespeeld.get(note); _gespeeld.delete(note);
        const k2 = _kbdRef && _kbdRef.querySelector(`[data-pitch="${off != null ? off : played}"]`);
        if (k2) k2.classList.remove('dn');
        Aura.noteOff(off != null ? off : played);
        if (Aura.sampleOff) Aura.sampleOff(off != null ? off : played);
      }
    };
    const hook = () => [...acc.inputs.values()].forEach(i => i.onmidimessage = onMsg);
    hook(); acc.onstatechange = hook;
  }).catch(() => {});
}
function bouwKeyboard(el, octStart = 3, octs = 2) {
  // octaaf-balk: OCT −/+ en snel-sprongen BAS/MEL — voor kleine keyboards
  const bar = document.createElement('div'); bar.className = 'oct-bar';
  bar.innerHTML = `<button class="oct-b" data-o="-1">◀ OCT −</button>
    <span class="oct-lbl">OCT 0</span>
    <button class="oct-b" data-o="1">OCT + ▶</button>
    <button class="oct-b oct-spr" data-j="-2">BAS</button>
    <button class="oct-b oct-spr" data-j="0">MIDDEN</button>`;
  el.appendChild(bar);
  const lbl = bar.querySelector('.oct-lbl');
  bar.querySelectorAll('[data-o]').forEach(b => b.onclick = () => octZet(_oct + +b.dataset.o, lbl));
  bar.querySelectorAll('[data-j]').forEach(b => b.onclick = () => octZet(+b.dataset.j, lbl));

  const wrap = document.createElement('div'); wrap.className = 'kbd';
  const names = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
  for (let o = 0; o < octs; o++) for (let i = 0; i < 12; i++) {
    const pitch = 12 * (octStart + o) + i;
    const k = document.createElement('div');
    k.className = 'key' + (names[i].includes('#') ? ' black' : '');
    k.dataset.pitch = pitch;
    k.onpointerdown = () => {
      const played = speelPitch(pitch); _gespeeld.set(pitch, played);
      k.classList.add('dn'); InstrumentMatrix.play(played);
    };
    k.onpointerup = k.onpointerleave = () => {
      const off = _gespeeld.get(pitch); _gespeeld.delete(pitch);
      k.classList.remove('dn'); Aura.noteOff(off != null ? off : speelPitch(pitch));
      if (Aura.sampleOff) Aura.sampleOff(off != null ? off : speelPitch(pitch));
    };
    wrap.appendChild(k);
  }
  el.appendChild(wrap); _kbdRef = wrap; midiInit(); return wrap;
}
function knipRij(el) {
  const r = document.createElement('div'); r.className = 'row';
  r.innerHTML = `<label>Knip <input type="range" min="0" max="0.20" step="0.01" value="0.05" id="knipSl"></label>
    <label>Volume <input type="range" min="0" max="1.5" step="0.05" value="1" id="volSl"></label>
    <button class="danger" id="panicBtn">⏹ STOP</button>`;
  el.appendChild(r);
  r.querySelector('#knipSl').oninput = e => Aura.setKnip(+e.target.value);
  r.querySelector('#volSl').oninput = e => Aura.setVolume(+e.target.value);
  r.querySelector('#panicBtn').onclick = () => Aura.panic();
  return r;
}
