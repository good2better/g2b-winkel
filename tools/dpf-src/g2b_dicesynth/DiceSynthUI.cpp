/*
 * G2B Dice-Synth — UI (NanoVG).
 * Grote DICE + MUTATE + undo/redo, 5 modi, slotjes per sectie,
 * 12 macro-knoppen (sleep = draaien), CHORD-toggle, naam-display.
 */

#include "DistrhoUI.hpp"
#include "dicesynth_roll.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cctype>

START_NAMESPACE_DISTRHO

static const float BG[3]={11,14,12},PN[3]={20,26,22},TX[3]={228,236,230};
static const float GR[3]={47,191,113},DG[3]={24,110,64},YE[3]={232,196,80},RD[3]={214,72,72};
static inline Color C(const float*c){return Color((int)c[0],(int)c[1],(int)c[2]);}

static const char* kModeNames[5]={"KEYS","PADS","TEXTURE","BASS","HIT"};

// knob slots: param index + label + group(0 osc,1 env,2 filt,3 fx,-1 geen)
struct Knob { int p; const char* lbl; int grp; };
static const Knob kKnobs[12] = {
    {kOsc1W,"GOLF1",0},{kOsc2W,"GOLF2",0},{kO2Det,"DETUNE",0},{kNoise,"NOISE",0},
    {kAtk,"AANVAL",1},{kDec,"VAL",1},{kSus,"HOUD",1},{kRel,"LOS",1},
    {kCut,"KLEUR",2},{kLfoD,"BEWEGING",2},{kRev,"RUIMTE",3},{kDrive,"GLOEI",3}
};
// extra kleine regelaars onder: kDly, kWarm, kGain als rij 2? -> 12 knoppen + 3 = netjes:
// we voegen DELAY/WARM/VOL toe als tweede rij via kKnobs2
static const Knob kKnobs2[5] = {{kDly,"DELAY",3},{kWarm,"WARM",3},{kGain,"VOL",-1},{kStrum,"STRUM",-1},{kHuman,"HUMAN",-1}};

class DiceSynthUI : public UI {
public:
    DiceSynthUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT) {
        loadSharedResources();
        std::memset(fVal,0,sizeof fVal);
        fVal[kOn]=1.f;fVal[kMode]=0.25f;fVal[kOsc1W]=0.3f;fVal[kOsc2W]=0.6f;fVal[kO2Oct]=0.5f;
        fVal[kO2Det]=0.6f;fVal[kO2Lvl]=0.7f;fVal[kNoise]=0.05f;fVal[kPEnv]=0.5f;
        fVal[kAtk]=0.45f;fVal[kDec]=0.5f;fVal[kSus]=0.85f;fVal[kRel]=0.5f;
        fVal[kCut]=0.4f;fVal[kRes]=0.3f;fVal[kFEnv]=0.4f;fVal[kLfoR]=0.15f;fVal[kLfoD]=0.4f;
        fVal[kDrive]=0.15f;fVal[kRev]=0.5f;fVal[kDly]=0.25f;fVal[kDlyDiv]=0.4f;fVal[kDlyFb]=0.4f;
        fVal[kWarm]=0.5f;fVal[kGain]=0.8f;
        std::strcpy(fName,"G2B START");
    }

protected:
    bool onKeyboard(const KeyboardEvent& ev) override {
        if(!ev.press)return true;
        static const char kKeys[]="awsedftgyhujkolp;";
        const char* hit=std::strchr(kKeys,(char)std::tolower((int)ev.key));
        if(hit){sendNote(0,(uint8_t)(48+(int)(hit-kKeys)),100);repaint();return true;}
        return false;
    }

    void parameterChanged(uint32_t index,float value) override {
        if(index<kParamCount)fVal[index]=value; repaint();
    }
    void stateChanged(const char* key,const char* value) override {
        if(!std::strcmp(key,"diceName")){std::strncpy(fName,value,63);fName[63]=0;repaint();}
    }

    bool onMouse(const MouseEvent& ev) override {
        const float x=ev.pos.getX(),y=ev.pos.getY();
        if(ev.press){
            fLastY=y;
            int hit=hitControl(x,y);
            if(hit>=0&&hit<100){ // knop (0..14) of toggle (>=20)
                if(hit<20){fDrag=hit;}
                else toggleControl(hit);
                repaint(); return true;
            }
            if(hit>=100){ // actie-knoppen
                if(hit==104){spv(kChord,fVal[kChord]>0.5f?0.f:1.f);}
                else if(hit==105){spv(kOn,fVal[kOn]>0.5f?0.f:1.f);}
                else if(hit==100)doRoll(false);
                else if(hit==101)doRoll(true);
                else if(hit==102)histNav(-1);
                else if(hit==103)histNav(+1);
                repaint(); return true;
            }
            fDrag=-1; return true;
        }else{fDrag=-1;return true;}
    }
    bool onMotion(const MotionEvent& ev) override {
        if(fDrag<0)return true;
        const int idx=fDrag;
        int p=(idx<12)?kKnobs[idx].p:kKnobs2[idx-12].p;
        float v=fVal[p]-(ev.pos.getY()-fLastY)*0.006f;
        if(v<0)v=0;if(v>1)v=1;
        fLastY=ev.pos.getY();
        spv(p,v); repaint(); return true;
    }
    bool onScroll(const ScrollEvent& ev) override {
        int hit=hitControl(ev.pos.getX(),ev.pos.getY());
        if(hit<0||hit>=20)return true;
        int p=(hit<12)?kKnobs[hit].p:kKnobs2[hit-12].p;
        float v=fVal[p]+ev.delta.getY()*0.04f;
        if(v<0)v=0;if(v>1)v=1;
        spv(p,v); repaint(); return true;
    }

    void onNanoDisplay() override {
        const float W=getWidth(),H=getHeight(); const float sc=W/720.f;
        beginPath();rect(0,0,W,H);fillColor(C(BG));fill();
        // titel + naam
        fontSize(15*sc);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);fillColor(C(TX));
        text(18*sc,22*sc,"G2B DICE-SYNTH",nullptr);
        fontSize(20*sc);fillColor(C(YE));
        text(18*sc,46*sc,fName,nullptr);
        fontSize(10*sc);fillColor(Color(120,140,125));
        text(18*sc,64*sc,"rol oneindig nieuwe sounds — 100% sync-veilig",nullptr);
        // modi
        const int mode=(int)(fVal[kMode]*4.f+0.5f);
        for(int i=0;i<5;i++){
            const float bx=200*sc+i*102*sc,by=14*sc,bw=94*sc,bh=26*sc;
            beginPath();roundedRect(bx,by,bw,bh,5*sc);
            fillColor(i==mode?C(GR):C(PN));fill();
            fontSize(11*sc);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(i==mode?C(BG):C(TX));
            text(bx+bw/2,by+bh/2,kModeNames[i],nullptr);
        }
        // DICE knop
        const float dx=560*sc,dy=52*sc,dw=140*sc,dh=52*sc;
        beginPath();roundedRect(dx,dy,dw,dh,9*sc);fillColor(C(GR));fill();
        fontSize(19*sc);fillColor(C(BG));text(dx+dw/2,dy+dh/2,"◆ DICE",nullptr);
        // MUTATE + undo/redo
        smallBtn(dx,dy+58*sc,dw,dh*0.55f,"MUTATE",C(PN),C(YE));
        smallBtn(500*sc,348*sc,100*sc,36*sc,"<< TERUG",C(PN),C(TX));
        smallBtn(608*sc,348*sc,92*sc,36*sc,"VOOR >>",C(PN),C(TX));
        // CHORD
        smallBtn(440*sc,52*sc,108*sc,26*sc,fVal[kChord]>0.5f?"CHORD: AAN":"CHORD: UIT",fVal[kChord]>0.5f?C(YE):C(PN),fVal[kChord]>0.5f?C(BG):C(TX));
        // ON
        smallBtn(440*sc,84*sc,108*sc,26*sc,fVal[kOn]>0.5f?"AAN":"UIT",fVal[kOn]>0.5f?C(GR):C(RD),C(BG));
        // lock-rij
        static const char* lockNames[4]={"SLOT GOLF","SLOT ENV","SLOT FILTER","SLOT FX"};
        static const int lockP[4]={kLockOsc,kLockEnv,kLockFilt,kLockFx};
        for(int i=0;i<4;i++){
            const bool lk=fVal[lockP[i]]>0.5f;
            smallBtn(200*sc+i*102*sc,52*sc,94*sc,20*sc,lockNames[i],lk?C(YE):C(PN),lk?C(BG):Color(140,150,142));
        }
        // knoppen hoofdrij (12)
        for(int i=0;i<12;i++){
            const int col=i%6,row=i/6;
            knobAt(52*sc+col*112*sc,150*sc+row*112*sc,42*sc,kKnobs[i]);
        }
        // onderste rij DELAY/WARM/VOL
        for(int i=0;i<5;i++)
            knobAt(52*sc+i*90*sc,374*sc,42*sc,kKnobs2[i]);
        // keyboard-hint
        fontSize(9*sc);textAlign(ALIGN_LEFT);fillColor(Color(90,105,95));
        text(18*sc,H-8*sc,"toetsen: a w s e d f t g ... = spelen",nullptr);
    }

private:
    float fVal[kParamCount]={}; int fDrag=-1; float fLastY=0; char fName[64];
    uint32_t fRnd=77777u;
    float fHist[24][kParamCount]; int fHistN=0,fHistPos=0,fView=0;

    void spv(uint32_t i,float v){setParameterValue(i,v);parameterChanged(i,v);}
    void pushHist(){ // huidige staat opslaan; redo-staart weggooien; p wijst naar nieuwste
        if(fHistPos<24){
            std::memcpy(fHist[fHistPos],fVal,sizeof fVal);fHistPos++;
        }else{
            std::memmove(fHist,fHist+1,sizeof fHist[0]*23);
            std::memcpy(fHist[23],fVal,sizeof fVal);
        }
        fHistN=fHistPos;
    }
    void histNav(int d){
        if(fHistN==0)return;
        if(d<0){if(fHistN<=1)return;fHistN--;}
        else{if(fHistN>=fHistPos)return;fHistN++;}
        for(int i=0;i<kParamCount;i++)spv(i,fHist[fHistN-1][i]);
    }
    void doRoll(bool mutate){
        if(fHistN>=fHistPos){pushHist();}      // pre-roll staat als laatste erbij
        else fHistPos=fHistN;                 // redo-staart weg
        g2bRollDice(fVal,fRnd,mutate);
        if(fHistPos<24){std::memcpy(fHist[fHistPos],fVal,sizeof fVal);fHistPos++;}
        else{std::memmove(fHist,fHist+1,sizeof fHist[0]*23);std::memcpy(fHist[23],fVal,sizeof fVal);}
        fHistN=fHistPos;
        for(int t=0;t<kRollCount;t++)spv(kRollTable[t].p,fVal[kRollTable[t].p]);
        const int mode=(int)(fVal[kMode]*4.f+0.5f);
        g2bDiceName(mode,fRnd,fName);
    }

    void smallBtn(float x,float y,float w,float h,const char*t,Color bg,Color fg){
        beginPath();roundedRect(x,y,w,h,4.f);fillColor(bg);fill();
        fontSize(10.f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);fillColor(fg);
        text(x+w/2,y+h/2,t,nullptr);
    }
    void knobAt(float cx,float cy,float r,const Knob&k){
        const float v=fVal[k.p];
        beginPath();circle(cx,cy,r);fillColor(C(PN));fill();
        beginPath();circle(cx,cy,r*0.82f);fillColor(C(BG));fill();
        // value-arc
        const float a0=-2.356f,a1=a0+v*4.712f;
        beginPath();arc(cx,cy,r*0.72f,a0,a1,CW);strokeColor(C(GR));strokeWidth(4);stroke();
        // pointer
        const float px=cx+cosf(a1)*r*0.62f,py=cy+sinf(a1)*r*0.62f;
        beginPath();moveTo(cx,cy);lineTo(px,py);strokeColor(C(TX));strokeWidth(2.5f);stroke();
        fontSize(9.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);fillColor(C(TX));
        text(cx,cy+r+9.f,k.lbl,nullptr);
        char vb[8];std::snprintf(vb,8,"%d",(int)(v*100));
        fontSize(9.f);fillColor(Color(130,150,135));text(cx,cy+r+21.f,vb,nullptr);
    }
    // knop-hitboxen matchen knobAt posities
    int hitControl(float x,float y){
        const float W=getWidth(),sc=W/720.f;
        for(int i=0;i<12;i++){const int col=i%6,row=i/6;
            float cx=52*sc+col*112*sc,cy=150*sc+row*112*sc;
            if(std::hypotf(x-cx,y-cy)<42*sc)return i;}
        for(int i=0;i<5;i++){float cx=52*sc+i*90*sc,cy=374*sc;
            if(std::hypotf(x-cx,y-cy)<42*sc)return 12+i;}
        // toggles 20..: modi/locks/chord/on
        for(int i=0;i<5;i++){float bx=200*sc+i*102*sc,by=14*sc;
            if(x>bx&&x<bx+94*sc&&y>by&&y<by+26*sc)return 20+i;}
        for(int i=0;i<4;i++){float bx=200*sc+i*102*sc,by=52*sc;
            if(x>bx&&x<bx+94*sc&&y>by&&y<by+20*sc)return 30+i;}
        // acties 100+:
        float dx=560*sc,dy=52*sc,dw=140*sc,dh=52*sc;
        if(x>dx&&x<dx+dw&&y>dy&&y<dy+dh)return 100;
        if(x>dx&&x<dx+dw&&y>dy+58*sc&&y<dy+58*sc+dh*0.55f)return 101;
        if(x>500*sc&&x<600*sc&&y>348*sc&&y<384*sc)return 102;
        if(x>608*sc&&x<700*sc&&y>348*sc&&y<384*sc)return 103;
        if(x>440*sc&&x<548*sc&&y>52*sc&&y<78*sc)return 104; // chord
        if(x>440*sc&&x<548*sc&&y>84*sc&&y<110*sc)return 105; // on
        return -1;
    }
    void toggleControl(int hit){
        if(hit>=20&&hit<25){spv(kMode,(hit-20)/4.f);}
        else if(hit>=30&&hit<34){const int p[4]={kLockOsc,kLockEnv,kLockFilt,kLockFx};
            const int pp=p[hit-30];spv(pp,fVal[pp]>0.5f?0.f:1.f);}
    }
};

UI* createUI(){return new DiceSynthUI();}
END_NAMESPACE_DISTRHO
