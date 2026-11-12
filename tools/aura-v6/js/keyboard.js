/* G2B AURA v6.0 — keyboard.js: piano-keyboard dat via InstrumentMatrix.play() speelt
   + Web MIDI: een echt MIDI-keyboard stuurt dezelfde play() aan (met velocity). */
let _kbdRef = null;
function midiInit() {
  if (midiInit.done || !navigator.requestMIDIAccess) return;
  midiInit.done = true;
  navigator.requestMIDIAccess().then(acc => {
    const onMsg = e => {
      const [st, note, vel] = e.data, cmd = st & 0xf0;
      const k = _kbdRef && _kbdRef.querySelector(`[data-pitch="${note}"]`);
      if (cmd === 0x90 && vel > 0) {
        if (k) k.classList.add('dn');
        InstrumentMatrix.play(note, { velocity: vel });
      } else if (cmd === 0x80 || (cmd === 0x90 && vel === 0)) {
        if (k) k.classList.remove('dn');
        Aura.noteOff(note);
      }
    };
    const hook = () => [...acc.inputs.values()].forEach(i => i.onmidimessage = onMsg);
    hook(); acc.onstatechange = hook;
  }).catch(() => {});
}
function bouwKeyboard(el, octStart = 3, octs = 2) {
  const wrap = document.createElement('div'); wrap.className = 'kbd';
  const names = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
  for (let o = 0; o < octs; o++) for (let i = 0; i < 12; i++) {
    const pitch = 12 * (octStart + o) + i;
    const k = document.createElement('div');
    k.className = 'key' + (names[i].includes('#') ? ' black' : '');
    k.dataset.pitch = pitch;
    k.onpointerdown = () => { k.classList.add('dn'); InstrumentMatrix.play(pitch); };
    k.onpointerup = k.onpointerleave = () => { k.classList.remove('dn'); Aura.noteOff(pitch); };
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
