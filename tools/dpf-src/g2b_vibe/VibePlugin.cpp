#include "DistrhoPlugin.hpp"
#include <cstring>
#include <cmath>
#include <cstdint>
#include <algorithm>

START_NAMESPACE_DISTRHO

static const char* kPresetNames[16] = {"G2B","CASSETTE","VINYL","BOOMBAP","DUB","JAREN-70","LOFI","WALKMAN","NACHT","RADIO","SOUL","TRAP","DRILL","BOOM","NEON","WARM"};
// POMP,CURVE,RATE, WOW,TAPE,STOF,ECHO,DARK,AGE | GALAXY,KRACHT
static const float kPresetParams[16][11] = {
    {0.50f,1.f,0.f, 0.30f,0.35f,0.20f,0.25f,0.30f,0.30f, 9,100},  // G2B
    {0.40f,1.f,0.f, 0.55f,0.55f,0.45f,0.30f,0.55f,0.65f, 4,100},  // CASSETTE
    {0.50f,1.f,0.f, 0.25f,0.30f,0.70f,0.15f,0.35f,0.45f, 5,100},  // VINYL
    {0.65f,0.f,0.f, 0.40f,0.50f,0.55f,0.20f,0.60f,0.60f, 6,100},  // BOOMBAP
    {0.55f,2.f,0.f, 0.30f,0.40f,0.25f,0.70f,0.45f,0.50f, 15,95},  // DUB
    {0.30f,2.f,0.f, 0.70f,0.60f,0.50f,0.35f,0.70f,0.85f, 12,90},  // JAREN-70
    {0.50f,0.f,1.f, 0.50f,0.55f,0.60f,0.20f,0.75f,0.70f, 8,90},  // LOFI
    {0.40f,1.f,0.f, 0.65f,0.45f,0.40f,0.25f,0.55f,0.75f, 3,90},  // WALKMAN
    {0.60f,3.f,0.f, 0.35f,0.45f,0.30f,0.45f,0.65f,0.55f, 14,95},  // NACHT
    {0.50f,1.f,0.f, 0.45f,0.50f,0.35f,0.30f,0.85f,0.60f, 4,100},  // RADIO
    {0.50f,2.f,0.f, 0.35f,0.45f,0.25f,0.35f,0.50f,0.55f, 11,100},  // SOUL
    {0.65f,0.f,1.f, 0.30f,0.40f,0.20f,0.30f,0.40f,0.40f, 2,100},  // TRAP
    {0.70f,0.f,2.f, 0.35f,0.45f,0.20f,0.35f,0.45f,0.45f, 2,100},  // DRILL
    {0.60f,1.f,0.f, 0.40f,0.45f,0.35f,0.30f,0.50f,0.55f, 9,100},  // BOOM
    {0.60f,4.f,2.f, 0.20f,0.35f,0.15f,0.50f,0.25f,0.30f, 18,95},  // NEON
    {0.40f,2.f,0.f, 0.30f,0.50f,0.15f,0.30f,0.40f,0.45f, 10,100},  // WARM
};

// ---- GALAXY-knop (zelfde als FX) — 27 posities ----
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

class VibePlugin : public Plugin {
public:
    enum Params { kPomp, kCurve, kRate, kWow, kTape, kStof, kEcho, kDark, kAge, kGalaxy, kDice, kPreset, kOn, kKracht, kNumParams };
    VibePlugin() : Plugin(kNumParams, 0, 2) {
        memset(fWowL,0,sizeof(fWowL)); memset(fWowR,0,sizeof(fWowR));
        memset(fEchL,0,sizeof(fEchL)); memset(fEchR,0,sizeof(fEchR));
        fRngState=22222u;
    }
protected:
    const char* getLabel() const override { return "G2B VIBE"; }
    const char* getDescription() const override { return "Pomp + band in een: sidechain-pomp op je tempo + wow/tape/stof/echo/dark/age"; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','V','b'); }

    void initParameter(uint32_t i, Parameter& p) override {
        if(i==kGalaxy){p.name="DIMENTIES";p.symbol="dimenties";p.ranges.min=0;p.ranges.max=26;p.ranges.def=9;p.hints|=kParameterIsInteger;return;}
        p.hints=kParameterIsAutomatable;
        switch(i){
        case kPomp:  p.name="POMP";   p.symbol="pomp";  p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.40f;p.unit="%";break;
        case kCurve: p.name="CURVE";  p.symbol="curve"; p.hints|=kParameterIsInteger;p.ranges.min=0.f;p.ranges.max=15.f;p.ranges.def=1.f;break;
        case kRate:  p.name="RATE";   p.symbol="rate";  p.hints|=kParameterIsInteger;p.ranges.min=0.f;p.ranges.max=3.f;p.ranges.def=0.f;break;
        case kWow:   p.name="WOW";    p.symbol="wow";   p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.30f;p.unit="%";break;
        case kTape:  p.name="TAPE";   p.symbol="tape";  p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.35f;p.unit="%";break;
        case kStof:  p.name="STOF";   p.symbol="stof";  p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.20f;p.unit="%";break;
        case kEcho:  p.name="ECHO";   p.symbol="echo";  p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.25f;p.unit="%";break;
        case kDark:  p.name="DARK";   p.symbol="dark";  p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.30f;p.unit="%";break;
        case kAge:   p.name="AGE";    p.symbol="age";   p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.30f;p.unit="%";break;
        case kDice:  p.name="DICE";   p.symbol="dice";  p.hints|=kParameterIsInteger|kParameterIsTrigger;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.f;break;
        case kPreset:p.name="PRESET"; p.symbol="preset";p.hints|=kParameterIsInteger; p.ranges.min=0.f;p.ranges.max=15.f;p.ranges.def=0.f;break;
        case kOn:    p.name="Aan";    p.symbol="aan";   p.hints|=kParameterIsBoolean; p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=1.f;break;
        case kKracht:p.name="Kracht"; p.symbol="kracht";p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=100.f;p.unit="%";break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){case kPomp:return fPomp;case kCurve:return fCurve;case kRate:return fRate;case kWow:return fWow;case kTape:return fTape;case kStof:return fStof;case kEcho:return fEcho;case kDark:return fDark;case kAge:return fAge;case kGalaxy:return fGalaxy;case kDice:return fDice;case kPreset:return fPreset;case kOn:return fOn;case kKracht:return fKr;}return 0.f;
    }
    void setParameterValue(uint32_t i, float v) override {
        if(i==kGalaxy){fGalaxy=v;galaxy.setPos((int)v);return;}
        if(i==kPreset){applyPreset((int)(v+0.5f));return;}
        switch(i){case kPomp:fPomp=v;break;case kCurve:fCurve=v;break;case kRate:fRate=v;break;case kWow:fWow=v;break;case kTape:fTape=v;break;case kStof:fStof=v;break;case kEcho:fEcho=v;break;case kDark:fDark=v;break;case kAge:fAge=v;break;
            case kDice:fDice=v;if(v>0.5f)rollDice();fDice=0.f;break;
            case kOn:fOn=v;break;case kKracht:fKr=v;break;}
    }
    void initState(uint32_t index, String& name, String& def) override {
        if(index==0){name="scope";def="";}
        else{name="presetnames";def="";for(int i=0;i<16;i++){def+=kPresetNames[i];if(i<15)def+=";";}}
    }
    void setState(const char* k, const char* v) override {(void)k;(void)v;}

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        if(fOn<0.5f){if(inputs[0]!=outputs[0])std::memcpy(outputs[0],inputs[0],frames*sizeof(float));if(inputs[1]!=outputs[1])std::memcpy(outputs[1],inputs[1],frames*sizeof(float));return;}
        processFx_(inputs, outputs, frames);
        for(uint32_t i=0;i<frames;i++){
            float L=outputs[0][i],R=outputs[1][i];
            galaxy.tick(L,R,getSampleRate());
            outputs[0][i]=L; outputs[1][i]=R;
        }
        // auto level-match
        {
            float inE=0.f,outE=0.f;
            for(uint32_t i=0;i<frames;i++){inE+=inputs[0][i]*inputs[0][i]+inputs[1][i]*inputs[1][i];outE+=outputs[0][i]*outputs[0][i]+outputs[1][i]*outputs[1][i];}
            inE/=(2.f*frames);outE/=(2.f*frames);
            fLvlIn=fLvlIn*0.98f+inE*0.02f; fLvlOut=fLvlOut*0.98f+outE*0.02f;
            if(fLvlIn>1e-7f&&fLvlOut>1e-7f){
                float t=sqrtf(fLvlIn/fLvlOut);
                if(t<0.55f)t=0.55f; if(t>1.8f)t=1.8f;
                fMakeup+=0.08f*(t-fMakeup);
            }
            for(uint32_t i=0;i<frames;i++){outputs[0][i]*=fMakeup;outputs[1][i]*=fMakeup;}
        }
        const float k=fKr/100.f;
        if(k<0.999f){const float d=1.f-k;for(uint32_t i=0;i<frames;i++){outputs[0][i]=inputs[0][i]*d+outputs[0][i]*k;outputs[1][i]=inputs[1][i]*d+outputs[1][i]*k;}}
    }

    void processFx_(const float** inputs, float** outputs, uint32_t frames) {
        const float sr=getSampleRate();
        const float srInv=1.f/sr;
        const float ageM=0.35f+1.3f*fAge;               // AGE versterkt alles
        // DARK: lowpass cutoff 16k → 3.2k
        const float darkFc=16000.f-fDark*12800.f*(0.5f+0.5f*ageM);
        const float darkA=1.f-expf(-2.f*M_PI*darkFc*srInv);
        // WOW: gemoduleerde delay 6-18ms; lfo-wow 0.55Hz + flutter 6.3Hz
        const float wowDepth=fWow*ageM;
        // TAPE: saturatie
        const float tDr=1.f+fTape*1.6f*ageM, tNorm=1.f/tanhf(tDr);
        // STOF: hiss + crackle
        const float hissLvl=fStof*ageM*0.0035f;
        const float crackP=fStof*ageM*0.00035f;
        // ECHO: 0.36s + 0.53s ping-pong
        const int e1=(int)(0.36f*sr), e2=(int)(0.53f*sr);
        const float eFb=fEcho*0.55f, eWet=fEcho;

        // POMP: tempo-sync pomp-envelop (rate: 0=1/4, 1=1/8, 2=1/16, 3=1/2)
        float bpm=120.f;
        const TimePosition& tp=getTimePosition();
        if(tp.playing&&tp.bbt.valid&&tp.bbt.beatsPerMinute>1)fBpm=fBpm*0.95f+tp.bbt.beatsPerMinute*0.05f;
        bpm=fBpm;
        const float rateMult[4]={1.f,0.5f,0.25f,2.f};
        const float cycBeats=rateMult[(int)(fRate+0.5f)&3];
        const float cycSamps=cycBeats*60.f/bpm*sr;
        for(uint32_t i=0;i<frames;i++){
            float L=inputs[0][i], R=inputs[1][i];

            // 0) POMP: duikt weg op de beat
            {
                fPumpPh+=1.f/cycSamps; if(fPumpPh>=1.f)fPumpPh-=1.f;
                float ph=fPumpPh, shape=0.f;
                int c=(int)(fCurve+0.5f); if(c<0)c=0; if(c>15)c=15;
                switch(c){
                case 0:  shape=expf(-ph*8.f);break;                 // STRAK
                case 1:  shape=expf(-ph*5.f);break;                 // NORMAAL
                case 2:  shape=expf(-ph*3.f);break;                 // LANGZAAM
                case 3:  shape=expf(-ph*2.f);break;                 // XL
                case 4:  shape=(ph<0.5f)?1.f:0.f;break;             // HOLD
                case 5:  shape=(ph<0.5f)?(1.f-2.f*ph):0.f;break;    // TRIANGLE
                case 6:  shape=(ph<0.5f)?0.f:expf(-(ph-0.5f)*8.f);break; // OFFBEAT
                case 7:  shape=expf(-(ph-0.25f)*(ph-0.25f)*60.f);break;  // GAUSS
                case 8:  shape=expf(-ph*6.f)*(0.7f+0.3f*sinf(ph*12.566f));break; // WOBBLE
                case 9:  shape=(ph<0.25f)?1.f:expf(-(ph-0.25f)*6.f);break;       // PLUCK
                case 10: shape=(ph<0.35f)?(1.f-ph/0.35f*0.4f):expf(-(ph-0.35f)*7.f);break; // DIP
                case 11: shape=expf(-ph*4.f)*expf(-ph*4.f);break;   // SOFT
                case 12: shape=expf(-ph*10.f);break;                // TICK
                case 13: shape=(ph<0.15f)?0.f:expf(-(ph-0.15f)*5.f);break; // GATE
                case 14: shape=0.5f+0.5f*cosf(ph*6.28318f);break;   // SINE
                case 15: shape=(ph<0.5f)?(ph*2.f):(2.f-ph*2.f);break;      // SWELL
                }
                float g=1.f-fPomp*shape;
                if(g<0.f)g=0.f;
                L*=g;R*=g;
            }

            // 1) WOW+FLUTTER: pitch-mee-deining (modulatie-delay)
            fWowPh+=0.55f*6.28318f*srInv; if(fWowPh>6.28318f)fWowPh-=6.28318f;
            fFluPh+=6.3f*6.28318f*srInv;  if(fFluPh>6.28318f)fFluPh-=6.28318f;
            {
                float mod=sinf(fWowPh)+0.35f*sinf(fFluPh);
                float rd=8.f+wowDepth*10.f*mod;          // 8ms ±10ms
                if(rd<1.f)rd=1.f; if(rd>24.f)rd=24.f;
                float pos=fWowW-rd; if(pos<0)pos+=kWowN;
                int i0=(int)pos; float fr=pos-i0;
                int i1=(i0+1)%kWowN;
                float wL=fWowL[i0]+(fWowL[i1]-fWowL[i0])*fr;
                float wR=fWowR[i0]+(fWowR[i1]-fWowR[i0])*fr;
                fWowL[fWowW]=L; fWowR[fWowW]=R; fWowW=(fWowW+1)%kWowN;
                L=L+(wL-L)*wowDepth; R=R+(wR-R)*wowDepth;
            }

            // 2) TAPE-saturatie + hoofdruimte-verlies
            L=tanhf(L*tDr)*tNorm; R=tanhf(R*tDr)*tNorm;
            L*=1.f-0.06f*fTape; R*=1.f-0.06f*fTape;

            // 3) STOF: band-hiss + kraakjes
            {
                fRngState^=fRngState<<13;fRngState^=fRngState>>17;fRngState^=fRngState<<5;
                float nz=(fRngState&0xFFFFu)/32768.f-1.f;
                fHissL+=0.3f*(nz-fHissL); fHissR+=0.3f*(fHissL*0.9f+0.1f*nz-fHissR);
                L+=fHissL*hissLvl*8.f; R+=fHissR*hissLvl*8.f;
                // crackle: zeldzame tikjes
                if(((fRngState>>8)&0xFFFFu)/65536.f<crackP){
                    fCrkEnv=(fRngState&0x8000u?1.f:-1.f)*(0.35f+0.65f*(((fRngState>>16)&0xFFu)/255.f));
                }
                L+=fCrkEnv*0.18f; R+=fCrkEnv*0.15f;
                fCrkEnv*=0.94f;
            }

            // 4) ECHO: ping-pong delay
            {
                int r1=(fEchW-e1+kEchN)%kEchN, r2=(fEchW-e2+kEchN)%kEchN;
                float dL=fEchL[r1], dR=fEchR[r2];
                fEDpL+=0.4f*(dL-fEDpL); fEDpR+=0.4f*(dR-fEDpR);
                fEchL[fEchW]=L+fEDpR*eFb; fEchR[fEchW]=R+fEDpL*eFb;
                fEchW=(fEchW+1)%kEchN;
                L=L*(1.f-eWet*0.35f)+fEDpL*eWet*0.55f;
                R=R*(1.f-eWet*0.35f)+fEDpR*eWet*0.55f;
            }

            // 5) DARK: toon-demper (band-verlies van oude tape)
            fDarkL+=darkA*(L-fDarkL); fDarkR+=darkA*(R-fDarkR);
            L=fDarkL; R=fDarkR;

            // veiligheids-tanh
            if(L>0.98f)L=0.98f; else if(L<-0.98f)L=-0.98f;
            if(R>0.98f)R=0.98f; else if(R<-0.98f)R=-0.98f;

            outputs[0][i]=L; outputs[1][i]=R;
            if(++fDecim>=3){fDecim=0;if(fScopeN<40)fScope[fScopeN++]=(L+R)*0.5f;}
        }
        if(fScopeN>=40){
            char buf[256];int pos=0;
            for(uint32_t s=0;s<fScopeN&&pos<230;++s)pos+=snprintf(buf+pos,sizeof(buf)-pos,"%.3f;",fScope[s]);
            buf[pos]=0;setState("scope",buf);fScopeN=0;
        }
    }
private:
    void rollDice(){
        uint32_t s=fRngState+1; auto rnd=[&](){s^=s<<13;s^=s>>17;s^=s<<5;return (s&0xFFFFFFu)/16777216.f;};
        setParameterValue(kPomp,0.2f+rnd()*0.7f);setParameterValue(kCurve,(float)(int)(rnd()*16.f));setParameterValue(kRate,(float)(int)(rnd()*4.f));setParameterValue(kWow,rnd());setParameterValue(kTape,rnd());setParameterValue(kStof,rnd()*0.7f);
        setParameterValue(kEcho,rnd());setParameterValue(kDark,rnd());setParameterValue(kAge,rnd());
        setParameterValue(kGalaxy,(float)(int)(rnd()*27.f));
    }
    void applyPreset(int pi){
        if(pi<0)pi=0;if(pi>15)pi=15;fPreset=(float)pi;
        setParameterValue(kPomp,kPresetParams[pi][0]);setParameterValue(kCurve,kPresetParams[pi][1]);setParameterValue(kRate,kPresetParams[pi][2]);
        setParameterValue(kWow,kPresetParams[pi][3]);setParameterValue(kTape,kPresetParams[pi][4]);setParameterValue(kStof,kPresetParams[pi][5]);
        setParameterValue(kEcho,kPresetParams[pi][6]);setParameterValue(kDark,kPresetParams[pi][7]);setParameterValue(kAge,kPresetParams[pi][8]);
        setParameterValue(kGalaxy,kPresetParams[pi][9]);setParameterValue(kKracht,kPresetParams[pi][10]);
    }
    float fPomp=0.40f,fCurve=1.f,fRate=0.f,fWow=0.30f,fTape=0.35f,fStof=0.20f,fEcho=0.25f,fDark=0.30f,fAge=0.30f;
    float fGalaxy=9.f,fDice=0.f,fPreset=0.f,fOn=1.f,fKr=100.f;
    Galaxy galaxy;
    // wow-delay (25ms max)
    static const int kWowN=2200; float fWowL[kWowN],fWowR[kWowN]; int fWowW=0; float fWowPh=0.f,fFluPh=0.f;
    // echo-delay (1.2s max)
    static const int kEchN=58000; float fEchL[kEchN],fEchR[kEchN]; int fEchW=0; float fEDpL=0,fEDpR=0;
    float fDarkL=0,fDarkR=0,fHissL=0,fHissR=0,fCrkEnv=0;
    uint32_t fRngState; float fPumpPh=0.f,fBpm=120.f;
    float fLvlIn=0.f,fLvlOut=0.f,fMakeup=1.f;
    float fScope[40]; int fScopeN=0; int fDecim=0;
};

Plugin* createPlugin() { return new VibePlugin(); }
END_NAMESPACE_DISTRHO
