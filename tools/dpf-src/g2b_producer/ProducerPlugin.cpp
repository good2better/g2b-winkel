#include "DistrhoPlugin.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#ifdef DISTRHO_OS_WINDOWS
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "brein_samples.h"
START_NAMESPACE_DISTRHO

// G2B ChordLab — akkoorden-generator: strummed akkoord-loops
// met 6 klanken (KLANK), 16 progressies (PROG), MIDI-bespeelbaar.
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
// akkoord-progressies (laddergraden, 0-basis) — 16 deuntjes
static const int PROGS[16][4] = {
 {0,5,3,4},  // I-vi-IV-V (pop)
 {0,3,4,0},  // I-IV-V-I (basis)
 {5,3,0,4},  // vi-IV-I-V (emotie)
 {0,3,5,4},  // I-IV-vi-V
 {0,4,5,3},  // I-V-vi-IV (hits)
 {1,4,0,5},  // ii-V-I-vi (jazzy)
 {5,4,0,3},  // vi-V-I-IV
 {0,4,3,4},  // I-V-IV-V (energie)
 {2,5,0,4},  // iii-vi-I-V
 {3,4,0,5},  // IV-V-I-vi
 {0,2,3,4},  // I-iii-IV-V
 {5,0,3,4},  // vi-I-IV-V
 {0,3,0,4},  // I-IV-I-V (loop)
 {1,5,3,4},  // ii-vi-IV-V
 {0,4,5,5},  // I-V-vi-vi
 {4,3,0,5},  // V-IV-I-vi
};
struct Voice { int note; int keyRoot; float phase,env,age,dec; int typ; bool on;
               float hm,dm,am;   // klank-variant: harmonieken-, decay-, attack-factor
               // Karplus-Strong snaar (typ 7): eigen DSP, delay-lijn met feedback
               float ks[2048]; int kp,kper; int smidx; float spos,sstep; };

// normale VST-klankkiezer: SOORT (groep) → GELUID (variant)
// {naam, basetype, harmonieken-mult, decay-mult, attack-mult}
struct Snd { const char* name; int typ; float hm,dm,am; int sm; };
static const Snd kSounds[16] = {
    {"E-Piano",0,1.00f,1.00f,1.0f,-1}, {"Rhodes",0,1.30f,0.75f,1.0f,-1}, {"Orgel",3,1.00f,1.00f,1.0f,-1},
    {"Strings",6,1.00f,1.00f,1.0f,-1}, {"Pad",1,1.00f,1.00f,1.0f,-1},      {"Harp",2,0.85f,0.50f,1.0f,-1},
    {"Pluck",2,1.00f,1.00f,1.0f,-1},   {"Marimba",2,1.35f,0.60f,1.0f,-1}, {"Echt Marimba",8,1.00f,1.00f,1.0f,2},
    {"Nylon",7,0.90f,1.00f,1.0f,-1},   {"Echt Slag",8,1.00f,1.00f,1.0f,1},
    {"Sub Bas",4,1.00f,1.00f,1.0f,-1}, {"Echt Bas",8,1.00f,1.00f,1.0f,3},
    {"Bell",5,1.00f,1.00f,1.0f,-1},    {"Vibe",5,0.80f,0.55f,1.0f,-1},    {"Echt Piano",8,1.00f,1.00f,1.0f,0}
};
static const int kNGroups=6;
static const char* kGroupNames[6]={"KEYS","STRINGS","PLUCK","GITAAR","BAS","BELLS"};
static const int kGroupFirst[6]={0,3,6,9,11,13};
static const int kGroupCount[6]={3,3,3,2,2,3};
static int groupOf(int snd){ for(int g=0;g<kNGroups;g++){ if(snd>=kGroupFirst[g]&&snd<kGroupFirst[g]+kGroupCount[g])return g; } return 0; }

// per-klank synthese; typ 0..7, env 0..1, age in sec, phase 0..1, hm = helderheid-variant
static float synth(int typ, float phase, float env, float age, float hm){
    float p=2*M_PI*phase;
    switch(typ){
    case 1: { // warme pad — lage harmonieken, langzame gloed
        float s=sinf(p)+0.45f*hm*sinf(2*p)*env+0.2f*hm*sinf(3*p)*env*env;
        return s*0.42f; }
    case 2: { // pluck/marimba — helder, snelle harmonieken
        float s=sinf(p)+0.6f*hm*sinf(2*p)*env+0.4f*hm*sinf(4*p)*env*env;
        return s*0.45f; }
    case 3: { // orgel — 3 harmonieken, constant
        return (sinf(p)+0.5f*hm*sinf(2*p)+0.3f*hm*sinf(3*p))*0.35f; }
    case 4: { // sub-bas — grondtoon + octaaf eronder gevoel; hm>1 = rondere bas
        return (sinf(p)+0.5f*sinf(0.5f*p)+0.25f*(hm-1.f)*sinf(2*p))*0.55f; }
    case 5: { // bel — niet-harmonische partialen die uitdoven
        float s=sinf(p)+0.5f*hm*sinf(2.76f*p)*powf(env,2.f)+0.3f*hm*sinf(5.4f*p)*powf(env,4.f);
        return s*0.45f; }
    case 6: { // strings — detuned ensemble, langzame attack
        float s=sinf(p)+sinf(p*1.007f)+sinf(p*0.994f)+0.35f*hm*sinf(2*p);
        return s*0.22f; }
    default:{ // e-piano (origineel)
        float s=sinf(p)+0.35f*hm*sinf(2*p)*powf(env,3.f)+0.15f*hm*sinf(3*p)*powf(env,5.f);
        return s*0.40f; }
    }
}
// per-klank envelope-tijd (sec) + attack
static float decTime(int typ){ switch(typ){case 1:return 2.2f;case 2:return 0.45f;case 3:return 3.5f;case 4:return 1.1f;case 5:return 1.6f;case 6:return 3.0f;case 7:return 1.4f;default:return 0.9f;} }
static float atkTime(int typ){ switch(typ){case 1:return 0.25f;case 3:return 0.02f;case 6:return 0.35f;default:return 0.006f;} }

class G2BChordPlugin : public Plugin {
public:
    enum Params {
        kOn=0, kKracht, kKey, kScale, kProg, kVoicing, kRate, kPreset, kDimensie, kSkin, kVoice, kDice, kMode, kGroup, kExpMid, kExpWav, kNum
    };
    G2BChordPlugin() : Plugin(kNum, 1, 0) {
        for (int i=0;i<16;i++){ v[i].on=false; v[i].phase=0; v[i].env=0; v[i].age=0; v[i].typ=0; }
        ph=0.99; chordIdx=0; strumN=0; retrig=false;
    }
protected:
    const char* getLabel() const override { return "G2B Producer"; }
    const char* getDescription() const override { return "Deuntjes-machine: 16 deuntjes x 16 klanken (SOORT → GELUID) + dobbelsteen, MIDI-bespeelbaar"; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "MIT"; }
    uint32_t getVersion() const override { return d_version(1,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','P','r'); }

    void initParameter(uint32_t i, Parameter& p) override {
        switch(i){
        case kOn:      p.name="Aan"; p.symbol="on"; p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;p.hints=kParameterIsBoolean; break;
        case kKracht:  p.name="Kracht"; p.symbol="kracht"; p.ranges.min=0;p.ranges.max=100;p.ranges.def=80; p.unit="%"; break;
        case kKey:     p.name="Toonsoort"; p.symbol="key"; p.ranges.min=0;p.ranges.max=11;p.ranges.def=0; p.hints=kParameterIsInteger; break;
        case kScale:   p.name="Ladder"; p.symbol="scale"; p.ranges.min=0;p.ranges.max=7;p.ranges.def=0; p.hints=kParameterIsInteger; break;
        case kProg:    p.name="Progressie"; p.symbol="prog"; p.ranges.min=0;p.ranges.max=15;p.ranges.def=0; p.hints=kParameterIsInteger; break;
        case kVoicing: p.name="Voicing"; p.symbol="voicing"; p.ranges.min=0;p.ranges.max=3;p.ranges.def=0; p.hints=kParameterIsInteger; break;
        case kRate:    p.name="Tempo"; p.symbol="rate"; p.ranges.min=40;p.ranges.max=160;p.ranges.def=90; p.unit="bpm"; break;
        case kPreset:  p.name="Preset"; p.symbol="preset"; p.ranges.min=0;p.ranges.max=31;p.ranges.def=0; p.hints=kParameterIsInteger; break;
        case kDimensie:p.name="DIMENTIES"; p.symbol="dims"; p.ranges.min=0;p.ranges.max=26;p.ranges.def=0; p.hints=kParameterIsInteger; break;
        case kSkin:    p.name="Skin"; p.symbol="skin"; p.ranges.min=0;p.ranges.max=7;p.ranges.def=0; p.hints=kParameterIsInteger; break;
        case kVoice:   p.name="GELUID"; p.symbol="geluid"; p.ranges.min=0;p.ranges.max=15;p.ranges.def=0; p.hints=kParameterIsInteger; break;
        case kDice:    p.name="DICE"; p.symbol="dice"; p.ranges.min=0;p.ranges.max=1;p.ranges.def=0; p.hints=kParameterIsBoolean; break;
        case kMode:    p.name="MODE"; p.symbol="mode"; p.ranges.min=0;p.ranges.max=1;p.ranges.def=1; p.hints=kParameterIsBoolean; break;
        case kGroup:   p.name="SOORT"; p.symbol="soort"; p.ranges.min=0;p.ranges.max=5;p.ranges.def=0; p.hints=kParameterIsInteger; break;
        }
        prm[i]=p.ranges.def;
    }
    float getParameterValue(uint32_t i) const override { return prm[i]; }
    void setParameterValue(uint32_t i, float v) override {
        if(i==kPreset && (int)v!=(int)prm[kPreset]){applyPreset((int)v);}
        if(i==kDice){ if(v>0.5f) rollDice(); return; }
        if(i==kExpMid){ if(v>0.5f){ writeMid(); } prm[i]=0.f; return; }
        if(i==kExpWav){ if(v>0.5f) writeWav(getSampleRate()); prm[i]=0.f; return; }
        if(i==kGroup){
            int g=(int)v; if(g<0)g=0; if(g>5)g=5;
            prm[kGroup]=(float)g;
            // groep gekozen → eerste geluid van die groep als huidige klank buiten groep valt
            if(groupOf((int)prm[kVoice])!=g) prm[kVoice]=(float)kGroupFirst[g];
            retrig=true; return;
        }
        if(i==kVoice){ int s=(int)v; if(s<0)s=0;if(s>15)s=15; prm[kVoice]=(float)s; prm[kGroup]=(float)groupOf(s); retrig=true; return; }
        if(i==kKey||i==kScale||i==kProg||i==kVoicing||i==kDimensie) retrig=true;
        prm[i]=v;
    }
    void rollDice(){
        // dobbelsteen: nieuwe random deuntje+klank+voicing (toonsoort behouden)
        fRnd=fRnd*1664525u+1013904223u; prm[kProg]=(float)((fRnd>>8)%16);
        fRnd=fRnd*1664525u+1013904223u; int s=(fRnd>>8)%16; prm[kVoice]=(float)s; prm[kGroup]=(float)groupOf(s);
        fRnd=fRnd*1664525u+1013904223u; prm[kVoicing]=(float)((fRnd>>8)%4);
        fRnd=fRnd*1664525u+1013904223u; prm[kDimensie]=(float)((fRnd>>8)%27);
        fRnd=fRnd*1664525u+1013904223u; prm[kScale]=(float)((fRnd>>8)%8);
        retrig=true;
        pushVals();
    }
    void initState(uint32_t i, String& name, String& def) override { if(i==0){name="vals";def="";} }
    void setState(const char*, const char*) override {}
    void pushVals(){ // UI ziet de dobbelsteen/preset ook werken (standalone heeft geen param-echo)
        char b[256]; int n=0;
        for(int i=0;i<16;i++) n+=snprintf(b+n,sizeof(b)-n,"%.3g;",prm[i]);
        setState("vals",b);
    }
    void applyPreset(int p){
        // 16 presets: {key,scale,prog,voicing,rate,dims,klank}
        static const int PR[32][7]={
          {0,0,0,0,90,0,0},{9,1,2,0,85,3,1},{7,0,4,1,110,6,2},{0,5,11,1,95,9,0},
          {2,3,12,0,80,12,6},{5,4,5,2,105,15,7},{9,1,14,2,75,18,1},{0,0,7,0,120,21,2},
          {4,6,8,1,70,24,4},{10,1,13,3,140,26,0},{0,0,9,2,100,1,5},{7,3,6,0,88,7,1},
          {2,1,10,3,78,10,7},{0,0,15,0,130,4,6},{9,7,1,1,72,13,3},{5,0,4,0,115,19,5},{3,2,1,0,96,2,8},{11,6,3,2,84,8,9},{6,4,10,1,104,14,10},{1,0,12,3,76,16,11},
          {8,1,5,0,118,20,12},{4,2,8,2,68,22,13},{0,3,6,1,92,5,14},{10,5,13,0,126,11,15},
          {5,0,2,1,86,17,0},{2,1,11,2,98,23,3},{9,4,14,0,108,25,4},{7,6,4,1,66,0,5},
          {0,1,0,3,132,3,8},{3,0,9,0,80,10,9},{11,3,7,2,94,13,12},{6,7,15,1,112,26,15}};
        const int*q=PR[p&31];
        prm[kKey]=q[0];prm[kScale]=q[1];prm[kProg]=q[2];prm[kVoicing]=q[3];
        prm[kRate]=q[4];prm[kDimensie]=q[5];prm[kVoice]=(float)(q[6]%16);
        prm[kGroup]=(float)groupOf((int)prm[kVoice]);
        retrig=true;
        pushVals();
    }

    void run(const float**, float** outs, uint32_t frames,
             const MidiEvent* mev, uint32_t nmev) override {
        float* L=outs[0]; float* R=outs[1];
        memset(L,0,frames*sizeof(float)); memset(R,0,frames*sizeof(float));
        double sr=getSampleRate();
        int on=(int)prm[kOn]; float mix=prm[kKracht]/100.f;
        int key=(int)prm[kKey], sc=(int)prm[kScale], pg=(int)prm[kProg], vo=(int)prm[kVoicing];
        float bpm=prm[kRate]; int dims=(int)prm[kDimensie]; int typ=(int)prm[kVoice];
        // MIDI-keyboard: elke noot die je speelt triggert een akkoord op die grondtoon
        for (uint32_t m=0;m<nmev;m++){
            const MidiEvent&e=mev[m];
            if (e.size<3) continue;
            const uint8_t msg=e.data[0]&0xF0;
            // note-off (0x80 of 0x90 vel=0): akkoord laat snel los — niet doorklinken
            if (msg==0x80 || (msg==0x90 && e.data[2]==0)){
                int root=e.data[1];
                for(int vx=0;vx<16;vx++) if(v[vx].on && v[vx].keyRoot==root) v[vx].dm*=0.10f;
                continue;
            }
            if (msg!=0x90) continue;
            int root=e.data[1];
            int nn=(vo==0?3:vo==1?4:vo==2?5:4)+((dims%3)==0?0:((dims%3)==1?-1:1));
            if(nn<3)nn=3; if(nn>5)nn=5;
            int steps[5]={0,2,4,6,8};
            if(vo==3)steps[1]=4;
            for(int k=0;k<nn;k++){
                int degx=steps[k];
                int note=root+SCALES[sc][degx%7]+12*(degx/7);
                triggerKey(note,typ,root);
            }
        }
        double beatsPerSec=bpm/60.0;
        for (uint32_t i=0;i<frames;i++){
            tickFrame(sr,beatsPerSec,on,mix,key,sc,pg,vo,dims,typ,L[i],R[i]);
            if(fWavOn && fWavPos<fWavN-1){ fWavBuf[fWavPos*2]=L[i]; fWavBuf[fWavPos*2+1]=R[i]; fWavPos++; }
        }
        if(fWavOn && fWavPos>=fWavN-1){ fWavOn=false; writeWav(sr); }
    }
    void tickFrame(double sr,double beatsPerSec,int on,float mix,int key,int sc,int pg,int vo,int dims,int typ,float&Lo,float&Ro){
            fPos++;
            if (prm[kMode]>0.5f) ph += beatsPerSec/sr;
            if (ph>=1.0){ ph-=1.0; chordIdx=(chordIdx+1)%4; strumN=0; strumAt=fPos; }
            if (retrig){ retrig=false; strumN=0; strumAt=fPos; } // knop-draai = direct hoorbaar
            // strum: elke ~22ms volgende noot aan
            if (strumN>=0 && (int)(fPos-strumAt)>=strumN*(int)(sr*0.022)){
                int deg=PROGS[pg][chordIdx];
                static const int VN[4]={3,4,5,4};
                int nn=VN[vo]+((dims%3)==0?0:((dims%3)==1?-1:1));
                if (nn<3)nn=3; if (nn>5)nn=5;
                if (strumN<nn){
                    int steps[5]={0,2,4,6,8};
                    if (vo==3) steps[1]=4;
                    int st=steps[strumN];
                    int degx=deg+st;
                    int note=60+key+SCALES[sc][degx%7]+12*(degx/7);
                    if (strumN==0) note-=12;
                    trigger(note,typ);
                    strumN++;
                    if (strumN>=nn) strumN=-1;
                }
            }
            float l=0,r=0;
            for (int vix=0;vix<16;vix++){
                Voice&V=v[vix]; if(!V.on)continue;
                V.env*=expf(-1.0/(sr*decTime(V.typ)*V.dm));
                if (V.env<0.0005){V.on=false;continue;}
                V.age+=1.0f/sr;
                double fq=440.0*pow(2.0,(V.note-69)/12.0);
                V.phase+=fq/sr; if(V.phase>=1)V.phase-=1;
                float atk=fminf(1.0f,V.age/(atkTime(V.typ)*V.am));
                float s;
                if(V.typ==8){ // echte sample uit de library — met eigen pitch-shift
                    int pi=(int)V.spos; const BrSmp&sm=kBrSmp[V.smidx];
                    if(pi>=(int)sm.n-2){V.on=false;continue;}
                    float fr=V.spos-pi;
                    s=(sm.d[pi]+fr*(sm.d[pi+1]-sm.d[pi]))/32768.f*0.9f;
                    V.spos+=V.sstep;
                } else if(V.typ==7){ // snaar: delay-lijn met gedempte feedback; hm stuurt de dempt
                    float y=V.ks[V.kp];
                    float nx=V.ks[(V.kp+1)%V.kper];
                    float b=0.4993f+(V.hm-1.f)*0.06f; if(b<0.44f)b=0.44f; if(b>0.55f)b=0.55f;
                    V.ks[V.kp]=y*b+nx*(0.9986f-b);
                    V.kp=(V.kp+1)%V.kper;
                    s=y*0.9f*V.env;
                } else {
                    s=synth(V.typ,V.phase,V.env,V.age,V.hm)*atk*V.env;
                }
                float pan=(vix%3-1)*0.25f+0.5f;
                l+=s*(1.5f-pan); r+=s*(0.5f+pan);
            }
            Lo+=tanhf(l*mix*on*1.4f); Ro+=tanhf(r*mix*on*1.4f);
    }
private:
    void trigger(int note,int snd){ triggerKey(note,snd,-1); }
    void triggerKey(int note,int snd,int keyRoot){
        for (int i=0;i<16;i++) if(!v[i].on){ initV(v[i],note,snd); v[i].keyRoot=keyRoot; return; }
        initV(v[0],note,snd); v[0].keyRoot=keyRoot;
    }
    void initV(Voice&V,int note,int snd){
        const Snd&sn=kSounds[snd&15];
        int typ=sn.typ;
        V.on=true;V.note=note;V.phase=0;V.env=1;V.age=0;V.typ=typ;
        V.hm=sn.hm;V.dm=sn.dm;V.am=sn.am;V.smidx=sn.sm;V.spos=0;
        if(typ==8){ const BrSmp&sm=kBrSmp[sn.sm];
            V.sstep=powf(2.f,(note-sm.root)/12.f)*(22050.0f/(float)getSampleRate()); V.dm=3.0f; }
        if(typ==7){ // gitaar: ruis-burst in de snaar-delay
            double fq=440.0*pow(2.0,(note-69)/12.0);
            int per=(int)(getSampleRate()/fq); if(per<4)per=4; if(per>2047)per=2047;
            V.kper=per; V.kp=0;
            unsigned s=12345u+note*777u;
            for(int k=0;k<per;k++){ s=s*1664525u+1013904223u; V.ks[k]=((s>>8)&0xFFFF)/32768.f-1.f; }
        }
    }
    static void g2bDir(char*out,int n){
#ifdef DISTRHO_OS_WINDOWS
        const char*u=getenv("USERPROFILE"); if(!u)u="C:\\Users\\Public";
        snprintf(out,n,"%s\\Music\\G2B",u); _mkdir(out);
        snprintf(out,n,"%s\\Music\\G2B\\",u);
#else
        const char*h=getenv("HOME"); if(!h)h="/tmp";
        char d[512]; snprintf(d,512,"%s/Music",h); mkdir(d,0755);
        snprintf(d,512,"%s/Music/G2B",h); mkdir(d,0755);
        snprintf(out,n,"%s/Music/G2B/",h);
#endif
    }
    static void putU32(uint8_t*p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
    static void putU16(uint8_t*p,uint16_t v){p[0]=v>>8;p[1]=v;}
    void writeMid(){
        // SMF-0: 4 akkoorden x noten, 96 ticks/kwartnoot, strum 1/16
        int key=(int)prm[kKey], sc=(int)prm[kScale], pg=(int)prm[kProg], vo=(int)prm[kVoicing];
        int dims=(int)prm[kDimensie]; float bpm=prm[kRate];
        uint8_t trk[8192]; int tp=0;
        auto ev=[&](int delta,uint8_t st,uint8_t n,uint8_t v){
            int d=delta; uint8_t buf[4]; int bl=0; buf[0]=d&0x7F; while((d>>=7))buf[++bl]=0x80|(d&0x7F);
            for(int k=bl;k>=0;k--)trk[tp++]=buf[k];
            trk[tp++]=st;trk[tp++]=n;trk[tp++]=v; };
        int usec=(int)(60000000.0/bpm);
        trk[tp++]=0;trk[tp++]=0xFF;trk[tp++]=0x51;trk[tp++]=3;
        trk[tp++]=(usec>>16)&255; trk[tp++]=(usec>>8)&255; trk[tp++]=usec&255;
        int first=1;
        for(int c=0;c<4;c++){
            int deg=PROGS[pg][c];
            static const int VN[4]={3,4,5,4};
            int nn=VN[vo]+((dims%3)==0?0:((dims%3)==1?-1:1)); if(nn<3)nn=3; if(nn>5)nn=5;
            int steps[5]={0,2,4,6,8}; if(vo==3)steps[1]=4;
            for(int k=0;k<nn;k++){
                int st=steps[k]; int degx=deg+st;
                int note=60+key+SCALES[sc][degx%7]+12*(degx/7); if(k==0)note-=12;
                ev(first?0:(k?6:96),0x90,note,90); // strum ~1/16
                first=0;
            }
            for(int k=0;k<nn;k++){
                int st=steps[k]; int degx=deg+st;
                int note=60+key+SCALES[sc][degx%7]+12*(degx/7); if(k==0)note-=12;
                ev(k==0?84:0,0x80,note,0); // duur ~1 kwartnoot - strum
            }
        }
        trk[tp++]=0;trk[tp++]=0xFF;trk[tp++]=0x2F;trk[tp++]=0;
        char dir[512]; g2bDir(dir,512); char fp[600]; snprintf(fp,600,"%sbrein-deuntje.mid",dir);
        FILE*f=fopen(fp,"wb"); if(!f)return;
        uint8_t h[14]={'M','T','h','d',0,0,0,6,0,0,0,1,0,96};
        fwrite(h,1,14,f);
        uint8_t th[8]={'M','T','r','k',0,0,0,0}; putU32(th+4,tp);
        fwrite(th,1,8,f); fwrite(trk,1,tp,f); fclose(f);
    }
    void writeWav(double sr){
        // offline-render: 2 volle rondes (8 akkoorden) — werkt altijd, ook zonder audio-stream
        int on=1; float mix=prm[kKracht]/100.f;
        int key=(int)prm[kKey],sc=(int)prm[kScale],pg=(int)prm[kProg],vo=(int)prm[kVoicing],dims=(int)prm[kDimensie],typ=(int)prm[kVoice];
        for(int i=0;i<16;i++)v[i].on=false;
        ph=0.99;chordIdx=0;strumN=0;strumAt=0;retrig=false;
        double bps=prm[kRate]/60.0; int want=(int)(sr*8.0/bps)+48000; if(want>fWavN)want=fWavN;
        float lo,ro; int n=0;
        for(;n<want;n++){ lo=0;ro=0; tickFrame(sr,bps,on,mix,key,sc,pg,vo,dims,typ,lo,ro); fWavBuf[n*2]=lo; fWavBuf[n*2+1]=ro; }
        // fade-out laatste 50ms + lichte normalize
        float mx2=0.001f; for(int i=0;i<n*2;i++){float a=fWavBuf[i];if(a<0)a=-a;if(a>mx2)mx2=a;}
        if(mx2>0.85f){float g=0.85f/mx2;for(int i=0;i<n*2;i++)fWavBuf[i]*=g;}
        int fade=(int)(sr*0.05); for(int i=0;i<fade&&i<n;i++){float g=(float)(fade-i)/fade; fWavBuf[(n-fade+i)*2]*=g;fWavBuf[(n-fade+i)*2+1]*=g;}
        fWavPos=n;
        char dir[512]; g2bDir(dir,512); char fp[600]; snprintf(fp,600,"%sbrein-loop.wav",dir);
        FILE*f=fopen(fp,"wb"); if(!f)return;
        int nf=fWavPos, ch=2, sri=(int)sr; int dataN=nf*ch*2;
        uint8_t h[44]={ 'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,
                        (uint8_t)ch,0,0,0,0,0,0,0,0,0,(uint8_t)(ch*2),0,16,0,'d','a','t','a',0,0,0,0 };
        auto le32=[&](int off,uint32_t v){h[off]=v&255;h[off+1]=(v>>8)&255;h[off+2]=(v>>16)&255;h[off+3]=(v>>24)&255;};
        le32(4,36+dataN); le32(24,sri); le32(28,sri*ch*2); le32(40,dataN);
        fwrite(h,1,44,f);
        for(int i=0;i<nf*ch;i++){ int16_t q=(int16_t)(fWavBuf[i]*30000.f); uint8_t b[2]={(uint8_t)(q&255),(uint8_t)((q>>8)&255)}; fwrite(b,2,1,f); }
        fclose(f);
    }
    float prm[kNum]={}; unsigned fRnd=97531;
    bool fWavOn=false; int fWavPos=0; uint32_t fPos=0; static const int fWavN=44100*10;
    float fWavBuf[44100*10*2];
    Voice v[16];
    double ph; int chordIdx; int strumN; uint32_t strumAt; bool retrig;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BChordPlugin)
};

Plugin* createPlugin(){ return new G2BChordPlugin(); }
END_NAMESPACE_DISTRHO
