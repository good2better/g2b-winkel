/*
 * G2B Dice-Synth — de dobbelsteen-synthesizer.
 * 5 modi (KEYS / PADS / TEXTURE / BASS / HIT). DICE rolt een compleet nieuwe,
 * altijd bruikbare sound binnen de grenzen van de modus. Slotjes houden
 * secties vast, MUTATE varieert licht, UNDO/REDO redt elke rol.
 * 100% eigen DSP — sync-veilig.
 */

#include "DistrhoPlugin.hpp"
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdio>

START_NAMESPACE_DISTRHO

#include "dicesynth_roll.h"

static const char* kParamNames[kParamCount] = {
    "mode","dice","mutate","undo","redo",
    "osc1wave","osc2wave","osc2oct","osc2det","osc2lvl","noise","pitchnv",
    "attack","decay","sustain","release",
    "cutoff","reso","filtenv",
    "lfo_rate","lfo_depth",
    "drive","reverb","delay","delaydiv","delayfb","warm","gain",
    "chord","chordshape","strum","human",
    "lock_osc","lock_env","lock_filt","lock_fx",
    "on"
};

static inline float shapeOSC(float ph,float w){ // ph 0..1, w 0..1 sine→tri→saw→square
    const float s=sinf(ph*6.2831853f);
    const float t=2.f*fabsf(2.f*(ph-floorf(ph))-1.f)-1.f;
    const float saw=2.f*(ph-floorf(ph))-1.f;
    const float sq=(ph-floorf(ph))<0.5f?1.f:-1.f;
    const float seg=w*3.f; const int i=(int)seg; const float f=seg-i;
    float a,b;
    switch(i<3?i:3){case 0:a=s;b=t;break;case 1:a=t;b=saw;break;case 2:a=saw;b=sq;break;default:a=sq;b=sq;}
    return a+(b-a)*f;
}

struct Voice {
    bool on=false,rel=false; int note=0; float vel=0,t=0,startDel=0;
    float ph1=0,ph2=0,f0=0,ratio2=1,pan=0.5f;
    float amp=0; int st=0; // 0 atk 1 dec 2 sus 3 rel
    float lo=0,ba=0;
    uint32_t rs=0x12345;
};

class DiceSynthPlugin : public Plugin {
public:
    DiceSynthPlugin() : Plugin(kParamCount,0,0) {
        float defs[kParamCount]={};
        defs[kMode]=0.25f; defs[kOn]=1.f;
        defs[kOsc1W]=0.3f;defs[kOsc2W]=0.6f;defs[kO2Oct]=0.5f;defs[kO2Det]=0.6f;defs[kO2Lvl]=0.7f;defs[kNoise]=0.05f;defs[kPEnv]=0.5f;
        defs[kAtk]=0.45f;defs[kDec]=0.5f;defs[kSus]=0.85f;defs[kRel]=0.5f;
        defs[kCut]=0.4f;defs[kRes]=0.3f;defs[kFEnv]=0.4f;
        defs[kLfoR]=0.15f;defs[kLfoD]=0.4f;
        defs[kDrive]=0.15f;defs[kRev]=0.5f;defs[kDly]=0.25f;defs[kDlyDiv]=0.4f;defs[kDlyFb]=0.4f;defs[kWarm]=0.5f;defs[kGain]=0.8f;
        defs[kStrum]=0.3f;defs[kHuman]=0.25f;
        for(int i=0;i<kParamCount;i++)fP[i]=defs[i];
        const char* nm=genName();
        setState("diceName",nm);
    }
    const char* getLabel()const override{return "DiceSynth";}
    const char* getDescription()const override{return "G2B Dice-Synth — rol eindeloos nieuwe sync-veilige sounds";}
    const char* getMaker()const override{return "G2B";}
    const char* getHomePage()const override{return "https://good2better.nl";}
    const char* getLicense()const override{return "Proprietary";}
    uint32_t getVersion()const override{return d_version(1,0,0);}
    int64_t getUniqueId()const override{return d_cconst('G','2','D','s');}

    void initParameter(uint32_t i,Parameter& p)override{
        p.hints=kParameterIsAutomatable; p.name=kParamNames[i]; p.symbol=kParamNames[i];
        p.ranges.def=0.f;p.ranges.min=0.f;p.ranges.max=1.f;
        switch(i){
            case kMode: p.hints|=kParameterIsInteger; p.name="Modus";break;
            case kDice: case kMutate: case kUndoB: case kRedoB:
                p.hints=kParameterIsAutomatable|kParameterIsTrigger|kParameterIsBoolean;break;
            case kChord: case kLockOsc: case kLockEnv: case kLockFilt: case kLockFx: case kOn:
                p.hints|=kParameterIsBoolean;break;
            case kChordShape: p.hints|=kParameterIsInteger;break;
            case kGain: p.ranges.def=0.8f;break;
            default: break;
        }
        p.ranges.def=getDefaultFor(i);
    }
    float getDefaultFor(int i){
        switch(i){
            case kMode:return 0.25f;case kOn:return 1.f;
            case kOsc1W:return 0.3f;case kOsc2W:return 0.6f;case kO2Oct:return 0.5f;case kO2Det:return 0.6f;
            case kO2Lvl:return 0.7f;case kNoise:return 0.05f;case kPEnv:return 0.5f;
            case kAtk:return 0.45f;case kDec:return 0.5f;case kSus:return 0.85f;case kRel:return 0.5f;
            case kCut:return 0.4f;case kRes:return 0.3f;case kFEnv:return 0.4f;
            case kLfoR:return 0.15f;case kLfoD:return 0.4f;
            case kDrive:return 0.15f;case kRev:return 0.5f;case kDly:return 0.25f;case kDlyDiv:return 0.4f;
            case kDlyFb:return 0.4f;case kWarm:return 0.5f;case kGain:return 0.8f;
            case kStrum:return 0.3f;case kHuman:return 0.25f;
            default:return 0.f;
        }
    }
    float getParameterValue(uint32_t i)const override{return fP[i];}
    void setParameterValue(uint32_t i,float v)override{
        if(i==kDice&&v>0.5f){pushHist();roll(false);fP[kDice]=0.f;setParameterValue(kDice,0.f);return;}
        if(i==kMutate&&v>0.5f){pushHist();roll(true);fP[kMutate]=0.f;setParameterValue(kMutate,0.f);return;}
        if(i==kUndoB&&v>0.5f){histNav(-1);fP[kUndoB]=0.f;setParameterValue(kUndoB,0.f);return;}
        if(i==kRedoB&&v>0.5f){histNav(+1);fP[kRedoB]=0.f;setParameterValue(kRedoB,0.f);return;}
        fP[i]=v;
        if(i==kMode){/* modus-wissel rolt niet — user rolt zelf */}
        updateCached();
    }

    const char* getStateKey(int32_t)const{return nullptr;}
    const char* getStateValue(const char*)const{return nullptr;}
    void setState(const char* key,const char* value){
        if(!std::strcmp(key,"diceName")){std::strncpy(fName,value,63);fName[63]=0;}
        else Plugin::setState(key,value);
    }

    void activate()override{for(auto&v:fV)v=Voice();fLfoPh=0;fDelW=0;std::memset(fDelL,0,sizeof fDelL);std::memset(fDelR,0,sizeof fDelR);std::memset(fRev,0,sizeof fRev);std::memset(fRevP,0,sizeof fRevP);std::memset(fAll,0,sizeof fAll);std::memset(fAllP,0,sizeof fAllP);fTiltL=fTiltR=0;fLoL=fLoR=0;}
    void deactivate()override{}

    void run(const float**,float** out,uint32_t n,const MidiEvent* ev,uint32_t evc)override{
        float* L=out[0]; float* R=out[1];
        if(fP[kOn]<0.5f){std::memset(L,0,n*4);std::memset(R,0,n*4);return;}
        for(uint32_t e=0;e<evc;e++){
            const MidiEvent&m=ev[e];
            if(m.size<3)continue;
            const uint8_t st=m.data[0]&0xF0,nt=m.data[1],vl=m.data[2];
            if(st==0x90&&vl>0)noteOn(nt,vl/127.f);
            else if(st==0x80||(st==0x90&&vl==0))noteOff(nt);
        }
        // per-block caches
        const float sr=(float)getSampleRate();
        // DICE-morph: continue params glijden ~300ms naar doel; toggles snappen direct
        {   static const uint8_t kSnap[]={1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1};
            const float mk=1.f-expf(-(float)n/(0.30f*sr));
            for(int i=0;i<kParamCount;i++){
                if(i<(int)(sizeof(kSnap)/sizeof(kSnap[0]))&&kSnap[i])fSm[i]=fP[i];
                else{float df=fP[i]-fSm[i];fSm[i]+=df*mk;if(df>-0.0005f&&df<0.0005f)fSm[i]=fP[i];}
            }
        }
        const float atkT=0.001f*powf(5000.f,fSm[kAtk]);
        const float decT=0.02f*powf(200.f,fSm[kDec]);
        const float relT=0.005f*powf(1200.f,fSm[kRel]);
        const float rAtk=1.f/(atkT*sr),rDec=1.f/(decT*sr),rRel=1.f/(relT*sr);
        const float susL=fSm[kSus];
        const float baseCut=80.f*powf(200.f,fSm[kCut]);
        const float res=fSm[kRes];
        const float fenvA=fSm[kFEnv]*5.f;
        const float lfoRate=0.05f*powf(400.f,fSm[kLfoR]);
        const float lfoDep=fSm[kLfoD]*4.f;
        const float penvSemis=(fSm[kPEnv]-0.5f)*48.f;
        const float drvK=1.f+fSm[kDrive]*6.f;
        const float drvN=1.f/(1.f+fSm[kDrive]*1.2f);
        const float w2p=fSm[kOsc2W];
        const float o1w=fSm[kOsc1W];
        const int oct2=(int)(fSm[kO2Oct]*4.f+0.5f)-2;
        const float detC=(fSm[kO2Det]-0.5f)*100.f;
        const float r2=powf(2.f,(12.f*oct2+detC/100.f)/12.f);
        const float o2l=fSm[kO2Lvl],nL=fSm[kNoise];
        const float revM=fSm[kRev],dlyM=fSm[kDly];
        const float gain=fSm[kGain]*1.4f;
        float bpm=120.f;
        const TimePosition& tp=getTimePosition();
        if(tp.bbt.valid&&tp.bbt.beatsPerMinute>30)bpm=(float)tp.bbt.beatsPerMinute;
        static const float kDivs[4]={1.f,0.5f,0.75f,0.25f};
        const int dIdx=(int)(fSm[kDlyDiv]*3.f+0.5f);
        const float dlyS=(60.f/bpm)*kDivs[dIdx]*sr;
        const float dlyFb=fSm[kDlyFb]*0.75f;
        const float warmLo=0.7f+0.6f*fSm[kWarm],warmHi=1.3f-0.6f*fSm[kWarm];
        const float tiltA=1.f-expf(-6.2831853f*850.f/sr);
        const float tiltLo=1.f-expf(-6.2831853f*150.f/sr);
        const int nV=countVoices();
        const float vNorm=1.f/sqrtf(nV>0?(float)nV:1.f);

        for(uint32_t i=0;i<n;i++){
            // LFO
            fLfoPh+=lfoRate/sr; if(fLfoPh>=1.f)fLfoPh-=1.f;
            const float lfoV=sinf(fLfoPh*6.2831853f);
            float sl=0,sr2=0;
            for(int v=0;v<12;v++){
                Voice&vc=fV[v]; if(!vc.on)continue;
                if(vc.startDel>0){vc.startDel-=1.f;continue;}
                vc.t+=1.f/sr;
                // env
                switch(vc.st){
                    case 0: vc.amp+=rAtk; if(vc.amp>=1.f){vc.amp=1.f;vc.st=1;}break;
                    case 1: vc.amp-=rDec; if(vc.amp<=susL){vc.amp=susL;vc.st=2;}break;
                    case 3: vc.amp-=rRel; if(vc.amp<=0.f){vc.amp=0;vc.on=false;}break;
                    default:break;
                }
                if(!vc.on)continue;
                const float penv=powf(2.f,penvSemis*expf(-vc.t*10.f)/12.f);
                vc.ph1+=vc.f0*penv/sr; if(vc.ph1>=1.f)vc.ph1-=1.f;
                vc.ph2+=vc.f0*vc.ratio2*penv/sr; if(vc.ph2>=1.f)vc.ph2-=1.f;
                float x=shapeOSC(vc.ph1,o1w)+o2l*shapeOSC(vc.ph2,w2p);
                vc.rs=vc.rs*1664525u+1013904223u;
                x+=nL*(((vc.rs>>9)&0xFFFF)/32768.f-1.f);
                x*=vc.amp*vc.vel;
                // filter: env*cutoff*lfo
                const float fcv=baseCut*powf(2.f,vc.amp*fenvA+lfoV*lfoDep);
                float fc=fcv>16000.f?16000.f:(fcv<40.f?40.f:fcv);
                const float fcof=2.f*sinf(3.1415926f*fc/sr);
                const float q=1.f-res*0.85f;
                vc.lo+=fcof*vc.ba;
                const float hi=x-vc.lo-q*vc.ba;
                vc.ba+=fcof*hi;
                const float y=vc.lo*0.8f;
                sl+=y*(1.f-vc.pan); sr2+=y*vc.pan;
            }
            sl*=vNorm; sr2*=vNorm;
            // drive
            sl=tanhf(sl*drvK)*drvN; sr2=tanhf(sr2*drvK)*drvN;
            // delay ping-pong
            int dS=(int)dlyS; if(dS>383999)dS=383999; if(dS<1)dS=1;
            float dR=fDelR[(fDelW+2*192000-dS)%(2*192000)];
            float dL=fDelL[(fDelW+2*192000-dS)%(2*192000)];
            fDelL[fDelW]=sl+dR*dlyFb; fDelR[fDelW]=sr2+dL*dlyFb;
            sl+=dL*dlyM; sr2+=dR*dlyM;
            // reverb (4 combs + 2 allpass per kanaal)
            float rl=revTick(sl,0),rr=revTick(sr2,1);
            sl+=(rl-sl)*revM; sr2+=(rr-sr2)*revM;
            // tilt warm
            fTiltL+=tiltA*(sl-fTiltL); fTiltR+=tiltA*(sr2-fTiltR);
            sl=sl*warmHi+fTiltL*(warmLo-warmHi); sr2=sr2*warmHi+fTiltR*(warmLo-warmHi);
            // mono-laag: alles onder ~150Hz strikt in het midden (geen fase-uitdoving)
            fLoL+=tiltLo*(sl-fLoL); fLoR+=tiltLo*(sr2-fLoR);
            {   const float loM=0.5f*(fLoL+fLoR);
                sl=loM+(sl-fLoL); sr2=loM+(sr2-fLoR); }
            // gain + soft clip
            L[i]=tanhf(sl*gain); R[i]=tanhf(sr2*gain);
            fDelW=(fDelW+1)%(2*192000);
        }
    }

private:
    int fStrumDir=1;
    float fLoL=0,fLoR=0;
    float fP[kParamCount]={};
    float fSm[kParamCount]={};
    Voice fV[12];
    float fLfoPh=0;
    // delay buffers 2s
    float fDelL[2*192000],fDelR[2*192000]; int fDelW=0;
    // reverb: 4 comb lines + 2 allpass per channel
    static const int kRevN=4;
    float fRev[2][4][1800]; int fRevP[2][4]; int fRevL[4]={1116,1188,1277,1356};
    float fAll[2][2][600]; int fAllP[2][2]; int fAllL[2]={225,556};
    float fTiltL=0,fTiltR=0;
    uint32_t fRnd=22222u;
    float fHist[24][kParamCount]; int fHistN=0,fHistPos=0,fView=0;
    char fName[64]={};

    float revTick(float in,int ch){
        float acc=0;
        for(int c=0;c<kRevN;c++){
            float*ln=fRev[ch][c]; int L=fRevL[c];
            float y=ln[fRevP[ch][c]];
            ln[fRevP[ch][c]]=in*0.35f+y*0.62f;
            fRevP[ch][c]=(fRevP[ch][c]+1)%L;
            acc+=y;
        }
        acc*=0.25f;
        for(int a=0;a<2;a++){
            float*ln=fAll[ch][a]; int L=fAllL[a];
            float buf=ln[fAllP[ch][a]];
            float y=-acc+buf;
            ln[fAllP[ch][a]]=acc+buf*0.5f;
            fAllP[ch][a]=(fAllP[ch][a]+1)%L;
            acc=y;
        }
        return acc;
    }

    int countVoices(){int n=0;for(auto&v:fV)if(v.on)n++;return n;}
    Voice* freeVoice(){
        for(auto&v:fV)if(!v.on)return &v;
        float old=-1;Voice*best=&fV[0];
        for(auto&v:fV)if(v.st==3&&v.t>old){old=v.t;best=&v;}
        if(old<0)for(auto&v:fV)if(v.t>old){old=v.t;best=&v;}
        return best;
    }
    void noteOn(int nt,float vel){
        static const int shapes[4][5]={{0,3,7,10,-1},{0,5,7,-1,-1},{0,7,12,-1,-1},{0,7,12,19,-1}};
        const int mode=(int)(fP[kMode]*4.f+0.5f);
        const bool ch=fP[kChord]>0.5f&&mode!=3&&mode!=4;
        const int sh=(int)(fP[kChordShape]*3.f+0.5f);
        const float sr=getSampleRate();
        const float strumS=(ch?fP[kStrum]*0.060f:0.f)*sr;
        const float hum=fP[kHuman]*(1.6f-vel); // zacht spelen = lossere timing
        // tel akkoordnoten voor up/down-strum
        int nN=0; for(int ci=0;ci<5&&ch;ci++){if(shapes[sh][ci]<0)break;nN++;}
        if(ch) fStrumDir=-fStrumDir; // wissel slag-richting per akkoord
        for(int ci=0;ci<5;ci++){
            const int iv=ch?shapes[sh][ci]:ci==0?0:-1;
            if(iv<0)break;
            Voice*v=freeVoice();
            const float a0=v->on?v->amp*0.5f:0.f; // oude staart meenemen = micro-crossfade
            *v=Voice(); v->amp=a0;
            v->on=true;v->note=nt+iv;
            v->vel=vel*(1.f+(rnd()-0.5f)*0.4f*hum);
            // exponentiele tokkel-curve; up-slag = hoge noot eerst
            const int sIdx=(fStrumDir<0)?(nN-1-ci):ci;
            const float strumPos=powf((float)sIdx,1.4f);
            v->startDel=strumPos*strumS+rnd()*hum*0.006f*sr;
            v->f0=440.f*powf(2.f,(v->note-69)/12.f);
            v->ratio2=r2c(); v->pan=0.5f+((ci&1)?0.18f:-0.18f)*(ci>0?1.f:0.f);
        }
    }
    void noteOff(int nt){for(auto&v:fV)if(v.on&&v.note==nt&&v.st!=3)v.st=3;}
    float r2c(){ // osc2 ratio from params (cached per note-on)
        const int oct2=(int)(fSm[kO2Oct]*4.f+0.5f)-2;
        const float detC=(fSm[kO2Det]-0.5f)*100.f;
        return powf(2.f,(12.f*oct2+detC/100.f)/12.f);
    }
    void updateCached(){}

    float rnd(){fRnd=fRnd*1664525u+1013904223u;return ((fRnd>>8)&0xFFFFFF)/16777216.f;}
    float rr(float a,float b){return a+(b-a)*rnd();}

    void pushHist(){
        std::memcpy(fHist[fHistPos%24],fP,sizeof fP);
        fHistPos++; if(fHistN<24)fHistN++;
        fView=fHistPos-1;
    }
    void histNav(int d){
        if(fHistN==0)return;
        // geldige entries: indexen fHistPos-fHistN .. fHistPos-1; fView=welke we tonen
        if(fView<=0)fView=fHistPos-1;
        if(d<0){if(fView>fHistPos-fHistN)fView--;else return;}
        else{if(fView<fHistPos-1)fView++;else return;}
        for(int i=0;i<kParamCount;i++){fP[i]=fHist[fView%24][i];setParameterValue(i,fP[i]);}
    }

    void roll(bool mutate){
        g2bRollDice(fP,fRnd,mutate);
        for(int t=0;t<kRollCount;t++)setParameterValue(kRollTable[t].p,fP[kRollTable[t].p]);
        const char* nm=genName();
        setState("diceName",nm);
    }

    const char* genName(){
        const int mode=(int)(fP[kMode]*4.f+0.5f);
        g2bDiceName(mode,fRnd,fName);
        return fName;
    }
};

Plugin* createPlugin(){return new DiceSynthPlugin();}
END_NAMESPACE_DISTRHO
