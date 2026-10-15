#include "DistrhoPlugin.hpp"
START_NAMESPACE_DISTRHO
// G2B LoFi-Maker — bitcrush + vinyl-kraak + tape-wobble + warm filter
class G2BLofiPlugin : public Plugin {
public:
    enum Params { kOn=0,kKracht,kCrush,kRate,kWobble,kCrackle,kTone,kDimensie,kSkin,kNum };
    G2BLofiPlugin() : Plugin(kNum,0,0) { sr=getSampleRate(); dl=djr=0; wph=0; hph=0; yl=yr=0; }
protected:
    const char* getLabel() const override { return "G2B LoFi-Maker"; }
    const char* getDescription() const override { return "LoFi texture: bitcrush + vinyl-kraak + tape-wobble + warm filter"; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "MIT"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','L','f'); }
    void initParameter(uint32_t i, Parameter& p) override {
        switch(i){
        case kOn:     p.name="Aan";p.symbol="on";p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;p.hints=kParameterIsBoolean;break;
        case kKracht: p.name="Kracht";p.symbol="kracht";p.ranges.min=0;p.ranges.max=100;p.ranges.def=70;p.unit="%";break;
        case kCrush:  p.name="Crush";p.symbol="crush";p.ranges.min=0;p.ranges.max=100;p.ranges.def=30;p.unit="%";break;
        case kRate:   p.name="Grof";p.symbol="rate";p.ranges.min=0;p.ranges.max=100;p.ranges.def=20;p.unit="%";break;
        case kWobble: p.name="Wobble";p.symbol="wobble";p.ranges.min=0;p.ranges.max=100;p.ranges.def=15;p.unit="%";break;
        case kCrackle:p.name="Kraak";p.symbol="crackle";p.ranges.min=0;p.ranges.max=100;p.ranges.def=12;p.unit="%";break;
        case kTone:   p.name="Tone";p.symbol="tone";p.ranges.min=0;p.ranges.max=100;p.ranges.def=65;p.unit="%";break;
        case kDimensie:p.name="Dimensies";p.symbol="dims";p.ranges.min=0;p.ranges.max=26;p.ranges.def=0;p.hints=kParameterIsInteger;break;
        case kSkin:   p.name="Skin";p.symbol="skin";p.ranges.min=0;p.ranges.max=7;p.ranges.def=5;p.hints=kParameterIsInteger;break;
        }
    }
    float getParameterValue(uint32_t i) const override { return prm[i]; }
    void setParameterValue(uint32_t i, float v) override { prm[i]=v; }
    void run(const float** ins, float** outs, uint32_t frames) override {
        const float* L=ins[0]; const float* R=ins[1];
        float* oL=outs[0]; float* oR=outs[1];
        int on=(int)prm[kOn]; float mix=prm[kKracht]/100.f;
        float crush=prm[kCrush]/100.f, rate=prm[kRate]/100.f, wob=prm[kWobble]/100.f;
        float crk=prm[kCrackle]/100.f, tone=prm[kTone]/100.f; int dims=(int)prm[kDimensie];
        // dims: 0-8 lichter, 9-17 medium, 18-26 heavy (boost alles licht)
        float boost=1.0f+(dims>=18?0.35f:(dims>=9?0.18f:0.0f));
        crush*=boost; rate*=boost; crk*=boost; wob*=boost;
        int bits=(int)(16-crush*11); if(bits<4)bits=4;
        float step=2.0f/(1<<bits);
        int hold=1+(int)(rate*24); // sample-hold lengte
        float wobAmt=wob*3.0f;     // ms pitch-drift
        double wspd=0.5+wob*2.0;   // wobble-snelheid Hz
        float lpA=tone*0.9f;       // one-pole LP
        float hl=0,hr=0; uint32_t cnt=0;
        for (uint32_t i=0;i<frames;i++){
            wph+=wspd/sr; if(wph>=1)wph-=1;
            float dly=(float)(sr*(wobAmt*sinf(6.283185f*2*M_PI*wph)*0.001f));
            if(dly<0)dly=0;
            int di=(int)dly; float fr=dly-di;
            int rd=(dl-di+DLY)%DLY, rd2=(dl-di-1+DLY)%DLY;
            float l=bufL[rd]*(1-fr)+bufL[rd2]*fr;
            int rr=(djr-di+DLY)%DLY, rr2=(djr-di-1+DLY)%DLY;
            float r=bufR[rr]*(1-fr)+bufR[rr2]*fr;
            bufL[dl]=L[i]; bufR[djr]=R[i]; dl=(dl+1)%DLY; djr=(djr+1)%DLY;
            if(++cnt>=(uint32_t)hold){cnt=0; hl=l; hr=r;}
            l=hl; r=hr; // sample&hold downsample
            l=floorf(l/step+0.5f)*step; r=floorf(r/step+0.5f)*step; // bitcrush
            // vinyl kraak: sparse clicks + hiss
            hph+=1.0/sr;
            float nz=0;
            if((rng=rng*1664525u+1013904223u) > 4294963200u){ nz=((rng>>9)&0x7fff)/16384.0f-1.0f; }
            float hiss=((rng>>8)&0x7fff)/32768.0f*0.012f;
            l+=(nz*0.5f+hiss)*crk*4; r+=(((int)rng&1)?nz:hiss*0.5f)*crk*4;
            yl+=lpA*(l-yl); yr+=lpA*(r-yr);
            oL[i]=L[i]*(1-mix)+yl*mix*on+L[i]*(1-on)*mix;
            oR[i]=R[i]*(1-mix)+yr*mix*on+R[i]*(1-on)*mix;
        }
    }
private:
    static const int DLY=4096;
    float bufL[DLY]={},bufR[DLY]={}; int dl,djr;
    double wph,hph; float yl,yr,sr; uint32_t rng=22222;
    float prm[kNum]={};
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BLofiPlugin)
};
Plugin* createPlugin(){ return new G2BLofiPlugin(); }
END_NAMESPACE_DISTRHO
