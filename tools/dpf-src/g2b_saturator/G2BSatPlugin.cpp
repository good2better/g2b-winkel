// G2B Saturator — tape/tube-drive: drive + bias + kleuring + 27 dimensies.
#include "DistrhoPlugin.hpp"
START_NAMESPACE_DISTRHO
class G2BSatPlugin:public Plugin{
public:enum Params{kOn,kKracht,kDrive,kBias,kTone,kLevel,kScLp,kDimensie,kSkin,kPreset,kParamCount};
    G2BSatPlugin():Plugin(kParamCount,0,0){}
    const char*getLabel()const override{return "G2B Saturator";}
    const char*getDescription()const override{return "Tape/tube saturatie met 27 dimensies";}
    const char*getMaker()const override{return "G2B";}
    const char*getLicense()const override{return "ISC";}
    uint32_t getVersion()const override{return d_version(1,0,0);}
    int64_t getUniqueId()const override{return d_cconst('G','2','S','t');}
protected:
    void initParameter(uint32_t i,Parameter&p)override{
        p.hints=kParameterIsAutomatable;
        switch(i){case kOn:p.name="Aan";p.symbol="aan";p.ranges.def=1;p.ranges.min=0;p.ranges.max=1;p.hints|=kParameterIsBoolean;break;
        case kKracht:p.name="Kracht";p.symbol="kracht";p.unit="%";p.ranges.def=100;p.ranges.min=0;p.ranges.max=100;break;
        case kDrive:p.name="Drive";p.symbol="drive";p.ranges.def=40;p.ranges.min=0;p.ranges.max=100;break;
        case kBias:p.name="Bias";p.symbol="bias";p.ranges.def=50;p.ranges.min=0;p.ranges.max=100;break;
        case kTone:p.name="Tone";p.symbol="tone";p.ranges.def=60;p.ranges.min=0;p.ranges.max=100;break;
        case kLevel:p.name="Level";p.symbol="level";p.ranges.def=80;p.ranges.min=0;p.ranges.max=100;break;
        case kScLp:p.name="SC LP";p.symbol="sclp";p.ranges.def=0;p.ranges.min=0;p.ranges.max=100;break;
        case kDimensie:p.name="Dimenties";p.symbol="dim";p.ranges.def=0;p.ranges.min=0;p.ranges.max=26;break;
        case kSkin:p.name="Skin";p.symbol="skin";p.ranges.def=3;p.ranges.min=0;p.ranges.max=7;break;case kPreset:p.name="Preset";p.symbol="pre";p.ranges.min=0;p.ranges.max=15;p.ranges.def=0;p.hints=kParameterIsInteger;break;}}
    float getParameterValue(uint32_t i)const override{switch(i){case kOn:return fOn;case kKracht:return fKr;case kDrive:return fDr;case kBias:return fBi;case kTone:return fTo;case kLevel:return fLe;case kScLp:return fLp;case kDimensie:return fDim;case kSkin:return fSkin;case kPreset:return fPre;default:return 0;}}
    void setParameterValue(uint32_t i,float v)override{switch(i){case kOn:fOn=v;break;case kKracht:fKr=v;break;case kDrive:fDr=v;break;case kBias:fBi=v;break;case kTone:fTo=v;break;case kLevel:fLe=v;break;case kScLp:fLp=v;break;case kDimensie:fDim=v;break;case kSkin:fSkin=v;break;case kPreset:if((int)v!=(int)fPre)applyPreset((int)v);fPre=v;break;}}
    void activate()override{sr=getSampleRate();lpL=lpR=0;dcL=dcR=0;}
    void run(const float**in,float**out,uint32_t n)override{
        if(fOn<0.5f){if(in[0]!=out[0])std::memcpy(out[0],in[0],n*sizeof(float));if(in[1]!=out[1])std::memcpy(out[1],in[1],n*sizeof(float));return;}
        int dim=(int)fDim;
        float wet=fKr/100,drv=1.0f+fDr*0.06f,bias=(fBi-50)*0.004f,lvl=fLe*0.01f;
        float tC=0.15f+fTo/100*0.55f,lpC=fLp/100*0.9f;
        float asym=dim>=18?0.12f:(dim>=9?0.05f:0.0f); // even-harmonieken voor tube/tape
        for(uint32_t i=0;i<n;i++){
            float l=in[0][i]+bias,r=in[1][i]+bias;
            // drive saturatie: tanh + asym voor 2e harmoniek
            l=std::tanh(l*drv)+asym*std::tanh(l*drv*2)*0.5f;
            r=std::tanh(r*drv)+asym*std::tanh(r*drv*2)*0.5f;
            // tone: simpele lp/hoogtilt-mix
            lpL=lpL+(l-lpL)*tC;lpR=lpR+(r-lpR)*tC;
            l=lpL+(l-lpL)*(1-tC);r=lpR+(r-lpR)*(1-tC);
            // sc lp op uitgang
            l=l-(l-lpL)*lpC;r=r-(r-lpR)*lpC;
            // dc-block
            dcL=dcL*0.995f+l*0.005f;dcR=dcR*0.995f+r*0.005f;
            l=(l-dcL)*lvl;r=(r-dcR)*lvl;
            out[0][i]=l*wet+in[0][i]*(1-wet);
            out[1][i]=r*wet+in[1][i]*(1-wet);}
    }
private:
    float lpL,lpR,dcL,dcR;
    void applyPreset(int p){static const float PR[16][5]={{25,30,70,80,40},{80,60,50,75,20},{50,20,60,85,60},{95,80,40,70,10},{10,10,80,90,70},{35,45,65,80,50},{85,70,60,75,30},{45,55,55,75,65},{60,30,75,85,25},{40,50,30,70,45},{55,35,85,80,35},{20,25,75,90,55},{98,90,35,65,5},{50,60,55,78,40},{70,65,45,70,55},{65,45,70,82,30}};const float*q=PR[p&15];fDr=q[0];fBi=q[1];fTo=q[2];fLe=q[3];fLp=q[4];}
    float fOn=1,fKr=100,fDr=40,fBi=50,fTo=60,fLe=80,fLp=0,fDim=0,fSkin=3,fPre=0;double sr=44100;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BSatPlugin)};
END_NAMESPACE_DISTRHO
DISTRHO::Plugin*DISTRHO::createPlugin(){return new DISTRHO::G2BSatPlugin();}
