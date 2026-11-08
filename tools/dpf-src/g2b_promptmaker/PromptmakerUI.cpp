/*
 * G2B Prompt-Maker — UI (NanoVG).
 * Typ een geluid ("donkere techno kick") → MAAK rolt N kandidaten.
 * Per kandidaat: ▶ spelen/laden · ✓ houden · ✗ slecht · ⚗ morph · 🔁 opnieuw · ⬇ wav-export · ✕ weg.
 * Woorden-chips bouwen je prompt. Knoppen fine-tunen de geladen sound.
 * 100% eigen DSP — sync-veilig, geen servers.
 */

#include "DistrhoUI.hpp"
#include "dicesynth_roll.h"
#include "prompt_words.h"
#include "prompt_render.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <vector>
#include <sys/stat.h>
#include <cstdlib>

START_NAMESPACE_DISTRHO

static const float BG[3]={11,14,12},PN[3]={20,26,22},TX[3]={228,236,230};
static const float GR[3]={47,191,113},YE[3]={232,196,80},RD[3]={214,72,72};
static const float PU[3]={140,100,220};
static inline Color C(const float*c){return Color((int)c[0],(int)c[1],(int)c[2]);}

static const char* kModeNames[5]={"KEYS","PADS","TEXTURE","BASS","HIT"};

struct Knob { int p; const char* lbl; };
static const Knob kKnobs[8] = {
    {kCut,"KLEUR"},{kRes,"RES"},{kAtk,"AANVAL"},{kRel,"LOS"},
    {kRev,"RUIMTE"},{kDrive,"GLOEI"},{kWarm,"WARM"},{kGain,"VOL"}
};

struct Cand { float P[kParamCount]; char naam[72]; char prompt[80]; int state; int mode; };

class PromptmakerUI : public UI {
public:
    PromptmakerUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT) {
        loadSharedResources();
        std::memset(fVal,0,sizeof fVal);
        fVal[kOn]=1.f;fVal[kMode]=0.25f;fVal[kOsc1W]=0.3f;fVal[kOsc2W]=0.6f;fVal[kO2Oct]=0.5f;
        fVal[kO2Det]=0.6f;fVal[kO2Lvl]=0.7f;fVal[kNoise]=0.05f;fVal[kPEnv]=0.5f;
        fVal[kAtk]=0.45f;fVal[kDec]=0.5f;fVal[kSus]=0.85f;fVal[kRel]=0.5f;
        fVal[kCut]=0.4f;fVal[kRes]=0.3f;fVal[kFEnv]=0.4f;fVal[kLfoR]=0.15f;fVal[kLfoD]=0.4f;
        fVal[kDrive]=0.15f;fVal[kRev]=0.5f;fVal[kDly]=0.25f;fVal[kDlyDiv]=0.4f;fVal[kDlyFb]=0.4f;
        fVal[kWarm]=0.5f;fVal[kGain]=0.8f;
        std::strcpy(fStatus,"typ een geluid en druk op MAAK (of Enter)");
    }

protected:
    bool onKeyboard(const KeyboardEvent& ev) override {
        if(!ev.press)return true;
        const int k=(int)ev.key;
        if(k==13||k==10){doMaak();repaint();return true;}
        if(k==8||k==127){
            int n=(int)std::strlen(fPrompt); if(n>0)fPrompt[n-1]=0;
            repaint();return true;
        }
        if(k>=32&&k<127){
            int n=(int)std::strlen(fPrompt);
            if(n<70){fPrompt[n]=(char)k;fPrompt[n+1]=0;}
            repaint();return true;
        }
        return true;
    }

    void parameterChanged(uint32_t index,float value) override {
        if(index<kParamCount)fVal[index]=value; repaint();
    }
    void stateChanged(const char*,const char*) override {}

    bool onMouse(const MouseEvent& ev) override {
        if(!ev.press){fDrag=-1;return true;}
        const float x=ev.pos.getX(),y=ev.pos.getY();
        fLastY=y;
        const int hit=hitControl(x,y);
        if(hit>=0&&hit<8){fDrag=hit;repaint();return true;}
        if(hit>=8&&hit<40){ // toggles: 20-24 modi, 30 chord, 31 on
            if(hit>=20&&hit<25)spv(kMode,(hit-20)/4.f);
            else if(hit==30)spv(kChord,fVal[kChord]>0.5f?0.f:1.f);
            else if(hit==31)spv(kOn,fVal[kOn]>0.5f?0.f:1.f);
            repaint();return true;
        }
        if(hit>=40&&hit<40+kChipsN){ // chip: woord toevoegen
            const int n=(int)std::strlen(fPrompt);
            if(n<65){if(n>0&&fPrompt[n-1]!=' ')std::strcat(fPrompt," ");
                std::strcat(fPrompt,kChips[hit-40]);}
            repaint();return true;
        }
        if(hit>=100&&hit<200){ // hoofd-acties
            if(hit==100)doMaak();
            else if(hit==101&&fN>1)fN--;
            else if(hit==102&&fN<12)fN++;
            else if(hit==104)fPrompt[0]=0; // wis prompt
            else if(hit==105){ // wis alle ✗
                for(int i=(int)fCands.size()-1;i>=0;i--)if(fCands[i].state==2)fCands.erase(fCands.begin()+i);
            }else if(hit==106){fCands.clear();fMorphA=-1;} // alles wissen
            repaint();return true;
        }
        if(hit>=200){ // kandidaat-rij
            const int i=(hit-200)/10,op=(hit-200)%10;
            if(i<(int)fCands.size())candOp(i,op);
            repaint();return true;
        }
        fDrag=-1;return true;
    }
    bool onMotion(const MotionEvent& ev) override {
        if(fDrag<0)return true;
        const int p=kKnobs[fDrag].p;
        float v=fVal[p]-(ev.pos.getY()-fLastY)*0.006f;
        if(v<0)v=0;if(v>1)v=1;
        fLastY=ev.pos.getY();
        spv(p,v);repaint();return true;
    }
    bool onScroll(const ScrollEvent& ev) override {
        const int hit=hitControl(ev.pos.getX(),ev.pos.getY());
        if(hit<0||hit>=8)return true;
        const int p=kKnobs[hit].p;
        float v=fVal[p]+ev.delta.getY()*0.04f;
        if(v<0)v=0;if(v>1)v=1;
        spv(p,v);repaint();return true;
    }

    void onNanoDisplay() override {
        const float W=getWidth(),H=getHeight(); const float sc=W/900.f;
        beginPath();rect(0,0,W,H);fillColor(C(BG));fill();
        fontSize(15*sc);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);fillColor(C(TX));
        text(18*sc,22*sc,"G2B PROMPT-MAKER",nullptr);
        fontSize(10*sc);fillColor(Color(120,140,125));
        text(18*sc,38*sc,"typ wat je wilt horen — woorden worden synth-settings · 100% sync-veilig",nullptr);
        // status rechts
        fontSize(10*sc);textAlign(ALIGN_RIGHT|ALIGN_MIDDLE);fillColor(Color(150,170,155));
        text(882*sc,22*sc,fStatus,nullptr);
        // ---- prompt-veld ----
        const float px=18*sc,py=52*sc,pw=560*sc,ph=30*sc;
        beginPath();roundedRect(px,py,pw,ph,6*sc);fillColor(C(PN));fill();
        beginPath();roundedRect(px,py,pw,ph,6*sc);strokeColor(C(GR));strokeWidth(1.5f);stroke();
        fontSize(14*sc);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        if(fPrompt[0]){fillColor(C(TX));text(px+10*sc,py+ph/2,fPrompt,nullptr);
            Rectangle<float> rb;
            const float tw=textBounds(px+10*sc,py+ph/2,fPrompt,nullptr,rb)-px;
            strokeColor(C(YE));strokeWidth(2.f);beginPath();
            moveTo(px+12*sc+tw,py+6*sc);lineTo(px+12*sc+tw,py+ph-6*sc);stroke();}
        else{fillColor(Color(90,110,98));text(px+10*sc,py+ph/2,"bijv. donkere techno kick · warme lofi pad",nullptr);}
        // MAAK + N
        smallBtn(590*sc,52*sc,90*sc,30*sc,"MAAK",C(GR),C(BG));
        smallBtn(686*sc,52*sc,26*sc,30*sc,"-",C(PN),C(TX));
        fontSize(12*sc);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);fillColor(C(TX));
        char nb[8];std::snprintf(nb,8,"x%d",fN);text(732*sc,67*sc,nb,nullptr);
        smallBtn(752*sc,52*sc,26*sc,30*sc,"+",C(PN),C(TX));
        smallBtn(790*sc,52*sc,92*sc,30*sc,"WIS PROMPT",C(PN),C(TX));
        // ---- chips ----
        float cx=18*sc,cy=92*sc;
        fontSize(10*sc);
        for(int i=0;i<kChipsN;i++){
            const float cw=(std::strlen(kChips[i])*6.4f+18.f)*sc;
            if(cx+cw>882*sc){cx=18*sc;cy+=26*sc;}
            beginPath();roundedRect(cx,cy,cw,20*sc,9*sc);fillColor(Color(30,38,33));fill();
            textAlign(ALIGN_CENTER|ALIGN_MIDDLE);fillColor(Color(180,200,185));
            text(cx+cw/2,cy+10*sc,kChips[i],nullptr);
            cx+=cw+5*sc;
        }
        // ---- kandidaten ----
        const float ly=cy+34*sc;
        fontSize(11*sc);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);fillColor(C(TX));
        text(18*sc,ly,"RESULTATEN — vink ✓/✗ · ⚗ morph = twee mengen · ⬇ = wav naar ~/G2B-promptmaker",nullptr);
        smallBtn(700*sc,ly-12*sc,88*sc,22*sc,"GOOI ✗ WEG",C(PN),C(RD));
        smallBtn(794*sc,ly-12*sc,88*sc,22*sc,"ALLES WISSEN",C(PN),C(TX));
        const int nC=(int)fCands.size();
        for(int i=0;i<nC&&i<8;i++){
            const Cand&cd=fCands[i];
            const float ry=ly+8*sc+i*26*sc;
            beginPath();roundedRect(18*sc,ry,864*sc,23*sc,4*sc);
            fillColor(i==fMorphA?Color(60,44,92):C(PN));fill();
            if(i==fMorphA){beginPath();roundedRect(18*sc,ry,864*sc,23*sc,4*sc);strokeColor(C(PU));strokeWidth(2.f);stroke();}
            smallBtn(24*sc,ry+2*sc,30*sc,19*sc,"▶",C(GR),C(BG));
            fontSize(11*sc);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);fillColor(C(TX));
            text(62*sc,ry+11.5f*sc,cd.naam,nullptr);
            fontSize(9*sc);fillColor(Color(120,140,125));
            text(340*sc,ry+11.5f*sc,kModeNames[cd.mode],nullptr);
            const char* ops[6]={"✓","✗","⚗","🔁","⬇","✕"};
            for(int o=0;o<6;o++){
                const float ox=560*sc+o*52*sc;
                Color bg=C(PN),fg=C(TX);
                if(o==0&&cd.state==1){bg=C(GR);fg=C(BG);}
                if(o==1&&cd.state==2){bg=C(RD);fg=C(BG);}
                if(o==2){fg=C(PU);}
                smallBtn(ox,ry+2*sc,44*sc,19*sc,ops[o],bg,fg);
            }
        }
        if(nC==0){fontSize(11*sc);fillColor(Color(90,110,98));textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
            text(30*sc,ly+30*sc,"nog geen kandidaten — typ een geluid en druk op MAAK",nullptr);}
        // ---- onderin: modi + toggles + knobs ----
        const float by=ly+8*sc+8*26*sc+12*sc;
        for(int i=0;i<5;i++){
            const float bx=18*sc+i*102*sc;
            const int mode=(int)(fVal[kMode]*4.f+0.5f);
            beginPath();roundedRect(bx,by,94*sc,24*sc,5*sc);
            fillColor(i==mode?C(GR):C(PN));fill();
            fontSize(10*sc);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(i==mode?C(BG):C(TX));
            text(bx+47*sc,by+12*sc,kModeNames[i],nullptr);
        }
        smallBtn(540*sc,by,120*sc,24*sc,fVal[kChord]>0.5f?"AKKOORD: AAN":"AKKOORD: UIT",fVal[kChord]>0.5f?C(YE):C(PN),fVal[kChord]>0.5f?C(BG):C(TX));
        smallBtn(668*sc,by,80*sc,24*sc,fVal[kOn]>0.5f?"AAN":"UIT",fVal[kOn]>0.5f?C(GR):C(RD),C(BG));
        for(int i=0;i<8;i++)
            knobAt(52*sc+i*105*sc,by+62*sc,34*sc,kKnobs[i]);
        fontSize(9*sc);textAlign(ALIGN_LEFT);fillColor(Color(90,105,95));
        text(18*sc,H-8*sc,"typ = prompt · Enter = MAAK · ▶ = laden+luisteren · ⚗⚗ = morph A→B",nullptr);
    }

private:
    float fVal[kParamCount]={}; int fDrag=-1; float fLastY=0;
    char fPrompt[72]={}; char fStatus[160]={};
    int fN=4, fMorphA=-1;
    std::vector<Cand> fCands;
    uint32_t fRnd=987654u;

    void spv(uint32_t i,float v){setParameterValue(i,v);parameterChanged(i,v);}

    static int noteForMode(int m){return m==3?36:(m==4?48:60);}

    void addCand(const char* prompt){
        if((int)fCands.size()>=40)return;
        Cand c; std::memcpy(c.P,fVal,sizeof c.P);
        const char* tag;
        c.mode=g2bPromptMode(prompt,&tag);
        c.P[kMode]=c.mode/4.f;
        g2bRollDice(c.P,fRnd,false);
        g2bPromptTweak(prompt,c.P,fRnd);
        std::strncpy(c.prompt,prompt,79);c.prompt[79]=0;
        char dnm[64];g2bDiceName(c.mode,fRnd,dnm);
        if(prompt[0])std::snprintf(c.naam,71,"%.40s · %.20s",prompt,dnm);
        else std::snprintf(c.naam,71,"%.60s",dnm);
        c.state=0;
        fCands.push_back(c);
    }
    void loadCand(int i){
        const Cand&c=fCands[i];
        for(int p=0;p<kParamCount;p++)
            if(p!=kDice&&p!=kMutate&&p!=kUndoB&&p!=kRedoB)spv(p,c.P[p]);
        sendNote(0,(uint8_t)noteForMode(c.mode),100);
    }
    void doMaak(){
        const char* p=fPrompt;
        for(int k=0;k<fN;k++)addCand(p);
        if(!fCands.empty())loadCand((int)fCands.size()-1);
        std::snprintf(fStatus,159,"%d kandidaten gerold — klik ▶ om te luisteren",fN);
    }
    void candOp(int i,int op){
        Cand&c=fCands[i];
        switch(op){
            case 0: loadCand(i);break;                                 // ▶
            case 1: c.state=c.state==1?0:1;break;                      // ✓
            case 2: c.state=c.state==2?0:2;break;                      // ✗
            case 3:                                                    // ⚗ morph
                if(fMorphA<0)fMorphA=i;
                else if(fMorphA!=i){
                    Cand m; std::memcpy(m.P,fCands[fMorphA].P,sizeof m.P);
                    for(int p=kOsc1W;p<=kWarm;p++)m.P[p]=(m.P[p]+c.P[p])*0.5f;
                    std::snprintf(m.naam,71,"morph %.28s ↔ %.28s",fCands[fMorphA].naam,c.naam);
                    m.prompt[0]=0;m.state=0;m.mode=fCands[fMorphA].mode;
                    m.P[kMode]=m.mode/4.f;
                    fCands.push_back(m); fMorphA=-1;
                    loadCand((int)fCands.size()-1);
                }break;
            case 4:                                                    // 🔁 reroll
                g2bRollDice(c.P,fRnd,false);
                if(c.prompt[0])g2bPromptTweak(c.prompt,c.P,fRnd);
                {char dnm[64];g2bDiceName(c.mode,fRnd,dnm);
                 if(c.prompt[0])std::snprintf(c.naam,71,"%.40s · %.20s",c.prompt,dnm);
                 else std::snprintf(c.naam,71,"%.60s",dnm);}
                loadCand(i);break;
            case 5:{                                                   // ⬇ wav-export
                const char* home=std::getenv("HOME");
                if(!home)home=std::getenv("USERPROFILE");
                if(!home)home=".";
                char dir[320];std::snprintf(dir,319,"%s/G2B-promptmaker",home);
#ifdef DISTRHO_OS_WINDOWS
                mkdir(dir);
#else
                mkdir(dir,0755);
#endif
                char clean[40];int ci=0;
                for(const char*s=c.naam;*s&&ci<35;s++)
                    if((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||*s=='-'||*s=='_')clean[ci++]=*s;
                    else if(*s==' '){if(ci>0&&clean[ci-1]!='-')clean[ci++]='-';}
                clean[ci]=0; if(ci==0)std::strcpy(clean,"sound");
                char path[420];std::snprintf(path,419,"%s/%s.wav",dir,clean);
                const float sr=44100.f; const int n=(int)(4.f*sr);
                std::vector<float> L(n),R(n);
                g2bpm::renderShot(c.P,noteForMode(c.mode),0.85f,4.f,sr,L.data(),R.data());
                if(g2bpm::writeWav(path,L.data(),R.data(),n,sr)==0)
                    std::snprintf(fStatus,159,"opgeslagen: %s",path);
                else std::snprintf(fStatus,159,"opslaan mislukt: %s",path);
                break;}
            case 6:                                                    // ✕
                if(fMorphA==i)fMorphA=-1;
                else if(fMorphA>i)fMorphA--;
                fCands.erase(fCands.begin()+i);break;
        }
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
        const float a0=-2.356f,a1=a0+v*4.712f;
        beginPath();arc(cx,cy,r*0.72f,a0,a1,CW);strokeColor(C(GR));strokeWidth(4);stroke();
        const float pxx=cx+cosf(a1)*r*0.62f,pyy=cy+sinf(a1)*r*0.62f;
        beginPath();moveTo(cx,cy);lineTo(pxx,pyy);strokeColor(C(TX));strokeWidth(2.5f);stroke();
        fontSize(9.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);fillColor(C(TX));
        text(cx,cy+r+9.f,k.lbl,nullptr);
        char vb[8];std::snprintf(vb,8,"%d",(int)(v*100));
        fontSize(9.f);fillColor(Color(130,150,135));text(cx,cy+r+21.f,vb,nullptr);
    }
    int hitControl(float x,float y){
        const float W=getWidth(),sc=W/900.f;
        for(int i=0;i<8;i++){
            const float cx=52*sc+i*105*sc;
            const float by=chipRowsEnd()+8*sc+8*26*sc+12*sc+62*sc;
            if(std::hypotf(x-cx,y-by)<34*sc)return i;
        }
        for(int i=0;i<5;i++){
            const float bx=18*sc+i*102*sc,by=chipRowsEnd()+8*sc+8*26*sc+12*sc;
            if(x>bx&&x<bx+94*sc&&y>by&&y<by+24*sc)return 20+i;
        }
        {   const float by=chipRowsEnd()+8*sc+8*26*sc+12*sc;
            if(x>540*sc&&x<660*sc&&y>by&&y<by+24*sc)return 30;
            if(x>668*sc&&x<748*sc&&y>by&&y<by+24*sc)return 31;
        }
        // chips
        {   float cx=18*sc,cy=92*sc;
            for(int i=0;i<kChipsN;i++){
                const float cw=(std::strlen(kChips[i])*6.4f+18.f)*sc;
                if(cx+cw>882*sc){cx=18*sc;cy+=26*sc;}
                if(x>cx&&x<cx+cw&&y>cy&&y<cy+20*sc)return 40+i;
                cx+=cw+5*sc;
            }
        }
        // hoofd-acties
        if(x>590*sc&&x<680*sc&&y>52*sc&&y<82*sc)return 100;
        if(x>686*sc&&x<712*sc&&y>52*sc&&y<82*sc)return 101;
        if(x>752*sc&&x<778*sc&&y>52*sc&&y<82*sc)return 102;
        if(x>790*sc&&x<882*sc&&y>52*sc&&y<82*sc)return 104;
        {   const float ly=chipRowsEnd();
            if(x>700*sc&&x<788*sc&&y>ly-12*sc&&y<ly+10*sc)return 105;
            if(x>794*sc&&x<882*sc&&y>ly-12*sc&&y<ly+10*sc)return 106;
            // kandidaat-rijen
            const int nC=(int)fCands.size();
            for(int i=0;i<nC&&i<8;i++){
                const float ry=ly+8*sc+i*26*sc;
                if(y<ry||y>ry+23*sc)continue;
                if(x>24*sc&&x<54*sc)return 200+i*10+0;      // ▶
                for(int o=0;o<6;o++){
                    const float ox=560*sc+o*52*sc;
                    if(x>ox&&x<ox+44*sc)return 200+i*10+1+o;
                }
            }
        }
        return -1;
    }
    // y waar de chip-rij eindigt (zelfde wrap-logica als de draw)
    float chipRowsEnd() const {
        const float W=getWidth(),sc=W/900.f;
        float cx=18*sc,cy=92*sc;
        for(int i=0;i<kChipsN;i++){
            const float cw=(std::strlen(kChips[i])*6.4f+18.f)*sc;
            if(cx+cw>882*sc){cx=18*sc;cy+=26*sc;}
            cx+=cw+5*sc;
        }
        return cy+34*sc;
    }
};

UI* createUI(){return new PromptmakerUI();}
END_NAMESPACE_DISTRHO
