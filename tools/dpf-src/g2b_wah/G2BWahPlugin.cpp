// G2B Filter-Wah — auto-wah: LFO-veegt resonant bandpass + envelope + 27 dimensies.
#include "DistrhoPlugin.hpp"
START_NAMESPACE_DISTRHO
class G2BWahPlugin:public Plugin{
public:enum Params{kOn,kKracht,kRate,kDepth,kRes,kSens,kFreq,kDimensie,kSkin,kParamCount};
    G2BWahPlugin():Plugin(kParamCount,0,0){}
    const char*getLabel()const override{return "G2B Filter-Wah";}
    const char*getDescription()const override{return "Auto-wah met LFO-veeg + envelope + 27 dimensies";}
    const char*getMaker()const override{return "G2B";}
    const char*getLicense()const override{return "ISC";}
    uint32_t getVersion()const override{return d_version(1,0,0);}
    int64_t getUniqueId()const override{return d_cconst('G','2','F','w');}
protected:
    void initParameter(uint32_t i,Parameter&p)override{
        p.hints=kParameterIsAutomatable;
        switch(i){case kOn:p.name="Aan";p.symbol="aan";p.ranges.def=1;p.ranges.min=0;p.ranges.max=1;p.hints|=kParameterIsBoolean;break;
        case kKracht:p.name="Kracht";p.symbol="kracht";p.unit="%";p.ranges.def=100;p.ranges.min=0;p.ranges.max=100;break;
        case kRate:p.name="Rate";p.symbol="rate";p.ranges.def=30;p.ranges.min=0;p.ranges.max=100;break;
        case kDepth:p.name="Depth";p.symbol="depth";p.ranges.def=70;p.ranges.min=0;p.ranges.max=100;break;
        case kRes:p.name="Resonantie";p.symbol="res";p.ranges.def=60;p.ranges.min=0;p.ranges.max=100;break;
        case kSens:p.name="Sens";p.symbol="sens";p.ranges.def=50;p.ranges.min=0;p.ranges.max=100;break;
        case kFreq:p.name="Freq";p.symbol="freq";p.ranges.def=50;p.ranges.min=0;p.ranges.max=100;break;
        case kDimensie:p.name="DIMENTIES";p.symbol="dim";p.ranges.def=0;p.ranges.min=0;p.ranges.max=26;break;
        case kSkin:p.name="Skin";p.symbol="skin";p.ranges.def=4;p.ranges.min=0;p.ranges.max=7;break;}}
    float getParameterValue(uint32_t i)const override{switch(i){case kOn:return fOn;case kKracht:return fKr;case kRate:return fRa;case kDepth:return fDe;case kRes:return fRe;case kSens:return fSe;case kFreq:return fFr;case kDimensie:return fDim;case kSkin:return fSkin;default:return 0;}}
    void setParameterValue(uint32_t i,float v)override{switch(i){case kOn:fOn=v;break;case kKracht:fKr=v;break;case kRate:fRa=v;break;case kDepth:fDe=v;break;case kRes:fRe=v;break;case kSens:fSe=v;break;case kFreq:fFr=v;break;case kDimensie:fDim=v;break;case kSkin:fSkin=v;break;}}
    void activate()override{sr=getSampleRate();phL=0;phR=0.25f;envL=envR=0;b1L=b2L=b1R=b2R=0;}
    void run(const float**in,float**out,uint32_t n)override{
        if(fOn<0.5f){if(in[0]!=out[0])std::memcpy(out[0],in[0],n*sizeof(float));if(in[1]!=out[1])std::memcpy(out[1],in[1],n*sizeof(float));return;}
        int dim=(int)fDim;
        float wet=fKr/100,rate=0.2f+fRa/100*8.0f,depth=fDe/100,res=0.1f+fRe/100*0.85f,sens=fSe/100*0.9f;
        float base=150+fFr/100*2500;
        float stereo=dim>=9?0.5f:0.0f,vel=dim>=18?1.3f:1.0f;
        for(uint32_t i=0;i<n;i++){
            phL+=rate/sr;phR+=rate/sr*vel;
            if(phL>1)phL-=1;if(phR>1)phR-=1;
            // envelope follower
            float eL=std::fabs(in[0][i]),eR=std::fabs(in[1][i]);
            envL=envL*0.999f+eL*0.01f;envR=envR*0.999f+eR*0.01f;
            float swL=0.5f+0.5f*std::sin(6.2832f*phL);
            float swR=0.5f+0.5f*std::sin(6.2832f*phR+stereo*6.2832f);
            float fL=base*(1+depth*(swL*3+envL*sens*8));
            float fR=base*(1+depth*(swR*3+envR*sens*8));
            // resonant bandpass (2-pole state-variable-ish)
            float gL=std::tan(3.14159f*fL/sr),gR=std::tan(3.14159f*fR/sr);
            float dL=1.0f/(1+gL*(gL+1/res)),dR=1.0f/(1+gR*(gR+1/res));
            float hpL=(in[0][i]-b2L-gL*b1L)*dL;
            float bpL=gL*hpL+b1L;b1L=gL*hpL+bpL;
            float lpL=gL*bpL+b2L;b2L=gL*bpL+lpL;
            float hpR=(in[1][i]-b2R-gR*b1R)*dR;
            float bpR=gR*hpR+b1R;b1R=gR*hpR+bpR;
            float lpR=gR*bpR+b2R;b2R=gR*bpR+lpR;
            float wl=bpL*(1+res*2),wr=bpR*(1+res*2);
            out[0][i]=wl*wet+in[0][i]*(1-wet);
            out[1][i]=wr*wet+in[1][i]*(1-wet);}
    }
private:
    float phL,phR,envL,envR,b1L,b2L,b1R,b2R;
    float fOn=1,fKr=100,fRa=30,fDe=70,fRe=60,fSe=50,fFr=50,fDim=0,fSkin=4;double sr=44100;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BWahPlugin)};
END_NAMESPACE_DISTRHO
DISTRHO::Plugin*DISTRHO::createPlugin(){return new DISTRHO::G2BWahPlugin();}
