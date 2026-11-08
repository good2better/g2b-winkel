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
#include <ctime>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

START_NAMESPACE_DISTRHO

class BreinPlugin : public Plugin
{
public:
    BreinPlugin() : Plugin(26, 0, 0) { regen_(); }

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
           kOn, kDim, kLeerMode, kGenN, kQuant, kFill, kHum, kVar, kAll, kUndo,
           kAuto, kSong };

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
        case kGen:    p.name="GENEREER";p.symbol="genereer";p.ranges.min=0;p.ranges.max=1; p.hints|=kParameterIsBoolean; break;
        case kDice:   p.name="DICE";   p.symbol="dice";  p.ranges.min=0; p.ranges.max=1;   p.hints|=kParameterIsBoolean; break;
        case kExport: p.name="EXPORT .MID";p.symbol="exportmid";p.ranges.min=0;p.ranges.max=1; p.hints|=kParameterIsBoolean; break;
        case kLight:  p.name="SKIN";   p.symbol="skin";  p.ranges.min=0; p.ranges.max=7;   p.ranges.def=0;    p.hints|=kParameterIsInteger; break;
        case kOn:     p.name="AAN/UIT";p.symbol="aan";   p.ranges.min=0; p.ranges.max=1;   p.ranges.def=1;    p.hints|=kParameterIsBoolean; break;
        case kDim:    p.name="DIMENTIES";p.symbol="dim"; p.ranges.min=0; p.ranges.max=26;  p.ranges.def=8;    p.hints|=kParameterIsInteger; break;
        case kLeerMode:p.name="MIJN-MODE";p.symbol="leer";p.ranges.min=0; p.ranges.max=1;   p.ranges.def=0;    p.hints|=kParameterIsBoolean; break;
        case kGenN:   p.name="GEN 8x PREVIEW";p.symbol="genn";p.ranges.min=0;p.ranges.max=1;  p.hints|=kParameterIsBoolean; break;
        case kQuant:  p.name="QUANTIZE";p.symbol="quant";p.ranges.min=0; p.ranges.max=1;   p.ranges.def=1;    p.hints|=kParameterIsBoolean; break;
        case kFill:   p.name="FILL";    p.symbol="fill"; p.ranges.min=0; p.ranges.max=1;   p.ranges.def=1;    p.hints|=kParameterIsBoolean; break;
        case kHum:    p.name="HUMAN";   p.symbol="human";p.ranges.min=0; p.ranges.max=1;   p.ranges.def=.5f; break;
        case kVar:    p.name="VARIANT"; p.symbol="var";  p.ranges.min=0; p.ranges.max=7;   p.ranges.def=0;    p.hints|=kParameterIsInteger; break;
        case kAll:    p.name="ALL 8 → MAP";p.symbol="all";p.ranges.min=0;p.ranges.max=1;   p.hints|=kParameterIsBoolean; break;
        case kUndo:   p.name="TERUG";  p.symbol="undo"; p.ranges.min=0;p.ranges.max=1;   p.hints|=kParameterIsBoolean; break;
        case kAuto:   p.name="AUTO-SCORE";p.symbol="auto";p.ranges.min=0;p.ranges.max=1;   p.hints|=kParameterIsBoolean; break;
        case kSong:   p.name="SONG";   p.symbol="song";  p.ranges.min=0;p.ranges.max=1;   p.hints|=kParameterIsBoolean; break;
        }
    }

    float getParameterValue(uint32_t i) const override {
        switch(i){case kGenre:return fGenre;case kKit:return fKit;case kBars:return fBars;
                  case kSeed:return fSeed;case kKracht:return fKracht;case kSwing:return fSwing;
                  case kDrums:return fDrums;case kMel:return fMel;case kMidi:return fMidi;
                  case kPlay:return fPlay;case kGen:return 0;case kDice:return 0;
                  case kExport:return 0;case kLight:return fLight;
                  case kOn:return fOn;case kDim:return fDim;case kLeerMode:return fLeer;
                  case kGenN:return 0;case kQuant:return fQuant;case kFill:return fFill;
                  case kHum:return fHum;case kVar:return (float)fVarSel;case kAll:return 0;case kUndo:return 0;
                  case kAuto:return 0;case kSong:return 0;}return 0.f;
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
        case kGenN:  if(v>0.5f)genBatch_(); break;
        case kQuant: fQuant=v; regen_(); break;
        case kFill:  fFill=v;  regen_(); break;
        case kHum:   fHum=v;   regen_(); break;
        case kVar:   pickVar_((int)v); break;
        case kAll:   if(v>0.5f)writeBatch_(); break;
        case kUndo:  if(v>0.5f)undo_(); break;
        case kAuto:  if(v>0.5f)autoScore_(); break;
        case kSong:  if(v>0.5f)songExport_(); break;
        }
    }

    void run(const float**, float** out, uint32_t frames, const MidiEvent*, uint32_t) override {
        const double sr=getSampleRate();
        for(uint32_t i=0;i<frames;i++){out[0][i]=0.f;out[1][i]=0.f;}
        float pvMix[8];
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
        // genre-swing: 808/TRAP strak (~55%), KAWINA ruim (~64%), KEYS subtiel, MISC medium
        static const double kGenreSwing[5]={0.10,0.28,0.06,0.15,0.10};
        const int totalSteps=clampi(fBars,1,16)*16;
        for(uint32_t i=0;i<frames;i++){
            const uint64_t abs=fAbs+i;
            while(offr_!=offw_){PendingOff&o=offq_[offr_];if(o.at>abs)break;
                emitMidi_(0x80|o.ch,o.note,0,i);offr_=(offr_+1)%128;}
            if(playNow){
                fPhase+=1.0/sr;
                const double sw=kGenreSwing[clampi(fGenre,0,4)]*fSwing;
                const double curLen=stepLen*((fStep&1)?(1.0-sw):(1.0+sw));
                if(fPhase>=curLen){
                    fPhase-=curLen; fStep++;
                    if(fStep>=totalSteps){fStep=0;ni_=0;}
                    const int st=fStep;
                    while(ni_<(int)gen_.size()&&gen_[ni_].t<=(st+1)*120-1){
                        const BNote&n=gen_[ni_++];
                        if(n.t<st*120)continue;
                        if((n.ch==9&&fDrums>0.5f)||(n.ch==0&&fMel>0.5f))
                            hit_(n.note,n.ch,(int)(n.vel*fKracht),i,n.dur*fpt_);
                    }
                    char sb[12];std::snprintf(sb,sizeof(sb),"%d",st);
                    updateStateValue("step",sb);
                }
            }
        }
        // preview-klank: ook hoorbaar als er geen MIDI-routing is (standalone)
        for(uint32_t i=0;i<frames;i++){
            float m=0.f;
            for(PVoice&c:pv_)if(c.on){
                c.t+=1.0/(float)sr; float e=expf(-c.t/c.dec);
                if(e<0.002f){c.on=false;continue;}
                float w=sinf(6.28318f*c.f*c.t);
                if(c.noise>0.f){fPN=fPN*1664525u+1013904223u;w=w*(1.f-c.noise)+(((float)(fPN>>9&0xFFFF)/32768.f-1.f))*c.noise;}
                m+=w*e*c.amp;
            }
            out[0][i]+=m*0.5f;out[1][i]+=m*0.5f;
        }
        fAbs+=frames;
    }

private:
    // ---- MIDI-out: note-on nu, note-off via absolute-frame queue ----
    struct PendingOff{uint8_t note,ch;uint64_t at;};
    PendingOff offq_[128]; int offw_=0,offr_=0;
    // hoorbaar zonder MIDI-routing: lichte preview-klank per noot
    struct PVoice{float f=0,t=0,dec=.2f,amp=0,noise=0;bool on=false;} pv_[12];
    unsigned fPN=1;
    void pvStart_(int note,int ch){
        if(fOn<0.5f)return;
        PVoice* v=&pv_[0];
        for(PVoice&c:pv_)if(!c.on){v=&c;break;}
        if(ch==9){ v->f=(note<40)?52.f:(note<50)?185.f:650.f;
                   v->dec=(note<40)?0.18f:(note<50)?0.10f:0.045f;
                   v->noise=(note>=42)?0.75f:0.12f; v->amp=0.8f; }
        else     { v->f=440.f*powf(2.f,(note-69)/12.f); v->dec=0.22f; v->noise=0.08f; v->amp=0.5f; }
        v->t=0; v->on=true;
    }
    std::vector<BNote> gen_; int ni_=0; int genN_=0;
    std::vector<BNote> basis_,slag_; bool hasAuto_=false; // AUTO-SCORE bouwveld-lagen
    double fPhase=0,fpt_=1.0,fBpm=100.0; int fStep=-1; uint64_t fAbs=0;
    bool wasPlaying_=false;

    float humCurve_(int t)const{return 0.85f+0.30f*sinf((float)M_PI*(float)t/(fBars*1920.f));}
    void emitMidi_(uint8_t st,uint8_t nt,uint8_t vel,uint32_t fr){
        MidiEvent e{}; e.frame=fr; e.size=3;
        e.data[0]=st; e.data[1]=nt; e.data[2]=vel;
        writeMidiEvent(e);
    }
    void hit_(int note,int ch,int vel,uint32_t i,double durFr){
        pvStart_(note,ch);
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
        if(!gen_.empty()){ undoStk_.push_back(gen_); if(undoStk_.size()>10)undoStk_.erase(undoStk_.begin()); }
        if(fLeer>0.5f && fLearnN>0){ regenLeer_(); return; }
        const int idx=kitAbs_();
        const bool melJit=(clampi(fGenre,0,4)==2||clampi(fGenre,0,4)==3);
        gen_=BreinGen((uint32_t)(clampi(fSeed,0,255)*7919+genN_*104729+1)).gen(idx,clampi(fBars,1,16),fFill>0.5f,fHum,melJit);
        // ---- EVOLUTIE (DIMENTIES): meer verandering + eigen fills per bar-einde ----
        { const float e=fDim/26.0f; const int bars=clampi(fBars,1,16);
          if(e>0.05f){
            BreinGen g2((uint32_t)(clampi(fSeed,0,255)*31337+bars*911+genN_*17));
            for(int b=0;b<bars;b++){ const int tEnd=(b+1)*1920;
                const bool zinEnd=(b%4==3)||(b==bars-1);  // einde van 4-bar-zin
                const float p=(zinEnd?(b==bars-1?1.0f:0.7f):0.08f)*e;
                if(g2.frand()<p){ // fill-bank: 4 recepten i.p.v. alleen toms
                    const int k=(int)(g2.frand()*4);
                    if(k==0){ static const int tom[4]={47,48,50,45};
                        for(int i=0;i<4;i++) gen_.push_back({tEnd-480+i*120,90,9,(uint8_t)tom[i],(uint8_t)(85+g2.frand()*40)}); }
                    else if(k==1){ for(int i=0;i<4;i++) gen_.push_back({tEnd-480+i*120,60,9,38,(uint8_t)(70+i*14)}); } // snare opbouw
                    else if(k==2){ gen_.push_back({tEnd-480,60,9,36,110}); gen_.push_back({tEnd-240,120,9,49,120}); } // kick + crash
                    else { for(int i=0;i<8;i++) gen_.push_back({tEnd-480+i*60,30,9,42,(uint8_t)(95-i*8)}); } // hat-roll
                }
            }
            // melodie-eindes evolueren: octaaf-sprongen in latere bars
            if(e>0.4f) for(BNote&n:gen_) if(n.ch==0&&n.t>=bars*960&&g2.frand()<e*0.3f)
                { n.note+=(n.note%12<6?12:-12); if(n.note<24)n.note+=12; if(n.note>108)n.note-=12; }
            for(size_t i=1;i<gen_.size();i++){BNote x=gen_[i];size_t j=i;
                while(j&&gen_[j-1].t>x.t){gen_[j]=gen_[j-1];j--;}gen_[j]=x;}
          } }
        ni_=0; if(fStep<0)fStep=-1;
        char nb[56];std::snprintf(nb,sizeof(nb),"%s · %d noten",kNames[kKits[idx].name],(int)gen_.size());
        updateStateValue("kit",nb);
    }
    // ---- TERUG: laatste 10 GENEREER/DICE-stappen ----
    void undo_(){
        if(undoStk_.empty()){ updateStateValue("kit","niets om terug te draaien"); return; }
        gen_=undoStk_.back(); undoStk_.pop_back();
        ni_=0; fStep=-1; fPhase=0;
        char nb[64];std::snprintf(nb,sizeof(nb),"TERUG · %d noten (%d stappen over)",(int)gen_.size(),(int)undoStk_.size());
        updateStateValue("kit",nb);
    }

    // ---- AUTO-SCORE: 3-fase — fase1 basis (mel+kick) → fase2 slag-only → merge ----
    // BPM_LOCK: één fBpm stuurt álle lagen — slag kan nooit afwijken.
    void autoScore_(){
        genN_++; regen_();
        basis_.clear();
        for(const BNote&n:gen_) if(n.ch==0||n.note<=36) basis_.push_back(n); // mel + kick/sub
        genN_++; regen_();
        slag_.clear();
        for(const BNote&n:gen_) if(n.ch==9&&n.note>36) slag_.push_back(n);   // snare/hat/ohat/perc
        hasAuto_=true;
        gen_=basis_; gen_.insert(gen_.end(),slag_.begin(),slag_.end());
        for(size_t i=1;i<gen_.size();i++){BNote x=gen_[i];size_t j=i;
            while(j&&gen_[j-1].t>x.t){gen_[j]=gen_[j-1];j--;}gen_[j]=x;}
        ni_=0; fStep=-1; fPhase=0;
        char m[96];std::snprintf(m,sizeof(m),"AUTO-SCORE · basis %d + slag %d noten",(int)basis_.size(),(int)slag_.size());
        updateStateValue("kit",m);
    }
    // ---- SONG: arrangement-export intro→verse→refrein uit de AUTO-lagen ----
    void songExport_(){
        if(gen_.empty()){ updateStateValue("midifile","SONG: eerst AUTO of GENEREER"); return; }
        const int srcBars=clampi(fBars,1,16);
        struct Sec{int n;bool mel,kick,hat,slag;};
        const Sec sec[]={{4,true,false,false,false},{8,true,true,true,false},{8,true,true,true,true}};
        std::vector<BNote> out; int barOff=0;
        for(const Sec&s:sec){
            for(int b=0;b<s.n;b++){
                const int srcBar=b%srcBars;
                for(const BNote&n:gen_){
                    if(n.t<srcBar*1920||n.t>=(srcBar+1)*1920)continue;
                    bool keep=false;
                    if(n.ch==0)keep=s.mel;
                    else if(n.ch==9){ if(n.note<=36)keep=s.kick;
                        else if(n.note==42||n.note==44||n.note==46)keep=s.hat;
                        else keep=s.slag; }
                    if(keep){ BNote m=n; m.t=n.t-srcBar*1920+(barOff+b)*1920; out.push_back(m); }
                }
            }
            barOff+=s.n;
        }
        // .mid + .wav wegschrijven (zelfde map als writeMid_)
        const char* home=std::getenv("USERPROFILE");
        if(!home||!*home)home=std::getenv("HOME");
        char dir[520],path[600];
#ifdef _WIN32
        if(home&&*home)std::snprintf(dir,sizeof(dir),"%s\\Music\\G2B",home);
        else std::snprintf(dir,sizeof(dir),".");
        _mkdir(dir);
#else
        char dir2[560];
        if(home&&*home){std::snprintf(dir2,sizeof(dir2),"%s/Music/G2B",home);
            std::snprintf(dir,sizeof(dir),"%s/Music/G2B",home);}
        else {std::snprintf(dir,sizeof(dir),".");std::snprintf(dir2,sizeof(dir2),".");}
        mkdir(dir2,0755); mkdir(dir,0755);
#endif
        smf::MidiFile mf; mf.setTicksPerQuarterNote(480);
        int seq=0;
        for(const BNote&n:out){
            int v=(int)(n.vel*fKracht); v=v<20?20:(v>127?127:v);
            smf::MidiEvent on(0x90|n.ch,n.note,v); on.tick=n.t; on.seq=seq++;
            smf::MidiEvent off(0x80|n.ch,n.note,0); off.tick=n.t+n.dur; off.seq=seq++;
            mf[0].push_back(on); mf[0].push_back(off);
        }
        mf.sortTracks();
        std::snprintf(path,sizeof(path),"%s%cG2B-Brein-SONG_%s.mid",dir,
#ifdef _WIN32
            '\\',
#else
            '/',
#endif
            stamp_());
        if(mf.write(path)){
            char wpath[600];std::snprintf(wpath,sizeof(wpath),"%s%cG2B-Brein-SONG_%s.wav",dir,
#ifdef _WIN32
                '\\',
#else
                '/',
#endif
                stamp_());
            writeWav_(out,fBpm,wpath);
            updateStateValue("midifile","SONG .mid + .wav opgeslagen: 20 maten intro→verse→refrein");
#ifdef _WIN32
            char cmd[700]; std::snprintf(cmd,sizeof(cmd),"explorer \"%s\"",dir);
            std::system(cmd);
#endif
        } else updateStateValue("midifile","SONG NIET gelukt — map niet schrijfbaar");
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
            int tt=t;
            if(fQuant>0.5f) tt=((tt+60)/120)*120;   // quantize aan → op 16e-grid
            gen_.push_back({tt,dur,0,(uint8_t)prev,(uint8_t)(60+g.frand()*55)});
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
    // ---- GEN 8x PREVIEW: acht variaties in geheugen — geen bestanden ----
    // Blader met V1-V8 (VARIANT), SPEEL om te horen; ↓MID schrijft alleen de gekozen
    // variant, ALL schrijft alle 8 naar de map.
    void genBatch_(){
        for(int v=0;v<8;v++){ genN_++; regen_(); fVars_[v]=gen_; }
        fVarSel=0; gen_=fVars_[0]; ni_=0; fStep=-1; fPhase=0;
        fSelMask=0; updateStateValue("selmask","0");
        char nb[64];std::snprintf(nb,sizeof(nb),"8 VARIANTEN klaar · V1 actief · %d noten",(int)gen_.size());
        updateStateValue("kit",nb);
        updateStateValue("midifile","preview — klik V = luister+vink; ↓MID bewaart aangevinkte, ALL bewaart alle 8");
    }
    void pickVar_(int v){
        if(v<0)v=0; if(v>7)v=7;
        if(fVars_[v].empty())return;
        fVarSel=v; gen_=fVars_[v]; ni_=0; fStep=-1; fPhase=0;
        fSelMask^=(1<<v);   // klik = beluister + markeer/ont-markeer
        char sm[8];std::snprintf(sm,sizeof(sm),"%d",fSelMask);updateStateValue("selmask",sm);
        char nb[64];std::snprintf(nb,sizeof(nb),"VARIANT V%d%s · %d noten",v+1,(fSelMask>>v)&1?" ✓":"",(int)gen_.size());
        updateStateValue("kit",nb);
    }
    // export-varianten V5-V8: forced fills aan elke zin-einde (evolutie = 100%)
    void evolveOut_(std::vector<BNote>&ns){
        const int bars=clampi(fBars,1,16);
        for(int b=0;b<bars;b++){ const int tEnd=(b+1)*1920;
            if((b%4==3)||(b==bars-1))
                for(int i=0;i<4;i++) ns.push_back({tEnd-480+i*120,60,9,38,(uint8_t)(70+i*14)}); // snare-opbouw fill
        }
        for(size_t i=1;i<ns.size();i++){BNote x=ns[i];size_t j=i;
            while(j&&ns[j-1].t>x.t){ns[j]=ns[j-1];j--;}ns[j]=x;}
    }

    // unieke naam-tag: HHMMSS — voorkomt overschrijven bij snelle exports
    static const char* stamp_(){
        static char s[8]; const time_t t=std::time(nullptr); const struct tm* tm=std::localtime(&t);
        std::snprintf(s,sizeof(s),"%02d%02d%02d",tm?tm->tm_hour:0,tm?tm->tm_min:0,tm?tm->tm_sec:0);
        return s;
    }

    // ---- ALL: alle 8 preview-varianten als .mid in eigen map ----
    void writeBatch_(){
        if(fVars_[0].empty())genBatch_();
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
            std::vector<BNote> nx=fVars_[v]; if(v>=4)evolveOut_(nx); // V5-V8 altijd volle evolutie
            smf::MidiFile mf; mf.setTicksPerQuarterNote(480);
            int seq=0;
            for(const BNote&n:nx){
                int vv=(int)(n.vel*fKracht*humCurve_(n.t)); vv=vv<20?20:(vv>127?127:vv);
                smf::MidiEvent on(0x90|n.ch,n.note,vv); on.tick=n.t+(std::rand()%11-5); on.seq=seq++;
                smf::MidiEvent off(0x80|n.ch,n.note,0); off.tick=n.t+n.dur; off.seq=seq++;
                mf[0].push_back(on); mf[0].push_back(off);
            }
            mf.sortTracks();
            char path[600];std::snprintf(path,sizeof(path),"%s/brein_%02d_%s.mid",dir,v+1,stamp_());
            if(mf.write(path))ok++;
            char wpath[600];std::snprintf(wpath,sizeof(wpath),"%s/brein_%02d_%s.wav",dir,v+1,stamp_());
            writeWav_(nx,fBpm,wpath);
        }
        char m[80];std::snprintf(m,sizeof(m),"%d .mid + .wav in Music/G2B/midigen",ok);
        updateStateValue("midifile",m);
#ifdef _WIN32
        char cmd[700]; std::snprintf(cmd,sizeof(cmd),"explorer \"%s\"",dir);
        std::system(cmd);
#endif
    }

    // ---- EXPORT .MID: aangevinkte varianten (of huidige) wegschrijven ----
    void writeMid_(){
        if(fSelMask){
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
            int ok=0,nSel=0;
            for(int v=0;v<8;v++){
                if(!((fSelMask>>v)&1)||fVars_[v].empty())continue;
                nSel++;
                std::vector<BNote> nx=fVars_[v]; if(v>=4)evolveOut_(nx); // V5-V8 volle evolutie
                smf::MidiFile mf; mf.setTicksPerQuarterNote(480);
                int seq=0;
                for(const BNote&n:nx){
                    int vv=(int)(n.vel*fKracht*humCurve_(n.t)); vv=vv<20?20:(vv>127?127:vv);
                    smf::MidiEvent on(0x90|n.ch,n.note,vv); on.tick=n.t+(std::rand()%11-5); on.seq=seq++;
                    smf::MidiEvent off(0x80|n.ch,n.note,0); off.tick=n.t+n.dur; off.seq=seq++;
                    mf[0].push_back(on); mf[0].push_back(off);
                }
                mf.sortTracks();
                char path[600];std::snprintf(path,sizeof(path),"%s/brein_%02d_%s.mid",dir,v+1,stamp_());
                if(mf.write(path))ok++;
                char wpath[600];std::snprintf(wpath,sizeof(wpath),"%s/brein_%02d_%s.wav",dir,v+1,stamp_());
                writeWav_(nx,fBpm,wpath);
            }
            char m[96];std::snprintf(m,sizeof(m),"%d .mid + .wav opgeslagen in Music/G2B/midigen",ok);
            updateStateValue("midifile",m);
#ifdef _WIN32
            if(ok){char cmd[700];std::snprintf(cmd,sizeof(cmd),"explorer \"%s\"",dir);std::system(cmd);}
#endif
            return;
        }
        smf::MidiFile mf; mf.setTicksPerQuarterNote(480);
        int seq=0;
        for(const BNote&n:gen_){
            if(n.ch==9&&fDrums<0.5f)continue;
            if(n.ch==0&&fMel<0.5f)continue;
            int v=(int)(n.vel*fKracht*humCurve_(n.t)); v=v<20?20:(v>127?127:v);
            smf::MidiEvent on(0x90|n.ch,n.note,v); on.tick=n.t+(std::rand()%11-5); on.seq=seq++;
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
        std::snprintf(path,sizeof(path),"%s\\G2B-Brein_%s.mid",dir,stamp_());
        if(mf.write(path)){
            char wpath[600];std::snprintf(wpath,sizeof(wpath),"%s\\G2B-Brein_%s.wav",dir,stamp_());
            writeWav_(gen_,fBpm,wpath);
            updateStateValue("midifile",".mid + .wav opgeslagen: Music\\G2B\\G2B-Brein");
#ifdef _WIN32
            char cmd[700]; std::snprintf(cmd,sizeof(cmd),"explorer \"%s\"",dir);
            std::system(cmd);
#endif
        }
        else updateStateValue("midifile",".mid NIET gelukt — map niet schrijfbaar");
    }

    // ---- WAV-bounce: render noten met ingebouwde mini-synth → stereo .wav ----
    static void putLE16(unsigned char*p,int v){p[0]=v&255;p[1]=(v>>8)&255;}
    static void putLE32(unsigned char*p,unsigned v){p[0]=v&255;p[1]=(v>>8)&255;p[2]=(v>>16)&255;p[3]=(v>>24)&255;}
    bool writeWav_(const std::vector<BNote>&notes,double bpm,const char* path){
        if(notes.empty())return false;
        if(bpm<40||bpm>300)bpm=120.0;
        const double sr=44100.0,spt=60.0/(bpm*480.0);
        int endT=0; for(const BNote&n:notes)if(n.t+n.dur>endT)endT=n.t+n.dur;
        const int n=(int)(endT*spt*sr+sr*1.5);
        std::vector<float> buf(n*2,0.f);
        for(const BNote&nt:notes){
            if(nt.ch==9&&fDrums<0.5f)continue; if(nt.ch==0&&fMel<0.5f)continue;
            // humanizer: ±5 ticks jitter + groove-curve (accent op kwart, rolls zachter)
            int jit=std::rand()%11-5;
            if(clampi(fGenre,0,4)==1)jit+=std::rand()%17-8; // KAWINA: extra losse timing
            const int pos16=(nt.t/120)%16;
            float vv=nt.vel*fKracht*humCurve_(nt.t)/127.f;
            if(pos16%4==0)vv*=1.15f; else if(nt.dur<110)vv*=0.75f;
            if(clampi(fGenre,0,4)==4&&nt.ch==9&&nt.note>=42&&nt.note<=44&&(pos16%2))vv*=0.7f; // TRAP hat-roll bounce
            const double t0=(nt.t+jit)*spt,dur=nt.dur*spt,v=vv;
            const int i0=(int)(t0*sr),i1=std::min(n-1,i0+(int)(dur*sr)+(int)(sr*1.2));
            const double fr=440.0*std::pow(2.0,(nt.note-69)/12.0);
            for(int i=i0;i<i1;i++){const double ph=(i-i0)/sr; double s=0;
                if(nt.ch==9){ // drum-synth op GM-noot
                    const int nn=nt.note;
                    if(nn<38)      s=std::sin(6.28318*(150.0*std::exp(-ph*28.0)+45.0)*ph)*std::exp(-ph*10.0);
                    else if(nn<42) s=(std::rand()%2000/1000.0-1.0)*std::exp(-ph*30.0);
                    else if(nn<46) s=(std::rand()%2000/1000.0-1.0)*std::exp(-ph*90.0)*0.6;
                    else           s=(std::rand()%2000/1000.0-1.0)*std::exp(-ph*18.0)*0.6;
                }else s=(2.0*std::fmod(ph*fr,1.0)-1.0)*std::exp(-ph*(dur>0?1.5/dur:4.0))*std::min(1.0,ph*80.0);
                buf[i*2]+=s*v; buf[i*2+1]+=s*v;
            }
        }
        float mx=0;for(int i=0;i<n;i++){const float a=std::fabs(buf[i*2]);if(a>mx)mx=a;}
        if(mx<1e-6f)return false;
        const float g=0.89f/mx;
        unsigned char h[44];std::memcpy(h,"RIFF",4);putLE32(h+4,36+n*4);
        std::memcpy(h+8,"WAVEfmt ",8);putLE32(h+16,16);putLE16(h+20,1);
        putLE16(h+22,2);putLE32(h+24,44100);putLE32(h+28,44100*4);putLE16(h+32,4);putLE16(h+34,16);
        std::memcpy(h+36,"data",4);putLE32(h+40,n*4);
        FILE*f=std::fopen(path,"wb");if(!f)return false;
        std::fwrite(h,1,44,f);
        for(int i=0;i<n;i++){unsigned char b2[4];
            int L=(int)(buf[i*2]*g*32767),R=(int)(buf[i*2+1]*g*32767);
            L=L>32767?32767:(L<-32768?-32768:L);R=R>32767?32767:(R<-32768?-32768:R);
            putLE16(b2,L&0xFFFF);putLE16(b2+2,R&0xFFFF);std::fwrite(b2,1,4,f);}
        std::fclose(f);return true;
    }

    static int clampi(float v,int a,int b){int x=(int)(v+0.5f);return x<a?a:(x>b?b:x);}

    float fGenre=0,fKit=0,fBars=4,fSeed=7,fKracht=.85f,fSwing=1.f;
    float fDrums=1,fMel=1,fMidi=1,fPlay=1,fLight=0;
    float fOn=1,fDim=8,fLeer=0,fQuant=1,fFill=1,fHum=.5f;
    std::vector<BNote> fVars_[8]; int fVarSel=0,fSelMask=0;
    std::vector<std::vector<BNote>> undoStk_;
    // GIORGIO leer-model
    int fLearnN=0,fLearnRoot=60;
    std::vector<int> fLearnIv[12]; std::vector<int> fLearnDur,fLearnRest;
    std::vector<int> fLearnCh9;
};

Plugin* createPlugin(){ return new BreinPlugin(); }
END_NAMESPACE_DISTRHO
