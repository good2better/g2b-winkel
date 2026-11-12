/* G2B AURA v6.0 — aura-engine.js
   Gedeelde audio-basis: AudioContext + AuraMasterWorklet, MIDI-toetsenbord,
   loop-raster (Wet 7), knip ≤20% + volume, PANIC. Gebruik: await Aura.init() */
const Aura = {
  ctx: null, node: null, master: null, _rec: null,
  _buf: {}, _live: new Set(),

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
      decayRate: opts.decay != null ? opts.decay : (pitch < 48 ? 0.99995 : 0.9997),
      glideFreq: opts.glideFreq || null
    });
  },
  noteOff(pitch) { if (this.node) this.node.port.postMessage({ type: 'NOTE_OFF', pitch }); },
  panic() {
    if (this.node) this.node.port.postMessage({ type: 'PANIC' });
    this._live.forEach(s => { try { s.stop(); } catch (e) {} });
    this._live.clear();
  },

  /* Echte wav-samples: laden + afspelen door dezelfde compressor/veiligheidsketen.
     basePitch = noot waarop de sample origineel klinkt (default 60 = C4). */
  async loadSample(url) {
    if (this._buf[url]) return this._buf[url];
    this._buf[url] = this.ctx.decodeAudioData(await (await fetch(url)).arrayBuffer());
    return this._buf[url];
  },
  async sampleOn(pitch, opts = {}) {
    this.resume();
    const buf = await this.loadSample(opts.src);
    const src = this.ctx.createBufferSource();
    src.buffer = buf;
    const base = opts.basePitch != null ? opts.basePitch : 60;
    let rate = Math.pow(2, (pitch - base) / 12) * (opts.rate || 1);
    if (rate < 0.5) { // max 1 octaaf omlaag → geen vervorming; schone sub-osc geeft de 'boom'
      rate = 0.5;
      this.noteOn(pitch, { wave: 'sine', velocity: (opts.velocity || 100) * 1.1, decay: 0.99995 });
    }
    src.playbackRate.value = rate;
    const g = this.ctx.createGain();
    const t = this.ctx.currentTime;
    let amp = opts.gain != null ? opts.gain : 0.9;
    if (pitch < 48) amp *= 1.4; // lage noten klinken zachter → auto-gain
    const hold = opts.hold != null ? opts.hold : Math.min(buf.duration, 4);
    g.gain.setValueAtTime(0, t);
    g.gain.linearRampToValueAtTime(amp, t + 0.004);
    g.gain.setTargetAtTime(0, t + Math.max(0.05, hold), 0.12);
    src.connect(g).connect(this.master);
    this._live.add(src);
    src.onended = () => this._live.delete(src);
    src.start();
    return src;
  },

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
