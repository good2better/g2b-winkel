#include "DistrhoPlugin.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

START_NAMESPACE_DISTRHO

static const char* kPresetNames[17] = {"G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"};
// SUB,LAAG,MID,AIR,WIDTH,DRIVE,OUT
static const float kPresetParams[17][7] = {
    {0.f,0.f,0.f,+1.f, 1.1f,0.15f,0.f},   // G2B — lichte balans
    {+1.f,+1.f,0.f,+0.5f, 1.0f,0.20f,0.f},// DEMBOW — laag voorop
    {+2.f,0.f,-0.5f,+1.f, 1.0f,0.25f,0.f},// DRILL — sub + donker
    {0.f,+0.5f,+1.f,+1.f, 1.15f,0.10f,0.f},// RNB — warm + glad
    {0.f,-0.5f,+1.f,+1.5f,1.2f,0.15f,0.f},// HOUSE — top open
    {+1.f,0.f,+0.5f,+1.f, 1.1f,0.20f,0.f},// AFRO
    {+2.f,0.f,-0.5f,+0.5f,1.0f,0.30f,0.f}, // TRAP
    {0.f,+1.f,+0.5f,+1.f, 1.05f,0.18f,0.f},// BOOM
    {0.f,-0.5f,+1.f,+1.5f,1.25f,0.15f,0.f},// UKG
    {+1.f,0.f,+0.5f,+1.f, 1.15f,0.20f,0.f},// JERSEY
    {0.f,+0.5f,+1.f,+1.f, 1.15f,0.18f,0.f},// BOUNCE
    {0.f,+1.f,+0.5f,+1.5f,1.1f,0.10f,0.f}, // SOUL
    {0.f,+0.5f,0.f,+0.5f, 0.95f,0.22f,0.f},// LOFI
    {+0.5f,0.f,+1.f,+1.f, 1.1f,0.20f,0.f}, // FUNK
    {+1.f,+0.5f,+0.5f,+1.f,1.05f,0.15f,0.f},// REGGAE
    {0.f,-0.5f,+1.f,+2.f, 1.25f,0.15f,0.f},// DANCE
    {+1.f,0.f,+0.5f,+1.f, 1.1f,0.18f,0.f}, // KAWINA

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

class BandenPlugin : public Plugin {
public:
    enum Params { kSub,kLaag,kMid,kAir,kWidth,kDrive,kOut,kListen,kGalaxy,kDice,kPreset,kNumParams };
    BandenPlugin() : Plugin(kNumParams,0,2) {}
protected:
    const char* getLabel() const override { return "G2B Banden"; }
    const char* getDescription() const override { return "Multiband-mixer: 4 freq-lagen elk met gain — 🧠 LUISTER stelt alles automatisch in balans"; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','B','n'); }

    void initParameter(uint32_t i, Parameter& p) override {
        if(i==kGalaxy){p.name="GALAXY";p.symbol="galaxy";p.ranges.min=0;p.ranges.max=26;p.ranges.def=9;p.hints|=kParameterIsInteger;return;}
        p.hints=kParameterIsAutomatable;
        switch(i){
        case kSub:  p.name="SUB";   p.symbol="sub";  p.ranges.min=-12;p.ranges.max=12;p.ranges.def=0;p.unit="dB";break;
        case kLaag: p.name="LAAG";  p.symbol="laag"; p.ranges.min=-12;p.ranges.max=12;p.ranges.def=0;p.unit="dB";break;
        case kMid:  p.name="MID";   p.symbol="mid";  p.ranges.min=-12;p.ranges.max=12;p.ranges.def=0;p.unit="dB";break;
        case kAir:  p.name="AIR";   p.symbol="air";  p.ranges.min=-12;p.ranges.max=12;p.ranges.def=0;p.unit="dB";break;
        case kWidth:p.name="WIDTH"; p.symbol="width";p.ranges.min=0;p.ranges.max=2;p.ranges.def=1.0f;break;
        case kDrive:p.name="DRIVE"; p.symbol="drive";p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.15f;break;
        case kOut:  p.name="OUTPUT";p.symbol="out";  p.ranges.min=-24;p.ranges.max=12;p.ranges.def=0;p.unit="dB";break;
        case kListen:p.name="LUISTER";p.symbol="listen";p.hints|=kParameterIsInteger|kParameterIsTrigger;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kDice: p.name="DICE";  p.symbol="dice"; p.hints|=kParameterIsInteger|kParameterIsTrigger;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kPreset:p.name="PRESET";p.symbol="preset";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=16;p.ranges.def=0;break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        if(i==kGalaxy)return fGalaxy;
        switch(i){case kSub:return fSub;case kLaag:return fLaag;case kMid:return fMid;case kAir:return fAir;case kWidth:return fWidth;case kDrive:return fDrive;case kOut:return fOut;case kListen:return fListen;case kDice:return fDice;case kPreset:return fPreset;}return 0.f;
    }
    void setParameterValue(uint32_t i, float v) override {
        if(i==kGalaxy){fGalaxy=v;galaxy.setPos((int)v);return;}
        if(i==kPreset){applyPreset((int)(v+0.5f));return;}
        switch(i){
        case kSub:fSub=v;break;case kLaag:fLaag=v;break;case kMid:fMid=v;break;case kAir:fAir=v;break;
        case kWidth:fWidth=v;break;case kDrive:fDrive=v;break;case kOut:fOut=v;break;
        case kListen:
            fListen=v;
            if(v>0.9f){ // echte knop-druk → start luisteren
                fListenN=(uint32_t)(2.f*getSampleRate());
                for(int b=0;b<4;b++)fAcc[b]=0;
                fPeak=0;fListenState=0;
            }else if(v>0.2f){ // status-update vanuit listenApply (0.35/0.5/0.75)
                fListenState=v;
            }
            fListen=0.f;break;
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
        const float g[4]={powf(10.f,fSub*0.05f),powf(10.f,fLaag*0.05f),powf(10.f,fMid*0.05f),powf(10.f,fAir*0.05f)};
        const float a1=1.f-expf(-2.f*M_PI*120.f/sr);
        const float a2=1.f-expf(-2.f*M_PI*800.f/sr);
        const float a3=1.f-expf(-2.f*M_PI*5000.f/sr);
        for(uint32_t i=0;i<frames;i++){
            float band[4]; float o[2];
            for(int c=0;c<2;c++){
                float x=in[c][i];
                fLp1[c]+=a1*(x-fLp1[c]);
                fLp2[c]+=a2*(x-fLp2[c]);
                fLp3[c]+=a3*(x-fLp3[c]);
                float sub=fLp1[c];
                float low=fLp2[c]-fLp1[c];
                float mid=fLp3[c]-fLp2[c];
                float air=x-fLp3[c];
                float w=x*g[0]*0.f+sub*g[0]+low*g[1]+mid*g[2]+air*g[3]-x; // bands met gain
                w=x+ (sub*(g[0]-1)+low*(g[1]-1)+mid*(g[2]-1)+air*(g[3]-1));
                if(fDrive>0.001f) w=tanhf(w*(1.f+fDrive*3.f))/(1.f+fDrive*0.8f);
                band[0]=sub;band[1]=low;band[2]=mid;band[3]=air;
                if(fListenN>0&&c==0){for(int b=0;b<4;b++)fAcc[b]+=fabsf(band[b]);float ax=fabsf(x);if(ax>fPeak)fPeak=ax;}
                o[c]=w;
            }
            // width op mid+air (sub/low blijft midden — theorie-correct)
            float mid=(o[0]+o[1])*0.5f, side=(o[0]-o[1])*0.5f*fWidth;
            // sub+low uit o halen zou te duur zijn — width alleen op het differentiële deel
            o[0]=mid+side;o[1]=mid-side;
            out[0][i]=o[0]*outG;out[1][i]=o[1]*outG;
            if(fListenN>0){ if(--fListenN==0) listenApply(); }
            if(++fDecim>=3){fDecim=0;if(fScopeN<40)fScope[fScopeN++]=out[0][i];}
        }
        if(fScopeN>=40){
            char buf[256];int p=0;
            for(uint32_t s=0;s<fScopeN&&p<230;++s)p+=snprintf(buf+p,sizeof(buf)-p,"%.3f;",fScope[s]);
            buf[p]=0;setState("scope",buf);fScopeN=0;
        }
    }
private:
    void listenApply(){
        // doel-curve per STIJL-preset: {sub, laag, mid, air} energie-share
        static const float kCurves[6][4]={
            {0.30f,0.35f,0.25f,0.10f}, // G2B     (balans)
            {0.40f,0.32f,0.20f,0.08f}, // DEMBOW  (dikke sub hoort)
            {0.38f,0.30f,0.24f,0.08f}, // DRILL
            {0.28f,0.34f,0.28f,0.10f}, // RNB
            {0.35f,0.30f,0.25f,0.10f}, // HOUSE
            {0.34f,0.33f,0.23f,0.10f}  // AFRO
        };
        int pi=(int)(fPreset+0.5f); if(pi<0)pi=0; if(pi>5)pi=5;
        const float* tgt=kCurves[pi];
        float tot=fAcc[0]+fAcc[1]+fAcc[2]+fAcc[3]+1e-9f;
        float g[4]; float mean=0,amax=0;
        for(int b=0;b<4;b++){
            float cur=fAcc[b]/tot;
            float adj=20.f*log10f((tgt[b]+0.02f)/(cur+0.02f));
            if(adj>6.f)adj=6.f; if(adj<-6.f)adj=-6.f;
            g[b]=adj;mean+=adj;
        }
        mean/=4.f;
        for(int b=0;b<4;b++){g[b]-=mean;float a=fabsf(g[b]);if(a>amax)amax=a;}
        if(amax<0.8f){
            // balans klopt al — niet schroeven, alleen melden
            setParameterValue(kListen,0.5f);
        }else{
            setParameterValue(kSub, g[0]);
            setParameterValue(kLaag,g[1]);
            setParameterValue(kMid, g[2]);
            setParameterValue(kAir, g[3]);
            setParameterValue(kListen,0.35f);
        }
        // breuk-bewaker: piek > 0.9 = heet signaal → output veilig terug
        if(fPeak>0.9f){ setParameterValue(kOut,-3.f); setParameterValue(kListen,0.75f); }
        fPeak=0.f;
    }
    void rollDice(){
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kSub,(((fRnd>>8)%100)/100.f-0.4f)*4.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kLaag,(((fRnd>>8)%100)/100.f-0.5f)*4.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kMid,(((fRnd>>8)%100)/100.f-0.5f)*4.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kAir,(((fRnd>>8)%100)/100.f-0.4f)*4.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kWidth,0.6f+((fRnd>>8)%100)/100.f*0.8f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kDrive,((fRnd>>8)%100)/100.f*0.5f);
    }
    void applyPreset(int i){
        if(i<0)i=0;if(i>5)i=5;fPreset=(float)i;
        setParameterValue(kSub, kPresetParams[i][0]);
        setParameterValue(kLaag,kPresetParams[i][1]);
        setParameterValue(kMid, kPresetParams[i][2]);
        setParameterValue(kAir, kPresetParams[i][3]);
        setParameterValue(kWidth,kPresetParams[i][4]);
        setParameterValue(kDrive,kPresetParams[i][5]);
        setParameterValue(kOut,  kPresetParams[i][6]);
    }
    float fSub=0,fLaag=0,fMid=0,fAir=0,fWidth=1.f,fDrive=0.15f,fOut=0.f;
    float fLp1[2]={0,0},fLp2[2]={0,0},fLp3[2]={0,0};
    float fListen=0,fDice=0,fPreset=0;
    uint32_t fListenN=0; float fAcc[4]={0,0,0,0}; float fListenState=0,fPeak=0;
    unsigned fRnd=999111;
    float fScope[40];uint32_t fScopeN=0;int fDecim=0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandenPlugin)
    float fGalaxy=9.f;
    Galaxy galaxy;
};
Plugin* createPlugin(){return new BandenPlugin();}
END_NAMESPACE_DISTRHO
