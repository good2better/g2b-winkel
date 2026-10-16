// G2B Sleutel-Helper — theory-assist MIDI utility.
// Luistert naar MIDI-in → detecteert toonsoort (Krumhansl-Schmuckler) → toont
// veilige noten / akkoorden / bass-808 wortels / G2B-pack suggesties per FOCUS.
// MIDI loopt ongemoeid door (THRU) — de plugin eet nooit noten.
// © G2B (good2better) — MIT.
#include "DistrhoPlugin.hpp"
#include "sleutel_data.h"
#include <cmath>
#include <cstring>
#include <cstdio>

START_NAMESPACE_DISTRHO

class SleutelPlugin : public Plugin
{
public:
    SleutelPlugin() : Plugin(15, 0, 0) { pushAna_(); }

protected:
    const char* getLabel() const override { return "G2B Sleutel-Helper"; }
    const char* getDescription() const override
    { return "G2B theory-assist: toonsoort-detectie + bass/akkoord/melodie hints + G2B-pack suggesties. MIDI-thru."; }
    const char* getMaker() const override { return "G2B (good2better)"; }
    const char* getHomePage() const override { return "https://github.com/good2better"; }
    const char* getLicense() const override { return "MIT"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','S','h'); }

    void initState(uint32_t, String&, String&) override {}
    void setState(const char*, const char*) override {}

    enum { kOn, kFocus, kKracht, kDim, kSkin, kClear, kThru, kHold,
           kOutKey, kOutConf, kOutMask, kOutFoc,
           kPlay, kOutChord, kOutHint };

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints = kParameterIsAutomatable;
        switch(i){
        case kOn:    p.name="AAN/UIT";      p.symbol="aan";    p.ranges.min=0; p.ranges.max=1;   p.ranges.def=1;    p.hints|=kParameterIsBoolean; break;
        case kFocus: p.name="FOCUS";        p.symbol="focus";  p.ranges.min=0; p.ranges.max=3;   p.ranges.def=0;    p.hints|=kParameterIsInteger; break;
        case kKracht:p.name="KRACHT";       p.symbol="kracht"; p.ranges.min=0; p.ranges.max=1;   p.ranges.def=.6f;  break;
        case kDim:   p.name="DIMENTIES";    p.symbol="dim";    p.ranges.min=0; p.ranges.max=26;  p.ranges.def=8;    p.hints|=kParameterIsInteger; break;
        case kSkin:  p.name="SKIN";         p.symbol="skin";   p.ranges.min=0; p.ranges.max=7;   p.ranges.def=0;    p.hints|=kParameterIsInteger; break;
        case kClear: p.name="WIS";          p.symbol="wis";    p.ranges.min=0; p.ranges.max=1;   p.hints|=kParameterIsTrigger; break;
        case kThru:  p.name="MIDI THRU";    p.symbol="thru";   p.ranges.min=0; p.ranges.max=1;   p.ranges.def=1;    p.hints|=kParameterIsBoolean; break;
        case kHold:  p.name="GEHEUGEN (s)"; p.symbol="hold";   p.ranges.min=4; p.ranges.max=120; p.ranges.def=30;   p.hints|=kParameterIsInteger; break;
        case kOutKey: p.name="key-out";  p.symbol="okey";  p.ranges.min=-1; p.ranges.max=23;   p.hints|=kParameterIsInteger|kParameterIsOutput|kParameterIsHidden; break;
        case kOutConf:p.name="conf-out"; p.symbol="oconf"; p.ranges.min=0;  p.ranges.max=99;   p.hints|=kParameterIsInteger|kParameterIsOutput|kParameterIsHidden; break;
        case kOutMask:p.name="mask-out"; p.symbol="omask"; p.ranges.min=0;  p.ranges.max=4095; p.hints|=kParameterIsInteger|kParameterIsOutput|kParameterIsHidden; break;
        case kOutFoc: p.name="foc-out";  p.symbol="ofoc";  p.ranges.min=0;  p.ranges.max=4095; p.hints|=kParameterIsInteger|kParameterIsOutput|kParameterIsHidden; break;
        case kPlay:  p.name="SPEEL HINT";  p.symbol="play";  p.ranges.min=0; p.ranges.max=1;   p.hints|=kParameterIsTrigger; break;
        case kOutChord:p.name="chord-out";p.symbol="ochord";p.ranges.min=0; p.ranges.max=4095; p.hints|=kParameterIsInteger|kParameterIsOutput|kParameterIsHidden; break;
        case kOutHint:p.name="hint-out";  p.symbol="ohint"; p.ranges.min=0; p.ranges.max=255;  p.hints|=kParameterIsInteger|kParameterIsOutput|kParameterIsHidden; break;
        }
    }

    float getParameterValue(uint32_t i) const override {
        switch(i){
        case kOn:return fOn; case kFocus:return fFocus; case kKracht:return fKracht;
        case kDim:return fDim; case kSkin:return fSkin; case kClear:return 0;
        case kThru:return fThru; case kHold:return fHold;
        case kOutKey:return key_<0?-1.f:(float)(key_*2+minor_);
        case kOutConf:return (float)conf_;
        case kOutMask:return (float)mask_;
        case kOutFoc:return (float)focusMask_;
        case kPlay:return 0;
        case kOutChord:return (float)chordOut_;
        case kOutHint:return (float)hintOut_;} return 0.f;
    }

    void setParameterValue(uint32_t i, float v) override {
        switch(i){
        case kOn:    fOn=v; pushAna_(); break;
        case kFocus: fFocus=v; pushAna_(); break;
        case kKracht:fKracht=v; pushAna_(); break;
        case kDim:   fDim=v; pushAna_(); break;
        case kSkin:  fSkin=v; break;
        case kClear: if(v>0.5f){nNotes_=0;lastChord_=-1;pushAna_();} break;
        case kThru:  fThru=v; break;
        case kHold:  fHold=v; break;
        case kPlay:  if(v>0.5f) playHint_(); break;
        }
    }

    void run(const float**, float** out, uint32_t frames,
             const MidiEvent* ev, uint32_t nEv) override
    {
        for(uint32_t i=0;i<frames;i++){out[0][i]=0.f;out[1][i]=0.f;}
        bool changed=false;
        for(uint32_t e=0;e<nEv;e++){
            const MidiEvent& m=ev[e];
            if(fThru>0.5f){                     // alles doorsluisbaar
                MidiEvent mo=m; mo.frame+=0;
                writeMidiEvent(mo);
            }
            if(m.size<3) continue;
            const uint8_t st=m.data[0]&0xF0;
            if(st==0x90 && m.data[2]>0){        // note-on
                noteOn_(m.data[1], m.data[2], fAbs+m.frame);
                changed=true;
            }
            else if(st==0x80 || (st==0x90 && m.data[2]==0)){  // note-off
                const int pc=m.data[1]%12;
                if(held_[pc]>0){ held_[pc]--; chordDirty_=true; }
            }
        }
        // uitgaande hint-events afspelen (SPEEL HINT)
        for(int i=0;i<nPend_;i++){
            Pend&p=pend_[i];
            if(!p.done && p.at<fAbs+frames){
                MidiEvent mo{}; mo.frame=(uint32_t)(p.at-fAbs); mo.size=3;
                mo.data[0]=p.st; mo.data[1]=p.note; mo.data[2]=p.vel;
                writeMidiEvent(mo); p.done=true;
            }
        }
        fAbs+=frames;
        if(changed) analyse_();
        if(chordDirty_){ chordDirty_=false; chordDetect_(); }
    }

private:
    struct NOn { uint8_t note, vel; uint64_t at; };
    NOn ring_[128]; int nNotes_=0, wpos_=0;
    uint64_t fAbs=0;
    float fOn=1,fFocus=0,fKracht=.6f,fDim=8,fSkin=0,fThru=1,fHold=30;
    int lastChord_=-1;
    // analyse-uitvoer
    int key_=-1, minor_=0, conf_=0;
    uint16_t mask_=0xFFF, focusMask_=0;
    char keyTxt_[32]="luister…";
    char sug1_[64]="", sug2_[64]="", sug3_[96]="";
    // feature 1+2: live-akkoord + buiten-toonsoort hint
    int held_[12]={0}; bool chordDirty_=false;
    int chordOut_=0;                                 // q*16+root  (q:0 geen,1 maj,2 min,3 dim,4 aug,5 sus4,6 7,7 maj7,8 m7)
    int hintOut_=0;                                  // fout*16+goed (0=geen)
    // feature 3: SPEEL HINT → midi-uit queue
    struct Pend { uint64_t at; uint8_t st,note,vel; bool done; };
    Pend pend_[64]; int nPend_=0;

    void noteOffHeld_(uint8_t n){ const int pc=n%12; if(held_[pc]>0)held_[pc]--; }

    // q-tabel: 1 maj{0,4,7} 2 min{0,3,7} 3 dim{0,3,6} 4 aug{0,4,8} 5 sus4{0,5,7} 6 7{0,4,7,10} 7 maj7{0,4,7,11} 8 m7{0,3,7,10}
    void chordDetect_(){
        static const int T[9][4]={{0,0,0,0},{0,4,7,-1},{0,3,7,-1},{0,3,6,-1},{0,4,8,-1},{0,5,7,-1},{0,4,7,10},{0,4,7,11},{0,3,7,10}};
        int n=0; for(int i=0;i<12;i++) if(held_[i]>0) n++;
        int bestQ=0,bestR=-1;
        if(n>=3){
            for(int r=0;r<12&&bestQ==0;r++)for(int q=1;q<=8;q++){
                bool ok=true;
                for(int k=1;k<4;k++){ const int iv=T[q][k]; if(iv<0)break;
                    if(held_[(r+iv)%12]==0){ok=false;break;} }
                if(ok && T[q][1]>0){ bestQ=q;bestR=r;break; }
            }
        }
        const int co=bestR<0?0:bestQ*16+bestR;
        if(co!=chordOut_){ chordOut_=co; requestParameterValueChange(kOutChord,(float)co); }
    }

    // buiten-de-toonsoort: stel dichtstbijzijnde veilige noot voor
    void hintCheck_(uint8_t note){
        const int pc=note%12;
        int h=0;
        if(mask_!=0xFFF && !(mask_>>pc&1)){
            int best=pc; for(int d=1;d<6;d++){
                if(mask_>>((pc+d)%12)&1){best=(pc+d)%12;break;}
                if(mask_>>((pc+12-d)%12)&1){best=(pc+12-d)%12;break;}
            }
            h=pc*16+best;
        }
        if(h!=hintOut_){ hintOut_=h; requestParameterValueChange(kOutHint,(float)h); }
    }

    // SPEEL HINT: huidige akkoord/bas-suggestie als MIDI-uit sturen
    void playHint_(){
        if(key_<0) return;
        nPend_=0;
        const uint64_t now=fAbs+32;
        const uint32_t sr=(uint32_t)getSampleRate();
        const uint64_t len=(uint64_t)(sr*6/5);             // ~1.2s
        if((int)fFocus==1){                              // BAS-808: root in oct 1-2
            const int n=36+key_;
            pend_[nPend_++]={now,0x90,(uint8_t)n,110,false};
            pend_[nPend_++]={now+len,0x80,(uint8_t)n,0,false};
        } else {                                          // triad oct 4 (+7e op hoge kracht)
            const int t3=minor_?3:4;
            const int notes[4]={48+key_,48+key_+t3,48+key_+7,48+key_+((fKracht>0.6f)?10:7)};
            const int cnt=(fKracht>0.6f)?4:3;
            for(int i=0;i<cnt;i++){
                pend_[nPend_++]={now,0x90,(uint8_t)notes[i],100,false};
                pend_[nPend_++]={now+len,0x80,(uint8_t)notes[i],0,false};
            }
        }
    }

    void noteOn_(uint8_t n, uint8_t v, uint64_t at){
        ring_[wpos_]={n,v,at}; wpos_=(wpos_+1)%128;
        if(nNotes_<128) nNotes_++;
        held_[n%12]++; chordDirty_=true;
        hintCheck_(n);
    }

    void analyse_(){
        if(fOn<0.5f){ std::snprintf(keyTxt_,32,"UIT"); pushAna_(); return; }
        const double sr=getSampleRate();
        const uint64_t win=(uint64_t)(fHold*sr);
        float hist[12]={0};
        for(int i=0;i<nNotes_;i++){
            const NOn&n=ring_[i];
            const uint64_t age=fAbs>n.at? fAbs-n.at:0;
            if(age>win) continue;
            const float rec=1.0f-(float)age/(float)(win?win:1);
            hist[n.note%12]+=(0.5f+0.5f*(n.vel/127.f))*(0.4f+0.6f*rec);
        }
        float tot=0; for(int i=0;i<12;i++) tot+=hist[i];
        if(tot<0.01f){ std::snprintf(keyTxt_,32,"luister…"); key_=-1; pushAna_(); return; }
        for(int i=0;i<12;i++) hist[i]/=tot;

        float best=-1e9f, second=-1e9f;
        for(int root=0;root<12;root++){
            float cMaj=0,cMin=0;
            for(int i=0;i<12;i++){
                const float h=hist[(root+i)%12];
                cMaj+=h*kProfMaj[i]; cMin+=h*kProfMin[i];
            }
            if(cMaj>best){second=best;best=cMaj;key_=root;minor_=0;}
            else if(cMaj>second) second=cMaj;
            if(cMin>best){second=best;best=cMin;key_=root;minor_=1;}
            else if(cMin>second) second=cMin;
        }
        conf_=(int)((best-second)/(best+1e-6f)*300.f); if(conf_>99)conf_=99;
        mask_=minor_?kMaskMin:kMaskMaj;
        if(minor_&&fKracht>0.5f) mask_=kMaskMinHarm;      // avontuurlijker: harmonisch
        std::snprintf(keyTxt_,32,"%s %s",kNoteNam[key_],minor_?"mineur":"majeur");

        buildSug_();
        pushAna_();
    }

    // focus-masker: welke toonhoogteklassen de piano-strip accentueert
    void buildSug_(){
        const int f=(int)fFocus;
        focusMask_=mask_;
        const int depth=1+(int)(fDim/3);                 // 27 standen → 1..9 hints
        char tmp[192]; tmp[0]=0;

        if(f==0||f==2){                                   // AKKOORDEN (+ALLES)
            std::strcat(tmp,"akkoorden: ");
            int shown=0;
            for(int d=0;d<7&&shown<depth;d++){
                const int pc=(key_+ (minor_?(int[]){0,2,3,5,7,8,10}[d]:(int[]){0,2,4,5,7,9,11}[d]))%12;
                const int q=minor_?kQualMin[d]:kQualMaj[d];
                char b[12]; std::snprintf(b,12,"%s%s ",kNoteNam[pc],q==1?"m":q==2?"°":"");
                std::strcat(tmp,b); shown++;
            }
            if(fKracht>0.55f){                            // geleende akkoorden
                const int br[3]={minor_?(key_+8)%12:(key_+3)%12, minor_?(key_+10)%12:(key_+8)%12, minor_?(key_+3)%12:(key_+10)%12};
                char b[48]; std::snprintf(b,48,"· leen: %s %s %s",kNoteNam[br[0]],kNoteNam[br[1]],kNoteNam[br[2]]);
                std::strcat(tmp,b);
            }
            std::snprintf(sug1_,64,"%.63s",tmp);
        }

        if(f==0||f==1){                                   // BAS-808 (+ALLES)
            const int degMin[4]={0,7,5,10};               // i, v, iv, bVII
            const int degMaj[4]={0,7,5,9};                // I, V, IV, vi
            char b[64]="808/bass roots: ";
            for(int i=0;i<(fKracht>0.5f?4:3);i++){
                const int pc=(key_+(minor_?degMin[i]:degMaj[i]))%12;
                std::strcat(b,kNoteNam[pc]); std::strcat(b," ");
            }
            std::strcat(b,"· laag houden: E1–B1");
            std::snprintf(sug2_,64,"%.63s",b);
            if(f==1) focusMask_=(1<<0)|(1<<5)|(1<<7)|(1<<(minor_?10:9));
        }

        if(f==3){                                         // MELODIE: pentatoniek markeren
            focusMask_=minor_?kMaskPentMin:kMaskPentMaj;
            std::snprintf(sug1_,64,"melodie: %s-pentatoniek veilig · %s-spanning voor kleur",
                          minor_?"mineur":"majeur", kNoteNam[(key_+(minor_?8:11))%12]);
        }

        // G2B pack-suggestie — uniek aan dit systeem
        std::snprintf(sug3_,96,"G2B %s",kG2BSug[(int)fFocus][minor_]);
    }

    void pushAna_(){
        // statusregels naar de UI — via state (hosts) én output-params (standalone)
        requestParameterValueChange(kOutKey, key_<0?-1.f:(float)(key_*2+minor_));
        requestParameterValueChange(kOutConf, (float)conf_);
        requestParameterValueChange(kOutMask, (float)mask_);
        requestParameterValueChange(kOutFoc, (float)focusMask_);
        requestParameterValueChange(kOutChord, (float)chordOut_);
        requestParameterValueChange(kOutHint, (float)hintOut_);
        char b[160];
        uint32_t m24=0;
        for(int pc=0;pc<12;pc++) if(mask_>>pc&1) m24|=(1u<<pc);
        uint32_t fm=0;
        for(int pc=0;pc<12;pc++) if(focusMask_>>pc&1) fm|=(1u<<pc);
        std::snprintf(b,sizeof(b),"%s|%d|%d|%u|%u",keyTxt_,conf_,minor_,m24,fm);
        updateStateValue("ana",b);
        updateStateValue("sug1",sug1_);
        updateStateValue("sug2",sug2_);
        updateStateValue("sug3",sug3_);
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SleutelPlugin)
};

Plugin* createPlugin() { return new SleutelPlugin(); }

END_NAMESPACE_DISTRHO
