/*
 * G2B Drums — UI (NanoVG): 4 knoppen + preset-rij + 16-staps LED-grid + scope.
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
static inline Color g2bsk(int sk,int r){if(sk<0)sk=0;if(sk>7)sk=7;const float*c=G2BSK[sk&7][r];return Color((int)c[0],(int)c[1],(int)c[2]);}

START_NAMESPACE_DISTRHO

static const int kNumPrograms = 17;
static const char* kProgramNames[kNumPrograms] = {
    "G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"
};
// patroon-LED's per rij (K/S/H/P) — zelfde patronen als DSP
static const char* kPatternRows[4] = {
    "K...K...K..K...","....S.......S...","H.H.H.H.H.H.H.H.","............P..."
};

class DrumsUI : public UI
{
public:
    DrumsUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        loadSharedResources();
        static const float mins[7]  = {0,0,0,0,0,0,0};
        static const float maxs[7]  = {1,1,0.5f,1,100,26,2};
        static const float defs[7]  = {0,0.5f,0.10f,0.85f,100,9,1};
        static const char* lbl[7]   = {"PATROON","DRIVE","SWING","LEVEL","KRACHT","DIMENTIES","SFEER"};
        std::memcpy(fMin,mins,sizeof(mins));
        std::memcpy(fMax,maxs,sizeof(maxs));
        std::memcpy(fValue,defs,sizeof(defs));
        std::memcpy(fLabel,lbl,sizeof(lbl));
        std::memset(fScope,0,sizeof(fScope));
        fScopePos=0;fDrag=-1;fLastY=0;fProg=-1;fStep=-1;
        for(int r=0;r<4;r++)for(int c=0;c<16;c++)fPatGrid[r][c]=kPatternRows[r][c];
    }
protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if(index<4)fValue[index]=value;
        else if(index==5)fAuto=value;
        else if(index==6)fProg=(int)(value+0.5f);
        else if(index==7)fOn=(int)(value+0.5f);
        else if(index>=8&&index<=10)fValue[index-4]=value; // 8=KRACHT→4,9=DIMENTIES→5,10=SFEER→6
        repaint();
    }
    void stateChanged(const char* key, const char* value) override
    {
        if(std::strcmp(key,"pat")==0){
            int r=0,c=0;
            for(const char* p=value;*p&&r<4;++p){
                if(*p==';'){r++;c=0;continue;}
                if(c<16)fPatGrid[r][c++]=*p;
            }
            repaint();return;
        }
        if(std::strcmp(key,"scope")==0){
            const char* p=value;
            while(*p){
                char* end=nullptr;
                const float v=std::strtof(p,&end);
                if(end==p)break;
                fScope[fScopePos]=v;fScopePos=(fScopePos+1)%kScopeSize;
                p=(*end==';')?end+1:end;
            }
            repaint();
        } else if(std::strcmp(key,"step")==0){
            fStep=atoi(value);repaint();
        }
    }
    void onNanoDisplay() override
    {
        const float W=getWidth(),H=getHeight();
        const Color BG=g2bsk(fLight,0);const Color TXT=g2bsk(fLight,2);const Color DIMC=g2bsk(fLight,3);
        beginPath();rect(0,0,W,H);fillColor(BG);fill();
        fontSize(20.0f);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        fillColor(47,191,113);
        text(18.0f,22.0f,"G2B DRUMS",nullptr);
        fontSize(11.5f);fillColor(143,160,152);
        text(18.0f,41.0f,"drum-machine: jouw stijl-patroon, gesyncd met je DAW",nullptr);

        // presets rechtsboven: rijen van 8
        for(int i=0;i<kNumPrograms;++i){
            const float bw=44.0f,bh=17.0f;
            const int prow=i/8, pcol=i%8;
            const float px=W-48.0f-((prow<2?8:1)-pcol)*(bw+4.0f), py=12.0f+prow*18.0f;
            const bool on=(i==fProg);
            beginPath();roundedRect(px,py,bw,bh,4.0f);
            if(on){fillColor(29,122,72);fill();}
            strokeColor(42,47,45);strokeWidth(1.0f);stroke();
            fontSize(8.0f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(on?Color(232,236,234):Color(143,160,152));
            text(px+bw*0.5f,py+bh*0.5f,kProgramNames[i],nullptr);
        }

        // ☀/🌙 + DICE + AUTO knoppen linksboven naast titel
        {
            const float tx=W-40.0f,ty=34.0f;
            beginPath();roundedRect(tx,ty,26.0f,17.0f,4.0f);
            fillColor(g2bsk(fLight,1));fill();
            strokeColor(42,47,45);strokeWidth(1.0f);stroke();
            fontSize(10.0f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(g2bsk(fLight,2));
            text(tx+13.0f,ty+8.5f,fLight>0?"🌙":"☀",nullptr);
            float bx2=14.0f;
            { // DICE
                beginPath();roundedRect(bx2,50.0f,26.0f,17.0f,4.0f);
                fillColor(g2bsk(fLight,1));fill();
                strokeColor(42,47,45);strokeWidth(1.0f);stroke();
                fontSize(11.0f);fillColor(g2bsk(fLight,2));
                text(bx2+13.0f,58.5f,"🎲",nullptr);
            }
            bx2+=30.0f;
            { // AUTO
                beginPath();roundedRect(bx2,50.0f,38.0f,17.0f,4.0f);
                if(fAuto>0.5f){fillColor(47,191,113);fill();}
                else{fillColor(g2bsk(fLight,1));fill();}
                strokeColor(42,47,45);strokeWidth(1.0f);stroke();
                fontSize(9.5f);fillColor(fAuto>0.5f?Color(13,15,14):Color(143,160,152));
                text(bx2+19.0f,58.5f,"AUTO",nullptr);
            }
        }

        // 16-staps LED-grid (4 rijen: K S H P)
        const float gx=18.0f, gy=74.0f, gw=W-36.0f;
        const char rowN[4]={'K','S','H','P'};
        for(int r=0;r<4;r++){
            for(int s=0;s<16;s++){
                const float cw=gw/16.0f;
                const float x=gx+s*cw, y=gy+r*16.0f;
                const bool hit=(fPatGrid[r][s]!='.');
                const bool now=(s==fStep);
                beginPath();rect(x+1.5f,y+1.5f,cw-3.0f,12.0f);
                if(hit) fillColor(now?Color(232,236,234):Color(47,191,113));
                else    fillColor(now?Color(29,122,72):Color(24,28,27));
                fill();
            }
            fontSize(9.0f);fillColor(90,100,94);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
            text(gx-2.0f,gy+r*16.0f+7.0f,&rowN[r],nullptr); // rij-letter links (valt deels buiten grid, ok)
        }

        // scope onder grid
        const float sx=18.0f,sy=132.0f,sw=W-36.0f,sh=36.0f;
        beginPath();rect(sx,sy,sw,sh);fillColor(8,10,9);fill();
        strokeColor(42,47,45);strokeWidth(1.0f);stroke();
        beginPath();
        const float mid=sy+sh*0.5f;
        for(int i=0;i<kScopeSize-1;++i){
            const int i0=(fScopePos+i)%kScopeSize,i1=(fScopePos+i+1)%kScopeSize;
            const float x0=sx+(float)i/(kScopeSize-1)*sw,x1=sx+(float)(i+1)/(kScopeSize-1)*sw;
            float y0=mid-fScope[i0]*sh*0.45f,y1=mid-fScope[i1]*sh*0.45f;
            if(y0<sy+1)y0=sy+1;if(y0>sy+sh-1)y0=sy+sh-1;
            if(y1<sy+1)y1=sy+1;if(y1>sy+sh-1)y1=sy+sh-1;
            if(i==0)moveTo(x0,y0);else lineTo(x0,y0);lineTo(x1,y1);
        }
        strokeColor(47,191,113);strokeWidth(1.5f);stroke();

        // AAN/UIT pil rechtsboven
        {
            const float ax=W-48.0f,ay=12.0f;
            beginPath();roundedRect(ax,ay,44.0f,17.0f,4.0f);
            fillColor(fOn?g2bsk(fLight,5):g2bsk(fLight,4));fill();
            strokeColor(g2bsk(fLight,3));strokeWidth(1.0f);stroke();
            fontSize(8.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(fOn?Color(232,236,234):g2bsk(fLight,3));
            text(ax+22.0f,ay+8.5f,fOn?"AAN":"UIT",nullptr);
        }

        // 7 knoppen
        const int n=7;
        const float step=(W-40.0f)/n;
        for(int i=0;i<n;++i){
            const float cx=20.0f+step*(i+0.5f),cy=218.0f,R=26.0f;
            const float norm=(fValue[i]-fMin[i])/(fMax[i]-fMin[i]);
            const float ang=-2.4f+norm*4.8f;
            beginPath();circle(cx,cy,R);fillColor(30,34,33);fill();
            strokeColor(42,47,45);strokeWidth(2.0f);stroke();
            beginPath();arc(cx,cy,R-4.0f,-2.4f-1.5708f,ang-1.5708f,CW);
            strokeColor(47,191,113);strokeWidth(3.0f);stroke();
            beginPath();moveTo(cx,cy);
            lineTo(cx+std::sin(ang)*(R-8.0f),cy-std::cos(ang)*(R-8.0f));
            strokeColor(232,236,234);strokeWidth(3.0f);stroke();
            fontSize(10.5f);fillColor(143,160,152);textAlign(ALIGN_CENTER|ALIGN_TOP);
            text(cx,cy+R+8.0f,fLabel[i],nullptr);
            fontSize(12.0f);fillColor(232,236,234);
            char buf[24];
            if(i==0)std::snprintf(buf,sizeof(buf),"%s",kProgramNames[(int)fValue[0]<0?0:((int)fValue[0]>16?16:(int)fValue[0])]);
            else if(i==4)std::snprintf(buf,sizeof(buf),"%.0f%%",fValue[i]);
            else if(i==5)std::snprintf(buf,sizeof(buf),"%.0f/26",fValue[i]);
            else if(i==6)std::snprintf(buf,sizeof(buf),"%s",(int)(fValue[i]+0.5f)==0?"DONKER":((int)(fValue[i]+0.5f)==2?"LICHT":"NEUTR"));
            else std::snprintf(buf,sizeof(buf),"%.0f %%",fValue[i]*100.0f);
            text(cx,cy+R+23.0f,buf,nullptr);
        }

        // XY-pad: X = KRACHT (0-100), Y = DIMENTIES (0-26)
        fXX=18.0f;fXY_=286.0f;fXW=W-36.0f;fXH=58.0f;
        {
            const float px=fXX,py=fXY_,pw=fXW,ph=fXH;
            beginPath();roundedRect(px,py,pw,ph,6.0f);
            fillColor(g2bsk(fLight,4));fill();
            strokeColor(g2bsk(fLight,3));strokeWidth(1.0f);stroke();
            const float nx=fValue[4]/100.0f, ny=fValue[5]/26.0f;
            const float dx=px+nx*pw, dy=py+(1.0f-ny)*ph;
            beginPath();
            moveTo(dx,py);lineTo(dx,py+ph);
            moveTo(px,dy);lineTo(px+pw,dy);
            strokeColor(g2bsk(fLight,3));strokeWidth(0.5f);stroke();
            beginPath();circle(dx,dy,5.0f);
            fillColor(g2bsk(fLight,5));fill();
            fontSize(9.0f);fillColor(g2bsk(fLight,3));
            textAlign(ALIGN_LEFT|ALIGN_TOP);
            text(px+6.0f,py+4.0f,"DIMENTIES ",nullptr);
            textAlign(ALIGN_RIGHT|ALIGN_BOTTOM);
            text(px+pw-6.0f,py+ph-4.0f,"KRACHT →",nullptr);
        }
        fontSize(10.5f);fillColor(90,100,94);textAlign(ALIGN_CENTER|ALIGN_BOTTOM);
        text(W*0.5f,H-8.0f,"groen = stap die klinkt · wit = nu · speelt zodra je DAW loopt",nullptr);
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if(ev.button!=1)return false;
        const float W=getWidth();
        if(ev.press){
            if((float)(ev.pos.getX()/getScaleFactor())>=W-40.0f&&(float)(ev.pos.getX()/getScaleFactor())<=W-14.0f&&(float)(ev.pos.getY()/getScaleFactor())>=34.0f&&(float)(ev.pos.getY()/getScaleFactor())<=51.0f){
                fLight=(fLight+1)%8;repaint();return true;
            }
            if((float)(ev.pos.getX()/getScaleFactor())>=14.0f&&(float)(ev.pos.getX()/getScaleFactor())<=40.0f&&(float)(ev.pos.getY()/getScaleFactor())>=50.0f&&(float)(ev.pos.getY()/getScaleFactor())<=67.0f){
                spv(4,1.0f);repaint();return true;
            }
            if((float)(ev.pos.getX()/getScaleFactor())>=44.0f&&(float)(ev.pos.getX()/getScaleFactor())<=82.0f&&(float)(ev.pos.getY()/getScaleFactor())>=50.0f&&(float)(ev.pos.getY()/getScaleFactor())<=67.0f){
                fAuto=fAuto>0.5f?0.f:1.f;spv(5,fAuto);repaint();return true;
            }
            // AAN/UIT pil
            if((float)(ev.pos.getX()/getScaleFactor())>=W-48.0f&&(float)(ev.pos.getX()/getScaleFactor())<=W-4.0f&&(float)(ev.pos.getY()/getScaleFactor())>=12.0f&&(float)(ev.pos.getY()/getScaleFactor())<=29.0f){
                fOn=1-fOn;spv(7,(float)fOn);repaint();return true;
            }
            // XY-pad
            if((float)(ev.pos.getX()/getScaleFactor())>=fXX&&(float)(ev.pos.getX()/getScaleFactor())<=fXX+fXW&&(float)(ev.pos.getY()/getScaleFactor())>=fXY_&&(float)(ev.pos.getY()/getScaleFactor())<=fXY_+fXH){
                fXYdrag=true;xySet((float)(ev.pos.getX()/getScaleFactor()),(float)(ev.pos.getY()/getScaleFactor()));return true;
            }
            for(int i=0;i<kNumPrograms;++i){
                const float bw=44.0f,bh=17.0f;
                const int prow=i/8, pcol=i%8;
                const float px=W-48.0f-((prow<2?8:1)-pcol)*(bw+4.0f), py=12.0f+prow*18.0f;
                if((float)(ev.pos.getX()/getScaleFactor())>=px&&(float)(ev.pos.getX()/getScaleFactor())<=px+bw&&(float)(ev.pos.getY()/getScaleFactor())>=py&&(float)(ev.pos.getY()/getScaleFactor())<=py+bh){
                    spv(6,(float)i);fProg=i;repaint();return true;
                }
            }
        }
        else if(fXYdrag){fXYdrag=false;return true;}
        const float step=(W-40.0f)/7;
        if(ev.press){
            for(int i=0;i<7;++i){
                const float cx=20.0f+step*(i+0.5f),cy=218.0f;
                const float dx=(float)(ev.pos.getX()/getScaleFactor())-cx,dy=(float)(ev.pos.getY()/getScaleFactor())-cy;
                if(dx*dx+dy*dy<=26.0f*26.0f+80.0f){fDrag=i;fLastY=(float)(ev.pos.getY()/getScaleFactor());editParameter(paramOf(i),true);return true;}
            }
        } else if(fDrag>=0){editParameter(paramOf(fDrag),false);fDrag=-1;}
        return true;
    }

    // knop-slots: 0 PATROON,1 DRIVE,2 SWING,3 LEVEL,4 KRACHT(8),5 DIMENTIES(9),6 SFEER(10)
    static int paramOf(int slot){ return slot<4?slot:slot+4; }
    void xySet(double mx,double my){
        float nx=(float)((mx-fXX)/fXW), ny=(float)(1.0-(my-fXY_)/fXH);
        if(nx<0)nx=0;if(nx>1)nx=1;if(ny<0)ny=0;if(ny>1)ny=1;
        const float kr=nx*100.0f, dm=ny*26.0f;
        fValue[4]=kr;fValue[5]=dm;
        editParameter(8,true);spv(8,kr);editParameter(8,false);
        editParameter(9,true);spv(9,dm);editParameter(9,false);
        repaint();
    }
    bool onMotion(const MotionEvent& ev) override
    {
        if(fXYdrag){xySet((float)(ev.pos.getX()/getScaleFactor()),(float)(ev.pos.getY()/getScaleFactor()));return true;}
        if(fDrag<0)return false;
        const float dy=fLastY-(float)(ev.pos.getY()/getScaleFactor());fLastY=(float)(ev.pos.getY()/getScaleFactor());
        const float range=fMax[fDrag]-fMin[fDrag];
        float v=fValue[fDrag]+dy*range/200.0f;
        if(v<fMin[fDrag])v=fMin[fDrag];if(v>fMax[fDrag])v=fMax[fDrag];
        fValue[fDrag]=v;spv(paramOf(fDrag),v);repaint();return true;
    }
    bool onScroll(const ScrollEvent& ev) override
    {
        const float W=getWidth();const float step=(W-40.0f)/7;
        for(int i=0;i<7;++i){
            const float cx=20.0f+step*(i+0.5f),cy=218.0f;
            const float dx=(float)(ev.pos.getX()/getScaleFactor())-cx,dy=(float)(ev.pos.getY()/getScaleFactor())-cy;
            if(dx*dx+dy*dy<=26.0f*26.0f+80.0f){
                const float range=fMax[i]-fMin[i];
                float v=fValue[i]+ev.delta.getY()*range/100.0f;
                if(v<fMin[i])v=fMin[i];if(v>fMax[i])v=fMax[i];
                fValue[i]=v;const int pi=paramOf(i);editParameter(pi,true);spv(pi,v);editParameter(pi,false);
                repaint();return true;
            }
        }
        return true;
    }
private:
    static const int kScopeSize=256;
    float fValue[7],fMin[7],fMax[7];
    const char* fLabel[7];
    float fScope[kScopeSize];int fScopePos;
    int fDrag;float fLastY;int fProg;int fStep;float fAuto=0;int fLight=0;
    int fOn=1; bool fXYdrag=false;
    float fXX=18.0f,fXY_=286.0f,fXW=524.0f,fXH=58.0f;
    char fPatGrid[4][16];
    void spv(uint32_t i,float v){setParameterValue(i,v);parameterChanged(i,v);}
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumsUI)
};
UI* createUI(){return new DrumsUI();}
END_NAMESPACE_DISTRHO
