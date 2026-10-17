/*
 * G2B Knip — luider en breder zonder pieken: soft-clipper + plafond +
 * stereo-breedte + mix. Met presets die weten wat ze doen.
 * Stuurt een live golfvorm naar de UI (state "scope").
 */

#include "DistrhoPlugin.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

START_NAMESPACE_DISTRHO

static const int kNumPrograms = 17;
// CLIP, CEIL(dB), BREEDTE(%), MIX(%), OUTPUT(dB), SOFT(%)
static const float kProgramParams[kNumPrograms][6] = {
    { 35.0f, -1.0f, 110.0f, 100.0f, 0.0f, 50.0f },   // G2B — signature: luider, breed, strak
    { 45.0f, -1.5f, 100.0f, 100.0f, 0.0f, 40.0f },   // DEMBOW — hard en in je gezicht
    { 55.0f, -0.8f,  90.0f, 100.0f, 0.0f, 20.0f },   // DRILL — agressief, smaller
    { 30.0f, -2.0f, 125.0f, 100.0f, 0.0f, 70.0f },   // RNB — schoon, extra breed
    { 40.0f, -1.0f, 115.0f, 100.0f, 0.0f, 50.0f },   // HOUSE — punchy width
    { 38.0f, -1.2f, 120.0f, 100.0f, 0.0f, 60.0f },   // AFRO — warm en rond
    { 60.0f, -0.8f,  95.0f, 100.0f, 0.0f, 30.0f },   // TRAP
    { 32.0f, -1.8f, 105.0f, 100.0f, 0.0f, 60.0f },   // BOOM
    { 42.0f, -1.0f, 118.0f, 100.0f, 0.0f, 50.0f },   // UKG
    { 50.0f, -1.2f, 110.0f, 100.0f, 0.0f, 45.0f },   // JERSEY
    { 44.0f, -1.0f, 112.0f, 100.0f, 0.0f, 50.0f },   // BOUNCE
    { 28.0f, -2.0f, 120.0f, 100.0f, 0.0f, 70.0f },   // SOUL
    { 25.0f, -2.2f, 100.0f, 90.0f, 0.0f, 80.0f },    // LOFI
    { 38.0f, -1.4f, 108.0f, 100.0f, 0.0f, 55.0f },   // FUNK
    { 36.0f, -1.6f, 116.0f, 100.0f, 0.0f, 65.0f },   // REGGAE
    { 46.0f, -1.0f, 115.0f, 100.0f, 0.0f, 40.0f },   // DANCE
    { 40.0f, -1.2f, 112.0f, 100.0f, 0.0f, 55.0f },   // KAWINA
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

class KnipPlugin : public Plugin
{
public:
    enum Parameters {
        kClip = 0, kCeil, kWidth, kMix, kOutput, kGalaxy, kDice, kPreset, kSoft, kOn, kKracht, kParameterCount
    };

    KnipPlugin()
        : Plugin(kParameterCount, kNumPrograms, 0),
          fClip(35.0f), fCeil(-1.0f), fWidth(110.0f), fMix(100.0f), fOut(0.0f),
          fSoft(50.0f), fScopeN(0), fDecim(0)
    {
        recalc();
    }

protected:
    const char* getLabel() const override { return "G2B Knip"; }
    const char* getDescription() const override
    {
        return "G2B Knip: soft-clipper + plafond + stereo-breedte — "
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
        case kClip:
            parameter.name = "Clip"; parameter.symbol = "clip"; parameter.unit = "%";
            parameter.ranges = {0.0f, 100.0f, 35.0f}; break;
        case kCeil:
            parameter.name = "Ceiling"; parameter.symbol = "ceil"; parameter.unit = "dB";
            parameter.ranges = {-6.0f, 0.0f, -1.0f}; break;
        case kWidth:
            parameter.name = "Breedte"; parameter.symbol = "width"; parameter.unit = "%";
            parameter.ranges = {0.0f, 200.0f, 110.0f}; break;
        case kMix:
            parameter.name = "Mix"; parameter.symbol = "mix"; parameter.unit = "%";
            parameter.ranges = {0.0f, 100.0f, 100.0f}; break;
        case kOutput:
            parameter.name = "Output"; parameter.symbol = "output"; parameter.unit = "dB";
            parameter.ranges = {-6.0f, 6.0f, 0.0f}; break;
        case kDice: parameter.name="Dice"; parameter.symbol="dice"; parameter.hints=kParameterIsInteger|kParameterIsTrigger; parameter.ranges={0.0f,1.0f,0.0f}; break;
        case kPreset:
            parameter.name = "Preset"; parameter.symbol = "preset"; parameter.unit = "";
            parameter.ranges = {0.0f, 16.0f, 0.0f};
            parameter.hints = kParameterIsInteger;
            break;
        case kSoft:
            parameter.name = "Soft"; parameter.symbol = "soft"; parameter.unit = "%";
            parameter.ranges = {0.0f, 100.0f, 50.0f}; break;
        case kOn: parameter.name="Aan"; parameter.symbol="aan"; parameter.hints=kParameterIsAutomatable|kParameterIsBoolean; parameter.ranges={0.0f,1.0f,1.0f}; break;
        case kKracht: parameter.name="Kracht"; parameter.symbol="kracht"; parameter.unit="%"; parameter.ranges={0.0f,100.0f,100.0f}; break;
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        if(index==kGalaxy)return fGalaxy;
        switch (index)
        {
        case kClip:   return fClip;
        case kCeil:   return fCeil;
        case kWidth:  return fWidth;
        case kMix:    return fMix;
        case kOutput: return fOut;
        case kSoft:   return fSoft;
        case kDice: return fDice; case kPreset: return fPreset;
        case kOn: return fOn; case kKracht: return fKr;
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
        case kClip:   fClip = value; break;
        case kCeil:   fCeil = value; break;
        case kWidth:  fWidth = value; break;
        case kMix:    fMix = value; break;
        case kOutput: fOut = value; break;
        case kSoft:   fSoft = value; break;
        case kDice: if(value>0.5f)rollDice(); fDice=0.f; break;
        case kOn: fOn = value; break; case kKracht: fKr = value; break;
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

    void activate() override { fScopeN = 0; }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        if(fOn<0.5f){if(inputs[0]!=outputs[0])std::memcpy(outputs[0],inputs[0],frames*sizeof(float));if(inputs[1]!=outputs[1])std::memcpy(outputs[1],inputs[1],frames*sizeof(float));return;}
        processFx_(inputs, outputs, frames);
        for(uint32_t i=0;i<frames;i++){
            float L=outputs[0][i],R=outputs[1][i];
            galaxy.tick(L,R,getSampleRate());
            outputs[0][i]=L; outputs[1][i]=R;
        }
        const float k=fKr/100.f;
        if(k<0.999f){const float d=1.f-k;for(uint32_t i=0;i<frames;i++){outputs[0][i]=inputs[0][i]*d+outputs[0][i]*k;outputs[1][i]=inputs[1][i]*d+outputs[1][i]*k;}}
    }

    void processFx_(const float** inputs, float** outputs, uint32_t frames)
    {
        const float* inL = inputs[0];
        const float* inR = inputs[1];
        float* outL = outputs[0];
        float* outR = outputs[1];

        const float drive  = fDrive;    // 1..9
        const float ceil   = fCeilLin;  // lineair plafond
        const float w      = fWidthM;   // mid/side width
        const float mix    = fMixN;     // 0..1
        const float gain   = fOutGain;
        const float soft   = fSoftN;   // 0..1 — hard clip ↔ soft knee

        for (uint32_t i = 0; i < frames; ++i)
        {
            float l = inL[i], r = inR[i];

            // breedte via mid/side
            const float m = (l + r) * 0.5f;
            const float s = (l - r) * 0.5f * w;
            float wl = m + s, wr = m - s;

            // clip: drive + soft-clip + plafond
            wl = clipAt(wl * drive, ceil, soft);
            wr = clipAt(wr * drive, ceil, soft);

            outL[i] = (l * (1.0f - mix) + wl * mix) * gain;
            outR[i] = (r * (1.0f - mix) + wr * mix) * gain;
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
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kClip,((fRnd>>8)%100)/100.f*70.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kCeil,-4.f+((fRnd>>8)%100)/100.f*3.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kWidth,50.f+((fRnd>>8)%100)/100.f*140.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kMix,60.f+((fRnd>>8)%100)/100.f*40.f);
    }
    float fDice = 0.f;
    unsigned fRnd = 998877;

    float fClip, fCeil, fWidth, fMix, fOut, fSoft, fPreset = 0.0f;
    float fDrive, fCeilLin, fWidthM, fMixN, fOutGain, fSoftN;
    static const int kScopeLen = 96;
    float fScope[kScopeLen];
    uint32_t fScopeN, fDecim;

    void applyPreset(int i)
    {
        if (i < 0 || i >= kNumPrograms) return;
        const float* p = kProgramParams[i];
        fClip = p[0]; fCeil = p[1]; fWidth = p[2]; fMix = p[3]; fOut = p[4]; fSoft = p[5];
        recalc();
        // knoppen in de UI/host meebewegen
        setParameterValue(kClip, fClip);
        setParameterValue(kCeil, fCeil);
        setParameterValue(kWidth, fWidth);
        setParameterValue(kMix, fMix);
        setParameterValue(kOutput, fOut);
        setParameterValue(kSoft, fSoft);
    }

    static inline float clipAt(float x, float ceil, float soft)
    {
        // SOFT=0 → hard clip op 'ceil'; SOFT=1 → tanh-knee rond 'ceil'
        const float a = std::fabs(x);
        const float hard = a <= ceil ? x : (x > 0.0f ? ceil : -ceil);
        float knee = x;
        if (a > ceil * 0.9f)
        {
            knee = std::tanh(x / ceil) * ceil;
        }
        return hard + (knee - hard) * soft;
    }

    void recalc()
    {
        fDrive = 1.0f + (fClip / 100.0f) * 8.0f;
        fCeilLin = std::pow(10.0f, fCeil / 20.0f);
        fWidthM = fWidth / 100.0f;
        fMixN = fMix / 100.0f;
        fOutGain = std::pow(10.0f, fOut / 20.0f);
        fSoftN = fSoft / 100.0f;
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KnipPlugin)
    float fOn=1.f,fKr=100.f,fGalaxy=9.f;
    Galaxy galaxy;
};

Plugin* createPlugin() { return new KnipPlugin(); }

END_NAMESPACE_DISTRHO
