// G2B Chorus — stereo chorus: LFO-gemoduleerde delay-taps, breedte, dimensies.
#include "DistrhoPlugin.hpp"
START_NAMESPACE_DISTRHO
static const char* dimNames[27]={"CLEAN 1","CLEAN 2","CLEAN 3","CLEAN 4","CLEAN 5","CLEAN 6","CLEAN 7","CLEAN 8","CLEAN 9","WARM 1","WARM 2","WARM 3","WARM 4","WARM 5","WARM 6","WARM 7","WARM 8","WARM 9","THICK 1","THICK 2","THICK 3","THICK 4","THICK 5","THICK 6","THICK 7","THICK 8","THICK 9"};
class G2BChorusPlugin:public Plugin{
public:enum Params{kOn,kKracht,kRate,kDepth,kDelay,kFeed,kWidth,kDimensie,kSkin,kParamCount};
    G2BChorusPlugin():Plugin(kParamCount,0,0){}
    const char*getLabel()const override{return "G2B Chorus";}
    const char*getDescription()const override{return "Stereo chorus met 27 dimensies";}
    const char*getMaker()const override{return "G2B";}
    const char*getLicense()const override{return "ISC";}
    uint32_t getVersion()const override{return d_version(1,0,0);}
    int64_t getUniqueId()const override{return d_cconst('G','2','C','h');}
protected:
    void initParameter(uint32_t i,Parameter&p)override{
        p.hints=kParameterIsAutomatable;
        switch(i){case kOn:p.name="Aan";p.symbol="aan";p.ranges.def=1;p.ranges.min=0;p.ranges.max=1;p.hints|=kParameterIsBoolean;break;
        case kKracht:p.name="Kracht";p.symbol="kracht";p.unit="%";p.ranges.def=70;p.ranges.min=0;p.ranges.max=100;break;
        case kRate:p.name="Rate";p.symbol="rate";p.unit="Hz";p.ranges.def=0.8f;p.ranges.min=0.05f;p.ranges.max=8;break;
        case kDepth:p.name="Depth";p.symbol="depth";p.ranges.def=50;p.ranges.min=0;p.ranges.max=100;break;
        case kDelay:p.name="Delay";p.symbol="delay";p.unit="ms";p.ranges.def=15;p.ranges.min=5;p.ranges.max=40;break;
        case kFeed:p.name="Feed";p.symbol="feed";p.ranges.def=0;p.ranges.min=0;p.ranges.max=95;break;
        case kWidth:p.name="Width";p.symbol="width";p.ranges.def=70;p.ranges.min=0;p.ranges.max=100;break;
        case kDimensie:p.name="Dimenties";p.symbol="dim";p.ranges.def=0;p.ranges.min=0;p.ranges.max=26;break;
        case kSkin:p.name="Skin";p.symbol="skin";p.ranges.def=2;p.ranges.min=0;p.ranges.max=7;break;}}
    float getParameterValue(uint32_t i)const override{switch(i){case kOn:return fOn;case kKracht:return fKr;case kRate:return fRate;case kDepth:return fDep;case kDelay:return fDel;case kFeed:return fFeed;case kWidth:return fWid;case kDimensie:return fDim;case kSkin:return fSkin;default:return 0;}}
    void setParameterValue(uint32_t i,float v)override{switch(i){case kOn:fOn=v;break;case kKracht:fKr=v;break;case kRate:fRate=v;break;case kDepth:fDep=v;break;case kDelay:fDel=v;break;case kFeed:fFeed=v;break;case kWidth:fWid=v;break;case kDimensie:fDim=v;break;case kSkin:fSkin=v;break;}}
    void activate()override{sr=getSampleRate();std::memset(buf,0,sizeof(buf));pos=0;phL=0;phR=0.5f;lpL=lpR=0;}
    void run(const float**in,float**out,uint32_t n)override{
        if(fOn<0.5f){if(in[0]!=out[0])std::memcpy(out[0],in[0],n*sizeof(float));if(in[1]!=out[1])std::memcpy(out[1],in[1],n*sizeof(float));return;}
        float wet=fKr/100,dep=fDep/100*8.0f,fb=fFeed/100,wid=fWidth_()/100;
        int dim=(int)fDim; float satDrive=dim>=18?1.0f+(dim-17)*0.18f:0.0f; float lpAmt=dim>=9&&dim<18?0.35f:(dim>=18?0.55f:0.0f);
        float rate=fRate*(dim>=18?1.35f:1.0f);
        for(uint32_t i=0;i<n;i++){
            float l=in[0][i],r=in[1][i];
            float modL=(std::sin(phL*6.2831853f)*0.5f+0.5f)*dep+fDel;
            float modR=(std::sin(phR*6.2831853f)*0.5f+0.5f)*dep+fDel;
            float dl=tap(modL),dr=tap(modR);
            // feedback + lp
            if(lpAmt>0){lpL=lpL+(dl-lpL)*lpAmt;dl=lpL;lpR=lpR+(dr-lpR)*lpAmt;dr=lpR;}
            put(l+dl*fb,r+dr*fb);
            float wl=dl*wet,wr=dr*wet;
            if(satDrive>0){wl=std::tanh(wl*satDrive);wr=std::tanh(wr*satDrive);}
            // width: L haalt L-tap + R-tap tegenfase (mid-side breedte)
            out[0][i]=l*(1-wet)+wl+ (wid-0.5f)*wr*0.5f;
            out[1][i]=r*(1-wet)+wr+ (wid-0.5f)*wl*0.5f;
            phL+=rate/sr;phR+=rate/sr;if(phL>=1)phL-=1;if(phR>=1)phR-=1;}
    }
private:
    static const int BS=8192; float buf[2][BS];int pos;float phL,phR,lpL,lpR;
    float fOn=1,fKr=70,fRate=0.8f,fDep=50,fDel=15,fFeed=0,fWid=70,fDim=0,fSkin=2;double sr=44100;
    float fWidth_()const{return fWid;}
    void put(float l,float r){buf[0][pos]=l;buf[1][pos]=r;pos=(pos+1)%BS;}
    float tap(float ms){float s=ms*0.001f*sr;int ipos=pos-(int)s-1;if(ipos<0)ipos+=BS;float fr=s-(int)s;int i2=(ipos+1)%BS;return (buf[0][ipos]+buf[1][ipos])*0.5f*(1-fr)+(buf[0][i2]+buf[1][i2])*0.5f*fr;}
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BChorusPlugin)};
END_NAMESPACE_DISTRHO
DISTRHO::Plugin*DISTRHO::createPlugin(){return new DISTRHO::G2BChorusPlugin();}
