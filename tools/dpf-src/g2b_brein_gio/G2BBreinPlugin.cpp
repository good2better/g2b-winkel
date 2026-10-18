// G2B Brein — het 986-midi patroon-brein als VSTi:
// genre-pills + kit-kiezer + GENEREER → MIDI-out naar je DAW (drums kanaal 10, melodie kanaal 1).
// Geleerde groove-DNA uit 219 kits (brein_data.h) — geen kopieën, nieuwe variaties per seed.
// © G2B (good2better) — MIT.
#include "DistrhoPlugin.hpp"
#include "brein_data.h"
#include "brein_gen.h"
#include "vendor/MidiFile.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

START_NAMESPACE_DISTRHO

class BreinPlugin : public Plugin
{
public:
    BreinPlugin() : Plugin(18, 0, 0) { regen_(); }

protected:
    const char* getLabel() const override { return "G2B Brein GIORGIO"; }
    const char* getDescription() const override
    { return "G2B patroon-brein: 219 geleerde groove-kits → nieuwe patronen als MIDI-out (drums ch10, melodie ch1)."; }
    const char* getMaker() const override { return "G2B (good2better)"; }
    const char* getHomePage() const override { return "https://github.com/good2better"; }
    const char* getLicense() const override { return "MIT"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','B','G'); }

    void initState(uint32_t, String&, String&) override {}
    void setState(const char* key, const char* value) override {
        if(std::strcmp(key,"leermid")==0){ loadLeerMid_(value); regen_(); }
    }

    enum { kGenre, kKit, kBars, kSeed, kKracht, kSwing,
           kDrums, kMel, kMidi, kPlay, kGen, kDice, kExport, kLight,
           kOn, kDim, kLeerMode, kGenN };

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints = kParameterIsAutomatable;
        switch(i){
        case kGenre:  p.name="GENRE";  p.symbol="genre"; p.ranges.min=0; p.ranges.max=4;   p.ranges.def=0;    p.hints|=kParameterIsInteger; break;
        case kKit:    p.name="KIT";    p.symbol="kit";   p.ranges.min=0; p.ranges.max=127; p.ranges.def=0;    p.hints|=kParameterIsInteger; break;
        case kBars:   p.name="BARS";   p.symbol="bars";  p.ranges.min=1; p.ranges.max=16;  p.ranges.def=4;    p.hints|=kParameterIsInteger; break;
        case kSeed:   p.name="SEED";   p.symbol="seed";  p.ranges.min=0; p.ranges.max=255; p.ranges.def=7;    p.hints|=kParameterIsInteger; break;
        case kKracht: p.name="KRACHT"; p.symbol="kracht";p.ranges.min=0; p.ranges.max=1;   p.ranges.def=.85f; break;
        case kSwing:  p.name="SWING";  p.symbol="swing"; p.ranges.min=0; p.ranges.max=2;   p.ranges.def=1.f;  break;
        case kDrums:  p.name="DRUMS";  p.symbol="drums"; p.ranges.min=0; p.ranges.max=1;   p.ranges.def=1;    p.hints|=kParameterIsBoolean; break;
        case kMel:    p.name="MEL";    p.symbol="mel";   p.ranges.min=0; p.ranges.max=1;   p.ranges.def=1;    p.hints|=kParameterIsBoolean; break;
        case kMidi:   p.name="MIDI OUT";p.symbol="midiout";p.ranges.min=0;p.ranges.max=1;  p.ranges.def=1;    p.hints|=kParameterIsBoolean; break;
        case kPlay:   p.name="SPEEL";  p.symbol="speel"; p.ranges.min=0; p.ranges.max=1;   p.ranges.def=1;    p.hints|=kParameterIsBoolean; break;
        case kGen:    p.name="GENEREER";p.symbol="genereer";p.ranges.min=0;p.ranges.max=1; p.hints|=kParameterIsTrigger; break;
        case kDice:   p.name="DICE";   p.symbol="dice";  p.ranges.min=0; p.ranges.max=1;   p.hints|=kParameterIsTrigger; break;
        case kExport: p.name="EXPORT .MID";p.symbol="exportmid";p.ranges.min=0;p.ranges.max=1; p.hints|=kParameterIsTrigger; break;
        case kLight:  p.name="SKIN";   p.symbol="skin";  p.ranges.min=0; p.ranges.max=7;   p.ranges.def=0;    p.hints|=kParameterIsInteger; break;
        case kOn:     p.name="AAN/UIT";p.symbol="aan";   p.ranges.min=0; p.ranges.max=1;   p.ranges.def=1;    p.hints|=kParameterIsBoolean; break;
        case kDim:    p.name="DIMENTIES";p.symbol="dim"; p.ranges.min=0; p.ranges.max=26;  p.ranges.def=8;    p.hints|=kParameterIsInteger; break;
        case kLeerMode:p.name="MIJN-MODE";p.symbol="leer";p.ranges.min=0; p.ranges.max=1;   p.ranges.def=0;    p.hints|=kParameterIsBoolean; break;
        case kGenN:   p.name="GEN 8x → MAP";p.symbol="genn";p.ranges.min=0;p.ranges.max=1;  p.hints|=kParameterIsTrigger; break;
        }
    }

    float getParameterValue(uint32_t i) const override {
        switch(i){case kGenre:return fGenre;case kKit:return fKit;case kBars:return fBars;
                  case kSeed:return fSeed;case kKracht:return fKracht;case kSwing:return fSwing;
                  case kDrums:return fDrums;case kMel:return fMel;case kMidi:return fMidi;
                  case kPlay:return fPlay;case kGen:return 0;case kDice:return 0;
                  case kExport:return 0;case kLight:return fLight;
                  case kOn:return fOn;case kDim:return fDim;case kLeerMode:return fLeer;
                  case kGenN:return 0;}return 0.f;
    }

    void setParameterValue(uint32_t i, float v) override {
        switch(i){
        case kGenre: fGenre=v; fKit=0; genN_=0; regen_(); break;
        case kKit:   fKit=v;   genN_=0; regen_(); break;
        case kBars:  fBars=v;  regen_(); break;
        case kSeed:  fSeed=v;  genN_=0; regen_(); break;
        case kKracht:fKracht=v;break;
        case kSwing: fSwing=v; break;
        case kDrums: fDrums=v; break;
        case kMel:   fMel=v;   break;
        case kMidi:  fMidi=v;  break;
        case kPlay:  if(v>0.5f&&fPlay<0.5f){fStep=-1;fPhase=0;ni_=0;} fPlay=v; break;
        case kGen:   if(v>0.5f){genN_++; regen_();} break;
        case kDice:  if(v>0.5f)dice_(); break;
        case kExport:if(v>0.5f)writeMid_(); break;
        case kLight: fLight=v; break;
        case kOn:    fOn=v; break;
        case kDim:   fDim=v; break;
        case kLeerMode: fLeer=v; regen_(); break;
        case kGenN:  if(v>0.5f)writeBatch_(); break;
        }
    }

    void run(const float**, float** out, uint32_t frames, const MidiEvent*, uint32_t) override {
        const double sr=getSampleRate();
        for(uint32_t i=0;i<frames;i++){out[0][i]=0.f;out[1][i]=0.f;}
        double bpm=120.0;
        const TimePosition& tp=getTimePosition();
        if(tp.playing && tp.bbt.valid && tp.bbt.beatsPerMinute>1.0) bpm=tp.bbt.beatsPerMinute;
        else { const BKit& k=kKits[kitAbs_()]; if(k.bpm>0) bpm=k.bpm; } // geen host-tempo → kit-bpm
        fBpm=bpm;
        // host-stop = echte stop: geen geldige transport-info → gewoon spelen (free-run hosts)
        const bool hostHasTransport = tp.bbt.valid;
        const bool playNow = fPlay>0.5f && (!hostHasTransport || tp.playing);
        if(wasPlaying_ && !playNow){                    // stop-transitie: stil + terug naar stap 1
            flushOffs_();
            fStep=-1; ni_=0; fPhase=0;
        }
        wasPlaying_=playNow;
        fpt_=sr*60.0/(bpm*480.0);                      // frames per tick (480 tpq)
        const double stepLen=60.0/bpm/4.0;             // sec per 16e
        const int totalSteps=clampi(fBars,1,16)*16;
        for(uint32_t i=0;i<frames;i++){
            const uint64_t abs=fAbs+i;
            while(offr_!=offw_){PendingOff&o=offq_[offr_];if(o.at>abs)break;
                emitMidi_(0x80|o.ch,o.note,0,i);offr_=(offr_+1)%128;}
            if(playNow){
                fPhase+=1.0/sr;
                const double curLen=stepLen*((fStep&1)?(1.0-0.335*fSwing):(1.0+0.335*fSwing));
                if(fPhase>=curLen){
                    fPhase-=curLen; fStep++;
                    if(fStep>=totalSteps){fStep=0;ni_=0;}
                    const int st=fStep;
                    while(ni_<(int)gen_.size()&&gen_[ni_].t<=st*120){
                        const BNote&n=gen_[ni_++];
                        if(n.t!=st*120)continue;
                        if((n.ch==9&&fDrums>0.5f)||(n.ch==0&&fMel>0.5f))
                            hit_(n.note,n.ch,(int)(n.vel*fKracht),i,n.dur*fpt_);
                    }
                    char sb[12];std::snprintf(sb,sizeof(sb),"%d",st);
                    updateStateValue("step",sb);
                }
            }
        }
        fAbs+=frames;
    }

private:
    // ---- MIDI-out: note-on nu, note-off via absolute-frame queue ----
    struct PendingOff{uint8_t note,ch;uint64_t at;};
    PendingOff offq_[128]; int offw_=0,offr_=0;
    std::vector<BNote> gen_; int ni_=0; int genN_=0;
    double fPhase=0,fpt_=1.0,fBpm=100.0; int fStep=-1; uint64_t fAbs=0;
    bool wasPlaying_=false;

    void emitMidi_(uint8_t st,uint8_t nt,uint8_t vel,uint32_t fr){
        MidiEvent e{}; e.frame=fr; e.size=3;
        e.data[0]=st; e.data[1]=nt; e.data[2]=vel;
        writeMidiEvent(e);
    }
    void hit_(int note,int ch,int vel,uint32_t i,double durFr){
        vel=vel<20?20:(vel>127?127:vel);
        if(fMidi<0.5f||fOn<0.5f)return;
        emitMidi_(0x90|ch,(uint8_t)note,(uint8_t)vel,i);
        PendingOff&o=offq_[offw_];o.note=(uint8_t)note;o.ch=(uint8_t)ch;
        o.at=fAbs+i+(uint64_t)durFr;offw_=(offw_+1)%128;
    }
    void flushOffs_(){ // alle hangende noten NU uit (host-stop / SPEEL uit)
        while(offr_!=offw_){PendingOff&o=offq_[offr_];
            emitMidi_(0x80|o.ch,o.note,0,0);offr_=(offr_+1)%128;}
    }
    int kitAbs_() const {
        const BGK& gk=kGenreKits[clampi(fGenre,0,kNGenres-1)];
        return gk.off+clampi(fKit,0,gk.n-1);
    }
    void regen_(){
        if(fLeer>0.5f && fLearnN>0){ regenLeer_(); return; }
        const int idx=kitAbs_();
        gen_=BreinGen((uint32_t)(clampi(fSeed,0,255)*7919+genN_*104729+1)).gen(idx,clampi(fBars,1,16));
        ni_=0; if(fStep<0)fStep=-1;
        char nb[56];std::snprintf(nb,sizeof(nb),"%s · %d noten",kNames[kKits[idx].name],(int)gen_.size());
        updateStateValue("kit",nb);
    }
    void dice_(){
        fSeed=(float)(rand()%256);
        fGenre=(float)(rand()%kNGenres);
        const BGK& gk=kGenreKits[(int)fGenre];
        fKit=(float)(rand()%gk.n);
        genN_++; regen_();
        updateStateValue("dice","1"); // UI pakt eigen random → deze trig is alleen DSP-side
    }
    // ---- GIORGIO LEER: geleerde midi → Markov-model → variaties in jouw stijl ----
    static int b64d_(const char* s,unsigned char* out,int cap){
        static const char* T="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        int inv[256]; for(int i=0;i<256;i++)inv[i]=-1;
        for(int i=0;i<64;i++)inv[(unsigned char)T[i]]=i;
        int n=0,val=0,bits=-8;
        for(const char* p=s;*p;p++){int c=(unsigned char)*p;if(c=='=')break;if(inv[c]<0)continue;
            val=(val<<6)|inv[c];bits+=6;if(bits>=0){if(n<cap)out[n++]=(val>>bits)&0xFF;bits-=8;}}
        return n;
    }
    void loadLeerMid_(const char* b64){
        unsigned char buf[512*1024];
        int n=b64d_(b64,buf,sizeof(buf));
        fLearnN=0; for(int i=0;i<12;i++)fLearnIv[i].clear(); fLearnDur.clear(); fLearnRest.clear(); fLearnCh9.clear();
        if(n<14||std::memcmp(buf,"MThd",4)!=0){updateStateValue("leer","geen .mid — probeer een ander bestand");return;}
        // mini-SMF parser (zelfde logica als midi.py): verzamel note-on/off
        const unsigned char* d=buf; int pos=14;
        int ntrk=(d[10]<<8)|d[11], div=(d[12]<<8)|d[13];
        if(div<10||div>9600)div=480;
        struct LN{int t,dur,note,vel,ch;};
        LN ev[4096]; int ne=0;
        for(int t=0;t<ntrk&&pos<n;t++){
            if(std::memcmp(d+pos,"MTrk",4)!=0)break;
            int ln=(d[pos+4]<<24)|(d[pos+5]<<16)|(d[pos+6]<<8)|d[pos+7]; pos+=8;
            int end=pos+ln; if(end>n)end=n;
            int tt=0,run=0; int ons[128][16]={}; int onc[128]={};
            while(pos<end&&ne<4096){
                int dt=0; while(true){int b=d[pos++];dt=(dt<<7)|(b&0x7F);if(b<0x80)break;}
                tt+=dt;
                int b=d[pos];
                if(b&0x80){run=b;pos++;
                    if(run==0xFF){int l2=d[pos+1];pos+=2+l2;run=0;continue;}
                    if(run==0xF0||run==0xF7){int l2=d[pos+1];pos+=1+l2;run=0;continue;}}
                int ev2=run&0xF0,ch=run&0x0F,nd=(ev2==0xC0||ev2==0xD0)?1:2;
                if(ev2==0x90&&d[pos+1]>0){ons[d[pos]][onc[d[pos]]<16?onc[d[pos]]++:15]=tt;pos+=nd;}
                else if(ev2==0x80||(ev2==0x90&&d[pos+1]==0)){
                    int nn=d[pos];int st=onc[nn]>0?ons[nn][--onc[nn]]:tt;
                    ev[ne++]={st,tt-st,nn,nd>1?d[pos+1]:80,ch};pos+=nd;}
                else pos+=nd;}
            pos=end;
        }
        if(ne<3){updateStateValue("leer","midi te klein/leeg");return;}
        // bouw Markov: interval per vorige pitchclass + duur + rests + drums
        for(int i=1;i<ne;i++){
            if(ev[i].ch==9){fLearnCh9.push_back(ev[i].note);continue;}
            int iv=ev[i].note-ev[i-1].note;
            fLearnIv[ev[i-1].note%12].push_back(iv);
            fLearnDur.push_back(ev[i].dur);
            int gap=ev[i].t-(ev[i-1].t+ev[i-1].dur);
            if(gap>0)fLearnRest.push_back(gap);
        }
        fLearnN=ne; fLearnRoot=ev[0].note;
        char m[80];std::snprintf(m,sizeof(m),"GELEERD: %d noten uit jouw midi",ne);
        updateStateValue("leer",m);
    }
    void regenLeer_(){
        gen_.clear();
        BreinGen g((uint32_t)(clampi(fSeed,0,255)*7919+genN_*104729+1));
        const int div=480,bars=clampi(fBars,1,16);
        int t=0,prev=fLearnRoot;
        const int dimBoost=(int)fDim; // dimensies → meer variatie in sprongen
        while(t<bars*div*4-60){
            int dur= fLearnDur.empty()?240:fLearnDur[(int)(g.frand()*fLearnDur.size())];
            if(dur<60)dur=60;if(dur>div*2)dur=div*2;
            gen_.push_back({t,dur,0,(uint8_t)prev,(uint8_t)(60+g.frand()*55)});
            t+=dur;
            if(!fLearnRest.empty()&&g.frand()<0.3f) t+=fLearnRest[(int)(g.frand()*fLearnRest.size())];
            const std::vector<int>& ivs=fLearnIv[prev%12];
            int iv= ivs.empty()? (int)(g.frand()*7)-3 : ivs[(int)(g.frand()*ivs.size())];
            if(dimBoost>12&&g.frand()<(dimBoost-12)/14.f*0.5f) iv+=(iv>=0?7:-7); // grote dim → octaaf-sprongen
            prev+=iv;
            while(prev<fLearnRoot-12)prev+=12; while(prev>fLearnRoot+24)prev-=12;
            if(prev<0)prev=0; if(prev>127)prev=127;
        }
        // geleerde drums meenemen (ch9 noten uit bron op 16e-grid)
        if(!fLearnCh9.empty()){
            for(int bar=0;bar<bars;bar++)for(int s=0;s<16;s++)
                if(g.frand()<0.22f)
                    gen_.push_back({(bar*16+s)*120,120,9,fLearnCh9[(int)(g.frand()*fLearnCh9.size())],100});
        }
        // sort op t
        for(size_t i=1;i<gen_.size();i++){BNote x=gen_[i];size_t j=i;
            while(j&&gen_[j-1].t>x.t){gen_[j]=gen_[j-1];j--;}gen_[j]=x;}
        ni_=0;
        char nb[56];std::snprintf(nb,sizeof(nb),"MIJN-STIJL · %d noten",(int)gen_.size());
        updateStateValue("kit",nb);
    }
    // ---- GEN 8x: acht variaties als .mid in eigen map ----
    void writeBatch_(){
        const char* home=std::getenv("USERPROFILE");
        if(!home||!*home)home=std::getenv("HOME");
        char dir[520],dir2[560];
        if(home&&*home){
            std::snprintf(dir2,sizeof(dir2),"%s/Music/G2B",home);
            std::snprintf(dir,sizeof(dir),"%s/Music/G2B/midigen",home);
        }else{ std::snprintf(dir,sizeof(dir),".");std::snprintf(dir2,sizeof(dir2),"."); }
#ifdef _WIN32
        _mkdir(dir2); _mkdir(dir);
#else
        mkdir(dir2,0755); mkdir(dir,0755);
#endif
        int ok=0;
        for(int v=0;v<8;v++){
            genN_++; regen_();
            smf::MidiFile mf; mf.setTicksPerQuarterNote(480);
            int seq=0;
            for(const BNote&n:gen_){
                int vv=(int)(n.vel*fKracht); vv=vv<20?20:(vv>127?127:vv);
                smf::MidiEvent on(0x90|n.ch,n.note,vv); on.tick=n.t; on.seq=seq++;
                smf::MidiEvent off(0x80|n.ch,n.note,0); off.tick=n.t+n.dur; off.seq=seq++;
                mf[0].push_back(on); mf[0].push_back(off);
            }
            mf.sortTracks();
            char path[600];std::snprintf(path,sizeof(path),"%s/brein_%02d.mid",dir,v+1);
            if(mf.write(path))ok++;
        }
        genN_--; regen_();
        char m[80];std::snprintf(m,sizeof(m),"%d .mid in Music/G2B/midigen",ok);
        updateStateValue("midifile",m);
    }

    // ---- EXPORT .MID: huidige patroon (met kracht + laag-schakelaars) wegschrijven ----
    void writeMid_(){
        smf::MidiFile mf; mf.setTicksPerQuarterNote(480);
        int seq=0;
        for(const BNote&n:gen_){
            if(n.ch==9&&fDrums<0.5f)continue;
            if(n.ch==0&&fMel<0.5f)continue;
            int v=(int)(n.vel*fKracht); v=v<20?20:(v>127?127:v);
            smf::MidiEvent on(0x90|n.ch,n.note,v); on.tick=n.t; on.seq=seq++;
            smf::MidiEvent off(0x80|n.ch,n.note,0); off.tick=n.t+n.dur; off.seq=seq++;
            mf[0].push_back(on); mf[0].push_back(off);
        }
        mf.sortTracks();
        const char* home=std::getenv("USERPROFILE");
        if(!home||!*home)home=std::getenv("HOME");
        char dir[520],path[600];
        if(home&&*home)std::snprintf(dir,sizeof(dir),"%s\\Music\\G2B",home);
        else std::snprintf(dir,sizeof(dir),".");
#ifdef _WIN32
        _mkdir(dir);
#else
        mkdir(dir,0755);
#endif
        std::snprintf(path,sizeof(path),"%s\\G2B-Brein.mid",dir);
        if(mf.write(path))updateStateValue("midifile",".mid opgeslagen: Music\\G2B\\G2B-Brein.mid");
        else updateStateValue("midifile",".mid NIET gelukt — map niet schrijfbaar");
    }

    static int clampi(float v,int a,int b){int x=(int)(v+0.5f);return x<a?a:(x>b?b:x);}

    float fGenre=0,fKit=0,fBars=4,fSeed=7,fKracht=.85f,fSwing=1.f;
    float fDrums=1,fMel=1,fMidi=1,fPlay=1,fLight=0;
    float fOn=1,fDim=8,fLeer=0;
    // GIORGIO leer-model
    int fLearnN=0,fLearnRoot=60;
    std::vector<int> fLearnIv[12]; std::vector<int> fLearnDur,fLearnRest;
    std::vector<int> fLearnCh9;
};

Plugin* createPlugin(){ return new BreinPlugin(); }
END_NAMESPACE_DISTRHO
