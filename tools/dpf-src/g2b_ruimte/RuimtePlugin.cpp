#include "DistrhoPlugin.hpp"
#include <cstring>
#include <cmath>
#include <cstdint>
#include <algorithm>

START_NAMESPACE_DISTRHO

static const char* kPresetNames[17] = {"G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"};
static const int kNumPresets = 17;
// RUIMTE(0-1), TIJD ms, DEMP Hz, BREEDTE x, PREDELAY ms, OUT dB
static const float kPresetParams[17][6] = {
    {0.30f, 280.f, 6000.f, 1.2f, 12.f, 0.f},   // G2B
    {0.22f, 300.f, 7000.f, 1.1f, 10.f, 0.f},   // DEMBOW
    {0.18f, 220.f, 5500.f, 1.0f, 8.f,  0.f},   // DRILL
    {0.35f, 340.f, 5200.f, 1.3f, 16.f, 0.f},   // RNB
    {0.28f, 250.f, 6500.f, 1.4f, 8.f,  0.f},   // HOUSE
    {0.26f, 270.f, 6200.f, 1.25f,11.f, 0.f},   // AFRO
    {0.20f, 260.f, 5000.f, 1.1f, 10.f, 0.f},   // TRAP
    {0.32f, 320.f, 5500.f, 1.0f, 14.f, 0.f},   // BOOM
    {0.24f, 240.f, 6800.f, 1.5f, 8.f,  0.f},   // UKG
    {0.22f, 200.f, 7000.f, 1.3f, 8.f,  0.f},   // JERSEY
    {0.27f, 260.f, 6300.f, 1.3f, 10.f, 0.f},   // BOUNCE
    {0.38f, 360.f, 4800.f, 1.2f, 18.f, 0.f},   // SOUL
    {0.30f, 300.f, 4500.f, 1.0f, 15.f, 0.f},   // LOFI
    {0.25f, 230.f, 6400.f, 1.4f, 9.f,  0.f},   // FUNK
    {0.34f, 340.f, 5600.f, 1.3f, 16.f, 0.f},   // REGGAE
    {0.26f, 250.f, 6600.f, 1.4f, 8.f,  0.f},   // DANCE
    {0.29f, 290.f, 6000.f, 1.25f, 12.f, 0.f},  // KAWINA

};


// ---- GALAXY-knop (bron: backend/app/galaxy_fx.py, devin-test) — 27 posities ----
struct GxShifter {
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
struct Galaxy { Galaxy(){reset();}
    GxShifter sh[4];
    static const int CN=2200;
    float cb[4][CN]; int cw[4];
    float ap[2][600]; int aw[2];
    float lp[2];
    int gpos=-1; float grate=2.f,gfb=0.5f,gfc=6500.f,gwet=0.6f,gdd=0.2f; int gmode=2;
    void reset(){for(int i=0;i<4;i++)sh[i].reset();std::memset(cb,0,sizeof cb);std::memset(ap,0,sizeof ap);
                 std::memset(cw,0,sizeof cw);std::memset(aw,0,sizeof aw);lp[0]=lp[1]=0;}
    void setPos(int pos){
        if(pos==gpos)return; gpos=pos;
        gmode=pos/9; int dim=pos%9;
        uint32_t sd=(gmode==0?10007u:(gmode==1?20011u:30017u))*9u+dim;
        auto rng=[&](){sd^=sd<<13;sd^=sd>>17;sd^=sd<<5;return (sd&0xFFFFFFu)/16777216.f;};
        grate=powf(2.f,(gmode==0?-12.f:12.f)/12.f);
        gdd=(gmode==0?0.20f:(gmode==1?0.16f:0.24f))*(0.8f+0.4f*rng());
        gfb=(gmode==0?0.50f:(gmode==1?0.40f:0.55f))*(0.85f+0.3f*rng()); if(gfb>0.72f)gfb=0.72f;
        gfc=(gmode==0?2600.f:(gmode==1?9000.f:6500.f))*(0.85f+0.35f*rng());
        gwet=(gmode==0?0.55f:(gmode==1?0.50f:0.62f));
    }
    void tick(float& L,float& R,float sr){
        float in[2]={L,R},shim[2]={0,0};
        for(int c=0;c<2;c++){
            float x=sh[c].tick(in[c],grate);
            if(gmode==2)x+=0.6f*sh[2+c].tick(in[c],1.4983f);
            float a=expf(-6.283f*(gmode==0?gfc:320.f)/sr);
            lp[c]=(1.f-a)*x+a*lp[c];
            x=(gmode==0)?lp[c]:(x-lp[c]);
            int dd=(int)(gdd*sr); if(dd<64)dd=64; if(dd>CN-8)dd=CN-8;
            float tail=0;
            for(int k=0;k<4;k++){
                int rp=(cw[k]-dd-(dd*k/4)+CN*8)%CN;
                float e=cb[k][rp]; tail+=e*0.25f;
                cb[k][cw[k]]=x+e*gfb; cw[k]=(cw[k]+1)%CN;
            }
            const float g=0.5f;
            float av=ap[c][aw[c]];
            float y=-g*tail+av;
            ap[c][aw[c]]=tail+g*y; aw[c]=(aw[c]+1)%600;
            shim[c]=y;
        }
        L=in[0]+gwet*shim[0]*0.10f; R=in[1]+gwet*shim[1]*0.10f;
        if(L>0.98f)L=0.98f; else if(L<-0.98f)L=-0.98f;
        if(R>0.98f)R=0.98f; else if(R<-0.98f)R=-0.98f;
    }
};

class RuimtePlugin : public Plugin {
public:
    enum Params { kRuimte, kTijd, kDemp, kBreedte, kPredelay, kOutput, kGalaxy, kDice, kPreset, kOn, kKracht, kNumParams };
    RuimtePlugin() : Plugin(kNumParams,0,3) {
        for (int i=0;i<kMaxDel;i++){fBufL[i]=0.f;fBufR[i]=0.f;}
        for (int i=0;i<kMaxPre;i++){fPreL[i]=0.f;fPreR[i]=0.f;}
    }
protected:
    const char* getLabel() const override { return "G2B Ruimte"; }
    const char* getDescription() const override { return "Ruimte: delay+reverb in één — jouw geluid krijgt plek"; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','R','u'); }

    void initParameter(uint32_t index, Parameter& p) override {
        if(index==kGalaxy){p.name="GALAXY";p.symbol="galaxy";p.ranges.min=0;p.ranges.max=26;p.ranges.def=9;p.hints|=kParameterIsInteger;return;}
        switch(index){
        case kRuimte:   p.name="RUIMTE";   p.symbol="ruimte";   p.hints=kParameterIsAutomatable; p.ranges.min=0.f; p.ranges.max=1.f; p.ranges.def=0.30f; p.unit="%"; break;
        case kTijd:     p.name="TIJD";     p.symbol="tijd";     p.hints=kParameterIsAutomatable; p.ranges.min=50.f; p.ranges.max=2000.f; p.ranges.def=280.f; p.unit="ms"; break;
        case kDemp:     p.name="DEMP";     p.symbol="demp";     p.hints=kParameterIsAutomatable; p.ranges.min=1000.f; p.ranges.max=12000.f; p.ranges.def=6000.f; p.unit="Hz"; break;
        case kBreedte:  p.name="BREEDTE";  p.symbol="breedte";  p.hints=kParameterIsAutomatable; p.ranges.min=0.5f; p.ranges.max=2.f; p.ranges.def=1.2f; p.unit="x"; break;
        case kPredelay: p.name="PREDELAY"; p.symbol="predelay"; p.hints=kParameterIsAutomatable; p.ranges.min=0.f; p.ranges.max=100.f; p.ranges.def=12.f; p.unit="ms"; break;
        case kOutput:   p.name="OUTPUT";   p.symbol="output";   p.hints=kParameterIsAutomatable; p.ranges.min=-24.f; p.ranges.max=12.f; p.ranges.def=0.f; p.unit="dB"; break;
        case kDice: p.name="DICE"; p.symbol="dice"; p.hints|=kParameterIsInteger|kParameterIsTrigger; p.ranges.min=0; p.ranges.max=1; p.ranges.def=0; break;
        case kPreset:   p.name="PRESET";   p.symbol="preset";   p.hints=kParameterIsAutomatable|kParameterIsInteger; p.ranges.min=0.f; p.ranges.max=16.f; p.ranges.def=0.f; break;
        case kOn: p.name="Aan"; p.symbol="aan"; p.hints|=kParameterIsBoolean; p.ranges.min=0.f; p.ranges.max=1.f; p.ranges.def=1.f; break;
        case kKracht: p.name="Kracht"; p.symbol="kracht"; p.ranges.min=0.f; p.ranges.max=100.f; p.ranges.def=100.f; p.unit="%"; break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        if(i==kGalaxy)return fGalaxy;
        switch(i){case kRuimte:return fRuimte;case kTijd:return fTijd;case kDemp:return fDemp;case kBreedte:return fBreedte;case kPredelay:return fPredelay;case kOutput:return fOutput;case kDice: return fDice; case kPreset: return fPreset;case kOn:return fOn;case kKracht:return fKr;} return 0.f;
    }
    void setParameterValue(uint32_t i, float v) override {
        if(i==kGalaxy){fGalaxy=v;galaxy.setPos((int)v);return;}
        if (i==kPreset){ applyPreset((int)(v+0.5f)); return; }
        switch(i){case kRuimte:fRuimte=v;break;case kTijd:fTijd=v;break;case kDemp:fDemp=v;break;case kBreedte:fBreedte=v;break;case kPredelay:fPredelay=v;break;case kOutput:fOutput=v;break;case kDice: if(v>0.5f)rollDice(); fDice=0.f; break;case kOn:fOn=v;break;case kKracht:fKr=v;break;}
        fDirty=true;
    }
    void initState(uint32_t index, String& name, String& def) override {
        if(index==0){name="scope";def="";}
        else if(index==2){name="knobs";def="";}
        else {name="presetnames";def=""; for(int i=0;i<6;i++){def+=kPresetNames[i]; if(i<5)def+=";";}}

    }
    void setState(const char* key, const char* value) override {
        (void)key; (void)value;
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        if(fOn<0.5f){if(inputs[0]!=outputs[0])std::memcpy(outputs[0],inputs[0],frames*sizeof(float));if(inputs[1]!=outputs[1])std::memcpy(outputs[1],inputs[1],frames*sizeof(float));return;}
        processFx_(inputs, outputs, frames);
        for(uint32_t i=0;i<frames;i++){
            float L=outputs[0][i],R=outputs[1][i];
            galaxy.tick(L,R,getSampleRate());
            outputs[0][i]=L; outputs[1][i]=R;
        }
        const float k=fKr/100.f;
        if(k<0.999f){const float d=1.f-k;for(uint32_t i=0;i<frames;i++){outputs[0][i]=inputs[0][i]*d+outputs[0][i]*k;outputs[1][i]=inputs[1][i]*d+outputs[1][i]*k;}}
    }

    void processFx_(const float** inputs, float** outputs, uint32_t frames) {
        const float sr = getSampleRate();
        const int delSamps = (int)(fTijd*0.001f*sr) < kMaxDel ? (int)(fTijd*0.001f*sr) : kMaxDel-1;
        const int preSamps = (int)(fPredelay*0.001f*sr) < kMaxPre ? (int)(fPredelay*0.001f*sr) : kMaxPre-1;
        const float fb = fRuimte*0.72f;                 // feedback = ruimte-hoeveelheid
        const float wet = fRuimte;
        const float lpA = 1.f-expf(-2.f*M_PI*fDemp/sr); // demp = lowpass in feedback
        const float outG = powf(10.f, fOutput*0.05f);
        const float wMix = 0.35f;                        // vaste mix fractie v.d. feedloop als "reverb-tail"

        for (uint32_t i=0;i<frames;i++){
            float inL = inputs[0][i], inR = inputs[1][i];
            // predelay
            fPreL[fPreW]=inL; fPreR[fPreW]=inR;
            int preR = (fPreW - preSamps + kMaxPre)%kMaxPre;
            float pL=fPreL[preR], pR=fPreR[preR];
            fPreW=(fPreW+1)%kMaxPre;
            // delay+feedback met demp-filter (2 taps iets verspr = reverb-sfeer)
            int rd1=(fDelW - delSamps + kMaxDel)%kMaxDel;
            int rd2=(fDelW - (delSamps*61/100) + kMaxDel)%kMaxDel;   // tweede tap (0.61×) verdicht de staart
            float dL=fBufL[rd1]+0.5f*fBufL[rd2];
            float dR=fBufR[rd1]+0.5f*fBufR[rd2];
            fLpL += lpA*(dL-fLpL); fLpR += lpA*(dR-fLpR);
            fBufL[fDelW]=pL + fLpL*fb; fBufR[fDelW]=pR + fLpR*fb;
            fDelW=(fDelW+1)%kMaxDel;
            float wetL=dL*wet, wetR=dR*wet;
            // breedte: mid/side op nat deel
            float m=(wetL+wetR)*0.5f, s=(wetL-wetR)*0.5f*fBreedte;
            wetL=m+s; wetR=m-s;
            float oL=(inL*(1.f-wet*wMix) + wetL)*outG;
            float oR=(inR*(1.f-wet*wMix) + wetR)*outG;
            outputs[0][i]=oL; outputs[1][i]=oR;
            if (++fDecim>=3){ fDecim=0; if (fScopeN<kScopeLen) fScope[fScopeN++]=(oL+oR)*0.5f; }
        }
        if (fScopeN>=40){
            char buf[256]; int pos=0;
            for(uint32_t s=0;s<fScopeN && pos<230;++s) pos+=snprintf(buf+pos,sizeof(buf)-pos,"%.3f;",fScope[s]);
            buf[pos]=0; setState("scope",buf); fScopeN=0;
        }
    }
private:
    void pushKnobs(){
        char buf[160];
        snprintf(buf,sizeof buf,"%.4f;%.4f;%.4f;%.4f;%.4f;%.4f;%.4f;%.4f",fRuimte,fTijd,fDemp,fBreedte,fPredelay,fOutput,fGalaxy,fKr);
        setState("knobs",buf);
    }
    void rollDice(){
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kRuimte,0.1f+((fRnd>>8)%100)/100.f*0.7f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kTijd,80.f+((fRnd>>8)%100)/100.f*800.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kDemp,2000.f+((fRnd>>8)%100)/100.f*8000.f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kBreedte,0.7f+((fRnd>>8)%100)/100.f*1.2f);
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kPredelay,((fRnd>>8)%100)/100.f*40.f);
        pushKnobs();
    }
    float fDice = 0.f;
    unsigned fRnd = 998877;

    void applyPreset(int i){
        if(i<0)i=0; if(i>16)i=16;
        fPreset=(float)i;
        setParameterValue(kRuimte,kPresetParams[i][0]);
        setParameterValue(kTijd,kPresetParams[i][1]);
        setParameterValue(kDemp,kPresetParams[i][2]);
        setParameterValue(kBreedte,kPresetParams[i][3]);
        setParameterValue(kPredelay,kPresetParams[i][4]);
        setParameterValue(kOutput,kPresetParams[i][5]);
        pushKnobs();
    }
    static const int kMaxDel = 96000*2;   // tot 2s @48k
    static const int kMaxPre = 4800;      // 100ms predelay
    float fRuimte=0.30f, fTijd=280.f, fDemp=6000.f, fBreedte=1.2f, fPredelay=12.f, fOutput=0.f, fPreset=0.f;
    float fBufL[kMaxDel], fBufR[kMaxDel];
    float fPreL[kMaxPre], fPreR[kMaxPre];
    float fLpL=0.f, fLpR=0.f;
    int fDelW=0, fPreW=0;
    static const uint32_t kScopeLen = 40;
    float fScope[40];
    uint32_t fScopeN=0;
    int fDecim=0;
    bool fDirty=true;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RuimtePlugin)
    float fOn=1.f,fKr=100.f,fGalaxy=9.f;
    Galaxy galaxy;
};

Plugin* createPlugin(){ return new RuimtePlugin(); }
END_NAMESPACE_DISTRHO
