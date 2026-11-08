// G2B DJEBU — meespelend bandlid (100% eigen code).
// Speel akkoorden/noten → hij speelt automatisch bass + drums mee in jouw genre,
// toonsoort en gevoel. Loopt continu zolang SPEL aan staat, syncs op host-BPM.
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

#ifdef G2B_GIO
static const int kNumPrograms=14;
static const char* kProgramNames[kNumPrograms]={
    "TRAP","DRILL","BOOMBAP","RNB","HOUSE","DEMBOW","KAWINA",
    "LOFI","SOUL","POP","AMBIENT","SOCA","DANCEHALL","ZOUK"};
#else
static const int kNumPrograms=10;
static const char* kProgramNames[kNumPrograms]={
    "TRAP","DRILL","BOOMBAP","RNB","HOUSE","DEMBOW","KAWINA","LOFI","SOUL","POP"};
#endif

// genre-smaak: {kickdichtheid, hatdichtheid, syncopatie, octaafsprong, notenduur16}
struct GStijl{float kick,hat,sync,oct;int dur;};
static const GStijl kStijl[kNumPrograms]={
    {0.35f,0.85f,0.5f,0.7f,2},  // TRAP
    {0.30f,0.50f,0.7f,0.5f,3},  // DRILL
    {0.40f,0.55f,0.6f,0.2f,2},  // BOOMBAP
    {0.30f,0.60f,0.8f,0.3f,4},  // RNB
    {0.65f,0.75f,0.3f,0.4f,4},  // HOUSE  four-on-floor
    {0.45f,0.60f,0.6f,0.5f,3},  // DEMBOW
    {0.50f,0.65f,0.9f,0.4f,2},  // KAWINA
    {0.25f,0.45f,0.4f,0.2f,6},  // LOFI
    {0.35f,0.55f,0.7f,0.3f,4},  // SOUL
    {0.50f,0.65f,0.3f,0.3f,4},  // POP
#ifdef G2B_GIO
    {0.18f,0.25f,0.2f,0.4f,8},  // AMBIENT
    {0.55f,0.70f,0.7f,0.5f,2},  // SOCA
    {0.42f,0.55f,0.8f,0.4f,3},  // DANCEHALL
    {0.38f,0.60f,0.7f,0.3f,4}   // ZOUK
#endif
};

static const int kMaj[7]={0,2,4,5,7,9,11};
static const int kMin[7]={0,2,3,5,7,8,10};
// basslijn-graden per halve tel (8 posities/maat): wortel,5,b7,oct variaties
static const int kBassSeq[8]={0,0,4,0,3,2,4,0};

struct Ev{uint8_t kind,note,vel;uint32_t at,dur;}; // kind 0=kick 1=snare 2=hat 3=bass 4=chord

class DjebuPlugin : public Plugin {
public:
    enum Params{kVolh,kMens,kSwing,kDice,kPreset,kOn,kKracht,kSpel,kMidi,kExport,
                kKeyO,kConfO,kLevelO,kNoteO,kBarO,kNumParams};
    DjebuPlugin():Plugin(kNumParams,0,0){applyPreset(0);}
protected:
    const char* getLabel() const override{return "G2B Djebu";}
    const char* getDescription() const override{
        return "G2B bandlid: speel akkoorden, hij speelt bass + drums mee in jouw stijl.";}
    const char* getMaker() const override{return "G2B";}
    const char* getLicense() const override{return "G2B";}
    uint32_t getVersion() const override{return d_version(1,0,0);}
    int64_t getUniqueId() const override{
#ifdef G2B_GIO
        return d_cconst('G','2','D','g');
#else
        return d_cconst('G','2','D','j');
#endif
    }

    void initParameter(uint32_t i, Parameter& p) override {
        switch(i){
        case kVolh:   p.name="VOLHED";p.symbol="volh";p.ranges.min=0;p.ranges.max=100;p.ranges.def=55;p.unit="%";break;
        case kMens:   p.name="MENS";p.symbol="mens";p.ranges.min=0;p.ranges.max=100;p.ranges.def=40;p.unit="%";break;
        case kSwing:  p.name="SWING";p.symbol="swing";p.ranges.min=0;p.ranges.max=100;p.ranges.def=20;p.unit="%";break;
        case kDice:   p.name="DICE";p.symbol="dice";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kPreset: p.name="PRESET";p.symbol="preset";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=(double)(kNumPrograms-1);p.ranges.def=0;break;
        case kOn:     p.name="Aan";p.symbol="aan";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;break;
        case kKracht: p.name="Volume";p.symbol="kracht";p.ranges.min=0;p.ranges.max=100;p.ranges.def=100;p.unit="%";break;
        case kSpel:   p.name="SPEL";p.symbol="spel";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
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
        switch(i){case kVolh:return fVolh;case kMens:return fMens;case kSwing:return fSwing;
                  case kDice:return 0.f;case kPreset:return fPreset;case kOn:return fOn;
                  case kKracht:return fKr;case kSpel:return fSpel;case kMidi:return fMidi;
                  case kExport:return 0.f;
                  case kKeyO:return (float)fRoot+12.f*fMinor;case kConfO:return fConf;
                  case kLevelO:return fLevel;case kNoteO:return (float)fNoteNow;
                  case kBarO:return (float)fStepInBar;}
        return 0.f;
    }
    void setParameterValue(uint32_t i,float v) override {
        switch(i){
        case kVolh:fVolh=v;break;case kMens:fMens=v;break;case kSwing:fSwing=v;break;
        case kDice:if(v>0.5f)rollDice();break;
        case kPreset:applyPreset((int)(v+0.5f));break;
        case kOn:fOn=v;break;case kKracht:fKr=v;break;
        case kSpel:fSpel=v;if(v>0.5f){fSpelStart=fAbs;fEvI=0;fEvN=0;}break;
        case kMidi:fMidi=v;break;
        case kExport:if(v>0.5f)writeMid();break;
        default:break;}
    }
    void initState(uint32_t,String&,String&) override{}
    void setState(const char*,const char*) override{}

    void activate() override{fAbs=0;fEvI=0;fEvN=0;fHeldN=0;fSpelStart=0;
        fStep16=getSampleRate()*60.0/120.0/4.0;genBar();}

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
        int note=e.note;
        if(e.kind<=2){drumHit(e.kind,e.vel);note=36+((e.kind==1?2:e.kind)*2);} // GM: 36,38,42
        else pluck(note,e.vel,e.dur/44100.f<0.15f?0.15f:e.dur/44100.f);
        fNoteNow=note;
        MidiRec&r=rec_[recW_];r.fr=fAbs+i;r.note=(uint8_t)note;
        r.ch=e.kind<=2?9:(e.kind==3?1:2);r.vel=e.vel;
        r.dur=(uint16_t)std::min<uint32_t>(60000,e.dur);recW_=(recW_+1)%4096;recN_++;
        if(fMidi>0.5f){
            uint8_t ch=e.kind<=2?9:1;
            emitMidi(0x90|ch,(uint8_t)note,(uint8_t)e.vel,i);
            PendingOff&o=offq_[offw_];o.note=(uint8_t)note;o.ch=ch;o.at=i+e.dur;offw_=(offw_+1)%128;}
    }

    void applyPreset(int pi){if(pi<0)pi=0;if(pi>=kNumPrograms)pi=kNumPrograms-1;fPreset=(float)pi;}
    void rollDice(){fVolh=20.f+(frand()%800)/10.f;fMens=10.f+(frand()%700)/10.f;
                    fSwing=(frand()%500)/10.f;applyPreset(frand()%10);}
    uint32_t frand(){fSeed=fSeed*1664525u+1013904223u;return fSeed>>9;}

    // ---------- akkoord-tracker ----------
    void noteOn(int note,int vel){
        for(int i=0;i<fHeldN;i++)if(fHeld[i].note==note){fHeld[i].vel=vel;fHeld[i].at=fAbs;return;}
        if(fHeldN<16){Held&h=fHeld[fHeldN++];h.note=note;h.vel=vel;h.at=fAbs;}
        chordDetect();
    }
    void noteOff(int note){
        for(int i=0;i<fHeldN;i++)if(fHeld[i].note==note){fHeld[i]=fHeld[--fHeldN];break;}
    }
    void chordDetect(){
        if(fHeldN==0)return;
        int lo=127;for(int i=0;i<fHeldN;i++)if(fHeld[i].note<lo)lo=fHeld[i].note;
        fRoot=lo%12;
        bool b3=false,m3=false,b7=false;
        for(int i=0;i<fHeldN;i++){int r=((fHeld[i].note-lo)%12+12)%12;
            if(r==4)b3=true;if(r==3)m3=true;if(r==10)b7=true;}
        fMinor=m3&&!b3?1:0;
        fConf=fHeldN>=3?90.f:fHeldN*30.f;
    }
    int scaleDeg(int deg)const{
        const int*sc=fMinor?kMin:kMaj;int oct=deg/7,d=deg%7;if(d<0){d+=7;oct--;}
        return sc[d]+12*oct;
    }

    // ---------- band genereren: 1 maat (16 zestienden) ----------
    void genBar(){
        fEvN=0;const GStijl&st=kStijl[(int)fPreset];
        const float dens=fVolh/100.f;
        const double sr=getSampleRate();
        uint32_t r=fSeed*2654435761u+fBar*97u+7u;
        for(int s=0;s<16;s++){
            float rnd=(r=(r*1664525u+1013904223u),((r>>9)&0xFFFF)/65535.f);
            // KICK: tel 1 + syncope-plekken
            if(s==0||rnd<st.kick*dens+(st.sync>0.5f&&(s==6||s==11)?0.35f:0.f))
                pushEv(0,36,95+(int)(rnd*30),s);
            // SNARE: 2 en 4
            if(s==4||s==12||(st.sync>0.6f&&s==14&&rnd<0.3f))
                pushEv(1,38,85+(int)(rnd*35),s);
            // HAT: dichtheid
            rnd=(r=(r*1664525u+1013904223u),((r>>9)&0xFFFF)/65535.f);
            if(rnd<st.hat*dens&&(s%2==0||st.hat>0.6f))
                pushEv(2,42,55+(int)(rnd*45),s);
            // BASS: op halve tellen
            if(s%2==0&&rnd<0.85f){
                int deg=kBassSeq[s/2]+((r>>5)%2==0?0:0);
                int note=36+fRoot+scaleDeg(deg);
                if(st.oct>0.4f&&((r>>9)%4)==0)note+=12;
                pushEv(3,note,70+(int)(rnd*40),s);
            }
            // CHORD stab (alleen bij held-akkoord)
            if(fHeldN>=2&&st.sync>0.5f&&(s==4||s==12)&&rnd<0.5f){
                for(int c=0;c<fHeldN&&c<4;c++)pushEv(4,fHeld[c].note,60+(int)(rnd*30),s);
            }
        }
        // mens: timing+velocity jitter
        float tJ=fMens/100.f*0.15f, vJ=fMens/100.f*16.f;
        for(int e=0;e<fEvN;e++){
            Ev&ev=evs_[e];
            int s=ev.at;
            float spos=s+ (fSwing/100.f*0.5f)*(s%2==1?1.f:0.f)
                       +(((frand()&0xFFFF)/65535.f)-0.5f)*tJ;
            ev.at=(uint32_t)std::max<int>(0,(int)(spos*fStep16));
            int vel=ev.vel+(int)(((frand()&0xFFFF)/65535.f-0.5f)*vJ);
            ev.vel=(uint8_t)(vel<20?20:(vel>127?127:vel));
            ev.dur=(uint32_t)((ev.kind<=2?1:2)*(float)fStep16*0.9f);
        }
        for(int i=1;i<fEvN;i++){Ev t=evs_[i];int j=i-1;
            while(j>=0&&evs_[j].at>t.at){evs_[j+1]=evs_[j];j--;}evs_[j+1]=t;}
        fBar++;
    }
    void pushEv(int kind,int note,int vel,int step){
        if(fEvN>=96)return;Ev&e=evs_[fEvN++];
        e.kind=(uint8_t)kind;e.note=(uint8_t)note;e.vel=(uint8_t)vel;
        e.at=(uint32_t)step;e.dur=0;
    }

    // ---------- export ----------
    struct MidiRec{uint64_t fr;uint8_t note,ch,vel;uint16_t dur;};
    MidiRec rec_[4096];int recW_=0;uint64_t recN_=0;
    void writeMid(){
        if(recN_==0)return;
        const double sr=getSampleRate(),fpt=sr*60.0/(fBpm*480.0);
        smf::MidiFile mf;mf.setTicksPerQuarterNote(480);
        const uint64_t n=recN_<4096?recN_:4096;const int start=(recN_<=4096)?0:recW_;int seq=0;
        for(uint64_t k=0;k<n;k++){const MidiRec&r=rec_[(start+(int)k)%4096];
            smf::MidiEvent on(0x90|r.ch,r.note,r.vel);on.tick=(int)(r.fr/fpt);on.seq=seq++;
            smf::MidiEvent off(0x80|r.ch,r.note,0);off.tick=(int)((r.fr+r.dur)/fpt);off.seq=seq++;
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
        std::snprintf(path,sizeof(path),"%s/G2B-DJEBU-%u.mid",dir,(unsigned)(recN_%100000));
        mf.write(path);
    }

    void run(const float**, float** out, uint32_t frames, const MidiEvent* evs, uint32_t nev) override {
        const float sr=getSampleRate();
        if(fStep16<=0){fBpm=120.0;fStep16=sr*60.0/fBpm/4.0;}
        const TimePosition&tp=getTimePosition();
        bool hostPlay=false;
        if(tp.bbt.valid&&tp.bbt.beatsPerMinute>1.0){
            fBpm=tp.bbt.beatsPerMinute;fStep16=sr*60.0/fBpm/4.0;
            hostPlay=tp.playing;
        }
        for(uint32_t m=0;m<nev;m++){const MidiEvent&e=evs[m];
            if(e.size>=3){
                uint8_t st=e.data[0]&0xF0;
                if(st==0x90&&e.data[2]>0)noteOn(e.data[1],e.data[2]);
                else if(st==0x80||(st==0x90&&e.data[2]==0))noteOff(e.data[1]);
            }}

        memset(out[0],0,frames*sizeof(float));memset(out[1],0,frames*sizeof(float));
        float levelPeak=0.f;
        const bool bandOn=(fSpel>0.5f||hostPlay)&&fOn>0.5f;
        for(uint32_t i=0;i<frames;i++){
            if(bandOn&&fStep16>0){
                const int64_t barLen=(int64_t)(16*fStep16);
                int64_t rel=fAbs+i-fSpelStart;
                if(rel<0)rel=0;
                int64_t relBar=rel%barLen;
                // maat gewikkeld → nieuwe maat genereren (met actueel akkoord)
                if(relBar<fLastRelBar||(fEvI>=fEvN&&relBar<(int64_t)fStep16)){
                    if(fEvI>=fEvN||relBar<fLastRelBar){genBar();fEvI=0;}
                }
                fLastRelBar=relBar;
                fStepInBar=(int)(relBar/fStep16)%16;
                while(fEvI<fEvN&&(int64_t)evs_[fEvI].at<=relBar){fire(evs_[fEvI],i);fEvI++;}
            }
            // midi note-offs
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
    float fVolh=55.f,fMens=40.f,fSwing=20.f,fPreset=0.f,fOn=1.f,fKr=100.f,fSpel=0.f,fMidi=1.f;
    int64_t fAbs=0,fSpelStart=0;
    struct Held{uint8_t note,vel;int64_t at;};Held fHeld[16];int fHeldN=0;
    int fRoot=9,fMinor=0;float fConf=0.f;
    Ev evs_[96];int fEvN=0,fEvI=0;int fBar=0,fStepInBar=0;int64_t fLastRelBar=0;
    float fLevel=0.f;int fNoteNow=0;
    double fStep16=0,fBpm=120.0;
    uint32_t fSeed=12345u;
    Galaxy fGal;
    struct PendingOff{uint8_t note=0,ch=0;uint32_t at=0;};PendingOff offq_[128];int offw_=0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DjebuPlugin)
};

Plugin* createPlugin(){return new DjebuPlugin();}
END_NAMESPACE_DISTRHO
