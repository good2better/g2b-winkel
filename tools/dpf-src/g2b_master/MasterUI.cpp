/*
 * G2B MASTER — UI (NanoVG): 4 grote knoppen + 10 genre-presets + LUFS-meter.
 * Sleep = draaien, scroll = fijn, klik preset = knoppen goed zetten.
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

static const int kNumPrograms = 10;
static const char* kProgramNames[kNumPrograms] = {
    "TRAP","DRILL","BOOMBAP","RNB","HOUSE","DEMBOW","KAWINA","LOFI","SOUL","POP"
};
// UI-kopie van de preset-tabel (TILT GLUE WARMTE LUID)
static const float kUiPresetParams[kNumPrograms][4] = {
    {-10.f,65.f,40.f,85.f},{-5.f,70.f,45.f,85.f},{-15.f,55.f,55.f,75.f},
    {+5.f,50.f,30.f,70.f},{+10.f,60.f,30.f,80.f},{0.f,65.f,35.f,80.f},
    {+5.f,55.f,30.f,75.f},{-20.f,40.f,70.f,55.f},{-5.f,45.f,50.f,65.f},
    {+10.f,65.f,25.f,85.f}};

class G2BMasterUI : public UI
{
public:
    G2BMasterUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        loadSharedResources();
        static const float mins[4] = {-50,0,0,0};
        static const float maxs[4] = {50,100,100,100};
        static const float defs[4] = {0,50,30,60};
        static const char* lbl[4]  = {"TILT","GLUE","WARMTE","LUID"};
        static const char* unt[4]  = {"%","%","%","%"};
        std::memcpy(fMin,mins,sizeof(mins));
        std::memcpy(fMax,maxs,sizeof(maxs));
        std::memcpy(fValue,defs,sizeof(defs));
        std::memcpy(fLabel,lbl,sizeof(lbl));
        std::memcpy(fUnit,unt,sizeof(unt));
        std::memset(fScope,0,sizeof(fScope));
        fScopePos=0;fDrag=-1;fLastY=0.f;fProg=0;
        applyUiPreset(0);
        for(int p=0;p<4;++p)fValue[p]=fTarget[p];
    }
protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if(index<4) fTarget[index]=value;
        else if(index==5){
            fProg=(int)(value+0.5f);if(fProg<0)fProg=0;if(fProg>9)fProg=9;
            for(int p=0;p<4;++p)fTarget[p]=kUiPresetParams[fProg][p];
        }
        else if(index==6) fOn=value;
        else if(index==7) fKracht=value;
        else if(index==9) fLufsM=value;
        else if(index==10) fLufsI=value;
        else if(index==11){fScope[fScopePos]=value;fScopePos=(fScopePos+1)%kScopeSize;}
        else if(index==12) fTarget[0]=value;
        else if(index==13) fTarget[1]=value;
        else if(index==14) fTarget[2]=value;
        else if(index==15) fTarget[3]=value;
        else if(index==16) fAutoB=value;
        else if(index==17) fDoelU=value;
        repaint();
    }

    void onNanoDisplay() override
    {
        const float W=getWidth(),H=getHeight();
        beginPath();rect(0,0,W,H);fillColor(g2bsk(fLight,0));fill();

        fontSize(22.0f);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        fillColor(g2bsk(fLight,5));
        text(18.0f,22.0f,"G2B MASTER",nullptr);
        fontSize(9.0f);fillColor(g2bsk(fLight,3));
        text(20.0f,40.0f,"v2 · mastering in 4 knoppen + mix-bus — kies je genre",nullptr);

        // BUS-indicator rechts naast titel
        fontSize(9.0f);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        if(fAutoB>0.5f){fillColor(g2bsk(fLight,5));text(200.0f,22.0f,"● BUS: sporen zichtbaar",nullptr);}
        else{fillColor(g2bsk(fLight,3));text(200.0f,22.0f,"○ BUS: geen mixers",nullptr);}
        // preset-pills (2 rijen van 5)
        for(int i=0;i<kNumPrograms;++i){
            const int row=i/5,col=i%5;
            const float bw=86.0f,bh=20.0f,gap=8.0f;
            const float rowW=5*bw+4*gap;
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

        // scope
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

        // LUFS-uitlezing in scope
        {
            char lb[64];
            std::snprintf(lb,sizeof(lb),"LUFS %.1f  |  %.1f",fLufsM,fLufsI);
            fontSize(10.5f);textAlign(ALIGN_RIGHT|ALIGN_TOP);
            fillColor(g2bsk(fLight,2));
            text(sx+sw-8.0f,sy+5.0f,lb,nullptr);
            fontSize(8.5f);fillColor(g2bsk(fLight,3));
            text(sx+sw-8.0f,sy+17.0f,"live    gemiddeld",nullptr);
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
            if(i==0)std::snprintf(buf,sizeof(buf),"%+.0f %%",fValue[i]);
            else std::snprintf(buf,sizeof(buf),"%.0f %%",fValue[i]);
            text(cx,cy+R+26.0f,buf,nullptr);
        }

        // LUFS-blok rechts
        {
            const float mx=W-120.0f,my=170.0f,mw=102.0f,mh=110.0f;
            beginPath();roundedRect(mx,my,mw,mh,8.0f);
            fillColor(g2bsk(fLight,1));fill();
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            fontSize(11.0f);textAlign(ALIGN_CENTER|ALIGN_TOP);
            fillColor(g2bsk(fLight,3));
            text(mx+mw*0.5f,my+8.0f,"LEVEL",nullptr);
            // meter-balk op momentary (-24..0)
            float mm=fLufsM;if(mm<-24.f)mm=-24.f;if(mm>0.f)mm=0.f;
            const float fh=(mm+24.f)/24.f;
            const float bx=mx+14.0f,bw2=14.0f,bh2=mh-40.0f;
            beginPath();rect(bx,my+22.0f,bw2,bh2);
            fillColor(fLight==4?Color(30,34,38):Color(8,10,9));fill();
            beginPath();rect(bx,my+22.0f+bh2*(1.f-fh),bw2,bh2*fh);
            fillColor(fh>0.85f?Color(220,80,60):g2bsk(fLight,5));fill();
            // doel-lijn op -6 LUFS (zijn richtlijn)
            const float gy=my+22.0f+bh2*(1.f-((fDoelU+24.f)/24.f));
            beginPath();rect(bx-3.0f,gy,bw2+6.0f,1.5f);
            fillColor(230,200,80);fill();
            char lb[32];
            std::snprintf(lb,sizeof(lb),"%.1f LUFS",fLufsM);
            fontSize(11.0f);fillColor(g2bsk(fLight,2));
            text(mx+mw*0.5f+12.0f,my+30.0f,lb,nullptr);
            fontSize(8.5f);fillColor(g2bsk(fLight,3));
            {char db[16];std::snprintf(db,sizeof(db),"doel %.0f",fDoelU);text(mx+mw*0.5f+12.0f,my+48.0f,db,nullptr);}
        }

        // KRACHT dry/wet — kleine horizontale slider onder scope
        {
            const float kx=90.0f,ky=150.0f,kw=180.0f,kh=8.0f;
            fontSize(9.5f);fillColor(g2bsk(fLight,3));
            textAlign(ALIGN_LEFT|ALIGN_TOP);
            char kb[24];std::snprintf(kb,sizeof(kb),"KRACHT %.0f %%",fKracht);
            text(20.0f,148.0f,kb,nullptr);
            beginPath();roundedRect(kx,ky,kw,kh,4.0f);
            fillColor(g2bsk(fLight,1));fill();
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            beginPath();roundedRect(kx,ky,kw*(fKracht/100.f),kh,4.0f);
            fillColor(g2bsk(fLight,5));fill();
        }

        fontSize(10.5f);fillColor(g2bsk(fLight,3));
        textAlign(ALIGN_CENTER|ALIGN_BOTTOM);
        text(W*0.5f,H-8.0f,"zet op je master-track: preset kiezen → beat klinkt gelijk luider + strakker",nullptr);

        // DICE + skin + AAN
        {
            const float dy=146.0f;
            beginPath();roundedRect(W-120.0f,dy,44.0f,18.0f,4.0f);
            fillColor(g2bsk(fLight,1));fill();
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            fontSize(9.0f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(g2bsk(fLight,2));
            text(W-98.0f,dy+9.0f,"DICE",nullptr);

            // AUTO — luistert ~5s en zet knoppen zelf
            beginPath();roundedRect(W-68.0f,dy,44.0f,18.0f,4.0f);
            fillColor(fAutoB>0.5f?g2bsk(fLight,5):g2bsk(fLight,1));fill();
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            fontSize(9.0f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(fAutoB>0.5f?g2bsk(fLight,0):g2bsk(fLight,2));
            text(W-46.0f,dy+9.0f,fAutoB>0.5f?"LUIST..":"AUTO",nullptr);

            const float tx=W-40.0f;
            beginPath();roundedRect(tx,34.0f,26.0f,17.0f,4.0f);
            fillColor(g2bsk(fLight,1));fill();
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            {   // zon/maan
                const float cx=tx+13.0f,cy=42.5f;
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
            if(px>=W-68.0f&&px<=W-24.0f&&py>=146.0f&&py<=164.0f)
            {setParameterValue(8,1.0f);fBtnHeld=8;;repaint();return true;}
            if(px>=W-40.0f&&px<=W-14.0f&&py>=34.0f&&py<=51.0f)
            {fLight=(fLight+1)%8;repaint();return true;}
            if(px>=W-120.0f&&px<=W-18.0f&&py>=170.0f&&py<=280.0f)
            {fDragDoel=true;return true;}
            if(px>=90.0f&&px<=270.0f&&py>=146.0f&&py<=166.0f)
            {float v=(px-90.0f)/180.0f*100.f;if(v<0)v=0;if(v>100)v=100;
             fKracht=v;setParameterValue(7,v);repaint();fDragKr=true;return true;}
            for(int i=0;i<kNumPrograms;++i){
                const int row=i/5,col=i%5;
                const float bw=86.0f,bh=20.0f,gap=8.0f;
                const float rowW=5*bw+4*gap;
                const float bx=(W-rowW)*0.5f+col*(bw+gap);
                const float by=row==0?58.0f:82.0f;
                if(px>=bx&&px<=bx+bw&&py>=by&&py<=by+bh)
                {applyUiPreset(i);return true;}
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
        else if(fDragDoel){const float py=(float)(ev.pos.getY()/getScaleFactor());
            float v=-24.f+(192.0f+70.0f-py)/70.0f*21.f;if(v>-3.f)v=-3.f;if(v<-24.f)v=-24.f;
            fDoelU=v;setParameterValue(17,v);repaint();return true;}
        if(fDragKr){fDragKr=false;}
        else if(fDragDoel){fDragDoel=false;}
        return true;
    }
    bool onMotion(const MotionEvent& ev) override
    {
        if(fDragKr){
            const float px=(float)(ev.pos.getX()/getScaleFactor());
            float v=(px-90.0f)/180.0f*100.f;if(v<0)v=0;if(v>100)v=100;
            fKracht=v;setParameterValue(7,v);repaint();return true;
        }
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
    void applyUiPreset(int i){
        if(i<0)i=0;if(i>9)i=9;
        fProg=i;
        setParameterValue(5,(float)i);
        for(int p=0;p<4;++p){fTarget[p]=kUiPresetParams[i][p];setParameterValue(p,fTarget[p]);}
        repaint();
    }
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
    float fOn=1.f,fKracht=100.f,fAutoB=0.f;
    float fScope[kScopeSize];int fScopePos;
    float fLufsM=-99.0f,fLufsI=-99.0f;
    int fDrag;float fLastY;int fProg;int fLight=0;bool fDragKr=false;bool fDragDoel=false;float fDoelU=-6.f;
    int fBtnHeld=-1;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BMasterUI)
};

UI* createUI() { return new G2BMasterUI(); }

END_NAMESPACE_DISTRHO
