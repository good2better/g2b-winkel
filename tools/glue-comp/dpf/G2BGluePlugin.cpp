/*
 * G2B Glue-Comp — stereo bus-compressor in de stijl van de klassieke
 * "glue" compressors (SSL-bus-achtig), in G2B-knoppen-stijl.
 *
 * DSP: feedforward RMS-detector → log-domain gain computer →
 * smoothed attack/release → make-up gain + KRACHT dry/wet-mix.
 * 27 DIMENTIES = karakter: van transparant (0) naar punch/limit (26).
 */

#include "DistrhoPlugin.hpp"

START_NAMESPACE_DISTRHO

class G2BGluePlugin : public Plugin {
public:
    G2BGluePlugin() : Plugin(kParamCount, 0, 0) {}

    const char* getLabel() const override { return "G2B Glue-Comp"; }
    const char* getDescription() const override {
        return "Bus-compressor die je mix lijmt — G2B";
    }
    const char* getMaker() const override { return "Good2Better"; }
    const char* getHomePage() const override {
        return "https://good2better.github.io/g2b-winkel/";
    }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(1, 0, 0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','G','C'); }

    enum Params {
        kOn,        // 0  AAN/UIT (bypass)
        kKracht,    // 1  dry/wet 0-100
        kThresh,    // 2  drempel dB (-40..0)
        kRatio,     // 3  1..20
        kAttack,    // 4  ms 0.1..100
        kRelease,   // 5  ms 10..1000
        kMakeup,    // 6  dB 0..24 (0 = auto)
        kScHp,      // 7  sidechain highpass Hz 20..300
        kDimensie,  // 8  0..26 karakter
        kSkin,      // 9  0..7
        kPreset,    // 10 0..15 presets
        kParamCount
    };

protected:
    void initParameter(uint32_t index, Parameter& p) override {
        switch (index) {
        case kOn:      p.name="AAN/UIT";   p.symbol="on";   p.ranges.min=0; p.ranges.max=1;  p.ranges.def=1; p.hints=kParameterIsBoolean; break;
        case kKracht:  p.name="KRACHT";    p.symbol="mix";  p.ranges.min=0; p.ranges.max=100;p.ranges.def=100; p.unit="%"; break;
        case kThresh:  p.name="DREMPEL";   p.symbol="thr";  p.ranges.min=-40;p.ranges.max=0; p.ranges.def=-18; p.unit="dB"; break;
        case kRatio:   p.name="RATIO";     p.symbol="rat";  p.ranges.min=1; p.ranges.max=20; p.ranges.def=4; p.unit=":1"; break;
        case kAttack:  p.name="ATTACK";    p.symbol="atk";  p.ranges.min=0.1f;p.ranges.max=100;p.ranges.def=10; p.unit="ms"; break;
        case kRelease: p.name="RELEASE";   p.symbol="rel";  p.ranges.min=10;p.ranges.max=1000;p.ranges.def=150; p.unit="ms"; break;
        case kMakeup:  p.name="GAIN";      p.symbol="gain"; p.ranges.min=0; p.ranges.max=24; p.ranges.def=0; p.unit="dB"; break;
        case kScHp:    p.name="SC-LOWCUT"; p.symbol="schp"; p.ranges.min=20;p.ranges.max=300;p.ranges.def=60; p.unit="Hz"; break;
        case kDimensie:p.name="DIMENTIES"; p.symbol="dim";  p.ranges.min=0; p.ranges.max=26; p.ranges.def=8; p.hints=kParameterIsInteger; break;
        case kSkin:    p.name="SKIN";      p.symbol="skin"; p.ranges.min=0; p.ranges.max=7;  p.ranges.def=2; p.hints=kParameterIsInteger; break;
        case kPreset:  p.name="PRESET";    p.symbol="pre";  p.ranges.min=0; p.ranges.max=15; p.ranges.def=0; p.hints=kParameterIsInteger; break;
        }
    }

    float getParameterValue(uint32_t index) const override {
        switch (index) {
        case kOn: return fOn; case kKracht: return fMix; case kThresh: return fThr;
        case kRatio: return fRatio; case kAttack: return fAtk; case kRelease: return fRel;
        case kMakeup: return fMake; case kScHp: return fHp; case kDimensie: return fDim;
        case kSkin: return fSkin; case kPreset: return fPre;
        }
        return 0;
    }

    void setParameterValue(uint32_t index, float v) override {
        switch (index) {
        case kOn: fOn=v>0.5f?1:0; break;
        case kKracht: fMix=v; break;
        case kThresh: fThr=v; break;
        case kRatio: fRatio=v; break;
        case kAttack: fAtk=v; break;
        case kRelease: fRel=v; break;
        case kMakeup: fMake=v; break;
        case kScHp: fHp=v; break;
        case kDimensie: fDim=v; break;
        case kSkin: fSkin=v; break;
        case kPreset: if((int)v!=(int)fPre)applyPreset((int)v); fPre=v; break;
        }
    }

    void activate() override {
        fEnv=0; fHpL=fHpR=0; fGr=0;
        fSrd=getSampleRate(); if (fSrd<=0) fSrd=44100;
    }

    void run(const float** in, float** out, uint32_t n) override {
        const float* il=in[0]; const float* ir=in[1];
        float* ol=out[0]; float* orr=out[1];
        if (fSrd<=0) fSrd=getSampleRate();
        const float dim=fDim/26.0f;
        // karakter: dimensie stuurt knee + timing + saturatie
        const float knee=2.0f+dim*10.0f;          // knee-breedte dB
        const float satDrive=1.0f+dim*0.6f;        // extra drive
        const float atkC=std::exp(-1.0f/(fSrd*fAtk*0.001f));
        const float relC=std::exp(-1.0f/(fSrd*fRel*0.001f));
        // sidechain highpass (1-pole)
        const float hpC=std::exp(-2.0f*M_PI*fHp/fSrd);
        // auto-makeup: compenseer gemiddelde GR als GAIN==0
        const float autoMk=(fMake<=0.01f)?-fThr*(1.0f-1.0f/fRatio)*0.5f*dim*0.5f-(-fThr)*0.0f:0;
        const float mkDb=(fMake<=0.01f)?(-fThr*(1.0f-1.0f/fRatio)*0.55f):fMake;
        const float mk=std::pow(10.0f,mkDb/20.0f);
        const float wet=fMix/100.0f;
        for (uint32_t i=0;i<n;i++) {
            float l=il[i], r=ir[i];
            if (fOn<0.5f) { ol[i]=l; orr[i]=r; continue; }
            // sidechain detect (na HP)
            float dl=l-fHpL*hpC; fHpL=(fHpL*hpC)+dl*0.0f+ (l* (1.0f-hpC)); // simplified: hp state
            float dr=r-fHpR*hpC; fHpR=(fHpR*hpC)+(r*(1.0f-hpC));
            float det=0.5f*(dl*dl+dr*dr);
            fEnv=det>fEnv? atkC*fEnv+(1.0f-atkC)*det : relC*fEnv+(1.0f-relC)*det;
            float level=std::sqrt(fEnv)+1e-9f;
            float lvDb=20.0f*std::log10(level);
            // gain computer met knee
            float over=lvDb-fThr;
            float grDb=0.0f;
            if (over>knee/2.0f) grDb=over*(1.0f/fRatio-1.0f);
            else if (over>-knee/2.0f) {
                float t0=over+knee/2.0f;
                grDb=(1.0f/fRatio-1.0f)*t0*t0/(2.0f*knee);
            }
            float g=std::pow(10.0f,grDb/20.0f)*mk;
            ol[i]=(l*(1.0f-wet)+std::tanh(l*g*satDrive)*wet)*0.92f;
            orr[i]=(r*(1.0f-wet)+std::tanh(r*g*satDrive)*wet)*0.92f;
        }
        fGr=20.0f*std::log10(std::pow(10.0f,fEnv>0?0:0));
        (void)fGr;
    }

    // voor UI: huidige gain-reduction in dB (geschat uit envelope)
    float getGrDb() const {
        float level=std::sqrt(fEnv)+1e-9f;
        float lvDb=20.0f*std::log10(level);
        float over=lvDb-fThr;
        return over>0? over*(1.0f/fRatio-1.0f) : 0.0f;
    }
    int getDim() const { return (int)fDim; }
    int getSkin() const { return (int)fSkin; }
    int getOn() const { return (int)fOn; }

private:
    void applyPreset(int p){static const float PR[16][6]={{-12,2,15,200,3,80},{-18,4,10,150,5,60},{-25,8,5,100,8,40},{-8,1.5f,30,400,2,120},{-15,3,20,250,4,100},{-22,6,8,120,7,50},{-10,2.5f,25,300,3,150},{-20,5,12,180,6,70},{-30,10,3,80,10,30},{-14,3.5f,18,220,4,90},{-16,4,15,160,5,60},{-24,7,6,110,8,45},{-9,2,35,500,2,140},{-19,4.5f,11,190,6,55},{-28,9,4,90,9,35},{-13,3,22,280,4,110}};const float*q=PR[p&15];fThr=q[0];fRatio=q[1];fAtk=q[2];fRel=q[3];fMake=q[4];fHp=q[5];}
    float fOn=1,fMix=100,fThr=-18,fRatio=4,fAtk=10,fRel=150,fMake=0,fHp=60,fDim=8,fSkin=2,fPre=0;
    float fEnv=0,fHpL=0,fHpR=0,fGr=0,fSrd=44100;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BGluePlugin)
};

Plugin* createPlugin() { return new G2BGluePlugin(); }

END_NAMESPACE_DISTRHO
