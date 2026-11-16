// G2B Sleutel-Helper — theory-assist: toonsoort uit AUDIO én/of MIDI.
// Audio (loop/vocal/instrument) → chroma (FFT) → Krumhansl-Schmuckler key.
// MIDI-noten → zelfde key-detectie + live akkoord + foute-noot waarschuwing.
// Volgend-akkoord suggestie op basis van het huidige akkoord (trap-logica).
// Audio loopt ongemoeid door (insert), MIDI loopt door (THRU).
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
    SleutelPlugin() : Plugin(kNumParams, 0, 0) { initDsp_(getSampleRate()); }

protected:
    const char* getLabel() const override { return "G2B Sleutel-Helper"; }
    const char* getDescription() const override
    { return "G2B theory-assist: toonsoort uit audio of MIDI, foute-noot waarschuwing, volgend-akkoord, bas/akkoord/melodie hints."; }
    const char* getMaker() const override { return "G2B (good2better)"; }
    const char* getHomePage() const override { return "https://github.com/good2better"; }
    const char* getLicense() const override { return "MIT"; }
    uint32_t getVersion() const override { return d_version(2,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','S','h'); }

    void initState(uint32_t, String&, String&) override {}
    void setState(const char*, const char*) override {}

    enum { kOn, kFocus, kKracht, kDim, kSkin, kClear, kThru, kHold,
           kOutKey, kOutConf, kOutMask, kOutFoc,
           kPlay, kOutChord, kOutHint, kDemo,
           kBron, kOutPlayed, kOutNext, kOutLvl, kNumParams };

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints = kParameterIsAutomatable;
        switch(i){
        case kOn:    p.name="AAN/UIT";      p.symbol="aan";    p.ranges.min=0; p.ranges.max=1;   p.ranges.def=1;    p.hints|=kParameterIsBoolean; break;
        case kFocus: p.name="FOCUS";        p.symbol="focus";  p.ranges.min=0; p.ranges.max=3;   p.ranges.def=0;    p.hints|=kParameterIsInteger; break;
        case kKracht:p.name="KRACHT";       p.symbol="kracht"; p.ranges.min=0; p.ranges.max=1;   p.ranges.def=.6f;  break;
        case kDim:   p.name="DIMENTIES";    p.symbol="dim";    p.ranges.min=0; p.ranges.max=26;  p.ranges.def=8;    p.hints|=kParameterIsInteger; break;
        case kSkin:  p.name="SKIN";         p.symbol="skin";   p.ranges.min=0; p.ranges.max=7;   p.ranges.def=0;    p.hints|=kParameterIsInteger; break;
        case kClear: p.name="WIS";          p.symbol="wis";    p.ranges.min=0; p.ranges.max=1;   p.hints|=kParameterIsBoolean; break;
        case kThru:  p.name="MIDI THRU";    p.symbol="thru";   p.ranges.min=0; p.ranges.max=1;   p.ranges.def=1;    p.hints|=kParameterIsBoolean; break;
        case kHold:  p.name="GEHEUGEN (s)"; p.symbol="hold";   p.ranges.min=4; p.ranges.max=120; p.ranges.def=30;   p.hints|=kParameterIsInteger; break;
        case kOutKey: p.name="key-out";  p.symbol="okey";  p.ranges.min=-1; p.ranges.max=23;   p.hints|=kParameterIsInteger|kParameterIsOutput; break;
        case kOutConf:p.name="conf-out"; p.symbol="oconf"; p.ranges.min=0;  p.ranges.max=99;   p.hints|=kParameterIsInteger|kParameterIsOutput; break;
        case kOutMask:p.name="mask-out"; p.symbol="omask"; p.ranges.min=0;  p.ranges.max=4095; p.hints|=kParameterIsInteger|kParameterIsOutput; break;
        case kOutFoc: p.name="foc-out";  p.symbol="ofoc";  p.ranges.min=0;  p.ranges.max=4095; p.hints|=kParameterIsInteger|kParameterIsOutput; break;
        case kPlay:  p.name="SPEEL HINT";  p.symbol="play";  p.ranges.min=0; p.ranges.max=1;   p.hints|=kParameterIsBoolean; break;
        case kOutChord:p.name="chord-out";p.symbol="ochord";p.ranges.min=0; p.ranges.max=4095; p.hints|=kParameterIsInteger|kParameterIsOutput; break;
        case kOutHint:p.name="hint-out";  p.symbol="ohint"; p.ranges.min=0; p.ranges.max=255;  p.hints|=kParameterIsInteger|kParameterIsOutput; break;
        case kDemo:  p.name="DEMO";         p.symbol="demo";  p.ranges.min=0; p.ranges.max=1;   p.hints|=kParameterIsBoolean; break;
        case kBron:  p.name="BRON";         p.symbol="bron";  p.ranges.min=0; p.ranges.max=2;   p.ranges.def=0; p.hints|=kParameterIsInteger; break;
        case kOutPlayed:p.name="played-out";p.symbol="oplay";p.ranges.min=0; p.ranges.max=4095; p.hints|=kParameterIsInteger|kParameterIsOutput; break;
        case kOutNext:p.name="next-out";  p.symbol="onext"; p.ranges.min=0; p.ranges.max=60000; p.hints|=kParameterIsInteger|kParameterIsOutput; break;
        case kOutLvl: p.name="level-out"; p.symbol="olvl";  p.ranges.min=0; p.ranges.max=100;  p.hints|=kParameterIsInteger|kParameterIsOutput; break;
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
        case kPlay:return 0; case kDemo:return 0;
        case kOutChord:return (float)chordOut_;
        case kOutHint:return (float)hintOut_;
        case kBron:return fBron;
        case kOutPlayed:return (float)playedMask_();
        case kOutNext:return (float)nextOut_;
        case kOutLvl:return (float)lvlOut_;
        } return 0.f;
    }

    void setParameterValue(uint32_t i, float v) override {
        switch(i){
        case kOn:    fOn=v; analyse_(); break;
        case kFocus: fFocus=v; analyse_(); break;
        case kKracht:fKracht=v; analyse_(); break;
        case kDim:   fDim=v; analyse_(); break;
        case kSkin:  fSkin=v; break;
        case kClear: if(v>0.5f){ nNotes_=0; wpos_=0; std::memset(held_,0,sizeof(held_));
                       std::memset(aLong_,0,sizeof(aLong_)); std::memset(aShort_,0,sizeof(aShort_));
                       aEnergy_=0; hintOut_=0; chordOut_=0; curChord_=36; analyse_(); } break;
        case kThru:  fThru=v; break;
        case kHold:  fHold=v; break;
        case kPlay:  if(v>0.5f) playHint_(); break;
        case kDemo:  if(v>0.5f) demoPhrase_(); break;
        case kBron:  fBron=v; analyse_(); break;
        }
    }

    void sampleRateChanged(double sr) override { initDsp_(sr); }

    void run(const float** in, float** out, uint32_t frames,
             const MidiEvent* ev, uint32_t nEv) override
    {
        // audio ongemoeid door (insert-effect)
        if(out[0]!=in[0]) std::memcpy(out[0],in[0],frames*sizeof(float));
        if(out[1]!=in[1]) std::memcpy(out[1],in[1],frames*sizeof(float));

        bool changed=false;
        for(uint32_t e=0;e<nEv;e++){
            const MidiEvent& m=ev[e];
            if(fThru>0.5f) writeMidiEvent(m);
            if(m.size<3) continue;
            const uint8_t st=m.data[0]&0xF0;
            if(st==0x90 && m.data[2]>0){ noteOn_(m.data[1], m.data[2], fAbs+m.frame); changed=true; }
            else if(st==0x80 || (st==0x90 && m.data[2]==0)){
                const int pc=m.data[1]%12;
                if(held_[pc]>0){ held_[pc]--; chordDirty_=true; }
            }
        }
        for(int i=0;i<nPend_;i++){
            Pend&p=pend_[i];
            if(!p.done && p.at<fAbs+frames){
                MidiEvent mo{}; mo.frame=(uint32_t)(p.at>fAbs?p.at-fAbs:0); mo.size=3;
                mo.data[0]=p.st; mo.data[1]=p.note; mo.data[2]=p.vel;
                writeMidiEvent(mo); p.done=true;
            }
        }

        // audio-analyse: mono → lowpass → decimeren → FFT-chroma
        if(fOn>0.5f && (int)fBron!=1){
            float pk=0.f;
            for(uint32_t i=0;i<frames;i++){
                const float x=0.5f*(in[0][i]+in[1][i]);
                const float ax=std::fabs(x); if(ax>pk) pk=ax;
                lp1_+=lpA_*(x-lp1_); lp2_+=lpA_*(lp1_-lp2_);
                if(++decCnt_>=dec_){ decCnt_=0;
                    ring_a_[apos_]=lp2_; apos_=(apos_+1)&(kN-1);
                    if(++hopCnt_>=kHop){ hopCnt_=0; if(audioFrame_()) changed=true; }
                }
            }
            const float db=20.f*std::log10(pk+1e-9f);
            int lv=(int)((db+60.f)*100.f/60.f); if(lv<0)lv=0; if(lv>100)lv=100;
            lvlOut_= lv>lvlOut_? lv : (lvlOut_>2? lvlOut_-2 : 0);
        } else lvlOut_=0;

        fAbs+=frames;
        if(changed) analyse_();
        if(chordDirty_){ chordDirty_=false; chordDetect_(); }
        // foute-noot melding na ~1.5 s weer weg
        if(hintOut_ && fAbs>hintAt_+(uint64_t)(1.5*getSampleRate())) hintOut_=0;
    }

private:
    static constexpr int kN=4096, kHop=2048;
    struct NOn { uint8_t note, vel; uint64_t at; };
    NOn ring_[128]; int nNotes_=0, wpos_=0;
    uint64_t fAbs=0, hintAt_=0;
    float fOn=1,fFocus=0,fKracht=.6f,fDim=8,fSkin=0,fThru=1,fHold=30,fBron=0;
    int key_=-1, minor_=0, conf_=0;
    uint16_t mask_=0xFFF, focusMask_=0;
    int held_[12]={0}; bool chordDirty_=false;
    int chordOut_=0, hintOut_=0, curChord_=36, nextOut_=0, lvlOut_=0;
    struct Pend { uint64_t at; uint8_t st,note,vel; bool done; };
    Pend pend_[64]; int nPend_=0;

    // audio-DSP
    float ring_a_[kN]={0}, win_[kN], re_[kN], im_[kN];
    float binW_[kN/2]; int8_t binPc_[kN/2];
    float aLong_[12]={0}, aShort_[12]={0}, aEnergy_=0;
    float lp1_=0, lp2_=0, lpA_=0.5f, fsA_=11025.f;
    int dec_=4, decCnt_=0, apos_=0, hopCnt_=0;
    int audioChord_=36;

    void initDsp_(double sr){
        if(sr<8000) sr=44100;
        dec_=(int)std::lround(sr/11025.0); if(dec_<1) dec_=1;
        fsA_=(float)(sr/dec_);
        lpA_=1.f-std::exp(-2.f*(float)M_PI*2200.f/(float)sr);
        for(int i=0;i<kN;i++) win_[i]=0.5f-0.5f*std::cos(2.f*(float)M_PI*i/(kN-1));
        for(int k=0;k<kN/2;k++){
            const float f=k*fsA_/kN;
            binPc_[k]=-1; binW_[k]=0;
            if(f<60.f||f>2400.f) continue;
            const float m=12.f*std::log2(f/440.f)+69.f;
            const float r=std::round(m), dev=std::fabs(m-r);   // 0..0.5 halve-toon
            const float w=std::cos(dev*(float)M_PI); if(w<=0) continue;
            binPc_[k]=(int8_t)(((int)r%12+12)%12);
            binW_[k]=w*w*(f<200.f?0.6f:1.f);                   // bas iets minder dominant
        }
    }

    void fft_(){
        const int n=kN;
        for(int i=1,j=0;i<n;i++){ int b=n>>1; for(;j&b;b>>=1) j^=b; j^=b;
            if(i<j){ float t=re_[i];re_[i]=re_[j];re_[j]=t; t=im_[i];im_[i]=im_[j];im_[j]=t; } }
        for(int len=2;len<=n;len<<=1){
            const float ang=-2.f*(float)M_PI/len, wr=std::cos(ang), wi=std::sin(ang);
            for(int i=0;i<n;i+=len){ float cr=1,ci=0;
                for(int k=0;k<len/2;k++){
                    const int a=i+k,b=a+len/2;
                    const float xr=re_[b]*cr-im_[b]*ci, xi=re_[b]*ci+im_[b]*cr;
                    re_[b]=re_[a]-xr; im_[b]=im_[a]-xi; re_[a]+=xr; im_[a]+=xi;
                    const float t=cr*wr-ci*wi; ci=cr*wi+ci*wr; cr=t; } } }
    }

    // één analyse-venster: true als de toonsoort-schatting mogelijk veranderd is
    bool audioFrame_(){
        float rms=0;
        for(int i=0;i<kN;i++){ const float x=ring_a_[(apos_+i)&(kN-1)]; rms+=x*x; re_[i]=x*win_[i]; im_[i]=0; }
        rms=std::sqrt(rms/kN);
        const float hopT=(float)kHop/fsA_;
        const float dL=std::exp(-hopT/(fHold>1?fHold:1)), dS=std::exp(-hopT/0.8f);
        if(rms<0.003f){                                    // stilte: alleen laten wegzakken
            for(int c=0;c<12;c++){ aLong_[c]*=dL; aShort_[c]*=dS; }
            aEnergy_*=dL; return false;
        }
        fft_();
        float ch[12]={0}, tot=0;
        for(int k=1;k<kN/2;k++){ if(binPc_[k]<0) continue;
            const float mg=std::sqrt(std::sqrt(re_[k]*re_[k]+im_[k]*im_[k]));
            ch[binPc_[k]]+=mg*binW_[k]; }
        for(int c=0;c<12;c++) tot+=ch[c];
        if(tot<1e-6f) return false;
        float mn=1e9f; for(int c=0;c<12;c++){ ch[c]/=tot; if(ch[c]<mn) mn=ch[c]; }
        for(int c=0;c<12;c++) ch[c]=ch[c]-mn*0.8f;          // vlakke ruis/drums wegtrekken
        for(int c=0;c<12;c++){ aLong_[c]=aLong_[c]*dL+ch[c]; aShort_[c]=aShort_[c]*dS+ch[c]; }
        aEnergy_=aEnergy_*dL+1.f;
        audioChordDetect_();
        return true;
    }

    // akkoord uit korte audio-chroma (maj/min drieklanken)
    void audioChordDetect_(){
        float s=0; for(int c=0;c<12;c++) s+=aShort_[c];
        if(s<1e-6f){ audioChord_=36; return; }
        float best=-1, second=-1; int bc=36;
        for(int r=0;r<12;r++) for(int q=0;q<2;q++){
            const float v=aShort_[r]+aShort_[(r+(q?3:4))%12]+aShort_[(r+7)%12]
                         -0.5f*(aShort_[(r+(q?4:3))%12]+aShort_[(r+1)%12]);
            if(v>best){ second=best; best=v; bc=r*3+q; } else if(v>second) second=v;
        }
        audioChord_=(best/s>0.30f && best-second>0.02f*s)? bc : 36;
        bool anyHeld=false; for(int i=0;i<12;i++) if(held_[i]>0) anyHeld=true;
        if(!anyHeld && audioChord_!=36 && audioChord_!=curChord_){
            curChord_=audioChord_;
            const int q=audioChord_%3;
            chordOut_=(q==1?2:1)*16+audioChord_/3;
            buildNext_();
        }
    }

    uint16_t playedMask_() const { uint16_t m=0; for(int i=0;i<12;i++) if(held_[i]>0) m|=(uint16_t)(1u<<i); return m; }

    // q-tabel: 1 maj 2 min 3 dim 4 aug 5 sus4 6 7 7 maj7 8 m7
    void chordDetect_(){
        static const int T[9][4]={{0,0,0,0},{0,4,7,-1},{0,3,7,-1},{0,3,6,-1},{0,4,8,-1},{0,5,7,-1},{0,4,7,10},{0,4,7,11},{0,3,7,10}};
        int n=0; for(int i=0;i<12;i++) if(held_[i]>0) n++;
        int bestQ=0,bestR=-1;
        if(n>=3){
            for(int q=8;q>=1&&bestR<0;q--) for(int r=0;r<12;r++){   // 4-klanken eerst
                bool ok=true;
                for(int k=0;k<4;k++){ const int iv=T[q][k]; if(iv<0)break;
                    if(held_[(r+iv)%12]==0){ok=false;break;} }
                if(ok){ bestQ=q;bestR=r;break; }
            }
        }
        if(bestR>=0){
            chordOut_=bestQ*16+bestR;
            const int q3=(bestQ==2||bestQ==8)?1:(bestQ==3?2:0);
            curChord_=bestR*3+q3;
            buildNext_();
        }
    }

    // volgend akkoord: trap van het huidige akkoord → 3 gangbare vervolgen
    void buildNext_(){
        if(key_<0){ nextOut_=0; return; }
        const int* step=minor_?kStepMin:kStepMaj;
        int deg=-1;
        if(curChord_<36){ const int r=curChord_/3;
            for(int d=0;d<7;d++) if((key_+step[d])%12==r){ deg=d; break; } }
        const int (*tab)[3]=minor_?kNextMin:kNextMaj;
        static const int homeMin[3]={0,4,5}, homeMaj[3]={0,4,3};
        const int* nx= deg>=0? tab[deg] : (minor_? homeMin : homeMaj);
        int code[3];
        for(int i=0;i<3;i++){
            const int d=nx[i]; const int r=(key_+step[d])%12;
            int q=minor_?kQualMin[d]:kQualMaj[d];
            if(minor_&&d==4&&fKracht>0.5f) q=0;               // harmonisch mineur: V majeur
            code[i]=r*3+q;
        }
        nextOut_=1+code[0]+37*code[1]+1369*code[2];
    }

    void hintCheck_(uint8_t note){
        const int pc=note%12;
        if(key_<0 || conf_<8) return;
        if(!(mask_>>pc&1)){
            int best=pc; for(int d=1;d<6;d++){
                if(mask_>>((pc+d)%12)&1){best=(pc+d)%12;break;}
                if(mask_>>((pc+12-d)%12)&1){best=(pc+12-d)%12;break;}
            }
            hintOut_=pc*16+best+1;   // +1: C→C# blijft ≠ 0
            hintAt_=fAbs;
        } else if(hintOut_ && fAbs>hintAt_+(uint64_t)(0.25*getSampleRate())) hintOut_=0;
    }

    void playHint_(){
        if(key_<0) return;
        nPend_=0;
        const uint64_t now=fAbs+32;
        const uint64_t len=(uint64_t)(getSampleRate()*1.2);
        if((int)fFocus==1){
            const int n=36+key_;
            pend_[nPend_++]={now,0x90,(uint8_t)n,110,false};
            pend_[nPend_++]={now+len,0x80,(uint8_t)n,0,false};
        } else {
            const int t3=minor_?3:4;
            const int notes[4]={48+key_,48+key_+t3,48+key_+7,48+key_+((fKracht>0.6f)?10:7)};
            const int cnt=(fKracht>0.6f)?4:3;
            for(int i=0;i<cnt;i++){
                pend_[nPend_++]={now,0x90,(uint8_t)notes[i],100,false};
                pend_[nPend_++]={now+len,0x80,(uint8_t)notes[i],0,false};
            }
        }
    }

    void demoPhrase_(){
        const uint32_t sr=(uint32_t)getSampleRate();
        const int seq[10]={57,60,64,62,60,57,55,57,60,64};
        for(int i=0;i<10;i++){ ring_[wpos_]={(uint8_t)seq[i],100,fAbs+(uint64_t)(i*sr/6)}; wpos_=(wpos_+1)%128; if(nNotes_<128)nNotes_++; }
        analyse_();
        curChord_=key_>=0? key_*3+(minor_?1:0) : 36;
        chordOut_=key_>=0? (minor_?2:1)*16+key_ : 0;
        buildNext_();
    }

    void noteOn_(uint8_t n, uint8_t v, uint64_t at){
        ring_[wpos_]={n,v,at}; wpos_=(wpos_+1)%128;
        if(nNotes_<128) nNotes_++;
        held_[n%12]++; chordDirty_=true;
        hintCheck_(n);
    }

    void analyse_(){
        if(fOn<0.5f){ key_=-1; conf_=0; mask_=0xFFF; focusMask_=0; nextOut_=0; return; }
        const int bron=(int)fBron;                         // 0 AUTO · 1 MIDI · 2 AUDIO
        float hist[12]={0};
        if(bron!=2){
            const double sr=getSampleRate();
            const uint64_t win=(uint64_t)(fHold*sr);
            float mh[12]={0}, mt=0;
            for(int i=0;i<nNotes_;i++){
                const NOn&n=ring_[i];
                const uint64_t age=fAbs>n.at? fAbs-n.at:0;
                if(age>win) continue;
                const float rec=1.0f-(float)age/(float)(win?win:1);
                mh[n.note%12]+=(0.5f+0.5f*(n.vel/127.f))*(0.4f+0.6f*rec);
            }
            for(int i=0;i<12;i++) mt+=mh[i];
            if(mt>0.01f) for(int i=0;i<12;i++) hist[i]+=mh[i]/mt;
        }
        if(bron!=1 && aEnergy_>0.5f){
            float at=0; for(int i=0;i<12;i++) at+=aLong_[i];
            if(at>1e-6f) for(int i=0;i<12;i++) hist[i]+=aLong_[i]/at;
        }
        float tot=0; for(int i=0;i<12;i++) tot+=hist[i];
        if(tot<0.01f){ key_=-1; conf_=0; mask_=0xFFF; focusMask_=0; nextOut_=0; return; }
        for(int i=0;i<12;i++) hist[i]/=tot;

        float best=-1e9f, second=-1e9f; int bk=-1, bm=0;
        for(int root=0;root<12;root++){
            float cMaj=0,cMin=0;
            for(int i=0;i<12;i++){ const float h=hist[(root+i)%12]; cMaj+=h*kProfMaj[i]; cMin+=h*kProfMin[i]; }
            if(cMaj>best){second=best;best=cMaj;bk=root;bm=0;} else if(cMaj>second) second=cMaj;
            if(cMin>best){second=best;best=cMin;bk=root;bm=1;} else if(cMin>second) second=cMin;
        }
        key_=bk; minor_=bm;
        conf_=(int)((best-second)/(best+1e-6f)*300.f); if(conf_>99)conf_=99;
        mask_=minor_?kMaskMin:kMaskMaj;
        if(minor_&&fKracht>0.5f) mask_=kMaskMinHarm;
        // mask relatief → absoluut
        uint16_t abs_=0; for(int i=0;i<12;i++) if(mask_>>i&1) abs_|=(uint16_t)(1u<<((key_+i)%12));
        const int f=(int)fFocus;
        uint16_t fr=abs_;
        if(f==1){ const int dg[4]={0,7,5,minor_?10:9}; fr=0; for(int i=0;i<4;i++) fr|=(uint16_t)(1u<<((key_+dg[i])%12)); }
        else if(f==3){ const uint16_t pm=minor_?kMaskPentMin:kMaskPentMaj; fr=0; for(int i=0;i<12;i++) if(pm>>i&1) fr|=(uint16_t)(1u<<((key_+i)%12)); }
        mask_=abs_; focusMask_=fr;
        buildNext_();
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SleutelPlugin)
};

Plugin* createPlugin() { return new SleutelPlugin(); }

END_NAMESPACE_DISTRHO
