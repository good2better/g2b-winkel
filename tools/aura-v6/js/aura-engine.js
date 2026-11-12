/* G2B AURA v6.0 — aura-engine.js
   Gedeelde audio-basis: AudioContext + AuraMasterWorklet, MIDI-toetsenbord,
   loop-raster (Wet 7), knip ≤20% + volume, PANIC. Gebruik: await Aura.init() */
const Aura = {
  ctx: null, node: null, master: null, _rec: null,

  async init(workletUrl = '../js/aura-dsp-worklet.js') {
    if (this.ctx) return this;
    this.ctx = new (window.AudioContext || window.webkitAudioContext)();
    await this.ctx.audioWorklet.addModule(workletUrl);
    this.node = new AudioWorkletNode(this.ctx, 'aura-master', { outputChannelCount: [2] });
    this.master = this.ctx.createDynamicsCompressor();
    this.node.connect(this.master).connect(this.ctx.destination);
    return this;
  },
  resume() { if (this.ctx && this.ctx.state === 'suspended') this.ctx.resume(); },

  noteOn(pitch, opts = {}) {
    this.resume();
    this.node.port.postMessage({
      type: 'NOTE_ON', pitch, velocity: opts.velocity || 100,
      waveType: opts.wave || 'sine', tone: opts.tone || 1.0,
      decayRate: opts.decay != null ? opts.decay : 0.9997,
      glideFreq: opts.glideFreq || null
    });
  },
  noteOff(pitch) { if (this.node) this.node.port.postMessage({ type: 'NOTE_OFF', pitch }); },
  panic() { if (this.node) this.node.port.postMessage({ type: 'PANIC' }); },

  setKnip(v) { this.node.parameters.get('knip').value = Math.min(0.20, Math.max(0, v)); },
  setVolume(v) { this.node.parameters.get('volume').value = v; },

  /* Wet 7: visueel maat/telraster + Loop + Step-Record.
     Maakt een canvas-raster + knoppen in een container. */
  loopRaster(el, opts = {}) {
    const bpm = opts.bpm || 120, bars = opts.bars || 4, steps = 16;
    const cvs = document.createElement('canvas');
    cvs.width = el.clientWidth || 640; cvs.height = 60;
    el.appendChild(cvs);
    const stepsOn = new Array(bars * steps).fill(false);
    let pos = -1, timer = null, recording = false;
    const draw = () => {
      const g = cvs.getContext('2d');
      g.clearRect(0, 0, cvs.width, cvs.height);
      const w = cvs.width / (bars * steps);
      for (let i = 0; i < bars * steps; i++) {
        g.fillStyle = i === pos ? '#0fc' : stepsOn[i] ? '#0f6' : '#222';
        if (i % 4 === 0) g.fillStyle = i === pos ? '#0fc' : stepsOn[i] ? '#0f6' : '#333';
        g.fillRect(i * w + 1, 6, w - 2, 48);
      }
    };
    const step = () => { pos = (pos + 1) % (bars * steps); draw(); };
    return {
      toggle(i) { stepsOn[i] = !stepsOn[i]; draw(); },
      start() { timer = setInterval(step, 60000 / bpm / 4); },
      stop() { clearInterval(timer); pos = -1; draw(); },
      record(on) { recording = on; },
      hit(note) { if (recording && pos >= 0) { stepsOn[pos] = note; draw(); } },
      grid: () => stepsOn, canvas: cvs
    };
  }
};
