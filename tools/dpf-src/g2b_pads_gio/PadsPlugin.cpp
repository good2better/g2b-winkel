/* G2B PADS — Giorgio's eigen drum/one-shot machine (privé, nooit verkopen).
 * 18 genre-banks x 16 pads (notes 36-51), shots ingebakken.
 * Knoppen: BANK | TUNE | DECAY | LO | HI | OUTPUT | DIMENTIES | SFEER | AAN | DICE
 */
#include "DistrhoPlugin.hpp"

#include "../g2b_std.h"
#include "pads_data.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>

START_NAMESPACE_DISTRHO

struct PVoice { bool on=false; int note=0; float vel=0.f; const int16_t* smp=nullptr; uint32_t slen=0; float spos=0.f,sstep=0.f,rel=0.f; float lp=0.f; };

class PadsPlugin : public Plugin {
public:
    PadsPlugin() : Plugin(kNumParams, 0, 2) {}
protected:
    const char* getLabel() const override { return "G2B Pads"; }
    const char* getMaker() const override { return "Good2Better"; }
    const char* getLicense() const override { return "G2B"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','P','d'); }
    enum { kBank=0,kTune,kDecay,kLo,kHi,kLevel,kOn,kGalaxy,kSfeer,kDice,kNumParams };
    void initParameter(uint32_t i, Parameter& p) override {
        p.ranges.def=0.f; p.hints=kParameterIsAutomatable;
        switch(i){
        case kBank:  p.name="BANK";   p.symbol="bank"; p.hints|=kParameterIsInteger; p.ranges.min=0;p.ranges.max=kNBanks-1;p.ranges.def=0;break;
        case kTune:  p.name="TUNE";   p.symbol="tune"; p.ranges.min=-12;p.ranges.max=12;p.ranges.def=0;p.unit="st";break;
        case kDecay: p.name="DECAY";  p.symbol="decay";p.ranges.min=0.05f;p.ranges.max=2.f;p.ranges.def=1.f;break;
        case kLo:    p.name="LO";     p.symbol="lo";   p.ranges.min=-12;p.ranges.max=12;p.ranges.def=0;p.unit="dB";break;
        case kHi:    p.name="HI";     p.symbol="hi";   p.ranges.min=-12;p.ranges.max=12;p.ranges.def=0;p.unit="dB";break;
        case kLevel: p.name="OUTPUT"; p.symbol="level";p.ranges.min=-24;p.ranges.max=12;p.ranges.def=0;p.unit="dB";break;
        case kOn:    p.name="Aan";    p.symbol="aan";  p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;break;
        case kGalaxy:p.name="DIMENTIES";p.symbol="dim";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=26;p.ranges.def=9;break;
        case kSfeer: p.name="Sfeer";  p.symbol="sfeer";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=2;p.ranges.def=1;break;
        case kDice:  p.name="DICE";   p.symbol="dice"; p.hints|=kParameterIsInteger|kParameterIsTrigger;p.ranges.min=0;p.ranges.max=1;break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){case kBank:return fBank;case kTune:return fTune;case kDecay:return fDecay;case kLo:return fLo;case kHi:return fHi;case kLevel:return fLevel;case kOn:return fOn;case kGalaxy:return fGalaxyV;case kSfeer:return fSfeerV;case kDice:return fDice;}return 0.f;
    }
    void setParameterValue(uint32_t i, float v) override {
        switch(i){
        case kBank:fBank=v;break;case kTune:fTune=v;break;case kDecay:fDecay=v;break;
        case kLo:fLo=v;break;case kHi:fHi=v;break;case kLevel:fLevel=v;break;case kOn:fOn=v;break;
        case kGalaxy:fGalaxyV=v;fGal.setPos((int)(v+0.5f));break;
        case kSfeer:fSfeerV=v;fSf.setMode((int)(v+0.5f));break;
        case kDice:fDice=v;if(v>0.5f){fRnd=fRnd*1664525u+1013904223u;fBank=(float)((fRnd>>9)%kNBanks);}fDice=0.f;break;
        }
    }
    void initState(uint32_t index, String& name, String& def) override {
        if(index==0){name="scope";def="";}
        else{name="banknames";def="";for(int b=0;b<kNBanks;b++){def+=kPadBanks[b].name;if(b<kNBanks-1)def+=";";}}
    }
    void setState(const char*, const char*) override {}
    void run(const float**, float** out, uint32_t frames, const MidiEvent* ev, uint32_t evc) override {
        const float sr=getSampleRate();
        memset(out[0],0,frames*sizeof(float));
        memset(out[1],0,frames*sizeof(float));
        if(fOn<0.5f)return;
        const float outG=powf(10.f,fLevel*0.05f);
        const float tune=powf(2.f,fTune/12.f);
        const float relT=0.05f*fDecay;           // seconds release, scaled
        const float loA=(fLo>=0.f)?0.12f:0.12f;  // one-pole lp for LO cut
        const float loG=powf(10.f,fLo*0.05f);
        const float hiG=powf(10.f,fHi*0.05f);
        const float hpA=0.995f-0.03f*(fHi>0.f?fHi/12.f:0.f);
        uint32_t pos=0;
        for(uint32_t e=0;e<=evc;e++){
            uint32_t t=(e<evc)?ev[e].frame:frames;
            while(pos<t){
                float s=0;
                for(auto& v:fV) if(v.on){
                    float w=0;
                    if(v.smp){
                        uint32_t i=(uint32_t)v.spos;
                        if(i>=v.slen-1){v.on=false;continue;}
                        float fr=v.spos-i;
                        w=(v.smp[i]+fr*(v.smp[i+1]-v.smp[i]))/32768.f;
                        v.spos+=v.sstep*tune;
                    }
                    float env=1.f;
                    if(v.rel>0.f){env=v.rel; v.rel-=1.f/(sr*relT); if(v.rel<=0.f){v.on=false;continue;}}
                    w*=env*v.vel;
                    /* simple tilt EQ: lp-state for LO, hp approx for HI */
                    v.lp+=0.15f*(w-v.lp);
                    float loPart=v.lp, hiPart=w-v.lp;
                    s+=loPart*loG+hiPart*hiG;
                }
                s*=outG*0.4f;
                if(s>0.85f)s=0.85f+(s-0.85f)*0.25f; else if(s<-0.85f)s=-0.85f+(s+0.85f)*0.25f;
                out[0][pos]=s; out[1][pos]=s;
                if(++fDecim>=3){fDecim=0;if(fScopeN<40)fScope[fScopeN++]=s;}
                ++pos;
            }
            if(e<evc){
                const MidiEvent& m=ev[e];
                if(m.size>=3&&(m.data[0]&0xF0)==0x90&&m.data[2]>0){
                    int pad=m.data[1]-36;
                    if(pad>=0&&pad<16){
                        const PadBank& B=kPadBanks[(int)fBank];
                        /* retrigger: kill same-note voice */
                        for(auto& v:fV) if(v.on&&v.note==m.data[1]){v.on=false;}
                        for(auto& v:fV) if(!v.on){
                            v.on=true; v.note=m.data[1]; v.rel=0.f;
                            v.vel=(m.data[2]/127.f);
                            v.smp=B.p[pad].d; v.slen=B.p[pad].n; v.spos=0.f;
                            v.sstep=22050.f/sr; v.lp=0.f;
                            /* pad hit flash */
                            fPadHit[pad]=1.f;
                            break;
                        }
                    }
                } else if(m.size>=3&&((m.data[0]&0xF0)==0x80||((m.data[0]&0xF0)==0x90&&m.data[2]==0))){
                    for(auto& v:fV) if(v.on&&v.note==m.data[1]&&v.rel==0.f) v.rel=1.f;
                }
            }
        }
        if(fScopeN>=40){
            char buf[512];int p=0;
            for(uint32_t s2=0;s2<fScopeN&&p<230;++s2)p+=snprintf(buf+p,sizeof(buf)-p,"%.3f;",fScope[s2]);
            p+=snprintf(buf+p,sizeof(buf)-p,"|");
            for(int s2=0;s2<16&&p<480;++s2){p+=snprintf(buf+p,sizeof(buf)-p,"%.2f;",fPadHit[s2]);fPadHit[s2]*=0.7f;}
            buf[p]=0;setState("scope",buf);fScopeN=0;
        }
        g2b_post(nullptr,out,frames,sr,fOn,100.f,fGal,&fSf,false);
    }
private:
    PVoice fV[16]={};
    float fBank=0.f,fTune=0.f,fDecay=1.f,fLo=0.f,fHi=0.f,fLevel=0.f,fOn=1.f,fGalaxyV=9.f,fSfeerV=1.f,fDice=0.f;
    unsigned fRnd=31337;
    Galaxy fGal; Sfeer fSf;
    float fScope[40];uint32_t fScopeN=0;int fDecim=0;
    float fPadHit[16]={};
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PadsPlugin)
};
Plugin* createPlugin(){return new PadsPlugin();}
END_NAMESPACE_DISTRHO
