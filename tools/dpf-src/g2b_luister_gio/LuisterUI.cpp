/*
 * G2B LUISTER — UI (NanoVG): 4 knoppen + genre-pills + status-scope.
 * LUISTER → speel geluid → GEN → MIDI-loops naar je VST-sporen.
 */

#include "DistrhoUI.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// G2B skins (8 thema's): bg,pn,tx,gy,ed,GR,DG
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
static const int kNumPrograms = 14;
static const int kPerRow = 7;
#else
static const int kNumPrograms = 10;
static const int kPerRow = 5;
#endif
static const char* kProgramNames[kNumPrograms] = {
    "TRAP","DRILL","BOOMBAP","RNB","HOUSE","DEMBOW","KAWINA","LOFI","SOUL","POP"
#ifdef G2B_GIO
    ,"AMBIENT","SOCA","DANCEHALL","ZOUK"
#endif
};
static const char* kKlasNames[5]={"—","DRUMS","MELODIE","ONE-SHOT","DRUMS+MELODIE"};
static const char* kStatNames[6]={"klaar — druk LUISTER","LUISTERT… speel je geluid","analyseert…","LOOP SPEELT",".mid opgeslagen in Music/G2B","—"};
static const char* kKeyNames[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

class G2BLuisterUI : public UI
{
public:
    G2BLuisterUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        loadSharedResources();
        static const float mins[4] = {0,0,0,0};
        static const float maxs[4] = {100,100,3,100};
        static const float defs[4] = {50,30,1,30};
        static const char* lbl[4]  = {"HEFT","MENS","LENGTE","VAR"};
        static const char* unt[4]  = {"%","%","","%"};
        std::memcpy(fMin,mins,sizeof(mins));
        std::memcpy(fMax,maxs,sizeof(maxs));
        std::memcpy(fValue,defs,sizeof(defs));
        std::memcpy(fTarget,defs,sizeof(defs));
        std::memcpy(fLabel,lbl,sizeof(lbl));
        std::memcpy(fUnit,unt,sizeof(unt));
        std::memset(fScope,0,sizeof(fScope));
        fScopePos=0;fDrag=-1;fLastY=0.f;fProg=0;
    }
protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if(index<4) fTarget[index]=value;
        else if(index==5){fProg=(int)(value+0.5f);if(fProg<0)fProg=0;if(fProg>kNumPrograms-1)fProg=kNumPrograms-1;}
        else if(index==6) fOn=value;
        else if(index==7) fLuist=value;
        else if(index==11){fScope[fScopePos]=value;fScopePos=(fScopePos+1)%kScopeSize;fLevel=value;}
        else if(index==12) fKlas=(int)(value+0.5f);
        else if(index==13) fKey=(int)(value+0.5f);
        else if(index==14) fBpm=value;
        else if(index==15) fStat=(int)(value+0.5f);
        else if(index==16) fHits=(int)(value+0.5f);
        else if(index>=17&&index<=20) fTarget[index-17]=value;
        repaint();
    }

    void onNanoDisplay() override
    {
        const float W=getWidth(),H=getHeight();
        beginPath();rect(0,0,W,H);fillColor(g2bsk(fLight,0));fill();

        fontSize(22.0f);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        fillColor(g2bsk(fLight,5));
        text(18.0f,22.0f,"G2B LUISTER"
#ifdef G2B_GIO
        " GIO"
#endif
        ,nullptr);
        fontSize(9.0f);fillColor(g2bsk(fLight,3));
        text(20.0f,40.0f,"v1 · speel een geluid → hij schrijft de MIDI-loop",nullptr);

        // ruimte-pills (2 rijen van 4, bredere pills)
        for(int i=0;i<kNumPrograms;++i){
            const int row=i/kPerRow,col=i%kPerRow;
            const float bw=(kPerRow==7?58.0f:86.0f),bh=20.0f,gap=8.0f;
            const float rowW=kPerRow*bw+(kPerRow-1)*gap;
            const float bx=(W-rowW)*0.5f+col*(bw+gap);
            const float by=row==0?58.0f:82.0f;
            const bool on=(i==fProg);
            beginPath();roundedRect(bx,by,bw,bh,5.0f);
            if(on){fillColor(g2bsk(fLight,5));fill();}
            else{fillColor(g2bsk(fLight,1));fill();}
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            fontSize(10.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(on?g2bsk(fLight,0):g2bsk(fLight,2));
            text(bx+bw*0.5f,by+bh*0.5f,kProgramNames[i],nullptr);
        }

        // scope + actieve-ruimte hint
        const float sx=18.0f,sy=112.0f,sw=W-36.0f,sh=30.0f;
        beginPath();rect(sx,sy,sw,sh);
        fillColor(fLight==4?Color(30,34,38):Color(8,10,9));fill();
        strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
        beginPath();
        const float mid=sy+sh*0.5f;
        for(int i=0;i<kScopeSize-1;++i){
            const int i0=(fScopePos+i)%kScopeSize,i1=(fScopePos+i+1)%kScopeSize;
            const float x0=sx+(float)i/(kScopeSize-1)*sw,x1=sx+(float)(i+1)/(kScopeSize-1)*sw;
            float y0=sy+sh-1.0f-fScope[i0]*(sh-2.0f),y1=sy+sh-1.0f-fScope[i1]*(sh-2.0f);
            if(y0<sy+1)y0=sy+1;if(y0>sy+sh-1)y0=sy+sh-1;
            if(y1<sy+1)y1=sy+1;if(y1>sy+sh-1)y1=sy+sh-1;
            if(i==0)moveTo(x0,y0);else lineTo(x0,y0);lineTo(x1,y1);
        }
        strokeColor(g2bsk(fLight,5));strokeWidth(1.5f);stroke();
        {   // status + wat hij hoorde
            char lb[160];const char*st=kStatNames[fStat<0?0:(fStat>5?5:fStat)];
            if(fKlas>0){char kb[48]="";if(fKey>=0)std::snprintf(kb,sizeof(kb)," · %s%s",kKeyNames[fKey%12],fKey>=12?" min":"");std::snprintf(lb,sizeof(lb),"%s · %s · %.0fBPM · %d hits%s",st,kKlasNames[fKlas<0?0:(fKlas>4?4:fKlas)],fBpm,fHits,kb);}
            else std::snprintf(lb,sizeof(lb),"%s · genre %s",st,kProgramNames[fProg]);
            fontSize(10.0f);textAlign(ALIGN_RIGHT|ALIGN_MIDDLE);
            fillColor(g2bsk(fLight,2));
            text(sx+sw-8.0f,sy+sh*0.5f,lb,nullptr);
        }

        // 4 grote knoppen (links-gebied, meter rechts)
        const float knArea=W-140.0f;
        const float step=(knArea-40.0f)/4;
        for(int i=0;i<4;++i){
            const float cx=20.0f+step*(i+0.5f);
            const float cy=200.0f;
            const float R=30.0f;
            const float norm=(fValue[i]-fMin[i])/(fMax[i]-fMin[i]);
            const float ang=-2.4f+norm*4.8f;
            beginPath();circle(cx,cy,R);
            fillColor(g2bsk(fLight,1));fill();
            strokeColor(g2bsk(fLight,4));strokeWidth(2.0f);stroke();
            beginPath();
            arc(cx,cy,R-6.0f,-2.4f-1.5708f,ang-1.5708f,CW);
            strokeColor(g2bsk(fLight,5));strokeWidth(5.0f);stroke();
            beginPath();moveTo(cx,cy);
            lineTo(cx+std::sin(ang)*(R-12.0f),cy-std::cos(ang)*(R-12.0f));
            strokeColor(g2bsk(fLight,2));strokeWidth(3.5f);stroke();
            fontSize(11.5f);fillColor(g2bsk(fLight,3));
            textAlign(ALIGN_CENTER|ALIGN_TOP);
            text(cx,cy+R+10.0f,fLabel[i],nullptr);
            fontSize(13.0f);fillColor(g2bsk(fLight,2));
            char buf[24];
            if(i==2){static const int bl[4]={2,4,8,16};std::snprintf(buf,sizeof(buf),"%d bars",bl[fValue[i]<0?0:(fValue[i]>3?3:(int)fValue[i])]);}
            else std::snprintf(buf,sizeof(buf),"%.0f %%",fValue[i]);
            text(cx,cy+R+26.0f,buf,nullptr);
        }

        // LEVEL-blok rechts
        {
            const float mx=W-120.0f,my=170.0f,mw=102.0f,mh=110.0f;
            beginPath();roundedRect(mx,my,mw,mh,8.0f);
            fillColor(g2bsk(fLight,1));fill();
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            fontSize(11.0f);textAlign(ALIGN_CENTER|ALIGN_TOP);
            fillColor(g2bsk(fLight,3));
            text(mx+mw*0.5f,my+8.0f,"LEVEL",nullptr);
            float fh=fLevel;if(fh<0)fh=0;if(fh>1)fh=1;
            const float bx=mx+14.0f,bw2=14.0f,bh2=mh-40.0f;
            beginPath();rect(bx,my+22.0f,bw2,bh2);
            fillColor(fLight==4?Color(30,34,38):Color(8,10,9));fill();
            beginPath();rect(bx,my+22.0f+bh2*(1.f-fh),bw2,bh2*fh);
            fillColor(fh>0.85f?Color(220,80,60):g2bsk(fLight,5));fill();
            fontSize(8.5f);fillColor(g2bsk(fLight,3));
            text(mx+mw*0.5f+12.0f,my+30.0f,"ingaand",nullptr);
            text(mx+mw*0.5f+12.0f,my+48.0f,"piek",nullptr);
        }

        // actie-knoppen: LUISTER · GEN · REFINE · ↓MID
        {
            static const char* btn[4]={"LUISTER","GEN","REFINE","↓MID"};
            for(int i=0;i<4;++i){
                const float bx=20.0f+i*78.0f,by=146.0f,bw=70.0f,bh=18.0f;
                const bool live=(i==0&&fLuist>0.5f);
                beginPath();roundedRect(bx,by,bw,bh,4.0f);
                fillColor(live?g2bsk(fLight,5):g2bsk(fLight,1));fill();
                strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
                fontSize(9.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
                fillColor(live?g2bsk(fLight,0):g2bsk(fLight,2));
                text(bx+bw*0.5f,by+bh*0.5f,btn[i],nullptr);
            }
        }

        fontSize(10.5f);fillColor(g2bsk(fLight,3));
        textAlign(ALIGN_CENTER|ALIGN_BOTTOM);
        text(W*0.5f,H-8.0f,"LUISTER → speel je geluid → GEN → MIDI speelt mee + ↓MID slaat 2 .mid's op (drums+melodie)",nullptr);

        // DICE + skin + AAN
        {
            const float dy=146.0f;
            beginPath();roundedRect(W-120.0f,dy,44.0f,18.0f,4.0f);
            fillColor(g2bsk(fLight,1));fill();
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            fontSize(9.0f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(g2bsk(fLight,2));
            text(W-98.0f,dy+9.0f,"DICE",nullptr);

            const float tx=W-40.0f;
            beginPath();roundedRect(tx,34.0f,26.0f,17.0f,4.0f);
            fillColor(g2bsk(fLight,1));fill();
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            {   const float cx=tx+13.0f,cy=42.5f;
                if(fLight>0){fillColor(g2bsk(fLight,2));
                    beginPath();arc(cx+1.5f,cy,5.0f,0.f,6.2832f,CW);fill();
                    fillColor(g2bsk(fLight,1));
                    beginPath();arc(cx-1.8f,cy,4.4f,0.f,6.2832f,CW);fill();}
                else{fillColor(g2bsk(fLight,2));
                    beginPath();arc(cx,cy,4.2f,0.f,6.2832f,CW);fill();
                    strokeColor(g2bsk(fLight,2));strokeWidth(1.2f);
                    for(int r=0;r<8;++r){const float a=r*0.7854f;
                        beginPath();moveTo(cx+cosf(a)*5.6f,cy+sinf(a)*5.6f);
                        lineTo(cx+cosf(a)*7.4f,cy+sinf(a)*7.4f);stroke();}}
            }
            const bool aan=fOn>0.5f;
            beginPath();roundedRect(W-96.0f,33.0f,52.0f,18.0f,9.0f);
            fillColor(aan?g2bsk(fLight,5):g2bsk(fLight,1));fill();
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            fontSize(10.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(aan?g2bsk(fLight,0):g2bsk(fLight,2));
            text(W-70.0f,42.0f,aan?"AAN":"UIT",nullptr);
        }
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if(!ev.press&&fBtnHeld>=0){setParameterValue(fBtnHeld,0.0f);fBtnHeld=-1;}
        if(ev.button!=1)return false;
        const float W=getWidth();
        const float px=(float)(ev.pos.getX()/getScaleFactor());
        const float py=(float)(ev.pos.getY()/getScaleFactor());
        if(ev.press){
            if(px>=W-96.0f&&px<=W-44.0f&&py>=33.0f&&py<=51.0f)
            {fOn=fOn>0.5f?0.f:1.f;setParameterValue(6,fOn);repaint();return true;}
            if(px>=W-120.0f&&px<=W-76.0f&&py>=146.0f&&py<=164.0f)
            {setParameterValue(4,1.0f);fBtnHeld=4;repaint();return true;}
            for(int i=0;i<4;++i){
                const float bx=20.0f+i*78.0f,bw=70.0f;
                if(px>=bx&&px<=bx+bw&&py>=146.0f&&py<=164.0f){
                    if(i==0){fLuist=fLuist>0.5f?0.f:1.f;setParameterValue(7,fLuist);}
                    else if(i==1){setParameterValue(8,1.0f);fBtnHeld=8;}
                    else if(i==2){setParameterValue(10,1.0f);fBtnHeld=10;}
                    else{setParameterValue(9,1.0f);fBtnHeld=9;};
                    repaint();return true;}
            }
            if(px>=W-40.0f&&px<=W-14.0f&&py>=34.0f&&py<=51.0f)
            {fLight=(fLight+1)%8;repaint();return true;}
            for(int i=0;i<kNumPrograms;++i){
                const int row=i/kPerRow,col=i%kPerRow;
                const float bw=(kPerRow==7?58.0f:86.0f),bh=20.0f,gap=8.0f;
                const float rowW=kPerRow*bw+(kPerRow-1)*gap;
                const float bx=(W-rowW)*0.5f+col*(bw+gap);
                const float by=row==0?58.0f:82.0f;
                if(px>=bx&&px<=bx+bw&&py>=by&&py<=by+bh)
                {fProg=i;setParameterValue(5,(float)i);repaint();return true;}
            }
            const float knArea=W-140.0f;
            const float step=(knArea-40.0f)/4;
            for(int i=0;i<4;++i){
                const float cx=20.0f+step*(i+0.5f),cy=200.0f;
                const float dx=px-cx,dy=py-cy;
                if(dx*dx+dy*dy<=30.0f*30.0f+140.0f)
                {fDrag=i;fLastY=py;editParameter(i,true);return true;}
            }
        }else if(fDrag>=0){editParameter(fDrag,false);fDrag=-1;}

        return true;
    }
    bool onMotion(const MotionEvent& ev) override
    {
        if(fDrag<0)return false;
        const float dy=fLastY-(float)(ev.pos.getY()/getScaleFactor());
        fLastY=(float)(ev.pos.getY()/getScaleFactor());
        const float range=fMax[fDrag]-fMin[fDrag];
        float v=fValue[fDrag]+dy*range/240.0f;
        if(v<fMin[fDrag])v=fMin[fDrag];if(v>fMax[fDrag])v=fMax[fDrag];
        fValue[fDrag]=v;fTarget[fDrag]=v;
        setParameterValue(fDrag,v);
        repaint();
        return true;
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
                fValue[i]=v;fTarget[i]=v;
                editParameter(i,true);setParameterValue(i,v);editParameter(i,false);
                repaint();return true;
            }
        }
        return true;
    }
private:
public:
    void uiIdle() override{
        bool moved=false;
        for(int p=0;p<4;++p){const float d=fTarget[p]-fValue[p];
            if(d>0.35f||d<-0.35f){fValue[p]+=d*0.25f;moved=true;}
            else if(d!=0.f){fValue[p]=fTarget[p];moved=true;}}
        if(moved)repaint();
    }
private:
    static const int kScopeSize=256;
    float fValue[4],fMin[4],fMax[4],fTarget[4];
    const char* fLabel[4];const char* fUnit[4];
    float fOn=1.f,fLuist=0.f,fLevel=0.f;
    float fScope[kScopeSize];int fScopePos;
    int fDrag;float fLastY;int fProg;int fLight=0;
    int fKlas=0,fKey=-1,fStat=0,fHits=0;float fBpm=120.f;
    int fBtnHeld=-1;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BLuisterUI)
};

UI* createUI() { return new G2BLuisterUI(); }

END_NAMESPACE_DISTRHO
