#include "drum_data.h"
#include "DistrhoPlugin.hpp"
#include "DrumsShared.h"
#include "../g2b_std.h"
#include <cstring>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>
#ifdef _WIN32
#  include <direct.h>
#else
#  include <sys/stat.h>
#endif

DrumsGridView* gDrumsView=nullptr;

START_NAMESPACE_DISTRHO

static const char* kPresetNames[17] = {"G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"};

// 16-step patronen per stijl: K=kick S=snare H=hat o=openhat P=perc X=alles
static const char* kPatterns[17][4] = {
    //        kick            snare           hat             perc
    {"K...K...K..K....","....S.......S...","H.H.H.H.H.H.H.H.","............P..."},   // G2B
    {"K..K..K...K..K..","....S.......S...","H.H.H.H.H.H.H.H.","P.....P.....P..."},   // DEMBOW
    {"K......K..K..K..","....S.......S...","H.H.HH.H.H.H.HH.","......P........."},   // DRILL
    {"K...K....K......","....S.......S...","H.H.H.H.H.H.H.H.","..P........P...."},   // RNB
    {"K...K...K...K...","....S.......S...","..H...H...H...H.","P.P.P.P.P.P.P.P."},   // HOUSE
    {"K..K....K.K..K..","....S.......S.S.","H.H.H.HoH.H.H.Ho","..P...P...P...P."},   // AFRO
    {"K........K......","....S...S....S..","H.H.H.H.H.HHHHHH","..........P....."},   // TRAP
    {"K...............","..S......S......","H..H..H..H..H.H.","P.....P........."},   // BOOM
    {"K..K....K..K.K..","....S.....S..S..","..H..H.H..H..H.H","P..P....P......."},   // UKG
    {"K..K..K...K...K.","....S..S........","H.H.HH.H.HH..H..","....P......P...."},   // JERSEY
    {"K...K.....K...K.","....S.S.....S...","H..H..H..H..H.H.","..P....P........"},   // BOUNCE
    {"K.....K.........","....S....S......","H..H....H..H....","P.......P......."},   // SOUL
    {"K....K....K.....","....S.......S...","H.H..H..H.H..H..","....P.........P."},   // LOFI
    {"K..K....K..K..K.","....S...S..S....","H.H..H.HH.H..H.H","...P...P........"},   // FUNK
    {"K.....K.....K...","..S..........S..","H..H..H..H..H...",".P....P....P...."},   // REGGAE
    {"K...K...K...K...","....S.......S...","H.H..H.H..H.H..H","P.......P......."},   // DANCE
    {"K..K.K..K..K.K..","....S.....S.S...","H.H.HH.H.H.HH.H.",".P..P..P..P..P."}    // KAWINA
};

// sintemper: DRIVE, SWING(0-0.5), LEVEL
static const float kPresetParams[17][3] = {
    {0.5f,0.10f,0.85f},   // G2B
    {0.55f,0.08f,0.9f},   // DEMBOW
    {0.65f,0.06f,0.85f},  // DRILL
    {0.4f,0.14f,0.8f},    // RNB
    {0.5f,0.02f,0.9f},    // HOUSE
    {0.5f,0.12f,0.85f},   // AFRO
    {0.7f,0.03f,0.88f},   // TRAP
    {0.6f,0.05f,0.85f},   // BOOM
    {0.45f,0.13f,0.85f},  // UKG
    {0.55f,0.10f,0.9f},   // JERSEY
    {0.5f,0.09f,0.85f},   // BOUNCE
    {0.35f,0.12f,0.8f},   // SOUL
    {0.4f,0.15f,0.78f},   // LOFI
    {0.55f,0.11f,0.85f},  // FUNK
    {0.45f,0.14f,0.82f},  // REGGAE
    {0.5f,0.07f,0.88f},   // DANCE
    {0.55f,0.11f,0.88f}   // KAWINA
};

struct DrumVoice {
    bool active=false; int type=0; float t=0,amp=0,f0=0,f1=0,dec=0;
    const int16_t* smp=nullptr; uint32_t slen=0; float spos=0,sstep=0;
};

// eigen .wav uit Music\G2B\DRUMS\ (RIFF PCM 8/16/24/32-bit of float, mono/stereo)
static bool g2bLoadWav(const char* path, std::vector<int16_t>& dst, float& ssr)
{
    FILE* f=std::fopen(path,"rb"); if(!f)return false;
    char hdr[12];
    if(std::fread(hdr,1,12,f)!=12||std::memcmp(hdr,"RIFF",4)||std::memcmp(hdr+8,"WAVE",4)){std::fclose(f);return false;}
    uint16_t fmt=0,ch=1,bits=16; uint32_t rate=44100; long dpos=-1,dlen=0;
    for(;;){
        char ch4[8];
        if(std::fread(ch4,1,8,f)!=8)break;
        const uint32_t sz=(uint8_t)ch4[4]|((uint8_t)ch4[5]<<8)|((uint8_t)ch4[6]<<16)|((uint8_t)ch4[7]<<24);
        if(!std::memcmp(ch4,"fmt ",4)){
            char fb[40];const uint32_t rd=sz<sizeof(fb)?sz:sizeof(fb);
            if(std::fread(fb,1,rd,f)!=rd)break;
            fmt=(uint8_t)fb[0]|((uint8_t)fb[1]<<8);
            ch =(uint8_t)fb[2]|((uint8_t)fb[3]<<8);
            rate=(uint8_t)fb[4]|((uint8_t)fb[5]<<8)|((uint8_t)fb[6]<<16)|((uint8_t)fb[7]<<24);
            bits=(uint8_t)fb[14]|((uint8_t)fb[15]<<8);
            if(sz>rd)std::fseek(f,sz-rd,SEEK_CUR);
        } else if(!std::memcmp(ch4,"data",4)){dpos=std::ftell(f);dlen=sz;std::fseek(f,sz+(sz&1),SEEK_CUR);}
        else std::fseek(f,sz+(sz&1),SEEK_CUR);
    }
    if(dpos<0||ch<1||ch>2||rate<4000||rate>192000){std::fclose(f);return false;}
    std::fseek(f,dpos,SEEK_SET);
    const uint32_t frames=dlen/(ch*(bits/8));
    const uint32_t n=frames>(uint32_t)(rate*10)?(uint32_t)(rate*10):frames; // max 10s
    dst.assign(n,0);
    if(fmt==1&&bits==16){ // int16
        for(uint32_t i=0;i<n;i++){int16_t v=0;std::fread(&v,2,1,f);
            if(ch==2){int16_t r=0;std::fread(&r,2,1,f);v=(int16_t)((v+r)/2);}
            dst[i]=v;}
    } else if(fmt==1&&bits==8){ // int8 unsigned
        for(uint32_t i=0;i<n;i++){int a=std::fgetc(f),b=a; if(ch==2)b=std::fgetc(f);dst[i]=(int16_t)(((a+b)/2-128)<<8);}
    } else if(fmt==1&&bits==24){
        for(uint32_t i=0;i<n;i++){unsigned char b[3]={0,0,0};std::fread(b,3,1,f);
            int32_t v=b[0]|(b[1]<<8)|(b[2]<<16);if(v&0x800000)v|=~0xFFFFFF;
            if(ch==2){std::fread(b,3,1,f);int32_t r=b[0]|(b[1]<<8)|(b[2]<<16);if(r&0x800000)r|=~0xFFFFFF;v=(v+r)/2;}
            dst[i]=(int16_t)(v>>8);}
    } else if(fmt==3&&bits==32){ // float32
        for(uint32_t i=0;i<n;i++){float v=0.f;std::fread(&v,4,1,f);
            if(ch==2){float r=0.f;std::fread(&r,4,1,f);v=(v+r)*0.5f;}
            v=v<-1.f?-1.f:(v>1.f?1.f:v);dst[i]=(int16_t)(v*32767.f);}
    } else {std::fclose(f);dst.clear();return false;}
    std::fclose(f); ssr=(float)rate; return n>64;
}

static float frand(unsigned& s){ s=s*1664525u+1013904223u; return (s>>8)*(1.f/16777216.f); }

class DrumsPlugin : public Plugin {
public:
    enum Params { kStijl, kDrive, kSwing, kLevel, kDice, kAuto, kPreset, kOn, kKracht, kGalaxy, kSfeer, kCell, kRun, kMine, kNumParams };
    DrumsPlugin() : Plugin(kNumParams,0,5) {
        gDrumsView=&gridView_;for(int i=0;i<kMaxV;i++)fV[i].active=false;
        loadMine(); // eigen .wav's uit Music\G2B\DRUMS automatisch gebruiken
    }
protected:
    const char* getLabel() const override { return "G2B Drums"; }
    const char* getDescription() const override { return "Drum-machine: jouw stijl-patronen, gesyncd met je DAW"; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','D','r'); }

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints=kParameterIsAutomatable;
        switch(i){
        case kStijl: p.name="PATROON";p.symbol="stijl";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=16;p.ranges.def=0;break;
        case kDrive: p.name="DRIVE";  p.symbol="drive"; p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.5f;p.unit="%";break;
        case kSwing: p.name="SWING";  p.symbol="swing"; p.ranges.min=0;p.ranges.max=0.5f;p.ranges.def=0.10f;p.unit="%";break;
        case kLevel: p.name="LEVEL";  p.symbol="level"; p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.85f;p.unit="%";break;
        case kDice:  p.name="DICE";   p.symbol="dice"; p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kAuto:  p.name="AUTO";   p.symbol="auto"; p.hints|=kParameterIsBoolean|kParameterIsInteger;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kPreset:p.name="PRESET"; p.symbol="preset";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=16;p.ranges.def=0;break;
        case kOn:    p.name="Aan";      p.symbol="aan";   p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;break;
        case kKracht:p.name="Kracht";   p.symbol="kracht";p.ranges.min=0;p.ranges.max=100;p.ranges.def=100;p.unit="%";break;
        case kGalaxy:p.name="DIMENTIES";p.symbol="dim";   p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=26;p.ranges.def=9;break;
        case kSfeer: p.name="Sfeer";    p.symbol="sfeer"; p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=2;p.ranges.def=1;break;
        case kCell: p.name="CELL";   p.symbol="cell"; p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=255;p.ranges.def=0;break;
        case kRun:  p.name="SPEEL";  p.symbol="speel"; p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;break;
        case kMine: p.name="MIJN";   p.symbol="mijn";  p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){case kStijl:return fStijl;case kDrive:return fDrive;case kSwing:return fSwing;case kLevel:return fLevel;case kDice:return fDice;case kAuto:return fAuto;case kPreset:return fPreset;case kOn:return fOn;case kKracht:return fKracht;case kGalaxy:return fGalaxyV;case kSfeer:return fSfeerV;case kCell:return 0.f;case kRun:return fRun;case kMine:return 0.f;}return 0.f;
    }
    void setParameterValue(uint32_t i, float v) override {
        if(i==kPreset){applyPreset((int)(v+0.5f));return;}
        switch(i){case kStijl:fStijl=v;break;case kDrive:fDrive=v;break;case kSwing:fSwing=v;break;case kLevel:fLevel=v;break;case kDice:fDice=v;if(v>0.5f){mutate(0);sendPat();}fDice=0.f;break;case kAuto:fAuto=v;break;
        case kOn:fOn=v;break;case kKracht:fKracht=v;break;
        case kGalaxy:fGalaxyV=v;fGal.setPos((int)(v+0.5f));break;
        case kSfeer:fSfeerV=v;fSf.setMode((int)(v+0.5f));break;
        case kCell:if(v>0.5f)handleCell((int)(v+0.5f));break;
        case kRun:fRun=v;break;
        case kMine:if(v>0.5f)loadMine();break;}
    }
    void initState(uint32_t index, String& name, String& def) override {
        if(index==0){name="scope";def="";}
        else if(index==1){name="step";def="";}
        else if(index==2){name="pat";def="";}
        else if(index==3){name="mine";def="";}
        else{name="presetnames";def="";for(int i=0;i<6;i++){def+=kPresetNames[i];if(i<5)def+=";";}}
    }
    void setState(const char* k, const char* v) override {(void)k;(void)v;}

    void trigger(int type, float ampMul=1.f, int hitIdx=0){
        for(auto& v:fV){
            if(!v.active){
                v.active=true;v.type=type;v.t=0;
                int ti=type<0?0:(type>4?4:type);
                if(type==5)ti=29;
                const int _st=((int)fStijl<0?0:((int)fStijl>16?16:(int)fStijl));
                if(type==2){ // hat → echte shaker op LOFI/SOUL/BOOM/JERSEY
                    static const char kHatMap[17]={-1,-1,-1,-1,-1,-1,-1,25,-1,27,-1,25,25,-1,-1,-1,-1};
                    const int hm=kHatMap[_st]; if(hm>=0)ti=hm;
                }
                if(ti==4){ // perc kiest per stijl: conga/darbuka/clap/cowbell/clave/bongo/agogo/tom
                    static const char kPercMap[17]={4,15,12,6,4,15,6,31,8,21,23,31,31,7,17,7,19};
                    ti=kPercMap[((int)fStijl<0?0:((int)fStijl>16?16:(int)fStijl))];
                }
                if(type==6)ti=(++fRoll&1)?13:12; // roll: afwisselend timpani L/R
                const int al=kDrumAlt[ti]; if(al>=0&&((++fRr)&1)) ti=al;
                v.smp=kDrum[ti].d;v.slen=kDrum[ti].len;v.spos=(type==6&&hitIdx>0)?12000:0;v.sstep=0;
                // eigen .wav (MIJN) vervangt de ingebouwde voor kick/snare/hat/perc
                {const int mi=(type==0)?0:(type==1)?1:(type==2)?2:(type==4)?3:-1;
                if(mi>=0&&fMineLen[mi]>0){v.smp=fMine[mi].data();v.slen=fMineLen[mi];v.spos=0;v.sstep=fMineSr[mi]/getSampleRate();}}
                switch(type){
                case 0:v.amp=1.f;v.f0=150;v.f1=45;v.dec=0.35f;break;   // kick
                case 1:v.amp=0.8f;v.f0=200;v.f1=180;v.dec=0.18f;break;  // snare
                case 2:v.amp=0.35f;v.f0=8000;v.f1=8000;v.dec=0.05f;break;// hat
                case 3:v.amp=0.4f;v.f0=8000;v.f1=8000;v.dec=0.25f;break; // openhat
                case 4:v.amp=0.5f;v.f0=900;v.f1=700;v.dec=0.12f;break;   // perc
                case 5:v.amp=0.45f;v.f0=8000;v.f1=8000;v.dec=1.0f;break;  // crash
                case 6:v.amp=0.8f;v.f0=200;v.f1=200;v.dec=1.2f;break;      // timpani-roll slag
                }
                if(type==6){ fRoll=fRoll*1664525u+1013904223u; v.sstep=(22050.f/getSampleRate())*(0.96f+((fRoll>>8)&63)/63.f*0.08f); }
                v.amp*=ampMul;
                return;
            }
        }
    }

    void run(const float** in, float** out, uint32_t frames, const MidiEvent*, uint32_t) override {
        const float sr=getSampleRate();
        if(fOn<0.5f){std::memset(out[0],0,frames*sizeof(float));std::memset(out[1],0,frames*sizeof(float));return;}
        // sequencer: interne klok op DAW-tempo (default 100 als geen pos)
        double bpm=100.0;
        const TimePosition& tp=getTimePosition();
        if(tp.playing && tp.bbt.valid && tp.bbt.beatsPerMinute>1.0) bpm=tp.bbt.beatsPerMinute;
        const double stepLen=60.0/bpm/4.0;          // 16e
        const int st=(int)fStijl<0?0:((int)fStijl>16?16:(int)fStijl);
        if(!fPatInit||st!=fLastStijl){for(int r=0;r<4;r++)for(int c=0;c<16;c++)fPat[r][c]=kPatterns[st][r][c];fPatInit=true;fLastStijl=st;sendPat();}
        for(uint32_t i=0;i<frames;i++){
            if(fRun>0.5f){
            fPhase+=1.0/sr;
            double stepDur=stepLen;
            int stepIdx=(int)fStep%16;
            // swing: oneven steps krijgen extra lengte
            if(stepIdx%2==1) stepDur*=1.0+fSwing*0.8;
            if(fPhase>=stepDur){
                fPhase=0; fStep++;
                int s=(int)fStep%16;
                const char* pat;
                if(fAuto>0.5f && s==0 && (fStep%64)==0){mutate(1);sendPat();}
                if(s==0 && (fStep%64)==0) trigger(5); // echte crash-cymbal elke 4 maten op de 1
                // timpani-roll: laatste maat van elke 4-maten cyclus, crescendo → landt op de crash (DRILL/TRAP/BOUNCE/DANCE)
                const int g64=(int)(fStep%64);
                if((st==2||st==6||st==10||st==15) && g64>=48){
                    const float ramp=(float)(g64-48)/15.f;
                    trigger(6,0.30f+0.85f*ramp,g64-48);
                }
                if(fPat[0][s]!='.')trigger(0);
                if(fPat[1][s]!='.')trigger(1);
                if(fPat[2][s]=='H')trigger(2); else if(fPat[2][s]=='o')trigger(3);
                if(fPat[3][s]!='.')trigger(4);
                char sb[8];snprintf(sb,sizeof(sb),"%d",s);setState("step",sb);
            }} else fPhase=0;
            // voices renderen
            float o=0.f;
            for(auto& v:fV){
                if(!v.active)continue;
                v.t+=1.0/sr;
                float env=expf(-v.t/v.dec*5.f);
                float f=v.f1+(v.f0-v.f1)*expf(-v.t*25.f);
                float ph=2.f*M_PI*f*v.t;
                float s=0;
                if(v.smp){ // echte opname (VCSL)
                    if(v.sstep<=0)v.sstep=22050.f/sr;
                    uint32_t i2=(uint32_t)v.spos;
                    if(i2>=v.slen-1){v.active=false;continue;}
                    float fr=v.spos-i2;
                    s=(v.smp[i2]+fr*(v.smp[i2+1]-v.smp[i2]))/32768.f;
                    v.spos+=v.sstep;
                    s*=env; // decay-envelop uit de stijl blijft werken
                }else{
                switch(v.type){
                case 0: s=sinf(ph)*env;break;
                case 1: s=(sinf(ph)*0.4f+(frand(fNfac)*2.f-1.f)*0.6f)*env;break;
                case 2: case 3: s=(frand(fNfac)*2.f-1.f)*env*0.7f;break;
                case 4: s=sinf(ph)*env+sinf(ph*2.7f)*env*0.3f;break;
                }}
                o+=s*v.amp;
                if(env<0.001)v.active=false;
            }
            o=tanhf(o*(1.f+fDrive*3.f))*fLevel;
            out[0][i]=o;out[1][i]=o;
            if(++fDecim>=3){fDecim=0;if(fScopeN<40)fScope[fScopeN++]=o;}
        }
        if(fScopeN>=40){
            char buf[256];int pos=0;
            for(uint32_t s=0;s<fScopeN&&pos<230;++s)pos+=snprintf(buf+pos,sizeof(buf)-pos,"%.3f;",fScope[s]);
            buf[pos]=0;setState("scope",buf);fScopeN=0;
        }
        g2b_post(nullptr,out,frames,sr,fOn,fKracht,fGal,&fSf,false);
    }
private:
    void handleCell(int c){ // 0..63 = vakje rij*16+stap aan/uit; 64..67 = rij-dice
        if(c>=64){int r=c-64; fRnd=fRnd*1664525u+1013904223u;
            for(int s=0;s<16;s++){fRnd=fRnd*1664525u+1013904223u;
                int dens=(r==2)?45:((r==0)?30:25); bool on=((fRnd>>8)%100)<dens;
                if(r==0&&s%4==0)on=true;
                if(r==1&&(s==4||s==12))on=true;
                if(r==2) fPat[r][s]=on?(((fRnd>>9)%5==0)?'o':'H'):'.';
                else fPat[r][s]=on?(r==0?'K':(r==1?'S':'P')):'.';}
            sendPat();return;}
        int r=c/16,s=c%16; if(r<0||r>3)return;
        char ch=(r==0)?'K':(r==1?'S':(r==2?'H':'P'));
        fPat[r][s]=(fPat[r][s]=='.')?ch:'.';
        sendPat();}
    void sendPat(){
        for(int r=0;r<4;r++)for(int c=0;c<16;c++)gridView_.pat[r][c]=fPat[r][c];
        char buf[70];int p=0;
        for(int r=0;r<4;r++){for(int c=0;c<16;c++)buf[p++]=fPat[r][c];buf[p++]=';';}
        buf[p]=0;setState("pat",buf);
    }
    void mutate(int mild){
        // alleen hat/perc wijzigen → kick/snare-groove blijft altijd staan
        fRnd=fRnd*1664525u+1013904223u;
        for(int n=0;n<(mild?3:8);n++){
            fRnd=fRnd*1664525u+1013904223u;
            const int r=2+((fRnd>>8)&1);
            fRnd=fRnd*1664525u+1013904223u; const int c=(fRnd>>8)%16;
            fRnd=fRnd*1664525u+1013904223u;
            const bool on=((fRnd>>8)%100)<40;
            if(r==2) fPat[r][c]=on?(((fRnd>>9)%5==0)?'o':'H'):'.';
            else fPat[r][c]=on?'P':'.';
        }
    }
    void loadMine(){
        const char* home=std::getenv("USERPROFILE"); if(!home||!*home)home=std::getenv("HOME");
        static const char* names[4]={"kick","snare","hat","perc"};
        char dir[520];
#ifdef _WIN32
        if(home&&*home)std::snprintf(dir,sizeof(dir),"%s\\Music\\G2B\\DRUMS",home); else std::snprintf(dir,sizeof(dir),".");
        _mkdir(dir);
#else
        if(home&&*home)std::snprintf(dir,sizeof(dir),"%s/Music/G2B/DRUMS",home); else std::snprintf(dir,sizeof(dir),".");
        if(home&&*home){char d2[560];std::snprintf(d2,sizeof(d2),"%s/Music",home);mkdir(d2,0755);std::snprintf(d2,sizeof(d2),"%s/Music/G2B",home);mkdir(d2,0755);}
        mkdir(dir,0755);
#endif
        char st[96];int p=0,got=0;
        for(int t=0;t<4;t++){
            char path[600];
#ifdef _WIN32
            std::snprintf(path,sizeof(path),"%s\\%s.wav",dir,names[t]);
#else
            std::snprintf(path,sizeof(path),"%s/%s.wav",dir,names[t]);
#endif
            float ssr=22050.f; std::vector<int16_t> tmp;
            if(g2bLoadWav(path,tmp,ssr)){
                fMine[t].swap(tmp);fMineLen[t]=(uint32_t)fMine[t].size();fMineSr[t]=ssr;got++;
                p+=std::snprintf(st+p,sizeof(st)-p,"%s ✓ ",names[t]);
            } else if(fMineLen[t]==0){
                p+=std::snprintf(st+p,sizeof(st)-p,"%s - ",names[t]);
            }
        }
        if(got==0)std::snprintf(st,sizeof(st),"geen .wav in Music\\G2B\\DRUMS");
        setState("mine",st);
    }
    void applyPreset(int i){
        if(i<0)i=0;if(i>16)i=16;fPreset=(float)i;
        setParameterValue(kStijl,(float)i);
        setParameterValue(kDrive,kPresetParams[i][0]);
        setParameterValue(kSwing,kPresetParams[i][1]);
        setParameterValue(kLevel,kPresetParams[i][2]);
    }
    float fStijl=0,fDrive=0.5f,fSwing=0.10f,fLevel=0.85f,fDice=0,fAuto=0,fPreset=0;
    uint32_t fRr=0;
    char fPat[4][17];DrumsGridView gridView_{};bool fPatInit=false;int fLastStijl=0;
    static const int kMaxV=24;
    DrumVoice fV[kMaxV];
    unsigned fRnd=24681357; unsigned fNfac=999; unsigned fRoll=0;
    float fOn=1.f,fKracht=100.f,fGalaxyV=9.f,fSfeerV=1.f,fRun=1.f;
    std::vector<int16_t> fMine[4]; uint32_t fMineLen[4]={0,0,0,0}; float fMineSr[4]={22050.f,22050.f,22050.f,22050.f};
    Galaxy fGal; Sfeer fSf;
    double fPhase=0; int64_t fStep=0;
    float fScope[40];uint32_t fScopeN=0;int fDecim=0;
    unsigned fNfu=7;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumsPlugin)
};
Plugin* createPlugin(){return new DrumsPlugin();}
END_NAMESPACE_DISTRHO
