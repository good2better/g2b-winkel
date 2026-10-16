#include "cb_data.h"
#include "DistrhoPlugin.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

START_NAMESPACE_DISTRHO

static const char* kPresetNames[17] = {"G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"};
// TYPE, DRIVE, FILTER, DECAY, LEVEL
static const float kPresetParams[17][5] = {
    {0.15f,0.25f,0.60f,0.80f,0.f},   // G2B — warme sub (synth)
    {0.20f,0.35f,0.55f,0.60f,0.f},   // DEMBOW — percussieve bas
    {0.90f,0.45f,0.40f,0.90f,-1.f},  // DRILL — STACK: echte contrabas+sub, donker lang
    {0.10f,0.20f,0.70f,0.75f,0.f},   // RNB — schoon
    {0.28f,0.30f,0.65f,0.50f,0.f},   // HOUSE — pompend
    {0.22f,0.30f,0.55f,0.65f,0.f},   // AFRO
    {0.90f,0.50f,0.35f,0.95f,-2.f},  // TRAP — STACK donder-sub
    {0.65f,0.30f,0.50f,0.85f,-1.f},  // BOOM — ECHT contrabas-pizz
    {0.17f,0.40f,0.65f,0.55f,0.f},   // UKG — wobble-achtig
    {0.30f,0.35f,0.60f,0.45f,0.f},   // JERSEY — stabby
    {0.22f,0.30f,0.55f,0.70f,0.f},   // BOUNCE — springend
    {0.65f,0.15f,0.75f,0.80f,0.f},   // SOUL — ECHT ronde contrabas
    {0.65f,0.20f,0.50f,0.85f,-1.f},  // LOFI — ECHT dof
    {0.65f,0.40f,0.60f,0.50f,0.f},   // FUNK — ECHT punchy pizz
    {0.65f,0.20f,0.65f,0.70f,0.f},   // REGGAE — ECHT dub
    {0.25f,0.35f,0.60f,0.55f,0.f},   // DANCE — pump
    {0.65f,0.30f,0.55f,0.75f,0.f}    // KAWINA — ECHT rollend
};

struct BVoice { bool on; int note; float vel, t, ph, phD, lp; const int16_t* smp=nullptr; uint32_t slen=0; float spos=0,sstep=0; };

class BasPlugin : public Plugin {
public:
    enum Params { kType,kDrive,kFilter,kDecay,kLevel,kKey,kScale,kDice,kPreset,kNumParams };
    BasPlugin() : Plugin(kNumParams,0,2) {}
protected:
    const char* getLabel() const override { return "G2B Bas"; }
    const char* getDescription() const override { return "Sub-bass & reese-instrument: pure sub → detuned reese → growl"; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','B','a'); }

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints=kParameterIsAutomatable;
        switch(i){
        case kType:  p.name="TYPE";   p.symbol="type";  p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.3f;break;
        case kDrive: p.name="DRIVE";  p.symbol="drive"; p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.25f;break;
        case kFilter:p.name="FILTER"; p.symbol="filter";p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.6f;break;
        case kDecay: p.name="DECAY";  p.symbol="decay"; p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.8f;break;
        case kLevel: p.name="OUTPUT"; p.symbol="level"; p.ranges.min=-24;p.ranges.max=12;p.ranges.def=0;p.unit="dB";break;
        case kKey:   p.name="KEY";    p.symbol="key";   p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=11;p.ranges.def=9;break;
        case kScale: p.name="SCALE";  p.symbol="scale"; p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kDice:  p.name="DICE";   p.symbol="dice";  p.hints|=kParameterIsInteger|kParameterIsTrigger;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        case kPreset:p.name="PRESET"; p.symbol="preset";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=16;p.ranges.def=0;break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){case kType:return fType;case kDrive:return fDrive;case kFilter:return fFilter;case kDecay:return fDecay;case kLevel:return fLevel;case kKey:return fKey;case kScale:return fScale;case kDice:return fDice;case kPreset:return fPreset;}return 0.f;
    }
    void setParameterValue(uint32_t i, float v) override {
        if(i==kPreset){applyPreset((int)(v+0.5f));return;}
        switch(i){
        case kType:fType=v;break;case kDrive:fDrive=v;break;case kFilter:fFilter=v;break;
        case kDecay:fDecay=v;break;case kLevel:fLevel=v;break;case kKey:fKey=v;break;case kScale:fScale=v;break;
        case kDice:fDice=v;if(v>0.5f)rollDice();fDice=0.f;break;
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
        const float outG=powf(10.f,fLevel*0.05f);
        memset(out[0],0,frames*sizeof(float));
        memset(out[1],0,frames*sizeof(float));
        uint32_t pos=0;
        for(uint32_t e=0;e<=evc;e++){
            uint32_t tt=(e<evc)?ev[e].frame:frames;
            while(pos<tt){
                float s=0;
                for(auto& v:fV) if(v.on){
                    float f0=440.f*powf(2.f,(v.note-69)/12.f);
                    // TYPE-zones: <.50 synth | .50-.80 ECHT contrabas | >=.80 STACK (echt + sub-laag)
                    if(v.smp){ // echte contrabas-pizzicato
                        uint32_t i2=(uint32_t)v.spos;
                        if(i2>=v.slen-1){ v.smp=nullptr; }
                        else {
                            float fr=v.spos-i2;
                            float w=(v.smp[i2]+fr*(v.smp[i2+1]-v.smp[i2]))/32768.f;
                            v.spos+=v.sstep; v.t+=1.f/sr;
                            if(fType>=0.80f){ // STACK: sub-octaaf-laag eronder (donder)
                                v.ph+=2.f*M_PI*f0*0.5f/sr; if(v.ph>2.f*M_PI)v.ph-=2.f*M_PI;
                                w+=0.35f*sinf(v.ph);
                            }
                            w=tanhf(w*(1.f+fDrive*4.f))/(1.f+fDrive);
                            float fc2=180.f+fFilter*1400.f;
                            v.lp+=(w-v.lp)*(1.f-expf(-2.f*M_PI*fc2/sr));
                            w=v.lp;
                            float env2=expf(-v.t*(0.2f+(1.f-fDecay)*4.f));
                            s+=w*env2*v.vel*4.0f;
                            continue;
                        }
                    }
                    float t0=fType>0.5f?0.5f:fType*2.f; // synth-regime in <.5

                    v.ph+=2.f*M_PI*f0/sr;         if(v.ph>2.f*M_PI)v.ph-=2.f*M_PI;
                    float det=1.f+0.006f*t0*t0;   // reese-detune groeit met TYPE
                    v.phD+=2.f*M_PI*f0*det/sr;    if(v.phD>2.f*M_PI)v.phD-=2.f*M_PI;
                    v.t+=1.f/sr;
                    float w=sinf(v.ph);
                    // saw-component
                    float saw=2.f*(v.ph/(2.f*M_PI))-1.f;
                    float sawD=2.f*(v.phD/(2.f*M_PI))-1.f;
                    w+=t0*0.35f*saw+t0*0.30f*sawD;   // reese-mix
                    // drive
                    w=tanhf(w*(1.f+fDrive*6.f))/(1.f+fDrive*1.5f);
                    // filter (lp, cutoff met type)
                    float fc=120.f+fFilter*900.f;
                    v.lp+=(w-v.lp)*(1.f-expf(-2.f*M_PI*fc/sr));
                    w=v.lp;
                    // decay-envelope
                    float env=expf(-v.t*(0.3f+(1.f-fDecay)*6.f));
                    s+=w*env*v.vel;
                    if(env<0.0001f)v.on=false;
                }
                s*=outG*0.5f;
                out[0][pos]=s;out[1][pos]=s;
                if(++fDecim>=3){fDecim=0;if(fScopeN<40)fScope[fScopeN++]=s;}
                ++pos;
            }
            if(e<evc){
                const MidiEvent& m=ev[e];
                if(m.size>=3&&(m.data[0]&0xF0)==0x90&&m.data[2]>0){
                    // mono-legato: steek de eerste vrije of oudste voice
                    BVoice* use=&fV[0];float oldT=-1;
                    for(auto& v:fV) if(!v.on){use=&v;break;} else if(v.t>oldT){oldT=v.t;use=&v;}
                    use->on=true;use->note=snap(m.data[1]);use->vel=m.data[2]/127.f;use->t=0;use->lp=0;use->smp=nullptr;
                    if(fType>=0.50f){ // ECHT/STACK: echte contrabas-pizz
                        int bi=0,bd=99; for(int k=0;k<kCbCount;k++){int dd=std::abs(kCb[k].midi-use->note);if(dd<bd){bd=dd;bi=k;}}
                        const CbSmp&pb=kCb[bi];const bool hd=use->vel>=0.55f;const int rr=fRR++&1;
                        use->smp=hd?(rr?pb.dH2:pb.dH):(rr?pb.d2:pb.d);
                        use->slen=hd?(rr?pb.lenH2:pb.lenH):(rr?pb.len2:pb.len);use->spos=0;
                        float f0n=440.f*powf(2.f,(use->note-69)/12.f);
                        float fs=440.f*powf(2.f,(pb.midi-69)/12.f);
                        use->sstep=(f0n/fs)*(22050.f/sr);
                    }
                }
            }
        }
        if(fScopeN>=40){
            char buf[256];int p=0;
            for(uint32_t s2=0;s2<fScopeN&&p<230;++s2)p+=snprintf(buf+p,sizeof(buf)-p,"%.3f;",fScope[s2]);
            buf[p]=0;setState("scope",buf);fScopeN=0;
        }
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
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kType,((fRnd>>8)%100)/100.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kDrive,((fRnd>>8)%100)/100.f*0.6f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kFilter,0.2f+((fRnd>>8)%100)/100.f*0.7f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kDecay,0.3f+((fRnd>>8)%100)/100.f*0.7f);
    }
    void applyPreset(int i){
        if(i<0)i=0;if(i>16)i=16;fPreset=(float)i;
        setParameterValue(kType,kPresetParams[i][0]);
        setParameterValue(kDrive,kPresetParams[i][1]);
        setParameterValue(kFilter,kPresetParams[i][2]);
        setParameterValue(kDecay,kPresetParams[i][3]);
        setParameterValue(kLevel,kPresetParams[i][4]);
    }
    BVoice fV[8]={}; int fRR=0;
    float fType=0.3f,fDrive=0.25f,fFilter=0.6f,fDecay=0.8f,fLevel=0.f;
    float fKey=9.f,fScale=0.f,fDice=0.f,fPreset=0.f;
    unsigned fRnd=1357913;
    float fScope[40];uint32_t fScopeN=0;int fDecim=0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BasPlugin)
};
const int BasPlugin::kScales_[2][7]={{0,2,3,5,7,8,10},{0,2,4,5,7,9,11}};
Plugin* createPlugin(){return new BasPlugin();}
END_NAMESPACE_DISTRHO
