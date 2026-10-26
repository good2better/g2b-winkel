/* G2B Best Of — Giorgio's eigen multi-sample bank (privé, nooit verkopen).
 * 24 instrumenten, speelbaar over heel het keyboard, samples ingebakken.
 * Knoppen: INSTR | KRACHT | OUTPUT | DIMENTIES | SFEER | AAN | DICE
 */
#include "DistrhoPlugin.hpp"

#include "../g2b_std.h"
#include "bestof_verk_data.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>

START_NAMESPACE_DISTRHO

struct IVoice { bool on=false; int note=0; float vel=0.f,t=0.f; const int16_t* smp=nullptr; uint32_t slen=0; float spos=0.f,sstep=0.f,rel=0.f; };

class InstrPlugin : public Plugin {
public:
    InstrPlugin() : Plugin(kNumParams, 0, 2) {}
protected:
    const char* getLabel() const override { return "G2B Best Of"; }
    const char* getMaker() const override { return "Good2Better"; }
    const char* getLicense() const override { return "G2B"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','B','S'); }
    enum { kInstr=0,kKracht,kLevel,kOn,kGalaxy,kSfeer,kDice,kNumParams };
    void initParameter(uint32_t i, Parameter& p) override {
        p.ranges.def=0.f; p.hints=kParameterIsAutomatable;
        switch(i){
        case kInstr: p.name="INSTRUMENT"; p.symbol="instr"; p.hints|=kParameterIsInteger; p.ranges.min=0;p.ranges.max=kInstrCount-1;p.ranges.def=0;break;
        case kKracht:p.name="KRACHT"; p.symbol="kracht"; p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.8f;break;
        case kLevel: p.name="OUTPUT"; p.symbol="level"; p.ranges.min=-24;p.ranges.max=12;p.ranges.def=0;p.unit="dB";break;
        case kOn:    p.name="Aan";  p.symbol="aan"; p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;break;
        case kGalaxy:p.name="DIMENTIES";p.symbol="dim";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=26;p.ranges.def=9;break;
        case kSfeer: p.name="Sfeer";p.symbol="sfeer";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=2;p.ranges.def=1;break;
        case kDice:  p.name="DICE"; p.symbol="dice"; p.hints|=kParameterIsInteger|kParameterIsTrigger;p.ranges.min=0;p.ranges.max=1;break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){case kInstr:return fInstr;case kKracht:return fKracht;case kLevel:return fLevel;case kOn:return fOn;case kGalaxy:return fGalaxyV;case kSfeer:return fSfeerV;case kDice:return fDice;}return 0.f;
    }
    void setParameterValue(uint32_t i, float v) override {
        switch(i){
        case kInstr:fInstr=v;break;case kKracht:fKracht=v;break;case kLevel:fLevel=v;break;
        case kOn:fOn=v;break;
        case kGalaxy:fGalaxyV=v;fGal.setPos((int)(v+0.5f));break;
        case kSfeer:fSfeerV=v;fSf.setMode((int)(v+0.5f));break;
        case kDice:fDice=v;if(v>0.5f){fRnd=fRnd*1664525u+1013904223u;fInstr=(float)((fRnd>>9)%kInstrCount);}fDice=0.f;break;
        }
    }
    void initState(uint32_t index, String& name, String& def) override {
        if(index==0){name="scope";def="";}
        else{name="instrnames";def="";for(int i=0;i<kInstrCount;i++){def+=kInstrNames[i];if(i<kInstrCount-1)def+=";";}}
    }
    void setState(const char*, const char*) override {}
    void run(const float**, float** out, uint32_t frames, const MidiEvent* ev, uint32_t evc) override {
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
                    v.t+=1.f/sr;
                    float w=0;
                    if(v.smp){
                        uint32_t i=(uint32_t)v.spos;
                        if(i>=v.slen-1){v.on=false;continue;}
                        float fr=v.spos-i;
                        w=(v.smp[i]+fr*(v.smp[i+1]-v.smp[i]))/32768.f;
                        v.spos+=v.sstep;
                    }
                    float env=1.f;
                    if(v.rel>0.f){env=v.rel; v.rel-=1.f/(sr*0.35f); if(v.rel<=0.f){v.on=false;continue;}}
                    s+=w*env*v.vel;
                }
                s*=outG*0.45f;
                if(s>0.85f)s=0.85f+(s-0.85f)*0.25f; else if(s<-0.85f)s=-0.85f+(s+0.85f)*0.25f;
                out[0][pos]=s; out[1][pos]=s;
                if(++fDecim>=3){fDecim=0;if(fScopeN<40)fScope[fScopeN++]=s;}
                ++pos;
            }
            if(e<evc){
                const MidiEvent& m=ev[e];
                if(m.size>=3&&(m.data[0]&0xF0)==0x90&&m.data[2]>0){
                    for(auto& v:fV) if(!v.on){
                        uint32_t slen=0; int src=0;
                        const int16_t* d=instr_pick((int)fInstr, m.data[1], m.data[2]/127.f, &slen, &src);
                        if(!d) break;
                        v.on=true; v.note=m.data[1]; v.rel=0.f;
                        v.vel=(m.data[2]/127.f)*(0.4f+0.6f*fKracht);
                        v.t=0; v.smp=d; v.slen=slen; v.spos=0.f;
                        float f0=440.f*powf(2.f,(m.data[1]-69)/12.f), fs=440.f*powf(2.f,(src-69)/12.f);
                        v.sstep=(f0/fs)*(22050.f/sr);
                        break;
                    }
                } else if(m.size>=3&&((m.data[0]&0xF0)==0x80||((m.data[0]&0xF0)==0x90&&m.data[2]==0))){
                    for(auto& v:fV) if(v.on&&v.note==m.data[1]&&v.rel==0.f) v.rel=1.f;
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
    IVoice fV[16]={};
    float fInstr=0.f,fKracht=0.8f,fLevel=0.f,fOn=1.f,fGalaxyV=9.f,fSfeerV=1.f,fDice=0.f;
    unsigned fRnd=515151;
    Galaxy fGal; Sfeer fSf;
    float fScope[40];uint32_t fScopeN=0;int fDecim=0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstrPlugin)
};
Plugin* createPlugin(){return new InstrPlugin();}
END_NAMESPACE_DISTRHO
