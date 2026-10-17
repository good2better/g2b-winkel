#include "DistrhoPlugin.hpp"
START_NAMESPACE_DISTRHO

// G2B ChordLab — akkoorden-generator: speelt strummed/stab akkoord-loops
// met e-piano timbre. Geen inputs: de plugin GENEERT audio.
// ladder-interval patronen (half-tonen per toon, 7 noten)
static const int SCALES[8][7] = {
 {0,2,4,5,7,9,11},   // majeur
 {0,2,3,5,7,8,10},   // mineur
 {0,2,3,5,7,9,10},   // dorisch
 {0,1,3,5,7,8,10},   // phrygisch
 {0,2,4,6,7,9,11},   // lydisch
 {0,2,4,5,7,9,10},   // mixolydisch
 {0,2,3,5,7,8,11},   // harmonisch mineur
 {0,2,3,5,7,9,11},   // melodisch mineur
};
// akkoord-progressies (laddergraden, 0-basis)
static const int PROGS[6][4] = {
 {0,5,3,4},  // I-vi-IV-V (pop)
 {0,3,4,0},  // I-IV-V-I (basis)
 {5,3,0,4},  // vi-IV-I-V (emotie)
 {0,3,5,4},  // I-IV-vi-V
 {0,4,5,3},  // I-V-vi-IV (hits)
 {1,4,0,5},  // ii-V-I-vi (jazzy)
};
// voicing uitbreidingen per laddergraad: toonafstanden binnen ladder
// triad={0,2,4}, 7={0,2,4,6}, 9={0,2,4,6,8}, sus={0,4 voeg}, add9={0,2,4,8}
struct Voice { int note; float phase,env,age; bool on; };

class G2BChordPlugin : public Plugin {
public:
    enum Params {
        kOn=0, kKracht, kKey, kScale, kProg, kVoicing, kRate, kPreset, kDimensie, kSkin, kNum
    };
    G2BChordPlugin() : Plugin(kNum, 0, 0) {
        for (int i=0;i<12;i++){ v[i].on=false; v[i].phase=0; v[i].env=0; v[i].age=0; }
        ph=0.99; chordIdx=0; strumN=0;
    }
protected:
    const char* getLabel() const override { return "G2B ChordLab"; }
    const char* getDescription() const override { return "Akkoord-generator: toonsoort+ladder -> strummed akkoorden met e-piano sound"; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "MIT"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','C','l'); }

    void initParameter(uint32_t i, Parameter& p) override {
        switch(i){
        case kOn:      p.name="Aan"; p.symbol="on"; p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;p.hints=kParameterIsBoolean; break;
        case kKracht:  p.name="Kracht"; p.symbol="kracht"; p.ranges.min=0;p.ranges.max=100;p.ranges.def=80; p.unit="%"; break;
        case kKey:     p.name="Toonsoort"; p.symbol="key"; p.ranges.min=0;p.ranges.max=11;p.ranges.def=0; p.hints=kParameterIsInteger; break;
        case kScale:   p.name="Ladder"; p.symbol="scale"; p.ranges.min=0;p.ranges.max=7;p.ranges.def=0; p.hints=kParameterIsInteger; break;
        case kProg:    p.name="Progressie"; p.symbol="prog"; p.ranges.min=0;p.ranges.max=5;p.ranges.def=0; p.hints=kParameterIsInteger; break;
        case kVoicing: p.name="Voicing"; p.symbol="voicing"; p.ranges.min=0;p.ranges.max=3;p.ranges.def=0; p.hints=kParameterIsInteger; break;
        case kRate:    p.name="Tempo"; p.symbol="rate"; p.ranges.min=40;p.ranges.max=160;p.ranges.def=90; p.unit="bpm"; break;
        case kPreset: p.name="Preset"; p.symbol="preset"; p.ranges.min=0;p.ranges.max=15;p.ranges.def=0; p.hints=kParameterIsInteger; break;
        case kDimensie:p.name="DIMENTIES"; p.symbol="dims"; p.ranges.min=0;p.ranges.max=26;p.ranges.def=0; p.hints=kParameterIsInteger; break;
        case kSkin:    p.name="Skin"; p.symbol="skin"; p.ranges.min=0;p.ranges.max=7;p.ranges.def=0; p.hints=kParameterIsInteger; break;
        }
        prm[i]=p.ranges.def; // standalone past defaults niet toe — zelf seeden
    }
    float getParameterValue(uint32_t i) const override { return prm[i]; }
    void setParameterValue(uint32_t i, float v) override {
        if(i==kPreset && (int)v!=(int)prm[kPreset]){applyPreset((int)v);}
        prm[i]=v;
    }
    void applyPreset(int p){
        // 16 presets: {key,scale,prog,voicing,rate,dims}
        static const int PR[16][6]={
          {0,0,0,0,90,0},{9,1,2,0,85,3},{7,0,4,1,110,6},{0,5,1,1,95,9},
          {2,3,2,0,80,12},{5,4,5,2,105,15},{9,1,0,2,75,18},{0,0,4,0,120,21},
          {4,6,2,1,70,24},{10,1,4,3,140,26},{0,0,0,2,100,1},{7,3,3,0,88,7},
          {2,1,0,3,78,10},{0,0,4,0,130,4},{9,7,2,1,72,13},{5,0,5,0,115,19}};
        const int*q=PR[p&15];
        prm[kKey]=q[0];prm[kScale]=q[1];prm[kProg]=q[2];prm[kVoicing]=q[3];
        prm[kRate]=q[4];prm[kDimensie]=q[5];
    }

    void run(const float**, float** outs, uint32_t frames,
             const MidiEvent*, uint32_t) override {
        float* L=outs[0]; float* R=outs[1];
        memset(L,0,frames*sizeof(float)); memset(R,0,frames*sizeof(float));
        double sr=getSampleRate();
        int on=(int)prm[kOn]; float mix=prm[kKracht]/100.f;
        int key=(int)prm[kKey], sc=(int)prm[kScale], pg=(int)prm[kProg], vo=(int)prm[kVoicing];
        float bpm=prm[kRate]; int dims=(int)prm[kDimensie];
        double beatsPerSec=bpm/60.0;
        for (uint32_t i=0;i<frames;i++){
            ph += beatsPerSec/sr;
            if (ph>=1.0){ ph-=1.0; chordIdx=(chordIdx+1)%4; strumN=0; strumAt=i; }
            // strum: elke ~22ms volgende noot aan
            if (strumN>=0 && (int)(i-strumAt)>=strumN*(int)(sr*0.022)){
                int deg=PROGS[pg][chordIdx];
                static const int VN[4]={3,4,5,4}; // triad/7/9/sus-ish
                int nn=VN[vo]+((dims%3)==0?0:((dims%3)==1?-1:1));
                if (nn<3)nn=3; if (nn>5)nn=5;
                if (strumN<nn){
                    int steps[5]={0,2,4,6,8};
                    if (vo==3) steps[1]=4; // sus4
                    int st=steps[strumN];
                    int degx=deg+st;
                    int note=36+key+SCALES[sc][degx%7]+12*(degx/7);
                    if (strumN==0) note-=12; // bas
                    trigger(note);
                    strumN++;
                    if (strumN>=nn) strumN=-1;
                }
            }
            float l=0,r=0;
            for (int vix=0;vix<12;vix++){
                Voice&V=v[vix]; if(!V.on)continue;
                V.env*=expf(-1.0/(sr*0.9)); // ~0.9s decay
                if (V.env<0.0005){V.on=false;continue;}
                V.age+=1.0f/sr;
                double fq=440.0*pow(2.0,(V.note-69)/12.0);
                V.phase+=fq/sr; if(V.phase>=1)V.phase-=1;
                // e-piano: grondtoon + harmonieken die sneller wegsterven (zachter, geen klik/clipt)
                float atk=fminf(1.0f,V.age/0.006f); // 6ms fade-in tegen note-on tik
                float s=sinf(2*M_PI*V.phase)+0.35f*sinf(4*M_PI*V.phase)*powf(V.env,3.f)+0.15f*sinf(6*M_PI*V.phase)*powf(V.env,5.f);
                s*=atk*V.env*0.16f;
                float pan=(vix%3-1)*0.25f+0.5f;
                l+=s*(1.5f-pan); r+=s*(0.5f+pan);
            }
            L[i]+=l*mix*on; R[i]+=r*mix*on;
        }
    }
private:
    void trigger(int note){
        for (int i=0;i<12;i++) if(!v[i].on){ v[i].on=true;v[i].note=note;v[i].phase=0;v[i].env=1;v[i].age=0;return; }
        v[0].note=note;v[0].env=1;v[0].phase=0;v[0].age=0;
    }
    float prm[kNum]={};
    Voice v[12];
    double ph; int chordIdx; int strumN; uint32_t strumAt;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BChordPlugin)
};

Plugin* createPlugin(){ return new G2BChordPlugin(); }
END_NAMESPACE_DISTRHO
