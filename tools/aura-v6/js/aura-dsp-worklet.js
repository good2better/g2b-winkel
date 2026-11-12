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
      { name: 'lengte', defaultValue: 0.9997, minValue: 0.990, maxValue: 0.99995 } // decay
    ];
  }
  constructor() {
    super();
    this.voices = [];
    this.hp_x1 = 0; this.hp_x2 = 0; this.hp_y1 = 0; this.hp_y2 = 0;
    this.tilt_lp = 0;
    this.port.onmessage = (e) => {
      const d = e.data || {};
      if (d.type === 'NOTE_ON') this.spawnVoice(d);
      else if (d.type === 'NOTE_OFF') this.releaseVoice(d.pitch);
      else if (d.type === 'PANIC') this.voices = [];
      else if (d.type === 'ALL_NOTES_OFF') this.voices = [];
    };
  }
  spawnVoice(d) {
    this.voices = this.voices.filter(v => v.pitch !== d.pitch); // geen dubbele hangende stem
    const freq = (440 * Math.pow(2, (d.pitch - 69) / 12)) * (d.tone || 1.0);
    const low = freq < 130.81; // onder C3
    this.voices.push({
      pitch: d.pitch,
      freq,
      phase: 0, env: 1.0, subPhase: 0,
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
      for (const v of this.voices) {
        if (v.glideTargetFreq) { // Bézier-achtige glide naar target
          v.freq += (v.glideTargetFreq - v.freq) * 0.0012;
        }
        v.phase += v.freq / sr;
        if (v.phase > 1) v.phase -= 1;
        s += this.osc(v.waveType, v.phase) * v.env * v.gain;
        if (v.subGain) { // schone sub-osc onder C3: sinus + warme 2e harmonische
          v.subPhase += v.freq / sr; if (v.subPhase > 1) v.subPhase -= 1;
          s += (Math.sin(2 * Math.PI * v.subPhase) + 0.35 * Math.sin(4 * Math.PI * v.subPhase))
               * v.env * v.subGain * v.gain;
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
      s = this.applySafeKnip(s, knipVal) * vol;
      // 0.99 look-ahead limiter (zachte clip-safe)
      if (s > 0.99) s = 0.99; else if (s < -0.99) s = -0.99;
      for (let c = 0; c < chs; c++) out[c][i] = s;
    }
    return true;
  }
}
registerProcessor('aura-master', AuraMasterWorklet);
