// G2B CO-PRODUCER — schrijft de tweede helft van jouw beat (100% eigen code).
// LUISTER: hij neemt jouw noten op en leert je stijl (tempo-feel, velocity,
// dichtheid, toonsoort, akkoorden). GEN: hij genereert het vervolg met
// metrische gewichten, voice-leading, counter-melodie en tension-build.
// ↓MID = de gegenereerde tweede helft als .mid (drums ch10, bass ch2, mel ch3).
#include "DistrhoPlugin.hpp"
#include "vendor/MidiFile.h"
#include "../g2b_std.h"
#include <cstring>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <sys/stat.h>
#include <cstdlib>
#include <cstdio>

START_NAMESPACE_DISTRHO

static const int kMaj[7]={0,2,4,5,7,9,11};
static const int kMin[7]={0,2,3,5,7,8,10};
// Krumhansl-achtige gewichten voor key-detect
static const float kMajW[12]={6.35f,2.23f,3.48f,2.33f,4.38f,4.09f,2.52f,5.19f,2.39f,3.66f,2.29f,2.88f};
static const float kMinW[12]={6.33f,2.68f,3.52f,5.38f,2.60f,3.53f,2.54f,4.75f,3.98f,2.69f,3.34f,3.17f};

// genre-smaak (zelfde tabel als DJEBU)
struct GStijl{float kick,hat,sync,oct;int dur;};
#ifdef G2B_GIO
static const int kNumPrograms=14;
static const char* kProgramNames[kNumPrograms]={
    "TRAP","DRILL","BOOMBAP","RNB","HOUSE","DEMBOW","KAWINA",
    "LOFI","SOUL","POP","AMBIENT","SOCA","DANCEHALL","ZOUK"};
static const GStijl kStijl[kNumPrograms]={
    {0.35f,0.85f,0.5f,0.7f,2},{0.30f,0.50f,0.7f,0.5f,3},{0.40f,0.55f,0.6f,0.2f,2},
    {0.30f,0.60f,0.8f,0.3f,4},{0.65f,0.75f,0.3f,0.4f,4},{0.45f,0.60f,0.6f,0.5f,3},
    {0.50f,0.65f,0.9f,0.4f,2},{0.25f,0.45f,0.4f,0.2f,6},{0.35f,0.55f,0.7f,0.3f,4},
    {0.50f,0.65f,0.3f,0.3f,4},{0.18f,0.25f,0.2f,0.4f,8},{0.55f,0.70f,0.7f,0.5f,2},
    {0.42f,0.55f,0.8f,0.4f,3},{0.38f,0.60f,0.7f,0.3f,4}};
#else
static const int kNumPrograms=10;
static const char* kProgramNames[kNumPrograms]={
    "TRAP","DRILL","BOOMBAP","RNB","HOUSE","DEMBOW","KAWINA","LOFI","SOUL","POP"};
static const GStijl kStijl[kNumPrograms]={
    {0.35f,0.85f,0.5f,0.7f,2},{0.30f,0.50f,0.7f,0.5f,3},{0.40f,0.55f,0.6f,0.2f,2},
    {0.30f,0.60f,0.8f,0.3f,4},{0.65f,0.75f,0.3f,0.4f,4},{0.45f,0.60f,0.6f,0.5f,3},
    {0.50f,0.65f,0.9f,0.4f,2},{0.25f,0.45f,0.4f,0.2f,6},{0.35f,0.55f,0.7f,0.3f,4},
    {0.50f,0.65f,0.3f,0.3f,4}};
#endif

struct Ev{uint8_t kind,note,vel;uint32_t at,dur;};
// kind: 0=kick 1=snare 2=hat 3=bass 4=mel 5=counter 6=chord 7=cc11(tension)
struct RecN{uint8_t note,vel;int64_t on,off;};

class CoproPlugin : public Plugin {
public:
    enum Params{kDurf,kMens,kSwing,kDice,kPreset,kOn,kKracht,kLuis,kGen,kMidi,kExport,
                kKeyO,kConfO,kLevelO,kNoteO,kBarO,kNumParams};
    CoproPlugin():Plugin(kNumParams,0,0){}
protected:
    const char* getLabel() const override{return "G2B Co-Producer";}
    const char* getDescription() const override{
        return "AI co-producer: hoort je halve beat en schrijft de tweede helft in jouw stijl.";}
    const char* getMaker() const override{return "G2B";}
    const char* getLicense() const override{return "G2B";}
    uint32_t getVersion() const override{return d_version(1,0,0);}
    int64_t getUniqueId() const override{
#ifdef G2B_GIO
        return d_cconst('G','2','C','g');
#else
        return d_cconst('G','2','C','p');
#endif
    }

    void initParameter(uint32_t i, Parameter& p) override {
        switch(i){
        case kDurf:   p.name="DURF";p.symbol="durf";p.ranges.min=0;p.ranges.max=100;p.ranges.def=35;p.unit="%";break;
        case kMens:   p.name="MENS";p.symbol="mens";p.ranges.min=0;p.ranges.max=100;p.ranges.def=40;p.unit="%";break;
        case kSwing:  p.name="SWING";p.symbol="swing";p.ranges.min=0;p.ranges.max=100;p.ranges.def=20;p.unit="%";break;
        case kDice:   p.name="DICE";p.symbol="dice";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kPreset: p.name="PRESET";p.symbol="preset";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=(double)(kNumPrograms-1);p.ranges.def=0;break;
        case kOn:     p.name="Aan";p.symbol="aan";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;break;
        case kKracht: p.name="Volume";p.symbol="kracht";p.ranges.min=0;p.ranges.max=100;p.ranges.def=100;p.unit="%";break;
        case kLuis:   p.name="LUISTER";p.symbol="luis";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kGen:    p.name="GEN";p.symbol="gen";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kMidi:   p.name="MIDI UIT";p.symbol="midiuit";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;break;
        case kExport: p.name="EXPORT";p.symbol="export";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kKeyO:   p.name="KEY";p.symbol="key";p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=23;p.ranges.def=9;break;
        case kConfO:  p.name="CONF";p.symbol="conf";p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=100;p.ranges.def=0;break;
        case kLevelO: p.name="LEVEL";p.symbol="level";p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kNoteO:  p.name="NOTE";p.symbol="note";p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=128;p.ranges.def=0;break;
        case kBarO:   p.name="BAR";p.symbol="bar";p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=16;p.ranges.def=0;break;
        default:break;}
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){case kDurf:return fDurf;case kMens:return fMens;case kSwing:return fSwing;
                  case kDice:return 0.f;case kPreset:return fPreset;case kOn:return fOn;
                  case kKracht:return fKr;case kLuis:return fLuis;case kGen:return fGen;
                  case kMidi:return fMidi;case kExport:return 0.f;
                  case kKeyO:return (float)fRoot+12.f*fMinor;case kConfO:return fConf;
                  case kLevelO:return fLevel;case kNoteO:return (float)fNoteNow;
                  case kBarO:return (float)fStepInBar;}
        return 0.f;
    }
    void setParameterValue(uint32_t i,float v) override {
        switch(i){
        case kDurf:fDurf=v;break;case kMens:fMens=v;break;case kSwing:fSwing=v;break;
        case kDice:if(v>0.5f)rollDice();break;
        case kPreset:fPreset=std::min<float>(v+0.5f,kNumPrograms-1);if(fPreset<0)fPreset=0;break;
        case kOn:fOn=v;break;case kKracht:fKr=v;break;
        case kLuis:
            if(v>0.5f&&fLuis<0.5f){fRecN=0;fRecStart=fAbs;gridReset();} // start opname
            fLuis=v;break;
        case kGen:
            fGen=v;
            if(v>0.5f){fLuis=0.f;if(fRecN>=3){analyze();generate();fPlayStart=fAbs;fEvI=0;fEvPlay=1;}
                       else fConf=0.f;}
            break;
        case kMidi:fMidi=v;break;
        case kExport:if(v>0.5f)writeMid();break;
        default:break;}
    }
    void initState(uint32_t,String&,String&) override{}
    void setState(const char*,const char*) override{}

    void activate() override{fAbs=0;fEvI=0;fRecN=0;fPlayStart=0;
        fStep16=getSampleRate()*60.0/120.0/4.0;gridReset();}

    // ---------- stemmen ----------
    struct Voice{bool on=false;float freq=0,ph=0,amp=0,dec=0,lp=0;int type=0;};
    Voice fV[16];
    void pluck(int note,int vel,float dur=0.35f){
        for(auto&v:fV)if(!v.on){v.on=true;v.freq=440.f*powf(2.f,(note-69)/12.f);v.ph=0;
            v.amp=vel/127.f*0.8f;v.dec=expf(-1.f/(dur*getSampleRate()));v.lp=0;v.type=0;return;}
        fV[0].freq=440.f*powf(2.f,(note-69)/12.f);fV[0].ph=0;fV[0].amp=vel/127.f*0.8f;fV[0].on=true;fV[0].type=0;
    }
    void drumHit(int kind,int vel){
        for(auto&v:fV)if(!v.on){v.on=true;v.type=kind+10;v.amp=vel/127.f;
            v.dec=expf(-1.f/((kind==0?0.18f:kind==1?0.12f:0.05f)*getSampleRate()));
            v.freq=kind==0?150.f:220.f;v.ph=0;return;}
    }
    void emitMidi(uint8_t st,uint8_t nt,uint8_t vel,uint32_t fr){
        MidiEvent e{};e.frame=fr;e.size=4;e.data[0]=st;e.data[1]=nt;e.data[2]=vel;e.data[3]=0;
        writeMidiEvent(e);}
    void fire(const Ev&e,uint32_t i){
        if(e.kind==7){ // CC11 tension automation
            if(fMidi>0.5f)emitMidi(0xB0|2,11,e.vel,i);
            return;}
        int note=e.note;
        if(e.kind<=2){drumHit(e.kind,e.vel);note=36+((e.kind==1?2:e.kind)*2);}
        else pluck(note,e.vel,e.dur/44100.f<0.15f?0.15f:e.dur/44100.f);
        fNoteNow=note;
        MidiRec&r=rec_[recW_];r.fr=fAbs+i;r.note=(uint8_t)note;
        r.ch=e.kind<=2?9:(e.kind==3?1:(e.kind==5?3:2));r.vel=e.vel;
        r.dur=(uint16_t)std::min<uint32_t>(60000,e.dur);recW_=(recW_+1)%4096;recN_++;
        if(fMidi>0.5f){
            uint8_t ch=e.kind<=2?9:(e.kind==3?1:2);
            emitMidi(0x90|ch,(uint8_t)note,(uint8_t)e.vel,i);
            PendingOff&o=offq_[offw_];o.note=(uint8_t)note;o.ch=ch;o.at=i+e.dur;offw_=(offw_+1)%128;}
    }
    void rollDice(){fDurf=10.f+(frand()%800)/10.f;fMens=10.f+(frand()%700)/10.f;
                    fSwing=(frand()%500)/10.f;fPreset=(float)(frand()%kNumPrograms);}
    uint32_t frand(){fSeed=fSeed*1664525u+1013904223u;return fSeed>>9;}

    // ---------- stijl-matrix: 16-staps groove-vingerafdruk (EMA-lerend) ----------
    struct TFP{float velSpread,tShift;};TFP fGrid[16];
    void gridReset(){
        for(int i=0;i<16;i++){
            if(i%4==0)fGrid[i]={0.f,0.f};
            else if(i%2==0)fGrid[i]={-0.04f,0.008f};
            else fGrid[i]={-0.09f,0.025f};}}
    void gridTrain(int step,double pos,uint8_t vel){
        TFP&g=fGrid[step&15];
        g.velSpread=g.velSpread*0.75f+((vel/127.f)-0.5f)*0.25f;
        g.tShift=g.tShift*0.75f+(float)(pos-std::floor(pos+0.5f))*0.25f;}

    // ---------- opname van jouw eerste helft ----------
    void noteOn(int note,int vel){
        if(fLuis>0.5f&&fRecN<512){
            // open noot registreren
            for(int i=0;i<fRecN;i++)if(rec2_[i].note==note&&rec2_[i].off<0){rec2_[i].vel=vel;return;}
            RecN&r=rec2_[fRecN++];r.note=note;r.vel=vel;r.on=fAbs;r.off=-1;
            if(fStep16>0){double pos=(r.on-fRecStart)/fStep16;gridTrain((int)pos,pos,vel);}
        }
        // held-tracking (zoals DJEBU) voor live toonsoort-hint
        for(int i=0;i<fHeldN;i++)if(fHeld[i].note==note){fHeld[i].vel=vel;fHeld[i].at=fAbs;return;}
        if(fHeldN<16){Held&h=fHeld[fHeldN++];h.note=note;h.vel=vel;h.at=fAbs;}
    }
    void noteOff(int note){
        if(fLuis>0.5f)for(int i=0;i<fRecN;i++)
            if(rec2_[i].note==note&&rec2_[i].off<0){rec2_[i].off=fAbs;break;}
        for(int i=0;i<fHeldN;i++)if(fHeld[i].note==note){fHeld[i]=fHeld[--fHeldN];break;}
    }
    int scaleDeg(int deg)const{
        const int*sc=fMinor?kMin:kMaj;int oct=deg/7,d=deg%7;if(d<0){d+=7;oct--;}
        return sc[d]+12*oct;
    }

    // ---------- STIJL-VINGERAFDRUK ----------
    void analyze(){
        fBars=0;fVelMean=90.f;fVelStd=15.f;fGroove=0.f;fDens=0.5f;
        int64_t lastOn=0;
        for(int i=0;i<fRecN;i++)if(rec2_[i].on>lastOn)lastOn=rec2_[i].on;
        if(lastOn<=fRecStart)fRecN=0;
        if(fRecN<3)return;
        fBars=(int)std::max<int64_t>(2,(lastOn-fRecStart)/(int64_t)(16*fStep16));
        if(fBars>8)fBars=8;
        // key-detect: pitch-class histogram vs Krumhansl-gewichten
        float hist[12]={0};
        for(int i=0;i<fRecN;i++)hist[rec2_[i].note%12]+=1.f;
        float best=-1e9f;fRoot=9;fMinor=0;
        for(int r=0;r<12;r++)for(int mm=0;mm<2;mm++){
            const float*w=mm?kMinW:kMajW;const int*sc=mm?kMin:kMaj;float s=0;
            for(int p=0;p<12;p++){
                int rel=((p-r)%12+12)%12;bool in=false;
                for(int d=0;d<7;d++)if(sc[d]==rel){in=true;break;}
                s+=hist[p]*(in?w[rel]:-1.f);}
            if(s>best){best=s;fRoot=r;fMinor=mm;}}
        // velocity fingerprint
        float vm=0,vs=0;
        for(int i=0;i<fRecN;i++)vm+=rec2_[i].vel;vm/=fRecN;
        for(int i=0;i<fRecN;i++){float d=rec2_[i].vel-vm;vs+=d*d;}
        fVelMean=vm;fVelStd=sqrtf(vs/fRecN);
        // micro-timing: gemiddelde afwijking tov 16e-grid (fractie van 16e)
        double acc=0;int cnt=0;
        for(int i=0;i<fRecN;i++){
            double pos=(rec2_[i].on-fRecStart)/fStep16;
            double near=std::round(pos);acc+=pos-near;cnt++;}
        fGroove=cnt?(float)(acc/cnt):0.f;
        if(fGroove>0.15f)fGroove=0.15f;if(fGroove<-0.15f)fGroove=-0.15f;
        // dichtheid per 16e
        fDens=fRecN/(float)(fBars*16);if(fDens>1.5f)fDens=1.5f;
        // akkoord-progressie: laagste noot per maat
        for(int b=0;b<fBars&&b<8;b++){
            int lo=127;
            for(int i=0;i<fRecN;i++){
                int bar=(int)((rec2_[i].on-fRecStart)/(16*(int64_t)fStep16));
                if(bar==b&&rec2_[i].note<lo)lo=rec2_[i].note;}
            fProg[b]=lo<127?(lo%12):fRoot;}
        // markov intervallen (melodie): delta-histogram
        memset(fMark,0,sizeof(fMark));
        const RecN* sorted=rec2_; // on is al chronologisch genoeg
        for(int i=1;i<fRecN;i++){
            if(sorted[i].note>51&&sorted[i-1].note>51){ // alleen melodieuze region
                int d=(int)sorted[i].note-(int)sorted[i-1].note;
                if(d>6)d=6;if(d<-6)d=-6;fMark[d+6]++;}}
        int tot=0;for(int d=0;d<13;d++)tot+=fMark[d];
        fConf=tot?std::min(95.f,40.f+tot*2.f):50.f;
    }

    // ---------- GENEREREN: tweede helft ----------
    void pushGen(int kind,int note,int vel,int64_t step,int64_t durSteps){
        if(fGenN>=1500)return;Ev&e=gen_[fGenN++];
        e.kind=(uint8_t)kind;e.note=(uint8_t)note;e.vel=(uint8_t)(vel<20?20:(vel>127?127:vel));
        e.at=(uint32_t)(step*fStep16);e.dur=(uint32_t)(durSteps*fStep16*0.9f);
    }
    int markDelta(){
        // softmax over markov-tellingen; DURF = temperatuur
        const float temp=std::max(0.05f,fDurf/100.f);
        float w[13],sum=0;
        for(int d=0;d<13;d++){w[d]=fMark[d]>0?expf(logf((float)fMark[d])/temp):0.f;sum+=w[d];}
        if(sum<=0.f)return ((frand()%5))-2;
        float pick=((frand()&0xFFFF)/65535.f)*sum,acc=0;
        for(int d=0;d<13;d++){acc+=w[d];if(pick<acc)return d-6;}
        return 0;
    }
    void generate(){
        fGenN=0;const GStijl&st=kStijl[(int)fPreset];
        const int B=fBars*2; // tweede helft even lang als eerste
        const float temp=fDurf/100.f; // anti-cliché: 0=strak voorspelbaar, 1=gewaagd
        int melLast=fLastMel>0?fLastMel:(fRoot+60);
        // melodie-continuatie vanuit laatste opgenomen hoge noot
        for(int i=fRecN-1;i>=0;i--)if(rec2_[i].note>=55){melLast=rec2_[i].note;break;}
        for(int b=0;b<B&&b<16;b++){
            // akkoord voor deze maat: herhaal progressie, laatste 2 maten vijf-variatie
            int cr;
            if(b<fBars)cr=fProg[b];
            else{
                int prev=fProg[b-1<0?0:b-1];
                cr=(temp>0.4f&&(b==B-1))?((prev+7)%12):fProg[b%fBars];
            }
            int proot=48+cr; // bass-regio
            const bool tension=(b>=B-2); // laatste 2 maten: opbouw
            for(int s=0;s<16;s++){
                uint32_t r=frand();
                float rnd=((r>>9)&0xFFFF)/65535.f;
                const int64_t step=(int64_t)b*16+s;
                // metrische gewichten: tel 1&3 sterk (s==0,8), tel 2&4 medium (s==4,12)
                float mw=s==0?1.f:(s==8?0.9f:(s==4||s==12?0.7f:0.35f));
                // KICK
                if(s==0||rnd<st.kick*mw+(st.sync>0.5f&&(s==6||s==11)?0.3f:0.f))
                    pushGen(0,36,95+(int)(rnd*30),step,1);
                // SNARE + fills in tension-maten
                if(s==4||s==12||(tension&&s>=13&&rnd<0.5f))
                    pushGen(1,38,85+(int)(rnd*35),step,1);
                // HAT: dichtheid groeit bij tension
                rnd=((r=(r*1664525u+1013904223u),((r>>9)&0xFFFF)/65535.f));
                float hd=st.hat*(tension?1.3f:1.f);
                if(rnd<hd&&(s%2==0||st.hat>0.6f))pushGen(2,42,55+(int)(rnd*45),step,1);
                // BASS: voice-led — kortste afstand van vorige bassnoot
                if(s%4==0&&rnd<0.8f){
                    int cand[3]={proot,proot+7,proot+scaleDeg(4)};
                    int best=cand[0],bd=99;
                    for(int c=0;c<3;c++){int d=std::abs(cand[c]-fLastBass);if(d<bd){bd=d;best=cand[c];}}
                    fLastBass=best;
                    int n=36+(best%24);if(n<36)n+=12;
                    pushGen(3,n,(int)(fVelMean*0.8f),step,st.dur);
                }
                // MELODIE: markov-delta + metrische voorkeur voor akkoordtonen
                float dp=fDens*0.35f*mw;
                if(temp>0.6f)dp*=1.2f;
                if(rnd<dp&&s%2==0){
                    int d=markDelta();
                    if(temp>0.5f&&(frand()&3)==0)d+=(frand()&1?12:-12); // gewaagde octaafsprong
                    melLast+=d;
                    // voer terug naar toonladder als hij eruit valt
                    int pc=((melLast-fRoot)%12+12)%12;bool in=false;
                    const int*sc=fMinor?kMin:kMaj;
                    for(int k=0;k<7;k++)if(sc[k]==pc){in=true;break;}
                    if(!in)melLast+=(frand()&1?1:-1);
                    if(melLast<55)melLast+=12;if(melLast>84)melLast-=12;
                    int vel=(int)(fVelMean+(((frand()>>9)&0xFFFF)/65535.f-0.5f)*2.f*fVelStd);
                    pushGen(4,melLast,vel,step,2);
                    // COUNTER-melodie: terts erboven als dichtheid laag is
                    if(fDens<0.6f&&(frand()&3)==0)
                        pushGen(5,melLast+scaleDeg(2)%12+ (scaleDeg(2)<pc?12:0),vel-15,step+1,2);
                }
                // CHORD-stab op tel 2/4 bij mid-densiteit
                if((s==4||s==12)&&st.sync>0.5f&&rnd<0.4f)
                    for(int c=0;c<3;c++)
                        pushGen(6,60+cr+scaleDeg(c*2),60+(int)(rnd*30),step,2);
            }
            // TENSION: CC11-ramp in laatste 2 maten
            if(tension)for(int s=0;s<16;s+=4)
                pushGen(7,0,40+(s+(b-(B-2))*16)*3,(int64_t)b*16+s,1);
        }
        // groove op alles: per-stap vingerafdruk (geleerd) + MENS-jitter
        float tJ=fMens/100.f*0.15f, vJ=fMens/100.f*fVelStd;
        for(int e=0;e<fGenN;e++){
            Ev&ev=gen_[e];
            int s=(int)(ev.at/fStep16);
            const TFP&g=fGrid[s&15];
            float spos=(float)s+(fSwing/100.f*0.5f)*(s%2==1?1.f:0.f)
                       +(((frand()&0xFFFF)/65535.f)-0.5f)*tJ+g.tShift;
            ev.at=(uint32_t)std::max<int>(0,(int)(spos*fStep16));
            if(ev.kind!=7){
                int vel=ev.vel+(int)(g.velSpread*24.f+((frand()&0xFFFF)/65535.f-0.5f)*vJ);
                ev.vel=(uint8_t)(vel<20?20:(vel>127?127:vel));}
        }
        for(int i=1;i<fGenN;i++){Ev t=gen_[i];int j=i-1;
            while(j>=0&&gen_[j].at>t.at){gen_[j+1]=gen_[j];j--;}gen_[j+1]=t;}
    }

    // ---------- export ----------
    struct MidiRec{uint64_t fr;uint8_t note,ch,vel;uint16_t dur;};
    MidiRec rec_[4096];int recW_=0;uint64_t recN_=0;
    void writeMid(){
        // schrijf de GEGENEREERDE tweede helft (niet de live-rec)
        if(fGenN==0)return;
        const double sr=getSampleRate(),fpt=sr*60.0/(fBpm*480.0);
        smf::MidiFile mf;mf.setTicksPerQuarterNote(480);
        int seq=0;
        for(int e=0;e<fGenN;e++){const Ev&ev=gen_[e];
            if(ev.kind==7)continue;
            int note=ev.kind<=2?36+((ev.kind==1?2:ev.kind)*2):ev.note;
            uint8_t ch=ev.kind<=2?9:(ev.kind==3?1:(ev.kind==5?3:2));
            smf::MidiEvent on(0x90|ch,note,ev.vel);on.tick=(int)(ev.at/fpt);on.seq=seq++;
            smf::MidiEvent off(0x80|ch,note,0);off.tick=(int)((ev.at+ev.dur)/fpt);off.seq=seq++;
            mf[0].push_back(on);mf[0].push_back(off);}
        mf.sortTracks();
        const char*home=std::getenv("USERPROFILE");if(!home||!*home)home=std::getenv("HOME");
        char dir[520],path[600];
        if(home&&*home)std::snprintf(dir,sizeof(dir),"%s/Music/G2B",home);else std::snprintf(dir,sizeof(dir),".");
        #ifdef _WIN32
        {std::string m=std::string(home?home:".")+"/Music";mkdir(m.c_str());mkdir(dir);}
        #else
        {std::string m=std::string(home?home:".")+"/Music";mkdir(m.c_str(),0755);mkdir(dir,0755);}
        #endif
        std::snprintf(path,sizeof(path),"%s/G2B-COPRO-%u.mid",dir,(unsigned)(fGenN%100000));
        mf.write(path);
    }

    void run(const float**, float** out, uint32_t frames, const MidiEvent* evs, uint32_t nev) override {
        const float sr=getSampleRate();
        if(fStep16<=0){fBpm=120.0;fStep16=sr*60.0/fBpm/4.0;}
        const TimePosition&tp=getTimePosition();
        if(tp.bbt.valid&&tp.bbt.beatsPerMinute>1.0){
            fBpm=tp.bbt.beatsPerMinute;fStep16=sr*60.0/fBpm/4.0;}
        for(uint32_t m=0;m<nev;m++){const MidiEvent&e=evs[m];
            if(e.size>=3){
                uint8_t st=e.data[0]&0xF0;
                if(st==0x90&&e.data[2]>0)noteOn(e.data[1],e.data[2]);
                else if(st==0x80||(st==0x90&&e.data[2]==0))noteOff(e.data[1]);
            }}

        memset(out[0],0,frames*sizeof(float));memset(out[1],0,frames*sizeof(float));
        float levelPeak=0.f;
        const bool bandOn=fGen>0.5f&&fEvPlay&&fOn>0.5f;
        const int64_t secLen=(int64_t)(fBars*2*16*fStep16);
        for(uint32_t i=0;i<frames;i++){
            if(bandOn&&secLen>0){
                int64_t rel=fAbs+i-fPlayStart;
                int64_t relSec=rel%secLen;
                if(relSec<fLastRel){fEvI=0;} // sectie gewikkeld → opnieuw
                fLastRel=relSec;
                fStepInBar=(int)(relSec/(int64_t)fStep16)%16;
                while(fEvI<fGenN&&(int64_t)gen_[fEvI].at<=relSec){fire(gen_[fEvI],i);fEvI++;}
            }
            for(int q=0;q<128;q++){PendingOff&o=offq_[q];
                if(o.note&&o.at<=i){emitMidi(0x80|o.ch,o.note,0,i);o.note=0;}}
            float o=0.f;
            for(auto&v:fV){
                if(!v.on)continue;
                float s;
                if(v.type==0){
                    s=sinf(v.ph*2.f*M_PI)*0.7f+sinf(v.ph*4.f*M_PI)*0.2f;
                    v.lp+=0.3f*(s-v.lp);s=v.lp;
                }else if(v.type==10){s=sinf(v.ph*2.f*M_PI);v.freq*=0.9985f;}
                else if(v.type==11){s=(((frand()&0xFFFF)/32768.f)-1.f)*0.6f+sinf(v.ph*2.f*M_PI)*0.4f;}
                else{s=(((frand()&0xFFFF)/32768.f)-1.f)*0.5f;}
                o+=s*v.amp;v.amp*=v.dec;v.ph+=v.freq/sr;if(v.ph>1.f)v.ph-=1.f;
                if(v.amp<0.0005f)v.on=false;
            }
            o=tanhf(o*1.3f);
            out[0][i]=o;out[1][i]=o;
            float a=std::fabs(o);if(a>levelPeak)levelPeak=a;
        }
        fAbs+=frames;
        fLevel=fLevel*0.7f+levelPeak*0.3f;
        g2b_post(nullptr,out,frames,sr,fOn,fKr,fGal,nullptr,false);
    }

private:
    float fDurf=35.f,fMens=40.f,fSwing=20.f,fPreset=0.f,fOn=1.f,fKr=100.f;
    float fLuis=0.f,fGen=0.f,fMidi=1.f;
    int64_t fAbs=0,fPlayStart=0,fRecStart=0;
    struct Held{uint8_t note,vel;int64_t at;};Held fHeld[16];int fHeldN=0;
    int fRoot=9,fMinor=0;float fConf=0.f;
    RecN rec2_[512];int fRecN=0;
    int fBars=0;float fVelMean=90.f,fVelStd=15.f,fGroove=0.f,fDens=0.5f;
    int fProg[8]={0};int fMark[13]={0};int fLastBass=48;int fLastMel=60;
    Ev gen_[1500];int fGenN=0,fEvI=0;bool fEvPlay=false;int fStepInBar=0;
    int64_t fLastRel=0;
    float fLevel=0.f;int fNoteNow=0;
    double fStep16=0,fBpm=120.0;
    uint32_t fSeed=77777u;
    Galaxy fGal;
    struct PendingOff{uint8_t note=0,ch=0;uint32_t at=0;};PendingOff offq_[128];int offw_=0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CoproPlugin)
};

Plugin* createPlugin(){return new CoproPlugin();}
END_NAMESPACE_DISTRHO
