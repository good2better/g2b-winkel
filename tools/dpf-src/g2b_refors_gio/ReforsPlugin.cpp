/*
 * G2B REFORS — referentie-simulator: hoor je track zoals kopers hem horen.
 * 8 ruimte-pills (STUDIO/CLUB/AUTO/TELEFOON/AIRPODS/FESTIVAL/TV/TIENTJE),
 * elk = eigen speaker-curve (HP+LP+bass-shelf+presence), drive, echo-ruimte,
 * stereo-brede. Knoppen: AFSTAND · KAMER · MENIGTE (publiek-galm) · VOLUME.
 */
#include "DistrhoPlugin.hpp"
#include <cstring>
#include <cmath>
#include <cstdint>
#include <algorithm>

START_NAMESPACE_DISTRHO

#ifdef G2B_GIO
static const int kNumProgs=10;
#else
static const int kNumProgs=8;
#endif
static const char* kPresetNames[kNumProgs] = {
    "STUDIO","CLUB","AUTO","TELEFOON","AIRPODS","FESTIVAL","TV","TIENTJE"
#ifdef G2B_GIO
    ,"KELDER","STRAAT"
#endif
};
// per ruimte: loHz, hiHz, bassDb(@120Hz), presDb(@3kHz bell), drive, echoMs, echoFb, echoMix, width(0=mono..1), gainDb
static const float kEnv[10][10] = {
    { 20.f,19000.f,  0.f, 0.f, 0.00f,   0.f,0.00f,0.00f, 1.0f,  0.f}, // STUDIO — plat
    { 35.f,13500.f, +5.f,+2.f, 0.15f, 220.f,0.35f,0.30f, 0.9f, +1.f}, // CLUB — grote PA + zaal
    { 50.f,14000.f, +7.f,+3.f, 0.10f,  60.f,0.25f,0.12f, 0.85f,+1.f}, // AUTO — basbult + cabine
    {350.f, 3400.f, -8.f,+2.f, 0.35f,   0.f,0.00f,0.00f, 0.0f, -2.f}, // TELEFOON — smal+mono+krakerig
    { 60.f,17000.f, -3.f,+2.f, 0.05f,   0.f,0.00f,0.00f, 0.9f,  0.f}, // AIRPODS — dun maar clean
    { 30.f, 9000.f, +6.f, 0.f, 0.08f, 350.f,0.45f,0.35f, 1.0f, -1.f}, // FESTIVAL — buiten, HF-verlies
    {150.f, 8000.f, -2.f,+4.f, 0.12f,  90.f,0.20f,0.10f, 0.7f,  0.f}, // TV — boxy midden
    { 90.f,11000.f, -2.f,+3.f, 0.25f,   0.f,0.00f,0.00f, 0.8f, -1.f}, // TIENTJE — goedkope oordopjes
    { 40.f, 8000.f, +8.f, 0.f, 0.12f, 140.f,0.50f,0.45f, 0.8f, -1.f}, // KELDER — donker+boomy fuif (GIO)
    {120.f,12000.f, -4.f,+5.f, 0.18f,  80.f,0.30f,0.15f, 0.85f,-2.f}, // STRAAT — ghettoblaster (GIO)
};

class ReforsPlugin : public Plugin {
public:
    enum Params { kAfst, kKamer, kMenigte, kVol, kDice, kPreset, kOn, kKracht,
                  kLevel, kOAfst, kOKamer, kOMenigte, kOVol, kNumParams };
    ReforsPlugin() : Plugin(kNumParams, 0, 0) {}
protected:
    const char* getLabel() const override { return "G2B Refors"; }
    const char* getDescription() const override {
        return "Hoor je track zoals kopers hem horen: telefoon, auto, club, festival — 8 ruimtes, 1 klik."; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','R','f'); }

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints=kParameterIsAutomatable;
        switch(i){
        case kAfst:   p.name="AFSTAND"; p.symbol="afstand"; p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=0.f;p.unit="%";break;
        case kKamer:  p.name="KAMER";   p.symbol="kamer";   p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=100.f;p.unit="%";break;
        case kMenigte:p.name="MENIGTE"; p.symbol="menigte"; p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=0.f;p.unit="%";break;
        case kVol:    p.name="VOLUME";  p.symbol="volume";  p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=80.f;p.unit="%";break;
        case kDice:   p.name="DICE";    p.symbol="dice";    p.hints|=kParameterIsBoolean;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.f;break;
        case kPreset: p.name="RUIMTE";  p.symbol="ruimte";  p.hints|=kParameterIsInteger;p.ranges.min=0.f;p.ranges.max=(float)(kNumProgs-1);p.ranges.def=0.f;break;
        case kOn:     p.name="Aan";     p.symbol="aan";     p.hints|=kParameterIsBoolean;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=1.f;break;
        case kKracht: p.name="Kracht";  p.symbol="kracht";  p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=100.f;p.unit="%";break;
        case kLevel:  p.name="LEVEL";   p.symbol="level";   p.hints=kParameterIsOutput;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.f;break;
        case kOAfst:  p.name="OAFST";   p.symbol="oafst";   p.hints=kParameterIsOutput;p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=0.f;break;
        case kOKamer: p.name="OKAMER";  p.symbol="okamer";  p.hints=kParameterIsOutput;p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=100.f;break;
        case kOMenigte:p.name="OMENIG"; p.symbol="omenig";  p.hints=kParameterIsOutput;p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=0.f;break;
        case kOVol:   p.name="OVOL";    p.symbol="ovol";    p.hints=kParameterIsOutput;p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=80.f;break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){case kAfst:return fAfst;case kKamer:return fKamer;case kMenigte:return fMenigte;
                  case kVol:return fVol;case kDice:return 0.f;case kPreset:return fPreset;
                  case kOn:return fOn;case kKracht:return fKr;case kLevel:return fLevel;
                  case kOAfst:return fAfst;case kOKamer:return fKamer;
                  case kOMenigte:return fMenigte;case kOVol:return fVol;}return 0.f;
    }
    void setParameterValue(uint32_t i, float v) override {
        switch(i){
        case kAfst:fAfst=v;break;case kKamer:fKamer=v;break;case kMenigte:fMenigte=v;break;
        case kVol:fVol=v;break;
        case kDice:if(v>0.5f)rollDice();break;
        case kPreset:fPreset=v;if(v<0)fPreset=0;if(v>kNumProgs-1)fPreset=(float)(kNumProgs-1);break;
        case kOn:fOn=v;break;case kKracht:fKr=v;break;}
    }
    void initState(uint32_t, String&, String&) override {}
    void setState(const char* k, const char* v) override {(void)k;(void)v;}

    void activate() override {
        std::memset(fLpL,0,sizeof(fLpL)); std::memset(fLpR,0,sizeof(fLpR));
        fDelW=0; std::memset(fDelL,0,sizeof(fDelL)); std::memset(fDelR,0,sizeof(fDelR));
        fNse=0.f; fNseHP=0.f; fCrL=0.f; fCrR=0.f; fLevelBlk=0.f;
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        if(fOn<0.5f){
            if(inputs[0]!=outputs[0])std::memcpy(outputs[0],inputs[0],frames*sizeof(float));
            if(inputs[1]!=outputs[1])std::memcpy(outputs[1],inputs[1],frames*sizeof(float));
            return;
        }
        const float sr=getSampleRate();
        const float srInv=1.f/sr;
        const int pi=(int)(fPreset+0.5f);
        const float* E=kEnv[pi<0?0:(pi>kNumProgs-1?kNumProgs-1:pi)];

        // AFSTAND: haalt top weg + iets zachter + iets meer ruimte
        const float af=fAfst/100.f;
        const float loHz=E[0], hiHz=E[1]*(1.f-0.65f*af)<800.f?800.f:E[1]*(1.f-0.65f*af);
        const float aLo=1.f-expf(-2.f*M_PI*loHz*srInv);          // HP via lp-subtract
        const float aHi=1.f-expf(-2.f*M_PI*hiHz*srInv);
        const float aBass=1.f-expf(-2.f*M_PI*120.f*srInv);       // bas-shelf band
        const float gBass=powf(10.f,E[2]*0.05f);
        const float gPres=powf(10.f,E[3]*0.05f);                 // presence-boost (simpel: hi-remnant)
        const float drv=1.f+E[4]*6.f, distMix=E[4]*0.8f;
        // echo/ruimte — KAMER schaalt echoMix 0..2x
        const float kam=fKamer/100.f;
        const float echoMix=fminf(E[7]*(0.5f+1.5f*kam)+af*0.15f, 0.6f);
        const int delN=(int)(E[5]*sr*0.001f);
        const float fb=E[6];
        const float width=E[8];
        const float outG=powf(10.f,(E[9]-af*6.f)*0.05f)*(0.5f+0.5f*(fVol/100.f)*2.f);
        // MENIGTE: zachte gefilterde publieksruis (roze-ish, langzaam golvend)
        const float crowd=fMenigte/100.f*0.10f;
        const float aCrowd=1.f-expf(-2.f*M_PI*3000.f*srInv);
        const float aCrowdHP=1.f-expf(-2.f*M_PI*300.f*srInv);

        for(uint32_t i=0;i<frames;i++){
            float L=inputs[0][i],R=inputs[1][i];
            const float dryL=L,dryR=R;

            // HP (onder loHz weg)
            fLpL[0]+=aLo*(L-fLpL[0]); fLpR[0]+=aLo*(R-fLpR[0]);
            L-=fLpL[0]; R-=fLpR[0];
            // bas-shelf @120Hz
            fLpL[1]+=aBass*(L-fLpL[1]); fLpR[1]+=aBass*(R-fLpR[1]);
            { const float bL=fLpL[1],bR=fLpR[1];
              L=(L-bL)+bL*gBass; R=(R-bR)+bR*gBass; }
            // presence: split op 2k, boost boven-band licht
            fLpL[2]+=(1.f-expf(-2.f*M_PI*2500.f*srInv))*(L-fLpL[2]);
            fLpR[2]+=(1.f-expf(-2.f*M_PI*2500.f*srInv))*(R-fLpR[2]);
            { const float pL=L-fLpL[2],pR=R-fLpR[2];
              L+=pL*(gPres-1.f)*0.6f; R+=pR*(gPres-1.f)*0.6f; }
            // LP (boven hiHz weg — telefoon/luidspreker-karakter)
            fLpL[3]+=aHi*(L-fLpL[3]); fLpR[3]+=aHi*(R-fLpR[3]);
            L=fLpL[3]; R=fLpR[3];
            // drive (kleine speakers vervormen)
            if(distMix>0.001f){
                const float sL=tanhf(L*drv),sR=tanhf(R*drv);
                L=L*(1.f-distMix)+sL*distMix/drv;
                R=R*(1.f-distMix)+sR*distMix/drv;
            }
            // echo-ruimte (mono-in, stereo-uit met kleine L/R-offset)
            if(echoMix>0.001f&&delN>16){
                const int dp=delN<kDelMax?delN:kDelMax;
                const int rd=(fDelW-dp+kDelMax)%kDelMax;
                const int rd2=(fDelW-dp-(int)(sr*0.011f)+kDelMax*2)%kDelMax;
                const float eL=fDelL[rd],eR=fDelR[rd2];
                const float in=(L+R)*0.5f;
                fDelL[fDelW]=in+eL*fb; fDelR[fDelW]=in+eR*fb;
                fDelW=(fDelW+1)%kDelMax;
                L=L*(1.f-echoMix)+eL*echoMix;
                R=R*(1.f-echoMix)+eR*echoMix;
            }
            // stereo-brede (0 = mono fold)
            if(width<0.999f){
                const float mid=(L+R)*0.5f, sd=(L-R)*0.5f*width;
                L=mid+sd; R=mid-sd;
            }
            // MENIGTE: gefilterde ruis met langzame deining (geen constante saaie hiss)
            if(crowd>0.0005f){
                fSeed=fSeed*1664525u+1013904223u;
                const float w=(float)((int)(fSeed>>16)-32768)/32768.f;
                fNse+=aCrowd*(w-fNse);
                fNseHP+=aCrowdHP*(fNse-fNseHP);
                const float bp=fNse-fNseHP;
                fCrL+=(1.f-expf(-1.f/(0.04f*sr)))*(bp-fCrL);
                fCrR+=(1.f-expf(-1.f/(0.05f*sr)))*(bp-fCrR);
                L+=fCrL*crowd; R+=fCrR*crowd;
            }

            L*=outG; R*=outG;
            // veiligheids-limiter zacht (speakers pieken nooit door)
            L=tanhf(L*1.1f)*0.91f; R=tanhf(R*1.1f)*0.91f;

            outputs[0][i]=L; outputs[1][i]=R;
            const float pk=fmaxf(fabsf(L),fabsf(R));
            if(pk>fLevelBlk)fLevelBlk=pk;
            (void)dryL;(void)dryR;
        }

        const float k=fKr/100.f;
        if(k<0.999f){const float d=1.f-k;
            for(uint32_t i=0;i<frames;i++){outputs[0][i]=inputs[0][i]*d+outputs[0][i]*k;
                                           outputs[1][i]=inputs[1][i]*d+outputs[1][i]*k;}}
        fLevel=fLevelBlk;fLevelBlk=0.f;
    }
private:
    void rollDice(){
        uint32_t sd=fSeed=fSeed*1664525u+1013904223u;
        auto rng=[&](){sd^=sd<<13;sd^=sd>>17;sd^=sd<<5;return (sd&0xFFFFu)/65536.f;};
        fAfst=rng()*60.f; fKamer=50.f+rng()*50.f;
        fMenigte=(int)(fPreset+0.5f)==1|| (int)(fPreset+0.5f)==5?rng()*70.f:rng()*30.f;
        fVol=60.f+rng()*40.f;
    }
    float fAfst=0.f,fKamer=100.f,fMenigte=0.f,fVol=80.f;
    float fPreset=0.f,fOn=1.f,fKr=100.f;
    uint32_t fSeed=777u;
    // DSP state: fLp[0]=HP-lp, [1]=bass-lp, [2]=presence-lp, [3]=hi-lp
    float fLpL[4]={},fLpR[4]={};
    static const int kDelMax=44100;
    float fDelL[kDelMax],fDelR[kDelMax]; int fDelW=0;
    float fNse,fNseHP,fCrL,fCrR;
    float fLevel=0.f,fLevelBlk=0.f;
};

Plugin* createPlugin() { return new ReforsPlugin(); }

END_NAMESPACE_DISTRHO
