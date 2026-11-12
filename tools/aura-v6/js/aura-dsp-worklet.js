/* G2B AURA v6.0 — AuraMasterWorklet
   Stabiele AudioWorklet DSP: voice-pool (geen hangende noten), automatische
   gain-staging (Wet 6), 28Hz rumble-cut, 700Hz tilt-EQ, 0.99 look-ahead limiter,
   Knip hard <= 20% (0.20) — nooit kraak/gruis. */
class AuraMasterWorklet extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: 'knip',   defaultValue: 0.05, minValue: 0.0, maxValue: 0.20 }, // HARDE MAX 20%
      { name: 'volume', defaultValue: 1.0,  minValue: 0.0, maxValue: 1.5 },
      { name: 'tone',   defaultValue: 1.0,  minValue: 0.25, maxValue: 4.0 }, // Bassdrum Toon (pitch)
      { name: 'lengte', defaultValue: 0.9997, minValue: 0.990, maxValue: 0.99995 }, // decay
      { name: 'glue',   defaultValue: 1.0,  minValue: 0.0, maxValue: 1.0 } // 🔥 PRO GLUE masterketen
    ];
  }
  constructor() {
    super();
    this.voices = [];
    this.hp_x1 = 0; this.hp_x2 = 0; this.hp_y1 = 0; this.hp_y2 = 0;
    this.tilt_lp = 0; this.mono_lp = 0;
    // stereo-chorus delaylijnen (~50ms) + langzame LFO
    this.dlL = new Float32Array(2400); this.dlR = new Float32Array(2400);
    this.dlW = 0; this.lfo = 0;
    this.port.onmessage = (e) => {
      const d = e.data || {};
      if (d.type === 'NOTE_ON') this.spawnVoice(d);
      else if (d.type === 'NOTE_OFF') this.releaseVoice(d.pitch);
      else if (d.type === 'PANIC') this.voices = [];
      else if (d.type === 'ALL_NOTES_OFF') this.voices = [];
    };
  }
  spawnVoice(d) {
    // cut-self: zelfde toonhoogte dooft in ~5ms ipv hard wegsnijden (geen tik)
    for (const v of this.voices) if (v.pitch === d.pitch) v.decayRate = Math.min(v.decayRate, 0.979);
    const freq = (440 * Math.pow(2, (d.pitch - 69) / 12)) * (d.tone || 1.0);
    const low = freq < 130.81; // onder C3
    this.voices.push({
      pitch: d.pitch,
      freq,
      phase: 0, env: 1.0, subPhase: 0, age: 0, // age: 2ms fade-in + 15ms punch
      decayRate: d.decayRate != null ? d.decayRate : (low ? 0.99995 : 0.9997),
      glideTargetFreq: d.glideFreq || null,
      waveType: d.waveType || 'sine',
      gain: ((d.velocity || 100) / 127) * 0.85 * (low ? 1.585 : 1.0), // +4dB sub-compensatie
      subGain: low ? 0.9 : 0
    });
  }
  releaseVoice(pitch) {
    this.voices.forEach(v => { if (v.pitch === pitch) v.decayRate = Math.min(v.decayRate, v.freq < 130.81 ? 0.9995 : 0.995); });
  }
  applySafeKnip(s, rawKnip) {
    const k = Math.min(0.20, Math.max(0.0, rawKnip));
    const drive = 1.0 + (k * 2.5);
    let x = s * drive;
    if (x > 0.95)  { const d = x - 0.95;  return 0.95 + d - Math.pow(d, 3); }
    if (x < -0.95) { const d = x + 0.95;  return -0.95 + d - Math.pow(d, 3); }
    return x;
  }
  osc(type, ph) {
    if (type === 'square') return ph < 0.5 ? 1 : -1;
    if (type === 'saw')    return 2 * (ph - Math.floor(ph + 0.5));
    if (type === 'tri')    return 2 * Math.abs(2 * (ph - Math.floor(ph + 0.5))) - 1;
    return Math.sin(2 * Math.PI * ph);
  }
  process(inputs, outputs, parameters) {
    const out = outputs[0];
    if (!out || out.length === 0) return true;
    const knipVal = Math.min(0.20, parameters.knip[0]);
    const vol = parameters.volume[0];
    const chs = out.length;
    const sr = sampleRate;
    for (let i = 0; i < out[0].length; i++) {
      let s = 0;
      const glueOn = parameters.glue ? parameters.glue[0] > 0.5 : true;
      for (const v of this.voices) {
        if (v.glideTargetFreq) { // Bézier-achtige glide naar target
          v.freq += (v.glideTargetFreq - v.freq) * 0.0012;
        }
        v.phase += v.freq / sr;
        if (v.phase > 1) v.phase -= 1;
        v.age = (v.age || 0) + 1;
        const att = v.age < 96 ? v.age / 96 : 1;                       // 2ms fade-in = 0 clicks
        const punch = glueOn && v.age < 720 ? 1 + 0.45 * (1 - v.age / 720) : 1; // 15ms transient-punch
        const amp = v.env * v.gain * att * punch;
        s += this.osc(v.waveType, v.phase) * amp;
        if (v.subGain) { // schone sub-osc onder C3: sinus + warme 2e harmonische
          v.subPhase += v.freq / sr; if (v.subPhase > 1) v.subPhase -= 1;
          s += (Math.sin(2 * Math.PI * v.subPhase) + 0.35 * Math.sin(4 * Math.PI * v.subPhase))
               * amp * v.subGain;
        }
        v.env *= v.decayRate;
      }
      this.voices = this.voices.filter(v => v.env > 0.0005);
      // ~20Hz rumble-cut — sub-boom (35-80Hz) blijft 100% intact
      const a = 0.9975;
      this.hp_y1 = a * (this.hp_y1 + s - this.hp_x1); this.hp_x1 = s;
      s = this.hp_y1;
      // warme low-shelf: ~460Hz LP +4dB op het laag, hoog vlak
      this.tilt_lp += (s - this.tilt_lp) * 0.06;
      s = this.tilt_lp * 1.2 + (s - this.tilt_lp) * 1.0;
      s = this.applySafeKnip(s, knipVal);
      // 🔥 PRO GLUE: mono <120Hz strak, >120Hz subtiel breder (chorus-delay)
      this.mono_lp += (s - this.mono_lp) * 0.0155; // ~120Hz LP
      const low = this.mono_lp, hi = s - low;
      let L = low + hi, R = low + hi;
      if (glueOn && chs > 1) {
        this.dlL[this.dlW] = hi; this.dlR[this.dlW] = hi;
        this.lfo += 0.4 / sr; if (this.lfo > 1) this.lfo -= 1;
        const dl = 384 + 96 * Math.sin(2 * Math.PI * this.lfo);        // L: 8-10ms
        const dr = 384 + 96 * Math.sin(2 * Math.PI * this.lfo + Math.PI); // R: tegenfase
        const rL = (this.dlW + 2400 - Math.floor(dl)) % 2400;
        const rR = (this.dlW + 2400 - Math.floor(dr)) % 2400;
        L = low + hi + 0.22 * this.dlR[rR]; // kruiselingse mix → breedte
        R = low + hi + 0.22 * this.dlL[rL];
        this.dlW = (this.dlW + 1) % 2400;
      }
      L *= vol; R *= vol;
      // 0.99 look-ahead limiter (zachte clip-safe)
      if (L > 0.99) L = 0.99; else if (L < -0.99) L = -0.99;
      if (R > 0.99) R = 0.99; else if (R < -0.99) R = -0.99;
      out[0][i] = L; if (chs > 1) out[1][i] = R;
      for (let c = 2; c < chs; c++) out[c][i] = L;
    }
    return true;
  }
}
registerProcessor('aura-master', AuraMasterWorklet);
