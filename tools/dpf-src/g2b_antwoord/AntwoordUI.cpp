/*
 * G2B ANTWOOORD — UI (NanoVG): 4 knoppen + 10 genre-pills + gespreks-status.
 * LUISTER = hij vangt jouw 4 tellen · antwoordt dan. VRAAG = hij stelt jou iets.
 */
#include "DistrhoUI.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>

static const float G2BSK[8][7][3]={
    {{13,15,14},{23,26,25},{232,236,234},{100,110,103},{42,47,45},{47,191,113},{29,122,72}},
    {{14,14,22},{22,22,34},{230,232,255},{110,110,150},{40,40,58},{120,110,255},{70,60,170}},
    {{10,20,22},{16,30,32},{225,245,245},{95,130,130},{30,55,55},{40,200,210},{20,120,130}},
    {{24,14,20},{34,20,28},{250,230,240},{150,105,125},{60,38,50},{255,110,180},{180,60,120}},
    {{228,230,232},{240,242,244},{30,34,38},{105,112,118},{205,210,214},{40,140,100},{20,100,70}},
    {{18,10,10},{28,16,16},{240,225,220},{140,100,95},{55,35,32},{200,80,60},{130,45,35}},
    {{14,14,10},{22,22,16},{245,240,220},{120,115,95},{50,48,35},{230,190,80},{160,125,45}},
    {{12,14,20},{20,24,32},{235,240,250},{105,115,135},{36,44,58},{80,160,255},{45,110,200}}};
static const char* G2BSKN[8]={"MELIOR","SPECTRUM","LAGOON","CANDY","GHOST","MAROON","GOLD","STUDIO"};
static inline Color g2bsk(int sk,int r){if(sk<0)sk=0;if(sk>7)sk=7;const float*c=G2BSK[sk][r];return Color((int)c[0],(int)c[1],(int)c[2]);}

START_NAMESPACE_DISTRHO

#ifdef G2B_GIO
static const int kNumPrograms=14;
static const char* kProgramNames[kNumPrograms]={
    "TRAP","DRILL","BOOMBAP","RNB","HOUSE","DEMBOW","KAWINA",
    "LOFI","SOUL","POP","AMBIENT","SOCA","DANCEHALL","ZOUK"};
static const int kPerRow=7;
#else
static const int kNumPrograms=10;
static const char* kProgramNames[kNumPrograms]={
    "TRAP","DRILL","BOOMBAP","RNB","HOUSE","DEMBOW","KAWINA","LOFI","SOUL","POP"};
static const int kPerRow=5;
#endif
static const char* kKeyNames[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static const char* kFaseTxt[4]={"klaar — druk LUISTER en speel","LUISTERT… speel 4 tellen","ANTWOORDT","STELT JOU EEN VRAAG"};

class G2BAntwoordUI : public UI {
public:
    G2BAntwoordUI() : UI(DISTRHO_UI_DEFAULT_WIDTH,DISTRHO_UI_DEFAULT_HEIGHT)
    {
        loadSharedResources();
        static const float mins[4]={0,0,0,0};
        static const float maxs[4]={100,100,100,100};
        static const float defs[4]={45,40,50,100};
        static const char* lbl[4]={"HEFTRIGHEID","MENS","GEHEUGEN","VOLUME"};
        static const char* unt[4]={"%","%","%","%"};
        std::memcpy(fMin,mins,sizeof(mins));std::memcpy(fMax,maxs,sizeof(maxs));
        std::memcpy(fValue,defs,sizeof(defs));
        std::memcpy(fLabel,lbl,sizeof(lbl));std::memcpy(fUnit,unt,sizeof(unt));
        fProg=0;fDrag=-1;fScopePos=0;std::memset(fScope,0,sizeof(fScope));
    }

    void parameterChanged(uint32_t index,float value) override
    {
        if(index<3) fValue[index]=value;
        else if(index==4){fProg=(int)(value+0.5f);if(fProg<0)fProg=0;if(fProg>=kNumPrograms)fProg=kNumPrograms-1;}
        else if(index==5) fOn=value;
        else if(index==6) fValue[3]=value;
        else if(index==8) fLoop=value;
        else if(index==12){fKey=(int)value%12;fMinor=value>=12.f;}
        else if(index==13) fConf=value;
        else if(index==14) fFase=(int)value;
        else if(index==15){fScope[fScopePos]=value;fScopePos=(fScopePos+1)%kScopeSize;}
        else if(index==16) fNoteNow=(int)value;
        else if(index==17) fProg2=value;
        repaint();
    }

    void onNanoDisplay() override
    {
        const float W=getWidth(),H=getHeight();
        beginPath();rect(0,0,W,H);fillColor(g2bsk(fLight,0));fill();

        fontSize(22.0f);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        fillColor(g2bsk(fLight,5));
        text(18.0f,22.0f,
#ifdef G2B_GIO
            "G2B ANTWOORD GIO",
#else
            "G2B ANTWOORD",
#endif
            nullptr);
        fontSize(9.0f);fillColor(g2bsk(fLight,3));
        text(20.0f,40.0f,"v1 · speel 4 tellen — hij antwoordt in jouw stijl",nullptr);

        // genre-pills (2 rijen van 5) — zelfde als Master
        for(int i=0;i<kNumPrograms;++i){
            const int row=i/kPerRow,col=i%kPerRow;
            const float bw=86.0f,bh=20.0f,gap=8.0f;
            const float rowW=kPerRow*bw+(kPerRow-1)*gap;
            const float bx=(W-rowW)*0.5f+col*(bw+gap);
            const float by=58.0f+row*24.0f;
            const bool on=(i==fProg);
            beginPath();roundedRect(bx,by,bw,bh,5.0f);
            if(on){fillColor(g2bsk(fLight,5));fill();}
            else{fillColor(g2bsk(fLight,1));fill();}
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            fontSize(10.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(on?g2bsk(fLight,0):g2bsk(fLight,2));
            text(bx+bw*0.5f,by+bh*0.5f,kProgramNames[i],nullptr);
        }

        // status-vlak: fase + key-detect met confidence
        {
            const float sx=18.0f,sy=112.0f,sw=W-36.0f,sh=30.0f;
            beginPath();roundedRect(sx,sy,sw,sh,5.0f);
            fillColor(g2bsk(fLight,1));fill();
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            fontSize(10.5f);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
            fillColor(g2bsk(fLight,5));
            text(sx+12.0f,sy+sh*0.5f,kFaseTxt[fFase<0?0:(fFase>3?3:fFase)],nullptr);
            // voortgangsbalk tijdens luister/antwoord
            if(fFase>0){
                beginPath();rect(sx+4.0f,sy+sh-4.0f,(sw-8.0f)*fProg2/100.0f,2.0f);
                fillColor(g2bsk(fLight,5));fill();
            }
            // key + confidence rechts
            char kb[48];
            std::snprintf(kb,sizeof(kb),"%s %s  %.0f%%",kKeyNames[fKey],fMinor?"min":"maj",fConf);
            fontSize(11.0f);fillColor(g2bsk(fLight,2));
            textAlign(ALIGN_RIGHT|ALIGN_MIDDLE);
            text(sx+sw-12.0f,sy+sh*0.5f,kb,nullptr);
        }

        // knoppen-rij: LUISTER VRAAG LOOP ↓MID
        {
            const char* bt[4]={"LUISTER","VRAAG","LOOP","↓MID"};
            const int bcol[4]={5,5,4,4};
            for(int i=0;i<4;i++){
                const float bx=90.0f+i*64.0f,by=146.0f,bw=56.0f,bh=18.0f;
                const bool on=(i==2&&fLoop>0.5f);
                beginPath();roundedRect(bx,by,bw,bh,4.0f);
                fillColor(on?g2bsk(fLight,5):g2bsk(fLight,bcol[i]));fill();
                fontSize(9.0f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
                fillColor(on?g2bsk(fLight,0):g2bsk(fLight,2));
                text(bx+bw*0.5f,by+bh*0.5f,bt[i],nullptr);
            }
            // huidige antwoord-noot rechts ervan
            if(fNoteNow>0){fontSize(9.0f);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
                fillColor(g2bsk(fLight,3));
                char nb[24];std::snprintf(nb,sizeof(nb),"↪ %s%d",kKeyNames[fNoteNow%12],fNoteNow/12-1);
                text(360.0f,155.0f,nb,nullptr);}
        }

        // 4 grote knoppen
        {
            const float knArea=W-140.0f;
            const float step=(knArea-40.0f)/4;
            for(int i=0;i<4;++i){
                const float cx=20.0f+step*(i+0.5f),cy=200.0f,R=30.0f;
                const float norm=(fValue[i]-fMin[i])/(fMax[i]-fMin[i]);
                const float ang=norm*4.8f-2.4f;
                beginPath();circle(cx,cy,R);
                fillColor(g2bsk(fLight,1));fill();
                strokeColor(g2bsk(fLight,4));strokeWidth(1.5f);stroke();
                beginPath();arc(cx,cy,R-6.0f,-2.4f-1.5708f,ang-1.5708f,CW);
                strokeColor(g2bsk(fLight,5));strokeWidth(4.0f);stroke();
                beginPath();moveTo(cx,cy);
                lineTo(cx+std::sin(ang)*(R-12.0f),cy-std::cos(ang)*(R-12.0f));
                strokeColor(g2bsk(fLight,2));strokeWidth(2.0f);stroke();
                fontSize(9.0f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
                fillColor(g2bsk(fLight,3));
                text(cx,cy+R+10.0f,fLabel[i],nullptr);
                char buf[16];std::snprintf(buf,sizeof(buf),"%.0f %%",fValue[i]);
                fontSize(10.0f);fillColor(g2bsk(fLight,2));
                text(cx,cy+R+26.0f,buf,nullptr);
            }
        }

        // level-meter rechts
        {
            const float mx=W-40.0f,my=200.0f-30.0f,mw=10.0f,mh=60.0f;
            beginPath();roundedRect(mx,my,mw,mh,3.0f);
            fillColor(g2bsk(fLight,1));fill();
            float lv=fScope[(fScopePos+kScopeSize-1)%kScopeSize];
            if(lv>1.f)lv=1.f;
            beginPath();rect(mx,my+mh*(1.f-lv),mw,mh*lv);
            fillColor(g2bsk(fLight,5));fill();
        }

        // DICE + AAN + skin-toggle (zelfde plek als Master)
        {
            beginPath();roundedRect(W-120,146,44,18,4.0f);
            fillColor(g2bsk(fLight,1));fill();
            fontSize(9.0f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(g2bsk(fLight,2));text(W-98,155,"DICE",nullptr);

            beginPath();roundedRect(W-96,33,52,18,4.0f);
            fillColor(fOn>0.5f?g2bsk(fLight,5):g2bsk(fLight,1));fill();
            fontSize(9.0f);fillColor(fOn>0.5f?g2bsk(fLight,0):g2bsk(fLight,2));
            text(W-70,42,fOn>0.5f?"AAN":"UIT",nullptr);

            beginPath();circle(W-27,42,9.0f);
            fillColor(g2bsk(fLight,4));fill();
            fontSize(8.0f);fillColor(g2bsk(fLight,2));
            text(W-27,42,"◐",nullptr);
        }
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if(!ev.press&&fBtnHeld>=0){setParameterValue(fBtnHeld,0.0f);fBtnHeld=-1;}
        const float W=getWidth();
        const float px=(float)(ev.pos.getX()/getScaleFactor());
        const float py=(float)(ev.pos.getY()/getScaleFactor());
        if(ev.press){
            if(px>=W-96.0f&&px<=W-44.0f&&py>=33.0f&&py<=51.0f)
                {fOn=fOn>0.5f?0.f:1.f;setParameterValue(5,fOn);repaint();return true;}
            if(px>=W-120.0f&&px<=W-76.0f&&py>=146.0f&&py<=164.0f)
                {setParameterValue(3,1.0f);fBtnHeld=3;;repaint();return true;}
            if(px>=W-40.0f&&px<=W-14.0f&&py>=34.0f&&py<=51.0f)
                {fLight=(fLight+1)%8;repaint();return true;}
            // knoppen-rij LUISTER VRAAG LOOP ↓MID
            if(py>=146.0f&&py<=164.0f){
                for(int i=0;i<4;i++){
                    const float bx=90.0f+i*64.0f;
                    if(px>=bx&&px<=bx+56.0f){
                        if(i==0){setParameterValue(7,1.f);fBtnHeld=7;}
                        else if(i==1){setParameterValue(9,1.f);fBtnHeld=9;}
                        else if(i==2){fLoop=fLoop>0.5f?0.f:1.f;setParameterValue(8,fLoop);}
                        else{setParameterValue(11,1.f);fBtnHeld=11;};
                        repaint();return true;}
                }
            }
            // pills
            for(int i=0;i<kNumPrograms;++i){
                const int row=i/kPerRow,col=i%kPerRow;
                const float bw=86.0f,bh=20.0f,gap=8.0f;
                const float rowW=kPerRow*bw+(kPerRow-1)*gap;
                const float bx=(W-rowW)*0.5f+col*(bw+gap);
                const float by=58.0f+row*24.0f;
                if(px>=bx&&px<=bx+bw&&py>=by&&py<=by+bh)
                    {fProg=i;setParameterValue(4,(float)i);repaint();return true;}
            }
            // knoppen
            const float knArea=W-140.0f;
            const float step=(knArea-40.0f)/4;
            for(int i=0;i<4;++i){
                const float cx=20.0f+step*(i+0.5f),cy=200.0f;
                const float dx=px-cx,dy=py-cy;
                if(dx*dx+dy*dy<=30.0f*30.0f+140.0f)
                    {fDrag=i;fLastY=py;editParameter(i==3?6:i,true);return true;}
            }
        }else if(fDrag>=0){editParameter(fDrag==3?6:fDrag,false);fDrag=-1;}
        return true;
    }
    bool onMotion(const MotionEvent& ev) override
    {
        if(fDrag>=0){
            const float py=(float)(ev.pos.getY()/getScaleFactor());
            const float range=fMax[fDrag]-fMin[fDrag];
            float v=fValue[fDrag]+(fLastY-py)*range/240.0f;
            fLastY=py;
            if(v<fMin[fDrag])v=fMin[fDrag];if(v>fMax[fDrag])v=fMax[fDrag];
            fValue[fDrag]=v;
            setParameterValue(fDrag==3?6:fDrag,v);
            repaint();return true;
        }
        return false;
    }
    bool onScroll(const ScrollEvent& ev) override
    {
        const float W=getWidth();
        const float px=(float)(ev.pos.getX()/getScaleFactor());
        const float py=(float)(ev.pos.getY()/getScaleFactor());
        const float knArea=W-140.0f;
        const float step=(knArea-40.0f)/4;
        for(int i=0;i<4;++i){
            const float cx=20.0f+step*(i+0.5f),cy=200.0f;
            const float dx=px-cx,dy=py-cy;
            if(dx*dx+dy*dy<=30.0f*30.0f+140.0f){
                const float range=fMax[i]-fMin[i];
                float v=fValue[i]+ev.delta.getY()*range/100.0f;
                if(v<fMin[i])v=fMin[i];if(v>fMax[i])v=fMax[i];
                fValue[i]=v;
                editParameter(i==3?6:i,true);setParameterValue(i==3?6:i,v);editParameter(i==3?6:i,false);
                repaint();return true;
            }
        }
        return true;
    }
private:
    static const int kScopeSize=256;
    float fValue[4],fMin[4],fMax[4];
    const char* fLabel[4];const char* fUnit[4];
    float fOn=1.f,fLoop=0.f,fConf=0.f,fProg2=0.f;
    float fScope[kScopeSize];int fScopePos;
    int fKey=9,fMinor=0,fFase=0,fNoteNow=0;
    int fDrag;float fLastY;int fProg;int fLight=0;
    int fBtnHeld=-1;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BAntwoordUI)
};

UI* createUI(){return new G2BAntwoordUI();}
END_NAMESPACE_DISTRHO
