/*
 * G2B TREIN — loop → heel nummer (100% eigen code, geen AI/cloud).
 * Druk LUISTER → speel je 8-bars loop erdoor → hij splitst het in
 * kick/snare/hat/melodie en genereert je loop in je genre.
 * ↓ARR schrijft een HEEL ARRANGEMENT als 1 .mid: INTRO · BUILD · DROP ·
 * BREAK · OUTRO als markers + sectie-gefilterde noten + MIX-kaart (.txt).
 * Speelt alleen mee als de DAW-transport PLAY is; volgt de host-BPM.
 * Knoppen: HEFT (dichtheid) · MENS (humanize) · LENGTE (bars) · VAR (variatie)
 */
#include "DistrhoPlugin.hpp"
#include "vendor/MidiFile.h"
#include <cstring>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <sys/stat.h>
#include <ctime>

START_NAMESPACE_DISTRHO

#ifdef G2B_GIO
static const int kNumProgs=14;
#else
static const int kNumProgs=10;
#endif
static const char* kProgNames[14]={
    "TRAP","DRILL","BOOMBAP","RNB","HOUSE","DEMBOW","KAWINA",
    "LOFI","SOUL","POP","AMBIENT","SOCA","DANCEHALL","ZOUK"};

// genre-drumtemplates 16 steps (bit i = stap i): kick, snare, hat
static const uint16_t kGKick[14]={0x8424,0x9084,0x8424,0x8421,0x1111,0x8424,0x9494,0x8424,0x8421,0x8424,0x8421,0x9090,0x8424,0x8421};
static const uint16_t kGSnar[14]={0x1010,0x1010,0x1010,0x0010,0x1010,0x1818,0x1090,0x1010,0x1010,0x1010,0x1010,0x1010,0x1818,0x0010};
static const uint16_t kGHats[14]={0xAAAA,0xEEBE,0xA2AA,0xAAAA,0xAAAA,0xAAAA,0xB2AA,0x2828,0xA2A2,0xAAAA,0xAAAA,0xAEAE,0xAAAA,0xAA8A};

class TreinPlugin : public Plugin {
public:
    enum Params { kHeft=0,kMens,kLen,kVar,kDice,kPreset,kOn,kLuister,kGen,kMidi,kRefine,kLevel,
                  kOKlas,kOKey,kOBpm,kOStat,kOHits,
                  kOHeft,kOMens,kOLen,kOVar, kNumParams };
    TreinPlugin() : Plugin(kNumParams,0,0) {}
protected:
    const char* getLabel() const override { return "G2B Trein"; }
    const char* getDescription() const override {
        return "Van je loop naar een heel nummer: luistert en schrijft je arrangement-MIDI (intro tot outro)."; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override {
#ifdef G2B_GIO
        return d_cconst('G','2','T','g');
#else
        return d_cconst('G','2','T','u');
#endif
    }

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints=kParameterIsAutomatable;
        switch(i){
        case kHeft:  p.name="HEFT";     p.symbol="heft";  p.ranges.min=0;p.ranges.max=100;p.ranges.def=50;p.unit="%";break;
        case kMens:  p.name="MENS";     p.symbol="mens";  p.ranges.min=0;p.ranges.max=100;p.ranges.def=30;p.unit="%";break;
        case kLen:   p.name="LENGTE";   p.symbol="lengte";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=3;p.ranges.def=1;break;
        case kVar:   p.name="VAR";      p.symbol="var";   p.ranges.min=0;p.ranges.max=100;p.ranges.def=30;p.unit="%";break;
        case kDice:  p.name="DICE";     p.symbol="dice";  p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;break;
        case kPreset:p.name="GENRE";    p.symbol="genre"; p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=(float)(kNumProgs-1);break;
        case kOn:    p.name="Aan";      p.symbol="aan";   p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;break;
        case kLuister:p.name="LUISTER"; p.symbol="luister";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;break;
        case kGen:   p.name="GEN";      p.symbol="gen";   p.hints|=kParameterIsBoolean|kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;break;
        case kMidi:  p.name="MID";      p.symbol="mid";   p.hints|=kParameterIsBoolean|kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;break;
        case kRefine:p.name="REFINE";   p.symbol="refine";p.hints|=kParameterIsBoolean|kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;break;
        case kLevel: p.name="LEVEL";    p.symbol="level"; p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=1;break;
        case kOKlas: p.name="OKLAS";    p.symbol="oklas"; p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=4;break;
        case kOKey:  p.name="OKEY";     p.symbol="okey";  p.hints=kParameterIsOutput;p.ranges.min=-1;p.ranges.max=23;p.ranges.def=-1;break;
        case kOBpm:  p.name="OBPM";     p.symbol="obpm";  p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=240;break;
        case kOStat: p.name="OSTAT";    p.symbol="ostat"; p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=5;break;
        case kOHits: p.name="OHITS";    p.symbol="ohits"; p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=999;break;
        case kOHeft: p.name="OHEFT";    p.symbol="oheft"; p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=100;p.ranges.def=50;break;
        case kOMens: p.name="OMENS";    p.symbol="omens"; p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=100;p.ranges.def=30;break;
        case kOLen:  p.name="OLEN";     p.symbol="olen";  p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=3;p.ranges.def=1;break;
        case kOVar:  p.name="OVAR";     p.symbol="ovar";  p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=100;p.ranges.def=30;break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){case kHeft:return fHeft;case kMens:return fMens;case kLen:return fLen;
        case kVar:return fVar;case kDice:return 0;case kPreset:return fPreset;case kOn:return fOn;
        case kLuister:return fLuist;case kGen:return 0;case kMidi:return 0;case kRefine:return 0;
        case kLevel:return fLevel;
        case kOKlas:return fKlas;case kOKey:return fKey;case kOBpm:return fBpm;case kOStat:return fStat;
        case kOHits:return fHits;case kOHeft:return fHeft;case kOMens:return fMens;
        case kOLen:return fLen;case kOVar:return fVar;}return 0;
    }
    void setParameterValue(uint32_t i, float v) override {
        switch(i){
        case kHeft:fHeft=v;break;case kMens:fMens=v;break;case kLen:fLen=v;break;case kVar:fVar=v;break;
        case kDice:if(v>0.5f)rollDice();break;
        case kPreset:fPreset=std::max(0.f,std::min(v,(float)(kNumProgs-1)));break;
        case kOn:fOn=v;break;
        case kLuister:
            if(v>0.5f&&fLuist<0.5f){fRecN=0;fStat=1;}
            if(v<0.5f&&fLuist>0.5f){fWantAna=1;}
            fLuist=v;break;
        case kGen:if(v>0.5f)fWantAna=1;break;
        case kMidi:if(v>0.5f)fWantMid=1;break;
        case kRefine:if(v>0.5f)fWantRefine=1;break;
        }
    }
    void initState(uint32_t,String&,String&) override {}
    void setState(const char*,const char*) override {}

    void activate() override {
        fRecN=0;fStat=0;fLoopPos=0;fBpm=120;fStep16=getSampleRate()*15.0/120.0;
        fLevelBlk=0;fNIdx=0;std::memset(fOff,0,sizeof(fOff));
    }

    struct N{uint32_t at,dur;uint8_t note,vel,ch;};

    void rollDice(){
        fRnd=fRnd*1664525u+1013904223u;
        fHeft=20+(fRnd>>8)%70;fVar=10+(fRnd>>14)%60;
    }

    void analyze(){
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
            static const int maj[7]={0,2,4,5,7,9,11},min[7]={0,2,3,5,7,8,10};
            int bs=-1,bk=-1;
            for(int root=0;root<12;root++)for(int mn=0;mn<2;mn++){
                const int*sc=mn?min:maj;int s=0;
                for(int g=0;g<7;g++)s+=hist[(root+sc[g])%12];
                if(s>bs){bs=s;bk=root+mn*12;}
            }
            fKey=(float)bk;
        }
        if(ons.size()<=2&&tonalFrac<0.25f)fKlas=3;
        else if(tonalFrac>0.4f&&ons.size()<8)fKlas=2;
        else if(tonalFrac>0.4f)fKlas=4;
        else fKlas=1;
        fLastOns=ons;fLastCls=onsCls;fLastPit=pitches;
        generate(ons,onsCls,pitches,false);
        fStat=3;
    }

    void generate(const std::vector<int>&ons,const std::vector<int>&cls,
                  const std::vector<float>&pitches,bool refine){
        fNotes.clear();
        const int barsL[4]={2,4,8,16};int bars=barsL[std::max(0,std::min(3,(int)(fLen+0.5f)))];
        const int steps=bars*16;
        const int gi=std::max(0,std::min(kNumProgs-1,(int)(fPreset+0.5f)));
        const float dens=fHeft/100.f,var=fVar/100.f,mens=fMens/100.f;
        uint32_t rng=fRnd;
        if(refine)rng=rng*1664525u+1013904223u; // andere variatie bij REFINE
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
            if(fKlas!=3){ // echte onsets meenemen
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
            static const int majS[7]={0,2,4,5,7,9,11},minS[7]={0,2,3,5,7,8,10};
            const int*sc=minor?minS:majS;
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

    // ↓ARR: schrijf HEEL ARRANGEMENT als 1 .mid + MIX-kaart.txt
    void writeMid(){
        if(fNotes.empty()||fLoopLen==0||fStep16<=0)return;
        const double fpt=fStep16/120.0; // samples per tick (480tpq)
        const int loopTicks=(int)(fLoopLen/fpt+0.5);
        const int bars=loopTicks/1920; // 16 stappen * 120 ticks
        if(bars<1)return;
        const char*home=std::getenv("USERPROFILE");if(!home||!*home)home=std::getenv("HOME");
        char dir[520];
        std::snprintf(dir,sizeof(dir),"%s/Music/G2B",home?home:".");
        #ifdef _WIN32
        {std::string m=std::string(home?home:".")+"/Music";mkdir(m.c_str());mkdir(dir);}
        #else
        {std::string m=std::string(home?home:".")+"/Music";mkdir(m.c_str(),0755);mkdir(dir,0755);}
        #endif
        // secties in loop-eenheden: INTRO 1 · BUILD 1 · DROP 2 · BREAK 1 · OUTRO 1
        static const char* kSec[5]={"INTRO","BUILD","DROP","BREAK","OUTRO"};
        static const int kLen[5]={1,1,2,1,1};
        int start[5]={0},tot=0;
        for(int i=0;i<5;i++){start[i]=tot;tot+=kLen[i];}
        smf::MidiFile mf;mf.addTracks(2);mf.setTicksPerQuarterNote(480); // default track + 2 = 3 totaal
        mf.addTempo(0,0,fBpm>10?fBpm:120);
        smf::MidiEvent tn;tn.tick=0;
        tn.makeTrackName("SECTIES");mf[0].push_back(tn);
        tn.makeTrackName("DRUMS");mf[1].push_back(tn);
        tn.makeTrackName("MELODIE+BAS");mf[2].push_back(tn);
        for(int i=0;i<5;i++)mf.addMarker(0,start[i]*loopTicks,kSec[i]);
        int seq=0;
        auto emit=[&](int tr,int tick,const N&n,int vel){
            vel=std::max(1,std::min(127,vel));
            smf::MidiEvent on(0x90|n.ch,n.note,vel);on.tick=tick;on.seq=seq++;
            smf::MidiEvent off(0x80|n.ch,n.note,0);off.tick=tick+(int)(n.dur/fpt);off.seq=seq++;
            mf[tr].push_back(on);mf[tr].push_back(off);
        };
        // filter per sectie: geef true als noot erin hoort + velocity-schaal
        auto secOK=[&](int sec,const N&n,int p)->int{
            bool drum=(n.ch==0),bass=(n.ch==2);
            int step=(p/120)%16,bar=(p/1920);
            switch(sec){
            case 0: // INTRO: alleen kicks op tel 1&3 + elke 2e melodie-noot zacht
                if(drum)return (n.note==36&&step%8==0)?n.vel*65/100:0;
                if(bass)return 0;
                return n.vel*60/100;
            case 1: // BUILD: alles, snare pas laatste bar, fill aan het eind
                if(drum){if(n.note==38&&bar<bars-1)return 0;return n.vel*85/100;}
                return n.vel*80/100;
            case 2: // DROP: alles vol
                return n.vel;
            case 3: // BREAK: alleen kick op bar-start, melodie zacht, geen bas
                if(drum)return (n.note==36&&step==0)?n.vel*70/100:0;
                if(bass)return 0;
                return n.vel*65/100;
            case 4: // OUTRO: kick op bar-start afnemend, alleen eerste helft melodie
                if(drum)return (n.note==36&&step==0)?std::max(20,n.vel*(70-bar*15)/100):0;
                if(bass)return 0;
                return bar<bars/2?n.vel*50/100:0;
            }
            return 0;
        };
        for(const N&n:fNotes){
            int p=(int)(n.at/fpt)%loopTicks; if(p<0)p+=loopTicks;
            int tr=(n.ch==0)?1:2;
            for(int s=0;s<5;s++)for(int rep=0;rep<kLen[s];rep++){
                int v=secOK(s,n,p);if(!v)continue;
                emit(tr,(start[s]+rep)*loopTicks+p,n,v);
            }
        }
        // BUILD-fill: extra snare-roll laatste 4 stappen voor de drop
        for(int i=0;i<8;i++){
            smf::MidiEvent on(0x90,38,60+i*8);on.tick=start[2]*loopTicks-4*120+i*60;on.seq=seq++;
            smf::MidiEvent off(0x80,38,0);off.tick=on.tick+50;off.seq=seq++;
            mf[1].push_back(on);mf[1].push_back(off);
        }
        mf.sortTracks();
        // unieke naam per arrangement: G2B-TREIN-ARR_20261103-214530.mid
        std::time_t tt=std::time(nullptr);std::tm*lt=std::localtime(&tt);
        char stamp[40];std::snprintf(stamp,sizeof(stamp),"%04d%02d%02d-%02d%02d%02d",
            lt->tm_year+1900,lt->tm_mon+1,lt->tm_mday,lt->tm_hour,lt->tm_min,lt->tm_sec);
        char path[640];
        std::snprintf(path,sizeof(path),"%s/G2B-TREIN-ARR_%s.mid",dir,stamp);
        mf.write(path);
        // MIX-kaart: welke secties waar + wat elke sectie mist/krijgt
        char kp[640];std::snprintf(kp,sizeof(kp),"%s/G2B-TREIN-KAART.txt",dir);
        FILE*f=std::fopen(kp,"w");
        if(f){
            std::fprintf(f,"G2B TREIN — MIX-KAART van je arrangement\n");
            std::fprintf(f,"BPM %.0f · loop %d bars · totaal %d bars\n\n",fBpm,bars,tot*bars);
            int b0=0;const char*adv[5]={
                "rustig: alleen kick + zachte melodie — houd het leeg",
                "opbouw: volledige drums vanaf laatste bar + fill naar de drop",
                "het moment: alles vol — jouw loop knalt hier",
                "ademruimte: melodie zacht, drums weg — spanning voor ronde 2",
                "afloop: kick verdwijnt bar voor bar — laat uitsterven"};
            for(int i=0;i<5;i++){std::fprintf(f,"bars %3d-%3d  %-6s  %s\n",b0+1,b0+kLen[i]*bars,kSec[i],adv[i]);b0+=kLen[i]*bars;}
            std::fprintf(f,"\nTip: elke sectie kan op een eigen spoor — kopieer de .mid en snij hem in Reason op de markers.\n");
            std::fclose(f);
        }
        fStat=4;
        #ifdef _WIN32
        char cmd[700];std::snprintf(cmd,sizeof(cmd),"explorer \"%s\"",dir);std::system(cmd);
        #else
        char cmd[700];std::snprintf(cmd,sizeof(cmd),"xdg-open \"%s\" >/dev/null 2>&1 &",dir);std::system(cmd);
        #endif
    }

    void run(const float** in, float** out, uint32_t frames, const MidiEvent*, uint32_t) override {
        const float sr=getSampleRate();
        const TimePosition&tp=getTimePosition();
        bool playing=true;
        if(tp.bbt.valid){
            if(tp.bbt.beatsPerMinute>1.0){fBpm=tp.bbt.beatsPerMinute;fBpmFromHost=1;fStep16=sr*60.f/fBpm/4.f;}
            playing=tp.playing;
        }
        if(in[0]!=out[0])std::memcpy(out[0],in[0],frames*sizeof(float));
        if(in[1]!=out[1])std::memcpy(out[1],in[1],frames*sizeof(float));
        if(fLuist>0.5f&&fStat==1){
            for(uint32_t i=0;i<frames&&fRecN<kRecMax;i++)
                fRec[fRecN++]=(in[0][i]+in[1][i])*0.5f;
            if(fRecN>=kRecMax){fLuist=0;fWantAna=1;}
        }
        if(fWantAna){fWantAna=0;analyze();}
        if(fWantRefine){fWantRefine=0;
            if(!fLastOns.empty())generate(fLastOns,fLastCls,fLastPit,true);}
        if(fWantMid){fWantMid=0;writeMid();}
        // midi-loop alleen meespelen als transport play (of geen transport-info)
        if(fOn>0.5f&&playing&&fStat==3&&fLoopLen>0){
            uint32_t base=fLoopPos;
            while(fNIdx<fNotes.size()&&fNotes[fNIdx].at<base+frames){
                const N&n=fNotes[fNIdx];
                int fr=(int)(n.at-base);if(fr<0)fr=0;if(fr>=(int)frames)fr=frames-1;
                MidiEvent e{};e.frame=(uint32_t)fr;e.size=3;e.data[0]=0x90|n.ch;e.data[1]=n.note;e.data[2]=n.vel;
                writeMidiEvent(e);
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
    }

private:
    static const uint32_t kRecMax=8*48000;
    float fRec[kRecMax];uint32_t fRecN=0;
    float fHeft=50,fMens=30,fLen=1,fVar=30,fPreset=0,fOn=1,fLuist=0;
    float fKlas=0,fKey=-1,fBpm=120,fStat=0,fHits=0,fLevel=0,fLevelBlk=0;
    int fWantAna=0,fWantMid=0,fWantRefine=0,fBpmFromHost=0;
    double fStep16=0;uint32_t fLoopPos=0,fLoopLen=0,fNIdx=0,fRnd=12345;
    std::vector<N> fNotes;
    std::vector<int> fLastOns,fLastCls;std::vector<float> fLastPit;
    struct O{uint32_t at;uint8_t note,ch;};O fOff[512];int fOffW=0;
};

Plugin* createPlugin(){return new TreinPlugin();}
END_NAMESPACE_DISTRHO
