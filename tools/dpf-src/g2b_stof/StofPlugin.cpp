#include "DistrhoPlugin.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

START_NAMESPACE_DISTRHO

static const char* kPresetNames[17] = {"G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"};
// KRAAK, WOBBLE, TOON, CRUSH, MIX, OUT
static const float kPresetParams[17][6] = {
    {0.25f,0.20f,0.35f,0.15f,1.0f,0.f},   // G2B — subtiel stof
    {0.30f,0.25f,0.45f,0.20f,1.0f,0.f},   // DEMBOW
    {0.40f,0.30f,0.55f,0.30f,1.0f,-1.f},  // DRILL — donker lo-fi
    {0.20f,0.15f,0.30f,0.10f,1.0f,0.f},   // RNB — zacht
    {0.15f,0.10f,0.20f,0.10f,1.0f,0.f},   // HOUSE — licht
    {0.35f,0.35f,0.45f,0.25f,1.0f,0.f},   // AFRO
    {0.45f,0.30f,0.50f,0.35f,1.0f,-1.f}, // TRAP
    {0.30f,0.25f,0.40f,0.20f,1.0f,0.f},   // BOOM
    {0.28f,0.22f,0.38f,0.18f,1.0f,0.f},   // UKG
    {0.32f,0.28f,0.42f,0.22f,1.0f,0.f},   // JERSEY
    {0.35f,0.30f,0.45f,0.25f,1.0f,0.f},   // BOUNCE
    {0.22f,0.18f,0.32f,0.12f,1.0f,0.f},   // SOUL
    {0.60f,0.55f,0.65f,0.50f,1.0f,-2.f},  // LOFI — maximale stof
    {0.30f,0.35f,0.40f,0.28f,1.0f,0.f},   // FUNK
    {0.38f,0.32f,0.44f,0.26f,1.0f,0.f},   // REGGAE
    {0.20f,0.12f,0.28f,0.14f,1.0f,0.f},   // DANCE
    {0.33f,0.28f,0.42f,0.22f,1.0f,0.f},   // KAWINA

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

class StofPlugin : public Plugin {
public:
    enum Params { kKraak,kWobble,kToon,kCrush,kMix,kOut,kGalaxy,kDice,kPreset,kNumParams };
    StofPlugin() : Plugin(kNumParams,0,2) {}
protected:
    const char* getLabel() const override { return "G2B Stof"; }
    const char* getDescription() const override { return "Lo-fi machine: vinyl-kraak, wow/flutter, donkere toon en bit-crush"; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','S','f'); }

    void initParameter(uint32_t i, Parameter& p) override {
        if(i==kGalaxy){p.name="GALAXY";p.symbol="galaxy";p.ranges.min=0;p.ranges.max=26;p.ranges.def=9;p.hints|=kParameterIsInteger;return;}
        p.hints=kParameterIsAutomatable;
        switch(i){
        case kKraak: p.name="KRAAK";  p.symbol="kraak"; p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.25f;break;
        case kWobble:p.name="WOBBLE"; p.symbol="wobble";p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.2f;break;
        case kToon:  p.name="TOON";   p.symbol="toon";  p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.35f;break;
        case kCrush: p.name="CRUSH";  p.symbol="crush"; p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.15f;break;
        case kMix:   p.name="MIX";    p.symbol="mix";   p.ranges.min=0;p.ranges.max=1;p.ranges.def=1.0f;break;
        case kOut:   p.name="OUTPUT"; p.symbol="out";   p.ranges.min=-24;p.ranges.max=12;p.ranges.def=0;p.unit="dB";break;
        case kDice:  p.name="DICE";   p.symbol="dice";  p.hints|=kParameterIsInteger|kParameterIsTrigger;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kPreset:p.name="PRESET"; p.symbol="preset";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=16;p.ranges.def=0;break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        if(i==kGalaxy)return fGalaxy;
        switch(i){case kKraak:return fKraak;case kWobble:return fWobble;case kToon:return fToon;case kCrush:return fCrush;case kMix:return fMix;case kOut:return fOut;case kDice:return fDice;case kPreset:return fPreset;}return 0.f;
    }
    void setParameterValue(uint32_t i, float v) override {
        if(i==kGalaxy){fGalaxy=v;galaxy.setPos((int)v);return;}
        if(i==kPreset){applyPreset((int)(v+0.5f));return;}
        switch(i){
        case kKraak:fKraak=v;break;case kWobble:fWobble=v;break;case kToon:fToon=v;break;
        case kCrush:fCrush=v;break;case kMix:fMix=v;break;case kOut:fOut=v;break;
        case kDice:fDice=v;if(v>0.5f)rollDice();fDice=0.f;break;
        }
    }
    void initState(uint32_t index, String& name, String& def) override {
        if(index==0){name="scope";def="";}
        else{name="presetnames";def="";for(int i=0;i<17;i++){def+=kPresetNames[i];if(i<16)def+=";";}}
    }
    void setState(const char* k, const char* v) override {(void)k;(void)v;}

    void run(const float** in, float** out, uint32_t frames) override
    {
        processFx_(in, out, frames);
        for(uint32_t i=0;i<frames;i++){
            float L=in[0][i],R=in[1][i];
            galaxy.tick(L,R,getSampleRate());
            out[0][i]=L; out[1][i]=R;
        }
    }

    void processFx_(const float** in, float** out, uint32_t frames) {
        const float sr=getSampleRate();
        const float outG=powf(10.f,fOut*0.05f);
        const float lpA=1.f-expf(-2.f*M_PI*(12000.f-fToon*9500.f)/sr); // donkerder bij meer TOON
        const float bits=powf(2.f,12.f-fCrush*8.f);                    // 12→4 bit
        const float wobA=2.f*M_PI*0.6f/sr;                             // wow ~0.6Hz
        const float wobD=fWobble*0.004f;                               // ±0.4% pitch
        for(uint32_t i=0;i<frames;i++){
            // --- WOBBLE: pitch-modulatie via mini-delay ---
            fWobPh+=wobA; if(fWobPh>2.f*M_PI)fWobPh-=2.f*M_PI;
            float wob=sinf(fWobPh)*wobD*sr*0.5f; // in samples
            float rd=(float)fWPos+wob+4096.f;
            int rp=((int)rd)%4096; int rp2=(rp+1)%4096; float frac=rd-(int)rd;
            float x0=fRingL[rp]+(fRingL[rp2]-fRingL[rp])*frac;
            float x1=fRingR[rp]+(fRingR[rp2]-fRingR[rp])*frac;
            fRingL[fWPos%4096]=in[0][i]; fRingR[fWPos%4096]=in[1][i]; fWPos++;
            // --- KRAAK: vinyl ruis + pops ---
            fRnd=fRnd*1664525u+1013904223u;
            float noise=((fRnd>>8)%1000/1000.f-0.5f);
            fLpNoise+=0.15f*(noise-fLpNoise); // licht gefilterd
            x0+=fLpNoise*fKraak*0.10f;
            x1+=fLpNoise*fKraak*0.10f;
            if((fRnd>>9)%100000<3){ // sporadische pop
                float pop=((fRnd>>7)%1000/1000.f-0.5f)*fKraak*0.6f;
                x0+=pop;x1+=pop;
            }
            // --- TOON: donkerder ---
            fLp0+=lpA*(x0-fLp0); fLp1+=lpA*(x1-fLp1);
            x0=fLp0; x1=fLp1;
            // --- CRUSH: bit-depth ---
            if(fCrush>0.001f){
                x0=floorf(x0*bits+0.5f)/bits;
                x1=floorf(x1*bits+0.5f)/bits;
            }
            float o0=(in[0][i]*(1.f-fMix)+x0*fMix)*outG;
            float o1=(in[1][i]*(1.f-fMix)+x1*fMix)*outG;
            out[0][i]=o0;out[1][i]=o1;
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
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kKraak,((fRnd>>8)%100)/100.f*0.6f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kWobble,((fRnd>>8)%100)/100.f*0.5f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kToon,0.1f+((fRnd>>8)%100)/100.f*0.6f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kCrush,((fRnd>>8)%100)/100.f*0.4f);
    }
    void applyPreset(int i){
        if(i<0)i=0;if(i>5)i=5;fPreset=(float)i;
        setParameterValue(kKraak,kPresetParams[i][0]);
        setParameterValue(kWobble,kPresetParams[i][1]);
        setParameterValue(kToon,kPresetParams[i][2]);
        setParameterValue(kCrush,kPresetParams[i][3]);
        setParameterValue(kMix,kPresetParams[i][4]);
        setParameterValue(kOut,kPresetParams[i][5]);
    }
    float fKraak=0.25f,fWobble=0.2f,fToon=0.35f,fCrush=0.15f,fMix=1.0f,fOut=0.f;
    float fLp0=0,fLp1=0,fLpNoise=0,fWobPh=0;
    float fRingL[4096]={0},fRingR[4096]={0};uint32_t fWPos=0;
    float fDice=0.f,fPreset=0.f;
    unsigned fRnd=424242;
    float fScope[40];uint32_t fScopeN=0;int fDecim=0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StofPlugin)
    float fGalaxy=9.f;
    Galaxy galaxy;
};
Plugin* createPlugin(){return new StofPlugin();}
END_NAMESPACE_DISTRHO
