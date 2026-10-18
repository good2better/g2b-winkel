#include "DistrhoPlugin.hpp"
extern "C" {
#include "vendor/ebur128.h"
}
#include <cstring>
#include <cmath>
#include <cstdint>
#include <algorithm>

START_NAMESPACE_DISTRHO

static const char* kPresetNames[17] = {"G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"};
// SUB,POETS,KNIP,GLANS,RUIMTE,OUT(dB) — 0..1 behalve OUT
static const float kPresetParams[17][6] = {
    {0.55f,0.35f,0.40f,0.30f,0.30f, 0.f},  // G2B
    {0.60f,0.40f,0.45f,0.25f,0.22f, 0.f},  // DEMBOW
    {0.65f,0.45f,0.55f,0.15f,0.18f, 0.f},  // DRILL
    {0.45f,0.30f,0.30f,0.40f,0.35f, 0.f},  // RNB
    {0.50f,0.38f,0.42f,0.35f,0.28f, 0.f},  // HOUSE
    {0.55f,0.36f,0.40f,0.30f,0.26f, 0.f},  // AFRO
    {0.62f,0.42f,0.50f,0.22f,0.20f, 0.f},  // TRAP
    {0.50f,0.40f,0.35f,0.25f,0.28f, 0.f},  // BOOM
    {0.52f,0.38f,0.44f,0.36f,0.30f, 0.f},  // UKG
    {0.58f,0.40f,0.46f,0.28f,0.24f, 0.f},  // JERSEY
    {0.55f,0.38f,0.42f,0.32f,0.27f, 0.f},  // BOUNCE
    {0.42f,0.32f,0.28f,0.42f,0.36f, 0.f},  // SOUL
    {0.48f,0.50f,0.30f,0.20f,0.26f, 0.f},  // LOFI
    {0.50f,0.36f,0.44f,0.34f,0.28f, 0.f},  // FUNK
    {0.56f,0.40f,0.36f,0.30f,0.30f, 0.f},  // REGGAE
    {0.52f,0.36f,0.44f,0.38f,0.28f, 0.f},  // DANCE
    {0.55f,0.37f,0.41f,0.31f,0.27f, 0.f},  // KAWINA

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

class MasterRackPlugin : public Plugin {
public:
    enum Params { kSub, kPoets, kKnip, kGlans, kRuimte, kOutput, kListen, kGalaxy, kDice, kPreset, kOn, kKracht, kLo, kMid, kHi, kNumParams };
    MasterRackPlugin() : Plugin(kNumParams, 0, 2) {
        memset(fBufL,0,sizeof(fBufL)); memset(fBufR,0,sizeof(fBufR));
        memset(fPreL,0,sizeof(fPreL)); memset(fPreR,0,sizeof(fPreR));
    }
    ~MasterRackPlugin() override { if(fEbur) ebur128_destroy(&fEbur); }
    void deactivate() override { if(fEbur){ebur128_destroy(&fEbur);fEbur=nullptr;} }
protected:
    const char* getLabel() const override { return "G2B FX"; }
    const char* getDescription() const override { return "Menger + complete keten: EQ + sub+vet+luid+air+ruimte — alle effecten in één"; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','F','x'); }

    void initParameter(uint32_t i, Parameter& p) override {
        if(i==kGalaxy){p.name="GALAXY";p.symbol="galaxy";p.ranges.min=0;p.ranges.max=26;p.ranges.def=9;p.hints|=kParameterIsInteger;return;}
        p.hints=kParameterIsAutomatable;
        switch(i){
        case kSub:    p.name="SUB";     p.symbol="sub";     p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.55f;p.unit="%";break;
        case kPoets:  p.name="POETS";   p.symbol="poets";   p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.35f;p.unit="%";break;
        case kKnip:   p.name="KNIP";    p.symbol="knip";    p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.40f;p.unit="%";break;
        case kGlans:  p.name="GLANS";   p.symbol="glans";   p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.30f;p.unit="%";break;
        case kRuimte: p.name="RUIMTE";  p.symbol="ruimte";  p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.30f;p.unit="%";break;
        case kOutput: p.name="OUTPUT";  p.symbol="output";  p.ranges.min=-24.f;p.ranges.max=12.f;p.ranges.def=0.f;p.unit="dB";break;
        case kListen: p.name="LUISTER"; p.symbol="listen";  p.hints|=kParameterIsInteger|kParameterIsTrigger;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.f;break;
        case kDice:   p.name="DICE";    p.symbol="dice";    p.hints|=kParameterIsInteger|kParameterIsTrigger;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.f;break;
        case kPreset: p.name="PRESET";  p.symbol="preset";  p.hints|=kParameterIsInteger; p.ranges.min=0.f;p.ranges.max=16.f;p.ranges.def=0.f;break;
        case kOn: p.name="Aan"; p.symbol="aan"; p.hints|=kParameterIsBoolean; p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=1.f;break;
        case kKracht: p.name="Kracht"; p.symbol="kracht"; p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=100.f;p.unit="%";break;
        case kLo:  p.name="LO";  p.symbol="lo";  p.ranges.min=-12.f;p.ranges.max=12.f;p.ranges.def=0.f;p.unit="dB";break;
        case kMid: p.name="MID"; p.symbol="mid"; p.ranges.min=-12.f;p.ranges.max=12.f;p.ranges.def=0.f;p.unit="dB";break;
        case kHi:  p.name="HI";  p.symbol="hi";  p.ranges.min=-12.f;p.ranges.max=12.f;p.ranges.def=0.f;p.unit="dB";break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        if(i==kGalaxy)return fGalaxy;
        switch(i){case kSub:return fSub;case kPoets:return fPoets;case kKnip:return fKnip;case kGlans:return fGlans;case kRuimte:return fRuimte;case kOutput:return fOut;case kListen:return fListen;case kDice:return fDice;case kPreset:return fPreset;case kOn:return fOn;case kKracht:return fKr;case kLo:return fLo;case kMid:return fMid;case kHi:return fHi;}return 0.f;
    }
    void setParameterValue(uint32_t i, float v) override {
        if(i==kGalaxy){fGalaxy=v;galaxy.setPos((int)v);return;}
        if(i==kPreset){applyPreset((int)(v+0.5f));return;}
        switch(i){case kSub:fSub=v;break;case kPoets:fPoets=v;break;case kKnip:fKnip=v;break;case kGlans:fGlans=v;break;case kRuimte:fRuimte=v;break;case kOutput:fOut=v;break;case kListen:
            fListen=v;
            if(v>0.9f){fListenN=(uint32_t)(2.f*getSampleRate());fAccRms=fAccLow=fAccAir=0;fPeak=0;}
            fListen=0.f;break;
            case kDice:fDice=v;if(v>0.5f)rollDice();fDice=0.f;break;
            case kOn:fOn=v;break;case kKracht:fKr=v;break;case kLo:fLo=v;break;case kMid:fMid=v;break;case kHi:fHi=v;break;}
    }
    void initState(uint32_t index, String& name, String& def) override {
        if(index==0){name="scope";def="";}
        else{name="presetnames";def="";for(int i=0;i<17;i++){def+=kPresetNames[i];if(i<16)def+=";";}}
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
        const float k=fKr/100.f;
        if(k<0.999f){const float d=1.f-k;for(uint32_t i=0;i<frames;i++){outputs[0][i]=inputs[0][i]*d+outputs[0][i]*k;outputs[1][i]=inputs[1][i]*d+outputs[1][i]*k;}}
        // ---- LUFS-meting op de output (EBU R128, libebur128 MIT) ----
        if(!fEbur) fEbur=ebur128_init(2,(unsigned long)getSampleRate(),EBUR128_MODE_M|EBUR128_MODE_I);
        if(fEbur){
            uint32_t off=0;
            while(off<frames){
                const uint32_t n=frames-off<256?frames-off:256;
                for(uint32_t i=0;i<n;i++){fInter[2*i]=outputs[0][off+i];fInter[2*i+1]=outputs[1][off+i];}
                ebur128_add_frames_float(fEbur,fInter,n);
                off+=n;
            }
        }
    }

    void processFx_(const float** inputs, float** outputs, uint32_t frames) {
        const float sr=getSampleRate();
        const float srInv=1.f/sr;
        // ---- voorberekeningen per module ----
        // SUB: lowpass-meting onder ~120Hz + tame-gain (0 = uit)
        const float subLpA=1.f-expf(-2.f*M_PI*140.f*srInv);
        // POETS: body-lift (laag-shelf ~200Hz), punch-volg-transient, sizzle-air ~10k
        const float bodyA=1.f-expf(-2.f*M_PI*200.f*srInv);
        const float airA =1.f-expf(-2.f*M_PI*9500.f*srInv);
        // KNIP: drive + plafond −1dB
        const float drive=1.f+fKnip*6.f, ceil=0.89f;
        // GLANS: air-boost dB = glans*6, breedte-factor op hoog
        const float airDb=fGlans*6.f, airGain=powf(10.f,airDb*0.05f);
        const float wGain=fGlans*0.6f;             // breedte op hoog
        // RUIMTE: delay+reverb (zelfde als Ruimte-plugin, vaste smaak)
        const int delSamps=(int)(0.27f*sr); const int preS=(int)(0.012f*sr);
        const float fb=fRuimte*0.72f, wet=fRuimte, wMix=0.35f;
        const float dmpA=1.f-expf(-2.f*M_PI*6000.f*srInv);
        const float outG=powf(10.f,fOut*0.05f);
        // menger-EQ: 3 banden (laag <200Hz · mid · hoog >6kHz)
        const float eqLoA=1.f-expf(-2.f*M_PI*200.f*srInv);
        const float eqHiA=1.f-expf(-2.f*M_PI*6000.f*srInv);
        const float gLo=powf(10.f,fLo*0.05f), gMid=powf(10.f,fMid*0.05f), gHi=powf(10.f,fHi*0.05f);

        for(uint32_t i=0;i<frames;i++){
            float L=inputs[0][i], R=inputs[1][i];

            // 0) MENGER-EQ: 3-bands voor alles
            fEqLoL+=eqLoA*(L-fEqLoL); fEqLoR+=eqLoA*(R-fEqLoR);
            fEqHiL+=eqHiA*(L-fEqHiL); fEqHiR+=eqHiA*(R-fEqHiR);
            {
                float loL=fEqLoL, loR=fEqLoR;
                float hiL=L-fEqHiL, hiR=R-fEqHiR;
                float midL=L-loL-hiL, midR=R-loR-hiR;
                L=loL*gLo+midL*gMid+hiL*gHi;
                R=loR*gLo+midR*gMid+hiR*gHi;
            }

            // LUISTER-analyse op input (lp250 + hp4k + rms)
            if(fListenN>0){
                fALpL+=(1.f-expf(-2.f*M_PI*250.f*srInv))*(L-fALpL);
                fAHpL+=(1.f-expf(-2.f*M_PI*4000.f*srInv))*(L-fAHpL);
                fAccRms+=fabsf(L);
                fAccLow+=fabsf(fALpL);
                fAccAir+=fabsf(L-fAHpL);
                float ax=fabsf(L);if(ax>fPeak)fPeak=ax;
                if(--fListenN==0) listenApply();
            }

            // 1) SUBTAMER: meet sub-energie, verzwak (damping)
            fSubLpL+=subLpA*(L-fSubLpL); fSubLpR+=subLpA*(R-fSubLpR);
            const float tame=1.f-fSub*0.5f;
            L-=fSubLpL*fSub*0.5f; R-=fSubLpR*fSub*0.5f;   // sub wordt rustiger
            // mono onder 120Hz: sub-deel naar midden
            float sM=(fSubLpL+fSubLpR)*0.5f;
            float sL=fSubLpL*tame+sM*fSub, sR=fSubLpR*tame+sM*fSub;
            L=L+fSubLpL-sL; R=R+fSubLpR-sR;

            // 2) POETS: punch(volg transient)+body(laag)+sizzle(air)
            float envL=fabsf(L), envR=fabsf(R);
            fEnvL+=0.01f*(envL-fEnvL); fEnvR+=0.01f*(envR-fEnvR);
            float trL=envL-fEnvL, trR=envR-fEnvR;          // transient-verschil
            fBodyL+=bodyA*(L-fBodyL); fBodyR+=bodyA*(R-fBodyR);
            fAirL+=airA*(L-fAirL);   fAirR+=airA*(R-fAirR);
            float pL=L + trL*fPoets*1.5f + fBodyL*fPoets*0.4f + (L-fAirL)*fPoets*0.25f;
            float pR=R + trR*fPoets*1.5f + fBodyR*fPoets*0.4f + (R-fAirR)*fPoets*0.25f;
            L=pL; R=pR;

            // tussen-begrenzer: vangt pieken na elke stage (altijd goed)
            L=tanhf(L);R=tanhf(R);

            // 3) KNIP: tanh-clipper met plafond
            L=clipAt(L*drive,ceil); R=clipAt(R*drive,ceil);

            // 4) GLANS: air-boost op hoog + breedte op hoog
            fGairL+=airA*(L-fGairL); fGairR+=airA*(R-fGairR);
            float hiL=L-fGairL, hiR=R-fGairR;
            L=fGairL+hiL*airGain; R=fGairR+hiR*airGain;
            float hiM=(hiL+hiR)*0.5f, hiS=(hiL-hiR)*0.5f*(1.f+wGain);
            hiL=hiM+hiS; hiR=hiM-hiS;
            L+=hiL*(airGain-1.f)*0.0f; // breedte loopt via hiS hierboven al

            L=tanhf(L);R=tanhf(R);   // stage-veiligheid 2

            // 5) RUIMTE: delay+reverb-chain (vaste smaak uit de Ruimte-plugin)
            fPreL[fPreW]=L; fPreR[fPreW]=R;
            int prI=(fPreW-preS+kMaxPre)%kMaxPre;
            float pPL=fPreL[prI], pPR=fPreR[prI]; fPreW=(fPreW+1)%kMaxPre;
            int r1=(fDelW-delSamps+kMaxDel)%kMaxDel, r2=(fDelW-delSamps*61/100+kMaxDel)%kMaxDel;
            float dL=fBufL[r1]+0.5f*fBufL[r2], dR=fBufR[r1]+0.5f*fBufR[r2];
            fDLpL+=dmpA*(dL-fDLpL); fDLpR+=dmpA*(dR-fDLpR);
            fBufL[fDelW]=pPL+fDLpL*fb; fBufR[fDelW]=pPR+fDLpR*fb; fDelW=(fDelW+1)%kMaxDel;
            float mW=(dL+dR)*0.5f, sW=(dL-dR)*0.5f*1.3f;
            L=(L*(1.f-wet*wMix)+(mW+sW)*wet)*outG;
            R=(R*(1.f-wet*wMix)+(mW-sW)*wet)*outG;

            outputs[0][i]=L; outputs[1][i]=R;
            if(++fDecim>=3){fDecim=0;if(fScopeN<40)fScope[fScopeN++]=(L+R)*0.5f;}
        }
        if(fScopeN>=40){
            char buf[256];int pos=0;
            for(uint32_t s=0;s<fScopeN&&pos<230;++s)pos+=snprintf(buf+pos,sizeof(buf)-pos,"%.3f;",fScope[s]);
            buf[pos]=0;setState("scope",buf);fScopeN=0;
            if(fEbur){
                double m=-70.0,g=-70.0;
                ebur128_loudness_momentary(fEbur,&m);
                ebur128_loudness_global(fEbur,&g);
                if(m<-69.9f)m=-99.0; if(g<-69.9f)g=-99.0;
                char lb[48];snprintf(lb,sizeof(lb),"%.1f;%.1f",m,g);
                setState("lufs",lb);
            }
        }
    }
private:
    void listenApply(){
        // stijl-doelen: hoeveel laag/air mag de stijl hebben {lowShare, airShare}
        static const float kTgt[6][2]={
            {0.35f,0.12f}, // G2B
            {0.45f,0.09f}, // DEMBOW (dikke sub hoort)
            {0.42f,0.10f}, // DRILL
            {0.32f,0.14f}, // RNB
            {0.38f,0.12f}, // HOUSE
            {0.37f,0.12f}  // AFRO
        };
        int pi=(int)(fPreset+0.5f);if(pi<0)pi=0;if(pi>5)pi=5;
        float tot=fAccRms+1e-9f;
        float lowShare=fAccLow/tot, airShare=fAccAir/tot, rms=fAccRms/(2.f*getSampleRate());
        float dSub=(lowShare-kTgt[pi][0])*2.5f, dGla=(kTgt[pi][1]-airShare)*5.f, dKni=(0.12f-rms)*3.f;
        float amax=fmaxf(fabsf(dSub),fmaxf(fabsf(dGla),fabsf(dKni)));
        if(amax<0.15f){
            setParameterValue(kListen,0.5f); // AL GOED
        }else{
            setParameterValue(kSub,  fminf(0.8f,fmaxf(0.2f,0.5f+dSub)));
            setParameterValue(kGlans,fminf(0.8f,fmaxf(0.2f,0.5f+dGla)));
            setParameterValue(kKnip, fminf(0.7f,fmaxf(0.15f,0.4f+dKni)));
            setParameterValue(kPoets,0.35f);
            setParameterValue(kRuimte,0.20f);
            setParameterValue(kListen,0.35f);
        }
        if(fPeak>0.9f){ setParameterValue(kOutput,-3.f); setParameterValue(kListen,0.75f); }
        fPeak=0.f;
    }
    void rollDice(){
        for(int p=kSub;p<=kRuimte;p++){
            fRnd=fRnd*1664525u+1013904223u;
            setParameterValue(p,((fRnd>>8)%100)/100.f);
        }
    }
    float fListen=0.f,fDice=0.f;
    uint32_t fListenN=0; float fAccRms=0,fAccLow=0,fAccAir=0,fALpL=0,fAHpL=0,fPeak=0;
    unsigned fRnd=777777;
    static inline float clipAt(float x,float c){
        const float th=c*0.9f;
        if(x>th) return c*tanhf((x-th)/(c-th))*1.f+th;   // zachte knie boven 90%
        if(x<-th) return -c*tanhf((-x-th)/(c-th))*1.f-th;
        return x;
    }
    void applyPreset(int i){
        if(i<0)i=0;if(i>5)i=5;fPreset=(float)i;
        setParameterValue(kSub,kPresetParams[i][0]);
        setParameterValue(kPoets,kPresetParams[i][1]);
        setParameterValue(kKnip,kPresetParams[i][2]);
        setParameterValue(kGlans,kPresetParams[i][3]);
        setParameterValue(kRuimte,kPresetParams[i][4]);
        setParameterValue(kOutput,kPresetParams[i][5]);
    }
    float fSub=0.55f,fPoets=0.35f,fKnip=0.40f,fGlans=0.30f,fRuimte=0.30f,fOut=0.f,fPreset=0.f;
    float fLo=0.f,fMid=0.f,fHi=0.f;
    float fSubLpL=0,fSubLpR=0,fEnvL=0,fEnvR=0,fBodyL=0,fBodyR=0,fAirL=0,fAirR=0,fGairL=0,fGairR=0;
    static const int kMaxDel=96000*2, kMaxPre=4800;
    float fBufL[kMaxDel],fBufR[kMaxDel],fPreL[kMaxPre],fPreR[kMaxPre];
    float fDLpL=0,fDLpR=0;int fDelW=0,fPreW=0;
    float fEqLoL=0,fEqLoR=0,fEqHiL=0,fEqHiR=0;
    float fScope[40];uint32_t fScopeN=0;int fDecim=0;
    ebur128_state* fEbur;
    float fInter[512];
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterRackPlugin)
    float fOn=1.f,fKr=100.f,fGalaxy=9.f;
    Galaxy galaxy;
};
Plugin* createPlugin(){return new MasterRackPlugin();}
END_NAMESPACE_DISTRHO
