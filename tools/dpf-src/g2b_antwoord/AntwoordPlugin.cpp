// G2B ANTWOOORD — call & response plugin (100% eigen code).
// Speel 4 tellen ("call") → hij antwoordt 4 tellen terug in jouw genre,
// toonsoort (eigen key-detect), met jouw velocity/swing-gevoel (RITME-SLOT).
// Fases: 0 IDLE · 1 LUISTERT (vangt call) · 2 ANTWOORDT · 3 VRAAG (hij stelt jou een vraag)
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

// genre-antwoord ritme/smaak: {dichtheid(0-1), triplet(0-1), syncopatie(0-1), octaafsprong(0-1), notenduur16}
struct GStijl{float dens,trip,sync,oct;int dur;};
static const GStijl kStijl[kNumPrograms]={
    {0.55f,0.8f,0.5f,0.7f,2},  // TRAP   triplet-runs
    {0.45f,0.4f,0.7f,0.5f,3},  // DRILL  glijdende syncope
    {0.40f,0.1f,0.6f,0.2f,2},  // BOOMBAP chopped/stutter
    {0.35f,0.2f,0.8f,0.3f,4},  // RNB    smooth syncope
    {0.60f,0.0f,0.3f,0.4f,4},  // HOUSE  rechte achtsten
    {0.50f,0.1f,0.6f,0.5f,3},  // DEMBOW dembow-lift
    {0.55f,0.1f,0.9f,0.4f,2},  // KAWINA poly-syncope
    {0.30f,0.1f,0.4f,0.2f,6},  // LOFI   spaarzaam geswingd
    {0.35f,0.2f,0.7f,0.3f,4},  // SOUL
    {0.50f,0.0f,0.3f,0.3f,4},  // POP    recht, clean
#ifdef G2B_GIO
    {0.22f,0.0f,0.2f,0.4f,8},  // AMBIENT spaarzaam, lange noten
    {0.55f,0.1f,0.7f,0.5f,2},  // SOCA    vrolijke syncope
    {0.45f,0.2f,0.8f,0.4f,3},  // DANCEHALL dembow-riddim
    {0.40f,0.1f,0.7f,0.3f,4}   // ZOUK    zacht Antilliaans
#endif
};

// majeur + mineur toonladders (semi vanaf grondtoon)
static const int kMaj[7]={0,2,4,5,7,9,11};
static const int kMin[7]={0,2,3,5,7,8,10};

struct CallN{uint8_t note,vel; float pos16; uint16_t dur16;}; // pos16 = 16e-noten sinds call-start
struct RespN{uint8_t note,vel; uint32_t at; uint32_t dur;};

class AntwoordPlugin : public Plugin {
public:
    enum Params{kHeft,kMens,kGeheugen,kDice,kPreset,kOn,kKracht,kLuister,kLoop,kVraag,
                kMidi,kExport,
                kKeyO,kConfO,kFaseO,kLevelO,kNoteO,kProgO,kNumParams};
    AntwoordPlugin():Plugin(kNumParams,0,0){applyPreset(0);}
protected:
    const char* getLabel() const override{return "G2B Antwoord";}
    const char* getDescription() const override{
        return "G2B call & response: speel 4 tellen, hij antwoordt in jouw stijl, toonsoort en gevoel.";}
    const char* getMaker() const override{return "G2B";}
    const char* getLicense() const override{return "G2B";}
    uint32_t getVersion() const override{return d_version(1,0,0);}
    int64_t getUniqueId() const override{
#ifdef G2B_GIO
        return d_cconst('G','2','A','g');
#else
        return d_cconst('G','2','A','n');
#endif
    }

    void initParameter(uint32_t i, Parameter& p) override {
        switch(i){
        case kHeft:    p.name="HEFTRIGHEID";p.symbol="heft";p.ranges.min=0;p.ranges.max=100;p.ranges.def=45;p.unit="%";break;
        case kMens:    p.name="MENS";p.symbol="mens";p.ranges.min=0;p.ranges.max=100;p.ranges.def=40;p.unit="%";break;
        case kGeheugen:p.name="GEHEUGEN";p.symbol="geheugen";p.ranges.min=0;p.ranges.max=100;p.ranges.def=50;p.unit="%";break;
        case kDice:    p.name="DICE";p.symbol="dice";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kPreset:  p.name="PRESET";p.symbol="preset";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=(double)(kNumPrograms-1);p.ranges.def=0;break;
        case kOn:      p.name="Aan";p.symbol="aan";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;break;
        case kKracht:  p.name="Volume";p.symbol="kracht";p.ranges.min=0;p.ranges.max=100;p.ranges.def=100;p.unit="%";break;
        case kLuister: p.name="LUISTER";p.symbol="luister";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kLoop:    p.name="LOOP";p.symbol="loop";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kVraag:   p.name="VRAAG";p.symbol="vraag";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kMidi:    p.name="MIDI UIT";p.symbol="midiuit";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;break;
        case kExport:  p.name="EXPORT";p.symbol="export";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kKeyO:    p.name="KEY";p.symbol="key";p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=23;p.ranges.def=9;break;
        case kConfO:   p.name="CONF";p.symbol="conf";p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=100;p.ranges.def=0;break;
        case kFaseO:   p.name="FASE";p.symbol="fase";p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=3;p.ranges.def=0;break;
        case kLevelO:  p.name="LEVEL";p.symbol="level";p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kNoteO:   p.name="NOTE";p.symbol="note";p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=128;p.ranges.def=0;break;
        case kProgO:   p.name="PROG";p.symbol="prog";p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=100;p.ranges.def=0;break;
        default:break;}
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){case kHeft:return fHeft;case kMens:return fMens;case kGeheugen:return fGeh;
                  case kDice:return 0.f;case kPreset:return fPreset;case kOn:return fOn;
                  case kKracht:return fKr;case kLuister:return 0.f;case kLoop:return fLoop;
                  case kVraag:return 0.f;case kMidi:return fMidi;case kExport:return 0.f;
                  case kKeyO:return (float)fKey+12.f*fMinor;case kConfO:return fConf;
                  case kFaseO:return (float)fFase;case kLevelO:return fLevel;
                  case kNoteO:return (float)fNoteNow;case kProgO:return fProg;}
        return 0.f;
    }
    void setParameterValue(uint32_t i,float v) override {
        switch(i){
        case kHeft:fHeft=v;break;case kMens:fMens=v;break;case kGeheugen:fGeh=v;break;
        case kDice:if(v>0.5f)rollDice();break;
        case kPreset:applyPreset((int)(v+0.5f));break;
        case kOn:fOn=v;break;case kKracht:fKr=v;break;
        case kLuister:if(v>0.5f)startLuister();break;
        case kLoop:fLoop=v;break;
        case kVraag:if(v>0.5f)stelVraag();break;
        case kMidi:fMidi=v;break;
        case kExport:if(v>0.5f)writeMid();break;
        default:break;}
    }
    void initState(uint32_t,String&,String&) override{}
    void setState(const char*,const char*) override{}

    void activate() override{fAbs=0;fFase=0;fFaseLeft=0;fCallN=0;fRespN=0;fRespI=0;
        fStep16=getSampleRate()*60.0/120.0/4.0;}

    // ---------- noten helpers ----------
    struct Voice{bool on=false;float freq=0,ph=0,amp=0,dec=0,lp=0;uint32_t till=0;int type=0;};
    Voice fV[16];
    void pluck(int note,int vel,float dur=0.35f,int type=0){
        for(auto&v:fV)if(!v.on){v.on=true;v.freq=440.f*powf(2.f,(note-69)/12.f);v.ph=0;
            v.amp=vel/127.f*0.8f;v.dec=expf(-1.f/(dur*getSampleRate()));v.lp=0;v.type=type;return;}
        fV[0].freq=440.f*powf(2.f,(note-69)/12.f);fV[0].ph=0;fV[0].amp=vel/127.f*0.8f;fV[0].on=true;fV[0].type=type;
    }
    // drum-antwoord stemmen: 0=kick,1=snare,2=hat
    void drumHit(int kind,int vel){
        for(auto&v:fV)if(!v.on){v.on=true;v.type=kind+10;v.amp=vel/127.f;
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
        if(fMidi>0.5f){emitMidi(0x90,(uint8_t)note,(uint8_t)vel,i);
            PendingOff&o=offq_[offw_];o.note=(uint8_t)note;o.at=i+dur;offw_=(offw_+1)%64;}
    }

    void applyPreset(int pi){if(pi<0)pi=0;if(pi>=kNumPrograms)pi=kNumPrograms-1;fPreset=(float)pi;}
    void rollDice(){fHeft=20.f+(frand()%800)/10.f;fMens=10.f+(frand()%700)/10.f;
                    fGeh=20.f+(frand()%700)/10.f;applyPreset(frand()%10);}
    uint32_t frand(){fSeed=fSeed*1664525u+1013904223u;return fSeed>>9;}

    // ---------- gespreks-fases ----------
    void startLuister(){fFase=1;fCallN=0;fCallStart=fAbs;fFaseLeft=(int64_t)(64.0*fStep16);} // 4 bars = 64 zestienden
    void stelVraag(){fFase=3;fCallN=0;fCallStart=fAbs;fRespStart=fAbs;fRespI=0;
        fFaseLeft=(int64_t)(64.0*fStep16);genVraag();}
    void endLuister(){detectKey();genAntwoord();fFase=2;fRespI=0;fRespStart=fAbs;
        fFaseLeft=(int64_t)(64.0*fStep16);}

    void noteIn(int note,int vel){
        if(fFase!=1)return;
        if(fCallN<256){CallN&c=call_[fCallN++];c.note=(uint8_t)note;c.vel=(uint8_t)vel;
            c.pos16=(float)((fAbs-fCallStart)/(double)fStep16);c.dur16=2;}
    }

    // ---------- key-detect (eigen, pitch-class histogram) ----------
    void detectKey(){
        float hist[12]={0};int n=0;
        for(int i=0;i<fCallN;i++){hist[call_[i].note%12]+=call_[i].vel/127.f;n++;}
        int low=0;for(int i=0;i<fCallN;i++)if(call_[i].note<50)low++;
        fDrum=(n>0&&low*2>n);
        if(n==0){fKey=9;fMinor=0;fConf=0;return;}
        float best=-1.f;int bk=0;bool bm=false;
        for(int k=0;k<12;k++){
            float sM=0,sN=0;
            for(int d=0;d<7;d++){sM+=hist[(k+kMaj[d])%12];sN+=hist[(k+kMin[d])%12];}
            if(sM>best){best=sM;bk=k;bm=false;}
            if(sN>best){best=sN;bk=k;bm=true;}
        }
        fKey=bk;fMinor=bm?1:0;
        float tot=0;for(int i=0;i<12;i++)tot+=hist[i];
        fConf=tot>0.5f?best/tot*100.f:0.f;if(fConf>100.f)fConf=100.f;
    }
    int scaleDeg(int deg)const{ // graad → midi-offset
        const int*sc=fMinor?kMin:kMaj;int oct=deg/7,d=deg%7;if(d<0){d+=7;oct--;}
        return sc[d]+12*oct;
    }

    // ---------- antwoord genereren ----------
    void genAntwoord(){
        fRespN=0;const float sr=getSampleRate();
        const GStijl&st=kStijl[(int)fPreset];
        // RITME-SLOT: swing + velocity uit de call meten
        float vSum=0,vN=0,swSum=0,swN=0;
        for(int i=0;i<fCallN;i++){vSum+=call_[i].vel;vN++;
            float frac=call_[i].pos16-floorf(call_[i].pos16+0.001f);
            if((int)floorf(call_[i].pos16)%2==1&&frac<0.5f){swSum+=frac;swN++;}}
        float callVel=vN>0?vSum/vN:80.f;
        float callSwing=swN>0?swSum/swN:0.f;
        // heftrigheid-mode: <33 EXACT · <66 VARIATIE · >=66 VRIJ
        int mode=fHeft<33.f?0:(fHeft<66.f?1:2);
        const int steps16=64; // 4 bars x 16
        int grid[64];for(int i=0;i<64;i++)grid[i]=-1;
        if(mode==0&&fCallN>0){ // EXACT: spiegel de call-ritmes, zelfde graden
            for(int i=0;i<fCallN;i++){int s=(int)(call_[i].pos16+0.5f)%steps16;
                grid[s]=call_[i].note;}}
        else{ // VARIATIE/VRIJ: nieuw patroon op genre-ritme
            uint32_t r=fSeed*2654435761u+97u;int prevDeg=0;
            // GEHEUGEN: bij voldoende geheugen eerdere call-graden hergebruiken
            for(int s=0;s<steps16;s++){
                float p=st.dens;
                if(s%16==0)p+=0.3f;
                if(st.sync>0.5f&&(s%8==6||s%8==7))p+=0.25f; // syncope-plekken
                float rnd=(r=(r*1664525u+1013904223u),((r>>9)&0xFFFF)/65535.f);
                if(rnd<p){
                    int deg;
                    if(mode==1&&fCallN>0&&(rnd*7)<4.f) // variatie: blijf dicht bij call
                        deg=noteDeg(call_[(int)(rnd*fCallN)%fCallN].note)+((r>>3)%3)-1;
                    else // vrij: random-walk in toonladder
                        deg=prevDeg+((int)((r>>5)%5))-2;
                    grid[s]=60+fKey+scaleDeg(deg);prevDeg=deg;
                    if(st.oct>0.4f&&((r>>9)%5)==0)grid[s]+=12;
                }
            }
        }
        // noten inplannen
        float velJ=fMens/100.f*14.f, tJ=fMens/100.f*0.12f;
        int last=-1;
        for(int s=0;s<steps16;s++)if(grid[s]>=0){
            RespN&rn=resp_[fRespN];if(fRespN>=256)break;
            float spos=s+callSwing*(s%2==1?1.f:0.f)+(((frand()&0xFFFF)/65535.f)-0.5f)*tJ;
            rn.at=(uint32_t)std::max<int>(0,(int)(spos*fStep16));
            rn.dur=(uint32_t)(st.dur*fStep16*0.9f);
            int vel=(int)(callVel+((frand()&0xFFFF)/65535.f-0.5f)*velJ);
            rn.vel=(uint8_t)(vel<20?20:(vel>127?127:vel));
            rn.note=(uint8_t)grid[s];last=s;fRespN++;
        }
        // sort op tijd
        for(int i=1;i<fRespN;i++){RespN t=resp_[i];int j=i-1;
            while(j>=0&&resp_[j].at>t.at){resp_[j+1]=resp_[j];j--;}resp_[j+1]=t;}
        (void)last;
    }
    int noteDeg(int midi)const{ // dichtstbijzijnde laddergraad
        int rel=((midi-fKey)%12+12)%12;const int*sc=fMinor?kMin:kMaj;
        int bd=99,bg=0;for(int d=0;d<7;d++){int dd=std::abs(rel-sc[d]);if(dd<bd){bd=dd;bg=d;}}
        return bg+(midi-60)/12*7;
    }
    void genVraag(){ // plugin stelt jou een vraag: genre-ritme met graden
        fRespN=0;const GStijl&st=kStijl[(int)fPreset];uint32_t r=99991u;
        for(int s=0;s<64;s++){
            float p=s%16==0?0.9f:st.dens*0.7f;
            if(((r=(r*1664525u+1013904223u),((r>>9)&0xFFFF))/65535.f)<p){
                RespN&rn=resp_[fRespN];if(fRespN>=256)break;
                rn.at=(uint32_t)(s*fStep16);rn.dur=(uint32_t)(st.dur*fStep16*0.9f);
                rn.vel=(uint8_t)(70+(r>>13)%40);rn.note=(uint8_t)(60+fKey+scaleDeg((r>>7)%7));
                fRespN++;
            }
        }
        for(int i=1;i<fRespN;i++){RespN t=resp_[i];int j=i-1;
            while(j>=0&&resp_[j].at>t.at){resp_[j+1]=resp_[j];j--;}resp_[j+1]=t;}
    }

    // ---------- export ----------
    struct MidiRec{uint64_t fr;uint8_t note,ch,vel;uint16_t dur;};
    MidiRec rec_[4096];int recW_=0;uint64_t recN_=0;
    void writeMid(){
        if(recN_==0){return;}
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
        std::snprintf(path,sizeof(path),"%s/G2B-ANTWOORD.mid",dir);
        mf.write(path);
    }

    void run(const float**, float** out, uint32_t frames, const MidiEvent* evs, uint32_t nev) override {
        const float sr=getSampleRate();
        if(fStep16<=0){fBpm=120.0;fStep16=sr*60.0/fBpm/4.0;bar16=16;}
        const TimePosition&tp=getTimePosition();
        if(tp.bbt.valid&&tp.bbt.beatsPerMinute>1.0){fBpm=tp.bbt.beatsPerMinute;fStep16=sr*60.0/fBpm/4.0;}

        for(uint32_t m=0;m<nev;m++){const MidiEvent&e=evs[m];
            if(e.size>=3&&(e.data[0]&0xF0)==0x90&&e.data[2]>0)noteIn(e.data[1],e.data[2]);}

        memset(out[0],0,frames*sizeof(float));memset(out[1],0,frames*sizeof(float));
        float levelPeak=0.f;
        for(uint32_t i=0;i<frames;i++){
            // fase-klok
            if(fFase>0&&--fFaseLeft<=0){
                if(fFase==1)endLuister();
                else if(fFase==2){if(fLoop>0.5f)startLuister();else fFase=0;}
                else if(fFase==3){fFase=1;fCallN=0;fCallStart=fAbs;fFaseLeft=(int64_t)(64.0*fStep16);}
            }
            fProg=(fFase>0&&fStep16>0)?100.f*(1.f-(float)fFaseLeft/(64.f*(float)fStep16)):0.f;
            // antwoord-noten afspelen
            while((fFase==2||fFase==3)&&fRespI<fRespN&&
                  (int64_t)(fAbs+i-fRespStart)>=(int64_t)resp_[fRespI].at){
                const RespN&rn=resp_[fRespI++];
                respNote(rn.note,rn.vel,i,rn.dur);}
            // midi note-offs
            for(int q=0;q<64;q++){PendingOff&o=offq_[q];
                if(o.note&&o.at<=i){emitMidi(0x80,o.note,0,i);o.note=0;}}
            // voices
            float o=0.f;
            for(auto&v:fV){
                if(!v.on)continue;
                float s;
                if(v.type==0){ // pluck
                    s=sinf(v.ph*2.f*M_PI)*0.7f+sinf(v.ph*4.f*M_PI)*0.2f;
                    v.lp+=0.3f*(s-v.lp);s=v.lp;
                }else if(v.type==10){ // kick: sine drop
                    s=sinf(v.ph*2.f*M_PI);v.freq*=0.9985f;
                }else if(v.type==11){ // snare: noise+tone
                    s=(((frand()&0xFFFF)/32768.f)-1.f)*0.6f+sinf(v.ph*2.f*M_PI)*0.4f;
                }else{ // hat
                    s=(((frand()&0xFFFF)/32768.f)-1.f)*0.5f;
                }
                o+=s*v.amp;v.amp*=v.dec;v.ph+=v.freq/sr;if(v.ph>1.f)v.ph-=1.f;
                if(v.amp<0.0005f)v.on=false;
            }
            o=tanhf(o*1.3f);
            out[0][i]=o;out[1][i]=o;
            float a=std::fabs(o);if(a>levelPeak)levelPeak=a;
        }
        fAbs+=frames;
        fLevel=fLevel*0.7f+levelPeak*0.3f;
        // drum-mode: als call vooral percussie is → drum-antwoorden (bij volgende luisterronde)
        g2b_post(nullptr,out,frames,sr,fOn,fKr,fGal,nullptr,false);
    }

private:
    float fHeft=45.f,fMens=40.f,fGeh=50.f,fPreset=0.f,fOn=1.f,fKr=100.f,fLoop=0.f,fMidi=1.f;
    int fFase=0;int64_t fFaseLeft=0,fCallStart=0,fRespStart=0,fAbs=0;
    CallN call_[256];int fCallN=0;
    RespN resp_[256];int fRespN=0,fRespI=0;
    int fKey=9,fMinor=0;float fConf=0.f;bool fDrum=false;
    float fLevel=0.f,fProg=0.f;int fNoteNow=0;
    double fStep16=0,fBpm=120.0;int bar16=16;
    uint32_t fSeed=12345u;
    Galaxy fGal;
    struct PendingOff{uint8_t note=0;uint32_t at=0;};PendingOff offq_[64];int offw_=0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AntwoordPlugin)
};

Plugin* createPlugin(){return new AntwoordPlugin();}
END_NAMESPACE_DISTRHO
