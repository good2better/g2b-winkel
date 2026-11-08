// G2B-Brein C++ groove-generator — zelfde logica als tools/patroon-brein/patroon_brein.py gen()
// Output: (tick, dur, ch, note, vel) — 480 tpq. Geen dependencies, plugbaar in DPF-plugin.
#pragma once
#include <stdint.h>
#include <vector>
struct BNote { int t, dur; uint8_t ch, note, vel; };

struct BreinGen {
  uint32_t rng_;
  explicit BreinGen(uint32_t seed = 12345) : rng_(seed ? seed : 1) {}
  float frand() { rng_ = rng_ * 1664525u + 1013904223u; return (rng_ >> 8) / 16777216.f; }

  int pickPc(const uint16_t* p, int off) {
    int tot = 0; for (int i = 0; i < 12; i++) tot += p[off + i];
    if (!tot) return 0;
    float r = frand() * tot;
    for (int i = 0; i < 12; i++) { r -= p[off + i]; if (r <= 0) return i; }
    return 0;
  }

  // kitIdx = index in kKits (kit binnen genre via kGenreKits[g].off+i)
  // hum: 0-1 humanize (±2-10 ticks timing-jitter; kick<=36 en anchors blijven exact)
  // melJit: melodie-kanaal mee-jitten (voor keys/misc; 808/trap-bas NIET — anker)
  // fill: breaker-fill in laatste bar
  std::vector<BNote> gen(int kitIdx, int bars, bool fill=true, float hum=0.5f, bool melJit=false) {
    std::vector<BNote> out;
    const BKit& k = kKits[kitIdx];
    const int jmax = 2 + (int)(hum * 8.f);
    for (int pi = 0; pi < k.n_parts; pi++) {
      const BPart& p = kParts[k.parts_off + pi];
      for (int ni = 0; ni < p.n_notes; ni++) {
        const BPNote& nd = kPNotes[p.notes_off + ni];
        // anchors = 2 sterkste stappen
        int a1 = -1, a2 = -1;
        for (int s = 0; s < 16; s++) {
          int v = kSteps[nd.steps_off + s];
          if (a1 < 0 || v > kSteps[nd.steps_off + a1]) { a2 = a1; a1 = s; }
          else if (a2 < 0 || v > kSteps[nd.steps_off + a2]) a2 = s;
        }
        for (int bar = 0; bar < bars; bar++)
          for (int s = 0; s < 16; s++) {
            int prob = kSteps[nd.steps_off + s];  // promille per bar
            const bool anchor = (s == a1 || s == a2);
            if (anchor || frand() < prob / 1000.f * .9f) {
              int v = nd.vel + (int)(frand() * (6 + hum * 8.f)) - (3 + (int)(hum * 4.f));
              int tt = (bar * 16 + s) * 120;
              // humanize: hats/snares/perc jitten; kick (<=36) + anchors muurvast
              if (!anchor && nd.note > 36)
                tt += (int)(frand() * (2 * jmax + 1)) - jmax;
              if (tt < 0) tt = 0;
              out.push_back({ tt, 120, 9,
                              (uint8_t)nd.note,
                              (uint8_t)(v < 30 ? 30 : v > 127 ? 127 : v) });
            }
          }
      }
    }
    for (int mi = 0; mi < k.n_mel; mi++) {
      const BMel& m = kMels[k.mel_off + mi];
      for (int bar = 0; bar < bars; bar++)
        for (int s = 0; s < 16; s++) {
          float prob = (float)kGrids[m.grid_off + s] / (float)m.bars;
          if (frand() < prob * .9f) {
            int kk = m.clus ? (m.clus + 5) / 10 : 1;
            if (kk > 4) kk = 4; if (kk < 1) kk = 1;
            for (int j = 0; j < kk; j++) {
              int pc = pickPc(kPcs, m.pc_off);
              int n = m.base - m.base % 12 + pc;
              if (n < 21) n += 12; if (n > 108) n -= 12;
              int v = m.vel + (int)(frand() * (6 + hum * 8.f)) - (3 + (int)(hum * 4.f));
              int tt = (bar * 16 + s) * 120;
              if (melJit) tt += (int)(frand() * (jmax + 1)) - jmax / 2;
              if (tt < 0) tt = 0;
              out.push_back({ tt, m.dur, 0,
                              (uint8_t)n, (uint8_t)(v < 30 ? 30 : v > 127 ? 127 : v) });
            }
          }
        }
    }
    // breaker-fill: laatste bar → snare-hits 13/15 + 16e-roll op stap 16 + kick-accent
    if (fill && bars >= 1) {
      int snr = 38, kik = 36;
      for (int pi = 0; pi < k.n_parts; pi++) {
        const BPart& p = kParts[k.parts_off + pi];
        for (int ni = 0; ni < p.n_notes; ni++) {
          const BPNote& nd = kPNotes[p.notes_off + ni];
          if (nd.note <= 36) kik = nd.note;
          else if (nd.note == 38 || nd.note == 40) snr = nd.note;
        }
      }
      const int lb = (bars - 1) * 16 * 120;
      out.push_back({ lb + 12 * 120, 120, 9, (uint8_t)kik, 112 });
      out.push_back({ lb + 13 * 120, 120, 9, (uint8_t)snr, 100 });
      out.push_back({ lb + 14 * 120, 120, 9, (uint8_t)snr, 110 });
      for (int j = 0; j < 4; j++)
        out.push_back({ lb + 15 * 120 + j * 30, 30, 9, (uint8_t)snr,
                        (uint8_t)(100 + j * 7) });
    }
    // sort op t (stabiel: insertion-friendly — kleine arrays)
    for (size_t i = 1; i < out.size(); i++) {
      BNote x = out[i]; size_t j = i;
      while (j && out[j - 1].t > x.t) { out[j] = out[j - 1]; j--; }
      out[j] = x;
    }
    return out;
  }
};
