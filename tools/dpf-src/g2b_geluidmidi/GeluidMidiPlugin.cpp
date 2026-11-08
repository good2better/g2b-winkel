/*
 * G2B GELUID→MIDI — 4-in-1: LUISTER + OPROEP + ANTWOORD (100% eigen code).
 * MODUS 0 LOOP     : speel een geluid → drums+melodie MIDI-loops (LUISTER-engine)
 * MODUS 1 MELODIE  : neurie/zing → pitch-track → strakke melodie-MIDI (OPROEP)
 * MODUS 2 INSTRUMENT: zing 1 toon → bespeelbaar instrument op je keyboard (OPROEP)
 * MODUS 3 ANTWOORD : speel 4 tellen MIDI → hij antwoordt 4 tellen terug (ANTWOORD)
 * Knoppen veranderen van betekenis per modus (zie UI-labels).
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
#include <ctime>
#include <sys/stat.h>

START_NAMESPACE_DISTRHO

#ifdef G2B_GIO
static const int kNumProgs=14;
#else
static const int kNumProgs=10;
#endif
static const char* kProgNames[14]={
    "TRAP","DRILL","BOOMBAP","RNB","HOUSE","DEMBOW","KAWINA",
    "LOFI","SOUL","POP","AMBIENT","SOCA","DANCEHALL","ZOUK"};

// genre-drumtemplates 16 steps (LUISTER/LOOP)
static const uint16_t kGKick[14]={0x8424,0x9084,0x8424,0x8421,0x1111,0x8424,0x9494,0x8424,0x8421,0x8424,0x8421,0x9090,0x8424,0x8421};
static const uint16_t kGSnar[14]={0x1010,0x1010,0x1010,0x0010,0x1010,0x1818,0x1090,0x1010,0x1010,0x1010,0x1010,0x1010,0x1818,0x0010};
static const uint16_t kGHats[14]={0xAAAA,0xEEBE,0xA2AA,0xAAAA,0xAAAA,0xAAAA,0xB2AA,0x2828,0xA2A2,0xAAAA,0xAAAA,0xAEAE,0xAAAA,0xAA8A};

// MELODIE-schalen (OPROEP)
static const int kNumScales=9;
static const char* kScaleNames[9]={
    "MAJEUR","MINEUR","PENTA","BLUES","CHROMA","DORIAN","PHRYG","MIXO","HARM"};
static const int kScales[9][7]={
    {0,2,4,5,7,9,11},{0,2,3,5,7,8,10},{0,2,4,7,9,0,0},
    {0,3,5,6,7,10,0},{0,1,2,3,4,5,6},{0,2,3,5,7,9,10},
    {0,1,3,5,7,8,10},{0,2,4,5,7,9,10},{0,2,3,5,7,8,11}};
static const int kScaleLen[9]={7,7,5,6,7,7,7,7,7};

// ANTWOORD genre-stijlen {dens,trip,sync,oct,dur16}
struct GStijl{float dens,trip,sync,oct;int dur;};
static const GStijl kStijl[kNumProgs]={
    {0.55f,0.8f,0.5f,0.7f,2},{0.45f,0.4f,0.7f,0.5f,3},{0.40f,0.1f,0.6f,0.2f,2},
    {0.35f,0.2f,0.8f,0.3f,4},{0.60f,0.0f,0.3f,0.4f,4},{0.50f,0.1f,0.6f,0.5f,3},
    {0.55f,0.1f,0.9f,0.4f,2},{0.30f,0.1f,0.4f,0.2f,6},{0.35f,0.2f,0.7f,0.3f,4},
    {0.50f,0.0f,0.3f,0.3f,4},
#ifdef G2B_GIO
    {0.22f,0.0f,0.2f,0.4f,8},{0.55f,0.1f,0.7f,0.5f,2},{0.45f,0.2f,0.8f,0.4f,3},
    {0.40f,0.1f,0.7f,0.3f,4}
#endif
};
static const int kMaj[7]={0,2,4,5,7,9,11};
static const int kMin[7]={0,2,3,5,7,8,10};

struct CallN{uint8_t note,vel;float pos16;uint16_t dur16;};
struct RespN{uint8_t note,vel;uint32_t at;uint32_t dur;};

class GeluidMidiPlugin : public Plugin {
public:
    enum Params {
        kHeft=0,kMens,kLen,kVar,kOct,kDice,kPreset,kSchaal,kMode,
        kOn,kLuister,kGen,kMidi,kRefine,kVraag,kAntLoop,
        kLevel,kOKlas,kOKey,kOBpm,kOStat,kOHits,kOFase,kOConf,kONote,
        kOHeft,kOMens,kOLen,kOVar,kOOct,kTerug,kLaad,kNumParams };
    GeluidMidiPlugin() : Plugin(kNumParams,0,0) { fBus=g2bBusOpen(); }
    ~GeluidMidiPlugin() { if(fBus)g2bBusClose(fBus); }
protected:
    const char* getLabel() const override { return "G2B Geluid-MIDI"; }
    const char* getDescription() const override {
        return "Geluid of spel erin → MIDI eruit: loop, melodie, instrument of antwoord."; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override {
#ifdef G2B_GIO
        return d_cconst('G','2','M','g');
#else
        return d_cconst('G','2','G','m');
#endif
    }

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints=kParameterIsAutomatable;
        switch(i){
        case kHeft:  p.name="HEFT";     p.symbol="heft";  p.ranges.min=0;p.ranges.max=100;p.ranges.def=50;p.unit="%";break;
        case kMens:  p.name="MENS";     p.symbol="mens";  p.ranges.min=0;p.ranges.max=100;p.ranges.def=30;p.unit="%";break;
        case kLen:   p.name="LENGTE";   p.symbol="lengte";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=3;p.ranges.def=1;break;
        case kVar:   p.name="VAR";      p.symbol="var";   p.ranges.min=0;p.ranges.max=100;p.ranges.def=30;p.unit="%";break;
        case kOct:   p.name="OCTAAF";   p.symbol="octaaf";p.hints|=kParameterIsInteger;p.ranges.min=-2;p.ranges.max=2;p.ranges.def=0;break;
        case kDice:  p.name="DICE";     p.symbol="dice";  p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;break;
        case kPreset:p.name="GENRE";    p.symbol="genre"; p.hints|=kParameterIsInteger;p.ranges.max=(float)(kNumProgs-1);break;
        case kSchaal:p.name="SCHAAL";   p.symbol="schaal";p.hints|=kParameterIsInteger;p.ranges.max=(float)(kNumScales-1);break;
        case kMode:  p.name="MODUS";    p.symbol="modus"; p.hints|=kParameterIsInteger;p.ranges.max=3;break;
        case kOn:    p.name="Aan";      p.symbol="aan";   p.hints|=kParameterIsBoolean;p.ranges.max=1;p.ranges.def=1;break;
        case kLuister:p.name="LUISTER"; p.symbol="luister";p.hints|=kParameterIsBoolean;p.ranges.max=1;break;
        case kGen:   p.name="GEN";      p.symbol="gen";   p.hints|=kParameterIsBoolean|kParameterIsBoolean;p.ranges.max=1;break;
        case kMidi:  p.name="MID";      p.symbol="mid";   p.hints|=kParameterIsBoolean|kParameterIsBoolean;p.ranges.max=1;break;
        case kRefine:p.name="REFINE";   p.symbol="refine";p.hints|=kParameterIsBoolean|kParameterIsBoolean;p.ranges.max=1;break;
        case kVraag: p.name="VRAAG";    p.symbol="vraag"; p.hints|=kParameterIsBoolean|kParameterIsBoolean;p.ranges.max=1;break;
        case kAntLoop:p.name="LOOP2";   p.symbol="loop2"; p.hints|=kParameterIsBoolean;p.ranges.max=1;break;
        case kLevel: p.name="LEVEL";    p.symbol="level"; p.hints=kParameterIsOutput;p.ranges.max=1;break;
        case kOKlas: p.name="OKLAS";    p.symbol="oklas"; p.hints=kParameterIsOutput;p.ranges.max=4;break;
        case kOKey:  p.name="OKEY";     p.symbol="okey";  p.hints=kParameterIsOutput;p.ranges.min=-1;p.ranges.max=23;p.ranges.def=-1;break;
        case kOBpm:  p.name="OBPM";     p.symbol="obpm";  p.hints=kParameterIsOutput;p.ranges.max=240;break;
        case kOStat: p.name="OSTAT";    p.symbol="ostat"; p.hints=kParameterIsOutput;p.ranges.max=6;break;
        case kOHits: p.name="OHITS";    p.symbol="ohits"; p.hints=kParameterIsOutput;p.ranges.max=999;break;
        case kOFase: p.name="OFASE";    p.symbol="ofase"; p.hints=kParameterIsOutput;p.ranges.max=3;break;
        case kOConf: p.name="OCONF";    p.symbol="oconf"; p.hints=kParameterIsOutput;p.ranges.max=100;break;
        case kONote: p.name="ONOTE";    p.symbol="onote"; p.hints=kParameterIsOutput;p.ranges.max=128;break;
        case kOHeft: p.name="OHEFT";    p.symbol="oheft"; p.hints=kParameterIsOutput;p.ranges.max=100;p.ranges.def=50;break;
        case kOMens: p.name="OMENS";    p.symbol="omens"; p.hints=kParameterIsOutput;p.ranges.max=100;p.ranges.def=30;break;
        case kOLen:  p.name="OLEN";     p.symbol="olen";  p.hints=kParameterIsOutput;p.ranges.max=3;p.ranges.def=1;break;
        case kOVar:  p.name="OVAR";     p.symbol="ovar";  p.hints=kParameterIsOutput;p.ranges.max=100;p.ranges.def=30;break;
        case kOOct:  p.name="OOCT";     p.symbol="ooct";  p.hints=kParameterIsOutput;p.ranges.min=-2;p.ranges.max=2;break;
        case kTerug: p.name="TERUG";    p.symbol="terug"; p.hints|=kParameterIsBoolean;p.ranges.max=1;break;
        case kLaad:  p.name="LAAD";     p.symbol="laad";  p.hints|=kParameterIsBoolean;p.ranges.max=1;break;
        default:break;}
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){
        case kHeft:return fHeft;case kMens:return fMens;case kLen:return fLen;
        case kVar:return fVar;case kOct:return fOct;case kDice:return 0;
        case kPreset:return fPreset;case kSchaal:return fSchaal;case kMode:return fMode;
        case kOn:return fOn;case kLuister:return fLuist;case kGen:return 0;
        case kMidi:return 0;case kRefine:return 0;case kVraag:return 0;case kAntLoop:return fAntLoop;
        case kLevel:return fLevel;case kOKlas:return fKlas;case kOKey:
            return fMode>2.5f?(float)(fAKey+12*fAMinor):fKey;
        case kOBpm:return fBpm;case kOStat:return fStat;case kOHits:return fHits;
        case kOFase:return (float)fFase;case kOConf:return fConf;case kONote:return (float)fNoteNow;
        case kOHeft:return fHeft;case kOMens:return fMens;case kOLen:return fLen;
        case kOVar:return fVar;case kOOct:return fOct;case kTerug:return 0;case kLaad:return 0;}return 0;
    }
    void setParameterValue(uint32_t i, float v) override {
        switch(i){
        case kHeft:fHeft=v;break;case kMens:fMens=v;break;case kLen:fLen=v;break;
        case kVar:fVar=v;break;case kOct:fOct=v;break;
        case kDice:if(v>0.5f){fRnd=fRnd*1664525u+1013904223u;fHeft=20+(fRnd>>8)%70;fVar=10+(fRnd>>14)%60;}break;
        case kPreset:fPreset=std::max(0.f,std::min(v,(float)(kNumProgs-1)));break;
        case kSchaal:fSchaal=std::max(0.f,std::min(v,(float)(kNumScales-1)));break;
        case kMode:fMode=std::max(0.f,std::min(3.f,v));fStat=0;fFase=0;fLuist=0;break;
        case kOn:fOn=v;break;
        case kLuister:
            if(fMode>2.5f){ // ANTWOORD: toggle fase 1
                if(v>0.5f){if(fFase==1)endLuister();else startLuister();fLuist=(fFase==1)?1.f:0.f;}
            }else{
                if(v>0.5f&&fLuist<0.5f){fRecN=0;fStat=1;}
                if(v<0.5f&&fLuist>0.5f){fWantAna=1;}
                fLuist=v;
            }break;
        case kGen:if(v>0.5f){if(fMode>2.5f&&fFase==1)endLuister();else fWantAna=1;}break;
        case kMidi:if(v>0.5f)fWantMid=1;break;
        case kRefine:if(v>0.5f)fWantRefine=1;break;
        case kVraag:if(v>0.5f&&fMode>2.5f)stelVraag();break;
        case kAntLoop:fAntLoop=v;break;
        case kTerug:if(v>0.5f)undo_();break;
        case kLaad:if(v>0.5f)fWantLoad=1;break;
        default:break;}
    }
    void initState(uint32_t,String&,String&) override {}
    void setState(const char*,const char*) override {}

    void activate() override {
        fRecN=0;fStat=0;fLoopPos=0;fBpm=120;fStep16=getSampleRate()*15.0/120.0;
        fLevelBlk=0;fNIdx=0;fSmpN=0;fSmpHz=0;fNotes.clear();
        fFase=0;fFaseLeft=0;fCallN=0;fRespN=0;fRespI=0;fAbs=0;recN_=0;recW_=0;
        std::memset(fOff,0,sizeof(fOff));
        for(auto&v:fIV)v.act=0;
        for(auto&v:fAV)v.on=false;
    }

    struct N{uint32_t at,dur;uint8_t note,vel,ch;};

    // ============ audio-analyse gedeeld ============
    float pitchOf(const float*s,int n,float sr){
        float e0=0;for(int i=0;i<n;i++)e0+=s[i]*s[i];
        if(e0<0.002f*n)return 0;
        float best=0;int blag=0;
        for(int lag=40;lag<700;lag+=2){
            float c=0;for(int i=0;i<n-lag;i++)c+=s[i]*s[i+lag];
            if(c>best){best=c;blag=lag;}
        }
        if(blag==0||best/(e0+1e-9f)<0.45f)return 0;
        return sr/(float)blag;
    }

    // ============ MODUS 0: LOOP (LUISTER) ============
    void analyzeLoop(){
        fStat=2;
        const float* b=fRec;int n=(int)fRecN;
        if(n<4800){fStat=0;fKlas=0;return;}
        const float sr=getSampleRate();
        const int HOP=512,FR=1024;int nf=(n-FR)/HOP;
        if(nf<8){fStat=0;fKlas=0;return;}
        std::vector<float> eL(nf),eM(nf),eH(nf);
        float lp1=0,lp2=0;
        const float aL=1.f-expf(-2.f*(float)M_PI*200.f/sr);
        const float aM=1.f-expf(-2.f*(float)M_PI*4000.f/sr);
        for(int f=0;f<nf;f++){
            float l=0,m=0,h=0;const float*s=b+f*HOP;
            for(int i=0;i<FR;i++){
                float x=s[i];lp1+=aL*(x-lp1);lp2+=aM*(x-lp2);
                float lo=lp1,hi=x-lp2,mi=lp2-lp1;
                l+=lo*lo;m+=mi*mi;h+=hi*hi;
            }
            eL[f]=l;eM[f]=m;eH[f]=h;
        }
        std::vector<int> ons,onsCls;
        for(int f=1;f<nf;f++){
            float dL=eL[f]-eL[f-1],dM=eM[f]-eM[f-1],dH=eH[f]-eH[f-1];
            float e=(eL[f]+eM[f]+eH[f])+1e-9f;
            float flux=std::max(dL,std::max(dM,dH))/e;
            if(flux>0.35f&&(ons.empty()||f-ons.back()>2)){
                int c=dL>=dM&&dL>=dH?0:(dM>=dH?1:2);
                ons.push_back(f);onsCls.push_back(c);
            }
        }
        fHits=(float)ons.size();
        float tonalE=0;int tc=0;std::vector<float> pitches;
        for(int f=0;f<nf;f+=4){
            const float*s=b+f*HOP;float e0=0;for(int i=0;i<FR;i++)e0+=s[i]*s[i];
            if(e0<0.001f*FR)continue;
            float best=0;int blag=0;
            for(int lag=40;lag<600;lag+=2){
                float c=0;for(int i=0;i<FR-lag;i++)c+=s[i]*s[i+lag];
                if(c>best){best=c;blag=lag;}
            }
            float norm=best/(e0+1e-9f);
            if(norm>0.5f){tonalE+=norm;tc++;pitches.push_back(sr/(float)blag);}
        }
        float tonalFrac=nf>0?(float)tc/(float)(nf/4):0.f;
        if(ons.size()>=4&&!fBpmFromHost){
            float bestB=0,bs=0;
            for(float bb=70;bb<180;bb+=0.5f){
                float sc=0,beat=60000.f/bb;
                for(size_t i=1;i<ons.size();i++){
                    float d=(ons[i]-ons[i-1])*HOP*1000.f/sr;
                    float r=fmodf(d+beat*0.5f,beat)-beat*0.5f;
                    if(fabsf(r)<beat*0.15f)sc+=1.f-fabsf(r)/(beat*0.15f);
                }
                if(sc>bs){bs=sc;bestB=bb;}
            }
            if(bs>1.f)fBpm=bestB;
        }
        fStep16=sr*60.f/fBpm/4.f;
        fKey=-1;
        if(tonalFrac>0.25f&&!pitches.empty()){
            int hist[12]={0};
            for(float p:pitches){int pc=(int)(69.5f+12.f*log2f(p/440.f))%12;if(pc<0)pc+=12;hist[pc]++;}
            int bs=-1,bk=-1;
            for(int root=0;root<12;root++)for(int mn=0;mn<2;mn++){
                const int*sc=mn?kMin:kMaj;int s=0;
                for(int g=0;g<7;g++)s+=hist[(root+sc[g])%12];
                if(s>bs){bs=s;bk=root+mn*12;}
            }
            fKey=(float)bk;
            float tot=0;for(int i=0;i<12;i++)tot+=hist[i];
            fConf=tot>0.5f?std::min(100.f,(float)bs/tot*100.f):0.f;
        }
        if(ons.size()<=2&&tonalFrac<0.25f)fKlas=3;
        else if(tonalFrac>0.4f&&ons.size()<8)fKlas=2;
        else if(tonalFrac>0.4f)fKlas=4;
        else fKlas=1;
        fLastOns=ons;fLastCls=onsCls;fLastPit=pitches;
        generateLoop(ons,onsCls,pitches,false);
        fStat=3;
    }

    void generateLoop(const std::vector<int>&ons,const std::vector<int>&cls,
                  const std::vector<float>&pitches,bool refine){
        pushUndo_();fNotes.clear();
        const int barsL[4]={2,4,8,16};int bars=barsL[std::max(0,std::min(3,(int)(fLen+0.5f)))];
        const int steps=bars*16;
        const int gi=std::max(0,std::min(kNumProgs-1,(int)(fPreset+0.5f)));
        const float dens=fHeft/100.f,var=fVar/100.f,mens=fMens/100.f;
        uint32_t rng=fRnd;
        if(refine)rng=rng*1664525u+1013904223u;
        auto rnd=[&](){rng=rng*1664525u+1013904223u;return (rng>>9&0xFFFF)/65535.f;};
        auto addN=[&](int step,int note,int vel,int dur16,int ch){
            if(step<0||step>=steps)return;
            N n{};n.at=(uint32_t)(step*fStep16+mens*(rnd()-0.5f)*fStep16*0.25f);
            n.dur=(uint32_t)(dur16*fStep16*0.9f);n.note=(uint8_t)note;
            n.vel=(uint8_t)std::max(1,std::min(127,vel+(int)(mens*(rnd()-0.5f)*30)));n.ch=(uint8_t)ch;
            fNotes.push_back(n);
        };
        bool wantDrums=(fKlas==1||fKlas==3||fKlas==4),wantMel=(fKlas==2||fKlas==3||fKlas==4);
        if(wantDrums){
            if(fKlas!=3){
                for(size_t i=0;i<ons.size();i++){
                    int st=(int)(ons[i]*512.f/fStep16+0.5f);
                    int c=cls[i];addN(st,c==0?36:(c==1?38:42),c==0?110:(c==1?105:85),1,0);
                }
            }
            for(int s=0;s<steps;s++){
                int m=s%16;float need=dens*0.9f;
                if((kGHats[gi]>>m)&1){if(rnd()<need+0.3f)addN(s,42,60+(int)(rnd()*30),1,0);}
                if((kGKick[gi]>>m)&1){if(rnd()<(fKlas==3?0.9f:need*0.4f))addN(s,36,110,1,0);}
                if((kGSnar[gi]>>m)&1){if(rnd()<0.95f)addN(s,38,105,1,0);}
                if(rnd()<var*0.08f)addN(s,rnd()<0.5f?45:46,80,1,0);
            }
        }
        if(wantMel){
            int root=fKey>=0?60+((int)fKey)%12:60;
            bool minor=fKey>=12;
            const int*sc=minor?kMin:kMaj;
            if(fKlas==3||pitches.empty()){
                int mot[8]={0,7,12,7,0,10,12,0};
                for(int s=0;s<steps;s+=2)
                    if(rnd()<0.5f+dens*0.4f)addN(s,root+mot[(s/2)%8],90,2,1);
            }else{
                for(size_t i=0;i<pitches.size()&&i<512;i++){
                    int midi=(int)(69.5f+12.f*log2f(pitches[i]/440.f));
                    int st=(int)(i*4*512.f/fStep16+0.5f);
                    addN(st,midi,95,2,1);
                }
            }
            for(int b=0;b<bars;b++){
                addN(b*16,root-12,100,4,2);
                if(rnd()<dens)addN(b*16+8,root-12+sc[(b%3)*2]%12,95,4,2);
            }
        }
        fLoopLen=(uint32_t)(steps*fStep16);fLoopPos=0;fNIdx=0;
        std::sort(fNotes.begin(),fNotes.end(),[](const N&a,const N&b){return a.at<b.at;});
        fRnd=rng;
    }

    // ============ MODUS 1: MELODIE (OPROEP pitch-track) ============
    void analyzeMelodie(){
        fStat=2;
        const float sr=getSampleRate();int n=(int)fRecN;
        if(n<4800){fStat=0;fKlas=0;return;}
        const int HOP=256,FR=1024;int nf=(n-FR)/HOP;
        if(nf<8){fStat=0;fKlas=0;return;}
        std::vector<float> hz(nf,0);
        int hist[12]={0};int voiced=0;
        for(int f=0;f<nf;f++){
            float h=pitchOf(fRec+f*HOP,FR,sr);hz[f]=h;
            if(h>0){voiced++;int pc=(int)(69.5f+12.f*log2f(h/440.f))%12;if(pc<0)pc+=12;hist[pc]++;}
        }
        if(voiced*100<nf*15){fStat=5;fKlas=0;fNotes.clear();return;}
        fKey=-1;{
            int bs=-1,bk=-1;
            for(int root=0;root<12;root++)for(int mn=0;mn<2;mn++){
                const int*sc=mn?kMin:kMaj;int s=0;
                for(int g=0;g<7;g++)s+=hist[(root+sc[g])%12];
                if(s>bs){bs=s;bk=root+mn*12;}
            }
            fKey=(float)bk;
            float tot=0;for(int i=0;i<12;i++)tot+=hist[i];
            fConf=tot>0.5f?std::min(100.f,(float)bs/tot*100.f):0.f;
        }
        pushUndo_();fNotes.clear();
        int cur=-1,st=0,lastV=0;
        for(int f=0;f<=nf;f++){
            int m=(f<nf&&hz[f]>0)?(int)(69.5f+12.f*log2f(hz[f]/440.f)):-1;
            if(m>=0&&cur<0){cur=m;st=f;}
            else if(m<0||cur<0||std::abs(m-cur)>1||f-lastV>3){
                if(cur>=0){
                    int du=(lastV+1-st)*HOP;
                    if(du>=(int)(sr*0.06f)){
                        N nn{};nn.at=(uint32_t)(st*HOP);nn.dur=(uint32_t)du;
                        nn.note=(uint8_t)std::max(24,std::min(96,cur));nn.vel=100;nn.ch=1;
                        fNotes.push_back(nn);
                    }
                    cur=-1;
                }
                if(m>=0){cur=m;st=f;}
            }
            if(m>=0)lastV=f;
        }
        int scale=(int)(fSchaal+0.5f);
        int root=fKey>=0?(int)fKey%12:0;
        const int*sc=kScales[scale];int sl=kScaleLen[scale];
        float q=fHeft/100.f;
        float stepS[3]={8,16,32};float g=stepS[std::max(0,std::min(2,(int)(fLen+0.5f)))];
        double gsmp=sr*60.f/fBpm*4.0/g;
        for(N&nn:fNotes){
            if(scale!=4){
                int pc=nn.note%12;int b=0,bd=99;
                for(int g2=0;g2<sl;g2++){int d=(pc-root-sc[g2]%12+18)%12-6;
                    if(std::abs(d)<bd){bd=std::abs(d);b=d;}}
                nn.note=(uint8_t)std::max(24,std::min(96,nn.note-b));
            }
            nn.note=(uint8_t)std::max(12,std::min(108,nn.note+(int)(fOct+0.5f)*12));
            uint32_t qa=(uint32_t)(nn.at/gsmp+0.5f)*(uint32_t)gsmp;
            nn.at=(uint32_t)(nn.at+q*((double)qa-nn.at));
            uint32_t qd=std::max((uint32_t)(gsmp*0.5f),(uint32_t)(nn.dur/gsmp+0.5f)*(uint32_t)gsmp);
            nn.dur=(uint32_t)(nn.dur+q*((double)qd-nn.dur)*0.9f);
        }
        fHits=(float)fNotes.size();
        if(fNotes.empty()){fStat=0;fKlas=0;return;}
        uint32_t end=0;for(const N&x:fNotes)end=std::max(end,x.at+x.dur);
        double bar=sr*60.f/fBpm*4.f;
        fLoopLen=(uint32_t)(std::max(1,(int)(end/bar+0.99f))*bar);
        std::sort(fNotes.begin(),fNotes.end(),[](const N&a,const N&b){return a.at<b.at;});
        if(!fBpmFromHost&&fNotes.size()>=4){
            double d[512];int dn=0;
            for(size_t i=1;i<fNotes.size()&&dn<512;i++)
                if(fNotes[i].at>fNotes[i-1].at)d[dn++]=fNotes[i].at-fNotes[i-1].at;
            double med=dn?d[dn/2]:sr*0.5f;
            if(med>0)fBpm=(float)std::max(60.,std::min(200.,sr*15.0/med));
        }
        fKlas=2;fLoopPos=0;fNIdx=0;fStat=3;
    }

    // ============ MODUS 2: INSTRUMENT ============
    void analyzeInstrument(){
        fStat=2;
        const float sr=getSampleRate();int n=(int)fRecN;
        if(n<4800){fStat=0;fKlas=0;return;}
        int win=(int)(sr*2);if(win>n)win=n;
        double be=0;int bs=0;
        for(int st=0;st<=n-win;st+=2048){
            double e=0;for(int i=st;i<st+win;i+=4)e+=fRec[i]*fRec[i];
            if(e>be){be=e;bs=st;}
        }
        float hz=pitchOf(fRec+bs,std::min(4096,win),sr);
        fSmpHz=hz>40?hz:220;
        fSmpN=std::min(win,(int)sizeof(fSmp)/4);
        std::memcpy(fSmp,fRec+bs,fSmpN*sizeof(float));
        int f2=fSmpN/40;
        for(int i=0;i<f2;i++){float r=(float)i/f2;fSmp[i]*=r;fSmp[fSmpN-1-i]*=r;}
        double dc=0;for(int i=0;i<fSmpN;i++)dc+=fSmp[i];dc/=fSmpN;
        for(int i=0;i<fSmpN;i++)fSmp[i]-=(float)dc*0.9f;
        fNotes.clear();fHits=1;fKlas=3;fStat=3;fLoopLen=0;fLoopPos=0;fNIdx=0;
    }

    // ============ MODUS 3: ANTWOORD (call & response) ============
    void startLuister(){fFase=1;fCallN=0;fCallStart=fAbs;fFaseLeft=(int64_t)(64.0*fStep16);}
    void stelVraag(){fFase=3;fCallN=0;fCallStart=fAbs;fRespStart=fAbs;fRespI=0;
        fFaseLeft=(int64_t)(64.0*fStep16);genVraag();}
    void endLuister(){detectKey();genAntwoord();fFase=2;fRespI=0;fRespStart=fAbs;
        fFaseLeft=(int64_t)(64.0*fStep16);fLuist=0;}

    void noteIn(int note,int vel){
        if(fFase!=1)return;
        if(fCallN<256){CallN&c=call_[fCallN++];c.note=(uint8_t)note;c.vel=(uint8_t)vel;
            c.pos16=(float)((fAbs-fCallStart)/(double)fStep16);c.dur16=2;}
    }
    void detectKey(){
        float hist[12]={0};int n=0;
        for(int i=0;i<fCallN;i++){hist[call_[i].note%12]+=call_[i].vel/127.f;n++;}
        int low=0;for(int i=0;i<fCallN;i++)if(call_[i].note<50)low++;
        fDrum=(n>0&&low*2>n);
        if(n==0){fAKey=9;fAMinor=0;fConf=0;return;}
        float best=-1.f;int bk=0;bool bm=false;
        for(int k=0;k<12;k++){
            float sM=0,sN=0;
            for(int d=0;d<7;d++){sM+=hist[(k+kMaj[d])%12];sN+=hist[(k+kMin[d])%12];}
            if(sM>best){best=sM;bk=k;bm=false;}
            if(sN>best){best=sN;bk=k;bm=true;}
        }
        fAKey=bk;fAMinor=bm?1:0;
        float tot=0;for(int i=0;i<12;i++)tot+=hist[i];
        fConf=tot>0.5f?best/tot*100.f:0.f;if(fConf>100.f)fConf=100.f;
    }
    int scaleDeg(int deg)const{
        const int*sc=fAMinor?kMin:kMaj;int oct=deg/7,d=deg%7;if(d<0){d+=7;oct--;}
        return sc[d]+12*oct;
    }
    int noteDeg(int midi)const{
        int rel=((midi-fAKey)%12+12)%12;const int*sc=fAMinor?kMin:kMaj;
        int bd=99,bg=0;for(int d=0;d<7;d++){int dd=std::abs(rel-sc[d]);if(dd<bd){bd=dd;bg=d;}}
        return bg+(midi-60)/12*7;
    }
    uint32_t frand(){fSeed=fSeed*1664525u+1013904223u;return fSeed>>9;}

    void* fBus=nullptr;
    void pubBus_(){
        G2BBus*b=(G2BBus*)fBus;if(!b)return;b->magic=kBusMagic;
        b->midiMagic=kMidiMagic;b->midiSeq++;
        b->midiGenre=(uint32_t)std::max(0,(int)(fPreset+0.5f));
        b->midiKeyMin=(uint32_t)std::max(0,fAKey+(fAMinor?12:0));
        int n=fRespN<256?fRespN:256;b->midiCount=(uint32_t)n;
        for(int i=0;i<n;i++){b->midiAt16[i]=(uint16_t)(resp_[i].at/fStep16);
            b->midiNote[i]=resp_[i].note;b->midiVel[i]=resp_[i].vel;}
    }
    void genAntwoord(){
        pushUndo_();fRespN=0;
        const GStijl&st=kStijl[std::max(0,std::min(kNumProgs-1,(int)(fPreset+0.5f)))];
        float vSum=0,vN=0,swSum=0,swN=0;
        for(int i=0;i<fCallN;i++){vSum+=call_[i].vel;vN++;
            float frac=call_[i].pos16-floorf(call_[i].pos16+0.001f);
            if((int)floorf(call_[i].pos16)%2==1&&frac<0.5f){swSum+=frac;swN++;}}
        float callVel=vN>0?vSum/vN:80.f;
        float callSwing=swN>0?swSum/swN:0.f;
        float geh=fVar; // in ANTWOORD-modus is VAR = GEHEUGEN
        const int genre=(int)(fPreset+0.5f); // 5=DEMBOW 6=KAWINA
        int mode=fHeft<33.f?0:(fHeft<66.f?1:2);
        const float morph=fHeft<33.f?0.f:(fHeft>66.f?1.f:(fHeft-33.f)/33.f); // 33-66 vloeiend
        const int steps16=64;
        int grid[64];for(int i=0;i<64;i++)grid[i]=-1;
        bool ghost[64]={},dembAcc[64]={};
        if(mode==0&&fCallN>0){
            for(int i=0;i<fCallN;i++){int s=(int)(call_[i].pos16+0.5f)%steps16;
                grid[s]=call_[i].note;}
        }else{
            bool callOcc[64]={};
            for(int i=0;i<fCallN;i++){int s=(int)(call_[i].pos16+0.5f)%steps16;callOcc[s]=true;}
            // morph-echo: tussen 33-66 een deel van je eigen noten meenemen (vervaagt naarmate HEFT hoger)
            if(mode==1&&fCallN>0){
                for(int i=0;i<fCallN;i++){int s=(int)(call_[i].pos16+0.5f)%steps16;
                    if(((frand()>>5)&0xFFFF)/65535.f<(1.f-morph)*0.8f&&grid[s]<0)grid[s]=call_[i].note;}
            }
            uint32_t r=fSeed*2654435761u+97u;int prevDeg=0;
            for(int s=0;s<steps16;s++){
                float p=st.dens;
                if(mode==2){p*=0.35f;if(!callOcc[s])p+=0.45f;}
                if(mode==1)p*=0.4f+morph*0.6f; // gatenvuller 'druipt' in met de knop
                if(s%16==0)p+=0.3f;
                if(st.sync>0.5f&&(s%8==6||s%8==7))p+=0.25f;
                if(genre==5){ // DEMBOW grid-lock: stap 3+11 altijd, ruimte erna
                    int s16=s%16;
                    if(s16==3||s16==11){p=1.f;dembAcc[s]=true;}
                    else if(s16==4||s16==5||s16==12||s16==13)p*=0.12f;
                }
                float rnd=(r=(r*1664525u+1013904223u),((r>>9)&0xFFFF)/65535.f);
                if(rnd<p){
                    int deg;
                    if(geh>30.f&&mode==1&&fCallN>0&&(rnd*7)<4.f)
                        deg=noteDeg(call_[(int)(rnd*fCallN)%fCallN].note)+((r>>3)%3)-1;
                    else
                        deg=prevDeg+((int)((r>>5)%5))-2;
                    grid[s]=60+fAKey+scaleDeg(deg);prevDeg=deg;
                    if(fCallN>0){int last=call_[fCallN-1].note;
                        while(grid[s]>last+4)grid[s]-=7;
                        while(grid[s]<last-4)grid[s]+=7;}
                    if(mode==2&&!callOcc[s])ghost[s]=true;
                    if(st.oct>0.4f&&((r>>9)%5)==0)grid[s]+=12;
                }
            }
        }
        float velJ=fMens/100.f*14.f,tJ=fMens/100.f*0.12f;
        for(int s=0;s<steps16;s++)if(grid[s]>=0){
            RespN&rn=resp_[fRespN];if(fRespN>=256)break;
            float spos=s+callSwing*(s%2==1?1.f:0.f)+(((frand()&0xFFFF)/65535.f)-0.5f)*tJ;
            rn.at=(uint32_t)std::max<int>(0,(int)(spos*fStep16));
            rn.dur=(uint32_t)(st.dur*fStep16*0.9f);
            int vel=(int)(callVel+((frand()&0xFFFF)/65535.f-0.5f)*velJ);
            // humanize-curve: opbouw naar het midden van de frase, daalt aan het eind
            vel=(int)(vel*(0.8f+0.35f*sinf((float)M_PI*(float)s/steps16)));
            if(dembAcc[s])vel=105+(int)(vel*0.15f); // dembow snare-accent
            if(genre==6){int s16=s%16; // KAWINA tresillo-puls
                if(s16==0||s16==3||s16==6||s16==8||s16==11||s16==14)vel=(int)(vel*1.25f);}
            if(ghost[s])vel=20+(int)(vel*0.15f); // spooknoot: zacht tussen de gaten
            rn.vel=(uint8_t)(vel<20?20:(vel>127?127:vel));
            rn.note=(uint8_t)grid[s];fRespN++;
        }
        for(int i=1;i<fRespN;i++){RespN t=resp_[i];int j=i-1;
            while(j>=0&&resp_[j].at>t.at){resp_[j+1]=resp_[j];j--;}resp_[j+1]=t;}
        pubBus_();
    }
    void genVraag(){
        fRespN=0;const GStijl&st=kStijl[std::max(0,std::min(kNumProgs-1,(int)(fPreset+0.5f)))];
        uint32_t r=99991u;
        for(int s=0;s<64;s++){
            float p=s%16==0?0.9f:st.dens*0.7f;
            if(((r=(r*1664525u+1013904223u),((r>>9)&0xFFFF))/65535.f)<p){
                RespN&rn=resp_[fRespN];if(fRespN>=256)break;
                rn.at=(uint32_t)(s*fStep16);rn.dur=(uint32_t)(st.dur*fStep16*0.9f);
                rn.vel=(uint8_t)(70+(r>>13)%40);rn.note=(uint8_t)(60+fAKey+scaleDeg((r>>7)%7));
                fRespN++;
            }
        }
        for(int i=1;i<fRespN;i++){RespN t=resp_[i];int j=i-1;
            while(j>=0&&resp_[j].at>t.at){resp_[j+1]=resp_[j];j--;}resp_[j+1]=t;}
    }

    void pluck(int note,int vel,float dur=0.35f,int type=0){
        for(auto&v:fAV)if(!v.on){v.on=true;v.freq=440.f*powf(2.f,(note-69)/12.f);v.ph=0;
            v.amp=vel/127.f*0.8f;v.dec=expf(-1.f/(dur*getSampleRate()));v.lp=0;v.type=type;return;}
        fAV[0].freq=440.f*powf(2.f,(note-69)/12.f);fAV[0].ph=0;fAV[0].amp=vel/127.f*0.8f;fAV[0].on=true;fAV[0].type=type;
    }
    void drumHit(int kind,int vel){
        for(auto&v:fAV)if(!v.on){v.on=true;v.type=kind+10;v.amp=vel/127.f;
            v.dec=expf(-1.f/((kind==0?0.18f:kind==1?0.12f:0.05f)*getSampleRate()));
            v.freq=kind==0?150.f:220.f;v.ph=0;return;}
    }
    void emitMidi(uint8_t st,uint8_t nt,uint8_t vel,uint32_t fr){
        MidiEvent e{};e.frame=fr;e.size=4;e.data[0]=st;e.data[1]=nt;e.data[2]=vel;e.data[3]=0;
        writeMidiEvent(e);}
    void respNote(int note,int vel,uint32_t i,uint32_t dur){
        if(fDrum)drumHit(note<45?0:(note%3==0?1:2),vel);
        else pluck(note,vel,dur/44100.f<0.12?0.12f:dur/44100.f);
        fNoteNow=note;
        MidiRec&r=rec_[recW_];r.fr=fAbs+i;r.note=(uint8_t)note;r.ch=0;r.vel=(uint8_t)vel;
        r.dur=(uint16_t)std::min<uint32_t>(60000,dur);recW_=(recW_+1)%4096;recN_++;
        emitMidi(0x90,(uint8_t)note,(uint8_t)vel,i);
        PendingOff&o=offq_[offw_];o.note=(uint8_t)note;o.at=i+dur;offw_=(offw_+1)%64;
    }

    // ============ .mid laden (IN.mid → call_ of fNotes) ============
    int loadIn_(){
        const char*home=std::getenv("USERPROFILE");if(!home||!*home)home=std::getenv("HOME");
        char path[520];std::snprintf(path,sizeof(path),"%s/Music/G2B/IN.mid",home?home:".");
        smf::MidiFile mf;
        if(!mf.read(path))return -1;
        mf.joinTracks();mf.linkNotePairs();
        pushUndo_();
        if(fMode>2.5f){ // ANTWOORD: geladen clip = de vraag
            fCallN=0;
            for(int i=0;i<mf[0].size()&&fCallN<256;i++){smf::MidiEvent&e=mf[0][i];
                if(!e.isNoteOn())continue;
                CallN&c=call_[fCallN++];c.note=(uint8_t)e[1];c.vel=(uint8_t)e[2];
                c.pos16=e.tick/120.f;c.dur16=2;}
            if(fCallN<=0)return 0;
            detectKey();genAntwoord();fFase=2;fRespI=0;fRespStart=fAbs;
            fFaseLeft=(int64_t)(64.0*fStep16);
            fHits=(float)fCallN;fStat=3;
            return fCallN;
        }
        fNotes.clear();
        const double fpt=fStep16>0?fStep16/120.0:1.0;
        int cnt=0;
        for(int i=0;i<mf[0].size();i++){smf::MidiEvent&e=mf[0][i];
            if(!e.isNoteOn()||!e.isLinked())continue;
            smf::MidiEvent*off=e.getLinkedEvent();
            N n{};n.at=(uint32_t)(e.tick*fpt);
            n.dur=(uint32_t)(std::max(1,off->tick-e.tick)*fpt);
            n.note=(uint8_t)e[1];n.vel=(uint8_t)e[2];
            n.ch=(e.getChannel()==9)?0:1;fNotes.push_back(n);cnt++;}
        if(cnt<=0)return 0;
        std::sort(fNotes.begin(),fNotes.end(),[](const N&a,const N&b){return a.at<b.at;});
        uint32_t end=0;for(const N&x:fNotes)end=std::max(end,x.at+x.dur);
        fLoopLen=end;fLoopPos=0;fNIdx=0;fHits=(float)cnt;fKlas=4;fStat=3;
        return cnt;
    }

    // ============ undo ============
    void pushUndo_(){Snap s;s.notes=fNotes;std::memcpy(s.resp,resp_,sizeof(resp_));s.respN=fRespN;
        undoStk_.push_back(s);if(undoStk_.size()>10)undoStk_.erase(undoStk_.begin());}
    void undo_(){if(undoStk_.empty())return;const Snap&s=undoStk_.back();
        fNotes=s.notes;std::memcpy(resp_,s.resp,sizeof(resp_));fRespN=s.respN;
        undoStk_.pop_back();fLoopPos=0;fNIdx=0;fRespI=0;fStat=3;}

    // ============ wav-bounce ============
    static const char*stamp_(){static char s[8];std::time_t t=std::time(nullptr);
        std::tm*tm=std::localtime(&t);
        std::snprintf(s,sizeof(s),"%02d%02d%02d",tm->tm_hour,tm->tm_min,tm->tm_sec);return s;}

    void writeWav_(const char*path,const std::vector<N>&notes){
        if(notes.empty())return;
        const double sr=getSampleRate();
        uint32_t end=0;for(const N&n:notes)end=std::max(end,n.at+n.dur);
        if(end==0)return;
        const uint32_t len=end+(uint32_t)(sr*1.5);
        std::vector<float>L(len,0.f),R(len,0.f);
        for(const N&n:notes){
            const double f=440.0*pow(2.0,(n.note-69)/12.0);
            const uint32_t t0=n.at,t1=std::min(len,n.at+n.dur+(uint32_t)(sr*1.2));
            const float amp=n.vel/127.f*0.5f;
            for(uint32_t i=t0;i<t1;i++){
                const double t=(i-t0)/sr;float s;
                if(n.ch==0){
                    int kind;
                    if(n.note==36||n.note==35)kind=0;
                    else if(n.note==38||n.note==40||n.note==45||n.note==46)kind=1;
                    else if(n.note>=41&&n.note<=60)kind=2;
                    else kind=n.note<45?0:(n.note%3==0?1:2);
                    if(kind==0){const double fo=120*exp(-t*25)+45;s=(float)sin(2*M_PI*fo*t)*(float)exp(-t*16);}
                    else if(kind==1){const float nz=((std::rand()&0xFFFF)/32768.f-1.f);s=(nz*0.6f+(float)sin(2*M_PI*220*t)*0.4f)*(float)exp(-t*22);}
                    else{const float nz=((std::rand()&0xFFFF)/32768.f-1.f);s=nz*(float)exp(-t*70);}
                }else{
                    const float s0=sinf((float)(2*M_PI*f*t))*0.7f+sinf((float)(2*M_PI*2*f*t))*0.2f;
                    s=s0*(float)exp(-t*3.0);
                }
                const float a=amp*std::min(1.f,(float)(t*500));
                L[i]+=s*a*0.9f;R[i]+=s*a;
            }
        }
        float pk=0;for(uint32_t i=0;i<len;i++){const float a=std::max(fabsf(L[i]),fabsf(R[i]));if(a>pk)pk=a;}
        if(pk>1e-6f){const float g=0.89f/pk;for(uint32_t i=0;i<len;i++){L[i]*=g;R[i]*=g;}}
        FILE*f=std::fopen(path,"wb");if(!f)return;
        const uint32_t dl=len*4;
        std::fwrite("RIFF",1,4,f);uint32_t v=36+dl;std::fwrite(&v,4,1,f);
        std::fwrite("WAVEfmt ",1,8,f);v=16;std::fwrite(&v,4,1,f);
        uint16_t w=1;std::fwrite(&w,2,1,f);w=2;std::fwrite(&w,2,1,f);
        v=(uint32_t)sr;std::fwrite(&v,4,1,f);v=(uint32_t)sr*4;std::fwrite(&v,4,1,f);
        w=4;std::fwrite(&w,2,1,f);w=16;std::fwrite(&w,2,1,f);
        std::fwrite("data",1,4,f);std::fwrite(&dl,4,1,f);
        for(uint32_t i=0;i<len;i++){const int16_t b[2]={(int16_t)(L[i]*32767),(int16_t)(R[i]*32767)};std::fwrite(b,4,1,f);}
        std::fclose(f);
    }

    // ============ .mid exports ============
    void writeMidAll(){
        const char*home=std::getenv("USERPROFILE");if(!home||!*home)home=std::getenv("HOME");
        char dir[520];
        std::snprintf(dir,sizeof(dir),"%s/Music/G2B",home?home:".");
        #ifdef _WIN32
        {std::string m=std::string(home?home:".")+"/Music";mkdir(m.c_str());mkdir(dir);}
        #else
        {std::string m=std::string(home?home:".")+"/Music";mkdir(m.c_str(),0755);mkdir(dir,0755);}
        #endif
        const double fpt=fStep16>0?fStep16/120.0:1.0;
        const char*ts=stamp_();
        if(fMode<0.5f){ // LOOP: 2 .mid's + 2 .wav's
            if(fNotes.empty())return;
            const char*suffix[2]={"DRUMS","MELODIE"};
            for(int g=0;g<2;g++){
                smf::MidiFile mf;mf.setTicksPerQuarterNote(480);int seq=0;bool any=false;
                for(const N&n:fNotes){
                    bool drum=n.ch==0;
                    if((g==0)!=drum)continue;
                    any=true;
                    smf::MidiEvent on(0x90,n.note,n.vel);{int tk=(int)(n.at/fpt);on.tick=tk+(std::rand()%11-5)+(int)(3.f*sinf(6.2832f*tk/7680.f));}on.seq=seq++;
                    smf::MidiEvent off(0x80,n.note,0);off.tick=(int)((n.at+n.dur)/fpt);off.seq=seq++;
                    mf[0].push_back(on);mf[0].push_back(off);
                }
                if(!any)continue;
                mf.sortTracks();
                char path[640];std::snprintf(path,sizeof(path),"%s/G2B-LOOP-%s_%s.mid",dir,suffix[g],ts);
                mf.write(path);
                {std::vector<N>sel;for(const N&n:fNotes)if((g==0)==(n.ch==0))sel.push_back(n);
                 std::snprintf(path,sizeof(path),"%s/G2B-LOOP-%s_%s.wav",dir,suffix[g],ts);
                 writeWav_(path,sel);}
            }
        }else if(fMode<1.5f){ // MELODIE: 1 .mid
            if(fNotes.empty())return;
            smf::MidiFile mf;mf.setTicksPerQuarterNote(480);int seq=0;
            for(const N&n:fNotes){
                smf::MidiEvent on(0x90,n.note,n.vel);{int tk=(int)(n.at/fpt);on.tick=tk+(std::rand()%11-5)+(int)(3.f*sinf(6.2832f*tk/7680.f));}on.seq=seq++;
                smf::MidiEvent off(0x80,n.note,0);off.tick=(int)((n.at+n.dur)/fpt);off.seq=seq++;
                mf[0].push_back(on);mf[0].push_back(off);
            }
            mf.sortTracks();
            char path[640];std::snprintf(path,sizeof(path),"%s/G2B-MELODIE_%s.mid",dir,ts);
            mf.write(path);
            std::snprintf(path,sizeof(path),"%s/G2B-MELODIE_%s.wav",dir,ts);
            writeWav_(path,fNotes);
        }else if(fMode>2.5f){ // ANTWOORD: response-mid (rec_ buffer, of resp_ direct bij LAAD)
            if(recN_==0&&fRespN>0){
                const double sr2=getSampleRate(),fptR=sr2*60.0/(fBpm*480.0);
                smf::MidiFile mf2;mf2.setTicksPerQuarterNote(480);int seq2=0;
                for(int i=0;i<fRespN;i++){const RespN&r=resp_[i];
                    smf::MidiEvent on(0x90,r.note,r.vel);{int tk=(int)(r.at/fptR);on.tick=tk+(std::rand()%15-7)+(int)(4.f*sinf(6.2832f*tk/7680.f));}on.seq=seq2++;
                    smf::MidiEvent off(0x80,r.note,0);off.tick=(int)((r.at+r.dur)/fptR);off.seq=seq2++;
                    mf2[0].push_back(on);mf2[0].push_back(off);}
                mf2.sortTracks();
                char path[640];std::snprintf(path,sizeof(path),"%s/G2B-ANTWOORD_%s.mid",dir,ts);
                mf2.write(path);
                {std::vector<N>rn;for(int i=0;i<fRespN;i++){const RespN&r=resp_[i];
                     N nn{};nn.at=r.at;nn.dur=r.dur;nn.note=r.note;nn.vel=r.vel;nn.ch=fDrum?0:1;rn.push_back(nn);}
                 std::snprintf(path,sizeof(path),"%s/G2B-ANTWOORD_%s.wav",dir,ts);
                 writeWav_(path,rn);}
                fStat=4;
                #ifdef _WIN32
                char cmd2[700];std::snprintf(cmd2,sizeof(cmd2),"explorer \"%s\"",dir);std::system(cmd2);
                #else
                char cmd2[700];std::snprintf(cmd2,sizeof(cmd2),"xdg-open \"%s\" >/dev/null 2>&1 &",dir);std::system(cmd2);
                #endif
                return;
            }
            if(recN_==0)return;
            const double sr=getSampleRate(),fptA=sr*60.0/(fBpm*480.0);
            smf::MidiFile mf;mf.setTicksPerQuarterNote(480);
            const uint64_t n=recN_<4096?recN_:4096;const int start=(recN_<=4096)?0:recW_;int seq=0;
            for(uint64_t k=0;k<n;k++){const MidiRec&r=rec_[(start+(int)k)%4096];
                smf::MidiEvent on(0x90|r.ch,r.note,r.vel);{int tk=(int)(r.fr/fptA);on.tick=tk+(std::rand()%15-7)+(int)(4.f*sinf(6.2832f*tk/7680.f));}on.seq=seq++;
                smf::MidiEvent off(0x80|r.ch,r.note,0);off.tick=(int)((r.fr+r.dur)/fptA);off.seq=seq++;
                mf[0].push_back(on);mf[0].push_back(off);}
            mf.sortTracks();
            char path[640];std::snprintf(path,sizeof(path),"%s/G2B-ANTWOORD_%s.mid",dir,ts);
            mf.write(path);
            {std::vector<N>rn;const int st2=(recN_<=4096)?0:recW_;uint64_t fr0=0;
             for(uint64_t k=0;k<n;k++){const MidiRec&r=rec_[(st2+(int)k)%4096];
                 if(k==0)fr0=r.fr;N nn{};nn.at=(uint32_t)(r.fr-fr0);nn.dur=r.dur;
                 nn.note=r.note;nn.vel=r.vel;nn.ch=fDrum?0:1;rn.push_back(nn);}
             std::snprintf(path,sizeof(path),"%s/G2B-ANTWOORD_%s.wav",dir,ts);
             writeWav_(path,rn);}
        }
        fStat=4;
        #ifdef _WIN32
        char cmd[700];std::snprintf(cmd,sizeof(cmd),"explorer \"%s\"",dir);std::system(cmd);
        #else
        char cmd[700];std::snprintf(cmd,sizeof(cmd),"xdg-open \"%s\" >/dev/null 2>&1 &",dir);std::system(cmd);
        #endif
    }

    // ============ run ============
    void run(const float** in, float** out, uint32_t frames, const MidiEvent* evs, uint32_t nev) override {
        const float sr=getSampleRate();
        if(fStep16<=0){fBpm=120.0;fStep16=sr*60.0/fBpm/4.0;}
        const TimePosition&tp=getTimePosition();
        bool playing=true;
        if(tp.bbt.valid){
            if(tp.bbt.beatsPerMinute>1.0){fBpm=tp.bbt.beatsPerMinute;fBpmFromHost=1;fStep16=sr*60.f/fBpm/4.f;}
            playing=tp.playing;
        }
        if(in[0]!=out[0])std::memcpy(out[0],in[0],frames*sizeof(float));
        if(in[1]!=out[1])std::memcpy(out[1],in[1],frames*sizeof(float));

        // midi-in: INSTRUMENT speelt stemmen, ANTWOORD vangt call-noten
        bool instr=fMode>1.5f&&fMode<2.5f&&fSmpN>256;
        bool antw=fMode>2.5f;
        for(uint32_t e=0;e<nev;e++){
            const MidiEvent&ev=evs[e];
            if(ev.size<3)continue;
            uint8_t st=ev.data[0]&0xF0,nt=ev.data[1],vl=ev.data[2];
            if(instr&&(st==0x90&&vl>0)){
                int slot=-1;for(int k=0;k<16;k++)if(!fIV[k].act){slot=k;break;}
                if(slot<0)slot=0;
                InstV&v=fIV[slot];v.note=nt;v.pos=0;v.env=0;
                float nh=440.f*powf(2.f,(nt-69)/12.f);
                v.rate=nh/fSmpHz*powf(2.f,floorf(fOct+0.5f));v.amp=vl/127.f;v.act=1;
            }else if(instr&&(st==0x80||(st==0x90&&vl==0))){
                for(int k=0;k<16;k++)if(fIV[k].act&&fIV[k].note==nt)fIV[k].act=2;
            }else if(antw&&st==0x90&&vl>0){
                noteIn(nt,vl);
            }
        }
        // instrument-stemmen
        if(instr){
            for(int k=0;k<16;k++){
                InstV&v=fIV[k];if(!v.act)continue;
                float atk=v.env<1.f?0.005f:0.f;
                for(uint32_t i=0;i<frames;i++){
                    if(v.env<1.f)v.env+=atk;
                    if(v.act==2)v.env*=0.9992f;
                    int p=(int)v.pos;float fr=v.pos-p;
                    int p2=(p+1)%fSmpN;
                    float s=(fSmp[p]*(1-fr)+fSmp[p2]*fr)*v.env*v.amp;
                    out[0][i]+=s;out[1][i]+=s;
                    v.pos+=v.rate;
                    if(v.pos>=fSmpN-64){v.pos-=fSmpN-128;if(v.pos<0)v.pos=0;}
                }
                if(v.act==2&&v.env<0.001f)v.act=0;
            }
        }
        // audio-opname (modes 0-2)
        if(fLuist>0.5f&&fStat==1&&fMode<2.5f){
            for(uint32_t i=0;i<frames&&fRecN<kRecMax;i++)
                fRec[fRecN++]=(in[0][i]+in[1][i])*0.5f;
            if(fRecN>=kRecMax){fLuist=0;fWantAna=1;}
        }
        if(fWantAna){fWantAna=0;
            if(fMode<0.5f)analyzeLoop();
            else if(fMode<1.5f)analyzeMelodie();
            else if(fMode<2.5f)analyzeInstrument();}
        if(fWantRefine){fWantRefine=0;
            if(fMode<0.5f&&!fLastOns.empty())generateLoop(fLastOns,fLastCls,fLastPit,true);
            else if(fMode<1.5f&&!fNotes.empty())analyzeMelodie();}
        if(fWantMid){fWantMid=0;writeMidAll();}
        if(fWantLoad){fWantLoad=0;if(loadIn_()<0)fStat=6;}

        // ANTWOORD fase-klok + response scheduling + stemmen
        if(antw){
            for(uint32_t i=0;i<frames;i++){
                if(fFase>0&&--fFaseLeft<=0){
                    if(fFase==1)endLuister();
                    else if(fFase==2){if(fAntLoop>0.5f){startLuister();fLuist=1;}else{fFase=0;fLuist=0;}}
                    else if(fFase==3){fFase=1;fCallN=0;fCallStart=fAbs+i;fFaseLeft=(int64_t)(64.0*fStep16);}
                }
                fProg=(fFase>0&&fStep16>0)?100.f*(1.f-(float)fFaseLeft/(64.f*(float)fStep16)):0.f;
                while((fFase==2||fFase==3)&&fRespI<fRespN&&
                      (int64_t)(fAbs+i-fRespStart)>=(int64_t)resp_[fRespI].at){
                    const RespN&rn=resp_[fRespI++];
                    respNote(rn.note,rn.vel,i,rn.dur);}
                for(int q=0;q<64;q++){PendingOff&o=offq_[q];
                    if(o.note&&o.at<=i){emitMidi(0x80,o.note,0,i);o.note=0;}}
                float o=0.f;
                for(auto&v:fAV){
                    if(!v.on)continue;
                    float s;
                    if(v.type==0){s=sinf(v.ph*2.f*M_PI)*0.7f+sinf(v.ph*4.f*M_PI)*0.2f;
                        v.lp+=0.3f*(s-v.lp);s=v.lp;}
                    else if(v.type==10){s=sinf(v.ph*2.f*M_PI);v.freq*=0.9985f;}
                    else if(v.type==11){s=(((frand()&0xFFFF)/32768.f)-1.f)*0.6f+sinf(v.ph*2.f*M_PI)*0.4f;}
                    else{s=(((frand()&0xFFFF)/32768.f)-1.f)*0.5f;}
                    o+=s*v.amp;v.amp*=v.dec;v.ph+=v.freq/sr;if(v.ph>1.f)v.ph-=1.f;
                    if(v.amp<0.0005f)v.on=false;
                }
                o=tanhf(o*1.3f);
                out[0][i]+=o;out[1][i]+=o;
            }
            fAbs+=frames;
            if(fFase!=0)fStat=(fFase==1?1:3);
        }

        // loop meespelen (modes 0 en 1) — alleen bij transport-play
        if(fMode<1.5f&&fOn>0.5f&&playing&&fStat==3&&fLoopLen>0&&!fNotes.empty()){
            uint32_t base=fLoopPos;
            while(fNIdx<fNotes.size()&&fNotes[fNIdx].at<base+frames){
                const N&n=fNotes[fNIdx];
                int fr=(int)(n.at-base);if(fr<0)fr=0;if(fr>=(int)frames)fr=frames-1;
                MidiEvent e{};e.frame=(uint32_t)fr;e.size=3;e.data[0]=0x90|n.ch;e.data[1]=n.note;e.data[2]=n.vel;
                writeMidiEvent(e);
                fNoteNow=n.note;
                fOff[fOffW].at=n.at+n.dur;fOff[fOffW].note=n.note;fOff[fOffW].ch=n.ch;fOffW=(fOffW+1)%512;
                fNIdx++;
            }
            for(int k=0;k<512;k++){
                if(fOff[k].note&&fOff[k].at>=base&&fOff[k].at<base+frames){
                    MidiEvent e{};e.frame=fOff[k].at-base;e.size=3;e.data[0]=0x80|fOff[k].ch;e.data[1]=fOff[k].note;e.data[2]=0;
                    writeMidiEvent(e);fOff[k].note=0;
                }
            }
            fLoopPos+=frames;
            if(fLoopPos>=fLoopLen){fLoopPos-=fLoopLen;fNIdx=0;
                for(int k=0;k<512;k++)if(fOff[k].at>=fLoopLen)fOff[k].at-=fLoopLen;}
        }
        float pk=0;for(uint32_t i=0;i<frames;i+=8){float a=fabsf(out[0][i]);if(a>pk)pk=a;}
        fLevelBlk=std::max(fLevelBlk*0.995f,pk);fLevel=fLevelBlk;
        if(antw)g2b_post(nullptr,out,frames,sr,fOn,100.f,fGal,nullptr,false);
    }

private:
    static const uint32_t kRecMax=8*48000;
    float fRec[kRecMax];uint32_t fRecN=0;
    // instrument-sample
    float fSmp[2*48000];int fSmpN=0;float fSmpHz=0;
    // params
    float fHeft=50,fMens=30,fLen=1,fVar=30,fOct=0,fPreset=0,fSchaal=0,fMode=0;
    float fOn=1,fLuist=0,fAntLoop=0;
    // gedeeld
    float fKlas=0,fKey=-1,fBpm=120,fStat=0,fHits=0,fLevel=0,fLevelBlk=0;
    int fWantAna=0,fWantMid=0,fWantRefine=0,fWantLoad=0,fBpmFromHost=0;
    double fStep16=0;uint32_t fLoopPos=0,fLoopLen=0,fNIdx=0,fRnd=12345;
    std::vector<N> fNotes;
    std::vector<int> fLastOns,fLastCls;std::vector<float> fLastPit;
    struct O{uint32_t at;uint8_t note,ch;};O fOff[512];int fOffW=0;
    // instrument-stemmen
    struct InstV{float pos=0,rate=0,amp=0,env=0;uint8_t note=0;int act=0;};
    InstV fIV[16];
    // antwoord-engine
    int fFase=0;int64_t fFaseLeft=0,fCallStart=0,fRespStart=0,fAbs=0;
    CallN call_[256];int fCallN=0;
    RespN resp_[256];int fRespN=0,fRespI=0;
    int fAKey=9,fAMinor=0;float fConf=0.f;bool fDrum=false;
    float fProg=0.f;int fNoteNow=0;
    uint32_t fSeed=12345u;
    struct AnsV{bool on=false;float freq=0,ph=0,amp=0,dec=0,lp=0;int type=0;};
    AnsV fAV[16];
    struct MidiRec{uint64_t fr;uint8_t note,ch,vel;uint16_t dur;};
    MidiRec rec_[4096];int recW_=0;uint64_t recN_=0;
    struct PendingOff{uint8_t note=0;uint32_t at=0;};PendingOff offq_[64];int offw_=0;
    struct Snap{std::vector<N>notes;RespN resp[256];int respN;};
    std::vector<Snap>undoStk_;
    Galaxy fGal;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GeluidMidiPlugin)
};

Plugin* createPlugin(){return new GeluidMidiPlugin();}
END_NAMESPACE_DISTRHO
