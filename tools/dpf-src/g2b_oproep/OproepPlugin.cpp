/*
 * G2B OPROEP — stem/geluid → melodie-MIDI + bespeelbaar instrument (100% eigen code).
 * MELODIE-modus: neurie/zing een lijn → pitch-track → strakke melodie in je toonsoort
 *   → .mid + live MIDI-uit (volgt transport).
 * INSTRUMENT-modus: zing 1 toon / speel een one-shot erdoor → wordt meteen een
 *   bespeelbaar instrument op je MIDI-keyboard (varispeed-resample + loop-fades).
 * Pills = toonladder (quantiseer valse noten mee). Knoppen:
 *   GEVOEL (quantize-sterkte) · GRID (8/16/32) · OCTAAF · VAR
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

START_NAMESPACE_DISTRHO

#ifdef G2B_GIO
static const int kNumProgs=9;
#else
static const int kNumProgs=5;
#endif
static const char* kProgNames[9]={
    "MAJEUR","MINEUR","PENTA","BLUES","CHROMA","DORIAN","PHRYG","MIXO","HARM"};
// intervallen per ladder (chroma = alles)
static const int kScales[9][7]={
    {0,2,4,5,7,9,11},{0,2,3,5,7,8,10},{0,2,4,7,9,0,0},
    {0,3,5,6,7,10,0},{0,1,2,3,4,5,6},{0,2,3,5,7,9,10},
    {0,1,3,5,7,8,10},{0,2,4,5,7,9,10},{0,2,3,5,7,8,11}};
static const int kScaleLen[9]={7,7,5,6,7,7,7,7,7};

class OproepPlugin : public Plugin {
public:
    enum Params { kGevoel=0,kGrid,kOct,kVar,kDice,kScale,kOn,kLuister,kGen,
                  kMidi,kRefine,kLevel,
                  kOKlas,kOKey,kOBpm,kOStat,kONotes,
                  kOGevoel,kOGrid,kOOct,kOVar,kMode, kNumParams };
    OproepPlugin() : Plugin(kNumParams,0,0) {}
protected:
    const char* getLabel() const override { return "G2B Oproep"; }
    const char* getDescription() const override {
        return "Neurie een melodie → MIDI-loop · zing 1 toon → bespeelbaar instrument."; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override {
#ifdef G2B_GIO
        return d_cconst('G','2','O','g');
#else
        return d_cconst('G','2','O','p');
#endif
    }

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints=kParameterIsAutomatable;
        switch(i){
        case kGevoel:p.name="GEVOEL"; p.symbol="gevoel";p.ranges.min=0;p.ranges.max=100;p.ranges.def=70;p.unit="%";break;
        case kGrid:  p.name="GRID";   p.symbol="grid";  p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=2;p.ranges.def=1;break;
        case kOct:   p.name="OCTAAF"; p.symbol="octaaf";p.hints|=kParameterIsInteger;p.ranges.min=-2;p.ranges.max=2;p.ranges.def=0;break;
        case kVar:   p.name="VAR";    p.symbol="var";   p.ranges.min=0;p.ranges.max=100;p.ranges.def=0;p.unit="%";break;
        case kDice:  p.name="DICE";   p.symbol="dice";  p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;break;
        case kScale: p.name="SCHAAL"; p.symbol="schaal";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=(float)(kNumProgs-1);break;
        case kMode:  p.name="MODUS";  p.symbol="modus"; p.hints|=kParameterIsInteger|kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;break;
        case kOn:    p.name="Aan";    p.symbol="aan";   p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;break;
        case kLuister:p.name="LUISTER";p.symbol="luister";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;break;
        case kGen:   p.name="GEN";    p.symbol="gen";   p.hints|=kParameterIsBoolean|kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;break;
        case kMidi:  p.name="MID";    p.symbol="mid";   p.hints|=kParameterIsBoolean|kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;break;
        case kRefine:p.name="REFINE"; p.symbol="refine";p.hints|=kParameterIsBoolean|kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;break;
        case kLevel: p.name="LEVEL";  p.symbol="level"; p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=1;break;
        case kOKey:  p.name="OKEY";   p.symbol="okey";  p.hints=kParameterIsOutput;p.ranges.min=-1;p.ranges.max=23;p.ranges.def=-1;break;
        case kOKlas: p.name="OKLAS";  p.symbol="oklas"; p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=4;break;
        case kOBpm:  p.name="OBPM";   p.symbol="obpm";  p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=240;break;
        case kOStat: p.name="OSTAT";  p.symbol="ostat"; p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=5;break;
        case kONotes:p.name="ONOTES"; p.symbol="onotes";p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=999;break;
        case kOGevoel:p.name="OGEVOEL";p.symbol="ogevoel";p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=100;p.ranges.def=70;break;
        case kOGrid: p.name="OGRID";  p.symbol="ogrid"; p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=2;p.ranges.def=1;break;
        case kOOct:  p.name="OOCT";   p.symbol="ooct";  p.hints=kParameterIsOutput;p.ranges.min=-2;p.ranges.max=2;p.ranges.def=0;break;
        case kOVar:  p.name="OVAR";   p.symbol="ovar";  p.hints=kParameterIsOutput;p.ranges.min=0;p.ranges.max=100;p.ranges.def=0;break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){case kGevoel:return fGevoel;case kGrid:return fGrid;case kOct:return fOct;
        case kVar:return fVar;case kDice:return 0;case kScale:return fScale;case kMode:return fMode;
        case kOn:return fOn;case kLuister:return fLuist;case kGen:return 0;case kMidi:return 0;
        case kRefine:return 0;case kLevel:return fLevel;
        case kOKey:return fKey;case kOBpm:return fBpm;case kOStat:return fStat;
        case kONotes:return fNN;case kOKlas:return fKlas;
        case kOGevoel:return fGevoel;case kOGrid:return fGrid;case kOOct:return fOct;case kOVar:return fVar;}return 0;
    }
    void setParameterValue(uint32_t i, float v) override {
        switch(i){
        case kGevoel:fGevoel=v;break;case kGrid:fGrid=v;break;case kOct:fOct=v;break;case kVar:fVar=v;break;
        case kDice:if(v>0.5f){fRnd=fRnd*1664525u+1013904223u;fGevoel=40+(fRnd>>8)%60;fVar=(fRnd>>14)%60;}break;
        case kScale:fScale=std::max(0.f,std::min(v,(float)(kNumProgs-1)));break;
        case kMode:fMode=v;break;
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
        fLevelBlk=0;fNIdx=0;fSmpN=0;fSmpHz=0;std::memset(fOff,0,sizeof(fOff));
        for(auto&v:fV)v.act=0;
    }

    struct N{uint32_t at,dur;uint8_t note,vel,ch;};

    // ---- pitch-track 1 frame (autocorrelatie) → hz of 0 ----
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

    void analyze(){
        fStat=2;
        const float sr=getSampleRate();int n=(int)fRecN;
        if(n<4800){fStat=0;fKlas=0;return;}
        if(fMode>0.5f){analyzeInstrument(sr,n);return;}
        // ---- MELODIE: pitch-track per 256-hop → note-segmenten ----
        const int HOP=256,FR=1024;int nf=(n-FR)/HOP;
        if(nf<8){fStat=0;fKlas=0;return;}
        std::vector<float> hz(nf,0);
        int hist[12]={0};
        for(int f=0;f<nf;f++){
            float h=pitchOf(fRec+f*HOP,FR,sr);hz[f]=h;
            if(h>0){int pc=(int)(69.5f+12.f*log2f(h/440.f))%12;if(pc<0)pc+=12;hist[pc]++;}
        }
        // toonsoort
        fKey=-1;{
            static const int maj[7]={0,2,4,5,7,9,11},min[7]={0,2,3,5,7,8,10};
            int bs=-1,bk=-1;
            for(int root=0;root<12;root++)for(int mn=0;mn<2;mn++){
                const int*sc=mn?min:maj;int s=0;
                for(int g=0;g<7;g++)s+=hist[(root+sc[g])%12];
                if(s>bs){bs=s;bk=root+mn*12;}
            }
            fKey=(float)bk;
        }
        // noot-segmentatie: zelfde midi ±1 & gap<3 frames samenvoegen
        fNotes.clear();
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
        // naar schaal corrigeren + octaaf-transpose + grid-quantize
        int scale=(int)(fScale+0.5f);
        int root=fKey>=0?(int)fKey%12:0;
        bool minor=fKey>=12;
        const int*sc=kScales[scale];int sl=kScaleLen[scale];
        float q=fGevoel/100.f;
        float stepS[3]={8,16,32};float g=stepS[std::max(0,std::min(2,(int)(fGrid+0.5f)))];
        double gsmp=sr*60.f/fBpm*4.0/g; // samples per grid-cel
        for(N&nn:fNotes){
            if(scale!=4){ // CHROMA = geen pitch-fix
                int pc=nn.note%12;int b=0,bd=99;
                for(int g2=0;g2<sl;g2++){int d=(pc-root-sc[g2]%12+18)%12-6;
                    if(std::abs(d)<bd){bd=std::abs(d);b=d;}}
                nn.note=(uint8_t)std::max(24,std::min(96,nn.note-b));
            }
            nn.note=(uint8_t)std::max(12,std::min(108,nn.note+(int)(fOct+0.5f)*12));
            uint32_t qa=(uint32_t)(nn.at/gsmp+0.5f)* (uint32_t)gsmp;
            nn.at=(uint32_t)(nn.at+q*((double)qa-nn.at));
            uint32_t qd=std::max((uint32_t)(gsmp*0.5f),(uint32_t)(nn.dur/gsmp+0.5f)*(uint32_t)gsmp);
            nn.dur=(uint32_t)(nn.dur+q*((double)qd-nn.dur)*0.9f);
        }
        fNN=(float)fNotes.size();
        if(fNotes.empty()){fStat=0;fKlas=0;return;}
        // loop-lengte = laatste noot-einde, op maat afgerond
        uint32_t end=0;for(const N&x:fNotes)end=std::max(end,x.at+x.dur);
        double bar=sr*60.f/fBpm*4.f;
        fLoopLen=(uint32_t)(std::max(1,(int)(end/bar+0.99f))*bar);
        std::sort(fNotes.begin(),fNotes.end(),[](const N&a,const N&b){return a.at<b.at;});
        // tempo schatten uit noot-intervallen als geen host-bpm
        if(!fBpmFromHost&&fNotes.size()>=4){
            double d[512];int dn=0;
            for(size_t i=1;i<fNotes.size()&&dn<512;i++)
                if(fNotes[i].at>fNotes[i-1].at)d[dn++]=fNotes[i].at-fNotes[i-1].at;
            double med=dn?d[dn/2]:sr*0.5f;
            if(med>0)fBpm=(float)std::max(60.,std::min(200.,sr*15.0/med));
        }
        fKlas=2;fLoopPos=0;fNIdx=0;fStat=3;
    }

    // ---- INSTRUMENT: pak het luidste stuk als sample, bepaal toonhoogte ----
    void analyzeInstrument(float sr,int n){
        // luidste 2s venster
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
        // fade-in/out 3% + DC-blok
        int f2=fSmpN/40;
        for(int i=0;i<f2;i++){float r=(float)i/f2;fSmp[i]*=r;fSmp[fSmpN-1-i]*=r;}
        double dc=0;for(int i=0;i<fSmpN;i++)dc+=fSmp[i];dc/=fSmpN;
        for(int i=0;i<fSmpN;i++)fSmp[i]-=(float)dc*0.9f;
        fNotes.clear();fNN=1;fKlas=3;fStat=3;fLoopLen=0;fLoopPos=0;fNIdx=0;
    }

    void writeMid(){
        if(fNotes.empty()||fMode>0.5f)return;
        const double fpt=480.0/(getSampleRate()*60.0/fBpm); // ticks per sample (480 tpq, 1 kwart = fBpm-tel)
        const char*home=std::getenv("USERPROFILE");if(!home||!*home)home=std::getenv("HOME");
        char dir[520];std::snprintf(dir,sizeof(dir),"%s/Music/G2B",home?home:".");
        #ifdef _WIN32
        {std::string m=std::string(home?home:".")+"/Music";mkdir(m.c_str());mkdir(dir);}
        #else
        {std::string m=std::string(home?home:".")+"/Music";mkdir(m.c_str(),0755);mkdir(dir,0755);}
        #endif
        smf::MidiFile mf;mf.setTicksPerQuarterNote(480);int seq=0;
        for(const N&n:fNotes){
            smf::MidiEvent on(0x90,n.note,n.vel);on.tick=(int)(n.at*fpt);on.seq=seq++;
            smf::MidiEvent off(0x80,n.note,0);off.tick=(int)((n.at+n.dur)*fpt);off.seq=seq++;
            mf[0].push_back(on);mf[0].push_back(off);
        }
        mf.sortTracks();
        char path[640];std::snprintf(path,sizeof(path),"%s/G2B-OPROEP-MELODIE.mid",dir);
        mf.write(path);fStat=4;
    }

    // ---- instrument-stem (varispeed resample + loop-crossfade) ----
    struct V{float pos=0,rate=0,amp=0,env=0;uint8_t note=0;int act=0;};
    V fV[16];

    void run(const float** in, float** out, uint32_t frames, const MidiEvent* evs, uint32_t nev) override {
        const float sr=getSampleRate();
        const TimePosition&tp=getTimePosition();
        bool playing=true;
        if(tp.bbt.valid){
            if(tp.bbt.beatsPerMinute>1.0){fBpm=tp.bbt.beatsPerMinute;fBpmFromHost=1;fStep16=sr*60.f/fBpm/4.f;}
            playing=tp.playing;
        }
        // audio passthrough behalve in INSTRUMENT-modus (dan is uit = instrumentgeluid)
        bool instr=fMode>0.5f&&fSmpN>256;
        for(uint32_t i=0;i<frames;i++){
            out[0][i]=in[0][i];out[1][i]=in[1][i];
        }
        // midi-noten in → instrument-stemmen
        for(uint32_t e=0;e<nev;e++){
            const MidiEvent&ev=evs[e];
            if(ev.size<3)continue;
            uint8_t st=ev.data[0]&0xF0,nt=ev.data[1],vl=ev.data[2];
            if(instr&&(st==0x90&&vl>0)){
                int slot=-1;for(int k=0;k<16;k++)if(!fV[k].act){slot=k;break;}
                if(slot<0)slot=0;
                V&v=fV[slot];v.note=nt;v.pos=0;v.env=0;
                float nh=440.f*powf(2.f,(nt-69)/12.f);
                v.rate=nh/fSmpHz;v.amp=vl/127.f;v.act=1;
            }else if(instr&&(st==0x80||(st==0x90&&vl==0))){
                for(int k=0;k<16;k++)if(fV[k].act&&fV[k].note==nt)fV[k].act=2; // release
            }
        }
        if(instr){
            for(int k=0;k<16;k++){
                V&v=fV[k];if(!v.act)continue;
                float atk=v.env<1.f?0.005f:0.f;
                for(uint32_t i=0;i<frames;i++){
                    if(v.env<1.f)v.env+=atk;
                    if(v.act==2)v.env*=0.9992f;
                    int p=(int)v.pos;float fr=v.pos-p;
                    int p2=(p+1)%fSmpN;
                    float s=(fSmp[p]*(1-fr)+fSmp[p2]*fr)*v.env*v.amp;
                    out[0][i]+=s;out[1][i]+=s;
                    v.pos+=v.rate;
                    if(v.pos>=fSmpN-64){ // loop met 64-sample crossfade naar start
                        v.pos-=fSmpN-128;if(v.pos<0)v.pos=0;
                    }
                }
                if(v.act==2&&v.env<0.001f)v.act=0;
            }
        }
        if(fLuist>0.5f&&fStat==1){
            for(uint32_t i=0;i<frames&&fRecN<kRecMax;i++)
                fRec[fRecN++]=(in[0][i]+in[1][i])*0.5f;
            if(fRecN>=kRecMax){fLuist=0;fWantAna=1;}
        }
        if(fWantAna){fWantAna=0;analyze();}
        if(fWantRefine){fWantRefine=0;if(!fNotes.empty()&&fMode<0.5f)analyze();}
        if(fWantMid){fWantMid=0;writeMid();}
        // melodie-loop meespelen (alleen bij transport-play)
        if(fMode<0.5f&&fOn>0.5f&&playing&&fStat==3&&fLoopLen>0&&!fNotes.empty()){
            uint32_t base=fLoopPos;
            while(fNIdx<fNotes.size()&&fNotes[fNIdx].at<base+frames){
                const N&nn=fNotes[fNIdx];
                int fr=(int)(nn.at-base);if(fr<0)fr=0;if(fr>=(int)frames)fr=frames-1;
                MidiEvent e{};e.frame=(uint32_t)fr;e.size=3;e.data[0]=0x90|nn.ch;e.data[1]=nn.note;e.data[2]=nn.vel;
                writeMidiEvent(e);
                fOff[fOffW].at=nn.at+nn.dur;fOff[fOffW].note=nn.note;fOff[fOffW].ch=nn.ch;fOffW=(fOffW+1)%512;
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
    float fSmp[2*48000];int fSmpN=0;float fSmpHz=0;
    float fGevoel=70,fGrid=1,fOct=0,fVar=0,fScale=0,fMode=0,fOn=1,fLuist=0;
    float fKey=-1,fBpm=120,fStat=0,fNN=0,fKlas=0,fLevel=0,fLevelBlk=0;
    int fWantAna=0,fWantMid=0,fWantRefine=0,fBpmFromHost=0;
    double fStep16=0;uint32_t fLoopPos=0,fLoopLen=0,fNIdx=0,fRnd=12345;
    std::vector<N> fNotes;
    struct O{uint32_t at;uint8_t note,ch;};O fOff[512];int fOffW=0;
};

Plugin* createPlugin(){return new OproepPlugin();}
END_NAMESPACE_DISTRHO
