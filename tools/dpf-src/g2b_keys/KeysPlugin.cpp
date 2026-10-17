#include "piano_data.h"
#include "steinway_data.h"
#include "DistrhoPlugin.hpp"
#include "../g2b_std.h"
#include <cmath>
#include <cstdio>
#include <cstring>

START_NAMESPACE_DISTRHO

static const char* kPresetNames[17] = {"G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"};
// TIMBRE, BELLS, DECAY, TREM, LEVEL — Rhodes-style variaties
static const float kPresetParams[17][5] = {
    {0.45f,0.35f,0.70f,0.00f,0.f},   // G2B — warme e-piano
    {0.55f,0.20f,0.55f,0.00f,0.f},   // DEMBOW — korter, strakker
    {0.30f,0.15f,0.75f,0.10f,-1.f},  // DRILL — donker + langzaam tremolo
    {0.40f,0.45f,0.80f,0.15f,0.f},   // RNB — bell erbij + subtiel tremolo
    {0.60f,0.30f,0.60f,0.05f,0.f},   // HOUSE — helder
    {0.50f,0.25f,0.65f,0.00f,0.f},   // AFRO — percussief
    {0.35f,0.20f,0.80f,0.20f,-1.f},  // TRAP — kil + trem
    {0.40f,0.25f,0.70f,0.00f,-1.f},  // BOOM — rond laag
    {0.65f,0.40f,0.45f,0.08f,0.f},   // UKG — fel kort
    {0.70f,0.50f,0.40f,0.00f,0.f},   // JERSEY — bell lead
    {0.55f,0.35f,0.55f,0.05f,0.f},   // BOUNCE — veerkrachtig
    {0.35f,0.50f,0.85f,0.12f,0.f},   // SOUL — zacht uitgehouden
    {0.30f,0.40f,0.90f,0.06f,-1.f},  // LOFI — tape e-piano
    {0.60f,0.20f,0.50f,0.15f,0.f},   // FUNK — clav-achtig
    {0.50f,0.30f,0.75f,0.00f,0.f},   // REGGAE — bubble organ
    {0.65f,0.45f,0.50f,0.00f,0.f},   // DANCE — stab lead
    {0.48f,0.35f,0.70f,0.04f,0.f}    // KAWINA — log-drum keys
};

struct KVoice { bool on; int note; float vel, t, ph1, ph2, ph3, ph4; const int16_t* smp=nullptr; uint32_t slen=0; float spos=0,sstep=0; };

class KeysPlugin : public Plugin {
public:
    enum Params { kTimbre,kBells,kDecay,kTrem,kLevel,kKey,kScale,kDice,kPreset,kOn,kGalaxy,kSfeer,kNumParams };
    KeysPlugin() : Plugin(kNumParams,0,2) {}
protected:
    const char* getLabel() const override { return "G2B Keys"; }
    const char* getDescription() const override { return "E-piano/keys-instrument: Rhodes-stijl tine + bell + tremolo, velocity-speelt"; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','K','y'); }

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints=kParameterIsAutomatable;
        switch(i){
        case kTimbre:p.name="TIMBRE"; p.symbol="timbre";p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.45f;break;
        case kBells: p.name="BELLS";  p.symbol="bells"; p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.35f;break;
        case kDecay: p.name="DECAY";  p.symbol="decay"; p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.7f;break;
        case kTrem:  p.name="TREMOLO";p.symbol="trem";  p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.f;break;
        case kLevel: p.name="OUTPUT"; p.symbol="level"; p.ranges.min=-24;p.ranges.max=12;p.ranges.def=0;p.unit="dB";break;
        case kKey:   p.name="KEY";    p.symbol="key";   p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=11;p.ranges.def=9;break;
        case kScale: p.name="SCALE";  p.symbol="scale"; p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kDice:  p.name="DICE";   p.symbol="dice";  p.hints|=kParameterIsInteger|kParameterIsTrigger;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kPreset:p.name="PRESET"; p.symbol="preset";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=16;p.ranges.def=0;break;
        case kOn:    p.name="Aan";      p.symbol="aan";   p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;break;
        case kGalaxy:p.name="DIMENTIES";p.symbol="dim";   p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=26;p.ranges.def=9;break;
        case kSfeer: p.name="Sfeer";    p.symbol="sfeer"; p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=2;p.ranges.def=1;break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){case kTimbre:return fTimbre;case kBells:return fBells;case kDecay:return fDecay;case kTrem:return fTrem;case kLevel:return fLevel;case kKey:return fKey;case kScale:return fScale;case kDice:return fDice;case kPreset:return fPreset;case kOn:return fOn;case kGalaxy:return fGalaxyV;case kSfeer:return fSfeerV;}return 0.f;
    }
    void setParameterValue(uint32_t i, float v) override {
        if(i==kPreset){applyPreset((int)(v+0.5f));return;}
        switch(i){
        case kTimbre:fTimbre=v;break;case kBells:fBells=v;break;case kDecay:fDecay=v;break;
        case kTrem:fTrem=v;break;case kLevel:fLevel=v;break;case kKey:fKey=v;break;case kScale:fScale=v;break;
        case kDice:fDice=v;if(v>0.5f)rollDice();fDice=0.f;break;
        case kOn:fOn=v;break;
        case kGalaxy:fGalaxyV=v;fGal.setPos((int)(v+0.5f));break;
        case kSfeer:fSfeerV=v;fSf.setMode((int)(v+0.5f));break;
        }
    }
    void initState(uint32_t index, String& name, String& def) override {
        if(index==0){name="scope";def="";}
        else{name="presetnames";def="";for(int i=0;i<17;i++){def+=kPresetNames[i];if(i<16)def+=";";}}
    }
    void setState(const char* k, const char* v) override {(void)k;(void)v;}

    void run(const float**, float** out, uint32_t frames,
             const MidiEvent* ev, uint32_t evc) override {
        const float sr=getSampleRate();
        memset(out[0],0,frames*sizeof(float));
        memset(out[1],0,frames*sizeof(float));
        if(fOn<0.5f)return;
        const float outG=powf(10.f,fLevel*0.05f);
        uint32_t pos=0;
        for(uint32_t e=0;e<=evc;e++){
            uint32_t t=(e<evc)?ev[e].frame:frames;
            while(pos<t){
                float s=0;
                for(auto& v:fV) if(v.on){
                    float f0=440.f*powf(2.f,(v.note-69)/12.f);
                    v.ph1+=2.f*M_PI*f0/sr;         if(v.ph1>2.f*M_PI)v.ph1-=2.f*M_PI;
                    v.ph2+=2.f*M_PI*f0*2.f/sr;     if(v.ph2>2.f*M_PI)v.ph2-=2.f*M_PI;
                    v.ph3+=2.f*M_PI*f0*4.f/sr;     if(v.ph3>2.f*M_PI)v.ph3-=2.f*M_PI;
                    v.ph4+=2.f*M_PI*f0*6.f/sr;     if(v.ph4>2.f*M_PI)v.ph4-=2.f*M_PI;
                    v.t+=1.f/sr;
                    // Rhodes-tine: fund + overtones met snelle bell-decay
                    float envB=expf(-v.t*(2.f+14.f*(1.f-fBells)));
                    float dec=0.4f+fDecay*4.f;
                    float env=expf(-v.t*dec);
                    float w;
                    if(v.smp){
                        uint32_t i=(uint32_t)v.spos;
                        if(i>=v.slen-1){v.on=false;continue;}
                        float fr=v.spos-i;
                        w=(v.smp[i]+fr*(v.smp[i+1]-v.smp[i]))/32768.f*7.f;
                        v.spos+=v.sstep;
                    }else{
                    w=sinf(v.ph1)
                           + (0.25f+fTimbre*0.3f)*sinf(v.ph2)*envB
                           + (0.06f+fTimbre*0.1f)*sinf(v.ph3)*envB*envB
                           + fBells*0.25f*sinf(v.ph4)*envB*envB;}
                    // tremolo (stereo pan-lfo per voice)
                    float tr=1.f;
                    if(fTrem>0.001f){float r=3.f+fTrem*4.f;tr=0.7f+0.3f*sinf(v.t*r*2.f*M_PI);}
                    s+=w*env*tr*v.vel;
                    if(env<0.0001f)v.on=false;
                }
                s*=outG*0.3f;
                if(fTrem>0.001f){ // stereo tremolo: L en R tegenfase
                    float r=3.f+fTrem*4.f;
                    float ph=pos/sr*r*2.f*M_PI;
                    float l=0.7f+0.3f*sinf(ph)*fTrem;
                    float rr=0.7f+0.3f*sinf(ph+M_PI)*fTrem;
                    out[0][pos]=s*l; out[1][pos]=s*rr;
                } else { out[0][pos]=s; out[1][pos]=s; }
                if(++fDecim>=3){fDecim=0;if(fScopeN<40)fScope[fScopeN++]=s;}
                ++pos;
            }
            if(e<evc){
                const MidiEvent& m=ev[e];
                if(m.size>=3&&(m.data[0]&0xF0)==0x90&&m.data[2]>0){
                    for(auto& v:fV) if(!v.on){
                        v.on=true;v.note=snap(m.data[1]);v.vel=m.data[2]/127.f;v.t=0;
                        v.ph1=v.ph2=v.ph3=v.ph4=0;
                        v.smp=nullptr;
                        if(fTimbre>=0.95f){ // STEINWAY echte vleugel
                            int bi=0,bd=99;
                            for(int k=0;k<8;k++){int d=std::abs(kStein[k].midi-v.note);if(d<bd){bd=d;bi=k;}}
                            const StSmp&ps=kStein[bi];const bool hrd=v.vel>=0.55f;v.smp=hrd?ps.d2:ps.d;v.slen=hrd?ps.len2:ps.len;
                            float f0=440.f*powf(2.f,(v.note-69)/12.f),fs=440.f*powf(2.f,(ps.midi-69)/12.f);
                            v.spos=0;v.sstep=(f0/fs)*(22050.f/sr);
                        } else if(fTimbre>=0.5f){ // KAWAI echte piano-laag
                            int bi=0,bd=99;
                            for(int k=0;k<8;k++){int d=std::abs(kPiano[k].midi-v.note);if(d<bd){bd=d;bi=k;}}
                            const PianoSmp&ps=kPiano[bi];const bool hrd=v.vel>=0.55f;v.smp=hrd?ps.d2:ps.d;v.slen=hrd?ps.len2:ps.len;
                            float f0=440.f*powf(2.f,(v.note-69)/12.f),fs=440.f*powf(2.f,(ps.midi-69)/12.f);
                            v.spos=0;v.sstep=(f0/fs)*(22050.f/sr);
                        }
                        break;
                    }
                } else if(m.size>=3&&((m.data[0]&0xF0)==0x80||((m.data[0]&0xF0)==0x90&&m.data[2]==0))){
                    for(auto& v:fV) if(v.on&&v.note==m.data[1]) v.vel*=0.0f,v.t*=4.f; // release versnellen
                }
            }
        }
        if(fScopeN>=40){
            char buf[256];int p=0;
            for(uint32_t s2=0;s2<fScopeN&&p<230;++s2)p+=snprintf(buf+p,sizeof(buf)-p,"%.3f;",fScope[s2]);
            buf[p]=0;setState("scope",buf);fScopeN=0;
        }
        g2b_post(nullptr,out,frames,sr,fOn,100.f,fGal,&fSf,false);
    }
private:
    static const int kScales_[2][7];
    int snap(int midi) const {
        int key=(int)fKey, sc=(int)fScale<0?0:((int)fScale>1?1:(int)fScale);
        int pc=((midi-key)%12+12)%12;
        int best=kScales_[sc][0],bd=99;
        for(int i=0;i<7;i++){int d=abs(pc-kScales_[sc][i]);if(d<bd){bd=d;best=kScales_[sc][i];}}
        return midi-pc+best;
    }
    void rollDice(){
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kTimbre,0.2f+((fRnd>>8)%100)/100.f*0.6f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kBells,((fRnd>>8)%100)/100.f*0.7f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kDecay,0.3f+((fRnd>>8)%100)/100.f*0.6f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kTrem,((fRnd>>8)%100)/100.f*0.4f);
    }
    void applyPreset(int i){
        if(i<0)i=0;if(i>16)i=16;fPreset=(float)i;
        setParameterValue(kTimbre,kPresetParams[i][0]);
        setParameterValue(kBells,kPresetParams[i][1]);
        setParameterValue(kDecay,kPresetParams[i][2]);
        setParameterValue(kTrem,kPresetParams[i][3]);
        setParameterValue(kLevel,kPresetParams[i][4]);
    }
    KVoice fV[16]={};
    float fTimbre=0.45f,fBells=0.35f,fDecay=0.7f,fTrem=0.f,fLevel=0.f;
    float fKey=9.f,fScale=0.f,fDice=0.f,fPreset=0.f;
    unsigned fRnd=777777;
    float fOn=1.f,fGalaxyV=9.f,fSfeerV=1.f;
    Galaxy fGal; Sfeer fSf;
    float fScope[40];uint32_t fScopeN=0;int fDecim=0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeysPlugin)
};
const int KeysPlugin::kScales_[2][7]={{0,2,3,5,7,8,10},{0,2,4,5,7,9,11}};
Plugin* createPlugin(){return new KeysPlugin();}
END_NAMESPACE_DISTRHO
