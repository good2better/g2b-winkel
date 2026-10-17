#include "akk_data.h"
#include "harp_data.h"
#include "strum_data.h"
#include "org_data.h"
#include "str_data.h"
#include "sus_data.h"
#include "../g2b_verb.h"
#include "DistrhoPlugin.hpp"
#include "../g2b_std.h"
#include <cstring>
#include <cmath>
#include <cstdint>
#include <algorithm>

START_NAMESPACE_DISTRHO

static const char* kPresetNames[17] = {"G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"};
// VOICING(0-5 = kwaliteit), TIMBRE(0 pluck→1 pad), OCTAVE(-2..2), DECAY(0.1-4s), LEVEL(0-1), —
static const float kPresetParams[17][5] = {
    {1.f, 0.35f, 0.f, 1.6f, 0.8f},   // G2B    — min9 warm
    {4.f, 0.15f, 0.f, 0.5f, 0.85f},  // DEMBOW — korte minor stab
    {2.f, 0.30f, -1.f,1.2f, 0.8f},   // DRILL  — donkere minor triad, oktaaf lager
    {0.f, 0.60f, 0.f, 2.2f, 0.8f},   // RNB    — maj7 pad
    {3.f, 0.20f, 0.f, 0.9f, 0.85f},  // HOUSE  — maj stab
    {5.f, 0.30f, 0.f, 1.1f, 0.82f},  // AFRO   — sus2/7 groove
    {2.f, 0.25f, -1.f,0.7f, 0.85f},  // TRAP   — kille triad laag
    {2.f, 0.40f, -1.f,1.8f, 0.8f},   // BOOM   — klassieke minor
    {4.f, 0.45f, 0.f, 0.8f, 0.85f},  // UKG    — min7 stab speedgarage
    {5.f, 0.30f, 1.f, 0.9f, 0.85f},  // JERSEY — sus hoog percussief
    {4.f, 0.20f, 0.f, 1.0f, 0.85f},  // BOUNCE — min7 stab
    {1.f, 0.70f, 0.f, 2.6f, 0.78f},  // SOUL   — min9 pad breed
    {2.f, 0.50f, -1.f,2.0f, 0.75f},  // LOFI   — dof minor
    {0.f, 0.55f, 0.f, 1.4f, 0.82f},  // FUNK   — maj7 tight
    {5.f, 0.45f, 0.f, 1.5f, 0.80f},  // REGGAE — sus skank
    {3.f, 0.35f, 1.f, 0.8f, 0.88f},  // DANCE  — maj stab hoog
    {1.f, 0.40f, 0.f, 1.3f, 0.85f},  // KAWINA — min9 rollend
};

// voicing-sets (halve tonen t.o.v. root), max 5 noten
static const int kVoicings[6][5] = {
    {0,4,7,11,-999},      // maj7
    {0,3,7,10,14},        // min9
    {0,3,7,-999,-999},    // minor triad
    {0,4,7,-999,-999},    // maj stab
    {0,3,10,-999,-999},   // min7 stab
    {0,7,10,14,-999},     // sus/7 afro
};

struct ChordVoice {
    bool active=false; float freq=0,phase=0,amp=0,vel=0; int age=0; float lp=0; int delay=0;
    const int16_t* smp=nullptr; uint32_t slen=0; float spos=0,sstep=0; int note=0; float sgain=1.3f; const int16_t* smp2=nullptr; uint32_t slen2=0; float spos2=0,sstep2=0; const int16_t* smp3=nullptr; uint32_t slen3=0; float spos3=0,sstep3=0;
};

static const int kScales[2][7] = {
    {0,2,3,5,7,8,10},   // minor
    {0,2,4,5,7,9,11},   // major
};

class AkkoordPlugin : public Plugin {
public:
    enum Params { kVoicing,kTimbre,kOctave,kDecay,kLevel,kKey,kScale,kStrum,kHuman,kDice,kAuto,kPreset,kOn,kKracht,kGalaxy,kSfeer,kNumParams };
    AkkoordPlugin() : Plugin(kNumParams, 0, 2) {}
protected:
    const char* getLabel() const override { return "G2B Akkoord"; }
    const char* getDescription() const override { return "Eén toets = een heel akkoord in jouw stijl"; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','A','k'); }

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints=kParameterIsAutomatable;
        switch(i){
        case kVoicing:p.name="VOICING";p.symbol="voicing";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=5;p.ranges.def=1;break;
        case kTimbre: p.name="TIMBRE"; p.symbol="timbre"; p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.35f;p.unit="";break;
        case kOctave: p.name="OCTAVE"; p.symbol="octave"; p.hints|=kParameterIsInteger;p.ranges.min=-2;p.ranges.max=2;p.ranges.def=0;break;
        case kDecay:  p.name="DECAY";  p.symbol="decay";  p.ranges.min=0.1f;p.ranges.max=4.f;p.ranges.def=1.6f;p.unit="s";break;
        case kLevel:  p.name="LEVEL";  p.symbol="level";  p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.8f;p.unit="%";break;
        case kKey:    p.name="KEY";    p.symbol="key";    p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=11;p.ranges.def=0;break;
        case kScale:  p.name="SCALE";  p.symbol="scale";  p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kStrum:  p.name="STRUM";  p.symbol="strum";  p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.f;p.unit="%";break;
        case kHuman:  p.name="HUMANIZE";p.symbol="human"; p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.3f;p.unit="%";break;
        case kPreset: p.name="PRESET"; p.symbol="preset"; p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=16;p.ranges.def=0;break;
        case kOn:    p.name="Aan";      p.symbol="aan";   p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;break;
        case kKracht:p.name="Kracht";   p.symbol="kracht";p.ranges.min=0;p.ranges.max=100;p.ranges.def=100;p.unit="%";break;
        case kGalaxy:p.name="DIMENTIES";p.symbol="dim";   p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=26;p.ranges.def=9;break;
        case kSfeer: p.name="Sfeer";    p.symbol="sfeer"; p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=2;p.ranges.def=1;break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){case kVoicing:return fVoicing;case kTimbre:return fTimbre;case kOctave:return fOctave;case kDecay:return fDecay;case kLevel:return fLevel;case kKey:return fKey;case kScale:return fScale;case kStrum:return fStrum;case kHuman:return fHuman;case kDice:return fDice;case kAuto:return fAuto;case kPreset:return fPreset;case kOn:return fOn;case kKracht:return fKracht;case kGalaxy:return fGalaxyV;case kSfeer:return fSfeerV;}return 0.f;
    }
    void setParameterValue(uint32_t i, float v) override {
        if(i==kPreset){applyPreset((int)(v+0.5f));return;}
        switch(i){case kVoicing:fVoicing=v;break;case kTimbre:fTimbre=v;break;case kOctave:fOctave=v;break;case kDecay:fDecay=v;break;case kLevel:fLevel=v;break;case kKey:fKey=v;break;case kScale:fScale=v;break;case kStrum:fStrum=v;break;case kHuman:fHuman=v;break;case kDice:fDice=v;if(v>0.5f)rollDice();fDice=0.f;break;case kAuto:fAuto=v;break;
        case kOn:fOn=v;break;case kKracht:fKracht=v;break;
        case kGalaxy:fGalaxyV=v;fGal.setPos((int)(v+0.5f));break;
        case kSfeer:fSfeerV=v;fSf.setMode((int)(v+0.5f));break;}
    }
    void initState(uint32_t index, String& name, String& def) override {
        if(index==0){name="scope";def="";}
        else{name="presetnames";def="";for(int i=0;i<17;i++){def+=kPresetNames[i];if(i<16)def+=";";}}
    }
    void setState(const char* k, const char* v) override {(void)k;(void)v;}

    void run(const float** in, float** out, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override {
        const float sr=getSampleRate();if(!fVerbOn){fVerb.init(sr);fVerbOn=true;}
        if(fOn<0.5f){std::memset(out[0],0,frames*sizeof(float));std::memset(out[1],0,frames*sizeof(float));return;}
        // MIDI events eerst
        for(uint32_t m=0;m<midiCount;m++){
            const MidiEvent& ev=midi[m];
            if(ev.size<3) continue;
            const uint8_t st=ev.data[0]&0xF0;
            if(st==0x90 && ev.data[2]>0){          // noteOn
                const int root=snap(ev.data[1]); const float vel=ev.data[2]/127.f;
                const int* voic=kVoicings[(int)fVoicing<0?0:((int)fVoicing>5?5:(int)fVoicing)];
                float strumMax=fStrum*0.030f*sr;   // tot 30ms per extra noot
                if(fAuto>0.5f){ fRnd=(fRnd*1664525u+1013904223u); strumMax*=(0.7f+(fRnd%1000)/1000.f*0.8f); }
                for(int n=0;n<5;n++){
                    if(voic[n]<=-999) break;
                    const float f=440.f*powf(2.f,((snap(root+voic[n])+(int)fOctave*12)-69)/12.f);
                    const float hv=1.f+fHuman*0.35f*((fRnd=(fRnd*1664525u+1013904223u))%1000/1000.f*2.f-1.f);
                    for(auto& v:fV){ if(!v.active){ v.active=true;v.freq=f;v.phase=0;v.amp=0;v.vel=vel*hv;v.age=0;v.lp=0;v.delay=(int)(strumMax*n);
                        v.smp=nullptr;v.smp2=nullptr;v.smp3=nullptr;
                        if(fTimbre>=0.92f){ // PAD: aangehouden strijkers (cello-susvib + contrabas-sus)
                            const int mn=snap(root+voic[n])+(int)fOctave*12; v.note=mn;
                            int bi=0,bd=99; for(int k=0;k<kVcSusCount;k++){int d=std::abs(kVcSus[k].midi-mn);if(d<bd){bd=d;bi=k;}}
                            const SusSmp&ps=kVcSus[bi];const bool hrd=v.vel>=0.55f;v.smp=hrd?ps.d2:ps.d;v.slen=hrd?ps.len2:ps.len;v.sgain=0.35f;
                            const float fs=440.f*powf(2.f,(ps.midi-69)/12.f);
                            v.spos=5500.f;v.sstep=(f/fs)*(22050.f/sr);
                            int c2i=0,c2d=99; for(int k=0;k<kCbSusCount;k++){int d=std::abs(kCbSus[k].midi-(mn-12));if(d<c2d){c2d=d;c2i=k;}}
                            const SusSmp&pc=kCbSus[c2i];v.smp2=pc.d;v.slen2=pc.len;
                            const float fc=440.f*powf(2.f,(pc.midi+12-69)/12.f);
                            v.spos2=0;v.sstep2=(f/fc)*(22050.f/sr);
                        } else if(fTimbre>=0.84f){ // STRINGS: viool+cello pizzicato gestapeld
                            const int mn=snap(root+voic[n])+(int)fOctave*12; v.note=mn;
                            int bi=0,bd=99; for(int k=0;k<kVlnCount;k++){int d=std::abs(kVln[k].midi-mn);if(d<bd){bd=d;bi=k;}}
                            const VlnSmp&ps=kVln[bi];const bool hd=v.vel>=0.5f;v.smp=hd?((fRR&1)?ps.dH2:ps.dH):((fRR&1)?ps.d2:ps.d);v.slen=hd?((fRR&1)?ps.lenH2:ps.lenH):((fRR&1)?ps.len2:ps.len);v.sgain=1.4f;
                            const float fs=440.f*powf(2.f,(ps.midi-69)/12.f);
                            v.spos=0;v.sstep=(f/fs)*(22050.f/sr);
                            int c2i=0,c2d=99; for(int k=0;k<kVcCount;k++){int d=std::abs(kVc[k].midi-(mn-12));if(d<c2d){c2d=d;c2i=k;}}
                            const VcSmp&pc=kVc[c2i];v.smp2=hd?((fRR&2)?pc.dH2:pc.dH):((fRR&2)?pc.d2:pc.d);v.slen2=hd?((fRR&2)?pc.lenH2:pc.lenH):((fRR&2)?pc.len2:pc.len);
                            const float fc=440.f*powf(2.f,(pc.midi+12-69)/12.f);
                            v.spos2=0;v.sstep2=(f/fc)*(22050.f/sr);
        int v3i=0,v3d=99; for(int k=0;k<kVlaCount;k++){int d=std::abs(kVla[k].midi-(mn-5));if(d<v3d){v3d=d;v3i=k;}}
        const VlaSmp&pv=kVla[v3i];v.smp3=hd?((fRR&4)?pv.dH2:pv.dH):((fRR&4)?pv.d2:pv.d);v.slen3=hd?((fRR&4)?pv.lenH2:pv.lenH):((fRR&4)?pv.len2:pv.len); fRR++;
        fRR&=7;
        const float fv=440.f*powf(2.f,(pv.midi+5-69)/12.f);
        v.spos3=0;v.sstep3=(f/fv)*(22050.f/sr);
                        } else if(fTimbre>=0.74f){ // STRUMSTICK echte geplukte snaar, 2 lagen
                            const int mn=snap(root+voic[n])+(int)fOctave*12; v.note=mn;
                            int bi=0,bd=99; for(int k=0;k<kStickCount;k++){int d=std::abs(kStickS[k].midi-mn);if(d<bd){bd=d;bi=k;}}
                            const StrSmp&ps=kStickS[bi];const bool hrd=v.vel>=0.55f;v.smp=hrd?ps.d2:ps.d;v.slen=hrd?ps.len2:ps.len;v.sgain=2.2f;
                            const float fs=440.f*powf(2.f,(ps.midi-69)/12.f);
                            v.spos=0;v.sstep=(f/fs)*(22050.f/sr);
                        } else if(fTimbre>=0.64f){ // ORGEL echte pijp-orgel (Rode Man3Open)
                            const int mn=snap(root+voic[n])+(int)fOctave*12; v.note=mn;
                            int bi=0,bd=99; for(int k=0;k<kOrgCount;k++){int d=std::abs(kOrg[k].midi-mn);if(d<bd){bd=d;bi=k;}}
                            const OrgSmp&ps=kOrg[bi];v.smp=ps.d;v.slen=ps.len;v.sgain=0.8f;
                            const float fs=440.f*powf(2.f,(ps.midi-69)/12.f);
                            v.spos=0;v.sstep=(f/fs)*(22050.f/sr);
                        } else if(fTimbre>=0.54f){ // YAMAHA echte upright-piano
                            const int mn=snap(root+voic[n])+(int)fOctave*12; v.note=mn;
                            int bi=0,bd=99; for(int k=0;k<8;k++){int d=std::abs(kYamaha[k].midi-mn);if(d<bd){bd=d;bi=k;}}
                            const AkkSmp&ps=kYamaha[bi];const bool hrd=v.vel>=0.55f;v.smp=hrd?ps.d2:ps.d;v.slen=hrd?ps.len2:ps.len;
                            const float fs=440.f*powf(2.f,(ps.midi-69)/12.f);
                            v.spos=0;v.sstep=(f/fs)*(22050.f/sr);
                        } else if(fTimbre>=0.44f){ // HARP echte concert-harp, 2 lagen
                            const int mn=snap(root+voic[n])+(int)fOctave*12; v.note=mn;
                            int bi=0,bd=99; for(int k=0;k<8;k++){int d=std::abs(kHarp[k].midi-mn);if(d<bd){bd=d;bi=k;}}
                            const HarpSmp&ps=kHarp[bi];const bool hrd=v.vel>=0.55f;v.smp=hrd?ps.d2:ps.d;v.slen=hrd?ps.len2:ps.len;v.sgain=2.0f;
                            const float fs=440.f*powf(2.f,(ps.midi-69)/12.f);
                            v.spos=0;v.sstep=(f/fs)*(22050.f/sr);
                        }
                        break; } }
                }
            }
        }
        const float decRate=expf(-1.f/(fDecay*sr));
        const float cutHz=600.f+fTimbre*6000.f;
        const float lpA=1.f-expf(-2.f*M_PI*cutHz/sr);
        for(uint32_t i=0;i<frames;i++){
            float o=0.f;
            for(auto& v:fV){
                if(!v.active) continue;
                if(v.delay>0){ v.delay--; continue; }
                if(v.amp<0.001f) v.amp=v.vel;   // start na strum-delay
                float s;
                if(v.smp){ // echte piano-sample
                    uint32_t i2=(uint32_t)v.spos;
                    if(i2>=v.slen-1){v.active=false;continue;}
                    float fr=v.spos-i2;
                    s=(v.smp[i2]+fr*(v.smp[i2+1]-v.smp[i2]))/32768.f*6.f;
                    v.spos+=v.sstep;
                    if(v.smp2){ uint32_t j2=(uint32_t)v.spos2; if(j2<v.slen2-1){ float fr2=v.spos2-j2; s+=(v.smp2[j2]+fr2*(v.smp2[j2+1]-v.smp2[j2]))/32768.f*6.f*0.75f; v.spos2+=v.sstep2; }if(v.smp3){ uint32_t j3=(uint32_t)v.spos3; if(j3<v.slen3-1){ float fr3=v.spos3-j3; s+=(v.smp3[j3]+fr3*(v.smp3[j3+1]-v.smp3[j3]))/32768.f*6.f*0.6f; v.spos3+=v.sstep3; } else v.smp3=nullptr; } }
                    o+=s*v.amp*v.sgain;
                }else{
                // osc: sine + driehoek-blend (pluck↔pad), via timbre
                float ph=v.phase*2.f*M_PI;
                s=sinf(ph)*(1.f-fTimbre*0.5f)+ (2.f*fabsf(2.f*(v.phase-floorf(v.phase))-1.f)-1.f)*fTimbre*0.35f;
                v.lp+=lpA*(s-v.lp);
                o+=v.lp*v.amp;}
                v.phase+=v.freq/sr; if(v.phase>1.f)v.phase-=1.f;
                v.amp*=decRate; v.age++;
                if(v.amp<0.0003f)v.active=false;
            }
            o=tanhf(o*1.4f)*fLevel;
            o=fVerb.tick(o,0.14f); out[0][i]=o; out[1][i]=o;
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
    int snap(int midi) const {
        // quantize naar KEY+SCALE — elke toets klinkt in key
        int key=(int)fKey, sc=(int)fScale<0?0:((int)fScale>1?1:(int)fScale);
        int pc=((midi-key)%12+12)%12;
        int best=kScales[sc][0],bd=99;
        for(int i=0;i<7;i++){int d=abs(pc-kScales[sc][i]);if(d<bd){bd=d;best=kScales[sc][i];}}
        return midi-pc+best;
    }
    void rollDice(){
        fRnd=fRnd*1664525u+1013904223u;
        setParameterValue(kVoicing,(float)((fRnd>>8)%17));
        fRnd=fRnd*1664525u+1013904223u;
        setParameterValue(kTimbre,((fRnd>>8)%100)/100.f);
        fRnd=fRnd*1664525u+1013904223u;
        setParameterValue(kStrum,((fRnd>>8)%40)/100.f);
        fRnd=fRnd*1664525u+1013904223u;
        setParameterValue(kHuman,((fRnd>>8)%170)/100.f);
    }
    void applyPreset(int i){
        if(i<0)i=0;if(i>16)i=16;fPreset=(float)i;
        setParameterValue(kVoicing,kPresetParams[i][0]);
        setParameterValue(kTimbre,kPresetParams[i][1]);
        setParameterValue(kOctave,kPresetParams[i][2]);
        setParameterValue(kDecay,kPresetParams[i][3]);
        setParameterValue(kLevel,kPresetParams[i][4]);
    }
    float fVoicing=1.f,fTimbre=0.35f,fOctave=0.f,fDecay=1.6f,fLevel=0.8f,fKey=0.f,fScale=0.f,fStrum=0.f,fHuman=0.3f,fPreset=0.f,fDice=0.f,fAuto=0.f;
    ChordVoice fV[24];
    unsigned fRnd=54321; int fRR=0;
    float fOn=1.f,fKracht=100.f,fGalaxyV=9.f,fSfeerV=1.f;
    Galaxy fGal; Sfeer fSf;
    float fScope[40];uint32_t fScopeN=0;int fDecim=0;
    G2bVerb fVerb; bool fVerbOn=false;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AkkoordPlugin)
};
Plugin* createPlugin(){return new AkkoordPlugin();}
END_NAMESPACE_DISTRHO
