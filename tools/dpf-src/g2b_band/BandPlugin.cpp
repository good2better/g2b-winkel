/*
 * G2B BAND — 3-in-1: DJEBU + CO-PRODUCER + TREIN (100% eigen code).
 * MODUS 0 SPEL        : speel akkoorden → hij speelt live bass + drums mee (DJEBU)
 * MODUS 1 VERVOLG     : LUISTER → speel je halve beat → GEN → hij schrijft de 2e helft (CO-PRODUCER)
 * MODUS 2 ARRANGEMENT : loop (audio) of je SPEL/VERVOLG → heel nummer INTRO..OUTRO als .mid (TREIN)
 * Alles speelt hoorbaar (eigen synth) én als MIDI uit; ↓MID schrijft naar Music/G2B.
 */
#include "DistrhoPlugin.hpp"
#include "vendor/MidiFile.h"
#include "../g2b_std.h"
#include "../g2b_bus.h"
#include <cstring>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <ctime>

START_NAMESPACE_DISTRHO

#ifdef G2B_GIO
static const int kNumProgs=14;
#else
static const int kNumProgs=10;
#endif

// genre-smaak SPEL/VERVOLG: {kick,hat,sync,oct,dur16}
struct GStijl{float kick,hat,sync,oct;int dur;};
static const GStijl kStijl[14]={
    {0.35f,0.85f,0.5f,0.7f,2},{0.30f,0.50f,0.7f,0.5f,3},{0.40f,0.55f,0.6f,0.2f,2},
    {0.30f,0.60f,0.8f,0.3f,4},{0.65f,0.75f,0.3f,0.4f,4},{0.45f,0.60f,0.6f,0.5f,3},
    {0.50f,0.65f,0.9f,0.4f,2},{0.25f,0.45f,0.4f,0.2f,6},{0.35f,0.55f,0.7f,0.3f,4},
    {0.50f,0.65f,0.3f,0.3f,4},{0.18f,0.25f,0.2f,0.4f,8},{0.55f,0.70f,0.7f,0.5f,2},
    {0.42f,0.55f,0.8f,0.4f,3},{0.38f,0.60f,0.7f,0.3f,4}};
// genre-drumtemplates ARRANGEMENT (16 steps)
static const uint16_t kGKick[14]={0x8424,0x9084,0x8424,0x8421,0x1111,0x8424,0x9494,0x8424,0x8421,0x8424,0x8421,0x9090,0x8424,0x8421};
static const uint16_t kGSnar[14]={0x1010,0x1010,0x1010,0x0010,0x1010,0x1818,0x1090,0x1010,0x1010,0x1010,0x1010,0x1010,0x1818,0x0010};
static const uint16_t kGHats[14]={0xAAAA,0xEEBE,0xA2AA,0xAAAA,0xAAAA,0xAAAA,0xB2AA,0x2828,0xA2A2,0xAAAA,0xAAAA,0xAEAE,0xAAAA,0xAA8A};

static const int kMaj[7]={0,2,4,5,7,9,11};
static const int kMin[7]={0,2,3,5,7,8,10};
static const float kMajW[12]={6.35f,2.23f,3.48f,2.33f,4.38f,4.09f,2.52f,5.19f,2.39f,3.66f,2.29f,2.88f};
static const float kMinW[12]={6.33f,2.68f,3.52f,5.38f,2.60f,3.53f,2.54f,4.75f,3.98f,2.69f,3.34f,3.17f};
static const int kBassSeq[8]={0,0,4,0,3,2,4,0};

// kind: 0=kick 1=snare 2=hat 3=bass 4=mel/chord 5=counter 6=chord 7=cc11
struct Ev{uint8_t kind,note,vel;uint32_t at,dur;};
struct RecN{uint8_t note,vel;int64_t on,off;};

class BandPlugin : public Plugin {
public:
    enum Params {
        kVolh=0,kDurf,kHeft,kMens,kSwing,kLen,kVar,
        kDice,kPreset,kMode,kOn,kKracht,
        kSpel,kLuister,kGen,kMid,kRefine,kMidiOut,
        kLevel,kOKey,kOConf,kONote,kOBar,kOBpm,kOStat,kOKlas,kOHits,
        kOV0,kOV1,kOV2,kOV3,kOV4,kOV5,kOV6,kOGenre,kOSpel,kOBus,
        kNumParams };
    BandPlugin() : Plugin(kNumParams,0,0) {}
protected:
    const char* getLabel() const override { return "G2B Band"; }
    const char* getDescription() const override {
        return "Jouw band: speelt mee, schrijft je vervolg en maakt er een heel nummer van."; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override {
#ifdef G2B_GIO
        return d_cconst('G','2','B','g');
#else
        return d_cconst('G','2','B','d');
#endif
    }

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints=kParameterIsAutomatable;
        auto pct=[&](const char*n,const char*s,float d){p.name=n;p.symbol=s;p.ranges.min=0;p.ranges.max=100;p.ranges.def=d;p.unit="%";};
        auto out=[&](const char*n,const char*s,float mn,float mx,float d){p.name=n;p.symbol=s;p.hints=kParameterIsOutput;p.ranges.min=mn;p.ranges.max=mx;p.ranges.def=d;};
        auto boo=[&](const char*n,const char*s,bool trig,float d){p.name=n;p.symbol=s;p.hints|=kParameterIsBoolean|(trig?kParameterIsBoolean:0);p.ranges.min=0;p.ranges.max=1;p.ranges.def=d;};
        switch(i){
        case kVolh:  pct("VOLHEID","volh",55);break;
        case kDurf:  pct("DURF","durf",35);break;
        case kHeft:  pct("HEFT","heft",50);break;
        case kMens:  pct("MENS","mens",35);break;
        case kSwing: pct("SWING","swing",20);break;
        case kLen:   p.name="LENGTE";p.symbol="lengte";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=3;p.ranges.def=1;break;
        case kVar:   pct("VAR","var",30);break;
        case kDice:  p.name="DICE";p.symbol="dice";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;break;
        case kPreset:p.name="GENRE";p.symbol="genre";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=(float)(kNumProgs-1);break;
        case kMode:  p.name="MODUS";p.symbol="modus";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=2;break;
        case kOn:    boo("Aan","aan",false,1);break;
        case kKracht:p.name="Volume";p.symbol="kracht";p.ranges.min=0;p.ranges.max=100;p.ranges.def=100;p.unit="%";break;
        case kSpel:  boo("SPEL","spel",false,0);break;
        case kLuister:boo("LUISTER","luister",false,0);break;
        case kGen:   boo("GEN","gen",true,0);break;
        case kMid:   boo("MID","mid",true,0);break;
        case kRefine:boo("REFINE","refine",true,0);break;
        case kMidiOut:boo("MIDI UIT","midiuit",false,1);break;
        case kLevel: out("LEVEL","level",0,1,0);break;
        case kOKey:  out("OKEY","okey",-1,23,-1);break;
        case kOConf: out("OCONF","oconf",0,100,0);break;
        case kONote: out("ONOTE","onote",0,128,0);break;
        case kOBar:  out("OBAR","obar",0,16,0);break;
        case kOBpm:  out("OBPM","obpm",0,240,120);break;
        case kOStat: out("OSTAT","ostat",0,5,0);break;
        case kOKlas: out("OKLAS","oklas",0,5,0);break;
        case kOHits: out("OHITS","ohits",0,4096,0);break;
        case kOV0: out("OV0","ov0",0,100,55);break;
        case kOV1: out("OV1","ov1",0,100,35);break;
        case kOV2: out("OV2","ov2",0,100,50);break;
        case kOV3: out("OV3","ov3",0,100,35);break;
        case kOV4: out("OV4","ov4",0,100,20);break;
        case kOV5: out("OV5","ov5",0,3,1);break;
        case kOV6: out("OV6","ov6",0,100,30);break;
        case kOGenre: out("OGENRE","ogenre",0,13,0);break;
        case kOBus: out("OBUS","obus",0,1,0);break;
        case kOSpel: out("OSPEL","ospel",0,1,0);break;
        default:break;}
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){
        case kVolh:case kOV0:return fVolh;case kDurf:case kOV1:return fDurf;
        case kHeft:case kOV2:return fHeft;case kMens:case kOV3:return fMens;
        case kSwing:case kOV4:return fSwing;case kLen:case kOV5:return fLen;
        case kVar:case kOV6:return fVar;
        case kPreset:case kOGenre:return fPreset;case kMode:return (float)fMode;
        case kOn:return fOn;case kKracht:return fKr;
        case kSpel:return fSpel;case kOBus:return fBusSync;case kOSpel:return fSpel;case kLuister:return fLuist;case kMidiOut:return fMidi;
        case kLevel:return fLevel;
        case kOKey:
            if(fMode==2)return fKey;
            return fConf>0?(float)(fRoot+12*fMinor):-1.f;
        case kOConf:return fConf;case kONote:return (float)fNoteNow;
        case kOBar:return (float)fStepInBar;case kOBpm:return (float)fBpm;
        case kOStat:return (float)fStat;case kOKlas:return fKlas;
        case kOHits:
            if(fMode==0)return (float)std::min<uint64_t>(recN_,4096);
            if(fMode==1)return (float)(fLuist>0.5f?fCRecN:fGenN);
            return fHits;
        default:return 0;}
    }
    void setParameterValue(uint32_t i, float v) override {
        switch(i){
        case kVolh:fVolh=v;break;case kDurf:fDurf=v;break;case kHeft:fHeft=v;break;
        case kMens:fMens=v;break;case kSwing:fSwing=v;break;case kLen:fLen=v;break;case kVar:fVar=v;break;
        case kDice:if(v>0.5f)rollDice();break;
        case kPreset:fPreset=std::max(0.f,std::min((float)(int)(v+0.5f),(float)(kNumProgs-1)));break;
        case kMode:{int m=(int)(v+0.5f);if(m<0)m=0;if(m>2)m=2;
            if(m!=fMode){fMode=m;fSpel=0;fLuist=0;fStat=0;fPanic=1;fEvPlay=false;}break;}
        case kOn:fOn=v;if(v<0.5f)fPanic=1;break;
        case kKracht:fKr=v;break;
        case kSpel:
            if(fMode==0){fSpel=v;if(v>0.5f){fWantSpelStart=1;fStat=3;fBandNewer=true;}else{fStat=0;fPanic=1;}}
            else if(fMode==1){
                if(v>0.5f){if(fGenN>0){fSpel=1;fWantPlayV=1;fStat=3;}else{fSpel=0;fStat=5;}}
                else{fSpel=0;fEvPlay=false;fPanic=1;if(fGenN>0)fStat=0;}
            }else if(fMode==2){fSpel=v;if(v<0.5f)fPanic=1;}
            break;
        case kLuister:
            if(fMode==1){
                if(v>0.5f&&fLuist<0.5f){fWantRecV=1;fStat=1;fEvPlay=false;fSpel=0;}
                else if(v<0.5f&&fLuist>0.5f){fStat=0;}
                fLuist=v;
            }else if(fMode==2){
                if(v>0.5f&&fLuist<0.5f){fRecN=0;fStat=1;fBandNewer=false;fArrReady=false;}
                if(v<0.5f&&fLuist>0.5f){fWantAna=1;}
                fLuist=v;
            }break;
        case kGen:if(v>0.5f){
            if(fMode==1){fLuist=0;fWantGenV=1;}
            else if(fMode==2){fLuist=0;fWantArrGen=1;}
            else if(fMode==0){fSpel=1;fWantSpelStart=1;fStat=3;fBandNewer=true;}
            }break;
        case kMid:if(v>0.5f)fWantMid=1;break;
        case kRefine:if(v>0.5f)fWantRefine=1;break;
        case kMidiOut:fMidi=v;break;
        default:break;}
    }
    void initState(uint32_t,String&,String&) override {}
    void setState(const char*,const char*) override {}

    void activate() override {
        fAbs=0;fEvI=0;fEvN=0;fHeldN=0;fSpelStart=0;fCRecN=0;fGenN=0;fGEvI=0;fEvPlay=false;
        fRecN=0;fLoopPos=0;fNIdx=0;fArrReady=false;recN_=0;recW_=0;
        fStep16=getSampleRate()*60.0/120.0/4.0;gridReset();genBar();
        for(auto&o:offq_)o.note=0;
        for(auto&v:fV)v.on=false;
    }

    // ================= stemmen (gedeelde preview-synth) =================
    struct Voice{bool on=false;float freq=0,ph=0,amp=0,dec=0,lp=0,att=0;int type=0;};
    Voice fV[24];
    Voice* freeVoice(){for(auto&v:fV)if(!v.on)return &v;
        Voice*q=&fV[0];for(auto&v:fV)if(v.amp<q->amp)q=&v;return q;}
    void pluck(int note,int vel,float dur){
        Voice*v=freeVoice();v->on=true;v->type=0;v->freq=440.f*powf(2.f,(note-69)/12.f);v->ph=0;
        v->amp=vel/127.f*0.55f;v->dec=expf(-1.f/(std::max(0.12f,dur)*getSampleRate()));v->lp=0;v->att=0;}
    void drumHit(int kind,int vel){
        Voice*v=freeVoice();v->on=true;v->type=kind+10;v->amp=vel/127.f*(kind==2?0.35f:0.8f);v->att=0;
        v->dec=expf(-1.f/((kind==0?0.18f:kind==1?0.12f:0.04f)*getSampleRate()));
        v->freq=kind==0?150.f:220.f;v->ph=0;v->lp=0;}
    void emitMidi(uint8_t st,uint8_t nt,uint8_t vel,uint32_t fr){
        MidiEvent e{};e.frame=fr;e.size=3;e.data[0]=st;e.data[1]=nt;e.data[2]=vel;writeMidiEvent(e);}
    void queueOff(uint8_t note,uint8_t ch,int64_t at){
        for(auto&o:offq_)if(!o.note){o.note=note;o.ch=ch;o.at=at;return;}
        offq_[offw_].note=note;offq_[offw_].ch=ch;offq_[offw_].at=at;offw_=(offw_+1)%kOffN;}

    // SPEL/VERVOLG event afvuren (drums ch10, bas ch2, melodie/akkoord ch3, counter ch4)
    void fire(const Ev&e,uint32_t i,bool record){
        if(e.kind==7){if(fMidi>0.5f)emitMidi(0xB0|2,11,e.vel,i);return;}
        int note=e.note;
        if(e.kind<=2){drumHit(e.kind,e.vel);note=e.kind==0?36:(e.kind==1?38:42);}
        else pluck(note,e.vel,(float)e.dur/getSampleRate());
        fNoteNow=note;
        const uint8_t ch=e.kind<=2?9:(e.kind==3?1:(e.kind==5?3:2));
        if(record){MidiRec&r=rec_[recW_];r.fr=fAbs+i;r.note=(uint8_t)note;r.ch=ch;r.vel=e.vel;
            r.dur=(uint32_t)std::max<uint32_t>(1,e.dur);recW_=(recW_+1)%4096;recN_++;}
        if(fMidi>0.5f){emitMidi(0x90|ch,(uint8_t)note,e.vel,i);
            queueOff((uint8_t)note,ch,fAbs+i+std::max<uint32_t>(e.dur,64));}
    }
    uint32_t frand(){fSeed=fSeed*1664525u+1013904223u;return fSeed>>9;}
    void rollDice(){
        if(fMode==0){fVolh=20.f+(frand()%800)/10.f;fMens=10.f+(frand()%700)/10.f;fSwing=(frand()%500)/10.f;fPreset=(float)(frand()%kNumProgs);}
        else if(fMode==1){fDurf=10.f+(frand()%800)/10.f;fMens=10.f+(frand()%700)/10.f;fSwing=(frand()%500)/10.f;fPreset=(float)(frand()%kNumProgs);}
        else{fRnd=fRnd*1664525u+1013904223u;fHeft=20+(fRnd>>8)%70;fVar=10+(fRnd>>14)%60;}
    }
    int scaleDeg(int deg)const{
        const int*sc=fMinor?kMin:kMaj;int oct=deg/7,d=deg%7;if(d<0){d+=7;oct--;}
        return sc[d]+12*oct;}

    // ================= MIDI-in: akkoord-tracker + VERVOLG-opname =================
    void noteOn(int note,int vel){
        if(fMode==1&&fLuist>0.5f&&fCRecN<512){
            bool dup=false;
            for(int i=0;i<fCRecN;i++)if(fCRec[i].note==note&&fCRec[i].off<0){fCRec[i].vel=vel;dup=true;break;}
            if(!dup){RecN&r=fCRec[fCRecN++];r.note=note;r.vel=vel;r.on=fAbs;r.off=-1;
                if(fStep16>0){double pos=(r.on-fRecStart)/fStep16;gridTrain((int)pos,pos,vel);}}
        }
        for(int i=0;i<fHeldN;i++)if(fHeld[i].note==note){fHeld[i].vel=vel;fHeld[i].at=fAbs;chordDetect();return;}
        if(fHeldN<16){Held&h=fHeld[fHeldN++];h.note=note;h.vel=vel;h.at=fAbs;}
        chordDetect();
    }
    void noteOff(int note){
        if(fMode==1&&fLuist>0.5f)for(int i=0;i<fCRecN;i++)
            if(fCRec[i].note==note&&fCRec[i].off<0){fCRec[i].off=fAbs;break;}
        for(int i=0;i<fHeldN;i++)if(fHeld[i].note==note){fHeld[i]=fHeld[--fHeldN];break;}
    }
    void chordDetect(){
        if(fHeldN==0||fMode!=0)return;
        int lo=127;for(int i=0;i<fHeldN;i++)if(fHeld[i].note<lo)lo=fHeld[i].note;
        fRoot=lo%12;bool b3=false,m3=false;
        for(int i=0;i<fHeldN;i++){int r=((fHeld[i].note-lo)%12+12)%12;if(r==4)b3=true;if(r==3)m3=true;}
        fMinor=m3&&!b3?1:0;fConf=fHeldN>=3?90.f:fHeldN*30.f;
    }

    // ================= MODUS 0: SPEL (DJEBU) =================
    void pushEv(int kind,int note,int vel,int step){
        if(fEvN>=96)return;Ev&e=evs_[fEvN++];
        e.kind=(uint8_t)kind;e.note=(uint8_t)note;e.vel=(uint8_t)vel;e.at=(uint32_t)step;e.dur=0;}
    void genBar(){
        fEvN=0;const GStijl&st=kStijl[(int)fPreset];const float dens=fVolh/100.f;
        uint32_t r=fSeed*2654435761u+fBar*97u+7u;
        for(int s=0;s<16;s++){
            float rnd=(r=(r*1664525u+1013904223u),((r>>9)&0xFFFF)/65535.f);
            if(s==0||rnd<st.kick*dens+(st.sync>0.5f&&(s==6||s==11)?0.35f:0.f))pushEv(0,36,95+(int)(rnd*30),s);
            if(s==4||s==12||(st.sync>0.6f&&s==14&&rnd<0.3f))pushEv(1,38,85+(int)(rnd*35),s);
            rnd=(r=(r*1664525u+1013904223u),((r>>9)&0xFFFF)/65535.f);
            if(rnd<st.hat*dens&&(s%2==0||st.hat>0.6f))pushEv(2,42,55+(int)(rnd*45),s);
            if(s%2==0&&rnd<0.85f){
                int note=36+fRoot+scaleDeg(kBassSeq[s/2]);
                if(st.oct>0.4f&&((r>>9)%4)==0)note+=12;
                pushEv(3,note,70+(int)(rnd*40),s);}
            if(fHeldN>=2&&st.sync>0.5f&&(s==4||s==12)&&rnd<0.5f)
                for(int c=0;c<fHeldN&&c<4;c++)pushEv(4,fHeld[c].note,60+(int)(rnd*30),s);
        }
        float tJ=fMens/100.f*0.15f,vJ=fMens/100.f*16.f;
        for(int e=0;e<fEvN;e++){Ev&ev=evs_[e];int s=ev.at;
            float spos=s+(fSwing/100.f*0.5f)*(s%2==1?1.f:0.f)+(((frand()&0xFFFF)/65535.f)-0.5f)*tJ;
            ev.at=(uint32_t)std::max<int>(0,(int)(spos*fStep16));
            int vel=ev.vel+(int)(((frand()&0xFFFF)/65535.f-0.5f)*vJ);
            ev.vel=(uint8_t)(vel<20?20:(vel>127?127:vel));
            ev.dur=(uint32_t)((ev.kind<=2?1:2)*(float)fStep16*0.9f);}
        std::sort(evs_,evs_+fEvN,[](const Ev&a,const Ev&b){return a.at<b.at;});
        fBar++;
    }

    // ================= MODUS 1: VERVOLG (CO-PRODUCER) =================
    struct TFP{float velSpread,tShift;};TFP fGrid[16];
    void gridReset(){for(int i=0;i<16;i++){
        if(i%4==0)fGrid[i]={0.f,0.f};else if(i%2==0)fGrid[i]={-0.04f,0.008f};else fGrid[i]={-0.09f,0.025f};}}
    void gridTrain(int step,double pos,uint8_t vel){TFP&g=fGrid[step&15];
        g.velSpread=g.velSpread*0.75f+((vel/127.f)-0.5f)*0.25f;
        g.tShift=g.tShift*0.75f+(float)(pos-std::floor(pos+0.5f))*0.25f;}
    void analyzeV(){
        fBars=0;fVelMean=90.f;fVelStd=15.f;fDens=0.5f;
        int64_t lastOn=0;for(int i=0;i<fCRecN;i++)if(fCRec[i].on>lastOn)lastOn=fCRec[i].on;
        if(lastOn<=fRecStart)fCRecN=0;
        if(fCRecN<3)return;
        fBars=(int)std::max<int64_t>(2,(lastOn-fRecStart)/(int64_t)(16*fStep16)+1);
        if(fBars>8)fBars=8;
        float hist[12]={0};for(int i=0;i<fCRecN;i++)hist[fCRec[i].note%12]+=1.f;
        float best=-1e9f;fRoot=9;fMinor=0;
        for(int r=0;r<12;r++)for(int mm=0;mm<2;mm++){
            const float*w=mm?kMinW:kMajW;const int*sc=mm?kMin:kMaj;float s=0;
            for(int p=0;p<12;p++){int rel=((p-r)%12+12)%12;bool in=false;
                for(int d=0;d<7;d++)if(sc[d]==rel){in=true;break;}
                s+=hist[p]*(in?w[rel]:-1.f);}
            if(s>best){best=s;fRoot=r;fMinor=mm;}}
        float vm=0,vs=0;for(int i=0;i<fCRecN;i++)vm+=fCRec[i].vel;vm/=fCRecN;
        for(int i=0;i<fCRecN;i++){float d=fCRec[i].vel-vm;vs+=d*d;}
        fVelMean=vm;fVelStd=sqrtf(vs/fCRecN);
        fDens=fCRecN/(float)(fBars*16);if(fDens>1.5f)fDens=1.5f;
        for(int b=0;b<fBars&&b<8;b++){int lo=127;
            for(int i=0;i<fCRecN;i++){int bar=(int)((fCRec[i].on-fRecStart)/(16*(int64_t)fStep16));
                if(bar==b&&fCRec[i].note<lo)lo=fCRec[i].note;}
            fProg[b]=lo<127?(lo%12):fRoot;}
        memset(fMark,0,sizeof(fMark));
        for(int i=1;i<fCRecN;i++)if(fCRec[i].note>51&&fCRec[i-1].note>51){
            int d=(int)fCRec[i].note-(int)fCRec[i-1].note;if(d>6)d=6;if(d<-6)d=-6;fMark[d+6]++;}
        int tot=0;for(int d=0;d<13;d++)tot+=fMark[d];
        fConf=tot?std::min(95.f,40.f+tot*2.f):50.f;
    }
    void pushGen(int kind,int note,int vel,int64_t step,int64_t durSteps){
        if(fGenN>=1500)return;Ev&e=gen_[fGenN++];
        e.kind=(uint8_t)kind;e.note=(uint8_t)note;e.vel=(uint8_t)(vel<20?20:(vel>127?127:vel));
        e.at=(uint32_t)(step*fStep16);e.dur=(uint32_t)(durSteps*fStep16*0.9f);}
    int markDelta(){
        const float temp=std::max(0.05f,fDurf/100.f);float w[13],sum=0;
        for(int d=0;d<13;d++){w[d]=fMark[d]>0?expf(logf((float)fMark[d])/temp):0.f;sum+=w[d];}
        if(sum<=0.f)return ((int)(frand()%5))-2;
        float pick=((frand()&0xFFFF)/65535.f)*sum,acc=0;
        for(int d=0;d<13;d++){acc+=w[d];if(pick<acc)return d-6;}
        return 0;}
    void generateV(){
        fGenN=0;const GStijl&st=kStijl[(int)fPreset];
        const int B=fBars;const float temp=fDurf/100.f;  // vervolg = even lang als jouw deel
        int melLast=fRoot+60;
        for(int i=fCRecN-1;i>=0;i--)if(fCRec[i].note>=55){melLast=fCRec[i].note;break;}
        for(int b=0;b<B&&b<16;b++){
            int cr=fProg[b%fBars];
            if(temp>0.4f&&b==B-1)cr=(fProg[(b-1<0?0:b-1)%fBars]+7)%12;
            const int proot=48+cr;const bool tension=(b>=B-2);
            for(int s=0;s<16;s++){
                uint32_t r=frand();float rnd=((r>>9)&0xFFFF)/65535.f;
                const int64_t step=(int64_t)b*16+s;
                float mw=s==0?1.f:(s==8?0.9f:(s==4||s==12?0.7f:0.35f));
                if(s==0||rnd<st.kick*mw+(st.sync>0.5f&&(s==6||s==11)?0.3f:0.f))pushGen(0,36,95+(int)(rnd*30),step,1);
                if(s==4||s==12||(tension&&s>=13&&rnd<0.5f))pushGen(1,38,85+(int)(rnd*35),step,1);
                rnd=((r=(r*1664525u+1013904223u),((r>>9)&0xFFFF)/65535.f));
                float hd=st.hat*(tension?1.3f:1.f);
                if(rnd<hd&&(s%2==0||st.hat>0.6f))pushGen(2,42,55+(int)(rnd*45),step,1);
                if(s%4==0&&rnd<0.8f){
                    int cand[3]={proot,proot+7,proot+scaleDeg(4)};int best=cand[0],bd=99;
                    for(int c=0;c<3;c++){int d=std::abs(cand[c]-fLastBass);if(d<bd){bd=d;best=cand[c];}}
                    fLastBass=best;int n=36+(best%24);
                    pushGen(3,n,(int)(fVelMean*0.8f),step,st.dur);}
                float dp=fDens*0.35f*mw;if(temp>0.6f)dp*=1.2f;
                if(rnd<dp&&s%2==0){
                    int d=markDelta();if(temp>0.5f&&(frand()&3)==0)d+=(frand()&1?12:-12);
                    melLast+=d;int pc=((melLast-fRoot)%12+12)%12;bool in=false;
                    const int*sc=fMinor?kMin:kMaj;for(int k=0;k<7;k++)if(sc[k]==pc){in=true;break;}
                    if(!in)melLast+=(frand()&1?1:-1);
                    if(melLast<55)melLast+=12;if(melLast>84)melLast-=12;
                    int vel=(int)(fVelMean+(((frand()>>9)&0xFFFF)/65535.f-0.5f)*2.f*fVelStd);
                    pushGen(4,melLast,vel,step,2);
                    if(fDens<0.6f&&(frand()&3)==0)pushGen(5,melLast+scaleDeg(2)%12,vel-15,step+1,2);}
                if((s==4||s==12)&&st.sync>0.5f&&rnd<0.4f)
                    for(int c=0;c<3;c++)pushGen(6,60+cr+scaleDeg(c*2),60+(int)(rnd*30),step,2);
            }
            if(tension)for(int s=0;s<16;s+=4)pushGen(7,0,std::min(127,40+(s+(b-(B-2))*16)*3),(int64_t)b*16+s,1);
        }
        float tJ=fMens/100.f*0.15f,vJ=fMens/100.f*fVelStd;
        for(int e=0;e<fGenN;e++){Ev&ev=gen_[e];int s=(int)(ev.at/fStep16);const TFP&g=fGrid[s&15];
            float spos=(float)s+(fSwing/100.f*0.5f)*(s%2==1?1.f:0.f)+(((frand()&0xFFFF)/65535.f)-0.5f)*tJ+g.tShift;
            ev.at=(uint32_t)std::max<int>(0,(int)(spos*fStep16));
            if(ev.kind!=7){int vel=ev.vel+(int)(g.velSpread*24.f+((frand()&0xFFFF)/65535.f-0.5f)*vJ);
                ev.vel=(uint8_t)(vel<20?20:(vel>127?127:vel));}}
        std::sort(gen_,gen_+fGenN,[](const Ev&a,const Ev&b){return a.at<b.at;});
        fGenLen=(int64_t)(B*16*fStep16);
    }

    // ================= MODUS 2: ARRANGEMENT (TREIN) =================
    struct N{uint32_t at,dur;uint8_t note,vel,ch;}; // ch 0=drums 1=melodie 2=bas (intern)
    void analyzeArr(){
        fStat=2;const float* b=fRec;int n=(int)fRecN;
        if(n<4800){fStat=5;fKlas=0;return;}
        const float sr=getSampleRate();const int HOP=512,FR=1024;int nf=(n-FR)/HOP;
        if(nf<8){fStat=5;fKlas=0;return;}
        std::vector<float> eL(nf),eM(nf),eH(nf);float lp1=0,lp2=0;
        const float aL=1.f-expf(-2.f*(float)M_PI*200.f/sr),aM=1.f-expf(-2.f*(float)M_PI*4000.f/sr);
        for(int f=0;f<nf;f++){float l=0,m=0,h=0;const float*s=b+f*HOP;
            for(int i=0;i<FR;i++){float x=s[i];lp1+=aL*(x-lp1);lp2+=aM*(x-lp2);
                float lo=lp1,hi=x-lp2,mi=lp2-lp1;l+=lo*lo;m+=mi*mi;h+=hi*hi;}
            eL[f]=l;eM[f]=m;eH[f]=h;}
        std::vector<int> ons,cls;
        for(int f=1;f<nf;f++){float dL=eL[f]-eL[f-1],dM=eM[f]-eM[f-1],dH=eH[f]-eH[f-1];
            float e=(eL[f]+eM[f]+eH[f])+1e-9f;float flux=std::max(dL,std::max(dM,dH))/e;
            if(flux>0.35f&&(ons.empty()||f-ons.back()>2)){ons.push_back(f);cls.push_back(dL>=dM&&dL>=dH?0:(dM>=dH?1:2));}}
        fHits=(float)ons.size();
        int tc=0;std::vector<float> pitches;
        for(int f=0;f<nf;f+=4){const float*s=b+f*HOP;float e0=0;for(int i=0;i<FR;i++)e0+=s[i]*s[i];
            if(e0<0.001f*FR)continue;float best=0;int blag=0;
            for(int lag=40;lag<600;lag+=2){float c=0;for(int i=0;i<FR-lag;i++)c+=s[i]*s[i+lag];if(c>best){best=c;blag=lag;}}
            if(blag>0&&best/(e0+1e-9f)>0.5f){tc++;pitches.push_back(sr/(float)blag);}}
        float tonalFrac=nf>=4?(float)tc/(float)(nf/4):0.f;
        if(ons.size()>=4&&!fBpmFromHost){float bestB=0,bs=0;
            for(float bb=70;bb<180;bb+=0.5f){float sc=0,beat=60000.f/bb;
                for(size_t i=1;i<ons.size();i++){float d=(ons[i]-ons[i-1])*HOP*1000.f/sr;
                    float r=fmodf(d+beat*0.5f,beat)-beat*0.5f;if(fabsf(r)<beat*0.15f)sc+=1.f-fabsf(r)/(beat*0.15f);}
                if(sc>bs){bs=sc;bestB=bb;}}
            if(bs>1.f)fBpm=bestB;}
        fStep16=sr*60.0/fBpm/4.0;fKey=-1;
        if(tonalFrac>0.25f&&!pitches.empty()){int hist[12]={0};
            for(float p:pitches){int pc=(int)(69.5f+12.f*log2f(p/440.f))%12;if(pc<0)pc+=12;hist[pc]++;}
            int bs=-1,bk=-1;
            for(int root=0;root<12;root++)for(int mn=0;mn<2;mn++){const int*sc=mn?kMin:kMaj;int s=0;
                for(int g=0;g<7;g++)s+=hist[(root+sc[g])%12];if(s>bs){bs=s;bk=root+mn*12;}}
            fKey=(float)bk;}
        if(ons.size()<=2&&tonalFrac<0.25f)fKlas=3;else if(tonalFrac>0.4f&&ons.size()<8)fKlas=2;
        else if(tonalFrac>0.4f)fKlas=4;else fKlas=1;
        fLastOns=ons;fLastCls=cls;fLastPit=pitches;fArrFromBand=false;
        generateArr(ons,cls,pitches,false);fStat=3;
    }
    void generateArr(const std::vector<int>&ons,const std::vector<int>&cls,const std::vector<float>&pitches,bool refine){
        fNotes.clear();
        const int barsL[4]={2,4,8,16};const int bars=barsL[std::max(0,std::min(3,(int)(fLen+0.5f)))];
        const int steps=bars*16;const int gi=std::max(0,std::min(kNumProgs-1,(int)(fPreset+0.5f)));
        const float dens=fHeft/100.f,var=fVar/100.f,mens=fMens/100.f;
        uint32_t rng=fRnd;if(refine)rng=rng*1664525u+1013904223u;
        auto rnd=[&](){rng=rng*1664525u+1013904223u;return (rng>>9&0xFFFF)/65535.f;};
        auto addN=[&](int step,int note,int vel,int dur16,int ch){
            if(step<0||step>=steps)return;N n{};
            n.at=(uint32_t)std::max(0.0,step*fStep16+mens*(rnd()-0.5f)*fStep16*0.25f);
            n.dur=(uint32_t)(dur16*fStep16*0.9f);n.note=(uint8_t)note;
            n.vel=(uint8_t)std::max(1,std::min(127,vel+(int)(mens*(rnd()-0.5f)*30)));n.ch=(uint8_t)ch;fNotes.push_back(n);};
        bool wantDrums=(fKlas==1||fKlas==3||fKlas==4),wantMel=(fKlas==2||fKlas==3||fKlas==4);
        // BUS-SINK: melodie-seed van G2B GELUID->MIDI als die gepubliceerd heeft
        std::vector<N> busMel;
        if(void*bp=g2bBusOpen()){G2BBus*b=(G2BBus*)bp;
            if(b->midiMagic==kMidiMagic&&b->midiCount>0){
                if(b->midiGenre<(uint32_t)kNumProgs)fPreset=(float)b->midiGenre; // genre meenemen
                for(uint32_t i=0;i<b->midiCount&&i<256;i++){
                    N n{};n.at=(uint32_t)(b->midiAt16[i]*fStep16);
                    n.dur=(uint32_t)(fStep16*0.9f);n.note=(uint8_t)b->midiNote[i];
                    n.vel=(uint8_t)b->midiVel[i];n.ch=1;busMel.push_back(n);}
            }
            g2bBusClose(bp);}
        if(!busMel.empty()){wantMel=true;fBusSync=1.f;}
        else fBusSync=0.f;
        if(wantDrums){
            if(fKlas!=3)for(size_t i=0;i<ons.size();i++){int st=(int)(ons[i]*512.f/fStep16+0.5f);int c=cls[i];
                addN(st,c==0?36:(c==1?38:42),c==0?110:(c==1?105:85),1,0);}
            for(int s=0;s<steps;s++){int m=s%16;float need=dens*0.9f;
                if((kGHats[gi]>>m)&1){if(rnd()<need+0.3f)addN(s,42,60+(int)(rnd()*30),1,0);}
                if((kGKick[gi]>>m)&1){if(rnd()<(fKlas==3?0.9f:need*0.4f))addN(s,36,110,1,0);}
                if((kGSnar[gi]>>m)&1){if(rnd()<0.95f)addN(s,38,105,1,0);}
                if(rnd()<var*0.08f)addN(s,rnd()<0.5f?45:46,80,1,0);}
        }
        if(wantMel){
            int root=fKey>=0?60+((int)fKey)%12:60;bool minor=fKey>=12;const int*sc=minor?kMin:kMaj;
            if(!busMel.empty()){
                // bus-patroon (64 stappen) herhaalt door het arrangement; opbouw in vel
                for(const N&bn:busMel){int bstep=(int)(bn.at/fStep16);
                    for(int rep=0;bstep+rep*64<steps;rep++){
                        float g=0.75f+0.5f*((bstep+rep*64)/(float)steps);
                        addN(bstep+rep*64,bn.note,(int)(bn.vel*g),1,1);}}
            }
            else if(fKlas==3||pitches.empty()){int mot[8]={0,7,12,7,0,10,12,0};
                for(int s=0;s<steps;s+=2)if(rnd()<0.5f+dens*0.4f)addN(s,root+mot[(s/2)%8],90,2,1);}
            else for(size_t i=0;i<pitches.size()&&i<512;i++){
                int midi=(int)(69.5f+12.f*log2f(pitches[i]/440.f));int st=(int)(i*4*512.f/fStep16+0.5f);addN(st,midi,95,2,1);}
            for(int b=0;b<bars;b++){addN(b*16,root-12,100,4,2);
                if(rnd()<dens)addN(b*16+8,root-12+sc[(b%3)*2]%12,95,4,2);}
        }
        fLoopLen=(uint32_t)(steps*fStep16);fLoopPos=0;fNIdx=0;
        std::sort(fNotes.begin(),fNotes.end(),[](const N&a,const N&b){return a.at<b.at;});
        fRnd=rng;fArrReady=!fNotes.empty();
    }
    // ARRANGEMENT uit je band (VERVOLG of SPEL) i.p.v. audio
    bool arrFromBand(){
        fNotes.clear();const double barLen=16*fStep16;
        if(fGenN>0&&fGenLen>0){
            for(int e=0;e<fGenN;e++){const Ev&ev=gen_[e];if(ev.kind==7)continue;N n{};
                n.at=ev.at;n.dur=std::max<uint32_t>(ev.dur,64);n.vel=ev.vel;
                if(ev.kind<=2){n.ch=0;n.note=ev.kind==0?36:(ev.kind==1?38:42);}
                else{n.ch=ev.kind==3?2:1;n.note=ev.note;}
                if(n.at<(uint32_t)fGenLen)fNotes.push_back(n);}
            fLoopLen=(uint32_t)fGenLen;
        }else if(recN_>0){
            const int last=(recW_-1+4096)%4096;const int64_t lastFr=(int64_t)rec_[last].fr;
            int64_t totalBars=(int64_t)((lastFr-fSpelStart)/barLen)+1;if(totalBars<1)totalBars=1;
            int64_t useBars=std::min<int64_t>(4,totalBars);
            const int64_t start=fSpelStart+(int64_t)((totalBars-useBars)*barLen);
            const int64_t end=start+(int64_t)(useBars*barLen);
            const uint64_t cnt=std::min<uint64_t>(recN_,4096);
            for(uint64_t k=0;k<cnt;k++){const MidiRec&r=rec_[k];
                if((int64_t)r.fr<start||(int64_t)r.fr>=end)continue;N n{};
                n.at=(uint32_t)(r.fr-start);n.dur=r.dur;n.note=r.note;n.vel=r.vel;
                n.ch=r.ch==9?0:(r.ch==1?2:1);fNotes.push_back(n);}
            fLoopLen=(uint32_t)(useBars*barLen);
        }
        if(fNotes.empty()){fStat=5;return false;}
        std::sort(fNotes.begin(),fNotes.end(),[](const N&a,const N&b){return a.at<b.at;});
        fKlas=4;fKey=(float)(fRoot+12*fMinor);fHits=(float)fNotes.size();
        fLoopPos=0;fNIdx=0;fArrReady=true;fArrFromBand=true;fStat=3;return true;
    }

    // ================= export =================
    struct MidiRec{uint64_t fr;uint8_t note,ch,vel;uint32_t dur;};
    MidiRec rec_[4096];int recW_=0;uint64_t recN_=0;
    std::string outDir(){
        const char*home=std::getenv("USERPROFILE");if(!home||!*home)home=std::getenv("HOME");
        std::string h=home&&*home?home:".";std::string m=h+"/Music",d=m+"/G2B";
        #ifdef _WIN32
        mkdir(m.c_str());mkdir(d.c_str());
        #else
        mkdir(m.c_str(),0755);mkdir(d.c_str(),0755);
        #endif
        return d;}
    std::string stamp(){std::time_t tt=std::time(nullptr);std::tm*lt=std::localtime(&tt);char s[40];
        std::snprintf(s,sizeof(s),"%04d%02d%02d-%02d%02d%02d",lt->tm_year+1900,lt->tm_mon+1,lt->tm_mday,lt->tm_hour,lt->tm_min,lt->tm_sec);return s;}
    void openDir(const std::string&d){
        #ifdef _WIN32
        std::string c="explorer \""+d+"\"";
        #else
        std::string c="xdg-open \""+d+"\" >/dev/null 2>&1 &";
        #endif
        if(std::system(c.c_str())){}}
    void writeMidAll(){
        if(fMode==0)writeSpel();else if(fMode==1)writeVervolg();else writeArr();}
    void writeSpel(){
        if(recN_==0){fStat=5;return;}
        const double fpt=fStep16/120.0;
        smf::MidiFile mf;mf.setTicksPerQuarterNote(480);mf.addTempo(0,0,fBpm);
        const uint64_t n=std::min<uint64_t>(recN_,4096);const int start=(recN_<=4096)?0:recW_;
        const uint64_t base0=rec_[start].fr;
        const uint64_t base=base0-(uint64_t)std::fmod((double)(base0-(uint64_t)std::min<int64_t>(fSpelStart,(int64_t)base0)),16*fStep16);
        int seq=0;
        for(uint64_t k=0;k<n;k++){const MidiRec&r=rec_[(start+(int)k)%4096];
            smf::MidiEvent on(0x90|r.ch,r.note,r.vel);on.tick=(int)((r.fr-base)/fpt);on.seq=seq++;
            smf::MidiEvent off(0x80|r.ch,r.note,0);off.tick=(int)((r.fr-base+r.dur)/fpt);off.seq=seq++;
            mf[0].push_back(on);mf[0].push_back(off);}
        mf.sortTracks();const std::string d=outDir();
        mf.write(d+"/G2B-BAND-SPEL_"+stamp()+".mid");fStat=4;openDir(d);
    }
    void writeVervolg(){
        if(fGenN==0){fStat=5;return;}
        const double fpt=fStep16/120.0;
        smf::MidiFile mf;mf.setTicksPerQuarterNote(480);mf.addTempo(0,0,fBpm);int seq=0;
        for(int e=0;e<fGenN;e++){const Ev&ev=gen_[e];if(ev.kind==7)continue;
            int note=ev.kind<=2?(ev.kind==0?36:(ev.kind==1?38:42)):ev.note;
            uint8_t ch=ev.kind<=2?9:(ev.kind==3?1:(ev.kind==5?3:2));
            smf::MidiEvent on(0x90|ch,note,ev.vel);on.tick=(int)(ev.at/fpt);on.seq=seq++;
            smf::MidiEvent off(0x80|ch,note,0);off.tick=(int)((ev.at+ev.dur)/fpt);off.seq=seq++;
            mf[0].push_back(on);mf[0].push_back(off);}
        mf.sortTracks();const std::string d=outDir();
        mf.write(d+"/G2B-BAND-VERVOLG_"+stamp()+".mid");fStat=4;openDir(d);
    }
    void writeArr(){
        if(fNotes.empty()||fLoopLen==0||fStep16<=0){fStat=5;return;}
        const double fpt=fStep16/120.0;
        const int loopTicks=(int)(fLoopLen/fpt+0.5);const int bars=std::max(1,loopTicks/1920);
        static const char* kSec[5]={"INTRO","BUILD","DROP","BREAK","OUTRO"};
        static const int kSL[5]={1,1,2,1,1};
        int start[5]={0},tot=0;for(int i=0;i<5;i++){start[i]=tot;tot+=kSL[i];}
        smf::MidiFile mf;mf.addTracks(2);mf.setTicksPerQuarterNote(480);mf.addTempo(0,0,fBpm>10?fBpm:120);
        smf::MidiEvent tn;tn.tick=0;
        tn.makeTrackName("SECTIES");mf[0].push_back(tn);
        tn.makeTrackName("DRUMS");mf[1].push_back(tn);
        tn.makeTrackName("MELODIE+BAS");mf[2].push_back(tn);
        for(int i=0;i<5;i++)mf.addMarker(0,start[i]*loopTicks,kSec[i]);
        int seq=0;
        auto emit=[&](int tr,int tick,const N&n,int vel){vel=std::max(1,std::min(127,vel));
            const int ch=n.ch==0?9:n.ch;
            smf::MidiEvent on(0x90|ch,n.note,vel);on.tick=tick;on.seq=seq++;
            smf::MidiEvent off(0x80|ch,n.note,0);off.tick=tick+std::max(1,(int)(n.dur/fpt));off.seq=seq++;
            mf[tr].push_back(on);mf[tr].push_back(off);};
        auto secOK=[&](int sec,const N&n,int p)->int{
            bool drum=(n.ch==0),bass=(n.ch==2);int step=(p/120)%16,bar=(p/1920);
            switch(sec){
            case 0:if(drum)return (n.note==36&&step%8==0)?n.vel*65/100:0;if(bass)return 0;return n.vel*60/100;
            case 1:if(drum){if(n.note==38&&bar<bars-1)return 0;return n.vel*85/100;}return n.vel*80/100;
            case 2:return n.vel;
            case 3:if(drum)return (n.note==36&&step==0)?n.vel*70/100:0;if(bass)return 0;return n.vel*65/100;
            case 4:if(drum)return (n.note==36&&step==0)?std::max(20,n.vel*(70-bar*15)/100):0;if(bass)return 0;
                   return bar<bars/2?n.vel*50/100:0;}
            return 0;};
        for(const N&n:fNotes){int p=(int)(n.at/fpt)%loopTicks;if(p<0)p+=loopTicks;int tr=(n.ch==0)?1:2;
            for(int s=0;s<5;s++)for(int rep=0;rep<kSL[s];rep++){int v=secOK(s,n,p);if(!v)continue;
                int tk=(start[s]+rep)*loopTicks+p;
                if(tk>=start[2]*loopTicks-4*120&&tk<start[2]*loopTicks&&v>45)continue; // KLOOF-KNIP
                emit(tr,tk,n,v);}}
        for(int i=0;i<8;i++){smf::MidiEvent on(0x99,38,60+i*8);on.tick=start[2]*loopTicks-4*120+i*60;on.seq=seq++;
            smf::MidiEvent off(0x89,38,0);off.tick=on.tick+50;off.seq=seq++;mf[1].push_back(on);mf[1].push_back(off);}
        mf.sortTracks();const std::string d=outDir();
        mf.write(d+"/G2B-BAND-ARR_"+stamp()+".mid");
        FILE*f=std::fopen((d+"/G2B-BAND-KAART.txt").c_str(),"w");
        if(f){std::fprintf(f,"G2B BAND — MIX-KAART van je arrangement\n");
            std::fprintf(f,"Bron: %s\n",fArrFromBand?"jouw BAND (SPEL/VERVOLG)":"jouw audio-loop");
            std::fprintf(f,"BPM %.0f · loop %d maten · totaal %d maten\n\n",fBpm,bars,tot*bars);
            const char*adv[5]={"rustig: alleen kick + zachte melodie — houd het leeg",
                "opbouw: volledige drums vanaf laatste maat + fill naar de drop",
                "het moment: alles vol — jouw loop knalt hier",
                "ademruimte: melodie zacht, drums weg — spanning voor ronde 2",
                "afloop: kick verdwijnt maat voor maat — laat uitsterven"};
            int b0=0;for(int i=0;i<5;i++){std::fprintf(f,"maten %3d-%3d  %-6s  %s\n",b0+1,b0+kSL[i]*bars,kSec[i],adv[i]);b0+=kSL[i]*bars;}
            std::fprintf(f,"\nDrums staan op MIDI-kanaal 10 (GM), melodie kanaal 2, bas kanaal 3.\n");
            std::fprintf(f,"Tip: sleep de .mid in je DAW — de sectie-markers staan er al in.\n");
            std::fclose(f);}
        fStat=4;openDir(d);
    }

    // ================= run =================
    void run(const float** in, float** out, uint32_t frames, const MidiEvent* evs, uint32_t nev) override {
        const float sr=getSampleRate();
        if(fStep16<=0){fBpm=120.0;fStep16=sr*60.0/fBpm/4.0;}
        const TimePosition&tp=getTimePosition();
        bool hostPlay=false,playing=true;
        if(tp.bbt.valid&&tp.bbt.beatsPerMinute>1.0){
            fBpm=tp.bbt.beatsPerMinute;fStep16=sr*60.0/fBpm/4.0;fBpmFromHost=1;hostPlay=tp.playing;playing=tp.playing;}
        // ARRANGEMENT-opname vóór we out overschrijven (in/out kunnen gedeeld zijn)
        if(fMode==2&&fLuist>0.5f&&fStat==1){
            for(uint32_t i=0;i<frames&&fRecN<kRecMax;i++)fRec[fRecN++]=(in[0][i]+in[1][i])*0.5f;
            if(fRecN>=kRecMax){fLuist=0;fWantAna=1;}}
        if(in[0]!=out[0])std::memcpy(out[0],in[0],frames*sizeof(float));
        if(in[1]!=out[1])std::memcpy(out[1],in[1],frames*sizeof(float));

        if(fPanic){fPanic=0;for(auto&o:offq_)if(o.note){emitMidi(0x80|o.ch,o.note,0,0);o.note=0;}}
        if(fWantSpelStart){fWantSpelStart=0;fSpelStart=fAbs;fEvI=0;fEvN=0;fLastRelBar=0;fNeedBar=true;}
        if(fWantRecV){fWantRecV=0;fCRecN=0;fRecStart=fAbs;gridReset();}
        if(fWantGenV){fWantGenV=0;analyzeV();
            if(fBars>0){generateV();fPlayStart=fAbs;fGEvI=0;fLastRel=0;fEvPlay=true;fSpel=1;fStat=3;fBandNewer=true;}
            else{fStat=5;fConf=0;}}
        if(fWantPlayV){fWantPlayV=0;fPlayStart=fAbs;fGEvI=0;fLastRel=0;fEvPlay=true;}
        if(fWantAna){fWantAna=0;analyzeArr();}
        if(fWantArrGen){fWantArrGen=0;
            if(fBandNewer||fRecN<4800){if(!arrFromBand()&&fRecN>=4800)analyzeArr();}
            else analyzeArr();}
        if(fWantRefine){fWantRefine=0;
            if(fMode==2&&!fArrFromBand&&!fLastOns.empty())generateArr(fLastOns,fLastCls,fLastPit,true);
            else if(fMode==2&&fArrFromBand)arrFromBand();
            else if(fMode==1&&fBars>0){generateV();fPlayStart=fAbs;fGEvI=0;fLastRel=0;fEvPlay=fSpel>0.5f;}}
        if(fWantMid){fWantMid=0;writeMidAll();}

        for(uint32_t m=0;m<nev;m++){const MidiEvent&e=evs[m];
            if(e.size>=3){uint8_t st=e.data[0]&0xF0;
                if(st==0x90&&e.data[2]>0)noteOn(e.data[1],e.data[2]);
                else if(st==0x80||(st==0x90&&e.data[2]==0))noteOff(e.data[1]);}}

        const bool on=fOn>0.5f;
        const bool spelOn=on&&fMode==0&&(fSpel>0.5f||hostPlay);
        if(spelOn&&fSpel<0.5f&&!fHostSpel){fHostSpel=true;fSpelStart=fAbs;fEvI=0;fEvN=0;fNeedBar=true;}
        if(!hostPlay)fHostSpel=false;
        const bool vervOn=on&&fMode==1&&fEvPlay&&fGenLen>0;
        const bool arrOn=on&&fMode==2&&playing&&fArrReady&&fLoopLen>0&&fLuist<0.5f;
        float levelPeak=0.f;
        for(uint32_t i=0;i<frames;i++){
            const int64_t now=fAbs+i;
            if(spelOn&&fStep16>0){
                const int64_t barLen=(int64_t)(16*fStep16);
                int64_t rel=now-fSpelStart;if(rel<0)rel=0;const int64_t relBar=rel%barLen;
                if(fNeedBar||relBar<fLastRelBar){genBar();fEvI=0;fNeedBar=false;}
                fLastRelBar=relBar;fStepInBar=(int)(relBar/fStep16)%16;
                while(fEvI<fEvN&&(int64_t)evs_[fEvI].at<=relBar){fire(evs_[fEvI],i,true);fEvI++;}
            }
            if(vervOn){
                int64_t rel=now-fPlayStart;const int64_t relSec=rel%fGenLen;
                if(relSec<fLastRel)fGEvI=0;
                fLastRel=relSec;fStepInBar=(int)(relSec/(int64_t)fStep16)%16;
                while(fGEvI<fGenN&&(int64_t)gen_[fGEvI].at<=relSec){fire(gen_[fGEvI],i,false);fGEvI++;}
            }
            if(arrOn){
                const uint32_t pos=fLoopPos;
                while(fNIdx<fNotes.size()&&fNotes[fNIdx].at<=pos){const N&n=fNotes[fNIdx];
                    const uint8_t ch=n.ch==0?9:n.ch;
                    if(n.ch==0)drumHit(n.note==36?0:(n.note==38?1:2),n.vel);
                    else pluck(n.note,n.vel,(float)n.dur/sr);
                    fNoteNow=n.note;
                    if(fMidi>0.5f){emitMidi(0x90|ch,n.note,n.vel,i);queueOff(n.note,ch,now+std::max<uint32_t>(n.dur,64));}
                    fNIdx++;}
                fStepInBar=(int)(pos/fStep16)%16;
                if(++fLoopPos>=fLoopLen){fLoopPos=0;fNIdx=0;}
            }
            for(auto&o:offq_)if(o.note&&o.at<=now){emitMidi(0x80|o.ch,o.note,0,i);o.note=0;}
            float o=0.f;
            for(auto&v:fV){if(!v.on)continue;float s;
                if(v.type==0){s=sinf(v.ph*2.f*(float)M_PI)*0.7f+sinf(v.ph*4.f*(float)M_PI)*0.2f;v.lp+=0.3f*(s-v.lp);s=v.lp;}
                else if(v.type==10){s=sinf(v.ph*2.f*(float)M_PI);v.freq*=0.9985f;if(v.freq<45.f)v.freq=45.f;}
                else if(v.type==11){float nz=(((frand()&0xFFFF)/32768.f)-1.f);v.lp+=0.5f*(nz-v.lp);s=v.lp*0.6f+sinf(v.ph*2.f*(float)M_PI)*0.4f;}
                else{float nz=(((frand()&0xFFFF)/32768.f)-1.f);s=(nz-v.lp)*0.5f;v.lp=nz;}
                if(v.att<1.f){v.att+=1.f/96.f;if(v.att>1.f)v.att=1.f;}
                o+=s*v.amp*v.att;v.amp*=v.dec;v.ph+=v.freq/sr;if(v.ph>1.f)v.ph-=1.f;
                if(v.amp<0.0003f)v.on=false;}
            o*=0.6f;const float ao=fabsf(o);
            if(ao>0.8f)o=copysignf(0.8f+0.2f*tanhf((ao-0.8f)/0.2f),o);
            out[0][i]+=o;out[1][i]+=o;
            if(ao>levelPeak)levelPeak=ao;
        }
        fAbs+=frames;
        fLevel=fLevel*0.7f+std::min(1.f,levelPeak)*0.3f;
        g2b_post(in,out,frames,sr,fOn,fKr,fGal,nullptr,true);
    }

private:
    // knoppen
    float fVolh=55,fDurf=35,fHeft=50,fMens=35,fSwing=20,fLen=1,fVar=30;
    float fPreset=0,fOn=1,fKr=100,fSpel=0,fLuist=0,fMidi=1;
    int fMode=0;
    // flags (UI-thread → audio-thread)
    volatile int fPanic=0,fWantSpelStart=0,fWantRecV=0,fWantGenV=0,fWantPlayV=0,fWantAna=0,fWantArrGen=0,fWantRefine=0,fWantMid=0;
    int fStat=0;bool fBandNewer=false,fHostSpel=false,fNeedBar=true;
    int64_t fAbs=0;double fStep16=0,fBpm=120.0;int fBpmFromHost=0;
    uint32_t fSeed=12345u,fRnd=12345u;
    // gedeeld
    struct Held{uint8_t note,vel;int64_t at;};Held fHeld[16];int fHeldN=0;
    int fRoot=9,fMinor=0;float fConf=0.f,fBusSync=0.f;
    float fLevel=0.f;int fNoteNow=0,fStepInBar=0;
    // SPEL
    Ev evs_[96];int fEvN=0,fEvI=0,fBar=0;int64_t fSpelStart=0,fLastRelBar=0;
    // VERVOLG
    RecN fCRec[512];int fCRecN=0;int64_t fRecStart=0,fPlayStart=0,fLastRel=0,fGenLen=0;
    int fBars=0;float fVelMean=90.f,fVelStd=15.f,fDens=0.5f;
    int fProg[8]={0};int fMark[13]={0};int fLastBass=48;
    Ev gen_[1500];int fGenN=0,fGEvI=0;bool fEvPlay=false;
    // ARRANGEMENT
    static const uint32_t kRecMax=8*48000;
    float fRec[kRecMax];uint32_t fRecN=0;
    float fKlas=0,fKey=-1,fHits=0;
    uint32_t fLoopPos=0,fLoopLen=0;size_t fNIdx=0;bool fArrReady=false,fArrFromBand=false;
    std::vector<N> fNotes;std::vector<int> fLastOns,fLastCls;std::vector<float> fLastPit;
    // midi-offs (absolute tijd)
    static const int kOffN=256;
    struct PendingOff{uint8_t note=0,ch=0;int64_t at=0;};PendingOff offq_[kOffN];int offw_=0;
    Galaxy fGal;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandPlugin)
};

Plugin* createPlugin(){return new BandPlugin();}
END_NAMESPACE_DISTRHO
