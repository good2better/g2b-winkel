// G2B KOOR+KEY — vocal → koor-generator + toonsoort-detectie in één.
// Vocal erin → lead + harmonie-stemmen eruit (delay-pitchshifters, podium-breedte).
// Toonsoort: leert uit MIDI-in (Krumhansl, zelfde als SLEUTEL-helper) of kies vast.
// © G2B (good2better) — MIT.
#include "DistrhoPlugin.hpp"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdint>

START_NAMESPACE_DISTRHO

// delay-gebaseerde pitch-shifter (zelfde principe als GALAXY in FX)
struct KoorShift {
    static const int N=2048; float buf[N]; int wp=0; float ph=0;
    void reset(){std::memset(buf,0,sizeof buf);wp=0;ph=0;}
    float tick(float in,float rate){
        buf[wp]=in; float acc=0;
        for(int t=0;t<2;t++){
            float p=ph+0.5f*t; if(p>=1.f)p-=1.f;
            float rd=(float)wp-p*(float)N;
            int i0=(int)floorf(rd); float fr=rd-i0;
            float a=buf[((i0%N)+N)%N],b=buf[(((i0+1)%N)+N)%N];
            float w=p<0.5f?p*2.f:2.f-p*2.f;
            acc+=(a+(b-a)*fr)*w;
        }
        ph+=(1.f-rate)/(float)N; if(ph<0)ph+=1.f; if(ph>=1.f)ph-=1.f;
        wp=(wp+1)%N;
        return acc;
    }
};

// Krumhansl-Schmuckler profielen (publiek-domein constanten)
static const float kProfMaj[12]={6.35f,2.23f,3.48f,2.33f,4.38f,4.09f,2.52f,5.19f,2.39f,3.66f,2.29f,2.88f};
static const float kProfMin[12]={6.33f,2.68f,3.52f,5.38f,2.60f,3.53f,2.54f,4.75f,3.98f,2.69f,3.34f,3.17f};

class KoorPlugin : public Plugin
{
public:
    enum { kOn,kPreset,kKey,kBreed,kKracht,kDetune,kOutKey,kNumParams };
    KoorPlugin() : Plugin(kNumParams,0,0) { resetVoices_(); }

protected:
    const char* getLabel() const override { return "G2B KOOR+KEY"; }
    const char* getDescription() const override
    { return "G2B koor-machine: vocal → lead+harmonie-stemmen. Toonsoort via MIDI-learn of vast. Presets: duo/triak/gospel/cathedral."; }
    const char* getMaker() const override { return "G2B (good2better)"; }
    const char* getHomePage() const override { return "https://github.com/good2better"; }
    const char* getLicense() const override { return "MIT"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','K','r'); }

    void initState(uint32_t, String&, String&) override {}
    void setState(const char*, const char*) override {}

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints=kParameterIsAutomatable;
        switch(i){
        case kOn:    p.name="AAN";        p.symbol="aan";    p.ranges.min=0;p.ranges.max=1;   p.ranges.def=1;   p.hints|=kParameterIsBoolean;break;
        case kPreset:p.name="KOOR";       p.symbol="koor";   p.ranges.min=0;p.ranges.max=3;   p.ranges.def=1;   p.hints|=kParameterIsInteger;break;
        case kKey:   p.name="TOONSOORT";  p.symbol="key";    p.ranges.min=0;p.ranges.max=24;  p.ranges.def=0;   p.hints|=kParameterIsInteger;break;
        case kBreed: p.name="BREEDTE";    p.symbol="breed";  p.ranges.min=0;p.ranges.max=1;   p.ranges.def=.7f; p.unit="%";break;
        case kKracht:p.name="KRACHT";     p.symbol="kracht"; p.ranges.min=0;p.ranges.max=1;   p.ranges.def=.6f; p.unit="%";break;
        case kDetune:p.name="DETUNE";     p.symbol="detune"; p.ranges.min=0;p.ranges.max=1;   p.ranges.def=.5f; p.unit="%";break;
        case kOutKey:p.name="key-out";    p.symbol="okey";   p.ranges.min=-1;p.ranges.max=23; p.hints|=kParameterIsInteger|kParameterIsOutput|kParameterIsHidden;break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){
        case kOn:return fOn; case kPreset:return fPreset; case kKey:return fKey;
        case kBreed:return fBreed; case kKracht:return fKracht; case kDetune:return fDetune;
        case kOutKey:return keyIdx_<0?-1.f:(float)(keyIdx_*2+minor_);}return 0.f;
    }
    void setParameterValue(uint32_t i,float v) override {
        switch(i){
        case kOn:fOn=v;break; case kPreset:fPreset=v;break; case kKey:fKey=v;break;
        case kBreed:fBreed=v;break; case kKracht:fKracht=v;break; case kDetune:fDetune=v;break;
        }
    }

    void run(const float** in, float** out, uint32_t frames,
             const MidiEvent* ev, uint32_t nEv) override
    {
        // MIDI-learn: note-ons → chroma-histogram → Krumhansl
        for(uint32_t e=0;e<nEv;e++){
            const MidiEvent&m=ev[e];
            if(m.size<3)continue;
            const uint8_t st=m.data[0]&0xF0;
            if(st==0x90&&m.data[2]>0){ pcHist_[m.data[1]%12]+=m.data[2]/127.f; newKey_=true; }
            else if((st==0x80||(st==0x90&&m.data[2]==0)) && pcHist_[m.data[1]%12]>0.01f)
                pcHist_[m.data[1]%12]*=0.6f; // note-off laat meedooselen
        }
        if(newKey_){newKey_=false;analyse_();}
        for(int i=0;i<12;i++)pcHist_[i]*=std::pow(0.5f,frames/(4.f*getSampleRate())); // langzaam vervagen

        const float on=fOn>0.5f?1.f:0.f;
        // key bepalen: vast (kKey>0) of AUTO (detectie, fallback C majeur)
        int key,minr;
        if((int)fKey>0){const int k=(int)fKey-1;key=k%12;minr=k/12;}
        else{key=keyIdx_<0?0:keyIdx_;minr=minor_;}
        const int t3=minr?3:4;
        int offs[5];int nv=0;
        switch((int)fPreset){
        case 0:{offs[nv++]=t3;break;}
        case 1:{offs[nv++]=t3;offs[nv++]=7;break;}
        case 2:{offs[nv++]=t3;offs[nv++]=7;offs[nv++]=12;break;}
        default:{offs[nv++]=t3;offs[nv++]=7;offs[nv++]=12;offs[nv++]=-12;break;}
        }
        // stem-rates + pan één keer per blok (goedkoper dan per-sample)
        float rates[4],pL[4],pR[4];
        for(int v=0;v<4;v++){
            rates[v]=std::pow(2.f,(offs[v]+detCents_[v]*fDetune)/12.f);
            const float a=(panBase_[v]*fBreed+1.f)*float(M_PI)/4.f;
            pL[v]=std::cos(a);pR[v]=std::sin(a);
        }
        for(int i=0;i<frames;i++){
            const float xm=(in[0][i]+in[1][i])*0.5f;
            float L=in[0][i],R=in[1][i];
            if(on>0.f){
                for(int v=0;v<nv;v++){
                    float x=sh_[v].tick(xm,rates[v]);
                    // pre-delay per stem
                    const int dd=dly_[v];
                    float dv=dlyBuf_[v][(dlyWp_[v]-dd+KoorPlugin::DLN)%DLN];
                    dlyBuf_[v][dlyWp_[v]]=x; dlyWp_[v]=(dlyWp_[v]+1)%DLN;
                    // body-filter: achtergrond-stemmen dofder
                    lp_[v]+=lpA_[v]*(dv-lp_[v]);
                    const float g=fKracht*(0.45f+0.10f*v);
                    L+=lp_[v]*g*pL[v]; R+=lp_[v]*g*pR[v];
                }
            }
            out[0][i]=L;out[1][i]=R;
        }
    }

private:
    static const int DLN=8192;
    KoorShift sh_[4];
    float dlyBuf_[4][DLN]; int dlyWp_[4]={0,0,0,0},dly_[4];
    float lp_[4]={0,0,0,0},lpA_[4],panBase_[4],detCents_[4];
    float fOn=1,fPreset=1,fKey=0,fBreed=.7f,fKracht=.6f,fDetune=.5f;
    float pcHist_[12]={0}; bool newKey_=false;
    int keyIdx_=-1,minor_=0;

    void resetVoices_(){
        for(int v=0;v<4;v++){
            sh_[v].reset();std::memset(dlyBuf_[v],0,sizeof dlyBuf_[v]);
            // vaste karakter per stem: detune, delay, pan-positie, filter
            detCents_[v]=(v%2?1.f:-1.f)*(6.f+5.f*v);              // cents
            dly_[v]=(18+9*v)*44;                                    // ~18..45ms @44.1k
            panBase_[v]=(v%2?1.f:-1.f)*(0.35f+0.18f*v);
            lpA_[v]=1.f-std::exp(-6.283f*(2800.f+1200.f*v)/44100.f);
        }
    }

    void analyse_(){
        // Krumhansl-correlatie op chroma-histogram
        float mx=0.01f;for(int i=0;i<12;i++)if(pcHist_[i]>mx)mx=pcHist_[i];
        float best=-2.f;int bk=-1,bm=0;
        for(int k=0;k<12;k++){
            for(int m=0;m<2;m++){
                const float*prof=m?kProfMin:kProfMaj;
                float a=0,b=0,ab=0,a2=0,b2=0;
                for(int i=0;i<12;i++){const float p=prof[(i-k+12)%12],c=pcHist_[i]/mx;
                    a+=p;b+=c;ab+=p*c;a2+=p*p;b2+=c*c;}
                const float den=std::sqrt((12.f*a2-a*a)*(12.f*b2-b*b));
                const float r=den>0?(12.f*ab-a*b)/den:-1.f;
                if(r>best){best=r;bk=k;bm=m;}
            }
        }
        if(bk!=keyIdx_||bm!=minor_){
            keyIdx_=bk;minor_=bm;
            requestParameterValueChange(kOutKey,(float)(bk*2+bm));
        }
    }
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KoorPlugin)
};

Plugin* createPlugin() { return new KoorPlugin(); }

END_NAMESPACE_DISTRHO
