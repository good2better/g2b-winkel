/*
 * G2B OchoOcho — de G2B 808: diepe sub-bas, zoals zijn DNA het meet
 * (63% energie onder 120Hz, zachte piek, lange uithaal).
 * Mono-synth, 8 stemmen: sine + pitch-drop + zachte drive + click.
 * Knoppen: DECAY (uithaal), GLIDE (portamento), SUB (diepte vs drive),
 * CLICK (aanslag-punt), LEVEL, TUNE (halve tonen).
 * Stuurt een live golfvorm naar de UI (state "scope").
 */

#include "DistrhoPlugin.hpp"
#include "../g2b_std.h"
#include <cmath>
#include <cstdio>
#include <cstring>

START_NAMESPACE_DISTRHO

static const char* kPresetNames[6] = {"G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO"};
static const int kNumPresets = 6;
static const float kPresetParams[6][7] = {
    { 60, 30, 30, 25, 50, 0, 80 },
    { 55, 40, 40, 30, 55, 0, 80 },
    { 70, 55, 50, 20, 60, 0, 80 },
    { 65, 25, 20, 15, 35, 0, 80 },
    { 50, 20, 45, 35, 45, 0, 80 },
    { 55, 35, 35, 30, 50, 0, 80 },
};


static const int kMaxVoices = 8;

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
          fPitchD(50.0f), fTune(0.0f), fLevel(80.0f), fKey(9.0f), fScale(0.0f),
          fLastFreq(0.0f), fScopeN(0), fDecim(0)
    {
        recalc();
    }

protected:
    const char* getLabel() const override { return "G2B OchoOcho"; }
    const char* getDescription() const override
    {
        return "De G2B 808: diepe mono sub-bas met glide, pitch-drop en "
               "zachte drive — gebouwd op het G2B-subgeluid.";
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
            parameter.ranges = {0.0f, 11.0f, 9.0f};
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
            parameter.name = "Preset";
            parameter.symbol = "preset";
            parameter.unit = "";
            parameter.hints = kParameterIsInteger;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 5.0f;
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
        fLastFreq = 0.0f;
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
                    noteOn(snapKey(data[1]), data[2] / 127.0f, tuneMul);
                else if (status == 0x80 || (status == 0x90 && data[2] == 0))
                    noteOff(data[1]);
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
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kDecay,10.f+(fRnd>>8)%91);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kGlide,(fRnd>>8)%80);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kSub,(fRnd>>8)%101);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kClick,(fRnd>>8)%60);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kPitchD,(fRnd>>8)%101);
    }
    float fDice = 0.f;
    unsigned fRnd = 1357911;

    float fDecay, fGlide, fSub, fClick, fPitchD, fTune, fLevel, fKey, fScale;
    float fDecRate, fGlRate, fDrive, fClickA, fDropAmt, fTuneMul, fLevelGain, fRelRate;
    float fLastFreq;
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
        const float f = noteFreq(note) * tuneMul;
        v->active = true;
        v->note = note;
        v->target = f;
        // glide: start vanaf vorige noot; zonder glide meteen op toonhoogte
        v->freq = (fGlide > 0.5f && fLastFreq > 0.0f) ? fLastFreq : f;
        v->amp = 1.0f;
        v->vel = vel;
        v->age = 0.0f;
        v->releasing = false;
        fLastFreq = f;
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

            float x = std::sin(v.phase * 6.28318530717959f);
            // sub<->drive: pure sine vs licht gedreven
            const float xd = std::tanh(x * (1.0f + 2.5f * drive));
            x = x * (1.0f - drive) + xd * drive;

            // click: hoge korte tik bij de aanslag
            if (v.age < sr * 0.012f)
            {
                const float ck = std::sin(v.phase * 6.2831853f * 9.0f)
                               * std::exp(-v.age * 4.0f / (sr * 0.012f));
                x += ck * clickA;
            }

            s += x * v.amp * v.vel;
            v.amp *= (v.releasing ? fRelRate : decRate);
            v.age += 1.0f;
        }

        const float y = std::tanh(s * 1.1f) * level;
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

    void applyPreset(int i)
    {
        if (i < 0 || i >= kNumPresets) return;
        const float* p = kPresetParams[i];
        setParameterValue(kDecay, p[0]);
        setParameterValue(kGlide, p[1]);
        setParameterValue(kSub, p[2]);
        setParameterValue(kClick, p[3]);
        setParameterValue(kPitchD, p[4]);
        setParameterValue(kTune, p[5]);
        setParameterValue(kLevel, p[6]);
        recalc();
    }

    void recalc()
    {
        const float sr = (float)getSampleRate();
        // decay-knop → amp factor per sample (0%=0.12s .. 100%=3.5s)
        const float decSec = 0.12f + (fDecay / 100.0f) * 3.38f;
        fDecRate = std::exp(-1.0f / (sr * decSec));
        fRelRate = std::exp(-1.0f / (sr * 0.12f)); // ~120ms release bij note-off
        // glide-knop → freq approach (0=uit .. 100%=~0.35s)
        const float glSec = (fGlide / 100.0f) * 0.35f;
        fGlRate = glSec > 0.0001f ? 1.0f - std::exp(-1.0f / (sr * glSec)) : 1.0f;
        fDrive = fSub / 100.0f;
        fClickA = (fClick / 100.0f) * 0.5f;
        fDropAmt = (fPitchD / 100.0f) * 0.9f;   // tot +0.9 oct-achtige bump
        fTuneMul = std::pow(2.0f, fTune / 12.0f);
        fLevelGain = (fLevel / 100.0f) * 0.9f;
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OchoPlugin)
};

Plugin* createPlugin() { return new OchoPlugin(); }

END_NAMESPACE_DISTRHO
