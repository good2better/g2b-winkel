/*
 * G2B Tijd — echte pitch-shift + formant op je audio (signalsmith-stretch, MIT).
 * TOON = semitones, FORMANT = karakter, MIX = dry/wet, SMOOTH = smoothness.
 * Stuurt een live golfvorm naar de UI (state "scope").
 */

#include "DistrhoPlugin.hpp"
#include "vendor/signalsmith-stretch.h"
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstring>

START_NAMESPACE_DISTRHO

static const int kNumPrograms = 17;
// CLIP, CEIL(dB), BREEDTE(%), MIX(%), OUTPUT(dB)
static const float kProgramParams[kNumPrograms][5] = {
    {   0.0f,  0.0f, 100.0f, 0.0f, 120.0f },  // G2B
    {  -5.0f,  0.0f, 100.0f, 0.0f, 120.0f },  // DEMBOW
    {  -7.0f, -2.0f,  90.0f, 0.0f, 100.0f },  // DRILL
    {  +2.0f, +1.0f,  80.0f, 0.0f, 150.0f },  // RNB
    {   0.0f, +2.0f, 100.0f, 0.0f, 140.0f },  // HOUSE
    {  -3.0f, +1.0f,  90.0f, 0.0f, 120.0f },  // AFRO
    { -12.0f,  0.0f, 100.0f, 0.0f, 100.0f },  // TRAP — oct-down
    {  -2.0f, -1.0f,  85.0f, 0.0f, 130.0f },  // BOOM
    {  +5.0f, +2.0f,  90.0f, 0.0f, 140.0f },  // UKG
    {  +7.0f, +3.0f,  85.0f, 0.0f, 140.0f },  // JERSEY
    {  -4.0f,  0.0f,  95.0f, 0.0f, 120.0f },  // BOUNCE
    {  -1.0f, -1.0f,  80.0f, 0.0f, 160.0f },  // SOUL
    {  -3.0f, -3.0f,  70.0f,-1.0f, 200.0f },  // LOFI — donker+los
    {  +3.0f, +2.0f,  85.0f, 0.0f, 150.0f },  // FUNK
    {  -2.0f,  0.0f,  80.0f, 0.0f, 120.0f },  // REGGAE
    {  +4.0f, +2.0f,  90.0f, 0.0f, 140.0f },  // DANCE
    {  -5.0f, -1.0f,  90.0f, 0.0f, 110.0f },  // KAW — surinaams donker
};
static const char* kProgramNames[kNumPrograms] = {"G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"};


// ---- GALAXY-knop (bron: backend/app/galaxy_fx.py, devin-test) — 27 posities ----
struct GxShifter {
    static const int N=2048; float buf[N]; int wp=0; float ph=0;
    void reset(){std::memset(buf,0,sizeof buf);wp=0;ph=0;}
    float tick(float in,float rate){
        buf[wp]=in; float acc=0;
        for(int t=0;t<2;t++){
            float p=ph+0.5f*t; if(p>=1.f)p-=1.f;
            float rd=(float)wp-p*(float)N;
            int i0=(int)floorf(rd); float fr=rd-i0;
            float a=buf[((i0%N)+N)%N],b=buf[(((i0+1)%N)+N)%N];
            float w=p<0.5f?p*2.f:2.f-p*2.f;
            acc+=(a+(b-a)*fr)*w;
        }
        ph+=(1.f-rate)/(float)N; if(ph<0)ph+=1.f; if(ph>=1.f)ph-=1.f;
        wp=(wp+1)%N;
        return acc;
    }
};
struct Galaxy { Galaxy(){reset();}
    GxShifter sh[4];
    static const int CN=2200;
    float cb[4][CN]; int cw[4];
    float ap[2][600]; int aw[2];
    float lp[2];
    int gpos=-1; float grate=2.f,gfb=0.5f,gfc=6500.f,gwet=0.6f,gdd=0.2f; int gmode=2;
    void reset(){for(int i=0;i<4;i++)sh[i].reset();std::memset(cb,0,sizeof cb);std::memset(ap,0,sizeof ap);
                 std::memset(cw,0,sizeof cw);std::memset(aw,0,sizeof aw);lp[0]=lp[1]=0;}
    void setPos(int pos){
        if(pos==gpos)return; gpos=pos;
        gmode=pos/9; int dim=pos%9;
        uint32_t sd=(gmode==0?10007u:(gmode==1?20011u:30017u))*9u+dim;
        auto rng=[&](){sd^=sd<<13;sd^=sd>>17;sd^=sd<<5;return (sd&0xFFFFFFu)/16777216.f;};
        grate=powf(2.f,(gmode==0?-12.f:12.f)/12.f);
        gdd=(gmode==0?0.20f:(gmode==1?0.16f:0.24f))*(0.8f+0.4f*rng());
        gfb=(gmode==0?0.50f:(gmode==1?0.40f:0.55f))*(0.85f+0.3f*rng()); if(gfb>0.72f)gfb=0.72f;
        gfc=(gmode==0?2600.f:(gmode==1?9000.f:6500.f))*(0.85f+0.35f*rng());
        gwet=(gmode==0?0.55f:(gmode==1?0.50f:0.62f));
    }
    void tick(float& L,float& R,float sr){
        float in[2]={L,R},shim[2]={0,0};
        for(int c=0;c<2;c++){
            float x=sh[c].tick(in[c],grate);
            if(gmode==2)x+=0.6f*sh[2+c].tick(in[c],1.4983f);
            float a=expf(-6.283f*(gmode==0?gfc:320.f)/sr);
            lp[c]=(1.f-a)*x+a*lp[c];
            x=(gmode==0)?lp[c]:(x-lp[c]);
            int dd=(int)(gdd*sr); if(dd<64)dd=64; if(dd>CN-8)dd=CN-8;
            float tail=0;
            for(int k=0;k<4;k++){
                int rp=(cw[k]-dd-(dd*k/4)+CN*8)%CN;
                float e=cb[k][rp]; tail+=e*0.25f;
                cb[k][cw[k]]=x+e*gfb; cw[k]=(cw[k]+1)%CN;
            }
            const float g=0.5f;
            float av=ap[c][aw[c]];
            float y=-g*tail+av;
            ap[c][aw[c]]=tail+g*y; aw[c]=(aw[c]+1)%600;
            shim[c]=y;
        }
        L=in[0]+gwet*shim[0]*0.10f; R=in[1]+gwet*shim[1]*0.10f;
        if(L>0.98f)L=0.98f; else if(L<-0.98f)L=-0.98f;
        if(R>0.98f)R=0.98f; else if(R<-0.98f)R=-0.98f;
    }
};

class TijdPlugin : public Plugin
{
public:
    enum Parameters {
        kToon = 0, kFormant, kMix, kOutput, kSmooth, kGalaxy, kDice, kPreset, kParameterCount
    };

    TijdPlugin()
        : Plugin(kParameterCount, kNumPrograms, 0),
          fToon(0.0f), fFormant(0.0f), fMix(100.0f), fOut(0.0f), fSmooth(120.0f),
          fScopeN(0), fDecim(0)
    {
        recalc();
    }

protected:
    const char* getLabel() const override { return "G2B Tijd"; }
    const char* getDescription() const override
    {
        return "G2B Tijd: soft-clipper + plafond + stereo-breedte — "
               "luider en breder zonder pieken. Met G2B-presets.";
    }
    const char* getMaker() const override { return "G2B (good2better)"; }
    const char* getHomePage() const override { return "https://github.com/good2better"; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','K','n'); }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        if(index==kGalaxy){parameter.name="GALAXY";parameter.symbol="galaxy";parameter.ranges.min=0;parameter.ranges.max=26;parameter.ranges.def=9;parameter.hints|=kParameterIsInteger;return;}
        parameter.hints = kParameterIsAutomatable;
        switch (index)
        {
        case kToon:
            parameter.name = "Toon"; parameter.symbol = "toon"; parameter.unit = "st";
            parameter.ranges = {-24.0f, 24.0f, 0.0f}; break;
        case kFormant:
            parameter.name = "Formant"; parameter.symbol = "formant"; parameter.unit = "st";
            parameter.ranges = {-12.0f, 12.0f, 0.0f}; break;
        case kMix:
            parameter.name = "Mix"; parameter.symbol = "mix"; parameter.unit = "%";
            parameter.ranges = {0.0f, 100.0f, 100.0f}; break;
        case kOutput:
            parameter.name = "Output"; parameter.symbol = "output"; parameter.unit = "dB";
            parameter.ranges = {-12.0f, 12.0f, 0.0f}; break;
        case kSmooth:
            parameter.name = "Smooth"; parameter.symbol = "smooth"; parameter.unit = "Hz";
            parameter.ranges = {20.0f, 500.0f, 120.0f}; break;
        case kDice: parameter.name="Dice"; parameter.symbol="dice"; parameter.hints=kParameterIsInteger|kParameterIsTrigger; parameter.ranges={0.0f,1.0f,0.0f}; break;
        case kPreset:
            parameter.name = "Preset"; parameter.symbol = "preset"; parameter.unit = "";
            parameter.ranges = {0.0f, 5.0f, 0.0f};
            parameter.hints = kParameterIsInteger;
            break;
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        if(index==kGalaxy)return fGalaxy;
        switch (index)
        {
        case kToon:   return fToon;
        case kFormant:return fFormant;
        case kMix:    return fMix;
        case kOutput: return fOut;
        case kSmooth: return fSmooth;
        case kDice: return fDice; case kPreset: return fPreset;
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
        case kToon:   fToon = value; break;
        case kFormant:fFormant = value; break;
        case kMix:    fMix = value; break;
        case kOutput: fOut = value; break;
        case kSmooth: fSmooth = value; break;case kDice: if(value>0.5f)rollDice(); fDice=0.f; break;
        }
        recalc();
    }

    void initProgramName(uint32_t index, String& programName) override
    {
        programName = kProgramNames[index];
    }

    void loadProgram(uint32_t index) override
    {
        applyPreset((int)index);
        for (int i = 0; i < kParameterCount; ++i)
            setParameterValue(i, getParameterValue(i));
    }

    void activate() override
    {
        fScopeN = 0;
        if (!stretchReady_)
        {
            const int sr = (int)getSampleRate();
            stretch_.presetCheaper(2, sr, true);
            setLatency(stretch_.inputLatency() + stretch_.outputLatency());
            wetL_.assign(8192, 0.f); wetR_.assign(8192, 0.f);
            stretchReady_ = true;
        }
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        processFx_(inputs, outputs, frames);
        for(uint32_t i=0;i<frames;i++){
            float L=outputs[0][i],R=outputs[1][i];
            galaxy.tick(L,R,getSampleRate());
            outputs[0][i]=L; outputs[1][i]=R;
        }
    }

    void processFx_(const float** inputs, float** outputs, uint32_t frames)
    {
        const float* inL = inputs[0];
        const float* inR = inputs[1];
        float* outL = outputs[0];
        float* outR = outputs[1];
        const float mix = fMix / 100.0f;
        const float gain = fOutGain;

        // params live aan de stretch-engine doorgeven
        if (fToon != fLastToon) { stretch_.setTransposeSemitones(fToon, fSmooth); fLastToon = fToon; }
        if (fFormant != fLastFormant) { stretch_.setFormantSemitones(fFormant, true); fLastFormant = fFormant; }
        if (fSmooth != fLastSmooth) { stretch_.setTransposeSemitones(fToon, fSmooth); fLastSmooth = fSmooth; }

        const uint32_t chunk = (frames <= 8192) ? frames : 8192;
        struct IW { const float* const* p; const float* operator[](int c) const { return p[c]; } };
        struct OW { float** p; float* operator[](int c) const { return p[c]; } };
        const float* ins[2] = { inL, inR };
        float* wets[2] = { wetL_.data(), wetR_.data() };
        for (uint32_t off = 0; off < frames; off += chunk)
        {
            const uint32_t n = (frames - off < chunk) ? (frames - off) : chunk;
            const float* ci[2] = { inL + off, inR + off };
            float* co[2] = { wets[0], wets[1] };
            IW iw{ci}; OW ow{co};
            stretch_.process(iw, (int)n, ow, (int)n);
            for (uint32_t i = 0; i < n; ++i)
            {
                const uint32_t j = off + i;
                outL[j] = (inL[j] * (1.0f - mix) + wets[0][i] * mix) * gain;
                outR[j] = (inR[j] * (1.0f - mix) + wets[1][i] * mix) * gain;
            }
        }

        // scope: mono uitgang ~10x/sec naar UI
        for (uint32_t i = 0; i < frames; ++i)
        {
            if (++fDecim >= 3)
            {
                fDecim = 0;
                if (fScopeN < kScopeLen)
                    fScope[fScopeN++] = (outL[i] + outR[i]) * 0.5f;
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
    void rollDice(){
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kToon,-12.f+((fRnd>>8)%100)/100.f*24.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kFormant,-6.f+((fRnd>>8)%100)/100.f*12.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kSmooth,60.f+((fRnd>>8)%100)/100.f*300.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kMix,50.f+((fRnd>>8)%100)/100.f*50.f);
    }
    float fDice = 0.f;
    unsigned fRnd = 998877;

    float fToon, fFormant, fMix, fOut, fSmooth, fPreset = 0.0f;
    float fOutGain;
    float fLastToon = -999.f, fLastFormant = -999.f, fLastSmooth = -999.f;
    signalsmith::stretch::SignalsmithStretch<float> stretch_;
    std::vector<float> wetL_, wetR_;
    bool stretchReady_ = false;
    static const int kScopeLen = 96;
    float fScope[kScopeLen];
    uint32_t fScopeN, fDecim;

    void applyPreset(int i)
    {
        if (i < 0 || i >= kNumPrograms) return;
        const float* p = kProgramParams[i];
        fToon = p[0]; fFormant = p[1]; fMix = p[2]; fOut = p[3]; fSmooth = p[4];
        recalc();
        setParameterValue(kToon, fToon);
        setParameterValue(kFormant, fFormant);
        setParameterValue(kMix, fMix);
        setParameterValue(kOutput, fOut);
        setParameterValue(kSmooth, fSmooth);
    }

    static inline float clipAt(float x, float ceil)
    {
        // soft-clip rond 'ceil': cubic knee, daarna hard af
        const float a = std::fabs(x);
        if (a <= ceil * 0.9f) return x;
        float y = x / ceil;
        y = std::tanh(y);
        y *= ceil;
        return y;
    }

    void recalc()
    {
        fOutGain = std::pow(10.0f, fOut / 20.0f);
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TijdPlugin)
    float fGalaxy=9.f;
    Galaxy galaxy;
};

Plugin* createPlugin() { return new TijdPlugin(); }

END_NAMESPACE_DISTRHO
