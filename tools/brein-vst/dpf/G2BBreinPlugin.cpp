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
    BreinPlugin() : Plugin(14, 0, 0) { regen_(); }

protected:
    const char* getLabel() const override { return "G2B Brein"; }
    const char* getDescription() const override
    { return "G2B patroon-brein: 219 geleerde groove-kits → nieuwe patronen als MIDI-out (drums ch10, melodie ch1)."; }
    const char* getMaker() const override { return "G2B (good2better)"; }
    const char* getHomePage() const override { return "https://github.com/good2better"; }
    const char* getLicense() const override { return "MIT"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','B','r'); }

    void initState(uint32_t, String&, String&) override {}
    void setState(const char*, const char*) override {}

    enum { kGenre, kKit, kBars, kSeed, kKracht, kSwing,
           kDrums, kMel, kMidi, kPlay, kGen, kDice, kExport, kLight };

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
        }
    }

    float getParameterValue(uint32_t i) const override {
        switch(i){case kGenre:return fGenre;case kKit:return fKit;case kBars:return fBars;
                  case kSeed:return fSeed;case kKracht:return fKracht;case kSwing:return fSwing;
                  case kDrums:return fDrums;case kMel:return fMel;case kMidi:return fMidi;
                  case kPlay:return fPlay;case kGen:return 0;case kDice:return 0;
                  case kExport:return 0;case kLight:return fLight;}return 0.f;
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
        if(fMidi<0.5f)return;
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
    // ---- EXPORT .MID: huidige patroon (met kracht + laag-schakelaars) wegschrijven ----
    void writeMid_(){
        smf::MidiFile mf; mf.setTicksPerQuarterNote(480); mf.addTrack(1);
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
};

Plugin* createPlugin(){ return new BreinPlugin(); }
END_NAMESPACE_DISTRHO
