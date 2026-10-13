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
  std::vector<BNote> gen(int kitIdx, int bars) {
    std::vector<BNote> out;
    const BKit& k = kKits[kitIdx];
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
            if (s == a1 || s == a2 || frand() < prob / 1000.f * .9f) {
              int v = nd.vel + (int)(frand() * 10) - 5;
              out.push_back({ (bar * 16 + s) * 120, 120, 9,
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
              int v = m.vel + (int)(frand() * 10) - 5;
              out.push_back({ (bar * 16 + s) * 120, m.dur, 0,
                              (uint8_t)n, (uint8_t)(v < 30 ? 30 : v > 127 ? 127 : v) });
            }
          }
        }
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
