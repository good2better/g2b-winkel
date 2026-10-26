#include "mel_data.h"
#include "mar_data.h"
#include "sax_data.h"
#include "glock_data.h"
#include "rec_data.h"
#include "oca_data.h"
#include "har_data.h"
#include "bra_data.h"
#include "obo_data.h"
#include "bsn_data.h"
#include "pic_data.h"
#include "../g2b_verb.h"
#include "DistrhoPlugin.hpp"
#include "../g2b_std.h"
#include <cstring>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <cstdio>

START_NAMESPACE_DISTRHO

static const char* kPresetNames[17] = {"G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"};

// melodie-patronen: 16 steps, waarde = scale-degree (99 = rust)
// de getoonde noot = key + scale[degree] (+oct-shift bij degree>6)
static const int kMelPat[17][16] = {
    {0,99,2,99, 1,99,0,4,  99,3,99,2, 1,0,99,99},      // G2B — rustige hook
    {0,0,99,2, 3,99,2,0,  4,4,99,3,  2,99,1,0},       // DEMBOW — springerig
    {0,99,0,99, 0,2,1,0,  99,0,99,0,  1,0,99,99},     // DRILL — donker minimaal
    {0,99,4,3, 2,99,1,0,  2,3,4,99,  99,4,3,2},       // RNB — flowend
    {0,2,4,99, 0,2,4,99,  5,4,2,0,  99,2,4,99},       // HOUSE — repeterend
    {0,99,3,2, 0,99,4,3,  2,0,3,99,  4,3,2,0},        // AFRO — syncope
    {0,99,99,99, 2,99,99,1, 0,99,99,99, 3,2,99,99},   // TRAP — spaarzaam donker
    {0,2,99,99, 0,2,99,99, 4,2,99,99, 0,2,99,99},     // BOOM — vaste herhaling
    {0,99,4,99, 0,3,99,2,  99,0,4,99, 2,3,99,0},      // UKG — hakkend syncope
    {0,2,4,2, 99,0,2,4,  3,99,4,3,  2,0,99,2},        // JERSEY — druk triplet
    {0,99,2,4, 99,0,3,2,  99,4,0,2,  1,99,0,99},      // BOUNCE — springend call
    {0,99,99,3, 99,99,2,99, 0,99,99,2, 1,0,99,99},    // SOUL — lang uitgehouden
    {0,2,99,1, 0,99,2,99, 3,99,2,0,  99,1,0,99},      // LOFI — lome stappen
    {0,4,0,2, 4,0,2,0,  3,2,4,2,  0,2,0,4},           // FUNK — octaaf-sprongen
    {0,99,99,0, 3,99,99,2, 0,99,99,0, 2,3,99,99},     // REGGAE — offbeat rust
    {0,2,0,2, 4,2,0,2,  0,2,4,2,  0,4,2,0},           // DANCE — vast pumpend
    {0,3,99,0, 2,0,3,99, 4,99,0,3,  2,0,3,99}         // KAWINA — rollende lead
};

// stijl-uint: TIMBRE, SWING, LEVEL
static const float kPresetParams[17][3] = {
    {0.35f,0.08f,0.8f},{0.15f,0.10f,0.85f},{0.30f,0.04f,0.8f},{0.60f,0.10f,0.8f},{0.20f,0.02f,0.85f},{0.28f,0.12f,0.8f},
    {0.30f,0.04f,0.82f},{0.25f,0.06f,0.8f},{0.45f,0.12f,0.82f},{0.35f,0.10f,0.85f},{0.40f,0.09f,0.82f},
    {0.55f,0.11f,0.78f},{0.30f,0.14f,0.75f},{0.45f,0.10f,0.82f},{0.50f,0.13f,0.8f},{0.35f,0.06f,0.85f},{0.40f,0.11f,0.85f}
};

static const int kScales[2][7] = {
    {0,2,3,5,7,8,10},
    {0,2,4,5,7,9,11},
};

struct MVoice { bool active=false; float freq=0,phase=0,amp=0,vel=0; int age=0; float lp=0; const int16_t* smp=nullptr; uint32_t slen=0; float spos=0,sstep=0,sstepT=0; const int16_t* smp2=nullptr; uint32_t slen2=0; float spos2=0,sstep2=0; const int16_t* smp3=nullptr; uint32_t slen3=0; float spos3=0,sstep3=0; };

class MelodiePlugin : public Plugin {
public:
    enum Params { kStijl, kKey, kScale, kOctave, kTimbre, kSwing, kLevel, kDice, kAuto, kPreset, kOn, kKracht, kGalaxy, kSfeer, kPlay, kNumParams };
    MelodiePlugin() : Plugin(kNumParams,0,2) {}
protected:
    const char* getLabel() const override { return "G2B Melodie"; }
    const char* getDescription() const override { return "Melodie-generator: schrijft hooks in jouw stijl, altijd in key"; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','M','d'); }

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints=kParameterIsAutomatable;
        switch(i){
        case kStijl: p.name="PATROON";p.symbol="stijl";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=16;p.ranges.def=0;break;
        case kKey:   p.name="KEY";    p.symbol="key";  p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=11;p.ranges.def=0;break;
        case kScale: p.name="SCALE";  p.symbol="scale";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kOctave:p.name="OCTAVE"; p.symbol="octave";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=2;p.ranges.def=1;break;
        case kTimbre:p.name="TIMBRE"; p.symbol="timbre";p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.35f;break;
        case kSwing: p.name="SWING";  p.symbol="swing";p.ranges.min=0;p.ranges.max=0.5f;p.ranges.def=0.08f;break;
        case kLevel: p.name="LEVEL";  p.symbol="level";p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.8f;break;
        case kDice:  p.name="DICE";   p.symbol="dice"; p.hints|=kParameterIsInteger|kParameterIsTrigger;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kAuto:  p.name="AUTO";   p.symbol="auto"; p.hints|=kParameterIsBoolean|kParameterIsInteger;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kPreset:p.name="PRESET"; p.symbol="preset";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=16;p.ranges.def=0;break;
        case kOn:    p.name="Aan";      p.symbol="aan";   p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;break;
        case kPlay:  p.name="Loop";    p.symbol="play";  p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;break;
        case kKracht:p.name="Kracht";   p.symbol="kracht";p.ranges.min=0;p.ranges.max=100;p.ranges.def=100;p.unit="%";break;
        case kGalaxy:p.name="DIMENTIES";p.symbol="dim";   p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=26;p.ranges.def=9;break;
        case kSfeer: p.name="Sfeer";    p.symbol="sfeer"; p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=2;p.ranges.def=1;break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){case kStijl:return fStijl;case kKey:return fKey;case kScale:return fScale;case kOctave:return fOctave;case kTimbre:return fTimbre;case kSwing:return fSwing;case kLevel:return fLevel;case kDice:return fDice;case kAuto:return fAuto;case kPreset:return fPreset;case kOn:return fOn;case kKracht:return fKracht;case kGalaxy:return fGalaxyV;case kSfeer:return fSfeerV;case kPlay:return fPlay;}return 0.f;
    }
    void setParameterValue(uint32_t i, float v) override {
        if(i==kPreset){applyPreset((int)(v+0.5f));return;}
        switch(i){case kStijl:fStijl=v;break;case kKey:fKey=v;break;case kScale:fScale=v;break;case kOctave:fOctave=v;break;case kTimbre:fTimbre=v;break;case kSwing:fSwing=v;break;case kLevel:fLevel=v;break;case kDice:fDice=v;if(v>0.5f)mutate();fDice=0.f;break;case kAuto:fAuto=v;break;
        case kOn:fOn=v;break;case kKracht:fKracht=v;break;
        case kGalaxy:fGalaxyV=v;fGal.setPos((int)(v+0.5f));break;
        case kSfeer:fSfeerV=v;fSf.setMode((int)(v+0.5f));break;
        case kPlay:fPlay=v;break;}
    }
    void initState(uint32_t index, String& name, String& def) override {
        if(index==0){name="scope";def="";}
        else if(index==1){name="step";def="";}
        else{name="presetnames";def="";for(int i=0;i<6;i++){def+=kPresetNames[i];if(i<5)def+=";";}}
    }
    void setState(const char* k, const char* v) override {(void)k;(void)v;}

    void triggerDegree(int degree, float vel){
        // degree → midi-noot in key+scale+octave
        int sc=(int)fScale<0?0:((int)fScale>1?1:(int)fScale);
        int oct=degree/7, st=degree%7;
        triggerMidi(60+(int)fKey+kScales[sc][st]+oct*12+((int)fOctave-1)*12, vel);
    }
    void triggerMidi(int midi, float vel){
        float f=440.f*powf(2.f,(midi-69)/12.f);
        for(auto& lg:fV){ // LEGATO: actieve sample-voice glijdt door naar de nieuwe noot (geen re-attack)
            if(lg.active&&lg.smp&&fTimbre>=0.46f){
                lg.sstepT=lg.sstep*(f/lg.freq); lg.freq=f; lg.vel=vel; return;
            }
        }
        for(auto& v:fV){
            if(!v.active){v.active=true;v.freq=f;v.phase=0;v.amp=vel;v.vel=vel;v.age=0;v.lp=0;
                v.smp=nullptr;v.smp2=nullptr;v.smp3=nullptr;
                if(fTimbre>=0.96f){ // PICCOLO: hoge sparkle-staccato
                    int pi=0,pd=99; for(int k=0;k<kPicCount;k++){int d=std::abs(kPic[k].midi-midi);if(d<pd){pd=d;pi=k;}}
                    const PicSmp&pp=kPic[pi];v.smp=pp.d;v.slen=pp.len;
                    const float fs=440.f*powf(2.f,(pp.midi-69)/12.f);
                    v.spos=0;v.sstep=(f/fs)*(22050.f/getSampleRate());v.vel=vel; }
                else if(fTimbre>=0.92f){ // OBOE: echte hobo-staccato
                    int oi=0,od=99; for(int k=0;k<kOboCount;k++){int d=std::abs(kObo[k].midi-midi);if(d<od){od=d;oi=k;}}
                    const OboSmp&po=kObo[oi];v.smp=po.d;v.slen=po.len;
                    const float fs=440.f*powf(2.f,(po.midi-69)/12.f);
                    v.spos=0;v.sstep=(f/fs)*(22050.f/getSampleRate());v.vel=vel; }
                else if(fTimbre>=0.86f){ // SAXELLO: vl1/vl2 x rr staccato (VCSL)
                    int bi=0,bd=99;for(int k=0;k<kSaxelloCount;k++){int d=std::abs(kSaxello[k].midi-midi);if(d<bd){bd=d;bi=k;}}
                    const SaxelloSmp&ps=kSaxello[bi];const int lv=vel<0.5f?0:(vel<0.75f?1:(fRR++&1?2:3));
                    v.smp=lv==0?ps.d:(lv==1?ps.d2:(lv==2?ps.d3:ps.d4));
                    v.slen=lv==0?ps.len:(lv==1?ps.len2:(lv==2?ps.len3:ps.len4));
                    const float fs=440.f*powf(2.f,(ps.midi-69)/12.f);
                    v.spos=0;v.sstep=(f/fs)*(22050.f/getSampleRate());
                }
                else if(fTimbre>=0.88f){ // BRASS: trompet+trombone+hoorn (brass-section)
                    int bi=0,bd=99; for(int k=0;k<kBraCount;k++){int d=std::abs(kBra[k].midi-midi);if(d<bd){bd=d;bi=k;}}
                    const BraSmp&ps=kBra[bi];v.smp=ps.d;v.slen=ps.len;
                    const float fs=440.f*powf(2.f,(ps.midi-69)/12.f);
                    v.spos=0;v.sstep=(f/fs)*(22050.f/getSampleRate());
                    int b2i=0,b2d=99; for(int k=0;k<kTbnCount;k++){int d=std::abs(kTbn[k].midi-(midi-12));if(d<b2d){b2d=d;b2i=k;}}
                    const TbnSmp&pt=kTbn[b2i];v.smp2=pt.d;v.slen2=pt.len;
                    const float fs2=440.f*powf(2.f,(pt.midi+12-69)/12.f);
                    v.spos2=0;v.sstep2=(f/fs2)*(22050.f/getSampleRate());
                    int b3i=0,b3d=99; for(int k=0;k<kHnCount;k++){int d=std::abs(kHn[k].midi-(midi-7));if(d<b3d){b3d=d;b3i=k;}}
                    const HnSmp&ph=kHn[b3i];v.smp3=ph.d;v.slen3=ph.len;
                    const float fs3=440.f*powf(2.f,(ph.midi+7-69)/12.f);
                    v.spos3=0;v.sstep3=(f/fs3)*(22050.f/getSampleRate());
                } else if(fTimbre>=0.84f){ // BASSOON: donkere houten laag
                    int bi=0,bd=99; for(int k=0;k<kBsnCount;k++){int d=std::abs(kBsn[k].midi-midi);if(d<bd){bd=d;bi=k;}}
                    const BsnSmp&pb=kBsn[bi];v.smp=pb.d;v.slen=pb.len;
                    const float fs=440.f*powf(2.f,(pb.midi-69)/12.f);
                    v.spos=0;v.sstep=(f/fs)*(22050.f/getSampleRate());v.vel=vel;
                } else if(fTimbre>=0.80f){ // HARMONICA echte Hohner Special20 (bluesy)
                    int bi=0,bd=99; for(int k=0;k<kHarCount;k++){int d=std::abs(kHar[k].midi-midi);if(d<bd){bd=d;bi=k;}}
                    const HarSmp&ps=kHar[bi];v.smp=ps.d;v.slen=ps.len;
                    const float fs=440.f*powf(2.f,(ps.midi-69)/12.f);
                    v.spos=0;v.sstep=(f/fs)*(22050.f/getSampleRate());
                } else if(fTimbre>=0.76f){ // OCARINA echte fluit van klei
                    int bi=0,bd=99; for(int k=0;k<kOcaCount;k++){int d=std::abs(kOca[k].midi-midi);if(d<bd){bd=d;bi=k;}}
                    const OcaSmp&ps=kOca[bi];v.smp=ps.d;v.slen=ps.len;
                    const float fs=440.f*powf(2.f,(ps.midi-69)/12.f);
                    v.spos=0;v.sstep=(f/fs)*(22050.f/getSampleRate());
                } else if(fTimbre>=0.70f){ // GLOCK echte klokkenspel
                    int bi=0,bd=99; for(int k=0;k<7;k++){int d=std::abs(kGlock[k].midi-midi);if(d<bd){bd=d;bi=k;}}
                    const GkSmp&ps=kGlock[bi];v.smp=ps.d;v.slen=ps.len;
                    float fs=440.f*powf(2.f,(ps.midi-69)/12.f);v.spos=0;v.sstep=(f/fs)*(22050.f/getSampleRate());
                } else if(fTimbre>=0.64f){ // FLUIT echte barok-altblokfluit
                    int bi=0,bd=99; for(int k=0;k<kRecCount;k++){int d=std::abs(kRec[k].midi-midi);if(d<bd){bd=d;bi=k;}}
                    const RecSmp&ps=kRec[bi];v.smp=ps.d;v.slen=ps.len;
                    float fs=440.f*powf(2.f,(ps.midi-69)/12.f);v.spos=0;v.sstep=(f/fs)*(22050.f/getSampleRate());
                } else if(fTimbre>=0.58f){ // SAX echte tenor-saxofoon
                    int bi=0,bd=99; for(int k=0;k<8;k++){int d=std::abs(kSax[k].midi-midi);if(d<bd){bd=d;bi=k;}}
                    const SaxSmp&ps=kSax[bi];v.smp=ps.d;v.slen=ps.len;
                    float fs=440.f*powf(2.f,(ps.midi-69)/12.f);v.spos=0;v.sstep=(f/fs)*(22050.f/getSampleRate());
                } else if(fTimbre>=0.52f){ // PSALTERY echte geplukte snaren
                    int bi=0,bd=99; for(int k=0;k<8;k++){int d=std::abs(kPsal[k].midi-midi);if(d<bd){bd=d;bi=k;}}
                    const MelSmp&ps=kPsal[bi];v.smp=ps.d;v.slen=ps.len;
                    const float fs=440.f*powf(2.f,(ps.midi-69)/12.f);
                    v.spos=0;v.sstep=(f/fs)*(22050.f/getSampleRate());
                } else if(fTimbre>=0.46f){ // MARIMBA echte klank, 2 lagen op velocity
                    int bi=0,bd=99; for(int k=0;k<10;k++){int d=std::abs(kMar[k].midi-midi);if(d<bd){bd=d;bi=k;}}
                    const MarSmp&ps=kMar[bi];const int lv=vel<0.45f?0:(vel<0.78f?1:2);v.smp=lv==0?ps.d:(lv==1?ps.d2:ps.d3);v.slen=lv==0?ps.len:(lv==1?ps.len2:ps.len3);
                    const float fs=440.f*powf(2.f,(ps.midi-69)/12.f);
                    v.spos=0;v.sstep=(f/fs)*(22050.f/getSampleRate());
                }
                return;}
        }
        fV[0].active=true;fV[0].freq=f;fV[0].phase=0;fV[0].amp=vel;fV[0].vel=vel;fV[0].age=0;
    }

    void run(const float** in, float** out, uint32_t frames, const MidiEvent* evs, uint32_t nev) override {
        for(uint32_t m=0;m<nev;m++){const MidiEvent&e=evs[m];
            if(e.size>=3&&(e.data[0]&0xF0)==0x90&&e.data[2]>0)triggerMidi(e.data[1],e.data[2]/127.f);}
        const float sr=getSampleRate();if(!fVerbOn){fVerb.init(sr);fVerbOn=true;}
        if(fOn<0.5f){std::memset(out[0],0,frames*sizeof(float));std::memset(out[1],0,frames*sizeof(float));return;}
        double bpm=100.0;
        const TimePosition& tp=getTimePosition();
        if(tp.playing && tp.bbt.valid && tp.bbt.beatsPerMinute>1.0) bpm=tp.bbt.beatsPerMinute;
        const bool hostHasTransport=tp.bbt.valid;
        const bool playNow=fPlay>0.5f&&(!hostHasTransport||tp.playing); // host-gate: stopt zodra de host stopt; losse exe speelt door
        const double stepLen=60.0/bpm/4.0;
        const int st=(int)fStijl<0?0:((int)fStijl>16?16:(int)fStijl);
        if(!fPatInit){memcpy(fPat,kMelPat[st],sizeof(fPat));fPatInit=true;fLastStijl=st;}
        if(st!=fLastStijl){memcpy(fPat,kMelPat[st],sizeof(fPat));fLastStijl=st;}
        const float lpA=1.f-expf(-2.f*M_PI*(800.f+fTimbre*6000.f)/sr);
        const float decRate=expf(-1.f/( (0.3f+fTimbre*1.6f)*sr ));
        for(uint32_t i=0;i<frames;i++){
            if(playNow)fPhase+=1.0/sr; else fPhase=0;
            double stepDur=stepLen;
            int si=(int)fStep%16;
            if(si%2==1) stepDur*=1.0+fSwing*0.8;
            if(fPhase>=stepDur){
                fPhase=0;fStep++;
                int s=(int)fStep%16;
                if(fAuto>0.5f && s==0 && (fStep%64)==0) mutate(1);
                int deg=fPat[s];
                if(fPlay>0.5f&&deg!=99) triggerDegree(deg,0.85f);
                char sb[12];snprintf(sb,sizeof(sb),"%d:%d",s,deg==99?-1:deg);setState("step",sb);
            }
            float o=0.f;
            for(auto& v:fV){
                if(!v.active)continue;
                if(v.smp){ // echte psaltery-sample
                    uint32_t i2=(uint32_t)v.spos;
                    if(i2>=v.slen-1){v.active=false;continue;}
                    float fr=v.spos-i2;
                    o+=(v.smp[i2]+fr*(v.smp[i2+1]-v.smp[i2]))/32768.f*7.f*v.amp;
                    v.spos+=v.sstep;
                    if(v.sstepT>0.f){ v.sstep+=(v.sstepT-v.sstep)*0.0015f; if(std::abs(v.sstepT-v.sstep)<0.0001f)v.sstepT=0; }
                    if(v.smp2){ uint32_t j2=(uint32_t)v.spos2; if(j2<v.slen2-1){ float fr2=v.spos2-j2; o+=(v.smp2[j2]+fr2*(v.smp2[j2+1]-v.smp2[j2]))/32768.f*7.f*v.amp*0.7f; v.spos2+=v.sstep2; } else v.smp2=nullptr; }
                    if(v.smp3){ uint32_t j3=(uint32_t)v.spos3; if(j3<v.slen3-1){ float fr3=v.spos3-j3; o+=(v.smp3[j3]+fr3*(v.smp3[j3+1]-v.smp3[j3]))/32768.f*7.f*v.amp*0.55f; v.spos3+=v.sstep3; } else v.smp3=nullptr; }
                }else{
                float ph=v.phase*2.f*M_PI;
                float s=sinf(ph)*(1.f-fTimbre*0.4f)+(2.f*fabsf(2.f*(v.phase-floorf(v.phase))-1.f)-1.f)*fTimbre*0.35f;
                v.lp+=lpA*(s-v.lp);o+=v.lp*v.amp;}
                v.phase+=v.freq/sr;if(v.phase>1.f)v.phase-=1.f;
                v.amp*=decRate;v.age++;
                if(v.amp<0.0003f)v.active=false;
            }
            o=tanhf(o*1.4f)*fLevel;
            o=fVerb.tick(o,0.14f); out[0][i]=o;out[1][i]=o;
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
    void mutate(int mild=0){
        fRnd=fRnd*1664525u+1013904223u;
        for(int n=0;n<(mild?2:5);n++){
            fRnd=fRnd*1664525u+1013904223u;
            int s=(fRnd>>8)%16;
            fRnd=fRnd*1664525u+1013904223u;
            if((fRnd>>8)%100<35) fPat[s]=99;              // soms rust
            else fPat[s]=(fRnd>>8)%7;                     // nieuwe degree in key
        }
    }
    void applyPreset(int i){
        if(i<0)i=0;if(i>16)i=16;fPreset=(float)i;
        setParameterValue(kStijl,(float)i);
        setParameterValue(kTimbre,kPresetParams[i][0]);
        setParameterValue(kSwing,kPresetParams[i][1]);
        setParameterValue(kLevel,kPresetParams[i][2]);
    }
    float fStijl=0,fKey=0,fScale=0,fOctave=1,fTimbre=0.35f,fSwing=0.08f,fLevel=0.8f,fDice=0,fAuto=0,fPreset=0;
    int fPat[16];bool fPatInit=false;int fLastStijl=0;unsigned fRnd=987654;int fRR=0;
    MVoice fV[16];
    double fPhase=0;int64_t fStep=0;
    float fScope[40];uint32_t fScopeN=0;int fDecim=0;
    G2bVerb fVerb; bool fVerbOn=false;
    float fOn=1.f,fKracht=100.f,fGalaxyV=9.f,fSfeerV=1.f,fPlay=1.f;
    Galaxy fGal; Sfeer fSf;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MelodiePlugin)
};
Plugin* createPlugin(){return new MelodiePlugin();}
END_NAMESPACE_DISTRHO
