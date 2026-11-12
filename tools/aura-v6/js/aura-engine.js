/* G2B AURA v6.0 — aura-engine.js
   Gedeelde audio-basis: AudioContext + AuraMasterWorklet, MIDI-toetsenbord,
   loop-raster (Wet 7), knip ≤20% + volume, PANIC. Gebruik: await Aura.init() */
const Aura = {
  ctx: null, node: null, master: null, _rec: null,
  _buf: {}, _live: new Set(), _pad: {},

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
    const buf = await this.ctx.decodeAudioData(await (await fetch(url)).arrayBuffer());
    // normalize → -1dB piek: overstuurde samples kunnen nooit meer gruizen/klikken
    let peak = 0;
    for (let c = 0; c < buf.numberOfChannels; c++) {
      const d = buf.getChannelData(c);
      for (let i = 0; i < d.length; i++) { const a = Math.abs(d[i]); if (a > peak) peak = a; }
    }
    if (peak > 0.891) {
      const f = 0.891 / peak;
      for (let c = 0; c < buf.numberOfChannels; c++) {
        const d = buf.getChannelData(c);
        for (let i = 0; i < d.length; i++) d[i] *= f;
      }
    }
    this._buf[url] = buf;
    return buf;
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
      this.noteOn(pitch, { wave: 'sine', velocity: (opts.velocity || 100) * 0.9, decay: 0.99995 });
    }
    src.playbackRate.value = rate;
    const g = this.ctx.createGain();
    const t = this.ctx.currentTime;
    let amp = opts.gain != null ? opts.gain : 0.9;
    if (pitch < 48) amp *= 1.2; // lage noten klinken zachter → auto-gain
    const hold = opts.hold != null ? opts.hold : Math.min(buf.duration, 4);
    g.gain.setValueAtTime(0, t);
    g.gain.linearRampToValueAtTime(amp * 1.3, t + 0.002); // 2ms fade-in + punch op de aanslag
    g.gain.setTargetAtTime(amp, t + 0.002, 0.005);         // punch valt terug in ~15ms
    g.gain.setTargetAtTime(0, t + Math.max(0.05, hold), 0.12); // zachte uitloop = geen eind-klik
    // cut-self: zelfde pad opnieuw = vorige dooft in 5ms ipv opstapelen
    const key = opts.src;
    const prev = this._pad[key];
    if (prev) { try { prev._g.gain.cancelScheduledValues(this.ctx.currentTime);
      prev._g.gain.setTargetAtTime(0, this.ctx.currentTime, 0.0017); prev.stop(this.ctx.currentTime + 0.02); } catch (e) {} }
    this._pad[key] = src; src._g = g;
    src.connect(g).connect(this.master);
    this._live.add(src);
    src.onended = () => { this._live.delete(src); if (this._pad[key] === src) delete this._pad[key]; };
    src.start();
    return src;
  },

  setGlue(v) { if (this.node) this.node.parameters.get('glue').value = v ? 1 : 0; },

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
