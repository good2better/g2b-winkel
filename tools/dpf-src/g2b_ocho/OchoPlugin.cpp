/*
 * G2B OchoOcho — Giorgio's Surinaamse one-shot machine.
 * SMAAK = échte ingebakken one-shots uit eigen bron (VCSL-hits + eigen
 * 808-synths): DONDER, SKRATJI, BANGI, KOTI, AGOGO, SEKI, SURDO, TIMBAL,
 * KLAP, DEMBOW, DRILL, SUB808. Toetsenbord plicht ze mee.
 * Knoppen: DECAY (uithaal), GLIDE (portamento op de pitch), SUB (extra
 * sine-sub onder de shot), CLICK (aanslag-tik), DROP (pitch-drop),
 * TUNE, LEVEL — plus de vaste G2B-knoppen.
 */

#include "DistrhoPlugin.hpp"
#include "../g2b_std.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#ifdef DISTRHO_OS_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

START_NAMESPACE_DISTRHO

#include "ocho_samples.h"


// volgorde = kSmpOff/kSmpLen in ocho_samples.h
static const char* kPresetNames[kNumSmp] = {"DONDER","KICKBAS","TRAP","BOOM","AFRO","DEMBOW","DRILL","SUB808","SKRATJI","BANGI","KOTI","AGOGO","SEKI","SURDO","TIMBAL","KLAP"};
static const int kNumPresets = kNumSmp;


static const int kMaxVoices = 8;
static const int kMaxUsr = 256; // ook submappen (genre-sets) — alles bereikbaar met < >

// per-smaak karakter (808-gidsen): release seconden + lowpass Hz op sub (0 = uit)
// DONDER KICKBAS TRAP BOOM AFRO DEMBOW DRILL SUB808 SKRATJI BANGI KOTI AGOGO SEKI SURDO TIMBAL KLAP
static const float kSmpRel[16] = {0.12f, 0.04f, 0.04f, 0.12f, 0.10f, 0.06f, 0.04f, 0.04f,
                                  0.05f, 0.05f, 0.05f, 0.05f, 0.05f, 0.06f, 0.05f, 0.05f};
static const float kSmpLp[16]  = {90.0f, 0.0f, 0.0f, 0.0f, 120.0f, 0.0f, 0.0f, 0.0f,
                                  0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

struct UsrSmp { char name[16]; char set[16] = "MIJN"; std::vector<int16_t> d; int len = 0; float fix = 1.0f; }; // fix = pitch-correctie naar C; set = geluiden-map

// minimale PCM16-wav lezer (mono/stereo → mono, resampled naar kSmpSR)
static bool loadWav(const char* path, UsrSmp& u)
{
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    unsigned char hdr[12];
    if (std::fread(hdr, 1, 12, f) != 12 || std::memcmp(hdr, "RIFF", 4)) { std::fclose(f); return false; }
    int ch = 0, bits = 0, srate = 0; long dpos = -1, dlen = 0;
    for (;;)
    {
        unsigned char ck[8];
        if (std::fread(ck, 1, 8, f) != 8) break;
        const unsigned sz = ck[4] | (ck[5]<<8) | (ck[6]<<16) | ((unsigned)ck[7]<<24);
        if (!std::memcmp(ck, "fmt ", 4))
        {
            unsigned char fm[16];
            if (std::fread(fm, 1, 16, f) != 16) break;
            ch = fm[2] | (fm[3]<<8); srate = fm[4] | (fm[5]<<8) | (fm[6]<<16) | (fm[7]<<24);
            bits = fm[14] | (fm[15]<<8);
            if (sz > 16) std::fseek(f, sz - 16, SEEK_CUR);
        }
        else if (!std::memcmp(ck, "data", 4)) { dpos = std::ftell(f); dlen = sz; std::fseek(f, sz, SEEK_CUR); }
        else std::fseek(f, sz, SEEK_CUR);
    }
    if (!ch || !srate || dpos < 0) { std::fclose(f); return false; }
    std::fseek(f, dpos, SEEK_SET);
    std::vector<int16_t> raw;
    if (bits == 16)
    {
        raw.resize(dlen / 2);
        std::fread(raw.data(), 2, raw.size(), f);
    }
    else if (bits == 8)
    {
        std::vector<unsigned char> b(dlen);
        std::fread(b.data(), 1, dlen, f);
        raw.resize(dlen);
        for (unsigned i = 0; i < dlen; ++i) raw[i] = ((int)b[i] - 128) << 8;
    }
    else { std::fclose(f); return false; }
    std::fclose(f);
    const int frames = (int)raw.size() / ch;
    if (frames < 64) return false;
    // mono + resample naar kSmpSR
    const double step = (double)srate / kSmpSR;
    const int outN = std::min((int)(frames / step), kSmpSR * 4);
    u.d.resize(outN);
    for (int i = 0; i < outN; ++i)
    {
        const double src = i * step;
        const int i0 = (int)src; const int i1 = i0 + 1 < frames ? i0 + 1 : i0;
        const double fr = src - i0;
        double a0 = raw[i0*ch], a1 = raw[i1*ch];
        if (ch > 1) { a0 = (a0 + raw[i0*ch+1]) * 0.5; a1 = (a1 + raw[i1*ch+1]) * 0.5; }
        u.d[i] = (int16_t)(a0 + (a1 - a0) * fr);
    }
    u.len = outN;
    // piek-norm zodat eigen shots net zo luid zijn als de ingebakken
    int mx = 1; for (int i = 0; i < outN; ++i) { int a = u.d[i] < 0 ? -u.d[i] : u.d[i]; if (a > mx) mx = a; }
    const double g = 0.92 * 32767.0 / mx;
    for (int i = 0; i < outN; ++i) u.d[i] = (int16_t)(u.d[i] * g);
    // DC-offset eruit + korte staart-fade → geen klik/dreun bij eigen shots
    double dc = 0.0; for (int i = 0; i < outN; ++i) dc += u.d[i]; dc /= outN;
    const int fadeN = std::min(outN, 441); // ~20ms uitfade
    for (int i = 0; i < outN; ++i)
    {
        double v = u.d[i] - dc;
        if (i >= outN - fadeN) v *= (double)(outN - 1 - i) / fadeN;
        u.d[i] = (int16_t)v;
    }
    // grondtoon-detectie (zero-crossing) → pitch-fix zodat noot 36 = C
    int zc = 0; const int s0 = outN / 5, s1 = std::min(outN, s0 + kSmpSR / 2);
    for (int i = s0 + 1; i < s1; ++i)
        if ((u.d[i-1] < 0) != (u.d[i] < 0)) ++zc;
    const double fDet = (double)zc * kSmpSR / (2.0 * (s1 - s0));
    if (fDet > 30.0 && fDet < 800.0) u.fix = (float)(65.4064 / fDet);
    return true;
}

struct Voice
{
    bool  active = false;
    uint8_t note = 0;
    float freq = 0.0f;      // huidige (gegleden) frequentie
    float target = 0.0f;
    float phase = 0.0f;
    float amp = 0.0f;
    float vel = 0.0f;
    float age = 0.0f;       // samples sinds note-on (voor pitch-drop)
    bool  releasing = false; // toets los → snelle release i.p.v. lange decay
    float pos = 0.0f;       // lees-positie in de one-shot
    float rate = 1.0f;      // afspeel-snelheid (pitch)
    float trate = 1.0f;     // doel-snelheid (glide)
    int   smp = 0;          // welke one-shot
    float relRate = 0.99f;  // per-smaak release (gids: trap kort, kawina/winti langer)
    float lpA = 0.0f, lpS = 0.0f; // per-smaak lowpass op de sub-laag
};

class OchoPlugin : public Plugin
{
public:
    enum Parameters {
        kDecay = 0,   // uithaal-tijd
        kGlide,       // portamento
        kSub,         // mix: puur sine <-> gedreven bas
        kClick,       // aanslag-click
        kPitchD,      // pitch-drop bij aanslag (klassieke 808-bump)
        kTune,        // halve tonen -12..+12
        kLevel,       // output
        kKey,         // key-lock (0-11)
        kScale,       // 0=minor 1=major
        kDice,        // randomize
        kPreset,
        kOn,          // AAN/UIT bypass
        kKracht,      // output-level 0-100%
        kGalaxy,      // DIMENTIES 0-26
        kSfeer,       // dark/neutraal/licht
        kParameterCount
    };

    OchoPlugin()
        : Plugin(kParameterCount, 0, 0),
          fDecay(60.0f), fGlide(0.0f), fSub(30.0f), fClick(25.0f),
          fPitchD(50.0f), fTune(0.0f), fLevel(80.0f), fKey(0.0f), fScale(0.0f),
          fLastFreq(0.0f), fScopeN(0), fDecim(0)
    {
        recalc();
    }

protected:
    const char* getLabel() const override { return "G2B OchoOcho"; }
    const char* getDescription() const override
    {
        return "Giorgio's Surinaamse one-shot machine: 16 echte eigen "
               "klanken + je eigen samples (auto in toon), 808-motor inbegrepen.";
    }
    const char* getMaker() const override { return "G2B (good2better)"; }
    const char* getHomePage() const override { return "https://github.com/good2better"; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','O','t'); }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        parameter.hints = kParameterIsAutomatable;
        switch (index)
        {
        case kDecay:
            parameter.name = "Decay"; parameter.symbol = "decay";
            parameter.unit = "%";
            parameter.ranges = {0.0f, 100.0f, 60.0f};
            break;
        case kGlide:
            parameter.name = "Glide"; parameter.symbol = "glide";
            parameter.unit = "%";
            parameter.ranges = {0.0f, 100.0f, 0.0f};
            break;
        case kSub:
            parameter.name = "Sub"; parameter.symbol = "sub";
            parameter.unit = "%";
            parameter.ranges = {0.0f, 100.0f, 30.0f};
            break;
        case kClick:
            parameter.name = "Click"; parameter.symbol = "click";
            parameter.unit = "%";
            parameter.ranges = {0.0f, 100.0f, 25.0f};
            break;
        case kPitchD:
            parameter.name = "Drop"; parameter.symbol = "drop";
            parameter.unit = "%";
            parameter.ranges = {0.0f, 100.0f, 50.0f};
            break;
        case kTune:
            parameter.name = "Tune"; parameter.symbol = "tune";
            parameter.unit = "st";
            parameter.ranges = {-12.0f, 12.0f, 0.0f};
            break;
        case kLevel:
            parameter.name = "Level"; parameter.symbol = "level";
            parameter.unit = "%";
            parameter.ranges = {0.0f, 100.0f, 80.0f};
            break;
        case kKey:
            parameter.name = "Key"; parameter.symbol = "key";
            parameter.hints = kParameterIsInteger;
            parameter.ranges = {0.0f, 11.0f, 0.0f};
            break;
        case kScale:
            parameter.name = "Scale"; parameter.symbol = "scale";
            parameter.hints = kParameterIsInteger;
            parameter.ranges = {0.0f, 1.0f, 0.0f};
            break;
        case kDice:
            parameter.name = "Dice";
            parameter.symbol = "dice";
            parameter.hints = kParameterIsInteger|kParameterIsTrigger;
            parameter.ranges = {0.0f, 1.0f, 0.0f};
            break;
        case kPreset:
            parameter.name = "Smaak";
            parameter.symbol = "smaak";
            parameter.unit = "";
            parameter.hints = kParameterIsInteger;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = (float)(kNumPresets + kMaxUsr - 1);
            parameter.ranges.def = 0.0f;
            break;
        case kOn:
            parameter.name = "Aan"; parameter.symbol = "aan";
            parameter.hints |= kParameterIsBoolean;
            parameter.ranges = {0.0f, 1.0f, 1.0f};
            break;
        case kKracht:
            parameter.name = "Kracht"; parameter.symbol = "kracht";
            parameter.unit = "%";
            parameter.ranges = {0.0f, 100.0f, 100.0f};
            break;
        case kGalaxy:
            parameter.name = "DIMENTIES"; parameter.symbol = "dim";
            parameter.hints |= kParameterIsInteger;
            parameter.ranges = {0.0f, 26.0f, 9.0f};
            break;
        case kSfeer:
            parameter.name = "Sfeer"; parameter.symbol = "sfeer";
            parameter.hints |= kParameterIsInteger;
            parameter.ranges = {0.0f, 2.0f, 1.0f};
            break;
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        switch (index)
        {
        case kDecay:  return fDecay;
        case kGlide:  return fGlide;
        case kSub:    return fSub;
        case kClick:  return fClick;
        case kPitchD: return fPitchD;
        case kTune:   return fTune;
        case kLevel:  return fLevel;
        case kKey:    return fKey;
        case kScale:  return fScale;
        case kDice:   return fDice;
        case kPreset: return fPreset;
        case kOn:     return fOn;
        case kKracht: return fKracht;
        case kGalaxy: return fGalaxyV;
        case kSfeer:  return fSfeerV;
        }
        return 0.0f;
    }

    void setParameterValue(uint32_t index, float value) override
    {
        if (index == kPreset) { fPreset = value; applyPreset((int)value); return; }
        switch (index)
        {
        case kDecay:  fDecay = value; break;
        case kGlide:  fGlide = value; break;
        case kSub:    fSub = value; break;
        case kClick:  fClick = value; break;
        case kPitchD: fPitchD = value; break;
        case kTune:   fTune = value; break;
        case kLevel:  fLevel = value; break;
        case kKey:    fKey = value; break;
        case kScale:  fScale = value; break;
        case kDice:   if (value > 0.5f) rollDice(); fDice = 0.f; break;
        case kOn:     fOn = value; break;
        case kKracht: fKracht = value; break;
        case kGalaxy: fGalaxyV = value; fGal.setPos((int)(value + 0.5f)); break;
        case kSfeer:  fSfeerV = value; fSf.setMode((int)(value + 0.5f)); break;
        }
        recalc();
    }

    void activate() override
    {
        for (int v = 0; v < kMaxVoices; ++v) fVoice[v] = Voice();
        fLastFreq = 0.0f; fLastRate = 0.0f;
        scanUserDir();
        fNamesSent = false;
        fScopeN = 0;
    }

    void run(const float**, float** outputs, uint32_t frames,
             const MidiEvent* midi, uint32_t midiCount) override
    {
        float* outL = outputs[0];
        float* outR = outputs[1];
        std::memset(outL, 0, frames * sizeof(float));
        std::memset(outR, 0, frames * sizeof(float));
        if (fOn < 0.5f) return;

        const float sr = (float)getSampleRate();
        const float decRate = fDecRate;      // amp decay per sample
        const float glRate  = fGlRate;       // glide per sample
        const float drive   = fDrive;        // sub->drive mix
        const float clickA  = fClickA;
        const float dropAmt = fDropAmt;
        const float tuneMul = fTuneMul;
        const float level   = fLevelGain;

        uint32_t curFrame = 0;
        for (uint32_t e = 0; e <= midiCount; ++e)
        {
            const uint32_t until = (e < midiCount) ? midi[e].frame : frames;
            // render tot volgende midi-event
            for (; curFrame < until; ++curFrame)
                renderOne(outL, outR, curFrame, sr, decRate, glRate,
                          drive, clickA, dropAmt, tuneMul, level);
            if (e < midiCount)
            {
                const MidiEvent& ev = midi[e];
                const uint8_t* data = ev.data;
                const uint8_t status = data[0] & 0xF0;
                if (status == 0x90 && data[2] > 0)
                {
                    // KEY transponéért je hele patroon naar die toonsoort
                    // (C3 = gekozen key), SCALE houdt het daarna in toon.
                    const int tk = std::min(127, std::max(0,
                        (int)data[1] + (int)std::lround(fKey)));
                    noteOn(snapKey((uint8_t)tk), data[2] / 127.0f, tuneMul);
                }
                else if (status == 0x80 || (status == 0x90 && data[2] == 0))
                    noteOff(data[1]);
            }
        }

        if (!fNamesSent)
        {
            fNamesSent = true;
            if (!fUsr.empty())
            {
                std::string nm, st;
                for (const UsrSmp& u : fUsr)
                {
                    if (!nm.empty()) nm += '|';
                    if (!st.empty()) st += '|';
                    nm += u.name; st += u.set;
                }
                setState("usrsmp", nm.c_str());
                setState("usrset", st.c_str());
            }
        }

        // scope naar de UI (~10x/sec)
        if (fScopeN >= 40)
        {
            char buf[256];
            int pos = 0;
            for (uint32_t s = 0; s < fScopeN && pos < 230; ++s)
                pos += std::snprintf(buf + pos, sizeof(buf) - pos, "%.3f;", fScope[s]);
            buf[pos] = '\0';
            setState("scope", buf);
            fScopeN = 0;
        }
        g2b_post(nullptr, outputs, frames, sr, fOn, fKracht, fGal, &fSf, false);
    }

private:
    void rollDice(){
        // nuttige dobbel: nieuwe SMAAK + passende klank-settings
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kPreset,(float)((fRnd>>8)%(kNumPresets+(int)fUsr.size())));
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kDecay,25.f+(fRnd>>8)%66);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kGlide,(fRnd>>8)%70);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kSub,15.f+(fRnd>>8)%71);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kClick,(fRnd>>8)%60);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kPitchD,(fRnd>>8)%101);
    }
    float fDice = 0.f;
    unsigned fRnd = 1357911;

    float fDecay, fGlide, fSub, fClick, fPitchD, fTune, fLevel, fKey, fScale;
    float fDecRate, fGlRate, fDrive, fClickA, fDropAmt, fTuneMul, fLevelGain, fRelRate;
    float fHp = 0.0f, fHpA = 0.01f;
    float fLastFreq, fLastRate = 0.0f;
    Voice fVoice[kMaxVoices];
    static const int kScopeLen = 96;
    float fScope[kScopeLen];
    uint32_t fScopeN, fDecim;

    static float noteFreq(uint8_t n)
    {
        return 440.0f * std::pow(2.0f, ((float)n - 69.0f) / 12.0f);
    }

    uint8_t snapKey(uint8_t m)
    {
        static const int sc[2][7] = {{0,2,3,5,7,8,10},{0,2,4,5,7,9,11}};
        const int* scale = sc[fScale > 0.5f ? 1 : 0];
        int pc = m % 12;
        for (int j = 0; j < 7; ++j) if (pc == (int)fKey % 12 + scale[j] % 12 || pc == ((int)fKey + scale[j]) % 12) return m;
        for (int d = 1; d < 4; ++d)
        {
            for (int j = 0; j < 7; ++j)
            {
                if ((pc - d) % 12 == ((int)fKey + scale[j]) % 12 && m - d >= 0) return m - d;
                if ((pc + d) % 12 == ((int)fKey + scale[j]) % 12 && m + d <= 127) return m + d;
            }
        }
        return m;
    }

    void noteOn(uint8_t note, float vel, float tuneMul)
    {
        // cut-itself: nieuwe noot kapt alle lopende stemmen af (808-mono, nooit modder)
        for (int i = 0; i < kMaxVoices; ++i)
            if (fVoice[i].active) fVoice[i].releasing = true;
        // zoek vrije stem, anders steel de oudste
        Voice* v = nullptr;
        for (int i = 0; i < kMaxVoices; ++i)
            if (!fVoice[i].active) { v = &fVoice[i]; break; }
        if (!v)
        {
            v = &fVoice[0];
            for (int i = 1; i < kMaxVoices; ++i)
                if (fVoice[i].age > v->age) v = &fVoice[i];
        }
        int si = (int)(fPreset + 0.5f);
        if (si < 0) si = 0;
        if (si >= kNumSmp + (int)fUsr.size()) si = kNumSmp + (int)fUsr.size() - 1;
        const float fix = (si >= kNumSmp && si - kNumSmp < (int)fUsr.size()) ? fUsr[si - kNumSmp].fix : 1.0f;
        const float f = noteFreq(note) * tuneMul;
        const float rt = std::pow(2.0f, ((float)note - 36.0f) / 12.0f) * tuneMul * fix;
        v->active = true;
        v->note = note;
        v->target = f;
        v->trate = rt;
        v->smp = si;
        // glide: start vanaf vorige noot; zonder glide meteen op toonhoogte
        v->freq = (fGlide > 0.5f && fLastFreq > 0.0f) ? fLastFreq : f;
        v->rate = (fGlide > 0.5f && fLastRate > 0.0f) ? fLastRate : rt;
        v->pos = 0.0f;
        v->amp = 1.0f;
        v->vel = vel;
        v->age = 0.0f;
        v->releasing = false;
        // per-smaak release + sub-lowpass (eigen wav's: standaard 40ms, geen lp)
        const float rel = (v->smp < kNumSmp) ? kSmpRel[v->smp] : 0.04f;
        v->relRate = std::exp(-1.0f / (getSampleRate() * rel));
        const float lp = (v->smp < kNumSmp) ? kSmpLp[v->smp] : 0.0f;
        v->lpA = (lp > 0.0f) ? (1.0f - std::exp(-6.2831853f * lp / getSampleRate())) : 0.0f;
        v->lpS = 0.0f;
        fLastFreq = f;
        fLastRate = rt;
    }

    void noteOff(uint8_t note)
    {
        for (int i = 0; i < kMaxVoices; ++i)
            if (fVoice[i].active && fVoice[i].note == note)
                fVoice[i].releasing = true; // snelle release-ramp, geen eindeloze tail
    }

    inline void renderOne(float* outL, float* outR, uint32_t i,
                          float sr, float decRate, float glRate,
                          float drive, float clickA, float dropAmt,
                          float, float level)
    {
        float s = 0.0f;
        for (int vi = 0; vi < kMaxVoices; ++vi)
        {
            Voice& v = fVoice[vi];
            if (v.amp <= 0.0005f) { v.active = false; continue; }

            // glide
            v.freq += glRate * (v.target - v.freq);

            // pitch-drop: noot start hoger en valt snel naar target
            const float drop = 1.0f + dropAmt * std::exp(-v.age * 9.0f / sr);
            const float f = v.freq * drop;

            v.phase += f / sr;
            if (v.phase >= 1.0f) v.phase -= 1.0f;

            // one-shot afspelen (pitch = rate × drop-envelope)
            float x = 0.0f;
            int len = 0;
            const int16_t* d = smpData(v.smp, len);
            const int i0 = (int)v.pos;
            if (d && i0 < len - 1)
            {
                const float fr = v.pos - (float)i0;
                x = (d[i0] * (1.0f - fr) + d[i0 + 1] * fr) / 32768.0f;
            }
            // sine-sub laag eronder (SUB-knop mengt hem erin)
            float xs = std::sin(v.phase * 6.28318530717959f);
            const float xd = std::tanh(xs * (1.0f + 2.5f * drive));
            xs = xs * (1.0f - drive) + xd * drive;
            if (v.lpA > 0.0f) { v.lpS += v.lpA * (xs - v.lpS); xs = v.lpS; } // sub lowpass (Winti/Afro)
            x = x * (1.0f - drive * 0.5f) + xs * drive;

            // click: hoge korte tik bij de aanslag
            if (v.age < sr * 0.012f)
            {
                const float ck = std::sin(v.phase * 6.2831853f * 9.0f)
                               * std::exp(-v.age * 4.0f / (sr * 0.012f));
                x += ck * clickA;
            }

            // rate glide + drop gelden voor de sample-positie
            v.rate += glRate * (v.trate - v.rate);
            v.pos += v.rate * drop * (float)kSmpSR / sr;

            s += x * v.amp * v.vel;
            v.amp *= (v.releasing ? v.relRate : decRate);
            v.age += 1.0f;
        }

        // rumble-filter: alles onder ~25Hz eruit (sub blijft schoon)
        fHp += fHpA * (s - fHp);
        float y = std::tanh((s - fHp) * 1.1f) * level;
        if (y > 0.95f) y = 0.95f; else if (y < -0.95f) y = -0.95f; // hard clip: pieken strak aftoppen
        outL[i] = y;
        outR[i] = y;

        if (++fDecim >= 3)
        {
            fDecim = 0;
            if (fScopeN < kScopeLen) fScope[fScopeN++] = y;
        }
    }

    float fPreset = 0.0f;
    float fOn = 1.0f, fKracht = 100.0f, fGalaxyV = 9.0f, fSfeerV = 1.0f;
    Galaxy fGal; Sfeer fSf;
    std::vector<UsrSmp> fUsr;
    bool fNamesSent = false;

#ifdef DISTRHO_OS_WINDOWS
    void scanDirWin(const std::string& dir, int depth, const char* set = nullptr)
    {
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return;
        do {
            if ((int)fUsr.size() >= kMaxUsr) break;
            if (!std::strcmp(fd.cFileName, ".") || !std::strcmp(fd.cFileName, "..")) continue;
            const std::string full = dir + "\\" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (depth < 2) scanDirWin(full, depth + 1, depth == 0 ? fd.cFileName : set);
                continue;
            }
            const size_t L = std::strlen(fd.cFileName);
            if (L < 5) continue;
            const char* ext = fd.cFileName + L - 4;
            if (_stricmp(ext, ".wav")) continue;
            UsrSmp u;
            std::snprintf(u.name, sizeof(u.name), "%.12s", fd.cFileName);
            if (set) std::snprintf(u.set, sizeof(u.set), "%.12s", set);
            char* dot = std::strchr(u.name, '.');
            if (dot) *dot = '\0';
            if (loadWav(full.c_str(), u)) fUsr.push_back(std::move(u));
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    void scanDirNix(const std::string& dir, int depth, const char* set = nullptr)
    {
        DIR* dp = opendir(dir.c_str());
        if (!dp) return;
        struct dirent* e;
        while ((e = readdir(dp)) && (int)fUsr.size() < kMaxUsr)
        {
            const char* nm = e->d_name;
            if (!std::strcmp(nm, ".") || !std::strcmp(nm, "..")) continue;
            const std::string full = dir + "/" + nm;
            struct stat st;
            if (stat(full.c_str(), &st)) continue;
            if (S_ISDIR(st.st_mode))
            {
                if (depth < 2) scanDirNix(full, depth + 1, depth == 0 ? nm : set);
                continue;
            }
            const size_t L = std::strlen(nm);
            if (L < 5) continue;
            const char* ext = nm + L - 4;
            if (std::strcmp(ext, ".wav") && std::strcmp(ext, ".WAV")) continue;
            UsrSmp u;
            std::snprintf(u.name, sizeof(u.name), "%.12s", nm);
            if (set) std::snprintf(u.set, sizeof(u.set), "%.12s", set);
            char* dot = std::strchr(u.name, '.');
            if (dot) *dot = '\0';
            if (loadWav(full.c_str(), u)) fUsr.push_back(std::move(u));
        }
        closedir(dp);
    }
#endif

    // eigen samples-map: <Muziek>/G2B-Ocho-Samples — elke .wav = extra smaak (ook submappen)
    void scanUserDir()
    {
        fUsr.clear();
#ifdef DISTRHO_OS_WINDOWS
        const char* up = std::getenv("USERPROFILE");
        if (!up) return;
        // G2B-Ocho-Samples = je eigen shots (set "MIJN" of per submap);
        // G2B-SMAAK-SETS = genre-mappen — elke submap = één set met de naam
        // van de map (AFRO\*.wav → set AFRO). Zo werkt SET-wisselen.
        const char* roots[4] = { "\\Music\\G2B-Ocho-Samples",
                                 "\\Documents\\G2B-Ocho-Samples",
                                 "\\Music\\G2B-SMAAK-SETS",
                                 "\\Documents\\G2B-SMAAK-SETS" };
        for (int d = 0; d < 4; ++d)
            scanDirWin(std::string(up) + roots[d], 0);
#else
        const char* home = std::getenv("HOME");
        if (!home) return;
        scanDirNix(std::string(home) + "/G2B-Ocho-Samples", 0);
        scanDirNix(std::string(home) + "/G2B-SMAAK-SETS", 0);
#endif
    }

    const int16_t* smpData(int i, int& len) const
    {
        if (i >= 0 && i < kNumSmp) { len = kSmpLen[i]; return &kSmpBlob[kSmpOff[i]]; }
        const int u = i - kNumSmp;
        if (u >= 0 && u < (int)fUsr.size()) { len = fUsr[u].len; return fUsr[u].d.data(); }
        len = 0; return nullptr;
    }

    void applyPreset(int i)
    {
        if (i < 0) i = 0;
        const int mx = kNumPresets + (int)fUsr.size();
        if (i >= mx) i = mx - 1;
        fPreset = (float)i; // smaak = welke one-shot (gebakken of eigen); knoppen blijven van jou
    }

    void recalc()
    {
        const float sr = (float)getSampleRate();
        // decay-knop → amp factor per sample (0%=0.06s .. 100%=0.9s — kick-strak, geen zoem)
        const float decSec = 0.06f + (fDecay / 100.0f) * 0.84f;
        fDecRate = std::exp(-1.0f / (sr * decSec));
        fRelRate = std::exp(-1.0f / (sr * 0.04f)); // ~40ms release bij note-off — stopt als kick
        // glide-knop → freq approach (0=uit .. 100%=0.12s — juiste 808 slide-zone 60-90ms)
        const float glSec = (fGlide / 100.0f) * 0.12f;
        fGlRate = glSec > 0.0001f ? 1.0f - std::exp(-1.0f / (sr * glSec)) : 1.0f;
        fDrive = fSub / 100.0f;
        fClickA = (fClick / 100.0f) * 0.5f;
        fDropAmt = (fPitchD / 100.0f) * 0.9f;   // tot +0.9 oct-achtige bump
        fTuneMul = std::pow(2.0f, fTune / 12.0f);
        fLevelGain = (fLevel / 100.0f) * 0.9f;
        fHpA = 1.0f - std::exp(-6.2831853f * 25.0f / sr); // high-pass ~25Hz
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OchoPlugin)
};

Plugin* createPlugin() { return new OchoPlugin(); }

END_NAMESPACE_DISTRHO
