// G2B Echo-Kamer — stereo delay: tijd/feedback/pingpong/ducking/karakter-dimensies.
// Vaste knoppen: AAN/UIT, KRACHT (mix), DIMENTIES 27 (karakter), SKIN, XY (tijd x feedback).
#include "DistrhoPluginInfo.h"
#include "DistrhoPlugin.hpp"
#include <cmath>
#include <cstring>

START_NAMESPACE_DISTRHO

static const uint32_t kMaxDelay = 96000; // 2s @48k

class G2BEchoPlugin : public Plugin {
public:
    enum Param {
        kOn, kKracht, kTijd, kFeedback, kPingpong, kDuck,
        kLp, kHp, kDimensie, kSkin, kXY, kParamCount
    };
    G2BEchoPlugin() : Plugin(kParamCount, 0, 0) {
        std::memset(bufL_, 0, sizeof(bufL_)); std::memset(bufR_, 0, sizeof(bufR_));
    }
    const char* getLabel() const override { return "G2B Echo-Kamer"; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "G2B-EULA"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return DISTRHO_PLUGIN_UNIQUE_ID; }

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints = kParameterIsAutomatable; p.ranges.min=0; p.ranges.max=100; p.ranges.def=50;
        switch(i){
        case kOn: p.name="AAN/UIT"; p.symbol="on"; p.hints|=kParameterIsBoolean; p.ranges.max=1; p.ranges.def=1; break;
        case kKracht: p.name="KRACHT"; p.symbol="mix"; p.unit="%"; break;
        case kTijd: p.name="TIJD"; p.symbol="tijd"; p.unit="ms"; p.ranges.min=20; p.ranges.max=1000; p.ranges.def=375; break;
        case kFeedback: p.name="FEEDBACK"; p.symbol="fb"; p.unit="%"; p.ranges.max=95; p.ranges.def=40; break;
        case kPingpong: p.name="PINGPONG"; p.symbol="pp"; p.hints|=kParameterIsBoolean; p.ranges.max=1; break;
        case kDuck: p.name="DUCKING"; p.symbol="duck"; p.unit="%"; break;
        case kLp: p.name="LOWPASS"; p.symbol="lp"; p.unit="Hz"; p.ranges.min=1000; p.ranges.max=18000; p.ranges.def=12000; break;
        case kHp: p.name="HIGHPASS"; p.symbol="hp"; p.unit="Hz"; p.ranges.min=20; p.ranges.max=2000; p.ranges.def=120; break;
        case kDimensie: p.name="DIMENTIES"; p.symbol="dim"; p.hints|=kParameterIsInteger; p.ranges.max=26; p.ranges.def=13; break;
        case kSkin: p.name="SKIN"; p.symbol="skin"; p.hints|=kParameterIsInteger; p.ranges.max=7; break;
        case kXY: p.name="XY"; p.symbol="xy"; p.hints|=kParameterIsInteger; p.ranges.max=1000; break;
        }
    }
    float getParameterValue(uint32_t i) const override { return v_[i]; }
    void setParameterValue(uint32_t i, float v) override {
        v_[i]=v;
        if(i==kXY){ // XY: X=tijd(0..1->20..1000ms), Y=feedback
            float x=std::fmod(v,32.0f)/31.0f, y=std::floor(v/32.0f)/31.0f;
            v_[kTijd]=20+x*980; v_[kFeedback]=y*95;
        }
    }
protected:
    void run(const float** in, float** out, uint32_t n) override {
        if(v_[kOn]<0.5f){ if(in[0]!=out[0])std::memcpy(out[0],in[0],n*4); if(in[1]!=out[1])std::memcpy(out[1],in[1],n*4); return; }
        const float sr=getSampleRate();
        const uint32_t dsm=(uint32_t)(v_[kTijd]*0.001f*sr)%kMaxDelay;
        const float fb=v_[kFeedback]/100.0f, mix=v_[kKracht]/100.0f;
        const bool pp=v_[kPingpong]>0.5f;
        const int dim=(int)v_[kDimensie];
        // karakter: dimensie moduleert fb-filters + saturatie + wow (licht LFO op delaytijd)
        const float dimN=dim/26.0f;
        const float sat=dimN*0.6f;                 // hogere dim = meer tape-crush
        const float wow=dimN*0.004f;               // hogere dim = meer modulatie
        const float duck=v_[kDuck]/100.0f;
        float lpA=std::exp(-2.f*3.14159f*v_[kLp]/sr), hpA=std::exp(-2.f*3.14159f*v_[kHp]/sr);
        float lpL=lpS_,lpR=lpS2_,hpL=hpS_,hpR=hpS2_;
        for(uint32_t i=0;i<n;++i){
            float modL=1.f+wow*std::sin(ph_), modR=1.f+wow*std::sin(ph_*0.9f+1.3f);
            ph_+=2.f*3.14159f*0.4f/sr; if(ph_>6.283f)ph_-=6.283f;
            uint32_t rL=(w_+kMaxDelay-(uint32_t)(dsm*modL))%kMaxDelay;
            uint32_t rR=(w_+kMaxDelay-(uint32_t)(dsm*modR*1.01f))%kMaxDelay;
            float dL=bufL_[rL], dR=bufR_[rR];
            // duck: demper op echo terwijl input hard is
            float env=in[0][i]*in[0][i]+in[1][i]*in[1][i];
            duckEnv_=duckEnv_*0.999f+env*0.001f;
            float dg=1.f-duck*std::min(1.f,duckEnv_*40.f);
            dL*=dg; dR*=dg;
            // filters in de echo-lijn
            lpL+=(dL-lpL)*(1-lpA); lpR+=(dR-lpR)*(1-lpA);
            hpL+=(lpL-hpL)*(1-hpA); hpR+=(lpR-hpR)*(1-hpA);
            float fL=lpL-hpL*0.9f, fR=lpR-hpR*0.9f;
            if(sat>0.01f){ fL=std::tanh(fL*(1+sat*3))/(1+sat*2); fR=std::tanh(fR*(1+sat*3))/(1+sat*2); }
            bufL_[w_]=in[0][i]+(pp?fR:fL)*fb;
            bufR_[w_]=in[1][i]+(pp?fL:fR)*fb;
            w_=(w_+1)%kMaxDelay;
            out[0][i]=in[0][i]*(1-mix*0.5f)+dL*mix;
            out[1][i]=in[1][i]*(1-mix*0.5f)+dR*mix;
        }
        lpS_=lpL; lpS2_=lpR; hpS_=hpL; hpS2_=hpR;
    }
private:
    float v_[kParamCount]={};
    float bufL_[kMaxDelay], bufR_[kMaxDelay]; uint32_t w_=0;
    float lpS_=0,lpS2_=0,hpS_=0,hpS2_=0,ph_=0,duckEnv_=0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BEchoPlugin)
};
Plugin* createPlugin(){ return new G2BEchoPlugin(); }
END_NAMESPACE_DISTRHO
