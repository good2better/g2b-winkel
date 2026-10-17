/*
 * G2B Poets — "één dikke draai, alles erin".
 * DSP: dezelfde POETS-macro als in G2B Sound Lab —
 *   punch  = transient-versterker (in - langzaam-gemiddelde, 8 ms)
 *   body   = laag-boost via LP 180 Hz
 *   sizzle = air-boost via HP 10 kHz
 *   glue   = zachte tanh-verzadiger
 * Stuurt elke ~45 ms een gedecimeerde golfvorm + punch-meting naar de UI
 * (state "scope" / "punch") zodat je ziet wat hij doet.
 */

#include "DistrhoPlugin.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

START_NAMESPACE_DISTRHO

static const char* kPresetNames[17] = {"G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"};
static const int kNumPresets = 17;
static const float kPresetParams[17][3] = {
    { 45, 0, 10000 },    // G2B
    { 60, 0, 9000 },     // DEMBOW
    { 70, -0.5, 8000 },  // DRILL — sizzle corto, oscuro
    { 30, 0, 11000 },    // RNB
    { 50, 0, 10000 },    // HOUSE
    { 40, 0, 9000 },     // AFRO
    { 65, -0.5, 8500 },  // TRAP
    { 35, 0, 9000 },     // BOOM
    { 55, 0, 11000 },    // UKG — brillo
    { 75, -1, 10000 },   // JERSEY
    { 50, 0, 10500 },    // BOUNCE
    { 40, 0, 8500 },     // SOUL
    { 30, 0, 6000 },     // LOFI — sizzle recortado
    { 60, -0.5, 9500 },  // FUNK
    { 48, 0, 8000 },     // REGGAE
    { 58, 0, 11000 },    // DANCE
    { 52, 0, 9000 },     // KAWINA
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

class PoetsPlugin : public Plugin
{
public:
    enum Parameters {
        kPoets = 0,   // POETS % — de dikke draai
        kOut,         // OUTPUT dB
        kGalaxy, kDice, kPreset,
        kFokus,       // FOKUS Hz — waar de sizzle-band begint
        kOn, kKracht,
        kParameterCount
    };

    PoetsPlugin()
        : Plugin(kParameterCount, 0, 0),
          fPoets(35.0f), fOutDb(0.0f), fFokus(10000.0f),
          fLpPunchL(0), fLpPunchR(0), fLpBodyL(0), fLpBodyR(0),
          fLpAirL(0), fLpAirR(0),
          fAPunch(0), fABody(0), fAAir(0), fOutGain(1.0f),
          fPunchMeter(0.0f), fDecim(0), fScopeN(0)
    {
        recalc();
    }

protected:
    const char* getLabel() const override { return "G2B Poets"; }
    const char* getDescription() const override
    {
        return "Één dikke draai: punch + body + sizzle tegelijk. "
               "Maakt je mix vet, helder en hard — G2B-stijl.";
    }
    const char* getMaker() const override { return "G2B (good2better)"; }
    const char* getHomePage() const override { return "https://github.com/good2better"; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','P','t'); }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        if(index==kGalaxy){parameter.name="GALAXY";parameter.symbol="galaxy";parameter.ranges.min=0;parameter.ranges.max=26;parameter.ranges.def=9;parameter.hints|=kParameterIsInteger;return;}
        parameter.hints = kParameterIsAutomatable;
        switch (index)
        {
        case kPoets:
            parameter.name = "Poets";
            parameter.shortName = "Poets";
            parameter.symbol = "poets";
            parameter.unit = "%";
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 100.0f;
            parameter.ranges.def = 35.0f;
            break;
        case kOut:
            parameter.name = "Output";
            parameter.shortName = "Output";
            parameter.symbol = "out";
            parameter.unit = "dB";
            parameter.ranges.min = -6.0f;
            parameter.ranges.max = 6.0f;
            parameter.ranges.def = 0.0f;
            break;
        case kDice: parameter.name="Dice"; parameter.symbol="dice"; parameter.hints=kParameterIsInteger|kParameterIsTrigger; parameter.ranges={0.0f,1.0f,0.0f}; break;
        case kPreset:
            parameter.name = "Preset";
            parameter.symbol = "preset";
            parameter.unit = "";
            parameter.hints = kParameterIsInteger;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 16.0f;
            parameter.ranges.def = 0.0f;
            break;
        case kFokus:
            parameter.name = "Fokus";
            parameter.shortName = "Fokus";
            parameter.symbol = "fokus";
            parameter.unit = "Hz";
            parameter.ranges.min = 4000.0f;
            parameter.ranges.max = 16000.0f;
            parameter.ranges.def = 10000.0f;
            break;
        case kOn: parameter.name="Aan"; parameter.symbol="aan"; parameter.hints=kParameterIsAutomatable|kParameterIsBoolean; parameter.ranges={0.0f,1.0f,1.0f}; break;
        case kKracht: parameter.name="Kracht"; parameter.symbol="kracht"; parameter.unit="%"; parameter.ranges={0.0f,100.0f,100.0f}; break;
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        if(index==kGalaxy)return fGalaxy;
        switch (index)
        {
        case kPoets: return fPoets;
        case kOut:   return fOutDb;
        case kFokus: return fFokus;
        case kDice: return fDice; case kPreset: return fPreset;
        case kOn: return fOn; case kKracht: return fKr;
        }
        return 0.0f;
    }

    void setParameterValue(uint32_t index, float value) override
    {
        if(index==kGalaxy){fGalaxy=value;galaxy.setPos((int)value);return;}
        if (index == kPreset) { fPreset = value; applyPreset((int)value); return; }
        switch (index)
        {
        case kPoets: fPoets = value; break;
        case kOut:   fOutDb = value; break;
        case kFokus: fFokus = value; break;
        case kDice: if(value>0.5f)rollDice(); fDice=0.f; break;
        case kOn: fOn = value; break; case kKracht: fKr = value; break;
        }
        recalc();
    }

    void activate() override
    {
        fLpPunchL = fLpPunchR = fLpBodyL = fLpBodyR = fLpAirL = fLpAirR = 0.0f;
        fPunchMeter = 0.0f;
        fScopeN = 0;
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

    void processFx_(const float** inputs, float** outputs, uint32_t frames)
    {
        const float* inL = inputs[0];
        const float* inR = inputs[1];
        float* outL = outputs[0];
        float* outR = outputs[1];

        const float a   = fPoets / 100.0f;
        const float ap  = fAPunch, ab = fABody, ah = fAAir;
        const float g   = fOutGain;
        const float drv = 1.0f + 0.4f * a;
        const float mk  = 1.0f - 0.10f * a;   // makeup na de tanh

        float ppL = fLpPunchL, ppR = fLpPunchR;
        float pbL = fLpBodyL,  pbR = fLpBodyR;
        float phL = fLpAirL,   phR = fLpAirR;
        float meter = fPunchMeter;

        for (uint32_t i = 0; i < frames; ++i)
        {
            // ---- L ----
            float l = inL[i];
            ppL += ap * (l - ppL);              // ~8 ms gemiddelde
            float transL = l - ppL;             // transient-deel
            pbL += ab * (l - pbL);              // sub-band <180 Hz
            phL += ah * (l - phL);              // lp 10 k → hp = l - ph
            float yL = l + transL * 0.9f * a + pbL * 0.45f * a + (l - phL) * 0.55f * a;
            // ---- R ----
            float r = inR[i];
            ppR += ap * (r - ppR);
            float transR = r - ppR;
            pbR += ab * (r - pbR);
            phR += ah * (r - phR);
            float yR = r + transR * 0.9f * a + pbR * 0.45f * a + (r - phR) * 0.55f * a;

            // glue: tanh + lichte makeup
            yL = std::tanh(yL * drv) * mk * g;
            yR = std::tanh(yR * drv) * mk * g;

            outL[i] = yL;
            outR[i] = yR;

            // punch-meter: hoeveel transient-energie loopt er nu
            const float t = std::fabs(transL) + std::fabs(transR);
            meter += (t > meter ? 0.5f : 0.02f) * (t - meter);

            // scope-buffer: gedecimeerd mono-signaal voor de UI
            if (++fDecim >= 3)
            {
                fDecim = 0;
                if (fScopeN < kScopeLen)
                    fScope[fScopeN++] = (yL + yR) * 0.5f;
            }
        }

        fLpPunchL = ppL; fLpPunchR = ppR;
        fLpBodyL = pbL;  fLpBodyR = pbR;
        fLpAirL = phL;   fLpAirR = phR;
        fPunchMeter = meter;

        // stuur scope-data ±10x/seconde naar de UI
        if (fScopeN >= 40)
        {
            char buf[256];
            int pos = 0;
            for (uint32_t s = 0; s < fScopeN && pos < 230; ++s)
                pos += std::snprintf(buf + pos, sizeof(buf) - pos, "%.3f;", fScope[s]);
            buf[pos] = '\0';
            setState("scope", buf);
            char mb[24];
            std::snprintf(mb, sizeof(mb), "%.3f", meter);
            setState("punch", mb);
            fScopeN = 0;
        }
    }

private:
    void rollDice(){
        fRnd=fRnd*1664525u+1013904223u; setParameterValue(kPoets,10.f+((fRnd>>8)%100)/100.f*80.f);
    }
    float fDice = 0.f;
    unsigned fRnd = 998877;

    static const int kScopeLen = 96;
    float fPoets, fOutDb, fFokus;
    float fLpPunchL, fLpPunchR, fLpBodyL, fLpBodyR, fLpAirL, fLpAirR;
    float fAPunch, fABody, fAAir, fOutGain;
    float fPunchMeter;
    uint32_t fDecim, fScopeN;
    float fScope[kScopeLen];

    float fPreset = 0.0f;

    void applyPreset(int i)
    {
        if (i < 0 || i >= kNumPresets) return;
        const float* p = kPresetParams[i];
        setParameterValue(kPoets, p[0]);
        setParameterValue(kOut, p[1]);
        setParameterValue(kFokus, p[2]);
        recalc();
    }

    void recalc()
    {
        const float sr = (float)getSampleRate();
        const float pi = 3.14159265358979f;
        fAPunch  = 1.0f - std::exp(-1.0f / (sr * 0.008f));   // 8 ms gemiddelde
        fABody   = std::exp(-2.0f * pi * 180.0f / sr);       // LP 180 Hz
        fAAir    = std::exp(-2.0f * pi * fFokus / sr);         // LP FOKUS Hz
        fOutGain = std::pow(10.0f, fOutDb / 20.0f);
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PoetsPlugin)
    float fOn=1.f,fKr=100.f,fGalaxy=9.f;
    Galaxy galaxy;
};

Plugin* createPlugin() { return new PoetsPlugin(); }

END_NAMESPACE_DISTRHO
