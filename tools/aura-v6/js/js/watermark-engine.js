/* G2B AURA v6.0 — watermark-engine.js — steganografische watermerk-injector.
   Audio: LSB-achtige micro-tick timing op samples. MIDI: micro-tick offset (zie exporter). */
class AuraWatermarkEngine {
  constructor(token = 'G2B_AURA_2026') { this.seed = this.hash(token); }
  hash(t) {
    let h = 2166136261 >>> 0;
    for (let i = 0; i < t.length; i++) { h ^= t.charCodeAt(i); h = Math.imul(h, 16777619); }
    return h >>> 0;
  }
  /* onhoorbare micro-variatie per sample (±1/100000) gebaseerd op token-seed */
  injectAudio(buffer /* Float32Array */) {
    let s = this.seed;
    for (let i = 0; i < buffer.length; i += 97) {
      s = (s * 1664525 + 1013904223) >>> 0;
      buffer[i] += ((s & 1) ? 1 : -1) * 0.00001;
    }
    return buffer;
  }
  /* controleer of een buffer ons watermerk draagt */
  verifyAudio(buffer) {
    let s = this.seed, hits = 0, n = 0;
    for (let i = 0; i < buffer.length; i += 97) {
      s = (s * 1664525 + 1013904223) >>> 0; n++;
      const expect = (s & 1) ? 1 : -1;
      if (Math.sign(buffer[i]) === expect) hits++;
    }
    return n ? hits / n : 0;
  }
}
