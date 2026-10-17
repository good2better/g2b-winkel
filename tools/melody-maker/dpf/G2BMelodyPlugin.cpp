#include "DistrhoPlugin.hpp"
START_NAMESPACE_DISTRHO
// G2B Melody-Maker — melodie/baslijn-generator: pluck-melodie bovenop progressie
static const int SCALES[8][7] = {
 {0,2,4,5,7,9,11},{0,2,3,5,7,8,10},{0,2,3,5,7,9,10},{0,1,3,5,7,8,10},
 {0,2,4,6,7,9,11},{0,2,4,5,7,9,10},{0,2,3,5,7,8,11},{0,2,3,5,7,9,11}};
static const int PROGS[6][4] = {
 {0,5,3,4},{0,3,4,0},{5,3,0,4},{0,3,5,4},{0,4,5,3},{1,4,0,5}};
// 8 melodie-patronen: offsets in ladder-stappen per 8ste (0=chordtone loop)
static const int PAT[8][8] = {
 {0,1,2,1,0,1,2,4},   // stappen
 {0,2,4,7,4,2,0,-2},  // sprongen
 {0,2,4,7,9,7,4,2},   // arp-up
 {7,4,2,0,-3,0,2,4},  // arp-down
 {0,2,0,4,0,2,4,2},   // hook
 {0,0,4,0,0,4,0,2},   // echo
 {0,0,0,4,0,0,7,0},   // baslijn (root/fifth)
 {0,3,2,5,4,1,2,4}};  // random-walk-seeded
struct Voice{int note;float ph,env;bool on;};
class G2BMelodyPlugin : public Plugin {
public:
    enum Params{ kOn=0,kKracht,kKey,kScale,kProg,kPattern,kDensity,kOctave,kSwing,kPreset,kRate,kDimensie,kSkin,kNum };
    G2BMelodyPlugin():Plugin(kNum,0,0){for(auto&x:v)x.on=false;ph=0;step=0;chord=0;lastdeg=-9;}
protected:
    const char* getLabel()const override{return "G2B Melody-Maker";}
    const char* getDescription()const override{return "Melodie/baslijn-generator: patronen bovenop akkoord-progressie";}
    const char* getMaker()const override{return "G2B";}
    const char* getLicense()const override{return "MIT";}
    uint32_t getVersion()const override{return d_version(1,0,0);}
    int64_t getUniqueId()const override{return d_cconst('G','2','M','l');}
    void initParameter(uint32_t i,Parameter&p)override{
        switch(i){
        case kOn:     p.name="Aan";p.symbol="on";p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;p.hints=kParameterIsBoolean;break;
        case kKracht: p.name="Kracht";p.symbol="kracht";p.ranges.min=0;p.ranges.max=100;p.ranges.def=85;p.unit="%";break;
        case kKey:    p.name="Toonsoort";p.symbol="key";p.ranges.min=0;p.ranges.max=11;p.ranges.def=0;p.hints=kParameterIsInteger;break;
        case kScale:  p.name="Ladder";p.symbol="scale";p.ranges.min=0;p.ranges.max=7;p.ranges.def=0;p.hints=kParameterIsInteger;break;
        case kProg:   p.name="Progressie";p.symbol="prog";p.ranges.min=0;p.ranges.max=5;p.ranges.def=0;p.hints=kParameterIsInteger;break;
        case kPattern:p.name="Patroon";p.symbol="pat";p.ranges.min=0;p.ranges.max=7;p.ranges.def=0;p.hints=kParameterIsInteger;break;
        case kDensity:p.name="Dichtheid";p.symbol="dens";p.ranges.min=0;p.ranges.max=100;p.ranges.def=60;p.unit="%";break;
        case kOctave: p.name="Octaaf";p.symbol="oct";p.ranges.min=-2;p.ranges.max=2;p.ranges.def=0;p.hints=kParameterIsInteger;break;
        case kSwing:  p.name="Swing";p.symbol="swing";p.ranges.min=0;p.ranges.max=100;p.ranges.def=0;p.unit="%";break;
        case kPreset: p.name="Preset";p.symbol="preset";p.ranges.min=0;p.ranges.max=15;p.ranges.def=0;p.hints=kParameterIsInteger;break;
        case kRate:   p.name="Tempo";p.symbol="rate";p.ranges.min=60;p.ranges.max=180;p.ranges.def=110;p.unit="bpm";break;
        case kDimensie:p.name="DIMENTIES";p.symbol="dims";p.ranges.min=0;p.ranges.max=26;p.ranges.def=0;p.hints=kParameterIsInteger;break;
        case kSkin:   p.name="Skin";p.symbol="skin";p.ranges.min=0;p.ranges.max=7;p.ranges.def=1;p.hints=kParameterIsInteger;break;
        }
        prm[i]=p.ranges.def;
    }
    float getParameterValue(uint32_t i)const override{return prm[i];}
    void setParameterValue(uint32_t i,float nv)override{
        // preset schrijft andere params mee
        if(i==kPreset&&(int)nv!=(int)prm[kPreset]){applyPreset((int)nv);}
        prm[i]=nv;
    }
    void run(const float**,float**outs,uint32_t frames,
             const MidiEvent*,uint32_t)override{
        float*L=outs[0];float*R=outs[1];
        memset(L,0,frames*sizeof(float));memset(R,0,frames*sizeof(float));
        int on=(int)prm[kOn];float mix=prm[kKracht]/100.f;
        int key=(int)prm[kKey],sc=(int)prm[kScale],pg=(int)prm[kProg],pat=(int)prm[kPattern];
        float dens=prm[kDensity]/100.f,bpm=prm[kRate];int oct=(int)prm[kOctave];
        float swing=prm[kSwing]/100.f;int dims=(int)prm[kDimensie];
        double sr=getSampleRate();double step8=bpm/60.0/2.0;
        int patx=pat+(dims>=13?1:0);if(patx>7)patx-=8;
        for(uint32_t i=0;i<frames;i++){
            double sw=(step%2==1)?swing*0.4:0;
            ph+=step8/sr;
            if(ph>=1.0+sw*step8/8.0){
                ph=0;step=(step+1)%8;if(step==0)chord=(chord+1)%4;
                int deg=PROGS[pg][chord];
                float rnd=frand();
                if(rnd<dens){
                    int st=PAT[patx][step]+deg;
                    int note=48+key+SCALES[sc][st%7]+12*(st/7)+12*oct;
                    if(patx==6)note-=12;
                    trig(note);
                }
            }
            float l=0,r=0;
            for(auto&V:v){if(!V.on)continue;
                V.env*=expf(-1.0/(sr*0.35));
                if(V.env<0.0005){V.on=false;continue;}
                double fq=440.0*pow(2.0,(V.note-69)/12.0);
                V.ph+=fq/sr;if(V.ph>=1)V.ph-=1;
                float s=sinf(2*M_PI*V.ph)*0.7f+sinf(4*M_PI*V.ph)*0.25f*V.env*2;
                s*=V.env*0.3f;l+=s;r+=s;}
            L[i]+=l*mix*on;R[i]+=r*mix*on;
        }
    }
private:
    void trig(int note){for(auto&x:v)if(!x.on){x.on=true;x.note=note;x.ph=0;x.env=1;return;}v[0].note=note;v[0].env=1;v[0].ph=0;}
    float frand(){rs=rs*1664525u+1013904223u;return (rs>>8)/16777216.0f;}
    void applyPreset(int p){
        // 16 presets: {key,scale,prog,pat,dens,oct,swing,bpm,dims}
        static const int PR[16][9]={
          {0,0,0,2,70,0,10,120,0},{9,1,2,0,60,0,15,90,4},{0,1,4,6,80,-1,0,140,8},{7,0,4,3,65,0,20,125,3},
          {2,3,2,4,55,0,25,85,12},{5,0,5,1,75,1,5,128,7},{9,1,0,7,50,-1,30,75,9},{0,5,1,2,70,0,8,118,5},
          {4,6,2,0,60,0,12,95,14},{10,1,4,6,85,-2,0,150,18},{0,0,0,5,65,1,18,110,6},{7,3,3,2,60,0,22,100,11},
          {2,1,0,4,55,0,28,88,15},{0,0,4,3,70,1,6,126,2},{9,7,2,1,50,0,35,80,17},{5,0,5,6,90,-1,0,145,20}};
        const int*q=PR[p&15];
        prm[kKey]=q[0];prm[kScale]=q[1];prm[kProg]=q[2];prm[kPattern]=q[3];
        prm[kDensity]=q[4];prm[kOctave]=q[5];prm[kSwing]=q[6];prm[kRate]=q[7];prm[kDimensie]=q[8];
    }
    float prm[kNum]={};Voice v[8];double ph;int step,chord,lastdeg;uint32_t rs=12345;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BMelodyPlugin)
};
Plugin* createPlugin(){return new G2BMelodyPlugin();}
END_NAMESPACE_DISTRHO
