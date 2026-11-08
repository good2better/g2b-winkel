/*
 * G2B DREN — Giorgio's droom-machine (Sranantongo: dren = droom).
 * Multi-key droom-instrument in Cymatics-stijl:
 *   melodie-laag (eigen dream-keys synth + echte piano/vleugel op hogere TOON)
 *   + SUB-sine onderlaag + WARM-sfeerbed + SPOOK-fragmenten (zachte
 *   octaaf-spookjes om de melodie heen).
 * 8 macro-knoppen zoals de grote plugins: TOON, WARM, DRIFT, WARP,
 * RUW, ECHO, RUIMTE, MIX — plus DECAY, SUB, SPOOK en OUTPUT.
 * 320 "afleveringen" (20 basis-episodes x 16 varianten) — ZAP = zappen.
 */

#include "DistrhoPlugin.hpp"
#include "../g2b_std.h"
#include "../g2b_keys/piano_data.h"
#include "../g2b_keys/steinway_data.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <thread>
#include <atomic>
#include <fstream>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <direct.h>
#else
#include <sys/stat.h>
#endif

START_NAMESPACE_DISTRHO

#ifdef DREN_LITE
#define DREN_LITE_TAG " LITE"
#else
#define DREN_LITE_TAG ""
#endif

// ---- 320 afleveringen: 20 episodes x 16 varianten -------------------------
#ifdef DREN_LITE
static const int kNumEp   = 4;   // LITE: 64 afleveringen
#else
static const int kNumEp   = 20;  // volle: 320 afleveringen
#endif
static const int kNumVar  = 16;
static const int kNumPres = kNumEp * kNumVar; // 320 volle / 64 lite

static const char* kEpNames[20] = {
    "GOUD-UR","BLAU-UR","REGEN","NACHT","ZONOP","PALM","RIVIER","BARI",
    "WINTI","KAWI","MISTI","STER","MAAN","DUIN","VELD","STED",
    "DREN","VONK","RUSTI","ZILV"
};

// per-episode basis: {toon,warm,drift,warp,ruw,echo,ruimte,sub,decay,spook}
static const float kEp[20][10] = {
    {0.20f,0.50f,0.15f,0.30f,0.00f,0.35f,0.55f,0.25f,0.70f,0.20f}, // GOUD-UR
    {0.15f,0.60f,0.20f,0.35f,0.00f,0.45f,0.65f,0.30f,0.75f,0.30f}, // BLAU-UR
    {0.25f,0.70f,0.25f,0.20f,0.05f,0.40f,0.60f,0.20f,0.80f,0.35f}, // REGEN
    {0.10f,0.65f,0.30f,0.40f,0.05f,0.30f,0.70f,0.35f,0.85f,0.45f}, // NACHT
    {0.35f,0.45f,0.10f,0.15f,0.00f,0.30f,0.45f,0.20f,0.60f,0.15f}, // ZONOP
    {0.30f,0.40f,0.12f,0.25f,0.00f,0.35f,0.50f,0.25f,0.65f,0.20f}, // PALM
    {0.22f,0.55f,0.18f,0.30f,0.00f,0.42f,0.58f,0.28f,0.72f,0.25f}, // RIVIER
    {0.28f,0.50f,0.20f,0.45f,0.10f,0.38f,0.55f,0.40f,0.68f,0.30f}, // BARI
    {0.26f,0.55f,0.22f,0.35f,0.08f,0.36f,0.52f,0.45f,0.70f,0.28f}, // WINTI
    {0.32f,0.45f,0.16f,0.30f,0.06f,0.34f,0.50f,0.42f,0.66f,0.25f}, // KAWI
    {0.12f,0.75f,0.28f,0.50f,0.04f,0.50f,0.75f,0.30f,0.85f,0.40f}, // MISTI
    {0.18f,0.60f,0.20f,0.42f,0.00f,0.55f,0.70f,0.25f,0.80f,0.50f}, // STER
    {0.14f,0.65f,0.25f,0.48f,0.00f,0.52f,0.72f,0.32f,0.82f,0.55f}, // MAAN
    {0.24f,0.42f,0.14f,0.28f,0.00f,0.28f,0.40f,0.22f,0.58f,0.18f}, // DUIN
    {0.30f,0.38f,0.10f,0.18f,0.00f,0.25f,0.38f,0.20f,0.55f,0.15f}, // VELD
    {0.38f,0.35f,0.12f,0.22f,0.15f,0.30f,0.42f,0.30f,0.50f,0.20f}, // STED
    {0.16f,0.70f,0.32f,0.55f,0.05f,0.60f,0.80f,0.35f,0.90f,0.60f}, // DREN
    {0.45f,0.30f,0.15f,0.30f,0.20f,0.28f,0.35f,0.35f,0.48f,0.30f}, // VONK
    {0.20f,0.55f,0.18f,0.25f,0.00f,0.45f,0.65f,0.28f,0.78f,0.35f}, // RUSTI
    {0.08f,0.80f,0.35f,0.60f,0.10f,0.65f,0.85f,0.40f,0.95f,0.65f}, // ZILV
};

static const int kMaxVoices = 16;

struct DVoice {
    bool on = false;
    int note = 0;
    float vel = 0.f, t = 0.f;
    float ph1 = 0.f, ph2 = 0.f, ph3 = 0.f, ph4 = 0.f, phS = 0.f;
    float det = 0.f;          // drift-detune (semitonen, loopt langzaam)
    unsigned rnd = 0;
    const int16_t* smp = nullptr; uint32_t slen = 0; float spos = 0.f, sstep = 0.f;
    bool ghost = false;       // spook-fragment (octaaf hoger, zacht, vertraagd)
};

class DrenPlugin : public Plugin {
public:
    enum Params {
        kToon = 0,   // timbre: synth <-> kawai <-> steinway
        kWarm,       // sfeer-bed volume
        kDrift,      // langzame pitch-drift
        kWarp,       // tape-pitch-drop bij aanslag
        kRuw,        // bitcrush + lichte drive
        kEcho,       // ping-delay mix
        kRuimte,     // mini-reverb mix
        kSub,        // sub-sine laag
        kDecay,      // noot-lengte
        kSpook,      // spook-fragmenten hoeveelheid
        kMix,        // dry/wet van de lagen
        kLevel,      // output dB
        kKey, kScale, kDice, kPreset, kOn, kGalaxy, kSfeer,
        kZapBack, kTijd, kCatch, kLocks, kNumParams
    };

    DrenPlugin() : Plugin(kNumParams, 0, 2) { applyPreset(0); }

protected:
    const char* getLabel() const override { return "G2B DREN" DREN_LITE_TAG; }
    const char* getDescription() const override {
        return "Droom-machine: 320 afleveringen — melodie + sub + sfeer + spook-"
               "fragmenten, 8 macro-knoppen, ZAP = zappen.";
    }
    const char* getMaker() const override { return "G2B"; }
    const char* getHomePage() const override { return "https://good2better.com"; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
#ifdef DREN_LITE
    int64_t getUniqueId() const override { return d_cconst('G','2','D','l'); }
#else
    int64_t getUniqueId() const override { return d_cconst('G','2','D','r'); }
#endif

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints = kParameterIsAutomatable;
        p.ranges.min = 0.f; p.ranges.max = 1.f; p.ranges.def = 0.5f;
        switch (i) {
        case kToon:   p.name="TOON";   p.symbol="toon";   break;
        case kWarm:   p.name="WARM";   p.symbol="warm";   break;
        case kDrift:  p.name="DRIFT";  p.symbol="drift";  break;
        case kWarp:   p.name="WARP";   p.symbol="warp";   break;
        case kRuw:    p.name="RUW";    p.symbol="ruw";    break;
        case kEcho:   p.name="ECHO";   p.symbol="echo";   break;
        case kRuimte: p.name="RUIMTE"; p.symbol="ruimte"; break;
        case kSub:    p.name="SUB";    p.symbol="sub";    break;
        case kDecay:  p.name="DECAY";  p.symbol="decay";  break;
        case kSpook:  p.name="SPOOK";  p.symbol="spook";  break;
        case kMix:    p.name="MIX";    p.symbol="mix";    break;
        case kLevel:  p.name="OUTPUT"; p.symbol="level"; p.unit="dB";
                      p.ranges.min=-24.f; p.ranges.max=12.f; p.ranges.def=0.f; break;
        case kKey:    p.name="KEY";    p.symbol="key"; p.hints|=kParameterIsInteger;
                      p.ranges.min=0.f; p.ranges.max=11.f; p.ranges.def=9.f; break;
        case kScale:  p.name="SCALE";  p.symbol="scale"; p.hints|=kParameterIsInteger;
                      p.ranges.min=0.f; p.ranges.max=1.f; p.ranges.def=0.f; break;
        case kDice:   p.name="ZAP";    p.symbol="dice"; p.hints|=kParameterIsBoolean;
                      p.ranges.min=0.f; p.ranges.max=1.f; p.ranges.def=0.f; break;
        case kPreset: p.name="AFLEVERING"; p.symbol="preset"; p.hints|=kParameterIsInteger;
                      p.ranges.min=0.f; p.ranges.max=(float)(kNumPres-1); p.ranges.def=0.f; break;
        case kOn:     p.name="Aan";    p.symbol="aan"; p.hints|=kParameterIsBoolean;
                      p.ranges.min=0.f; p.ranges.max=1.f; p.ranges.def=1.f; break;
        case kGalaxy: p.name="DIMENTIES"; p.symbol="dim"; p.hints|=kParameterIsInteger;
                      p.ranges.min=0.f; p.ranges.max=26.f; p.ranges.def=9.f; break;
        case kSfeer:  p.name="Sfeer";  p.symbol="sfeer"; p.hints|=kParameterIsInteger;
                      p.ranges.min=0.f; p.ranges.max=2.f; p.ranges.def=1.f; break;
        case kZapBack:p.name="ZAP TERUG";p.symbol="zapback";p.hints|=kParameterIsBoolean;
                      p.ranges.min=0.f; p.ranges.max=1.f; p.ranges.def=0.f; break;
        case kTijd:   p.name="TIJDREIS";p.symbol="tijd";
                      p.ranges.min=0.f; p.ranges.max=1.f; p.ranges.def=0.f; break;
        case kCatch:  p.name="DREAM CATCH";p.symbol="catch";p.hints|=kParameterIsBoolean;
                      p.ranges.min=0.f; p.ranges.max=1.f; p.ranges.def=0.f; break;
        case kLocks:  p.name="LOCKS";    p.symbol="locks"; p.hints|=kParameterIsInteger;
                      p.ranges.min=0.f; p.ranges.max=1023.f; p.ranges.def=0.f; break;
        }
    }

    void initState(uint32_t index, String& name, String& def) override {
        if (index == 0) { name = "scope"; def = ""; }
        else if (index == 1) { name = "presetname"; def = ""; }
        else { name = ""; def = ""; }
    }
    void setState(const char* k, const char* v) override { (void)k; (void)v; }

    float getParameterValue(uint32_t i) const override {
        switch (i) {
        case kToon:return fToon;case kWarm:return fWarm;case kDrift:return fDrift;
        case kWarp:return fWarp;case kRuw:return fRuw;case kEcho:return fEcho;
        case kRuimte:return fRuimte;case kSub:return fSub;case kDecay:return fDecay;
        case kSpook:return fSpook;case kMix:return fMix;case kLevel:return fLevel;
        case kKey:return fKey;case kScale:return fScale;case kDice:return fDice;
        case kPreset:return fPreset;case kOn:return fOn;case kGalaxy:return fGalaxyV;
        case kSfeer:return fSfeerV;case kTijd:return fTijd;case kLocks:return fLocks;
        }
        return 0.f;
    }

    void setParameterValue(uint32_t i, float v) override {
        if (i == kPreset) { applyPreset((int)(v + 0.5f)); return; }
        switch (i) {
        case kToon:fToon=v;break;case kWarm:fWarm=v;break;case kDrift:fDrift=v;break;
        case kWarp:fWarp=v;break;case kRuw:fRuw=v;break;case kEcho:fEcho=v;break;
        case kRuimte:fRuimte=v;break;case kSub:fSub=v;break;case kDecay:fDecay=v;break;
        case kSpook:fSpook=v;break;case kMix:fMix=v;break;case kLevel:fLevel=v;break;
        case kKey:fKey=v;break;case kScale:fScale=v;break;
        case kDice:fDice=v;if(v>0.5f)rollDice();fDice=0.f;break;
        case kOn:fOn=v;break;
        case kGalaxy:fGalaxyV=v;fGal.setPos((int)(v+0.5f));break;
        case kSfeer:fSfeerV=v;fSf.setMode((int)(v+0.5f));break;
        case kZapBack:if(v>0.5f)zapBack_();break;
        case kTijd:fTijd=v;break;
        case kCatch:if(v>0.5f)dumpCatch_();break;
        case kLocks:fLocks=v;break;
        }
    }

    void activate() override {
        for (auto& v : fV) v = DVoice();
        std::memset(fDlxL, 0, sizeof(fDlxL));
        std::memset(fDlxR, 0, sizeof(fDlxR));
        for (int c = 0; c < 4; ++c) std::memset(fComb[c], 0, sizeof(fComb[c]));
        fDlPos = 0; for (int c = 0; c < 4; ++c) fCbPos[c] = 0;
        for (int a = 0; a < 2; ++a) std::memset(fAp[a], 0, sizeof(fAp[a]));
        fApPos[0] = fApPos[1] = 0;
        fNoise = 12345u;
        fTrLp[0] = fTrLp[1] = fTrHp[0] = fTrHp[1] = 0.f;
        if (fRingL.size() != kRingLen) { fRingL.assign(kRingLen, 0.f); fRingR.assign(kRingLen, 0.f); }
        fRingPos = 0; fHistP = 0; fHistN = 0; fRestoring = false;
    }

    void run(const float**, float** out, uint32_t frames,
             const MidiEvent* ev, uint32_t evc) override {
        const float sr = (float)getSampleRate();
        std::memset(out[0], 0, frames * sizeof(float));
        std::memset(out[1], 0, frames * sizeof(float));
        if (fOn < 0.5f) return;

        const float outG  = std::pow(10.f, fLevel * 0.05f);
        const float warpA = fWarp;         // st-pitch-drop bij aanslag
        const float ruw   = fRuw;
        const float echoMix = fEcho, revMix = fRuimte;
        const float warmA = fWarm, subA = fSub, mixW = fMix;

        // delay-tijden op basis van sample-rate (~330ms L / ~495ms R ping)
        const int dL = std::min(kDlxLen - 1, (int)(sr * 0.330f));
        const int dR = std::min(kDlxLen - 1, (int)(sr * 0.495f));

        uint32_t pos = 0;
        for (uint32_t e = 0; e <= evc; ++e) {
            const uint32_t until = (e < evc) ? ev[e].frame : frames;
            while (pos < until) {
                float dry = 0.f, wet = 0.f;
                // ---- stemmen ------------------------------------------------
                for (auto& v : fV) if (v.on) {
                    if (v.t < 0.f) { v.t += 1.f / sr; continue; } // spook-wachttijd: nog stil
                    // drift-detune wandelt langzaam (max +-drift*0.15 st)
                    v.rnd = v.rnd * 1664525u + 1013904223u;
                    v.det += (((v.rnd >> 9) & 1023) / 1023.f - 0.5f) * fDrift * 0.004f;
                    const float maxD = fDrift * 0.15f;
                    if (v.det > maxD) v.det = maxD; if (v.det < -maxD) v.det = -maxD;
                    float f0 = 440.f * std::pow(2.f, ((float)v.note - 69.f) / 12.f + v.det / 12.f);
                    // warp: toon begint hoger en zakt naar f0 (tape)
                    if (warpA > 0.001f && v.t < 0.25f)
                        f0 *= 1.f + warpA * 0.5f * (1.f - v.t / 0.25f);
                    if (v.ghost) f0 *= 2.f;
                    v.ph1 += 2.f * (float)M_PI * f0 / sr;        if (v.ph1 > 2.f*(float)M_PI) v.ph1 -= 2.f*(float)M_PI;
                    v.ph2 += 2.f * (float)M_PI * f0 * 2.f / sr;  if (v.ph2 > 2.f*(float)M_PI) v.ph2 -= 2.f*(float)M_PI;
                    v.ph3 += 2.f * (float)M_PI * f0 * 4.f / sr;  if (v.ph3 > 2.f*(float)M_PI) v.ph3 -= 2.f*(float)M_PI;
                    v.ph4 += 2.f * (float)M_PI * f0 * 6.f / sr;  if (v.ph4 > 2.f*(float)M_PI) v.ph4 -= 2.f*(float)M_PI;
                    v.phS += 2.f * (float)M_PI * f0 * 0.5f / sr; if (v.phS > 2.f*(float)M_PI) v.phS -= 2.f*(float)M_PI;
                    v.t += 1.f / sr;
                    const float envB = std::exp(-v.t * (2.f + 14.f * (1.f - 0.35f)));
                    const float dec  = 0.4f + fDecay * 4.f;
                    const float env  = std::exp(-v.t * dec);
                    float w;
                    if (v.smp) {
                        const uint32_t i2 = (uint32_t)v.spos;
                        if (i2 >= v.slen - 1) { v.on = false; continue; }
                        const float fr = v.spos - i2;
                        w = (v.smp[i2] + fr * (v.smp[i2+1] - v.smp[i2])) / 32768.f * 7.f;
                        v.spos += v.sstep;
                    } else {
                        w = std::sin(v.ph1)
                          + (0.25f + fToon * 0.3f) * std::sin(v.ph2) * envB
                          + (0.06f + fToon * 0.1f) * std::sin(v.ph3) * envB * envB
                          + 0.35f * 0.25f * std::sin(v.ph4) * envB * envB;
                    }
                    const float vGain = v.ghost ? 0.18f : 1.f;
                    dry += w * env * v.vel * vGain;
                    // sub-sine laag
                    dry += std::sin(v.phS) * env * v.vel * subA * 0.6f * vGain;
                    if (env < 0.0001f) v.on = false;
                }

                // ---- WARM-sfeerbed (filtered noise, L/R licht anders) -------
                fNoise = fNoise * 1664525u + 1013904223u;
                const float nz = ((int)(fNoise >> 9) & 65535) / 32768.f - 1.f;
                fNzLp += 0.002f * (nz - fNzLp);            // heel donker sfeerbed
                wet += fNzLp * warmA * 0.35f;

                // ---- RUW: bitcrush + zachte drive ---------------------------
                float s = dry * 0.3f;
                if (ruw > 0.001f) {
                    const int bits = 16 - (int)(ruw * 11.f);
                    const float q = (float)(1 << (16 - bits));
                    s = std::floor(s * q + 0.5f) / q;
                    s = std::tanh(s * (1.f + ruw * 1.5f));
                }

                // ---- ECHO (ping L/R) + RUIMTE (4 combs + 2 allpass) ---------
                const int rL = (fDlPos - dL + kDlxLen) % kDlxLen;
                const int rR = (fDlPos - dR + kDlxLen) % kDlxLen;
                const float dlL = fDlxL[rL], dlR = fDlxR[rR];
                fDlxL[fDlPos] = s + dlR * 0.35f;
                fDlxR[fDlPos] = s + dlL * 0.35f;
                fDlPos = (fDlPos + 1) % kDlxLen;
                float wetL = dlL * echoMix, wetR = dlR * echoMix;

                float rv = 0.f;
                for (int c = 0; c < 4; ++c) {
                    const int len = kCombLen[c];
                    const int rp = (fCbPos[c] - len + kCombMax) % kCombMax;
                    const float cb = fComb[c][rp];
                    fComb[c][fCbPos[c]] = s * 0.5f + cb * (0.62f + revMix * 0.12f);
                    fCbPos[c] = (fCbPos[c] + 1) % kCombMax;
                    rv += cb;
                }
                rv *= 0.25f;
                for (int a = 0; a < 2; ++a) {
                    const int len = kApLen[a];
                    const int rp = (fApPos[a] - len + kApMax) % kApMax;
                    const float ap = fAp[a][rp];
                    fAp[a][fApPos[a]] = rv + ap * 0.5f;
                    fApPos[a] = (fApPos[a] + 1) % kApMax;
                    rv = ap - rv * 0.5f;
                }
                wetL += rv * revMix; wetR += rv * revMix;

                // mix + zachte plafond: nooit hard clippen
                float sigL = (s * (1.f - mixW * 0.5f) + (wet + wetL) * mixW) * outG;
                float sigR = (s * (1.f - mixW * 0.5f) + (wet + wetR) * mixW) * outG;
                sigL = std::tanh(sigL * 0.7f) * 1.2f;
                sigR = std::tanh(sigR * 0.7f) * 1.2f;
                // TIJDREIS: 0 = nu (clean) · 0.5 = 1950 (bandpass+warm) · 1 = 1920 (kraak)
                if (fTijd > 0.01f) {
                    const float t = fTijd;
                    const float lpA = 0.60f - 0.45f * t;
                    fTrLp[0] += lpA * (sigL - fTrLp[0]); fTrLp[1] += lpA * (sigR - fTrLp[1]);
                    sigL = fTrLp[0]; sigR = fTrLp[1];
                    const float hpA = 0.28f * t;
                    fTrHp[0] += hpA * (sigL - fTrHp[0]); sigL -= fTrHp[0];
                    fTrHp[1] += hpA * (sigR - fTrHp[1]); sigR -= fTrHp[1];
                    const float dr = 1.f + 3.f * t;
                    sigL = std::tanh(sigL * dr); sigR = std::tanh(sigR * dr);
                    if (t > 0.55f) {
                        fNoise = fNoise * 1103515245u + 12345u;
                        if ((fNoise & 0xFFFFFu) < 240u) { sigL += 0.30f * t; sigR += 0.30f * t; }
                    }
                    const float g = 1.f - 0.22f * t; sigL *= g; sigR *= g;
                }
                // hard plafond: nooit boven 0.98 (brickwall)
                if (sigL > 0.98f) sigL = 0.98f; else if (sigL < -0.98f) sigL = -0.98f;
                if (sigR > 0.98f) sigR = 0.98f; else if (sigR < -0.98f) sigR = -0.98f;
                out[0][pos] = sigL;
                out[1][pos] = sigR;
                // DREAM CATCHER: altijd laatste ~30s bijhouden
                if (fRingL.size() == kRingLen) {
                    fRingL[fRingPos] = sigL; fRingR[fRingPos] = sigR;
                    fRingPos = (fRingPos + 1) % kRingLen;
                }

                if (++fDecim >= 3) { fDecim = 0; if (fScopeN < 40) fScope[fScopeN++] = s; }
                ++pos;
            }
            if (e < evc) {
                const MidiEvent& m = ev[e];
                if (m.size >= 3 && (m.data[0] & 0xF0) == 0x90 && m.data[2] > 0) {
                    const int note = snap(m.data[1]);
                    spawnVoice(note, m.data[2] / 127.f, false);
                    // spook-fragment: zachte octaaf-spook met korte vertraging
                    if (fSpook > 0.05f && ((fGcount++ & 1) == 0 || fSpook > 0.5f))
                        spawnVoice(note, m.data[2] / 127.f * fSpook, true);
                } else if (m.size >= 3 && ((m.data[0] & 0xF0) == 0x80 ||
                        ((m.data[0] & 0xF0) == 0x90 && m.data[2] == 0))) {
                    for (auto& v : fV) if (v.on && v.note == m.data[1] && !v.ghost)
                        { v.vel *= 0.0f; v.t *= 4.f; }
                }
            }
        }

        if (fScopeN >= 40) {
            char buf[256]; int pp = 0;
            for (uint32_t s2 = 0; s2 < fScopeN && pp < 230; ++s2)
                pp += std::snprintf(buf + pp, sizeof(buf) - pp, "%.3f;", fScope[s2]);
            buf[pp] = 0; setState("scope", buf); fScopeN = 0;
        }
        g2b_post(nullptr, out, frames, sr, fOn, 100.f, fGal, &fSf, false);
    }

private:
    static const int kDlxLen = 65536;
    static const int kCombMax = 4096;
    static const int kApMax = 1024;
    static const int kCombLen[4];
    static const int kApLen[2];

    void spawnVoice(int note, float vel, bool ghost) {
        for (auto& v : fV) if (!v.on) {
            v.on = true; v.note = note; v.vel = vel; v.t = 0.f;
            v.ph1 = v.ph2 = v.ph3 = v.ph4 = v.phS = 0.f;
            v.det = 0.f; v.smp = nullptr; v.ghost = ghost;
            v.rnd = (unsigned)(note * 2654435761u + (ghost ? 99 : 0));
            if (ghost) v.t = -0.18f - 0.4f * ((v.rnd >> 8) & 255) / 255.f; // vertraagd start
            if (!ghost && fToon >= 0.5f) {
                // echte piano/vleugel-laag
                if (fToon >= 0.95f) {
                    int bi = 0, bd = 99;
                    for (int k = 0; k < 8; ++k) { int d = std::abs(kStein[k].midi - note); if (d < bd) { bd = d; bi = k; } }
                    const StSmp& ps = kStein[bi]; const bool hrd = vel >= 0.55f;
                    v.smp = hrd ? ps.d2 : ps.d; v.slen = hrd ? ps.len2 : ps.len;
                    float f0 = 440.f * std::pow(2.f, (note - 69) / 12.f), fs = 440.f * std::pow(2.f, (ps.midi - 69) / 12.f);
                    v.spos = 0; v.sstep = (f0 / fs) * (22050.f / (float)getSampleRate());
                } else {
                    int bi = 0, bd = 99;
                    for (int k = 0; k < 8; ++k) { int d = std::abs(kPiano[k].midi - note); if (d < bd) { bd = d; bi = k; } }
                    const PianoSmp& ps = kPiano[bi]; const bool hrd = vel >= 0.55f;
                    v.smp = hrd ? ps.d2 : ps.d; v.slen = hrd ? ps.len2 : ps.len;
                    float f0 = 440.f * std::pow(2.f, (note - 69) / 12.f), fs = 440.f * std::pow(2.f, (ps.midi - 69) / 12.f);
                    v.spos = 0; v.sstep = (f0 / fs) * (22050.f / (float)getSampleRate());
                }
            }
            return;
        }
    }

    static const int kScales_[2][7];
    int snap(int midi) const {
        const int key = (int)fKey, sc = (int)fScale < 0 ? 0 : ((int)fScale > 1 ? 1 : (int)fScale);
        const int pc = ((midi - key) % 12 + 12) % 12;
        int best = kScales_[sc][0], bd = 99;
        for (int i = 0; i < 7; ++i) { int d = std::abs(pc - kScales_[sc][i]); if (d < bd) { bd = d; best = kScales_[sc][i]; } }
        return midi - pc + best;
    }

    void rollDice() {
        // ZAP = zappen naar een willekeurige aflevering (ZAP-smaak)
        fRnd = fRnd * 1664525u + 1013904223u;
        applyPreset((int)((fRnd >> 8) % kNumPres));
    }

    void applyPreset(int i) {
        if (i < 0) i = 0; if (i >= kNumPres) i = kNumPres - 1;
        if (!fRestoring) {
            fHist[fHistP].p = (int)fPreset;
            float* m = fHist[fHistP].m;
            m[0]=fToon;m[1]=fWarm;m[2]=fDrift;m[3]=fWarp;m[4]=fRuw;
            m[5]=fEcho;m[6]=fRuimte;m[7]=fSub;m[8]=fDecay;m[9]=fSpook;
            fHistP = (fHistP + 1) % 20; if (fHistN < 20) ++fHistN;
        }
        fPreset = (float)i;
        const int ep = i / kNumVar, va = i % kNumVar;
        // variant verschuift subtiel: drift/warp/echo/spook groeien per variant,
        // toon wisselt om en om — elke aflevering is net anders
        const float vsh = (float)va / (kNumVar - 1);
        const int lk = (int)(fLocks + 0.5f);
        if (!(lk & 1))   fToon   = kEp[ep][0] + ((va & 1) ? 0.06f : -0.06f) * vsh;
        if (!(lk & 2))   fWarm   = kEp[ep][1];
        if (!(lk & 4))   fDrift  = std::min(1.f, kEp[ep][2] + vsh * 0.15f);
        if (!(lk & 8))   fWarp   = std::min(1.f, kEp[ep][3] + vsh * 0.20f);
        if (!(lk & 16))  fRuw    = kEp[ep][4];
        if (!(lk & 32))  fEcho   = std::min(1.f, kEp[ep][5] + vsh * 0.10f);
        if (!(lk & 64))  fRuimte = std::min(1.f, kEp[ep][6] + vsh * 0.10f);
        if (!(lk & 128)) fSub    = kEp[ep][7];
        if (!(lk & 256)) fDecay  = kEp[ep][8];
        if (!(lk & 512)) fSpook  = std::min(1.f, kEp[ep][9] + vsh * 0.20f);
        if (!fRestoring) fMix    = 0.55f;
        // naam naar de UI
        char nb[24]; std::snprintf(nb, sizeof(nb), "%s-%02d", kEpNames[ep], va + 1);
        setState("presetname", nb);
    }

    // ZAP-TERUG: geschiedenis van laatste 20 afleveringen + macro-standen
    struct ZapSnap { int p; float m[10]; };
    void zapBack_() {
        if (fHistN <= 0) return;
        fHistP = (fHistP + 19) % 20; --fHistN;
        const ZapSnap& s = fHist[fHistP];
        fRestoring = true;
        applyPreset(s.p);
        fToon=s.m[0];fWarm=s.m[1];fDrift=s.m[2];fWarp=s.m[3];fRuw=s.m[4];
        fEcho=s.m[5];fRuimte=s.m[6];fSub=s.m[7];fDecay=s.m[8];fSpook=s.m[9];
        fRestoring = false;
    }
    // DREAM CATCHER: bewaar de laatste ~30 seconden als .wav
    void dumpCatch_() {
        if (fDumpBusy.exchange(true)) return;
        CatchJob* j = new CatchJob{this, fRingL, fRingR, fRingPos,
            getSampleRate() > 0 ? getSampleRate() : 44100.0};
    #ifdef _WIN32
        CreateThread(nullptr, 0, catchThread_, j, 0, nullptr);
    #else
        std::thread(catchThread_, j).detach();
    #endif
    }
    struct CatchJob { DrenPlugin* self; std::vector<float> L, R; uint32_t pos; double sr; };
#ifdef _WIN32
    static DWORD WINAPI catchThread_(LPVOID arg) {
#else
    static void catchThread_(void* arg) {
#endif
        CatchJob* j = (CatchJob*)arg;
        j->self->writeCatchWav_(j->L, j->R, j->pos, j->sr);
        j->self->fDumpBusy = false;
        delete j;
    #ifdef _WIN32
        return 0;
    #endif
    }
    void writeCatchWav_(const std::vector<float>& L, const std::vector<float>& R,
                        uint32_t pos, double sr) {
        const uint32_t n = kRingLen;
        char dir[512];
    #ifdef _WIN32
        std::snprintf(dir, sizeof(dir), "%s\\Music\\G2B", std::getenv("USERPROFILE") ? std::getenv("USERPROFILE") : ".");
        _mkdir(dir);
    #else
        std::snprintf(dir, sizeof(dir), "%s/Music/G2B", std::getenv("HOME") ? std::getenv("HOME") : ".");
        mkdir(dir, 0755);
    #endif
        char path[600]; std::snprintf(path, sizeof(path), "%s/DREN-CATCH.wav", dir);
        std::ofstream f(path, std::ios::binary);
        if (!f.good()) return;
        const uint32_t dataN = n * 4;
        const int sri = (int)sr;
        f.write("RIFF", 4); { uint32_t v = 36 + dataN; f.write((char*)&v, 4); }
        f.write("WAVEfmt ", 8); { uint32_t v = 16; f.write((char*)&v, 4);
            uint16_t fmt = 1, ch = 2; f.write((char*)&fmt, 2); f.write((char*)&ch, 2); }
        f.write((char*)&sri, 4); { uint32_t br = sri * 4, ba = 4; uint16_t bb = 16;
            f.write((char*)&br, 4); f.write((char*)&ba, 2); f.write((char*)&bb, 2); }
        f.write("data", 4); f.write((char*)&dataN, 4);
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t k = (pos + i) % n;
            int16_t a = (int16_t)(std::max(-1.f, std::min(1.f, L[k])) * 32767.f);
            int16_t b = (int16_t)(std::max(-1.f, std::min(1.f, R[k])) * 32767.f);
            f.write((char*)&a, 2); f.write((char*)&b, 2);
        }
    }

    DVoice fV[kMaxVoices];
    float fToon = .2f, fWarm = .5f, fDrift = .15f, fWarp = .3f, fRuw = 0.f;
    float fEcho = .35f, fRuimte = .55f, fSub = .25f, fDecay = .7f, fSpook = .2f;
    float fMix = .55f, fLevel = 0.f;
    float fTijd = 0.f, fLocks = 0.f;
    float fTrLp[2] = {}, fTrHp[2] = {};
    static const uint32_t kRingLen = 48000 * 30;
    std::vector<float> fRingL, fRingR;
    uint32_t fRingPos = 0;
    ZapSnap fHist[20] = {}; int fHistP = 0, fHistN = 0; bool fRestoring = false;
    std::atomic<bool> fDumpBusy{false};
    float fKey = 9.f, fScale = 0.f, fDice = 0.f, fPreset = 0.f;
    float fOn = 1.f, fGalaxyV = 9.f, fSfeerV = 1.f;
    unsigned fRnd = 424242, fGcount = 0, fNoise = 12345u;
    float fNzLp = 0.f;
    float fDlxL[kDlxLen] = {}, fDlxR[kDlxLen] = {}; int fDlPos = 0;
    float fComb[4][kCombMax] = {}; int fCbPos[4] = {};
    float fAp[2][kApMax] = {}; int fApPos[2] = {};
    Galaxy fGal; Sfeer fSf;
    float fScope[40]; uint32_t fScopeN = 0; int fDecim = 0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrenPlugin)
};

const int DrenPlugin::kScales_[2][7] = {{0,2,3,5,7,8,10},{0,2,4,5,7,9,11}};
const int DrenPlugin::kCombLen[4] = {1116, 1188, 1277, 1356};
const int DrenPlugin::kApLen[2] = {556, 441};

Plugin* createPlugin() { return new DrenPlugin(); }
END_NAMESPACE_DISTRHO
