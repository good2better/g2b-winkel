/*
 * G2B MIXER v2 — slimme channel-strip + sporen die naar elkaar luisteren.
 * Keten: TRIM → HP → tilt-EQ → glue-comp → warmte → level-meter.
 * MAAK MIX-KLAAR: luistert 5s → kalibreert naar -12 dBFS RMS + zet TILT/GLUE
 * zelf goed op basis van het gekozen instrument-profiel.
 * v2 RUIMTE: elke instantie deelt zijn 8-band spectrum via een gedeelde bus;
 * waar een ander spoor dezelfde band domineert, duikt dit spoor daar automatisch
 * (ontmaskeren — zoals smart:EQ/true:level van Sonible).
 */
#include "DistrhoPlugin.hpp"
extern "C" {
#include "vendor/ebur128.h"
}
#include <cstring>
#include <cmath>
#include <cstdint>
#include <algorithm>
#if defined(DISTRHO_OS_WINDOWS)
# include <windows.h>
#else
# include <fcntl.h>
# include <sys/mman.h>
# include <sys/stat.h>
# include <unistd.h>
#endif
#include <time.h>

// ---- gedeelde bus (gedeeld met g2b_master via ../g2b_bus.h) ------------------
#include "../g2b_bus.h"

START_NAMESPACE_DISTRHO

// 6 instrument-profielen — elke rij: HP(Hz) TILT(-50..50) GLUE(0..100) WARMTE(0..100)
static const char* kProfNames[6] = {"DRUMS","808/BASS","KEYS","MELODIE","PERC","FX/LOOP"};
static const float kProfParams[6][4] = {
    { 40.f,   0.f, 55.f, 35.f},  // DRUMS   — punch vasthouden
    { 20.f, -15.f, 65.f, 45.f},  // 808/BASS — diep, warm, geperst
    { 60.f,  +5.f, 40.f, 30.f},  // KEYS    — schoon en open
    { 80.f, +10.f, 45.f, 25.f},  // MELODIE — helder, vooraan
    {100.f, +15.f, 35.f, 20.f},  // PERC    — hoogfrequent, los
    { 50.f,   0.f, 50.f, 40.f},  // FX/LOOP — neutraal lijmen
};

class MixerPlugin : public Plugin {
public:
    enum Params { kTrim, kHp, kTilt, kGlue, kWarmte, kLuid,
                  kDice, kProf, kOn, kKracht, kMixKlaar,
                  kLufsM, kLufsI, kLevel, kGainOut, kBusy,
                  kRuimte, kBusN, kPrio,
                  kNumParams };
    MixerPlugin() : Plugin(kNumParams, 0, 0) { applyProf(0); }
    ~MixerPlugin() override { if(fEbur) ebur128_destroy(&fEbur); busLeave(); }
    void deactivate() override { if(fEbur){ebur128_destroy(&fEbur);fEbur=nullptr;} busLeave(); }
protected:
    const char* getLabel() const override { return "G2B Mixer v2"; }
    const char* getDescription() const override {
        return "Slimme channel-strip: zet hem op een spoor, kies het instrument en druk MAAK MIX-KLAAR — hij luistert 5 sec en kalibreert naar -12 dB."; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(2,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','M','x'); }

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints=kParameterIsAutomatable;
        switch(i){
        case kTrim:   p.name="TRIM";   p.symbol="trim";   p.ranges.min=-24.f;p.ranges.max=24.f;p.ranges.def=0.f;p.unit="dB";break;
        case kHp:     p.name="HP";     p.symbol="hp";     p.ranges.min=20.f;p.ranges.max=400.f;p.ranges.def=40.f;p.unit="Hz";break;
        case kTilt:   p.name="TILT";   p.symbol="tilt";   p.ranges.min=-50.f;p.ranges.max=50.f;p.ranges.def=0.f;p.unit="%";break;
        case kGlue:   p.name="GLUE";   p.symbol="glue";   p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=50.f;p.unit="%";break;
        case kWarmte: p.name="WARMTE"; p.symbol="warmte"; p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=30.f;p.unit="%";break;
        case kLuid:   p.name="LUID";   p.symbol="luid";   p.ranges.min=-24.f;p.ranges.max=24.f;p.ranges.def=0.f;p.unit="dB";break;
        case kDice:   p.name="DICE";   p.symbol="dice";   p.hints|=kParameterIsBoolean;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.f;break;
        case kProf:   p.name="PROFIEL";p.symbol="prof";   p.hints|=kParameterIsInteger;p.ranges.min=0.f;p.ranges.max=5.f;p.ranges.def=0.f;break;
        case kOn:     p.name="Aan";    p.symbol="aan";    p.hints|=kParameterIsBoolean;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=1.f;break;
        case kKracht: p.name="Kracht"; p.symbol="kracht"; p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=100.f;p.unit="%";break;
        case kMixKlaar:p.name="MAAK MIX-KLAAR"; p.symbol="mixklaar"; p.hints|=kParameterIsBoolean;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.f;break;
        case kLufsM:  p.name="LUFS_M"; p.symbol="lufs_m"; p.hints=kParameterIsOutput;p.ranges.min=-70.f;p.ranges.max=0.f;p.ranges.def=-99.f;break;
        case kLufsI:  p.name="LUFS_I"; p.symbol="lufs_i"; p.hints=kParameterIsOutput;p.ranges.min=-70.f;p.ranges.max=0.f;p.ranges.def=-99.f;break;
        case kLevel:  p.name="LEVEL";  p.symbol="level";  p.hints=kParameterIsOutput;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.f;break;
        case kGainOut:p.name="GR";     p.symbol="gr";     p.hints=kParameterIsOutput;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=1.f;break;
        case kBusy:   p.name="BUSY";   p.symbol="busy";   p.hints=kParameterIsOutput;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.f;break;
        case kRuimte: p.name="RUIMTE"; p.symbol="ruimte"; p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=60.f;p.unit="%";break;
        case kBusN:   p.name="BUSN";   p.symbol="busn";   p.hints=kParameterIsOutput;p.ranges.min=0.f;p.ranges.max=32.f;p.ranges.def=0.f;break;
        case kPrio:   p.name="PRIO";   p.symbol="prio";   p.hints|=kParameterIsInteger;p.ranges.min=0.f;p.ranges.max=2.f;p.ranges.def=1.f;break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){case kTrim:return fTrim;case kHp:return fHp;case kTilt:return fTilt;
                  case kGlue:return fGlue;case kWarmte:return fWarmte;case kLuid:return fLuid;
                  case kDice:return 0.f;case kProf:return fProf;
                  case kOn:return fOn;case kKracht:return fKr;case kMixKlaar:return 0.f;
                  case kLufsM:return fLufsM;case kLufsI:return fLufsI;case kLevel:return fLevel;
                  case kGainOut:return fGain;case kBusy:return fAutoN>0?1.f:0.f;
                  case kRuimte:return fRuimte;case kBusN:return (float)fBusPeers;
                  case kPrio:return fPrio;}return 0.f;
    }
    void setParameterValue(uint32_t i, float v) override {
        switch(i){
        case kTrim:fTrim=v;break;case kHp:fHp=v;break;case kTilt:fTilt=v;break;
        case kGlue:fGlue=v;break;case kWarmte:fWarmte=v;break;case kLuid:fLuid=v;break;
        case kDice:if(v>0.5f)rollDice();break;
        case kProf:applyProf((int)(v+0.5f));break;
        case kOn:fOn=v;break;case kKracht:fKr=v;break;
        case kMixKlaar:if(v>0.5f)startListen();break;
        case kRuimte:fRuimte=v;break;
        case kPrio:fPrio=v;if(fPrio<0.f)fPrio=0.f;if(fPrio>2.f)fPrio=2.f;break;}
    }

    void initState(uint32_t, String&, String&) override {}
    void setState(const char* k, const char* v) override {(void)k;(void)v;}

    void activate() override { fEnv=1e-6f; fGain=1.f; fHpL=fHpR=0.f;
        fTiltLpL=fTiltLpR=0.f; fSatLpL=fSatLpR=0.f;
        std::memset(fLpL,0,sizeof(fLpL)); std::memset(fLpR,0,sizeof(fLpR));
        std::memset(fOwnE,0,sizeof(fOwnE)); std::memset(fAcc,0,sizeof(fAcc));
        for(int b=0;b<kBusBands;b++){fGS[b]=1.f;fGT[b]=1.f;fSS[b]=1.f;fST[b]=1.f;}
        busJoin(); }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        if(fOn<0.5f){
            if(inputs[0]!=outputs[0])std::memcpy(outputs[0],inputs[0],frames*sizeof(float));
            if(inputs[1]!=outputs[1])std::memcpy(outputs[1],inputs[1],frames*sizeof(float));
            return;
        }
        const float sr=getSampleRate();
        const float srInv=1.f/sr;

        const float hpA=1.f-expf(-2.f*M_PI*fHp*srInv);
        const float tiltA=1.f-expf(-2.f*M_PI*700.f*srInv);
        const float t=fTilt/50.f;
        const float gLoT=powf(10.f,( t*7.f)*0.05f);
        const float gHiT=powf(10.f,(-t*7.f)*0.05f);
        const float thr=0.126f;
        const float ratio=1.f+(fGlue/100.f)*3.f;
        const float cAtt=1.f-expf(-1.f/(0.010f*sr));
        const float cRel=1.f-expf(-1.f/(0.160f*sr));
        const float gAtt=1.f-expf(-1.f/(0.004f*sr));
        const float gRel=1.f-expf(-1.f/(0.200f*sr));
        const float drv=1.f+(fWarmte/100.f)*4.f;
        const float satMix=fWarmte/100.f*0.6f;
        const float satLpA=1.f-expf(-2.f*M_PI*8000.f*srInv);
        const float trimG=powf(10.f,fTrim*0.05f);
        const float outG=powf(10.f,fLuid*0.05f);

        const bool listening=(fAutoN>0);
        for(uint32_t i=0;i<frames;i++){
            float L=inputs[0][i]*trimG, R=inputs[1][i]*trimG;

            if(listening){
                const float lo=(fTiltLpL+fTiltLpR)*0.5f;
                fAElo+=lo*lo;
                const float hiL=L-fTiltLpL, hiR=R-fTiltLpR;
                fAEhi+=(hiL*hiL+hiR*hiR)*0.5f;
                fAEall+=(L*L+R*R)*0.5f;
                const float pk=fmaxf(fabsf(L),fabsf(R));
                if(pk>fAPk)fAPk=pk; fAN++;
            }

            fHpL+=hpA*(L-fHpL); fHpR+=hpA*(R-fHpR);
            L-=fHpL; R-=fHpR;

            fTiltLpL+=tiltA*(L-fTiltLpL); fTiltLpR+=tiltA*(R-fTiltLpR);
            { const float loL=fTiltLpL,loR=fTiltLpR,hiL=L-loL,hiR=R-loR;
              L=loL*gLoT+hiL*gHiT; R=loR*gLoT+hiR*gHiT; }

            const float m2=(L*L+R*R)*0.5f;
            fEnv += ((m2>fEnv)?cAtt:cRel)*(m2+1e-5f-fEnv);
            {
                const float rms=sqrtf(fEnv);
                if(rms>thr){
                    const float gr=rms/thr;
                    const float target=powf(gr,-(1.f-1.f/ratio));
                    fGain += ((target<fGain)?gAtt:gRel)*(target-fGain);
                }else fGain += gRel*(1.f-fGain);
                L*=fGain; R*=fGain;
            }

            // v2: 8-band decompositie + ontmasker-ducking op de bus
            {
                float xl=L, xr=R, recL=0.f, recR=0.f, prevL=fLpL[0], prevR=fLpR[0];
                for(int b=0;b<7;b++){
                    fLpL[b]+=fBA[b]*(xl-fLpL[b]); fLpR[b]+=fBA[b]*(xr-fLpR[b]);
                    const float bl=(b==0?fLpL[0]:fLpL[b]-prevL), br=(b==0?fLpR[0]:fLpR[b]-prevR);
                    // anti-zwiep: wijken snel (~20ms), terugkeren traag (~300ms)
                    fGS[b]+=(fGT[b]<fGS[b]?fGA[b]:fGR[b])*(fGT[b]-fGS[b]);
                    fSS[b]+=(fST[b]>fSS[b]?fGA[b]:fGR[b])*(fST[b]-fSS[b]);
                    // M/S: duk het midden, vlucht naar de zijkant — mix blijft breed
                    const float m=(bl+br)*0.5f, s=(bl-br)*0.5f;
                    recL+=m*fGS[b]+s*fSS[b]; recR+=m*fGS[b]-s*fSS[b];
                    fAcc[b]+=(bl*bl+br*br)*0.5f;
                    prevL=fLpL[b]; prevR=fLpR[b];
                }
                { const float b7l=xl-fLpL[6], b7r=xr-fLpR[6];
                  fGS[7]+=(fGT[7]<fGS[7]?fGA[7]:fGR[7])*(fGT[7]-fGS[7]); fSS[7]+=(fST[7]>fSS[7]?fGA[7]:fGR[7])*(fST[7]-fSS[7]);
                  const float m=(b7l+b7r)*0.5f, s=(b7l-b7r)*0.5f;
                  recL+=m*fGS[7]+s*fSS[7]; recR+=m*fGS[7]-s*fSS[7];
                  fAcc[7]+=(b7l*b7l+b7r*b7r)*0.5f; }
                L=recL; R=recR;
            }

            {
                float sL=tanhf(L*drv), sR=tanhf(R*drv);
                fSatLpL+=satLpA*(sL-fSatLpL); fSatLpR+=satLpA*(sR-fSatLpR);
                L=L*(1.f-satMix)+fSatLpL*satMix;
                R=R*(1.f-satMix)+fSatLpR*satMix;
            }

            L*=outG; R*=outG;
            outputs[0][i]=L; outputs[1][i]=R;
            fCorrLR+=L*R; fCorrL+=L*L; fCorrR+=R*R;
            const float pk=fmaxf(fabsf(L),fabsf(R));
            if(pk>fLevelBlk)fLevelBlk=pk;
        }

        const float k=fKr/100.f;
        if(k<0.999f){const float d=1.f-k;
            for(uint32_t i=0;i<frames;i++){outputs[0][i]=inputs[0][i]*d+outputs[0][i]*k;
                                           outputs[1][i]=inputs[1][i]*d+outputs[1][i]*k;}}

        // bus update per blok: eigen band-energie schrijven + duck-targets zetten
        fAccN+=frames;
        if(fAccN>=512){
            const float inv=1.f/(float)fAccN;
            for(int b=0;b<kBusBands;b++){
                fOwnE[b]+=0.3f*(fAcc[b]*inv-fOwnE[b]);
                fAcc[b]=0.f;
            }
            fAccN=0;
            busUpdate();
        }

        if(!fEbur) fEbur=ebur128_init(2,(unsigned long)sr,EBUR128_MODE_M|EBUR128_MODE_I);
        if(fEbur){
            uint32_t off=0;
            while(off<frames){
                const uint32_t n=frames-off<256?frames-off:256;
                for(uint32_t i=0;i<n;i++){fInter[2*i]=outputs[0][off+i];fInter[2*i+1]=outputs[1][off+i];}
                ebur128_add_frames_float(fEbur,fInter,n);
                off+=n;
            }
            double m=-70.0,g=-70.0;
            ebur128_loudness_momentary(fEbur,&m);
            ebur128_loudness_global(fEbur,&g);
            if(m<-69.9f)m=-99.0; if(g<-69.9f)g=-99.0;
            fLufsM=(float)m;fLufsI=(float)g;
        }
        fLevel=fLevelBlk;fLevelBlk=0.f;

        if(fAutoN>0){
            fAutoN-=(int)frames;
            if(fAutoN<=0){fAutoN=0;finishListen();}
        }
    }
private:
    void startListen(){
        fAutoN=(int)(getSampleRate()*5.0);
        fAElo=fAEhi=fAEall=0.f;fAPk=0.f;fAN=0;
        fPreTrimRms=0.f;
    }
    void finishListen(){
        if(fAN<4410||fAEall<1e-8f)return;
        // gemeten RMS na huidige TRIM → TRIM bijstellen zodat uitgang ≈ -12 dBFS
        const float rms=sqrtf(fAEall/fAN);
        const float measured=20.f*log10f(rms+1e-9f);
        float need=-12.f-measured;             // dB die erbij/eraf moet
        if(need> 0.5f && need>24.f-fTrim) need=24.f-fTrim;
        if(need<-0.5f && need<-24.f-fTrim) need=-24.f-fTrim;
        fTrim+=need; if(fTrim>24.f)fTrim=24.f; if(fTrim<-24.f)fTrim=-24.f;
        // lo/hi-balans → TILT rond profiel-basis
        const float d=10.f*log10f((fAElo+1e-9f)/(fAEhi+1e-9f));
        float tilt=fTiltBase+d*4.f; if(tilt>50.f)tilt=50.f;if(tilt<-50.f)tilt=-50.f;
        fTilt=tilt;
        // dynamiek → GLUE rond profiel-basis
        const float crest=20.f*log10f((fAPk+1e-9f)/(rms+1e-9f));
        float glue=fGlueBase+(crest-6.f)*8.f;
        if(glue>95.f)glue=95.f;if(glue<10.f)glue=10.f;
        fGlue=glue;
        // auto-PRIO verfijning: piekigheid zegt meer dan het gekozen profiel
        if(crest<5.f)      fPrio=2.f;   // pad-achtig: de gever
        else if(crest>10.f) fPrio=fminf(fPrio,1.f); // percussief: nooit LAAG
    }
    void applyProf(int pi){
        if(pi<0)pi=0;if(pi>5)pi=5;
        fProf=(float)pi;
        fHp=kProfParams[pi][0]; fTiltBase=kProfParams[pi][1];
        fTilt=fTiltBase; fGlueBase=kProfParams[pi][2]; fGlue=fGlueBase;
        fWarmte=kProfParams[pi][3];
        // auto-PRIO: lead/melodie is de held (HOOG=0), fx/loop is de gever (LAAG=2)
        static const float kProfPrio[6]={1.f,1.f,1.f,0.f,1.f,2.f};
        fPrio=kProfPrio[pi];
    }
    void busJoin(){
        if(fBus) return;
        fBus=(G2BBus*)g2bBusOpen();
        if(!fBus) return;
        if(fBus->magic!=kBusMagic){ std::memset((void*)fBus,0,sizeof(G2BBus)); fBus->magic=kBusMagic; }
        // unieke id: seed + tijd + (op POSIX) pid → nooit 2 instanties hetzelfde
        fSeed=fSeed*1664525u+1013904223u;
        fSeed^=(uint32_t)time(nullptr)*2654435761u;
#if !defined(DISTRHO_OS_WINDOWS)
        fSeed^=(uint32_t)getpid()*40503u;
#else
        fSeed^=(uint32_t)GetCurrentProcessId()*40503u;
#endif
        fMyId=(fSeed&0x7FFFFFFFu)|1u;
        for(int i=0;i<kBusSlots;i++){
            uint32_t want=0;
            if(__sync_bool_compare_and_swap(&fBus->s[i].id,want,fMyId)){fBusSlot=i;break;}
        }
    }
    void busLeave(){
        if(fBus&&fBusSlot>=0){fBus->s[fBusSlot].id=0;fBusSlot=-1;}
        if(fBus){g2bBusClose(fBus);fBus=nullptr;}
    }
    void busUpdate(){
        if(!fBandInit){
            const float sr=getSampleRate();
            const float xr[7]={60.f,120.f,250.f,500.f,1000.f,2000.f,4000.f};
            for(int b=0;b<7;b++)fBA[b]=1.f-expf(-2.f*M_PI*xr[b]/sr);
            for(int b=0;b<kBusBands;b++){fGS[b]=1.f;fGT[b]=1.f;fSS[b]=1.f;fST[b]=1.f;fGA[b]=1.f-expf(-1.f/(0.020f*sr));fGR[b]=1.f-expf(-1.f/(0.300f*sr));}
            fBandInit=true;
        }
        if(!fBus||fBusSlot<0){fBusPeers=0;return;}
        fBus->s[fBusSlot].tick++;
        fBus->s[fBusSlot].prio=(uint32_t)(fPrio+0.5f);
        for(int b=0;b<kBusBands;b++)fBus->s[fBusSlot].e[b]=fOwnE[b];
        int peers=0; float other[kBusBands];
        for(int b=0;b<kBusBands;b++)other[b]=0.f;
        for(int i=0;i<kBusSlots;i++){
            if(i==fBusSlot||fBus->s[i].id==0){fPeerTick[i]=0;fPeerAge[i]=0;continue;}
            // dode instanties opruimen: tick stilstaand ~2,5s → slot vrij
            const uint64_t tk=fBus->s[i].tick;
            if(tk==fPeerTick[i]){if(++fPeerAge[i]>200){fBus->s[i].id=0;fPeerAge[i]=0;fPeerTick[i]=0;continue;}}
            else{fPeerTick[i]=tk;fPeerAge[i]=0;}
            peers++;
            // PRIO: ik duk alleen voor sporen die mínder belangrijk zijn dan mij —
            // gelijke prio = luidste wint (oud gedrag), lagere prio negeren.
            const uint32_t pp=fBus->s[i].prio;
            if(pp>(uint32_t)(fPrio+0.5f))continue;
            for(int b=0;b<kBusBands;b++)if(fBus->s[i].e[b]>other[b])other[b]=fBus->s[i].e[b];
        }
        fBusPeers=peers;
        // MASTER op de bus? dan stuurt hij top-down bij: LAAG dukit voluit,
        // MIDDEN half, HOOG negeert de master-mask volledig.
        float mScale=0.f;
        if(fBus->masterTick!=fMasterTick){fMasterTick=fBus->masterTick;fMasterAlive=200;}
        else if(fMasterAlive>0)--fMasterAlive;
        if(fMasterAlive>0){
            const int pr=(int)(fPrio+0.5f);
            mScale=(pr>=2)?1.f:(pr==1?0.4f:0.f);
        }
        // mono-waak: L/R-correlatie laag -> zijkant-boost afvlakken (mono-veilig)
        const float den=sqrtf(fCorrL*fCorrR)+1e-9f;
        const float corr=fCorrLR/den;
        fCorrL=fCorrR=fCorrLR=0.f;
        float sideOK=(corr-0.10f)/0.30f;
        if(sideOK>1.f)sideOK=1.f; if(sideOK<0.f)sideOK=0.f;
        const float amt=(fRuimte/100.f)*3.f;
        for(int b=0;b<kBusBands;b++){
            const float r=other[b]/(fOwnE[b]+1e-9f);
            float g=1.f/(1.f+amt*r);
            if(g<0.25f)g=0.25f; if(g>1.f)g=1.f;
            if(mScale>0.f){
                float md=fBus->masterMask[b]*mScale;
                if(md>0.6f)md=0.6f;
                g*=1.f-md;
            }
            fGT[b]=g;
            // M/S: de gedukte band-energie schuift naar de zijkant (mono-waak begrensd)
            float sg=1.f+(1.f-g)*0.35f*sideOK; if(sg>1.3f)sg=1.3f;
            fST[b]=sg;
        }
    }
    void rollDice(){
        uint32_t sd=fSeed=fSeed*1664525u+1013904223u;
        auto rng=[&](){sd^=sd<<13;sd^=sd>>17;sd^=sd<<5;return (sd&0xFFFFu)/65536.f;};
        fTilt=(rng()*100.f-50.f)*0.6f;
        fGlue=30.f+rng()*60.f;
        fWarmte=10.f+rng()*70.f;
        fTrim=rng()*16.f-8.f;
    }
    float fTrim=0.f,fHp=40.f,fTilt=0.f,fGlue=50.f,fWarmte=30.f,fLuid=0.f;
    float fProf=0.f,fOn=1.f,fKr=100.f;
    float fTiltBase=0.f,fGlueBase=50.f,fPreTrimRms=0.f;
    uint32_t fSeed=54321u;
    float fHpL=0.f,fHpR=0.f;
    float fTiltLpL=0.f,fTiltLpR=0.f;
    float fEnv=1e-6f,fGain=1.f;
    float fSatLpL=0.f,fSatLpR=0.f;
    float fLevel=0.f, fLevelBlk=0.f, fLufsM=-99.f, fLufsI=-99.f;
    int fAutoN=0, fAN=0; float fAElo=0.f,fAEhi=0.f,fAEall=0.f,fAPk=0.f;
    float fInter[512];
    ebur128_state* fEbur=nullptr;
    // v2: RUIMTE-bus
    float fRuimte=60.f,fPrio=1.f;
    G2BBus* fBus=nullptr; int fBusSlot=-1; uint32_t fMyId=0;
    int fBusPeers=0; int fMasterAlive=0; uint64_t fMasterTick=0;
    float fLpL[7]={},fLpR[7]={},fBA[7]={},fGS[8]={},fGT[8]={},fGA[8]={},fGR[8]={},fSS[8]={},fST[8]={};
    float fCorrL=0.f,fCorrR=0.f,fCorrLR=0.f;
    float fOwnE[kBusBands]={},fAcc[kBusBands]={}; uint32_t fAccN=0;
    uint64_t fPeerTick[kBusSlots]={}; uint32_t fPeerAge[kBusSlots]={};
    bool fBandInit=false;
};

Plugin* createPlugin() { return new MixerPlugin(); }

END_NAMESPACE_DISTRHO
