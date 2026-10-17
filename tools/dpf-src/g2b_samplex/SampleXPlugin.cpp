/*
 * G2B SampleX — laad je eigen one-shot/loop, maak er je eigen instrument van.
 * Sample komt binnen via state "sample" (base64 WAV) — blijft in je project.
 * 8 stemmen: pitch st, LP-filter, drive, pitch-drop, start-punt, level.
 */

#include "DistrhoPlugin.hpp"
#include "../g2b_std.h"
#include "vendor/signalsmith-stretch.h"
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
#define DR_MP3_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#include "vendor/dr_mp3.h"
#include "vendor/dr_flac.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

START_NAMESPACE_DISTRHO

static const int kNumPresets = 17;
static const char* kPresetNames[17] = {"G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"};
// PITCH, FILTER, DRIVE, DROP, START, LEVEL
static const float kPresetParams[17][6] = {
    {  0, 8000, 30, 40, 0, 85 },
    { -2, 6000, 45, 55, 0, 85 },
    { -5, 4000, 55, 60, 0, 80 },
    { +2, 10000, 20, 25, 0, 85 },
    {  0, 7000, 40, 45, 0, 85 },
    { -3, 5500, 35, 50, 0, 80 },
    { -7, 3500, 60, 65, 0, 78 },
    { -2, 7000, 35, 40, 0, 85 },
    {  0, 9000, 40, 35, 0, 85 },
    { +5, 9500, 30, 30, 0, 85 },
    { -1, 7500, 45, 45, 0, 85 },
    { +3, 10500, 25, 30, 0, 85 },
    { -4, 4500, 50, 55, 0, 80 },
    { +1, 8500, 40, 45, 0, 85 },
    { -6, 4000, 55, 58, 0, 80 },
    { +4, 9000, 30, 35, 0, 85 },
    { -3, 6500, 45, 50, 0, 82 },

};

static const int kMaxVoices = 8;

struct SVoice
{
    bool active = false;
    uint8_t note = 0;
    float pos = 0.0f;
    float amp = 0.0f;
    float vel = 0.0f;
    float age = 0.0f;
    float lp = 0.0f;
    float end = -1.0f;
    float rate = 0.0f;
};

// ---- base64 ----
static int b64v(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}


// GALAXY-engine komt uit ../g2b_std.h (gedeelde DIMENTIES-blok)

class SampleXPlugin : public Plugin
{
public:
    enum Parameters {
        kPitch = 0, kFilter, kDrive, kDrop, kStart, kLevel, kGalaxy, kDice, kPreset, kTime, kTone, kFormant, kAutofit, kChop, kSnap, kExport, kOn, kKracht, kSfeer, kParameterCount
    };

    SampleXPlugin()
        : Plugin(kParameterCount, 0, 0),
          fPitch(0.0f), fFilter(8000.0f), fDrive(30.0f), fDrop(40.0f),
          fStart(0.0f), fLevel(85.0f), fPreset(0.0f),
          fScopeN(0), fDecim(0)
    {
        recalc();
        // demo-build convenience: auto-load a WAV if present (does not affect UI loads)
        if (FILE* f = std::fopen("/tmp/g2b_samplex_demo.wav", "rb"))
        {
            std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
            std::vector<unsigned char> bytes((size_t)n);
            if (std::fread(bytes.data(), 1, (size_t)n, f) == (size_t)n && n > 44)
            {
                // reuse the WAV branch via a base64 round-trip
                static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                std::string s; s.reserve(n * 4 / 3 + 4);
                for (long i = 0; i < n; i += 3)
                {
                    unsigned v = bytes[i] << 16 | (i + 1 < n ? bytes[i + 1] << 8 : 0) | (i + 2 < n ? bytes[i + 2] : 0);
                    s += b64[(v >> 18) & 63]; s += b64[(v >> 12) & 63];
                    s += i + 1 < n ? b64[(v >> 6) & 63] : '='; s += i + 2 < n ? b64[v & 63] : '=';
                }
                loadSampleB64(s.c_str());
            }
            std::fclose(f);
        }
    }

protected:
    const char* getLabel() const override { return "G2B SampleX"; }
    const char* getDescription() const override
    {
        return "Laad je eigen one-shot/loop en maak er je eigen instrument van — "
               "pitch, filter, drive, pitch-drop, start-punt. G2B SampleX.";
    }
    const char* getMaker() const override { return "G2B (good2better)"; }
    const char* getHomePage() const override { return "https://github.com/good2better"; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','S','x'); }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        if(index==kGalaxy){parameter.name="DIMENTIES";parameter.symbol="galaxy";parameter.ranges.min=0;parameter.ranges.max=26;parameter.ranges.def=9;parameter.hints|=kParameterIsInteger;return;}
        parameter.hints = kParameterIsAutomatable;
        switch (index)
        {
        case kPitch:
            parameter.name = "Pitch"; parameter.symbol = "pitch"; parameter.unit = "st";
            parameter.ranges = {-12.0f, 12.0f, 0.0f}; break;
        case kFilter:
            parameter.name = "Filter"; parameter.symbol = "filter"; parameter.unit = "Hz";
            parameter.ranges = {500.0f, 15000.0f, 8000.0f}; break;
        case kDrive:
            parameter.name = "Drive"; parameter.symbol = "drive"; parameter.unit = "%";
            parameter.ranges = {0.0f, 100.0f, 30.0f}; break;
        case kDrop:
            parameter.name = "Drop"; parameter.symbol = "drop"; parameter.unit = "%";
            parameter.ranges = {0.0f, 100.0f, 40.0f}; break;
        case kStart:
            parameter.name = "Start"; parameter.symbol = "start"; parameter.unit = "%";
            parameter.ranges = {0.0f, 90.0f, 0.0f}; break;
        case kLevel:
            parameter.name = "Level"; parameter.symbol = "level"; parameter.unit = "%";
            parameter.ranges = {0.0f, 100.0f, 85.0f}; break;
        case kDice: parameter.name="Dice"; parameter.symbol="dice"; parameter.hints=kParameterIsInteger|kParameterIsTrigger; parameter.ranges={0.0f,1.0f,0.0f}; break;
        case kPreset:
            parameter.name = "Preset"; parameter.symbol = "preset"; parameter.unit = "";
            parameter.hints = kParameterIsInteger;
            parameter.ranges = {0.0f, 5.0f, 0.0f}; break;
        case kTime:
            parameter.name = "Tijd"; parameter.symbol = "tijd"; parameter.unit = "%";
            parameter.ranges = {50.0f, 200.0f, 100.0f}; break;
        case kTone:
            parameter.name = "Toon"; parameter.symbol = "toon"; parameter.unit = "st";
            parameter.ranges = {-12.0f, 12.0f, 0.0f}; break;
        case kAutofit: parameter.name="AUTO-FIT"; parameter.symbol="autofit"; parameter.hints=kParameterIsTrigger|kParameterIsBoolean; parameter.ranges={0.0f,1.0f,0.0f}; break;
        case kChop:   parameter.name="CHOP";    parameter.symbol="chop";    parameter.unit="slices"; parameter.hints|=kParameterIsInteger; parameter.ranges={0.0f,16.0f,0.0f}; break;
        case kSnap:   parameter.name="KEY-SNAP";parameter.symbol="snap";    parameter.hints|=kParameterIsBoolean; parameter.ranges={0.0f,1.0f,0.0f}; break;
        case kExport: parameter.name="EXPORT WAV"; parameter.symbol="export"; parameter.hints=kParameterIsTrigger|kParameterIsBoolean; parameter.ranges={0.0f,1.0f,0.0f}; break;
        case kFormant:
            parameter.name = "Formant"; parameter.symbol = "formant"; parameter.unit = "st";
            parameter.ranges = {-12.0f, 12.0f, 0.0f}; break;
        case kOn:
            parameter.name = "Aan"; parameter.symbol = "aan";
            parameter.hints |= kParameterIsBoolean;
            parameter.ranges = {0.0f, 1.0f, 1.0f}; break;
        case kKracht:
            parameter.name = "Kracht"; parameter.symbol = "kracht"; parameter.unit = "%";
            parameter.ranges = {0.0f, 100.0f, 100.0f}; break;
        case kSfeer:
            parameter.name = "Sfeer"; parameter.symbol = "sfeer";
            parameter.hints |= kParameterIsInteger;
            parameter.ranges = {0.0f, 2.0f, 1.0f}; break;
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        if(index==kGalaxy)return fGalaxy;
        switch (index)
        {
        case kPitch:  return fPitch;
        case kFilter: return fFilter;
        case kDrive:  return fDrive;
        case kDrop:   return fDrop;
        case kStart:  return fStart;
        case kLevel:  return fLevel;
        case kDice: return fDice; case kPreset: return fPreset;
        case kTime: return fTime; case kTone: return fTone; case kFormant: return fFormant;
        case kAutofit: return 0.f; case kChop: return fChop; case kSnap: return fSnap; case kExport: return 0.f;
        case kOn: return fOn; case kKracht: return fKracht; case kSfeer: return fSfeerV;
        }
        return 0.0f;
    }

    void setParameterValue(uint32_t index, float value) override
    {
        if(index==kGalaxy){fGalaxy=value;galaxy.setPos((int)value);return;}
        if (index == kPreset)
        {
            fPreset = value;
            applyPreset((int)value);
            return;
        }
        switch (index)
        {
        case kPitch:  fPitch = value; break;
        case kFilter: fFilter = value; break;
        case kDrive:  fDrive = value; break;
        case kDrop:   fDrop = value; break;
        case kStart:  fStart = value; break;
        case kLevel:  fLevel = value; break;case kDice: if(value>0.5f)rollDice(); fDice=0.f; break;
        case kTime: if(value!=fTime){fTime=value;fStretchDirty=true;} break;
        case kTone: if(value!=fTone){fTone=value;fStretchDirty=true;} break;
        case kFormant: if(value!=fFormant){fFormant=value;fStretchDirty=true;} break;
        case kAutofit: if(value>0.5f){autofit();} break;
        case kChop: fChop=value; break;
        case kSnap: fSnap=value; break;
        case kExport: if(value>0.5f){exportWav();} break;
        case kOn: fOn=value; break;
        case kKracht: fKracht=value; break;
        case kSfeer: fSfeerV=value; fSf.setMode((int)(value+0.5f)); break;
        }
        recalc();
    }

    void initState(uint32_t, String&, String&) override {}

    void setState(const char* key, const char* value) override
    {
        if (std::strcmp(key, "sample") == 0)
            loadSampleB64(value);
    }

    void activate() override
    {
        for (int v = 0; v < kMaxVoices; ++v) fVoice[v] = SVoice();
        fScopeN = 0;
    }

    void run(const float**, float** outputs, uint32_t frames,
             const MidiEvent* midi, uint32_t midiCount) override
    {
        if(fOn<0.5f){std::memset(outputs[0],0,frames*sizeof(float));std::memset(outputs[1],0,frames*sizeof(float));return;}
        processFx_(nullptr, outputs, frames, midi, midiCount);
        g2b_post(nullptr, outputs, frames, (float)getSampleRate(), fOn, fKracht, galaxy, &fSf, false);
    }

    void processFx_(const float**, float** outputs, uint32_t frames,
             const MidiEvent* midi, uint32_t midiCount)
    {
        float* outL = outputs[0];
        float* outR = outputs[1];
        std::memset(outL, 0, frames * sizeof(float));
        std::memset(outR, 0, frames * sizeof(float));

        const float sr = (float)getSampleRate();
        const float lpA   = 1.0f - std::exp(-2.0f * 3.14159f * fFilter / sr);
        const float drive = 1.0f + (fDrive / 100.0f) * 3.0f;
        const float dropAmt = fDrop / 100.0f * 0.9f;
        const float level = fLevelGain;
        if(fStretchDirty){fStretchDirty=false;rebuildWork();}
        const float startF = (fStart / 100.0f) * (float)fWork.size();
        const bool hasSamp = fWork.size() > 64;

        uint32_t curFrame = 0;
        for (uint32_t e = 0; e <= midiCount; ++e)
        {
            const uint32_t until = (e < midiCount) ? midi[e].frame : frames;
            for (; curFrame < until; ++curFrame)
            {
                float s = 0.0f;
                if (hasSamp)
                {
                    for (int vi = 0; vi < kMaxVoices; ++vi)
                    {
                        SVoice& v = fVoice[vi];
                        if (!v.active) continue;
                        const float drop = 1.0f + dropAmt * std::exp(-v.age * 9.0f / sr);
                        const float rate = v.rate * drop;
                        const int n = (int)fWork.size();
                        const int i0 = (int)v.pos;
                        if (i0 >= n - 1 || (v.end > 0.f && v.pos >= v.end)) { v.active = false; continue; }
                        const float frac = v.pos - i0;
                        float x = fWork[i0] * (1.0f - frac) + fWork[i0 + 1] * frac;
                        v.pos += rate;
                        v.lp += lpA * (x - v.lp);
                        x = std::tanh(v.lp * drive);
                        s += x * v.amp * v.vel;
                        v.age += 1.0f;
                        v.amp *= 0.99995f;
                    }
                }
                const float y = s * level;
                outL[curFrame] = y;
                outR[curFrame] = y;
                if (++fDecim >= 3)
                {
                    fDecim = 0;
                    if (fScopeN < kScopeLen) fScope[fScopeN++] = y;
                }
            }
            if (e < midiCount)
            {
                const MidiEvent& ev = midi[e];
                const uint8_t* data = ev.data;
                const uint8_t status = data[0] & 0xF0;
                if (status == 0x90 && data[2] > 0)
                {
                    SVoice* v = nullptr;
                    for (int i = 0; i < kMaxVoices; ++i)
                        if (!fVoice[i].active) { v = &fVoice[i]; break; }
                    if (!v) v = &fVoice[0];
                    v->active = true;
                    v->note = data[1];
                    v->pos = startF;
                    v->end = -1.0f;
                    const int nChop=(int)fChop;
                    if(nChop>0&&hasSamp){
                        const int idx=data[1]-36;
                        if(idx>=0&&idx<nChop){
                            const float sl=(float)fWork.size()/nChop;
                            v->pos=idx*sl; v->end=v->pos+sl;
                        }
                    }
                    if(fSnap>0.5f){ // start snap naar zero-crossing (geen klik) + hele halve tonen
                        const int n=(int)fWork.size(); int zc=(int)v->pos;
                        for(int d=0;d<4000&&(zc+d)<n-1;d++){if(std::fabs(fWork[zc+d])<0.002f){zc+=d;break;}}
                        v->pos=(float)zc;
                        v->rate=std::pow(2.0f,std::round(data[1]-60.0f+fPitch)/12.0f);
                    } else v->rate=vRate(data[1]);
                    v->amp = 1.0f;
                    v->vel = data[2] / 127.0f;
                    v->age = 0.0f;
                    v->lp = 0.0f;
                }
                else if (status == 0x80 || (status == 0x90 && data[2] == 0))
                {
                    for (int i = 0; i < kMaxVoices; ++i)
                        if (fVoice[i].active && fVoice[i].note == data[1])
                            fVoice[i].amp *= 0.3f;
                }
            }
        }

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
    }

private:
    float fChop=0.f, fSnap=0.f;
    void autofit(){
        const int n=(int)fSamp.size(); if(n<2048)return;
        const float sr=(float)getSampleRate();
        // 1) toon-snap: schat grondtoon via zero-crossing, centreer op dichtste hele noot
        int zc=0,used=0; float prev=0.f;
        const int win=n<sr?n:(int)sr;
        for(int i=1;i<win;i++){if((prev<0&&fSamp[i]>=0)||(prev>0&&fSamp[i]<=0))zc++;prev=fSamp[i];}
        if(zc>4){float fr=zc*sr/(2.f*win);if(fr>40.f&&fr<2000.f){float midi=69.f+12.f*std::log2(fr/440.f);float off=std::round(midi)-midi;
            setParameterValue(kPitch,off<-12.f?-12.f:(off>12.f?12.f:off));}}
        used=zc; (void)used;
        // 2) tempo-fit: stretchen zodat de loop exact X tellen van het DAW-tempo vult
        float bpm=120.f; const TimePosition& tp=getTimePosition(); if(tp.bbt.valid) bpm=(float)tp.bbt.beatsPerMinute;
        const float dur=n/sr, beats=dur*bpm/60.f; float tgt=1.f;
        for(float b: {1.f,2.f,4.f,8.f,16.f}) if(std::fabs(beats-b)<std::fabs(beats-tgt)) tgt=b;
        float tf=(tgt*60.f/bpm)/dur*100.f; if(tf<50.f)tf=50.f; if(tf>200.f)tf=200.f;
        setParameterValue(kTime,tf);
        setState("autofit","sample gefit naar tempo+toon");
    }
    void exportWav(){
        if(fWork.size()<64&&fSamp.size()>=64){fStretchDirty=false;rebuildWork();}
        const char* home=std::getenv("USERPROFILE"); if(!home||!*home)home=std::getenv("HOME");
        char dir[520],path[600];
#ifdef _WIN32
        if(home&&*home)std::snprintf(dir,sizeof(dir),"%s\Music\G2B",home); else std::snprintf(dir,sizeof(dir),".");
        _mkdir(dir);
        std::snprintf(path,sizeof(path),"%s\G2B-SampleX.wav",dir);
#else
        if(home&&*home)std::snprintf(dir,sizeof(dir),"%s/Music/G2B",home); else std::snprintf(dir,sizeof(dir),".");
        char d2[560];std::snprintf(d2,sizeof(d2),"%s/Music",home);mkdir(d2,0755);mkdir(dir,0755);
        std::snprintf(path,sizeof(path),"%s/G2B-SampleX.wav",dir);
#endif
        FILE* f=fopen(path,"wb"); if(!f){setState("wav","export NIET gelukt");return;}
        const uint32_t dn=(uint32_t)fWork.size(),dsz=dn*2,sr=(uint32_t)getSampleRate();
        fwrite("RIFF",1,4,f);uint32_t rs=dsz+36;fwrite(&rs,4,1,f);fwrite("WAVE",1,4,f);
        fwrite("fmt ",1,4,f);uint32_t fmt=16;uint16_t af=1,ch=1,ba=2,bits=16;uint32_t br=sr*2;
        fwrite(&fmt,4,1,f);fwrite(&af,2,1,f);fwrite(&ch,2,1,f);fwrite(&sr,4,1,f);fwrite(&br,4,1,f);fwrite(&ba,2,1,f);fwrite(&bits,2,1,f);
        fwrite("data",1,4,f);fwrite(&dsz,4,1,f);
        for(uint32_t i=0;i<dn;i++){float x=fWork[i];x=x<-1?-1:(x>1?1:x);int16_t v=(int16_t)(x*32767);fwrite(&v,2,1,f);}
        fclose(f); setState("wav",".wav opgeslagen: Music\G2B\G2B-SampleX.wav");
    }
    void rollDice(){
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kPitch,-12.f+((fRnd>>8)%100)/100.f*24.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kFilter,500.f+((fRnd>>8)%100)/100.f*12000.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kDrive,((fRnd>>8)%100)/100.f*70.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kDrop,((fRnd>>8)%100)/100.f*90.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kStart,((fRnd>>8)%100)/100.f*70.f);
    }
    float fDice = 0.f;
    unsigned fRnd = 998877;

    float fPitch, fFilter, fDrive, fDrop, fStart, fLevel, fPreset;
    float fLevelGain;
    std::vector<float> fSamp;
    std::vector<float> fWork;
    float fTime=100.f, fTone=0.f, fFormant=0.f;
    bool fStretchDirty=false;

    void rebuildWork()
    {
        const int inN=(int)fSamp.size();
        if(inN<64){fWork=fSamp;return;}
        float tf=fTime/100.f; if(tf<0.5f)tf=0.5f; if(tf>2.f)tf=2.f;
        if(tf>0.999f&&tf<1.001f&&fTone==0.f&&fFormant==0.f){fWork=fSamp;return;}
        int outN=(int)(inN*tf); if(outN<64)outN=64;
        fWork.assign(outN,0.f);
        struct IW{const float* p; const float* operator[](int)const{return p;}} iw{fSamp.data()};
        struct OW{float* p; float* operator[](int)const{return p;}} ow{fWork.data()};
        signalsmith::stretch::SignalsmithStretch<float> st;
        st.presetCheaper(1,(int)getSampleRate());
        if(fTone!=0.f)st.setTransposeSemitones(fTone,120.f);
        if(fFormant!=0.f)st.setFormantSemitones(fFormant,true);
        st.exact(iw,inN,ow,outN);
    }
    SVoice fVoice[kMaxVoices];
    static const int kScopeLen = 96;
    float fScope[kScopeLen];
    uint32_t fScopeN, fDecim;

    float vRate(uint8_t note) const
    {
        // basistoon = C4 (60): sample klinkt als de bron bij C4
        return std::pow(2.0f, ((float)note - 60.0f + fPitch) / 12.0f);
    }

    void applyPreset(int i)
    {
        if (i < 0 || i >= kNumPresets) return;
        const float* p = kPresetParams[i];
        setParameterValue(kPitch, p[0]);
        setParameterValue(kFilter, p[1]);
        setParameterValue(kDrive, p[2]);
        setParameterValue(kDrop, p[3]);
        setParameterValue(kStart, p[4]);
        setParameterValue(kLevel, p[5]);
        recalc();
    }

    void recalc()
    {
        fLevelGain = (fLevel / 100.0f) * 0.95f;
    }

    void loadSampleB64(const char* b64)
    {
        // decode base64 -> bytes
        std::vector<unsigned char> bytes;
        const size_t len = std::strlen(b64);
        bytes.reserve(len * 3 / 4);
        int acc = 0, nbits = 0;
        for (size_t i = 0; i < len; ++i)
        {
            const int v = b64v(b64[i]);
            if (v < 0) continue;
            acc = (acc << 6) | v;
            nbits += 6;
            if (nbits >= 8)
            {
                nbits -= 8;
                bytes.push_back((unsigned char)((acc >> nbits) & 0xFF));
            }
        }
        // format-detectie: FLAC, MP3 of WAV (PCM16)
        fSamp.clear();
        if (bytes.size() >= 4 && std::memcmp(bytes.data(), "fLaC", 4) == 0)
        {
            unsigned int ch = 0, srate = 0; drflac_uint64 frames = 0;
            float* pcm = drflac_open_memory_and_read_pcm_frames_f32(
                bytes.data(), bytes.size(), &ch, &srate, &frames, nullptr);
            if (pcm && frames > 0 && ch > 0)
            {
                fSamp.resize((size_t)frames);
                for (drflac_uint64 i = 0; i < frames; ++i)
                {
                    float s = 0.0f;
                    for (unsigned int c = 0; c < ch; ++c) s += pcm[i * ch + c];
                    fSamp[(size_t)i] = s / (float)ch;
                }
            }
            if (pcm) drflac_free(pcm, nullptr);
            if (!fSamp.empty()) { fStretchDirty = true; return; }
            return;
        }
        if (bytes.size() >= 3 && (std::memcmp(bytes.data(), "ID3", 3) == 0 ||
                                  (bytes[0] == 0xFF && (bytes[1] & 0xE0) == 0xE0)))
        {
            drmp3_config cfg; drmp3_uint64 frames = 0;
            float* pcm = drmp3_open_memory_and_read_pcm_frames_f32(
                bytes.data(), bytes.size(), &cfg, &frames, nullptr);
            if (pcm && frames > 0 && cfg.channels > 0)
            {
                fSamp.resize((size_t)frames);
                for (drmp3_uint64 i = 0; i < frames; ++i)
                {
                    float s = 0.0f;
                    for (drmp3_uint32 c = 0; c < cfg.channels; ++c) s += pcm[i * cfg.channels + c];
                    fSamp[(size_t)i] = s / (float)cfg.channels;
                }
            }
            if (pcm) drmp3_free(pcm, nullptr);
            return;
        }
        // parse WAV: RIFF ... fmt ... data (PCM16)
        if (bytes.size() < 44 || std::memcmp(bytes.data(), "RIFF", 4) != 0)
            return;
        int ch = 1, bits = 16, dataOff = -1; size_t dataLen = 0;
        for (size_t p = 12; p + 8 <= bytes.size();)
        {
            const uint32_t sz = bytes[p+4] | (bytes[p+5]<<8) | (bytes[p+6]<<16) | ((uint32_t)bytes[p+7]<<24);
            if (std::memcmp(bytes.data()+p, "fmt ", 4) == 0 && sz >= 16)
            {
                ch = bytes[p+10] | (bytes[p+11]<<8);
                bits = bytes[p+22] | (bytes[p+23]<<8);
            }
            if (std::memcmp(bytes.data()+p, "data", 4) == 0)
            { dataOff = (int)p + 8; dataLen = sz; break; }
            p += 8 + sz + (sz & 1);
        }
        if (dataOff < 0 || bits != 16) return;
        const int frames = (int)dataLen / (2 * ch);
        fSamp.resize(frames);
        const short* raw = (const short*)(bytes.data() + dataOff);
        for (int i = 0; i < frames; ++i)
        {
            float s = 0.0f;
            for (int c = 0; c < ch; ++c) s += raw[i * ch + c] / 32768.0f;
            fSamp[i] = s / ch;
        }
        fStretchDirty=true;
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleXPlugin)
    float fGalaxy=9.f;
    Galaxy galaxy;
    float fOn=1.f, fKracht=100.f, fSfeerV=1.f;
    Sfeer fSf;
};

Plugin* createPlugin() { return new SampleXPlugin(); }

END_NAMESPACE_DISTRHO
