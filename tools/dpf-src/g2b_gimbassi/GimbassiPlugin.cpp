/*
 * G2B GIM BASSI PRO (VST) — haalt bas + kick uit een loop/track, live.
 * Browser-versie maar dan realtime in je DAW, met pro-upgrades:
 *   FILT  — cutoff van de lage band (bas/kick-gebied)   40..200 Hz
 *   GATE  — hoeveel "rest" van de lage band doorgelaten wordt (0=alleen slagen)
 *   DUCK  — hoe hard de bas onder de kick wegduikt
 *   QUANT — slag-gate snapt op het 16e-raster van de DAW (host-tempo)
 *   MODE  — MIX (L=R extractie) of SPLIT (L=bas+leak, R=kick)
 *   MIX   — dry/wet van de extractie
 */

#include "DistrhoPlugin.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>

START_NAMESPACE_DISTRHO

class GimbassiPlugin : public Plugin {
public:
    enum Params { kFilt, kGate, kDuck, kQuant, kMode, kMix, kOn, kMeter,
                  kNumParams };
    GimbassiPlugin() : Plugin(kNumParams, 0, 0) {}
protected:
    const char* getLabel() const override { return "G2B Gim Bassi PRO"; }
    const char* getDescription() const override {
        return "Haalt bas (sub/808) + kick uit een loop — live, gequantized op je DAW-tempo. SPLIT: L=bas, R=kick."; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','G','B'); }

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints=kParameterIsAutomatable;
        switch(i){
        case kFilt:  p.name="FILT";   p.symbol="filt";  p.ranges.min=40.f;p.ranges.max=200.f;p.ranges.def=120.f;p.unit="Hz";break;
        case kGate:  p.name="GATE";   p.symbol="gate";  p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=15.f;p.unit="%";break;
        case kDuck:  p.name="DUCK";   p.symbol="duck";  p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=85.f;p.unit="%";break;
        case kQuant: p.name="QUANT";  p.symbol="quant"; p.hints|=kParameterIsBoolean;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=1.f;break;
        case kMode:  p.name="MODE";   p.symbol="mode";  p.hints|=kParameterIsInteger;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.f;break;
        case kMix:   p.name="MIX";    p.symbol="mix";   p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=100.f;p.unit="%";break;
        case kOn:    p.name="Aan";    p.symbol="aan";   p.hints|=kParameterIsBoolean;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=1.f;break;
        case kMeter: p.name="METER";  p.symbol="meter"; p.hints=kParameterIsOutput;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.f;break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){case kFilt:return fFilt;case kGate:return fGate;case kDuck:return fDuck;
                  case kQuant:return fQuant;case kMode:return fMode;case kMix:return fMix;
                  case kOn:return fOn;case kMeter:return fMeter;}return 0.f;
    }
    void setParameterValue(uint32_t i, float v) override {
        switch(i){
        case kFilt:fFilt=v;break;case kGate:fGate=v;break;case kDuck:fDuck=v;break;
        case kQuant:fQuant=v;break;case kMode:fMode=v;break;case kMix:fMix=v;break;
        case kOn:fOn=v;break;}
    }

    void activate() override {
        fLp=0.f;fEnv=0.f;fMean=0.f;fKEnv=0.f;fDEnv=1.f;
        fSinceOn=1e9f;fGateIn=-1.f;fAbs=0.f;fMeter=0.f;fHold=0.f;
        fStep16=0.f;fBpm=120.0;
        fDelN=(int)(0.025f*getSampleRate());if(fDelN<1)fDelN=1;
        fDel.assign(fDelN,0.f);fDelI=0;
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override {
        const float* inL=inputs[0];const float* inR=inputs[1];
        float* outL=outputs[0];float* outR=outputs[1];
        if(fOn<0.5f){
            if(inL!=outL)std::memcpy(outL,inL,frames*sizeof(float));
            if(inR!=outR)std::memcpy(outR,inR,frames*sizeof(float));
            fMeter=0.f;return;
        }
        const float sr=getSampleRate();
        // host-tempo voor QUANT (16e-stap in frames)
        const TimePosition&tp=getTimePosition();
        if(tp.bbt.valid&&tp.bbt.beatsPerMinute>1.0){
            fBpm=tp.bbt.beatsPerMinute;fStep16=sr*60.0/fBpm/4.0;
            // pos-in-16e stappen aan het begin van dit blok
            if(tp.playing)fPos16=(tp.bbt.bar-1)*tp.bbt.beatsPerBar*4.0+(tp.bbt.beat-1)*4.0+(double)tp.bbt.tick/tp.bbt.ticksPerBeat*4.0;
        }
        const float lpA=1.f-expf(-2.f*M_PI*fFilt/sr);
        const float att=1.f-expf(-1.f/(0.005f*sr));   // onset-env attack 5ms
        const float rel=expf(-1.f/(0.100f*sr));       // onset-env release 100ms
        const float mRel=1.f-expf(-1.f/(0.300f*sr));  // adaptief niveau 300ms
        const float kRel=expf(-1.f/(0.070f*sr));      // kick-gate release 70ms
        const float dRel=expf(-1.f/(0.220f*sr));      // duck herstel 220ms
        const float holdF=0.280f*sr;                  // gate houdt 280ms (zoals browser)
        const float refrac=0.120f*sr;                 // 120ms dubbele-slag-blokkade
        // 25ms vertraagde env (ring fDel, opgezet in activate) voor rise-detectie
        const float leak=fGate/100.f, duck=fDuck/100.f, mix=fMix/100.f;
        const bool split=fMode>0.5f;
        float mAcc=0.f;
        for(uint32_t i=0;i<frames;i++){
            const float mono=(inL[i]+inR[i])*0.5f;
            fLp+=lpA*(mono-fLp);
            const float low=fLp;
            // onset-env volgt |low| (browser-methode)
            const float a=fabsf(low);
            if(a>fEnv)fEnv+=att*(a-fEnv);else fEnv*=rel;
            fMean+=mRel*(fEnv-fMean);
            const float delv=fDel[fDelI];fDel[fDelI]=fEnv;fDelI=(fDelI+1)%fDelN;
            fSinceOn+=1.f;
            // onset = duidelijke stijging t.o.v. 25ms terug + boven ruisvloer
            const float floor_=fMean*0.8f>0.02f?fMean*0.8f:0.02f;
            if(fEnv>delv*1.4f&&fEnv>floor_&&fSinceOn>refrac){
                fSinceOn=0.f;
                fGateIn=0.f;
                if(fQuant>0.5f&&fStep16>0.f){
                    // snap de gate op de volgende 16e-grens
                    const float p=fPos16;
                    const float nxt=ceilf(p+1e-6f);
                    fGateIn=(nxt-p)*fStep16;
                }
            }
            if(fGateIn>=0.f){fGateIn-=1.f;
                if(fGateIn<0.f){fKEnv=1.f;fHold=holdF;fDEnv=1.f-duck;}}
            // kick-gate: houd dan release
            if(fHold>0.f)fHold-=1.f;else fKEnv*=kRel;
            // duck herstelt naar 1
            fDEnv+=(1.f-fDEnv)*(1.f-dRel)*(fHold<=0.f?1.f:0.f)+0.f;
            const float e=fKEnv;
            const float kickSig=low*e;
            const float basSig=low*fDEnv*(1.f-e);
            const float leakSig=low*leak*(1.f-e);
            const float extract=kickSig+basSig+leakSig;
            float L,R;
            if(split){L=basSig+leakSig;R=kickSig;}
            else{L=R=extract;}
            outL[i]=inL[i]*(1.f-mix)+L*mix;
            outR[i]=inR[i]*(1.f-mix)+R*mix;
            mAcc+=e;
            fAbs+=1.f;if(fStep16>0)fPos16+=1.f/fStep16;
        }
        fMeter=mAcc/frames;
        fMeter=fMeter>1.f?1.f:fMeter;
    }

private:
    float fFilt=120.f,fGate=15.f,fDuck=85.f,fQuant=1.f,fMode=0.f,fMix=100.f,fOn=1.f;
    float fLp=0.f,fEnv=0.f,fMean=0.f,fKEnv=0.f,fDEnv=1.f;
    float fSinceOn=1e9f,fGateIn=-1.f,fAbs=0.f,fMeter=0.f,fHold=0.f;
    double fBpm=120.0,fStep16=0.f,fPos16=0.f;
    std::vector<float> fDel;int fDelN=0,fDelI=0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GimbassiPlugin)
};

Plugin* createPlugin() { return new GimbassiPlugin(); }

END_NAMESPACE_DISTRHO
