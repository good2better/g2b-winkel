/*
 * G2B SubTamer — "sub always on the right spot".
 * DSP: splits the sub band (below VANAF Hz) with a two-stage one-pole LP,
 * then levels it with a soft compressor (envelope follower, TEM sets how
 * hard it pulls the sub back to a steady level), forces the sub toward
 * mono (MONO), and trims it (STERKTE). Everything above the split passes
 * through untouched, stereo intact.
 */

#include "DistrhoPlugin.hpp"
#include <cstring>

START_NAMESPACE_DISTRHO

static const char* kPresetNames[17] = {"G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"};
static const int kNumPresets = 17;
static const float kPresetParams[17][5] = {
    { 55, 120, -3, 100, 0 },
    { 65, 110, -3.5, 100, 0 },
    { 70, 90, -4, 100, 0 },
    { 50, 130, -2.5, 90, 0 },
    { 60, 120, -3, 100, 0 },
    { 55, 100, -3, 100, 0 },
    { 75, 95, -4.5, 100, 0 },
    { 45, 140, -2, 90, 0 },
    { 70, 100, -4, 95, 0 },
    { 80, 90, -5, 100, 0 },
    { 60, 115, -3.2, 100, 0 },
    { 50, 125, -2.8, 95, 0 },
    { 55, 135, -2.5, 90, 0 },
    { 65, 105, -3.8, 100, 0 },
    { 58, 118, -3, 100, 0 },
    { 62, 112, -3.5, 100, 0 },
    { 60, 110, -3.2, 100, 0 },
};



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

class SubTamerPlugin : public Plugin
{
public:
    enum Parameters {
        kTem = 0,     // TEM % — hoe hard de sub wordt getemd
        kVanaf,       // VANAF Hz — alles hieronder is "de sub"
        kSterkte,     // STERKTE dB — hoe luid de sub zit
        kMono,        // MONO % — hoe mono de sub wordt
        kOut,         // OUTPUT dB
        kGalaxy, kDice, kPreset,
        kOn, kKracht,
        kParameterCount
    };

    SubTamerPlugin()
        : Plugin(kParameterCount, 0, 0),
          fTem(50.0f), fVanaf(100.0f), fSterkte(0.0f),
          fMono(100.0f), fOutDb(0.0f),
          fLp1L(0.0f), fLp2L(0.0f), fLp1R(0.0f), fLp2R(0.0f), fEnv(0.0f),
          fAlpha(0.0f), fEnvAtk(0.0f), fEnvRel(0.0f),
          fThr(0.0f), fKneeK(0.0f), fSubGain(1.0f), fOutGain(1.0f)
    {
        recalc();
    }

protected:
    const char* getLabel() const override { return "G2B SubTamer"; }
    const char* getDescription() const override
    {
        return "Sub always on the right spot: temt de sub-band naar een "
               "vast niveau en houdt die mono — nooit te wild, nooit uit "
               "balans.";
    }
    const char* getMaker() const override { return "G2B (good2better)"; }
    const char* getHomePage() const override { return "https://github.com/good2better"; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','S','t'); }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        if(index==kGalaxy){parameter.name="GALAXY";parameter.symbol="galaxy";parameter.ranges.min=0;parameter.ranges.max=26;parameter.ranges.def=9;parameter.hints|=kParameterIsInteger;return;}
        parameter.hints = kParameterIsAutomatable;
        switch (index)
        {
        case kTem:
            parameter.name = "Tem";
            parameter.shortName = "Tem";
            parameter.symbol = "tem";
            parameter.unit = "%";
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 100.0f;
            parameter.ranges.def = 50.0f;
            break;
        case kVanaf:
            parameter.name = "Sub vanaf";
            parameter.shortName = "Vanaf";
            parameter.symbol = "vanaf";
            parameter.unit = "Hz";
            parameter.ranges.min = 60.0f;
            parameter.ranges.max = 160.0f;
            parameter.ranges.def = 100.0f;
            break;
        case kSterkte:
            parameter.name = "Sterkte";
            parameter.shortName = "Sterkte";
            parameter.symbol = "sterkte";
            parameter.unit = "dB";
            parameter.ranges.min = -12.0f;
            parameter.ranges.max = 6.0f;
            parameter.ranges.def = 0.0f;
            break;
        case kMono:
            parameter.name = "Mono";
            parameter.shortName = "Mono";
            parameter.symbol = "mono";
            parameter.unit = "%";
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 100.0f;
            parameter.ranges.def = 100.0f;
            break;
        case kOut:
            parameter.name = "Output";
            parameter.shortName = "Output";
            parameter.symbol = "out";
            parameter.unit = "dB";
            parameter.ranges.min = -6.0f;
            parameter.ranges.max = 6.0f;
            parameter.ranges.def = 0.0f;
            break;
        case kDice: parameter.name="Dice"; parameter.symbol="dice"; parameter.hints=kParameterIsInteger|kParameterIsTrigger; parameter.ranges={0.0f,1.0f,0.0f}; break;
        case kPreset:
            parameter.name = "Preset";
            parameter.symbol = "preset";
            parameter.unit = "";
            parameter.hints = kParameterIsInteger;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 16.0f;
            parameter.ranges.def = 0.0f;
            break;
        case kOn: parameter.name="Aan"; parameter.symbol="aan"; parameter.hints=kParameterIsAutomatable|kParameterIsBoolean; parameter.ranges={0.0f,1.0f,1.0f}; break;
        case kKracht: parameter.name="Kracht"; parameter.symbol="kracht"; parameter.unit="%"; parameter.ranges={0.0f,100.0f,100.0f}; break;
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        if(index==kGalaxy)return fGalaxy;
        switch (index)
        {
        case kTem:     return fTem;
        case kVanaf:   return fVanaf;
        case kSterkte: return fSterkte;
        case kMono:    return fMono;
        case kOut:     return fOutDb;
        case kDice: return fDice; case kPreset: return fPreset;
        case kOn: return fOn; case kKracht: return fKr;
        }
        return 0.0f;
    }

    void setParameterValue(uint32_t index, float value) override
    {
        if(index==kGalaxy){fGalaxy=value;galaxy.setPos((int)value);return;}
        if (index == kPreset) { fPreset = value; applyPreset((int)value); return; }
        switch (index)
        {
        case kTem:     fTem = value; break;
        case kVanaf:   fVanaf = value; break;
        case kSterkte: fSterkte = value; break;
        case kMono:    fMono = value; break;
        case kOut:     fOutDb = value; break;case kDice: if(value>0.5f)rollDice(); fDice=0.f; break;
        case kOn: fOn = value; break; case kKracht: fKr = value; break;
        }
        recalc();
    }

    void activate() override
    {
        fLp1L = fLp2L = fLp1R = fLp2R = fEnv = 0.0f;
    }

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

        const float a    = fAlpha;
        const float atk  = fEnvAtk;
        const float rel  = fEnvRel;
        const float thr  = fThr;
        const float kk   = fKneeK;
        const float mono = fMono / 100.0f;
        const float sg   = fSubGain;
        const float g    = fOutGain;

        float lp1l = fLp1L, lp2l = fLp2L;
        float lp1r = fLp1R, lp2r = fLp2R;
        float env  = fEnv;

        for (uint32_t i = 0; i < frames; ++i)
        {
            const float l = inL[i], r = inR[i];

            // sub-splits: tweepools LP per kanaal
            lp1l += a * (l - lp1l);
            lp2l += a * (lp1l - lp2l);
            lp1r += a * (r - lp1r);
            lp2r += a * (lp1r - lp2r);
            const float subL = lp2l, subR = lp2r;
            const float topL = l - subL, topR = r - subR;

            // enveloppe van de sub → hoe wild is-ie nu
            const float lvl = std::fabs(subL) + std::fabs(subR);
            env += (lvl > env ? atk : rel) * (lvl - env);

            // tem-gain: alles boven thr wordt zacht teruggetrokken
            float gain = 1.0f;
            if (env > thr && env > 1e-6f)
                gain = std::pow(thr / env, kk);

            // mono onder de split: sub mag niet breed worden
            const float mSub = (subL + subR) * 0.5f;
            const float sL = subL + (mSub - subL) * mono;
            const float sR = subR + (mSub - subR) * mono;

            outL[i] = (topL + sL * gain * sg) * g;
            outR[i] = (topR + sR * gain * sg) * g;
        }

        fLp1L = lp1l; fLp2L = lp2l;
        fLp1R = lp1r; fLp2R = lp2r;
        fEnv  = env;
    }

private:
    void rollDice(){
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kTem,20.f+((fRnd>>8)%100)/100.f*80.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kVanaf,60.f+((fRnd>>8)%100)/100.f*80.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kSterkte,-6.f+((fRnd>>8)%100)/100.f*8.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kMono,((fRnd>>8)%100)/100.f*100.f);
    }
    float fDice = 0.f;
    unsigned fRnd = 998877;

    float fTem, fVanaf, fSterkte, fMono, fOutDb;
    float fLp1L, fLp2L, fLp1R, fLp2R, fEnv;
    float fAlpha, fEnvAtk, fEnvRel, fThr, fKneeK, fSubGain, fOutGain;

    float fPreset = 0.0f;

    void applyPreset(int i)
    {
        if (i < 0 || i >= kNumPresets) return;
        const float* p = kPresetParams[i];
        setParameterValue(kTem, p[0]);
        setParameterValue(kVanaf, p[1]);
        setParameterValue(kSterkte, p[2]);
        setParameterValue(kMono, p[3]);
        setParameterValue(kOut, p[4]);
        recalc();
    }

    void recalc()
    {
        const float sr = (float)getSampleRate();
        const float pi = 3.14159265358979f;
        fAlpha   = std::exp(-2.0f * pi * fVanaf / sr);
        fEnvAtk  = 1.0f - std::exp(-1.0f / (sr * 0.005f));  // ~5 ms
        fEnvRel  = 1.0f - std::exp(-1.0f / (sr * 0.150f));  // ~150 ms
        // TEM: hoger = lagere drempel + sterkere terugtrekking
        fThr     = 0.35f - 0.20f * (fTem / 100.0f);         // ~-9..-15 dBFS
        fKneeK   = 0.75f * (fTem / 100.0f);
        fSubGain = std::pow(10.0f, fSterkte / 20.0f);
        fOutGain = std::pow(10.0f, fOutDb / 20.0f);
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SubTamerPlugin)
    float fOn=1.f,fKr=100.f,fGalaxy=9.f;
    Galaxy galaxy;
};

Plugin* createPlugin() { return new SubTamerPlugin(); }

END_NAMESPACE_DISTRHO
