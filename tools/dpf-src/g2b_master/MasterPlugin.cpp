/*
 * G2B MASTER — mastering in 4 knoppen: TILT · GLUE · WARMTE · LUID
 * Keten: HP-ruimte-cut → tilt-EQ → glue-comp → saturatie → limiter (-1 dBTP)
 * 10 genre-presets op basis van G2B producer-kennis (Cymatics/Speizer-stijl).
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
#include "../g2b_bus.h"

START_NAMESPACE_DISTRHO

static const char* kPresetNames[10] = {
    "TRAP","DRILL","BOOMBAP","RNB","HOUSE","DEMBOW","KAWINA","LOFI","SOUL","POP"
};
// per preset: TILT(-50..50) GLUE WARMTE LUID (0..100)
static const float kPresetParams[10][4] = {
    {-10.f, 65.f, 40.f, 85.f},  // TRAP    — donker, zwaar geperst
    { -5.f, 70.f, 45.f, 85.f},  // DRILL   — hard en koud
    {-15.f, 55.f, 55.f, 75.f},  // BOOMBAP — warm, vinyl
    { +5.f, 50.f, 30.f, 70.f},  // RNB     — glad
    {+10.f, 60.f, 30.f, 80.f},  // HOUSE   — helder en punchy
    {  0.f, 65.f, 35.f, 80.f},  // DEMBOW  — strak
    { +5.f, 55.f, 30.f, 75.f},  // KAWINA  — levendig
    {-20.f, 40.f, 70.f, 55.f},  // LOFI    — donker, veel korrel
    { -5.f, 45.f, 50.f, 65.f},  // SOUL    — rond en warm
    {+10.f, 65.f, 25.f, 85.f},  // POP     — glanzend en luid
};

class MasterPlugin : public Plugin {
public:
    enum Params { kTilt, kGlue, kWarmte, kLuid, kDice, kPreset, kOn, kKracht, kAuto,
                  kLufsM, kLufsI, kLevel, kOutTilt, kOutGlue, kOutWarmte, kOutLuid, kAutoBusy, kDoel,
                  kNumParams };
    MasterPlugin() : Plugin(kNumParams, 0, 0) {
        applyPreset(0,false);
    }
    ~MasterPlugin() override { if(fEbur) ebur128_destroy(&fEbur); g2bBusClose(fBus); }
    void deactivate() override { if(fEbur){ebur128_destroy(&fEbur);fEbur=nullptr;} g2bBusClose(fBus);fBus=nullptr; }
protected:
    const char* getLabel() const override { return "G2B Master"; }
    const char* getDescription() const override {
        return "Mastering in 4 knoppen: tilt-EQ + glue-comp + warmte + limiter. Kies je genre = staat gelijk goed."; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','M','s'); }

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints=kParameterIsAutomatable;
        switch(i){
        case kTilt:   p.name="TILT";   p.symbol="tilt";   p.ranges.min=-50.f;p.ranges.max=50.f;p.ranges.def=0.f;p.unit="%";break;
        case kGlue:   p.name="GLUE";   p.symbol="glue";   p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=50.f;p.unit="%";break;
        case kWarmte: p.name="WARMTE"; p.symbol="warmte"; p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=30.f;p.unit="%";break;
        case kLuid:   p.name="LUID";   p.symbol="luid";   p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=60.f;p.unit="%";break;
        case kDice:   p.name="DICE";   p.symbol="dice";   p.hints|=kParameterIsBoolean;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.f;break;
        case kPreset: p.name="PRESET"; p.symbol="preset"; p.hints|=kParameterIsInteger;p.ranges.min=0.f;p.ranges.max=9.f;p.ranges.def=0.f;break;
        case kOn:     p.name="Aan";    p.symbol="aan";    p.hints|=kParameterIsBoolean;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=1.f;break;
        case kKracht: p.name="Kracht"; p.symbol="kracht"; p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=100.f;p.unit="%";break;
        case kAuto:   p.name="AUTO";   p.symbol="auto";   p.hints|=kParameterIsBoolean;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.f;break;
        case kLufsM:  p.name="LUFS_M"; p.symbol="lufs_m"; p.hints=kParameterIsOutput;p.ranges.min=-70.f;p.ranges.max=0.f;p.ranges.def=-99.f;break;
        case kLufsI:  p.name="LUFS_I"; p.symbol="lufs_i"; p.hints=kParameterIsOutput;p.ranges.min=-70.f;p.ranges.max=0.f;p.ranges.def=-99.f;break;
        case kLevel:  p.name="LEVEL";  p.symbol="level";  p.hints=kParameterIsOutput;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.f;break;
        case kOutTilt:  p.name="OTILT";  p.symbol="otilt";  p.hints=kParameterIsOutput;p.ranges.min=-50.f;p.ranges.max=50.f;p.ranges.def=0.f;break;
        case kOutGlue:  p.name="OGLUE";  p.symbol="oglue";  p.hints=kParameterIsOutput;p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=0.f;break;
        case kOutWarmte:p.name="OWARMTE";p.symbol="owarm";  p.hints=kParameterIsOutput;p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=0.f;break;
        case kOutLuid:  p.name="OLUID";  p.symbol="oluid";  p.hints=kParameterIsOutput;p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=0.f;break;
        case kAutoBusy: p.name="ABUSY";  p.symbol="abusy";  p.hints=kParameterIsOutput;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.f;break;
        case kDoel:    p.name="DOEL";    p.symbol="doel";    p.ranges.min=-24.f;p.ranges.max=-3.f;p.ranges.def=-6.f;p.unit="LUFS";break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){case kTilt:return fTilt;case kGlue:return fGlue;case kWarmte:return fWarmte;
                  case kLuid:return fLuid;case kDice:return 0.f;case kPreset:return fPreset;
                  case kOn:return fOn;case kKracht:return fKr;case kAuto:return 0.f;
                  case kLufsM:return fLufsM;case kLufsI:return fLufsI;case kLevel:return fLevel;
                  case kOutTilt:return fTilt;case kOutGlue:return fGlue;
                  case kOutWarmte:return fWarmte;case kOutLuid:return fLuid;
                  case kAutoBusy:return fAutoN>0?1.f:0.f;
                  case kDoel:return fDoel;}return 0.f;
    }
    void setParameterValue(uint32_t i, float v) override {
        switch(i){
        case kTilt:fTilt=v;break;case kGlue:fGlue=v;break;case kWarmte:fWarmte=v;break;
        case kLuid:fLuid=v;break;
        case kDice:if(v>0.5f)rollDice();break;
        case kPreset:applyPreset((int)(v+0.5f),true);break;
        case kOn:fOn=v;break;case kKracht:fKr=v;break;
        case kAuto:if(v>0.5f)startAuto();break;case kDoel:fDoel=v;break;}
    }

    void initState(uint32_t, String&, String&) override {}
    void setState(const char* k, const char* v) override {(void)k;(void)v;}

    void activate() override { fEnv=1e-6f; fGain=1.f; fLimEnv=0.f; fHpL=fHpR=0.f;
        fTiltLpL=fTiltLpR=0.f; fSatLpL=fSatLpR=0.f; }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        if(fOn<0.5f){
            if(inputs[0]!=outputs[0])std::memcpy(outputs[0],inputs[0],frames*sizeof(float));
            if(inputs[1]!=outputs[1])std::memcpy(outputs[1],inputs[1],frames*sizeof(float));
            return;
        }
        const float sr=getSampleRate();
        const float srInv=1.f/sr;

        // ---- coëfficiënten per block ----
        // HP-ruimte-cut ~28Hz (rumble altijd weg — zijn regel)
        const float hpA=1.f-expf(-2.f*M_PI*28.f*srInv);
        // TILT: split op 700Hz; tilt% → ±7dB laag tegen ±7dB hoog
        const float tiltA=1.f-expf(-2.f*M_PI*700.f*srInv);
        // brein v2 sensoren: stem-band + transient-envelope
        const float lp8A=1.f-expf(-2.f*M_PI*800.f*srInv);
        const float lp35A=1.f-expf(-2.f*M_PI*3500.f*srInv);
        const float trA=1.f-expf(-1.f/(0.050f*sr));     // 50ms lokale rms-env
        const float t=fTilt/50.f;                       // -1..1
        const float gLoT=powf(10.f,( t*7.f)*0.05f);
        const float gHiT=powf(10.f,(-t*7.f)*0.05f);
        // GLUE-comp: RMS-follower, threshold -18dBFS, ratio 1→4
        const float thr=0.126f;                          // ~-18 dBFS
        const float ratio=1.f+(fGlue/100.f)*3.f;
        const float cAtt=1.f-expf(-1.f/(0.010f*sr));     // 10ms
        const float cRel=1.f-expf(-1.f/(0.160f*sr));     // 160ms release
        const float gAtt=1.f-expf(-1.f/(0.004f*sr));     // gain attack 4ms
        const float gRel=1.f-expf(-1.f/(0.200f*sr));     // gain release 200ms
        // WARMTE: drive + LP op het verzadigde deel (rond, geen gruis)
        const float drv=1.f+(fWarmte/100.f)*4.f;
        const float satMix=fWarmte/100.f*0.6f;
        const float satLpA=1.f-expf(-2.f*M_PI*8000.f*srInv);
        // LUID: pre-gain 0..+9dB → peak-limiter ceiling -1dB (0.89)
        const float preG=powf(10.f,(fLuid/100.f)*9.f*0.05f);
        const float ceil=0.891f;
        const float limRel=expf(-1.f/(0.050f*sr));       // 50ms release

        const bool autoOn=(fAutoN>0);
        for(uint32_t i=0;i<frames;i++){
            float L=inputs[0][i], R=inputs[1][i];

            if(autoOn){
                // AUTO-luistert: meet lo/hi-energie + rms + piek
                const float loA=fTiltLpL*0.5f+fTiltLpR*0.5f;
                fAElo+=loA*loA; const float hiL=L-fTiltLpL,hiR=R-fTiltLpR;
                fAEhi+=(hiL*hiL+hiR*hiR)*0.5f;
                fAEall+=(L*L+R*R)*0.5f;
                const float pk=fmaxf(fabsf(L),fabsf(R));
                if(pk>fAPk)fAPk=pk; fAN++;
                // brein v2: stem-band 800-3500 + transient-dichtheid + fase
                fLp8L+=lp8A*(L-fLp8L); fLp8R+=lp8A*(R-fLp8R);
                fLp35L+=lp35A*(L-fLp35L); fLp35R+=lp35A*(R-fLp35R);
                { const float bL=fLp35L-fLp8L,bR=fLp35R-fLp8R;
                  fAEmid+=(bL*bL+bR*bR)*0.5f; }
                fALR+=L*R;
                { const float m=(L*L+R*R)*0.5f;
                  if(m>9.f*fTrEnv)fATr++;              // piek > ~9.5dB boven lokale rms
                  fTrEnv+=trA*(m-fTrEnv); }
            }

            // 0) HP 28Hz — rumble/modder onder de sub weg
            fHpL+=hpA*(L-fHpL); fHpR+=hpA*(R-fHpR);
            L-=fHpL; R-=fHpR;

            // 1) TILT-EQ
            fTiltLpL+=tiltA*(L-fTiltLpL); fTiltLpR+=tiltA*(R-fTiltLpR);
            { const float loL=fTiltLpL,loR=fTiltLpR,hiL=L-loL,hiR=R-loR;
              L=loL*gLoT+hiL*gHiT; R=loR*gLoT+hiR*gHiT; }

            // 2) GLUE-compressor (feed-forward RMS, stereo-gelinkt)
            const float m2=(L*L+R*R)*0.5f;
            const float dc=1e-5f;
            fEnv += ((m2>fEnv)?cAtt:cRel)*(m2+dc-fEnv);
            {
                const float rms=sqrtf(fEnv);
                if(rms>thr){
                    const float gr=rms/thr;
                    const float target=powf(gr,-(1.f-1.f/ratio));
                    fGain += ((target<fGain)?gAtt:gRel)*(target-fGain);
                }else fGain += gRel*(1.f-fGain);
                L*=fGain; R*=fGain;
            }

            // 3) WARMTE — tanh saturatie, LP-na-filter op het sat-deel
            {
                float sL=tanhf(L*drv), sR=tanhf(R*drv);
                fSatLpL+=satLpA*(sL-fSatLpL); fSatLpR+=satLpA*(sR-fSatLpR);
                L=L*(1.f-satMix)+fSatLpL*satMix;
                R=R*(1.f-satMix)+fSatLpR*satMix;
            }

            // 4) LUID — pre-gain + peak-limiter -1 dBTP
            L*=preG; R*=preG;
            {
                const float pk=fmaxf(fabsf(L),fabsf(R));
                fLimEnv=fmaxf(pk,fLimEnv*limRel);
                const float g=(fLimEnv>ceil)?ceil/fLimEnv:1.f;
                L*=g; R*=g;
            }

            outputs[0][i]=L; outputs[1][i]=R;
            const float pk=fmaxf(fabsf(L),fabsf(R));
            if(pk>fLevelBlk)fLevelBlk=pk;
            // v2 bus: 8-band energie van de hele mix (mono-som)
            {
                const float x=(L+R)*0.5f;
                float prev=0.f;
                for(int b=0;b<7;b++){
                    fLpM[b]+=fBAM[b]*(x-fLpM[b]);
                    const float band=(b==0?fLpM[0]:fLpM[b]-prev);
                    prev=fLpM[b];
                    fAccM[b]+=band*band;
                }
                const float b7=x-fLpM[6]; fAccM[7]+=b7*b7;
            }
        }

        // KRACHT dry/wet
        const float k=fKr/100.f;
        if(k<0.999f){const float d=1.f-k;
            for(uint32_t i=0;i<frames;i++){outputs[0][i]=inputs[0][i]*d+outputs[0][i]*k;
                                           outputs[1][i]=inputs[1][i]*d+outputs[1][i]*k;}}

        // LUFS-meting (EBU R128) + level — via output-params naar UI
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

        // AUTO: genoeg geluisterd? → knoppen zelf zetten
        if(fAutoN>0){
            fAutoN-=(int)frames;
            if(fAutoN<=0){fAutoN=0;finishAuto();}
        }

        // v2 bus-update per ~512 samples
        if(!fBusInit){
            const float xr[7]={60.f,120.f,250.f,500.f,1000.f,2000.f,4000.f};
            for(int b=0;b<7;b++)fBAM[b]=1.f-expf(-2.f*M_PI*xr[b]/sr);
            fBus=(G2BBus*)g2bBusOpen();
            if(fBus&&fBus->magic!=kBusMagic){std::memset((void*)fBus,0,sizeof(G2BBus));fBus->magic=kBusMagic;}
            fBusInit=true;
        }
        fAccN+=frames;
        if(fAccN>=512){
            const float inv=1.f/(float)fAccN;
            float tot=1e-9f;
            for(int b=0;b<kBusBands;b++){
                fOwnE[b]+=0.3f*(fAccM[b]*inv-fOwnE[b]); fAccM[b]=0.f;
                tot+=fOwnE[b];
            }
            fAccN=0;
            if(fBus){
                fBus->masterTick++;
                for(int b=0;b<kBusBands;b++){
                    fBus->masterE[b]=fOwnE[b];
                    // top-down mask: band die >20% van de mix vreet →
                    // lage-prio sporen duken extra daar (0..0.5)
                    float sh=fOwnE[b]/tot;
                    float m=(sh-0.20f)*2.f;
                    if(m<0.f)m=0.f; if(m>0.5f)m=0.5f;
                    fBus->masterMask[b]=m;
                }
            }
        }
    }
private:
    void startAuto(){
        fAutoN=(int)(getSampleRate()*5.0);
        fAElo=fAEhi=fAEall=0.f;fAPk=0.f;fAN=0;
        fAEmid=fALR=0.f;fATr=0;fTrEnv=1e-6f;
        fLp8L=fLp8R=fLp35L=fLp35R=0.f;
    }
    void finishAuto(){
        if(fAN<4410||fAEall<1e-8f)return;      // te weinig signaal → niks doen
        const float rms=sqrtf(fAEall/fAN);
        const float lufs=20.f*log10f(rms+1e-9f);
        // bas/hoog-balans → TILT (donker signaal → positieve tilt = helderder)
        const float d=10.f*log10f((fAElo+1e-9f)/(fAEhi+1e-9f));
        float aTilt=d*4.f; if(aTilt>50.f)aTilt=50.f;if(aTilt<-50.f)aTilt=-50.f;
        // dynamiek (crest) → GLUE (dynamischer → meer lijm)
        const float crest=20.f*log10f((fAPk+1e-9f)/(rms+1e-9f));
        float aGlue=40.f+(crest-6.f)*8.f; if(aGlue>90.f)aGlue=90.f;if(aGlue<30.f)aGlue=30.f;
        // hi-aandeel → WARMTE (fel signaal → minder warmte nodig)
        const float hiShare=fAEhi/(fAElo+fAEhi+1e-9f);
        float aWarm=60.f-hiShare*400.f; if(aWarm>80.f)aWarm=80.f;if(aWarm<15.f)aWarm=15.f;
        // luidheid → LUID (richt op ~-8 LUFS-momentary, max +9dB)
        const float need=fDoel-lufs;
        float aLuid=need/9.f*100.f; if(aLuid>100.f)aLuid=100.f;if(aLuid<10.f)aLuid=10.f;
        // brein v2: sensoren sturen de engine bij
        const float trRate=(float)fATr/((float)fAN/getSampleRate()+1e-9f); // pieken/sec
        const float vocal=fAEmid/(fAEall+1e-9f);        // stem-band aandeel
        if(trRate>4.f)aGlue-=4.f;else if(trRate<0.5f)aGlue+=3.f; // druk ritme → transiënten vrij
        if(vocal>0.30f){aWarm-=5.f;aGlue-=3.f;}         // stem aanwezig → middengebied open
        if(aGlue>90.f)aGlue=90.f;if(aGlue<30.f)aGlue=30.f;
        if(aWarm>80.f)aWarm=80.f;if(aWarm<15.f)aWarm=15.f;
        // pill-mindset [luidMax, glueBias, warmteBias] — hoe AUTO per genre denkt
        static const float kMind[10][3]={{95,5,0},{90,5,0},{80,0,5},{75,0,5},{90,0,0},{85,3,0},{80,0,3},{55,-15,5},{70,-5,8},{90,0,0}};
        if(fAutoBlend){ // pill = smaak → AUTO stelt bij op de track (50/50) + mindset
            fTilt=(fBlendP[0]+aTilt)*0.5f; fGlue=(fBlendP[1]+aGlue)*0.5f;
            fWarmte=(fBlendP[2]+aWarm)*0.5f; fLuid=(fBlendP[3]+aLuid)*0.5f;
            const int pi=(int)(fPreset+0.5f);
            fGlue+=kMind[pi][1]; fWarmte+=kMind[pi][2];
            if(fLuid>kMind[pi][0])fLuid=kMind[pi][0];
            if(fGlue<0)fGlue=0;if(fGlue>100)fGlue=100;
            if(fWarmte<0)fWarmte=0;if(fWarmte>100)fWarmte=100;
            if(fLuid<0)fLuid=0;
            fAutoBlend=false;
        }else{
            fTilt=aTilt; fGlue=aGlue; fWarmte=aWarm; fLuid=aLuid;
        }
    }
    void applyPreset(int pi, bool autoBlend){
        if(pi<0)pi=0;if(pi>9)pi=9;
        fPreset=(float)pi;
        fTilt=kPresetParams[pi][0]; fGlue=kPresetParams[pi][1];
        fWarmte=kPresetParams[pi][2]; fLuid=kPresetParams[pi][3];
        if(autoBlend){ // genre-pill klik → ook 5 s luisteren en bijstellen
            fBlendP[0]=fTilt;fBlendP[1]=fGlue;fBlendP[2]=fWarmte;fBlendP[3]=fLuid;
            fAutoBlend=true; startAuto();
        }
        // stuur de knop-waarden ook naar buiten zodat de UI meegaat
        if(fInsideRun){/* host pikt changed op via parameterChanged */}
    }
    void rollDice(){
        uint32_t sd=fSeed=fSeed*1664525u+1013904223u;
        auto rng=[&](){sd^=sd<<13;sd^=sd>>17;sd^=sd<<5;return (sd&0xFFFFu)/65536.f;};
        fTilt=(rng()*100.f-50.f)*0.6f;
        fGlue=30.f+rng()*60.f;
        fWarmte=10.f+rng()*70.f;
        fLuid=40.f+rng()*60.f;
    }
    // params
    float fTilt=0.f,fGlue=50.f,fWarmte=30.f,fLuid=60.f;
    float fPreset=0.f,fOn=1.f,fKr=100.f,fDoel=-6.f;
    uint32_t fSeed=12345u; bool fInsideRun=false;
    // DSP-state
    float fHpL=0.f,fHpR=0.f;
    float fTiltLpL=0.f,fTiltLpR=0.f;
    float fEnv=1e-6f,fGain=1.f;
    float fSatLpL=0.f,fSatLpR=0.f;
    float fLimEnv=0.f;
    // scope/lufs
    float fLevel=0.f, fLevelBlk=0.f, fLufsM=-99.f, fLufsI=-99.f;
    int fAutoN=0, fAN=0; float fAElo=0.f,fAEhi=0.f,fAEall=0.f,fAPk=0.f;
    float fLp8L=0.f,fLp8R=0.f,fLp35L=0.f,fLp35R=0.f,fAEmid=0.f,fALR=0.f,fTrEnv=1e-6f; int fATr=0;
    bool fAutoBlend=false; float fBlendP[4]={0,0,0,0};
    float fInter[512];
    ebur128_state* fEbur=nullptr;
    // v2 bus-state
    G2BBus* fBus=nullptr; bool fBusInit=false;
    float fLpM[7]={},fLpMPrev[7]={},fBAM[7]={},fAccM[8]={},fOwnE[8]={};
    int fAccN=0;
};

Plugin* createPlugin() { return new MasterPlugin(); }

END_NAMESPACE_DISTRHO
