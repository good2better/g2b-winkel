/*
 * G2B Glans — air + stereo width above a split frequency.
 * DSP: one-pole high-shelf "air" on the mid path + side-widening only
 * above the split point (sub/bass stays mono and powerful).
 * Same math as the JSFX g2b_glans used by G2B Engineer.
 */

#include "DistrhoPlugin.hpp"
#include <cstring>

START_NAMESPACE_DISTRHO

static const char* kPresetNames[17] = {"G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"};
static const int kNumPresets = 17;
static const float kPresetParams[17][5] = {
    { 5, 1.3, 400, 9000, 0 },
    { 6, 1.15, 600, 8500, 0 },
    { 4, 1, 800, 8000, 0 },
    { 6.5, 1.5, 400, 10000, 0 },
    { 5.5, 1.25, 500, 9500, 0 },
    { 5, 1.4, 400, 9000, 0 },
    { 6, 1.2, 500, 8000, 0 },
    { 4, 1.1, 700, 9000, 0 },
    { 5, 1.6, 400, 10000, 0 },
    { 7, 1.4, 500, 11000, 0 },
    { 5, 1.3, 450, 9000, 0 },
    { 6, 1.2, 600, 9500, 0 },
    { 4, 1.0, 800, 8500, 0 },
    { 5, 1.5, 400, 9000, 0 },
    { 6, 1.2, 500, 9500, 0 },
    { 7, 1.4, 400, 10000, 0 },
    { 5, 1.3, 500, 9500, 0 },
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

class GlansPlugin : public Plugin
{
public:
    enum Parameters {
        kAir = 0,      // AIR dB
        kWidth,        // BREEDTE x
        kSplit,        // breedte vanaf Hz
        kAirFreq,      // air vanaf Hz
        kOut,          // output dB
        kGalaxy, kDice, kPreset,
        kOn, kKracht,
        kParameterCount
    };

    GlansPlugin()
        : Plugin(kParameterCount, 0, 0),
          fAirDb(2.5f), fWidth(1.15f), fSplit(1500.0f),
          fAirFreq(9500.0f), fOutDb(0.0f),
          fLpA{0.0f,0.0f}, fLpB{0.0f,0.0f}, fLpS(0.0f),
          fAlphaAir(0.0f), fAlphaSplit(0.0f), fAirK(0.0f), fOutGain(1.0f)
    {
        recalc();
    }

protected:
    const char* getLabel() const override { return "G2B Glans"; }
    const char* getDescription() const override
    {
        return "Glans aan je mix: air-boost en stereo-breedte ALLEEN boven "
               "een splitpunt — sub en midden blijven strak en gecentreerd.";
    }
    const char* getMaker() const override { return "G2B (good2better)"; }
    const char* getHomePage() const override { return "https://github.com/good2better"; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','G','l'); }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        if(index==kGalaxy){parameter.name="GALAXY";parameter.symbol="galaxy";parameter.ranges.min=0;parameter.ranges.max=26;parameter.ranges.def=9;parameter.hints|=kParameterIsInteger;return;}
        parameter.hints = kParameterIsAutomatable;
        switch (index)
        {
        case kAir:
            parameter.name = "Air";
            parameter.shortName = "Air";
            parameter.symbol = "air";
            parameter.unit = "dB";
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 10.0f;
            parameter.ranges.def = 2.5f;
            break;
        case kWidth:
            parameter.name = "Breedte";
            parameter.shortName = "Breedte";
            parameter.symbol = "width";
            parameter.unit = "x";
            parameter.ranges.min = 0.5f;
            parameter.ranges.max = 2.0f;
            parameter.ranges.def = 1.15f;
            break;
        case kSplit:
            parameter.name = "Breedte vanaf";
            parameter.shortName = "Vanaf";
            parameter.symbol = "split";
            parameter.unit = "Hz";
            parameter.ranges.min = 400.0f;
            parameter.ranges.max = 4000.0f;
            parameter.ranges.def = 1500.0f;
            break;
        case kAirFreq:
            parameter.name = "Air vanaf";
            parameter.shortName = "Air vanaf";
            parameter.symbol = "airfreq";
            parameter.unit = "Hz";
            parameter.ranges.min = 6000.0f;
            parameter.ranges.max = 14000.0f;
            parameter.ranges.def = 9500.0f;
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
        case kAir:     return fAirDb;
        case kWidth:   return fWidth;
        case kSplit:   return fSplit;
        case kAirFreq: return fAirFreq;
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
        case kAir:     fAirDb = value; break;
        case kWidth:   fWidth = value; break;
        case kSplit:   fSplit = value; break;
        case kAirFreq: fAirFreq = value; break;
        case kOut:     fOutDb = value; break;case kDice: if(value>0.5f)rollDice(); fDice=0.f; break;
        case kOn: fOn = value; break; case kKracht: fKr = value; break;
        }
        recalc();
    }

    void activate() override
    {
        fLpA[0] = fLpA[1] = fLpB[0] = fLpB[1] = fLpS = 0.0f;
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

        const float aAir = fAlphaAir;
        const float aHp  = fAlphaSplit;
        const float airK = fAirK;
        const float w    = fWidth;
        const float g    = fOutGain;

        float lpa0 = fLpA[0], lpa1 = fLpA[1];
        float lpb0 = fLpB[0], lpb1 = fLpB[1];
        float lps  = fLpS;

        for (uint32_t i = 0; i < frames; ++i)
        {
            float l = inL[i], r = inR[i];

            // air-shelf: een-pools hoogdoorlaat op elk kanaal
            lpa0 += aAir * (l - lpa0);
            lpa1 += aAir * (r - lpa1);
            l += (l - lpa0) * airK;
            r += (r - lpa1) * airK;

            // mid/side — breedte alleen boven de split
            const float mid  = (l + r) * 0.5f;
            const float side = (l - r) * 0.5f;
            lps += aHp * (side - lps);
            const float sideHi = side - lps;
            const float side2 = (side - sideHi) + sideHi * w;

            outL[i] = (mid + side2) * g;
            outR[i] = (mid - side2) * g;
        }

        fLpA[0] = lpa0; fLpA[1] = lpa1;
        fLpB[0] = lpb0; fLpB[1] = lpb1;
        fLpS = lps;
    }

private:
    void rollDice(){
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kAir,((fRnd>>8)%100)/100.f*8.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kWidth,0.8f+((fRnd>>8)%100)/100.f*1.0f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kSplit,800.f+((fRnd>>8)%100)/100.f*2400.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kAirFreq,7000.f+((fRnd>>8)%100)/100.f*6000.f);
    }
    float fDice = 0.f;
    unsigned fRnd = 998877;

    float fAirDb, fWidth, fSplit, fAirFreq, fOutDb;
    float fLpA[2], fLpB[2], fLpS;
    float fAlphaAir, fAlphaSplit, fAirK, fOutGain;

    float fPreset = 0.0f;

    void applyPreset(int i)
    {
        if (i < 0 || i >= kNumPresets) return;
        const float* p = kPresetParams[i];
        setParameterValue(kAir, p[0]);
        setParameterValue(kWidth, p[1]);
        setParameterValue(kSplit, p[2]);
        setParameterValue(kAirFreq, p[3]);
        setParameterValue(kOut, p[4]);
        recalc();
    }

    void recalc()
    {
        const float sr = (float)getSampleRate();
        const float pi = 3.14159265358979f;
        fAirK      = std::pow(10.0f, fAirDb / 20.0f) - 1.0f;
        fAlphaAir  = std::exp(-2.0f * pi * fAirFreq / sr);
        fAlphaSplit= std::exp(-2.0f * pi * fSplit / sr);
        fOutGain   = std::pow(10.0f, fOutDb / 20.0f);
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlansPlugin)
    float fOn=1.f,fKr=100.f,fGalaxy=9.f;
    Galaxy galaxy;
};

Plugin* createPlugin() { return new GlansPlugin(); }

END_NAMESPACE_DISTRHO
