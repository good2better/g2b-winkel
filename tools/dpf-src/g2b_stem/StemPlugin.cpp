#include "DistrhoPlugin.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

START_NAMESPACE_DISTRHO

static const char* kPresetNames[17] = {"G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"};
// DESS, BODY, AIR, COMP, WIDTH, OUT
static const float kPresetParams[17][6] = {
    {0.40f,0.35f,0.30f,0.45f,1.10f,0.f},   // G2B — gebalanceerde vocal
    {0.50f,0.45f,0.25f,0.55f,1.00f,0.f},   // DEMBOW — punchy toon
    {0.45f,0.50f,0.15f,0.60f,0.90f,-1.f},  // DRILL — donker & strak
    {0.55f,0.40f,0.40f,0.35f,1.30f,0.f},   // RNB — zacht & breed
    {0.35f,0.30f,0.45f,0.50f,1.20f,1.f},   // HOUSE — helder & luchtig
    {0.50f,0.50f,0.30f,0.55f,1.10f,0.f},   // AFRO — vol & ritmisch
    {0.48f,0.40f,0.20f,0.58f,0.95f,-1.f}, // TRAP
    {0.42f,0.38f,0.28f,0.48f,1.05f,0.f},   // BOOM
    {0.36f,0.30f,0.42f,0.52f,1.25f,1.f},   // UKG
    {0.44f,0.36f,0.32f,0.56f,1.15f,0.f},   // JERSEY
    {0.46f,0.42f,0.30f,0.54f,1.10f,0.f},   // BOUNCE
    {0.52f,0.44f,0.35f,0.38f,1.20f,0.f},   // SOUL
    {0.40f,0.46f,0.22f,0.50f,0.92f,-1.f},  // LOFI
    {0.45f,0.38f,0.34f,0.50f,1.12f,0.f},   // FUNK
    {0.50f,0.42f,0.28f,0.52f,1.08f,0.f},   // REGGAE
    {0.38f,0.32f,0.44f,0.54f,1.25f,1.f},   // DANCE
    {0.47f,0.40f,0.30f,0.52f,1.10f,0.f},   // KAWINA

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

class StemPlugin : public Plugin {
public:
    enum Params { kDess,kBody,kAir,kComp,kWidth,kOut,kGalaxy,kDice,kPreset,kOn,kKracht,kNumParams };
    StemPlugin() : Plugin(kNumParams,0,2) {}
protected:
    const char* getLabel() const override { return "G2B Stem"; }
    const char* getDescription() const override { return "Complete vocal-keten in één scherm: de-esser, body, air, compressie, breedte"; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','S','v'); }

    void initParameter(uint32_t i, Parameter& p) override {
        if(i==kGalaxy){p.name="GALAXY";p.symbol="galaxy";p.ranges.min=0;p.ranges.max=26;p.ranges.def=9;p.hints|=kParameterIsInteger;return;}
        p.hints=kParameterIsAutomatable;
        switch(i){
        case kDess:  p.name="DE-ESS";   p.symbol="dess";  p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.4f;break;
        case kBody:  p.name="BODY";     p.symbol="body";  p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.35f;break;
        case kAir:   p.name="AIR";      p.symbol="air";   p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.3f;break;
        case kComp:  p.name="COMP";     p.symbol="comp";  p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.45f;break;
        case kWidth: p.name="BREEDTE";  p.symbol="width"; p.ranges.min=0.5f;p.ranges.max=2;p.ranges.def=1.1f;break;
        case kOut:   p.name="OUTPUT";   p.symbol="out";   p.ranges.min=-24;p.ranges.max=12;p.ranges.def=0;p.unit="dB";break;
        case kDice:  p.name="DICE";     p.symbol="dice";  p.hints|=kParameterIsInteger|kParameterIsTrigger;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kPreset:p.name="PRESET";   p.symbol="preset";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=16;p.ranges.def=0;break;
        case kOn:p.name="Aan";p.symbol="aan";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;break;
        case kKracht:p.name="Kracht";p.symbol="kracht";p.ranges.min=0;p.ranges.max=100;p.ranges.def=100;p.unit="%";break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        if(i==kGalaxy)return fGalaxy;
        switch(i){case kDess:return fDess;case kBody:return fBody;case kAir:return fAir;case kComp:return fComp;case kWidth:return fWidth;case kOut:return fOut;case kDice:return fDice;case kPreset:return fPreset;case kOn:return fOn;case kKracht:return fKr;}return 0.f;
    }
    void setParameterValue(uint32_t i, float v) override {
        if(i==kGalaxy){fGalaxy=v;galaxy.setPos((int)v);return;}
        if(i==kPreset){applyPreset((int)(v+0.5f));return;}
        switch(i){
        case kDess:fDess=v;break;case kBody:fBody=v;break;case kAir:fAir=v;break;
        case kComp:fComp=v;break;case kWidth:fWidth=v;break;case kOut:fOut=v;break;
        case kDice:fDice=v;if(v>0.5f)rollDice();fDice=0.f;break;
        case kOn:fOn=v;break;case kKracht:fKr=v;break;
        }
    }
    void initState(uint32_t index, String& name, String& def) override {
        if(index==0){name="scope";def="";}
        else{name="presetnames";def="";for(int i=0;i<17;i++){def+=kPresetNames[i];if(i<16)def+=";";}}
    }
    void setState(const char* k, const char* v) override {(void)k;(void)v;}

    void run(const float** in, float** out, uint32_t frames) override
    {
        if(fOn<0.5f){if(in[0]!=out[0])std::memcpy(out[0],in[0],frames*sizeof(float));if(in[1]!=out[1])std::memcpy(out[1],in[1],frames*sizeof(float));return;}
        processFx_(in, out, frames);
        for(uint32_t i=0;i<frames;i++){
            float L=out[0][i],R=out[1][i];
            galaxy.tick(L,R,getSampleRate());
            out[0][i]=L; out[1][i]=R;
        }
        const float k=fKr/100.f;
        if(k<0.999f){const float d=1.f-k;for(uint32_t i=0;i<frames;i++){out[0][i]=in[0][i]*d+out[0][i]*k;out[1][i]=in[1][i]*d+out[1][i]*k;}}
    }

    void processFx_(const float** in, float** out, uint32_t frames) {
        const float sr=getSampleRate();
        const float outG=powf(10.f,fOut*0.05f);
        // coefficienten
        const float lpEss =1.f-expf(-2.f*M_PI*3000.f/sr);   // sibilance-band detectie
        const float lpBody=1.f-expf(-2.f*M_PI*250.f/sr);
        const float lpAir =1.f-expf(-2.f*M_PI*8000.f/sr);
        const float envA=1.f-expf(-1.f/(0.0015f*sr));       // attack ~1.5ms
        const float envR=1.f-expf(-1.f/(0.12f*sr));         // release ~120ms
        for(uint32_t i=0;i<frames;i++){
            float x0=in[0][i],x1=in[1][i];
            // --- DE-ESSER: meet sibilance (hp>3k), duwt die band terug ---
            fHp0+=lpEss*(x0-fHp0); fHp1+=lpEss*(x1-fHp1);
            float s0=x0-fHp0, s1=x1-fHp1;               // sibilance-inhoud
            float e0=fabsf(s0), e1=fabsf(s1);
            fEssE0+=envR*(e0-fEssE0); if(e0>fEssE0)fEssE0=e0;
            fEssE1+=envR*(e1-fEssE1); if(e1>fEssE1)fEssE1=e1;
            float g0=1.f-fDess*0.75f*(fEssE0>0.01f?fminf(fEssE0*3.f,1.f):0.f);
            float g1=1.f-fDess*0.75f*(fEssE1>0.01f?fminf(fEssE1*3.f,1.f):0.f);
            x0-=s0*(1.f-g0); x1-=s1*(1.f-g1);
            // --- BODY: laag-mid versterking ---
            fBL+=lpBody*x0; fBR+=lpBody*x1;
            x0+=fBL*fBody*0.5f; x1+=fBR*fBody*0.5f;
            // --- AIR: hoog-shelf ---
            fAL+=lpAir*x0; fAR+=lpAir*x1;
            x0+=(x0-fAL)*fAir*0.7f; x1+=(x1-fAR)*fAir*0.7f;
            // --- COMP: RMS-env, zachte knie ---
            float lvl0=fabsf(x0),lvl1=fabsf(x1);
            fCL+=envA*(lvl0-fCL); if(lvl0<fCL)fCL+=envR*(lvl0-fCL)*10.f;
            fCR+=envA*(lvl1-fCR); if(lvl1<fCR)fCR+=envR*(lvl1-fCR)*10.f;
            const float thr=0.15f;
            float gr0=1.f,gr1=1.f;
            if(fCL>thr)gr0=1.f-fComp*0.8f*fminf((fCL-thr)*4.f,0.8f);
            if(fCR>thr)gr1=1.f-fComp*0.8f*fminf((fCR-thr)*4.f,0.8f);
            x0*=gr0; x1*=gr1;
            // saturatie tikje + makeup
            x0=tanhf(x0*(1.f+fComp*1.2f))*(1.f+fComp*0.9f);
            x1=tanhf(x1*(1.f+fComp*1.2f))*(1.f+fComp*0.9f);
            // --- WIDTH (mid/side) ---
            float mid=(x0+x1)*0.5f, side=(x0-x1)*0.5f*fWidth;
            float o0=(mid+side)*outG, o1=(mid-side)*outG;
            out[0][i]=o0; out[1][i]=o1;
            if(++fDecim>=3){fDecim=0;if(fScopeN<40)fScope[fScopeN++]=o0;}
        }
        if(fScopeN>=40){
            char buf[256];int pos=0;
            for(uint32_t s=0;s<fScopeN&&pos<230;++s)pos+=snprintf(buf+pos,sizeof(buf)-pos,"%.3f;",fScope[s]);
            buf[pos]=0;setState("scope",buf);fScopeN=0;
        }
    }
private:
    void rollDice(){
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kDess,0.2f+((fRnd>>8)%100)/100.f*0.5f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kBody,0.15f+((fRnd>>8)%100)/100.f*0.5f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kAir,0.1f+((fRnd>>8)%100)/100.f*0.5f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kComp,0.2f+((fRnd>>8)%100)/100.f*0.6f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kWidth,0.8f+((fRnd>>8)%100)/100.f*0.8f);
    }
    void applyPreset(int i){
        if(i<0)i=0;if(i>5)i=5;fPreset=(float)i;
        setParameterValue(kDess,kPresetParams[i][0]);
        setParameterValue(kBody,kPresetParams[i][1]);
        setParameterValue(kAir,kPresetParams[i][2]);
        setParameterValue(kComp,kPresetParams[i][3]);
        setParameterValue(kWidth,kPresetParams[i][4]);
        setParameterValue(kOut,kPresetParams[i][5]);
    }
    float fDess=0.4f,fBody=0.35f,fAir=0.3f,fComp=0.45f,fWidth=1.1f,fOut=0.f;
    float fHp0=0,fHp1=0,fEssE0=0,fEssE1=0,fBL=0,fBR=0,fAL=0,fAR=0,fCL=0,fCR=0;
    float fDice=0.f,fPreset=0.f;
    unsigned fRnd=112233;
    float fScope[40];uint32_t fScopeN=0;int fDecim=0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemPlugin)
    float fOn=1.f,fKr=100.f,fGalaxy=9.f;
    Galaxy galaxy;
};
Plugin* createPlugin(){return new StemPlugin();}
END_NAMESPACE_DISTRHO
