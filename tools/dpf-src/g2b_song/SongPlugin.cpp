// G2B SONG — de G2B all-in-one beat-machine:
// drums + GLIJDENDE 808 + akkoorden + melodie, één scherm.
// 808-engine met echte pitch-slide (portamento), pitch-drop en sub/drive-blend.
// MIDI-bespeelbaar: speel de 808 live met je keyboard (legato = glijdt).
// Bouwt op de Song-engine + OchoOcho-808. © G2B (good2better) — MIT.
#include "DistrhoPlugin.hpp"
#include "vendor/MidiFile.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "SongShared.h"
#include "piano_data.h"
#include "mar_data.h"
#include "harp_data.h"

SongRackView* gSongRackView=nullptr;
#include "cb_data.h"
#include "drum_data.h"
#include "psal_data.h"
#include "../g2b_verb.h"
#ifdef _WIN32
#endif

START_NAMESPACE_DISTRHO

static const char kScaleM[7]={0,2,3,5,7,8,10};
static const char kScaleJ[7]={0,2,4,5,7,9,11};
static const float kMelDec[27]={2.4f,2.0f,3.2f,1.8f,2.2f,2.0f,2.8f,2.4f,2.2f,2.0f,2.2f,1.6f,1.4f,2.0f,1.8f,2.4f,2.0f,3.0f,2.8f,2.0f,1.8f,1.8f,2.2f,2.2f,1.6f,2.0f,2.0f};
static const float kMelPart[27]={.3f,.3f,.45f,.35f,.3f,.35f,.4f,.3f,.35f,.35f,.3f,.4f,.2f,.35f,.3f,.3f,.35f,.4f,.45f,.35f,.3f,.3f,.3f,.35f,.2f,.35f,.3f};
static const char kModes[7][7]={ // ionisch,dorisch,frygisch,lydisch,mixolydisch,aeolisch,locrisch
    {0,2,4,5,7,9,11},{0,2,3,5,7,9,10},{0,1,3,5,7,8,10},{0,2,4,6,7,9,11},
    {0,2,4,5,7,9,10},{0,2,3,5,7,8,10},{0,1,3,5,6,8,10}};
static const char kStijlMode[27]={5,4,2,1,4,3,5,1,4,3,4,1,1,4,0,0,5,2,2,3,4,0,3,0,0,4,5};
static const char kPat[27][3][17]={
    {"x...x...x...x...","....x.......x...","x.x.x.x.x.x.x.x."}, // DEMBOW
    {"x.....x..x......","....x.......x...","x.xxx..x.xx.x.x."}, // DRILL
    {"x.....x...x.....","....x.......x...","x..x..x..x..x..x"}, // RNB
    {"x...x...x...x...","....x.......x...","..x...x...x...x."}, // HOUSE
    {"x..x....x..x....","....x.......x...","x.x..x.x.x.x..x."}, // AFRO
    {"x........x......","....x...x....x..","x.x.x.x.x.xxxxxx"}, // TRAP
    {"x...............","..x......x......","x..x..x..x..x.x."}, // BOOM
    {"x..x....x..x.x..","....x.....x..x..","..x..x.x..x..x.x"}, // UKG
    {"x..x..x...x...x.","....x..x........","x.x.xx.x.xx..x.."}, // JERSEY
    {"x...x.....x...x.","....x.x.....x...","x..x..x..x..x.x."}, // BOUNCE
    {"x.....x.........","....x....x......","x..x....x..x...."}, // SOUL
    {"x....x....x.....","....x.......x...","x.x..x..x.x..x.."}, // LOFI
    {"x..x....x..x..x.","....x...x..x....","x.x..x.xx.x..x.x"}, // FUNK
    {"x.....x.....x...","..x..........x..","x..x..x..x..x..."}, // REGGAE
    {"x...x...x...x...","....x.......x...","x.x..x.x..x.x..x"}, // DANCEHALL
    {"x..x....x...x..x","....x..x........","x..xx..x..x..x.x"}, // AMA-sort
    {"x.........x.....","....x....x..x...","x.x.x.x.x.x.x.x."}, // KAWINA v2: kwakwa bangi-tijdlijn (oneven stappen) + skratjie 1&~9, chik vóór de tel
    {"x..x....x..x....","....x.......x...","x.x.x.x.x.x.x.xx"}, // DNB
    {"x..x.....x......","......x....x....","x..xx..x..x..x.."}, // GRIME
    {"x...x...x...x...","....x.......x...","x..x..x.xx..x..x"}, // AFROTECH
    {"x..x..x...x..x..","....x..x....x...","x.x.x.xx.x.x.x.x"}, // LATIN
    {"x.....x...x.....","......x....x....","x..x..x..x..x.x."}, // ZOUK
    {"x.....x...x.....","...x...x....x.x.","x.x.x.x.x.x.x.x."}, // BUBBLE v2: DJ Moortje rimshot-patroon 1/7/11 + rim 4/8/13/15
    {"x...x...x...x...","....x.......x...","x.x.x.x.x.x.x.x."}, // EDM
    {"x...x...x...x...","....x.......x...","x..x..x..x..x..x"}, // POP
    {"x..x..x....x..x.","....x.......x...","x.x.xx.x.x.xx.x."}, // FUNKY
    {"x..x.x..x..x..x.","....x..x.....x..","x..x.x.x.x.x.x.x"}   // SURIN
};
static const char kBasPat[27][8]={
    {0,-1,0,-1,3,-1,0,-1},  // DEMBOW
    {0,-1,-1,5,-1,0,-1,-1}, // DRILL
    {0,-1,-1,-1,5,-1,-1,-1},// RNB
    {0,0,-1,-1,0,-1,5,-1},  // HOUSE
    {0,-1,3,-1,0,-1,5,-1},  // AFRO
    {0,-1,-1,0,-1,-1,5,-1}, // TRAP
    {0,-1,-1,-1,0,-1,-1,-1},// BOOM
    {0,-1,2,-1,0,-1,5,-1},  // UKG
    {0,-1,0,3,-1,0,-1,5},   // JERSEY
    {0,-1,0,-1,3,-1,0,-1},  // BOUNCE
    {0,-1,-1,-1,-1,-1,0,-1},// SOUL
    {0,-1,3,-1,0,-1,-1,-1}, // LOFI
    {0,0,-1,3,-1,0,-1,5},   // FUNK
    {0,-1,-1,5,-1,-1,0,-1}, // REGGAE
    {0,-1,0,-1,3,-1,0,-1},  // DANCEHALL
    {0,-1,-1,0,-1,5,-1,-1}, // AMA-sort
    {0,-1,-1,0,-1,-1,-1,-1},// KAWINA v2: syncopische 808-bounce (tel 1 + eind tel 2) — 'haast'-effect
    {0,-1,-1,-1,5,-1,0,-1}, // DNB
    {0,-1,5,-1,-1,3,-1,-1}, // GRIME
    {0,0,-1,3,-1,0,-1,-1},  // AFROTECH
    {0,-1,3,-1,0,-1,2,-1},  // LATIN
    {0,-1,-1,3,-1,0,-1,-1}, // ZOUK
    {0,-1,-1,0,0,-1,0,-1},  // BUBBLE v2: staccato chugging bas — nooit doorklinken
    {0,0,0,0,0,0,0,0},      // EDM
    {0,-1,0,-1,0,-1,3,-1},  // POP
    {0,-1,2,-1,3,-1,5,-1},  // FUNKY
    {0,-1,3,-1,2,-1,0,-1}   // SURIN
};
static const char kChr[4]={0,3,10,14};
static const char kThird[27]={3,4,3,4,4,4,3,3,4,4,4,4,3,4,4,4,4,3,3,4,4,4,4,4,4,4,4};
static const char kSev[27]={10,10,10,11,10,10,10,10,10,10,10,11,11,10,10,10,11,10,10,10,10,11,10,10,11,10,11};
static const char kProg[27][4]={ // v110: echte akkoord-progressies per stijl
    {0,5,3,4},{0,5,3,4},{0,5,2,4},{0,3,5,4},{0,5,3,6},{0,5,3,6},{0,3,5,4},
    {0,3,1,4},{0,5,3,6},{0,6,3,5},{0,5,6,3},{3,0,4,5},{1,4,0,3},{0,3,0,4},
    {0,4,3,5},{0,5,3,6},{0,5,3,4},{0,5,1,4},{0,2,5,4},{0,3,5,6},{0,4,3,5},
    {0,3,5,4},{0,6,3,5},{0,5,3,6},{0,4,5,3},{0,3,4,5},{0,5,3,4}};
static const char kGPerc[27]={19,15,12,6,7,5,6,31,8,21,23,31,31,7,17,6,19,8,23,4,8,5,25,6,6,7,4}; // echte perc per stijl




// ---- GALAXY-knop (bron: backend/app/galaxy_fx.py, devin-test) — 27 posities ----
// 0-8=DARK (octaaf ONDER+donker), 9-17=LIGHT (octaaf BOVEN+helder), 18-26=GALAXY (+kwint, kosmisch)
struct GxShifter { // pitch-shift via gemoduleerde delay, 2 taps + driehoek-window
    static const int N=2048; float buf[N]={}; int wp=0; float ph=0.f;
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
struct Galaxy {
    GxShifter sh[4]; // 2 = shimmer L/R, 2 = kwint L/R
    static const int CN=2200;
    float cb[4][CN]={}; int cw[4]={};
    float ap[2][600]={}; int aw[2]={};
    float lp[2]={};
    int gpos=-1; float grate=2.f,gfb=0.5f,gfc=6500.f,gwet=0.6f,gdd=0.2f; int gmode=2; float keyMul=1.f; // v91: key-follow shimmer
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
            float x=sh[c].tick(in[c],grate*keyMul);
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
        L=in[0]+gwet*shim[0]*0.5f; R=in[1]+gwet*shim[1]*0.5f;
        if(L>0.98f)L=0.98f; else if(L<-0.98f)L=-0.98f;
        if(R>0.98f)R=0.98f; else if(R<-0.98f)R=-0.98f;
    }
};

class SongPlugin : public Plugin
{
public:
    SongPlugin() : Plugin(28, 0, 13) { for(int r=0;r<10;r++)celMute_[r]=true; extraSnd_[0]=extraSnd_[1]=0; gSongRackView=&rackView_;
        for(int a=0;a<3;a++)for(int b=0;b<17;b++)mut_[a][b]=kPat[0][a][b]; for(int b=0;b<8;b++)mutBas_[b]=kBasPat[0][b]; for(int s=0;s<16;s++)melDegU_[s]=-1; seedMask_(); }

protected:
    const char* getLabel() const override { return "G2B SONG"; }
    const char* getDescription() const override
    { return "G2B all-in-one beat-machine: drums + glijdende 808 + akkoorden + melodie. Echte pitch-slide, MIDI-bespeelbaar."; }
    const char* getMaker() const override { return "G2B (good2better)"; }
    const char* getHomePage() const override { return "https://github.com/good2better"; }
    const char* getLicense() const override { return "MIT"; }
    uint32_t getVersion() const override { return d_version(1,1,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','D','g'); }

    void initState(uint32_t index, String& name, String& def) override {
        static const char* k[13]={"step","midifile","pat0","pat1","pat2","pat3","pat4","pat5","pat6","pat7","pat8","pat9","lanes"};
        name=k[index<13?index:12]; def="";
    }
    void setState(const char*, const char*) override {}

    enum { kStijl, kKey, kScale, kDrums, kBas, kChord, kMel,
           kGlide, kDrop, kSub,kGalaxy, kDice, kAuto, kLight, kPreset, kMidi, kExport, kDens, kSwing, kHuman, kFill, kGeluid, kRoom, kTouch, kEen, kSugg, kCell, kRun };

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints = kParameterIsAutomable;
        switch(i){
        case kStijl: p.name="STYLE"; p.symbol="style"; p.ranges.min=0; p.ranges.max=26; p.ranges.def=0; p.hints|=kParameterIsInteger; break;
        case kKey:   p.name="KEY";   p.symbol="key";   p.ranges.min=0; p.ranges.max=11; p.ranges.def=9; p.hints|=kParameterIsInteger; break;
        case kScale: p.name="SCALE"; p.symbol="scale"; p.ranges.min=0; p.ranges.max=2; p.ranges.def=0; p.hints|=kParameterIsInteger; break;
        case kDrums: p.name="DRUMS"; p.symbol="drums"; p.ranges.min=0; p.ranges.max=1; p.ranges.def=0.8f; break;
        case kBas:   p.name="808";   p.symbol="bas";   p.ranges.min=0; p.ranges.max=1; p.ranges.def=0.7f; break;
        case kChord: p.name="CHORD"; p.symbol="chord"; p.ranges.min=0; p.ranges.max=1; p.ranges.def=0.6f; break;
        case kMel:   p.name="MEL";   p.symbol="mel";   p.ranges.min=0; p.ranges.max=1; p.ranges.def=0.6f; break;
        case kGlide: p.name="GLIDE"; p.symbol="glide"; p.ranges.min=0; p.ranges.max=1; p.ranges.def=0.35f; break;
        case kDrop:  p.name="DROP";  p.symbol="drop";  p.ranges.min=0; p.ranges.max=1; p.ranges.def=0.55f; break;
        case kSub:   p.name="DONDER"; p.symbol="donder";   p.ranges.min=0; p.ranges.max=1; p.ranges.def=0.35f; break;
        case kGalaxy:p.name="GALAXY"; p.symbol="galaxy";p.ranges.min=0; p.ranges.max=26; p.ranges.def=9; p.hints|=kParameterIsInteger; break;
        case kDice:  p.name="DICE";  p.symbol="dice";  p.ranges.min=0; p.ranges.max=1; p.hints|=kParameterIsTrigger; break;
        case kAuto:  p.name="AUTO";  p.symbol="auto";  p.ranges.min=0; p.ranges.max=1; p.ranges.def=0; p.hints|=kParameterIsInteger; break;
        case kLight: p.name="SKIN"; p.symbol="skin"; p.ranges.min=0; p.ranges.max=7; p.ranges.def=0; p.hints|=kParameterIsInteger; break;
        case kPreset:p.name="PRESET";p.symbol="preset";p.ranges.min=0; p.ranges.max=26; p.ranges.def=0; p.hints|=kParameterIsInteger; break;
        case kMidi:  p.name="MIDI OUT";p.symbol="midiout";p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;p.hints|=kParameterIsBoolean; break;
        case kEen:   p.name="1-TOETS"; p.symbol="eentoets";p.ranges.min=0;p.ranges.max=5;p.ranges.def=0;p.hints|=kParameterIsInteger; break;
        case kSugg:  p.name="SUGG";    p.symbol="sugg";   p.ranges.min=0;p.ranges.max=7;p.ranges.def=0;p.hints|=kParameterIsOutput; break;
        case kCell:  p.name="CELL";    p.symbol="cell";   p.ranges.min=0;p.ranges.max=413;p.ranges.def=0;p.hints|=kParameterIsInteger; break;
        case kExport:p.name="EXPORT .MID";p.symbol="exportmid";p.ranges.min=0;p.ranges.max=1;p.hints|=kParameterIsTrigger; break;
        case kDens:  p.name="DENS";p.symbol="dens";p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.7f; break;
        case kSwing: p.name="SWING";p.symbol="swing";p.ranges.min=0;p.ranges.max=2;p.ranges.def=1.f; break;
        case kHuman: p.name="HUMAN";p.symbol="human";p.ranges.min=0;p.ranges.max=2;p.ranges.def=1.f; break;
        case kFill:  p.name="FILL";p.symbol="fill";p.ranges.min=0;p.ranges.max=2;p.ranges.def=1.f; break;
        case kGeluid:p.name="GELUID";p.symbol="geluid";p.ranges.min=0;p.ranges.max=2;p.ranges.def=1.f;p.hints|=kParameterIsInteger; break;
        case kRoom:  p.name="ROOM";  p.symbol="room";  p.ranges.min=0;p.ranges.max=1;p.ranges.def=.25f; break;
        case kTouch: p.name="TOUCH"; p.symbol="touch"; p.ranges.min=0;p.ranges.max=1;p.ranges.def=.5f; break;
        case kRun:   p.name="SPEEL"; p.symbol="speel"; p.hints|=kParameterIsBoolean; p.ranges.min=0;p.ranges.max=1;p.ranges.def=1; break;
        }
    }

    float getParameterValue(uint32_t i) const override {
        switch(i){case kStijl:return fStijl;case kKey:return fKey;case kScale:return fScale;
                  case kDrums:return fDrums;case kBas:return fBas;case kChord:return fChord;
                  case kMel:return fMel;case kGlide:return fGlide;case kDrop:return fDrop;
                  case kSub:return fSub;case kGalaxy:return fGalaxy;case kDice:return fDice;case kAuto:return fAuto;
                  case kLight:return fLight;case kPreset:return fPreset;case kMidi:return fMidi;case kExport:return 0.f;case kDens:return fDens;case kSwing:return fSwing;case kHuman:return fHuman;case kFill:return fFill;case kGeluid:return fGeluid;case kRoom:return fRoom;case kTouch:return fTouch;case kEen:return fEen;case kSugg:return fSugg;case kCell:return 0.f;case kRun:return fRun;}return 0.f;
    }

    void setParameterValue(uint32_t i, float v) override {
        switch(i){
        case kStijl:fStijl=v;break; case kKey:fKey=v;break; case kScale:fScale=v;break;
        case kDrums:fDrums=v;break; case kBas:fBas=v;break; case kChord:fChord=v;break;
        case kMel:fMel=v;break; case kGlide:fGlide=v;break; case kDrop:fDrop=v;break;
        case kSub:fSub=v;break; case kGalaxy:fGalaxy=v;galaxy.setPos((int)v);break;
        case kDice:if(v>0.5f)rollDice();fDice=0;break;
        case kAuto:fAuto=v;break; case kLight:fLight=v;break; case kMidi:fMidi=v;break; case kEen:fEen=v;break; case kRun:fRun=v;break;
        case kExport:if(v>0.5f)writeMid_();break;
        case kPreset:if(v!=fPreset)applyPreset((int)v);fPreset=v;break;
        case kDens:fDens=v;break; case kSwing:fSwing=v;break; case kHuman:fHuman=v;break;
        case kFill:fFill=v;break;
        case kGeluid:fGeluid=v;break;case kRoom:fRoom=v;break;case kTouch:fTouch=v;break;
        case kCell:{ // 10-rij rack: 0..159 = vakje rij*16+stap; 170..179 = laag-dice; 180..189 = laag-mute; 190/191 = +X1/+X2 geluid wisselen; 200..209 = laag terug naar stijl-origineel (SHIFT+dice)
            int c=(int)(v+0.5f); if(c<0||c>413)break;
            if(c>=350){int i2=c-350,p=i2/8,c2=i2%8; // 808-roll: 350+p*8+cel — toontrap 0..7 zelf zetten
                if(p<8){ if(mutBas_[c2]==p){mutBas_[c2]=-1;celMask_[7][c2*2]=0;}
                    else {mutBas_[c2]=p;celMask_[7][c2*2]=1;bass(deg2midi(p+root_),0.8f);} pushPats();break;}}
            if(c>=210){int i2=c-210,p=i2/16,s=i2%16; // MEL-roll: 210+p*16+stap — jouw noot op jouw stap
                if(p<8){ if(melDegU_[s]==p){melDegU_[s]=-1;celMask_[9][s]=0;}
                    else {melDegU_[s]=p;celMask_[9][s]=1;int nm=deg2midi(p+root_)+12;mv_.f=m2f(nm);mv_.on=true;mv_.t=0;mvPick_(nm);lastMel_=nm;hookM_[((int)(fStep/16))%4][s/4]=nm;} pushPats();break;}}
            if(c>=200){laneRestore(c-200);break;}
            if(c==190||c==191){extraSnd_[c-190]=(extraSnd_[c-190]+1)%6;pushLanes();break;}
            if(c>=180){int r=c-180; if(r<0||r>9)break; celMute_[r]=!celMute_[r]; pushLanes(); break;}
            if(c>=170){laneDice(c-170);break;}
            int r=c/16,s2=c%16; if(r<0||r>9)break;
            celMask_[r][s2]=celMask_[r][s2]?0:1;
            if(r==7&&celMask_[7][s2]&&mutBas_[s2/2]<0)mutBas_[s2/2]=0; // 808-cel aan → geef hem een noot
            if(celMask_[r][s2]){ // hoor-bij-klik: speel het geluid 1x zodat je hoort wat je aanzette
                if(r==0)dv(0,0,150.f,48.f,.28f,.8f);
                else if(r==1)dv(1,1,190.f,170.f,.16f,.45f);
                else if(r==2)dv(2,2,0.f,0.f,.05f,.25f);
                else if(r==3)dv(1,1,240.f,190.f,.10f,.35f);
                else if(r==4)dv(3,4,0.f,0.f,.12f,.18f);
                else if(r==5||r==6){int xi=r-5;if(extraSnd_[xi]==0)extraSnd_[xi]=1;if(extraSnd_[xi]==5)melody();else dv(2,2,0.f,0.f,.10f,.30f);}
                else if(r==7)bass(deg2midi(root_),0.8f);
                else if(r==8)chord(0.8f);
                else melody();}
            pushPats();}
            break;
        }
        if(i<27)rackView_.prm[i]=getParameterValue(i); // spiegel → UI knoppen draaien mee
    }

    void run(const float** in, float** out, uint32_t frames, const MidiEvent* evs, uint32_t nev) override {
        const float sr=getSampleRate();
        double bpm=100.0;
        const TimePosition& tp=getTimePosition();
        if(tp.playing && tp.bbt.valid && tp.bbt.beatsPerMinute>1.0) bpm=tp.bbt.beatsPerMinute;
        fBpm=bpm;
        // host-stop = echte stop: geen geldige transport-info → gewoon spelen (free-run hosts)
        const bool hostHasTransport=tp.bbt.valid;
        const bool playNow=(fRun>0.5f)&&(!hostHasTransport||tp.playing);
        if(wasPlaying_ && !playNow){                    // stop-transitie: stil + terug naar stap 1
            for(auto& v:dv_)v.on=false;
            bv_.on=false; cv_.on=false; cv2_.on=false; mv_.on=false; mv2_.on=false;
            while(offr_!=offw_){PendingOff& o=offq_[offr_];emitMidi(0x80|o.ch,o.note,0,0);offr_=(offr_+1)%64;}
            phn_=0; runN_=0; arpOn_=false; respAt_=0xFFFFFFFFu;
            fStep=0; fPhase=0;
        }
        wasPlaying_=playNow;
        const double stepLen=60.0/bpm/4.0;
        if(fStepLen<=0)fStepLen=stepLen; // swing: oneven stappen iets later — menselijke feel
        float stF=(fStijl<0?0:(fStijl>26?26:fStijl)); const int st=(int)stF; const float morphF=stF-st;
        if(st!=fMutStyle && fAuto>0.5f && pendSt_<0){pendSt_=st;}
        if(st!=fMutStyle || fabsf(morphF-fMorphF)>0.02f){if(fAuto>0.5f&&fStep%16!=0)goto skipSeed; fMutStyle=st;fMorphF=morphF;for(int l2=0;l2<3;l2++)mtLane_[l2]=(int)((frand(fRs)-0.5f)*fStepLen*(fStepLen>0?1.f/fStepLen:1.f)*0.f);for(int l2=0;l2<3;l2++)mtLane_[l2]=(int)((frand(fRs)-0.5f)*sr*0.010f);const int stB=(st<26)?st+1:0;for(int a=0;a<3;a++)for(int b=0;b<17;b++)mut_[a][b]=(frand(fRs)<morphF?kPat[stB][a][b]:kPat[st][a][b]);for(int b=0;b<8;b++)mutBas_[b]=(frand(fRs)<morphF?kBasPat[stB][b]:kBasPat[st][b]);seedMask_();phn_=0;}
        skipSeed:
        uint32_t ei=0;
        for(uint32_t i=0;i<frames;i++){
            // MIDI-note events op exacte sample-positie
            while(ei<nev && evs[ei].frame<=i){
                const MidiEvent& e=evs[ei++];
                if(e.size>=3){
                    uint8_t st2=e.data[0]&0xF0, ch=e.data[0]&0x0F, nt=e.data[1], vel=e.data[2];
                    if(st2==0x90 && vel>0){
                        if(ch==0&&fEen>0.5f){ eenKey(nt,vel,i); }
                        else if(ch==0){ bassMidi(nt,vel/127.f);
                            int rel=nt-36-(int)fKey; rel=((rel%12)+12)%12;
                            int sc2[7]={0,2,3,5,7,8,10}; int bd=0,bdD=99;
                            for(int d=0;d<7;d++){int dd=abs(rel-sc2[d]);if(dd<bdD){bdD=dd;bd=d;}}
                            mutBas_[(fStep/2)%8]=bd; // jouw bass-noot → bascel
                            // RESPOND: engine antwoordt een 16e later, een interval omhoog in de toonsoort
                            int rd=bd+(frand(fRs)<.5f?2:3); int oct2=rd/7; rd%=7;
                            respN_=nt+sc2[rd]-sc2[bd]+12*oct2; respAt_=i+(uint32_t)(stepLen*sr);
                        }
                        else if(ch==1){cv2_.on=true;cv2_.t=0;cv2_.f=m2f(nt);
                            if(fGeluid>=0.5f){const HarpSmp* ps;pickHarp(ps,nt);cv2_.smp=vel>=80?ps->d2:ps->d;cv2_.slen=vel>=80?ps->len2:ps->len;cv2_.sstep=(cv2_.f/m2f(ps->midi))*(22050.f/getSampleRate());cv2_.spos=0;}} // akkoord-stab live
                        else if(ch==2){mv_.f=m2f(nt);mv_.on=true;mv_.t=0;lastMel_=nt;hookM_[(fStep/16)%4][(fStep%16)/4]=nt;mvPick_(nt,vel/127.f);} // jouw noot → hook-cel
                        else { // ch3: drums live — jouw taps schrijven direct in het patroon
                            int rs=fStep%16;
                            if(nt==36){dv(0,0,kitKf0,kitKf1,kitKdec,kitKamp*vel/127.f);fDuck=0.f;mut_[0][rs]='1';}
                            else if(nt==38){dv(1,1,kitSf0,kitSf1,kitSdec,kitSamp*vel/127.f);mut_[1][rs]='1';}
                            else if(nt==45||nt==47){dv(1,2,nt==47?95.f:110.f,nt==47?72.f:80.f,.10f,.22f*vel/127.f);mut_[2][rs]='1';}
                            else if(nt==49){dv(2,3,0.f,0.f,kitHdec*6.f,.34f*vel/127.f);} // crash
                            else if(nt==51){dv(2,2,0.f,0.f,kitHdec*2.5f,.20f*vel/127.f);} // ride
                            else {dv(2,2,0.f,0.f,kitHdec,kitHamp*vel/127.f);mut_[2][rs]='1';}
                        }
                    }
                    else if(st2==0x80 || (st2==0x90 && vel==0)){ if(ch==0){bassOff(nt);arpOn_=false;} }
                }
            }
            if(playNow){ // sequencer + geplande hits lopen alleen tijdens host-play
            fPhase+=1.0/sr;
            // MIDI note-offs die deze sample vallen
            for(int k=0;k<phn_;k++)if(ph_[k].at<=i){dv(ph_[k].t,ph_[k].v,ph_[k].f0,ph_[k].f1,ph_[k].dec,ph_[k].amp);ph_[k].at=0xFFFFFFFFu;}
            while(offr_!=offw_){PendingOff& o=offq_[offr_];if(o.at>i)break;emitMidi(0x80|o.ch,o.note,0,i);offr_=(offr_+1)%64;}
            if(respAt_==i){bass(respN_,0.55f);midiHit(respN_,1,75,i,stepLen*sr*1.5f);respAt_=0xFFFFFFFFu;} // RESPOND-answer
            for(int k2=0;k2<runN_;k2++)if(runQ_[k2].at==i){mvPick_(runQ_[k2].n,runQ_[k2].v/127.f);mv_.f=m2f(runQ_[k2].n);mv_.on=true;mv_.t=0;midiHit(runQ_[k2].n,2,runQ_[k2].v,i,stepLen*sr*0.5f);} // RUN-toets geplande noten
            if(arpOn_&&arpAt_==i){int an=chordDeg(arpI_%4,0,root_)+12;mvPick_(an,0.7f);mv_.f=m2f(an);mv_.on=true;mv_.t=0;midiHit(an,2,80,i,stepLen*sr*0.3f);arpI_++;arpAt_+= (uint32_t)(stepLen*sr);} // ARP blijft lopen
            if(fPhase>=fStepLen){
                fPhase=0; fStep++;
                int s=(int)fStep%16;
                if(pendSt_>=0&&s==0){int ps=pendSt_;pendSt_=-1;fMutStyle=-1;setParameterValue(kStijl,ps);} int mtK=(int)((frand(fRs)+frand(fRs)+frand(fRs)-1.5f)*sr*0.010f*fHuman); // v161 drummer-jitter · v164 HUMAN-knop
                int mtS=(int)((frand(fRs)+frand(fRs)+frand(fRs)-1.5f)*sr*0.012f*fHuman);
                int mtH=(int)((frand(fRs)+frand(fRs)+frand(fRs)-1.5f)*sr*0.018f*fHuman);
                const float sw=((st==16||st==0||st==1||st==15||st==25||st==26)?0.19f:((st==17||st==18||st==23||st==24)?0.03f:0.14f))*fSwing; // v163 SWING-knop schaalt stijl-swing
                if(fAuto>0.5f && s==0 && (fStep%64)==0) rollDice();
                int bar=(int)(fStep/16)%8;
                const bool fill=(fFill>0.25f && bar==7 && s>=12) || (fFill>1.5f && bar==3 && s>=14); // v165 FILL-knop
                                const int root=kProg[st][bar%4]; root_=root; // v110: huidige akkoord-grondtoon
                if(s==0 && fStep>0 && (fStep%(16*8))==0){
                    if(frand(fRs)<.3f)hookM_[(int)(frand(fRs)*3.99f)][(int)(frand(fRs)*3.99f)]=0; // v98 hook-cel vrij → nieuwe noot mag
                 // evolutie: nieuwe sectie muteert 1-2 cellen + bas
                    {int b2=(int)(frand(fRs)*7.99f); mutBas_[b2]=(mutBas_[b2]<0)?(int)(frand(fRs)*6.99f):-1;}
                    for(int m=0;m<2;m++){int a=(int)(frand(fRs)*2.99f),b=(int)(frand(fRs)*15.99f);
                        if(!(a==0&&(b==0||b==8))) mut_[a][b]=(mut_[a][b]=='.')?'X':'.';}
                    seedMask_(); phn_=0;
                }
                // stijl-eigen kit: elke dimensie klinkt anders (deterministisch per stijl)
                uint32_t ks=1000u+(uint32_t)st*61u;
                auto kr=[&](){ks^=ks<<13;ks^=ks>>17;ks^=ks<<5;return (ks&0xFFFFFFu)/16777216.f;};
                const float kf0=140.f+kr()*60.f,kf1=42.f+kr()*18.f,kdec=.20f+kr()*.16f,kamp=.78f+kr()*.24f;
                const float sf0=175.f+kr()*40.f,sf1=150.f+kr()*45.f,sdec=.11f+kr()*.09f,samp=.40f+kr()*.22f;
                const float hdec=.032f+kr()*.05f,hamp=.22f+kr()*.18f;
                kitKf0=kf0;kitKf1=kf1;kitKdec=kdec;kitKamp=kamp;kitSf0=sf0;kitSf1=sf1;kitSdec=sdec;kitSamp=samp;kitHdec=hdec;kitHamp=hamp;
                float jv=(0.88f+0.24f*frand(fRs))*((s==4||s==12)?1.15f:1.f); // humanize + accent op backbeat
                jv=1.f+(jv-1.f)*fHuman; // v164 HUMAN schaalt velocity-jitter
                // AUTO-arrange: intro spaars (bar0), break (bar3), fill (bar7)
                const int sec=(int)(fStep/16)%16; // v39: 16-bar form
                const float arc=(fStep>0)?(0.72f+0.28f*sinf(6.28318f*((float)(fStep%1024))/1024.f+1.57f)):1.f; // v69: energie-boog over 64 bars
                const int songB=(int)(fStep/16)%64; // v111 AUTO-SONG: 0-7 intro,8-23 verse,24-31 chorus,32-47 verse2,48-55 chorus2,56-63 brug
                const bool chorus=(songB>=24&&songB<32)||(songB>=48&&songB<56);lift_=chorus?7:0; // kwintencirkel-lift naar dominant
                const bool intro=songB<8; const bool outro=songB>=56;
                const float secE=intro?0.55f:(chorus?1.25f:(outro?0.7f:1.f));
                const bool bd=(sec>=8&&sec<12); // breakdown
                const bool choke=(songB==23||songB==47)&&s>=14; // v151 stilte voor de drop
                const bool gKick=(bar!=3&&!(bd&&s%8!=0&&s%8!=4))&&(bar!=0||s==0||s==8)&&(intro?(s%8==0||s%8==4):true)&&!choke;
                const bool gSnare=bar>0&&bar!=3&&!intro&&!choke;
                const bool gHat=(bar!=0||s%2==1||chorus)&&!(bd&&s%4!=0&&!chorus)&&!choke;
                const bool gBass=bar>0;
                if((songB==23||songB==47)&&s>=12&&s%2==0&&phn_<21){ // v113 riser naar chorus
                    ph_[phn_++]={2,2,9000.f+600.f*(s-12),11000.f,hdec*.5f,.12f+(s-12)*.03f,i};}
                if((st==5||st==16||st==20||st==26)&&s%4==3&&phn_<21&&frand(fRsD)<fDens){ // v121 polyrhythm tripel-perc · v162 DENS
                    uint32_t trd=(uint32_t)(stepLen*sr/3.f);
                    for(int k=0;k<2;k++)ph_[phn_++]={2,2,0.f,0.f,hdec*.8f,hamp*.30f,i+trd*(k+1)};
                }
                if(fill){ // v46: cycli — snare-roll / tom-fill / EUCLIDEAN / cascade
                    if(((int)(fStep/128)%4)==3){if(s>=13&&phn_<21){uint32_t trd2=(uint32_t)(stepLen*sr/3.f);float tp2=230.f-(s-13)*36.f;for(int k=0;k<3;k++){ph_[phn_++]={1,2,tp2,tp2*.7f,.10f,.28f,i+trd2*k};midiHit(45,0,66,i+trd2*k,stepLen*sr*0.2f);}}}
                    else
                    if(((int)(fStep/128)%3)==2){
                        const int kk=3+((int)(fStep/384)%3); // v166 bjorklund: e(3,8)/e(4,8)/e(5,8) — (s*k)%n<k = eerlijke spreiding
                        if(s>=8&&s<16&&((s-8)*kk)%8<kk){dv(1,1,sf0,sf1*1.1f,sdec*.9f,samp*.55f);midiHit(38,0,72,i,stepLen*sr*0.4f);
                            dv(2,2,0.f,0.f,hdec*.7f,hamp*.4f);midiHit(42,0,48,i,stepLen*sr*0.25f);}
                    }
                    else if(((int)(fStep/128)%2)==0){dv(1,1,sf0,sf1,sdec,samp*(.5f+.13f*(s-12))); midiHit(38,0,60+(s-12)*9,i,stepLen*sr*0.4f);
                        dv(2,2,0.f,0.f,hdec,hamp*.5f);           midiHit(42,0,45,i,stepLen*sr*0.3f);}
                    else{float tp=130.f-(s-12)*18.f;dv(1,2,tp,tp*.8f,.09f,.26f+.05f*(s-12));midiHit(45,0,65,i,stepLen*sr*0.35f);
                        dv(2,2,0.f,0.f,hdec*.8f,hamp*.35f);     midiHit(42,0,40,i,stepLen*sr*0.25f);}
                    if((st==5||st==17||st==18)&&s>=14&&phn_<22){ // trap/dnb/grime hat-roll
                        uint32_t third=(uint32_t)(stepLen*sr/3.f);
                        for(int k=0;k<2;k++){ph_[phn_++]={2,2,0.f,0.f,hdec*.5f,hamp*.4f,i+third*(k+1)};midiHit(42,0,40,i+third*(k+1),stepLen*sr*0.25f);}}
                } else {
                if((st==8||st==9) && s%4==2 && gKick && frand(fRsD)<fDens){dv(0,0,kf0*1.4f,kf1,kdec*.5f,kamp*.22f*jv);} // v77 jersey ghost-kick
                if(songB>=48&&songB<56&&s%4==2&&gKick&&!choke&&frand(fRsD)<fDens){dv(0,0,kf0*1.25f,kf1,kdec*.4f,kamp*.20f*jv);midiHit(36,0,50,i,stepLen*sr*0.3f);} // v153 2e chorus extra
                if(arc>.78f && s%8==6 && frand(fRsD)<fDens){dv(1,2,110.f,80.f,.10f,.20f*jv*arc);midiHit(45,0,(int)(55*arc),i+mtH,stepLen*sr*0.5f);} // tom
                if((st==12||st==11||st==13)&&frand(fRs)<.07f&&frand(fRsD)<fDens){dv(2,2,0.f,0.f,.008f,.07f);} // v156 vinyl-knetsers
                { // v32 shaker-bed: 16e perc onder de beat
                    float shA=(st==16||st==20||st==25||st==0)?.16f:((st==6||st==2)?.05f:.10f); shA*=arc*secE*fDens; // shaker ademt met boog+sectie · v162 DENS
                    dv(2,2,0.f,0.f,hdec*.6f,shA*((s%2==0)?1.f:.45f)*jv*((st==20||st==16||st==26)&&(s%8==2)?1.4f:1.f));}
                if((st==1||st==5||st==18||st==2)&&gKick && celMask_[0][s]&&phn_<22){ph_[phn_++]={3,3,35.f,32.f,.5f,.30f,i+2};} // v38 sub
                if(gKick && celMask_[0][s]&&celMute_[0]){if(chorus||phn_>9){dv(0,0,kf0,kf1,kdec,kamp*jv*(s==0?1.14f:1.f));}else{ph_[phn_++]={0,0,kf0,kf1,kdec,kamp*jv*(s==0?1.14f:1.f),i+(uint32_t)(sr*0.004f)};}midiHit(36,0,(int)(105*jv),i+mtH,stepLen*sr*0.9f);fDuck=0.f;} // v158 laid-back kick
                if(gSnare && celMask_[1][s]&&celMute_[1]){dv(1,1,sf0,sf1,sdec,samp*jv);midiHit(38,0,(int)(95*jv),i+mtH,stepLen*sr*0.9f);}
                if(celMask_[3][s]&&celMute_[3]){dv(1,1,sf0*1.25f,sf1*1.15f,sdec*.65f,samp*.55f*jv);midiHit(39,0,(int)(70*jv),i+mtH,stepLen*sr*0.5f);} // CLAP-rij
                if(celMask_[4][s]&&celMute_[4]){dv(3,4,0.f,0.f,.12f,.20f*jv);midiHit(37,0,(int)(50*jv),i+mtH,stepLen*sr*0.4f);} // PERC-rij
                for(int xr=0;xr<2;xr++){int xrrow=5+xr; // +X1/+X2: eigen geluid kies je met het label
                    if(celMask_[xrrow][s]&&celMute_[xrrow]&&extraSnd_[xr]>0){
                        int xs=extraSnd_[xr];
                        if(xs==1){dv(2,2,0.f,0.f,hdec*3.5f,hamp*.5f*jv);}            // TAMBO
                        else if(xs==2){dv(1,2,660.f,420.f,.09f,.30f*jv);}          // COWBELL
                        else if(xs==3){dv(2,2,0.f,0.f,hdec*.28f,hamp*1.1f*jv);}    // RIM
                        else if(xs==4){dv(2,2,0.f,0.f,hdec*.6f,hamp*.5f*jv);}      // SHAKER
                        else {melody();}                                           // BELL: melodie-stem
                        midiHit(xs==5?lastMel_:54+xr,0,(int)(60*jv),i+mtH,stepLen*sr*0.3f);}}
                    if(phn_<22&&frand(fRsD)<fDens){uint32_t dly=(uint32_t)(sr*.09f);ph_[phn_++]={1,1,sf0*.9f,sf1,sdec*3.2f,samp*.12f*jv,i+dly};} // v34 staart
                if(s==0 && (bar==0||bar==4)){dv(2,2,0.f,0.f,.7f,.14f);midiHit(49,0,80,i,stepLen*sr*4.f);} // crash op sectie-start
                if(gHat && s%4==2 && bar>0 && frand(fRsD)<fDens && (((int)fStijl==4||(int)fStijl==20||(int)fStijl==15)?true:(frand(fRs)<.4f))){int ho=mtH+mtLane_[2];if(phn_<22){ph_[phn_++]={2,2,0.f,0.f,hdec*4.5f,hamp*.55f*jv,i+(uint32_t)(ho>0?ho:0)};}else{dv(2,2,0.f,0.f,hdec*4.5f,hamp*.55f*jv);}midiHit(46,0,(int)(55*jv),i+mtH,stepLen*sr*0.8f);} // open hat
                if(fGeluid>=0.5f&&(s==6||s==14)&&frand(fRsD)<fDens*.9f){dv(3,4,0.f,0.f,.12f,.16f*jv);} // v165 echte perc per stijl
                if(gHat && celMask_[2][s]&&celMute_[2] && !((st==6||st==13||st==2)&&s%2==1&&frand(fRs)<.42f)){dv(2,2,0.f,0.f,hdec*((st==4||st==23||st==20)?1.6f:1.f),hamp*jv*((st==1||st==2||st==6||st==18)?0.72f:1.f)); // v141 hat-kleur
                    midiHit(42,0,(int)(75*jv),i+mtH,stepLen*sr*0.5f);}
                else if(frand(fRsD)<fDens&&(fRs=fRs*1664525u+1013904223u,(fRs>>9)&15)<2){dv(2,2,0.f,0.f,.04f,.09f);midiHit(42,0,30,i,stepLen*sr*0.4f);} // ghost-hat
                if(s%4==2 && mut_[1][s]=='.' && frand(fRsD)<fDens && ((fRs>>9)&31)<3){dv(1,1,190.f,170.f,.10f,.14f);midiHit(38,0,28,i,stepLen*sr*0.5f);} // ghost-snare
                }
                int b8=(s/2)%8;
                if(gBass && s%2==0 && celMask_[7][s]&&celMute_[7]){int nm=deg2midi((mutBas_[b8]<0?0:mutBas_[b8])+root)+(((int)fStep/16%8)==7&&s>=14?-7:0);bass(nm,0.9f*jv*(s==0||s==8?1.08f:1.f));midiHit(nm,1,110,i,stepLen*sr*(2.f+fGlide*2.f));}
                // syncopatie: odden stappen een octaaf-akkoord op dichte stijlen
                if(outro&&bar>=6&&s==0){int nm=deg2midi(root);bass(nm,0.8f);midiHit(nm,1,100,i,stepLen*sr*3.f);} // v130 outro-tag
                if(gBass && s%2==1 && celMask_[7][s]&&celMute_[7] && (st%3==0||st%5==0) && frand(fRsD)<fDens){int nm=deg2midi((mutBas_[b8]<0?0:mutBas_[b8])+root)+12;bass(nm,0.42f*jv);midiHit(nm,1,60,i,stepLen*sr*0.9f);}
                if(s==8&&celMask_[8][s]&&celMute_[8]&&(((int)fStijl==4||(int)fStijl==23||(int)fStijl==20||(int)fStijl==24)||chorus)){chord(jv);for(int j=0;j<4;j++)midiHit(chordDeg(j,(lastBD_>=0?lastBD_:bar)%4,root)+12,2,50,i,stepLen*sr*1.5f);} // 2e stoot (voice-led)
                if((s==4||s==12)&&((int)fStijl==4||(int)fStijl==23||(int)fStijl==24)&&frand(fRsD)<fDens){ // v33 arp-stoot
                    int j2=(s==4?1:2); midiHit(chordDeg(j2,bar%4,root)+24,2,45,i,stepLen*sr*0.8f);
                    {CV2&c2=cv2_; c2.on=true;c2.t=0;c2.f=m2f(chordDeg(j2,bar%4,root)+24);}}
                if(s==0&&(songB==24||songB==48)){dv(3,3,70.f,38.f,1.1f,.85f);fDuck=0.f;} // v139 drop-impact
                if(s==0&&(songB==24||songB==32||songB==48||songB==56)){dv(2,3,0.f,0.f,hdec*6.f,.30f);midiHit(49,0,80,i,stepLen*sr*2.f);} // v122 sectie-crash
                if(celMask_[8][s]&&celMute_[8]){chord(jv);for(int j=0;j<4;j++)midiHit(chordDeg(j,(lastBD_>=0?lastBD_:bar)%4,root)+(((int)fStijl==1||(int)fStijl==5||(int)fStijl==18||(int)fStijl==6)?0:12),2,60,i,stepLen*sr*((st==4||st==23||st==20)?1.4f:3.f));} // voice-led + stijl-release + gebruikers-stoten
                if(s%4==2&&bar>4&&!intro&&frand(fRs)<.28f&&frand(fRsD)<fDens){melody();midiHit(lastMel_,3,45,i,stepLen*sr*.8f);} // v36 call-response
                if(celMask_[9][s]&&celMute_[9]&&bar!=3){ // v47 hook: per 4 bars herhaalt de melodie zich — gebruiker tekent de stappen
                    int hb=bar%4;
                    if(melDegU_[s]>=0){int nm=deg2midi(melDegU_[s]+root_)+12;mv_.f=m2f(lastMel_=nm);mv_.on=true;mv_.t=0;mvPick_(nm,jv);midiHit(nm,3,80,i,stepLen*sr*1.5f);} // jouw getekende noot
                    else if(bar<4&&!intro){melody();hookM_[hb][s/4]=lastMel_;}
                    else if(hookM_[hb][s/4]>0&&frand(fRs)<.8f){int hn=hookM_[hb][s/4];
                    if(frand(fRs)<.15f){const char* hs=fScale<0.5f?kModes[(int)kStijlMode[(int)fStijl]]:(fScale<1.5f?kScaleM:kScaleJ);int anch=hookM_[hb][0]>0?hookM_[hb][0]:hn;int inv=2*anch-hn;int bn=hn,bd=99;for(int o=-1;o<=1;o++)for(int k=0;k<7;k++){int c=(int)fKey+lift_+hs[k]+o*12;if(std::abs(c-inv)<bd){bd=std::abs(c-inv);bn=c;}}hn=bn;} // motief-inversie
                    mv_.f=m2f(lastMel_=hn+(frand(fRs)<.2f?12:0)+(outro?-12:0));mv_.on=true;mv_.t=0;mvPick_(lastMel_,jv);
                        if(!(st==4||st==23||st==20||st==21)){cv2_.on=true;cv2_.t=0;cv2_.f=m2f(lastMel_-5);
                        if(fGeluid>=0.5f){const HarpSmp* ps;pickHarp(ps,lastMel_-5);cv2_.smp=jv>=0.98f?ps->d2:ps->d;cv2_.slen=jv>=0.98f?ps->len2:ps->len;cv2_.sstep=(cv2_.f/m2f(ps->midi))*(22050.f/getSampleRate());cv2_.spos=0;}}} // v116 duet-stem
                        midiHit(lastMel_,3,70,i,stepLen*sr*1.5f);}
                char sb[8];std::snprintf(sb,sizeof(sb),"%d",s);updateStateValue("step",sb);
                fStepLen=stepLen*(s%2==0?1.f+sw:1.f-sw); // swing-gap
            }}
            fDuck+=1.f/sr;
            const int stD=(int)fStijl; const float duckAmt=(stD==4||stD==23||stD==20)?0.44f:((stD==6||stD==13||stD==2)?0.16f:0.32f);
            float duck=1.f-duckAmt*expf(-fDuck*5.5f);
            fDrumsS+=(fDrums-fDrumsS)/sr*40.f; fBasS+=(fBas-fBasS)/sr*40.f;
            fChordS+=(fChord-fChordS)/sr*40.f; fMelS+=(fMel-fMelS)/sr*40.f;
            if(!fVerbOn){fVerb.init(sr);fVerbOn=true;}
            float wide=(chordRender(sr)+arpRender(sr)+melRender(sr))*duck;
            wide=fVerb.tick(wide,0.04f+fRoom*0.55f); // ROOM: zaal op de echte instrumenten, drums/808 droog
            float oC=drumRender()+basRender(sr)*(0.80f+0.20f*duck);
            float mixx=oC+wide;
            const int WD=530; float wd=wideB_[wideW_%WD]; wideB_[wideW_%WD]=wide*0.55f; wideW_++;
            float Lo=oC+wide*0.62f, Ro=oC+wide*0.30f+wd*0.62f;
            out[0][i]=tanhf(Lo*1.4f)*0.8f; out[1][i]=tanhf(Ro*1.4f)*0.8f; // v146 stereo-breedte
        }
        for(uint32_t i=0;i<frames;i++){
            float L=out[0][i],R=out[1][i];
            galaxy.tick(L,R,getSampleRate());
            out[0][i]=L; out[1][i]=R;
        }
        fAbsFrames+=frames;
    }

private:
    // MIDI-uitsturing: gegenereerde hits als MIDI naar de DAW (sleep/routeerbaar)
    struct PendingOff{uint8_t note,ch;uint32_t at;};
    PendingOff offq_[64]; int offw_=0,offr_=0;
    char mut_[3][17]; int mutBas_[8]; char celMask_[10][16]; char celMute_[10]; int extraSnd_[2]; int melDegU_[16]; SongRackView rackView_{}; struct PendHit{int t,v;float f0,f1,dec,amp;uint32_t at;} ph_[24]; int phn_=0; int mtLane_[3]={0,0,0}; int pendSt_=-1; int root_=0; int lastBD_=-1; int respN_=-1; uint32_t respAt_=0xFFFFFFFFu;
    void seedMask_(){ // raster = engine-patroon als startpunt; gebruiker klikt cellen aan/uit
        for(int s=0;s<16;s++){celMask_[0][s]=mut_[0][s]!='.'?1:0;celMask_[1][s]=mut_[1][s]!='.'?1:0;celMask_[2][s]=mut_[2][s]!='.'?1:0;
            celMask_[3][s]=(s%8==4)?1:0;celMask_[4][s]=(s==6||s==14)?1:0;celMask_[5][s]=0;celMask_[6][s]=0; // CLAP backbeat, PERC 6&14, +X1/+X2 leeg
            celMask_[7][s]=mutBas_[s/2]>=0?1:0;celMask_[8][s]=(s==0||s==8)?1:0; celMask_[9][s]=(s%4==0)?1:0;melDegU_[s]=-1;} pushPats();pushLanes();}
    void laneDice(int r){ // dobbel één laag opnieuw — toonsoort/grondtoon blijft, dus past altijd
        if(r<0||r>9)return;
        if(r<3){for(int b=0;b<16;b++)mut_[r][b]=(frand(fRs)<(r==0?0.30f:(r==1?0.20f:0.45f))||(r==0&&(b==0||b==8)))?'x':'.';}
        else if(r==3){for(int s=0;s<16;s++)celMask_[3][s]=(frand(fRs)<0.16f||(s%8==4))?1:0;}
        else if(r==4){for(int s=0;s<16;s++)celMask_[4][s]=(frand(fRs)<0.22f||(s==6||s==14))?1:0;}
        else if(r==5||r==6){for(int s=0;s<16;s++)celMask_[r][s]=(frand(fRs)<0.30f)?1:0;if(extraSnd_[r-5]==0)extraSnd_[r-5]=1;pushLanes();}
        else if(r==7){for(int b=0;b<8;b++)mutBas_[b]=(frand(fRs)<0.45f)?(int)(frand(fRs)*6.99f):-1;if(mutBas_[0]<0)mutBas_[0]=0;}
        else if(r==8){for(int s=0;s<16;s++)celMask_[8][s]=(frand(fRs)<0.20f||(s==0))?1:0;}
        else {for(int s=0;s<16;s++){celMask_[9][s]=(frand(fRs)<0.30f||(s%4==0&&frand(fRs)<0.7f))?1:0;melDegU_[s]=-1;}}
        if(r<3||r==7)seedMask_(); // her-sync rijen die uit engine-patronen komen
        else pushPats();}
    void syncRack_(){ // spiegel rack-status voor de UI (standalone levert geen state terug)
        for(int r=0;r<10;r++){for(int s=0;s<16;s++)rackView_.mask[r][s]=celMask_[r][s];rackView_.mute[r]=celMute_[r];}
        rackView_.extra[0]=extraSnd_[0];rackView_.extra[1]=extraSnd_[1];
        for(int s=0;s<16;s++)rackView_.melDeg[s]=(signed char)melDegU_[s];
        for(int b=0;b<8;b++)rackView_.basDeg[b]=(signed char)mutBas_[b];
        for(int i=0;i<27;i++)rackView_.prm[i]=getParameterValue(i);}
    void laneRestore(int r){ // SHIFT+dice: laag terug naar stijl-origineel
        if(r<0||r>9)return;
        if(r<3){for(int s=0;s<16;s++)celMask_[r][s]=mut_[r][s]!='.'?1:0;}
        else if(r==3){for(int s=0;s<16;s++)celMask_[3][s]=(s%8==4)?1:0;}
        else if(r==4){for(int s=0;s<16;s++)celMask_[4][s]=(s==6||s==14)?1:0;}
        else if(r==5||r==6){for(int s=0;s<16;s++)celMask_[r][s]=0;}
        else if(r==7){for(int s=0;s<16;s++)celMask_[7][s]=mutBas_[s/2]>=0?1:0;}
        else if(r==8){for(int s=0;s<16;s++)celMask_[8][s]=(s==0||s==8)?1:0;}
        else {for(int s=0;s<16;s++){celMask_[9][s]=(s%4==0)?1:0;melDegU_[s]=-1;}}
        pushPats();}
    void pushPats(){ // raster → UI
        syncRack_();
        char b[17]; b[16]=0; static const char* k[10]={"pat0","pat1","pat2","pat3","pat4","pat5","pat6","pat7","pat8","pat9"};
        for(int r=0;r<10;r++){for(int s=0;s<16;s++)b[s]=celMask_[r][s]?'x':'.';updateStateValue(k[r],b);}}
    void pushLanes(){ // mutes + +X-geluiden → UI: 10 mute-chars + 2 geluid-cijfers
        syncRack_();
        char b[13]; for(int r=0;r<10;r++)b[r]=celMute_[r]?'1':'0';
        b[10]='0'+extraSnd_[0]; b[11]='0'+extraSnd_[1]; b[12]=0; updateStateValue("lanes",b);}
    double fStepLen=0; int lastMel_=60;
    void emitMidi(uint8_t st,uint8_t nt,uint8_t vel,uint32_t fr){
        MidiEvent e{}; e.frame=fr; e.size=3;
        e.data[0]=st; e.data[1]=nt; e.data[2]=vel;
        writeMidiEvent(e);
    }
    void midiHit(int note,int ch,int vel,uint32_t i,double dur){
        // neem op voor .mid-export (altijd, ook met MIDI OUT uit)
        MidiRec& r=rec_[recW_];
        r.fr=fAbsFrames+i; r.note=(uint8_t)note; r.ch=(uint8_t)ch; r.vel=(uint8_t)vel;
        r.dur=(uint16_t)(dur>60000.0?60000.0:(dur<1.0?1.0:dur));
        recW_=(recW_+1)%8192; recN_++;
        if(fMidi<0.5f)return;
        emitMidi(0x90|ch,(uint8_t)note,(uint8_t)vel,i);
        PendingOff& o=offq_[offw_]; o.note=(uint8_t)note; o.ch=(uint8_t)ch; o.at=i+(uint32_t)dur;
        offw_=(offw_+1)%64;
    }
    // ---- 1-TOETS modes: één toets = akkoord/beat/walk/run/arp ----
    float fEen=0,fSugg=0; int walkI_=0; bool arpOn_=false; uint32_t arpAt_=0; int arpI_=0;
    struct RunQ{int n,v;uint32_t at;} runQ_[12]; int runN_=0;
    static const char kSuggNext[8];
    void eenKey(int nt,int vel,uint32_t i){
        const int st=(int)fStijl;
        const char* sc=fScale<0.5f?kModes[kStijlMode[st<0?0:(st>26?26:st)]]:(fScale<1.5f?kScaleM:kScaleJ);
        int rel=nt-36-(int)fKey; rel=((rel%12)+12)%12;
        int deg=0,bd=99; for(int d=0;d<7;d++){int dd=std::abs(rel-sc[d]);if(dd<bd){bd=dd;deg=d;}}
        const int mode=(int)fEen;
        if(mode==1){ // SMART: toets → diatonisch akkoord van die trap
            root_=deg; chord(vel/127.f);
            for(int j=0;j<4;j++){int cn=chordDeg(j,0,deg)+12;midiHit(cn,2,vel,i,(double)(fStepLen*getSampleRate()*2.));}
            fSugg=kSuggNext[deg]+1;
        }
        else if(mode==2){ // BEAT: 1 toets = groove start in die toonsoort
            fKey=nt%12; fAuto=1.f; fStep=0;
        }
        else if(mode==3){ // WALK: elke toets = volgende akkoord van de progressie
            root_=kProg[st<0?0:(st>26?26:st)][walkI_%4]; chord(vel/127.f);
            for(int j=0;j<4;j++){int cn=chordDeg(j,walkI_%4,root_)+12;midiHit(cn,2,vel,i,(double)(fStepLen*getSampleRate()*2.));}
            fSugg=(walkI_+1)%4+1; walkI_++;
        }
        else if(mode==4){ // RUN: snelle run naar je toets in de toonsoort
            int start=lastMel_,goal=36+(int)fKey+lift_+sc[deg]+((nt-36)/12)*12;
            if(goal<start)goal+=12; runN_=0;
            uint32_t gap=(uint32_t)(fStepLen*getSampleRate()/4); if(gap<800)gap=800;
            for(int k=0;k<8&&runN_<12;k++){
                int n=start+(int)((float)(goal-start)*(k+1)/8.f);
                int nr=n-36-(int)fKey-lift_; int dd=99,dd2=0;
                for(int d=0;d<7;d++){int rr=((n%12+12)%12);rr=((rr-(int)fKey%12+12)%12);int dif=std::abs(rr-sc[d]);if(dif<dd){dd=dif;dd2=d;}}
                int nn=36+(int)fKey+lift_+sc[dd2]+(n/12-3)*12;
                runQ_[runN_++]={nn,(int)(vel*.8f),i+gap*(k+1)};
            }
        }
        else if(mode==5){ // ARP: arpeggio van het akkoord blijft lopen tot je loslaat
            arpOn_=true;arpI_=0;arpAt_=i;root_=deg;
        }
    }
    // ---- EXPORT .MID: opgenomen hits als echt midi-bestand wegschrijven ----
    struct MidiRec{uint64_t fr;uint8_t note,ch,vel;uint16_t dur;};
    MidiRec rec_[8192]; int recW_=0; uint64_t recN_=0, fAbsFrames=0; double fBpm=100.0;
    void writeMid_(){
        if(recN_==0){updateStateValue("midifile","nog niets opgenomen — druk eerst SPEEL/laat het patroon lopen");return;}
        const double sr=getSampleRate();
        const double fpt=sr*60.0/(fBpm*480.0); // frames per tick @480 tpq
        smf::MidiFile mf; mf.setTicksPerQuarterNote(480);
        const uint64_t n=recN_<8192?recN_:8192;
        const int start=(recN_<=8192)?0:recW_;
        int seq=0;
        for(uint64_t k=0;k<n;k++){
            const MidiRec& r=rec_[(start+(int)k)%8192];
            if(r.note==0&&r.vel==0&&r.fr==0)continue;
            smf::MidiEvent on(0x90|r.ch,r.note,r.vel); on.tick=(int)(r.fr/fpt); on.seq=seq++;
            smf::MidiEvent off(0x80|r.ch,r.note,0); off.tick=(int)((r.fr+r.dur)/fpt); off.seq=seq++;
            mf[0].push_back(on); mf[0].push_back(off);
        }
        mf.sortTracks();
        const char* home=std::getenv("USERPROFILE");
        if(!home||!*home)home=std::getenv("HOME");
        char dir[520],path[600];
        if(home&&*home)std::snprintf(dir,sizeof(dir),"%s/Music/G2B",home);
        else std::snprintf(dir,sizeof(dir),".");
#ifdef _WIN32
        _mkdir(dir);
#else
        {std::string m=std::string(home?home:".")+"/Music";mkdir(m.c_str(),0755);}
        mkdir(dir,0755);
#endif
        std::snprintf(path,sizeof(path),"%s/G2B-SONG.mid",dir);
        if(mf.write(path))updateStateValue("midifile",".mid opgeslagen: Music/G2B/G2B-SONG.mid");
        else updateStateValue("midifile",".mid NIET gelukt — map niet schrijfbaar");
    }

    static int clampi(float v,int a,int b){int x=(int)(v+0.5f);return x<a?a:(x>b?b:x);}
    float frand(uint32_t& s){s=s*1664525u+1013904223u;return (s>>9)*(1.f/8388608.f);}
    static float m2f(int m){return 440.f*powf(2.f,(m-69)/12.f);}
    static float smpAt(const int16_t* d,uint32_t l,float& p,float st){
        if(!d||p>=l-1.f)return 0;uint32_t i=(uint32_t)p;float fr=p-i;
        float o=d[i]*(1.f-fr)+d[i+1]*fr; p+=st; return o*(1.f/32768.f);}
    static void pickStein(const PianoSmp*& ps,int note){int bi=0,bd=99;for(int k=0;k<8;k++){int d=std::abs(kPiano[k].midi-note);if(d<bd){bd=d;bi=k;}}ps=&kPiano[bi];}
    static void pickMar(const MarSmp*& ps,int note){int bi=0,bd=99;for(int k=0;k<10;k++){int d=std::abs(kMar[k].midi-note);if(d<bd){bd=d;bi=k;}}ps=&kMar[bi];}
    static void pickHarp(const HarpSmp*& ps,int note){int bi=0,bd=99;for(int k=0;k<8;k++){int d=std::abs(kHarp[k].midi-note);if(d<bd){bd=d;bi=k;}}ps=&kHarp[bi];}

    int deg2midi(int deg){
        const char* sc=fScale<0.5f?kModes[(int)kStijlMode[(int)fStijl]]:(fScale<1.5f?kScaleM:kScaleJ);
        int d=deg%7; if(d<0)d+=7;
        return 36+(int)fKey+lift_+sc[d]+12*(deg/7);
    }

    // ---- drums ----
    struct DV{bool on;int type;float t,f0,f1,dec,amp;const int16_t* smp;uint32_t slen;float spos;DV():on(false),type(0),t(0),f0(0),f1(0),dec(.1f),amp(.5f),smp(nullptr),slen(0),spos(0){}};
    DV dv_[4];
    void dv(int i,int ty,float f0,float f1,float dec,float amp){DV&v=dv_[i];v.on=true;v.type=(ty==4&&fGeluid<0.5f)?2:ty;v.t=0;v.f0=f0;v.f1=f1;v.dec=dec;v.amp=amp;v.smp=nullptr;
        if(fGeluid>=0.5f){ // echte drums (VCSL)
            int di;
            if(ty==0)di=0; else if(ty==1)di=1; else if(ty==3)di=29; else if(ty==4)di=kGPerc[(int)fStijl<0?0:((int)fStijl>26?26:(int)fStijl)];
            else if(f0>60.f)di=(f0>100.f?21:23);
            else di=(dec>0.15f?3:2);
            static int drr=0; const int alt=kDrumAlt[di]; if(alt>=0&&((drr++)&1))di=alt;
            v.smp=kDrum[di].d;v.slen=kDrum[di].len;v.spos=0;
        }}
    void kick(){dv(0,0,150.f,48.f,.28f,.9f);}
    void snare(){dv(1,1,190.f,170.f,.16f,.5f);}
    void hat(){dv(2,2,0.f,0.f,.05f,.3f);}
    float drumRender(){
        float o=0;
        for(auto&v:dv_){if(!v.on)continue;v.t+=1.f/getSampleRate();
            float e=expf(-v.t/v.dec*5.f),s=0;
            if(v.smp){
                uint32_t i2=(uint32_t)v.spos;
                if(i2>=v.slen-1){v.on=false;continue;}
                float fr=v.spos-i2;
                s=(v.smp[i2]+fr*(v.smp[i2+1]-v.smp[i2]))/32768.f*2.4f;
                v.spos+=22050.f/getSampleRate();
                o+=s*v.amp;
            }else{
            if(v.type==0){float f=v.f1+(v.f0-v.f1)*expf(-v.t*30.f);s=sinf(2*M_PI*f*v.t)*e;}
            else if(v.type==1){s=(frand(fNs)*2-1)*e*0.7f+sinf(2*M_PI*v.f0*v.t)*e*0.4f;}
            else{s=(frand(fNs)*2-1)*e*0.8f;}
            o+=s*v.amp; if(e<0.001)v.on=false;}}
        return o*fDrumsS;
    }

    // ---- GLIJDENDE 808 ----
    // f = huidige pitch, fT = doel-pitch; legato → glijdt, verse noot → pitch-drop
    struct BV{bool on,rel;float t,f,fT,vel,ph,relT;int note;const int16_t* smp;uint32_t slen;float spos,sstep;
              BV():on(false),rel(false),t(0),f(55),fT(55),vel(.8f),ph(0),relT(0),note(-1),smp(nullptr),slen(0),spos(0),sstep(0){}};
    BV bv_;
    void bass(int m,float vel){
        float fT=m2f(m); galaxy.keyMul=powf(2.f,log2f(fT/220.f)*0.5f); // shimmer volgt de 808
        
        if(!bv_.on || fGlide<0.01f){
            // verse noot: start boven doel (klassieke 808 pitch-drop)
            bv_.f=fT*(1.f+fDrop*0.6f);
            bv_.on=true;bv_.rel=false;bv_.t=0;bv_.vel=vel;
        }else{
            // legato: glijd van huidige pitch naar doel — dé drill-slide
            bv_.on=true;bv_.rel=false;bv_.vel=vel;
            if(fMidi>0.5f&&bv_.note>0){ // MIDI pitch-bend voor de slide in de DAW
                int st=(int)roundf(12.f*log2f(m2f(m)/m2f(bv_.note)));
                if(st<-2)st=-2; if(st>2)st=2;
                int bend=8192+(int)(st*4096.f/2.f);
                MidiEvent e{}; e.frame=0; e.size=3;
                e.data[0]=0xE0|1; e.data[1]=(uint8_t)(bend&0x7F); e.data[2]=(uint8_t)(bend>>7);
                writeMidiEvent(e);
            }
        }
        bv_.fT=fT;bv_.note=m;
        bv_.smp=nullptr;
        const int sti=(int)fStijl;
        if(fGeluid>=0.5f&&(sti==7||sti==8||sti==9||sti==11)){ // CONTRABAS-pizz: jazz-walk bas ipv 808
            int bi=0,bd=99; for(int k=0;k<kCbCount;k++){int d=std::abs(kCb[k].midi-m);if(d<bd){bd=d;bi=k;}}
            const CbSmp&pb=kCb[bi];bv_.smp=pb.d;bv_.slen=pb.len;bv_.spos=0;
            bv_.sstep=(fT/m2f(pb.midi))*(22050.f/getSampleRate());
        }
    }
    void bassMidi(int m,float vel){bass(m,0.55f+0.45f*vel);} // keyboard: velocity mee
    void bassOff(int m){if(bv_.on && bv_.note==m){bv_.rel=true;bv_.relT=bv_.t;}}
    float basRender(float sr){
        if(!bv_.on)return 0;
        if(bv_.smp){ // echte contrabas-pizz
            uint32_t i2=(uint32_t)bv_.spos;
            if(i2>=bv_.slen-1){bv_.on=false;return 0;}
            float fr=bv_.spos-i2;
            float sv=(bv_.smp[i2]+fr*(bv_.smp[i2+1]-bv_.smp[i2]))/32768.f;
            bv_.spos+=bv_.sstep;
            return sv*3.4f*bv_.vel*fBasS;
        }
        bv_.t+=1.f/sr;
        // pitch-slide: exponentieel naar doel, tempo uit GLIDE (20ms..400ms)
        float gsec=0.02f+fGlide*0.38f;
        bv_.f+= (bv_.fT-bv_.f)*(1.f-expf(-1.f/(sr*gsec)));
        bv_.ph+=bv_.f/sr; if(bv_.ph>1.f)bv_.ph-=1.f;
        // lange 808-uithaal + release bij note-off
        float e=expf(-bv_.t*2.6f);
        if(bv_.rel)e*=expf(-(bv_.t-bv_.relT)*14.f);
        float s=sinf(2.f*M_PI*bv_.ph);
        // SUB-blend: 0=puur sine → 1=tanh-drive (meer body/harmonischen)
        s=(1.f-fSub)*s + fSub*tanhf(s*(1.f+2.4f))*0.75f;
        if(fSub>0.5f)s+=sinf((float)M_PI*bv_.ph)*(fSub-0.5f)*0.8f; // DONDER: sub-octaaf eronder
        if(e<0.0015)bv_.on=false;
        return s*e*bv_.vel*fBasS*0.62f;
    }

    // ---- akkoorden ----
    struct CV{bool on;float t;float f[4];
              const int16_t* sm[4];uint32_t sl[4];float sp[4],ss[4];
              CV():on(false),t(0){f[0]=f[1]=f[2]=f[3]=0;sm[0]=sm[1]=sm[2]=sm[3]=nullptr;sl[0]=sl[1]=sl[2]=sl[3]=0;sp[0]=sp[1]=sp[2]=sp[3]=0;ss[0]=ss[1]=ss[2]=ss[3]=0;}};
    CV cv_; struct CV2{bool on=false;float t,f;const int16_t* smp=nullptr;uint32_t slen=0;float spos=0,sstep=0;} cv2_; int lastChord_[4]={0,0,0,0}; bool hasLast_=false;
    int chordDeg(int j,int vo,int root=0){ // voicings + progressie-root
        int d=j==0?0:(j==1?kThird[(int)fStijl]:(j==2?kSev[(int)fStijl]:14)); d+=root; // v167: kleur per stijl
        if(vo==1&&j==0)d+=12; else if(vo==2&&j==3)d+=12; else if(vo==3&&j==1)d+=1;
        return deg2midi(d);
    }
    void chord(float vel=1.0f){
        cv_.on=true;cv_.t=0;
        int vo=0,best=1<<30;
        for(int v=0;v<4;v++){int c=0;for(int j=0;j<4;j++){int d=chordDeg(j,v,root_);c+=hasLast_?std::abs(d-lastChord_[j]):0;}if(c<best){best=c;vo=v;}}
        for(int j=0;j<4;j++)lastChord_[j]=chordDeg(j,vo,root_); hasLast_=true; // echte voice-leading: kleinste beweging
        int co=((int)fStijl==1||(int)fStijl==5||(int)fStijl==18||(int)fStijl==6)?0:12; // v42 donker=-12
        for(int j=0;j<4;j++){int nt=chordDeg(j,vo,root_)+co;cv_.f[j]=m2f(nt);
            if(fGeluid>=0.5f){const PianoSmp* ps;pickStein(ps,nt);const bool hrd=vel>=0.98f;const float tv=vel*(0.55f+fTouch*0.9f); // TOUCH schuift de laag-keuze
                const int lv=tv<0.5f?0:(tv<0.75f?1:(tv<0.9f?2:3));
                cv_.sm[j]=lv==0?ps->d:(lv==1?ps->d2:(lv==2?ps->d3:ps->d4));
                cv_.sl[j]=lv==0?ps->len:(lv==1?ps->len2:(lv==2?ps->len3:ps->len4));
                float fs=m2f(ps->midi);cv_.ss[j]=(cv_.f[j]/fs)*(22050.f/getSampleRate());cv_.sp[j]=0;}}
    }
    float arpRender(float sr){
        if(!cv2_.on)return 0; cv2_.t+=1.f/sr; float e=expf(-cv2_.t*3.2f);
        float s;
        if(fGeluid>=0.5f&&cv2_.smp)s=smpAt(cv2_.smp,cv2_.slen,cv2_.spos,cv2_.sstep)*0.9f;
        else s=(sinf(2*M_PI*cv2_.f*cv2_.t)+.25f*sinf(2*M_PI*cv2_.f*3*cv2_.t)*e)*e;
        if(e<0.004)cv2_.on=false; return s*fChordS*.5f*e;
    }
    float chordRender(float sr){
        float dr=1.f+0.0023f*sinf(2*M_PI*fDrift+.7f);
        if(!cv_.on)return 0;
        cv_.t+=1.f/sr;
        float e=expf(-cv_.t*0.9f);
        float s=0;
        if(fGeluid>=0.5f&&cv_.sm[0]){
            for(int j=0;j<4;j++)s+=smpAt(cv_.sm[j],cv_.sl[j],cv_.sp[j],cv_.ss[j])*0.30f;
        }else{
            for(int j=0;j<4;j++)s+=sinf(2*M_PI*cv_.f[j]*dr*cv_.t)*0.22f+sinf(2*M_PI*cv_.f[j]*2*cv_.t)*e*0.06f;}
        if(e<0.004)cv_.on=false;
        return s*fChordS*e;
    }

    // ---- melodie ----
    struct MV{bool on;float t,f,amp;const int16_t* smp=nullptr;uint32_t slen=0;float spos=0,sstep=0;MV():on(false),t(0),f(440),amp(.5f){}};
    MV mv_,mv2_;int fMelDeg=0,lift_=0; int fOblique=0,fObBase=0; int hookM_[4][4]={{0}};
    void melody(){
        int bar32=(int)(fStep/16)%32;
        if(bar32>=24&&bar32<28){ // v82 brug: melodie improviseert op akkoord-tonen
            lastMel_=chordDeg((int)(frand(fRs)*3.99f),(lastBD_>=0?lastBD_:0)%4,root_)+24;
            mv_.f=m2f(lastMel_); mv_.on=true; mv_.t=0; return;
        }
        int mv=(int)(frand(fRs)*4)-1;
        fMelDeg+=mv; if(fMelDeg<0)fMelDeg+=7; if(fMelDeg>10)fMelDeg-=7;
        if(fOblique>0){int r=fMelDeg%3;if(r<0)r+=3;fMelDeg=fObBase+r;} // Oblique: 3-tonen hook
        int oct=(fMelDeg/7)*12 + (((int)fStijl==19||(int)fStijl==24||(int)fStijl==20||(int)fStijl==23)?12:0); if(((int)(fStep/16)%8)==4)oct-=12; int d=fMelDeg%7;
        const char* sc=fScale<0.5f?kModes[(int)kStijlMode[(int)fStijl]]:(fScale<1.5f?kScaleM:kScaleJ);
        d=(d+root_)%14; // v137: melodie beweegt mee met de progressie
        lastMel_=(int)fKey+lift_+sc[d%7]+oct+12+36+12*(d/7);
        mv_.f=m2f(lastMel_);
        mv_.on=true;mv_.t=0;mvPick_(lastMel_);
    }
    void mvPick_(int note,float vel=1.0f){
        if(fGeluid<0.5f){mv_.smp=nullptr;return;}
        const MarSmp* ps;pickMar(ps,note);const float tv=vel*(0.55f+fTouch*0.9f);const int lv=tv<0.55f?0:(tv<0.82f?1:2);
        mv_.smp=lv==0?ps->d:(lv==1?ps->d2:ps->d3);mv_.slen=lv==0?ps->len:(lv==1?ps->len2:ps->len3);
        mv_.sstep=(mv_.f/m2f(ps->midi))*(22050.f/getSampleRate());mv_.spos=0;
        if(fGeluid>=1.5f){int bn=0,bd=99;for(int k=0;k<kPsalCount;k++){int d=std::abs(kPsal[k].midi-(note+12));if(d<bd){bd=d;bn=k;}}
            mv2_.f=m2f(note+12);mv2_.smp=kPsal[bn].d;mv2_.slen=kPsal[bn].len;
            mv2_.sstep=(mv2_.f/m2f(kPsal[bn].midi))*(22050.f/getSampleRate());mv2_.spos=0;mv2_.on=true;}
        else mv2_.smp=nullptr;
    }
    float melRender(float sr){
        fDrift+=1.f/sr*.13f; float dr=1.f+0.0023f*sinf(2*M_PI*fDrift);
        if(!mv_.on)return 0;
        mv_.t+=1.f/sr;
        float e=expf(-mv_.t*kMelDec[(int)fStijl]);
        float s;
        if(fGeluid>=0.5f&&mv_.smp){s=smpAt(mv_.smp,mv_.slen,mv_.spos,mv_.sstep)*0.9f;
            if(fGeluid>=1.5f&&mv2_.smp&&mv2_.on)s+=smpAt(mv2_.smp,mv2_.slen,mv2_.spos,mv2_.sstep)*0.5f;} // STACK: psaltery-oct erop
        else s=(sinf(2*M_PI*mv_.f*dr*mv_.t)+kMelPart[(int)fStijl]*sinf(2*M_PI*mv_.f*2*mv_.t)*e)*e;
        if(e<0.003){mv_.on=false;mv2_.on=false;}
        return s*fMelS*0.5f*e;
    }

    void rollDice(){
        for(int m=0;m<4;m++){int a=(int)(frand(fRs)*2.99f),b=(int)(frand(fRs)*15.99f);
            if(!(a==0&&(b==0||b==8))) mut_[a][b]=(mut_[a][b]=='.')?'X':'.';}
        for(int b=0;b<6;b++)frand(fRs);
        setParameterValue(kStijl,(int)(frand(fRs)*27));
        setParameterValue(kDrums,0.6f+frand(fRs)*0.4f);
        setParameterValue(kBas,0.5f+frand(fRs)*0.5f);
        setParameterValue(kChord,0.4f+frand(fRs)*0.6f);
        setParameterValue(kMel,0.4f+frand(fRs)*0.6f);
        setParameterValue(kGlide,0.2f+frand(fRs)*0.5f);
        fMelDeg=(int)(frand(fRs)*7);
        fOblique=(frand(fRs)<0.3f)?3:0; fObBase=(int)(frand(fRs)*6);
    }
    void applyPreset(int i){
        i=clampi(i,0,26);
        setParameterValue(kStijl,i);
        setParameterValue(kDrums,0.8f); setParameterValue(kBas,0.7f);
        setParameterValue(kChord,0.6f); setParameterValue(kMel,0.6f);
        setParameterValue(kGlide,i==2?0.55f:0.35f); // DRILL = meer slide
    }

    float fStijl=0,fKey=9,fScale=0,fDrums=.8f,fBas=.7f,fChord=.6f,fMel=.6f;
    float fGlide=.35f,fDrop=.55f,fSub=.35f;
    int fMutStyle=-1;
    float fMorphF=0; float kitKf0=150,kitKf1=48,kitKdec=.28,kitKamp=.9,kitSf0=190,kitSf1=170,kitSdec=.16,kitSamp=.5,kitHdec=.05,kitHamp=.3; float fDrift=0; float roomB_[4000]; uint32_t roomW_=0; float fDrumsS=1,fBasS=1,fChordS=1,fMelS=1; float fDice=0,fAuto=0,fDuck=9,fLight=0,fPreset=0,fGalaxy=9,fMidi=1,fRun=1.f; float wideB_[640]; uint32_t wideW_=0;
    Galaxy galaxy;
    double fPhase=1.0; uint64_t fStep=0; bool wasPlaying_=false;
    uint32_t fNs=1234,fRs=777,fRsD=24601; float fDens=0.7f; float fSwing=1.f; float fHuman=1.f; float fFill=1.f; float fGeluid=1.f; float fRoom=.25f; float fTouch=.5f; G2bVerb fVerb; bool fVerbOn=false;
};

const char SongPlugin::kSuggNext[8]={4,5,4,5,1,4,1,1}; // trap→vervolgstap (I→V, II→V, V→I)

Plugin* createPlugin(){ return new SongPlugin(); }
END_NAMESPACE_DISTRHO
