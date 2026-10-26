/* G2B PADS — UI: genre-bank kiezer + 4x4 pad-grid + drum-knippen. */
#include "pads_names.h"
#include "DistrhoUI.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>

static const float G2BSK[9][7][3]={
    {{13,15,14},{23,26,25},{232,236,234},{100,110,103},{42,47,45},{47,191,113},{29,122,72}},
    {{14,14,22},{22,22,34},{230,232,255},{110,110,150},{40,40,58},{120,110,255},{70,60,170}},
    {{10,20,22},{16,30,32},{225,245,245},{95,130,130},{30,55,55},{40,200,210},{20,120,130}},
    {{24,14,20},{34,20,28},{250,230,240},{150,105,125},{60,38,50},{255,110,180},{180,60,120}},
    {{228,230,232},{240,242,244},{30,34,38},{105,112,118},{205,210,214},{40,140,100},{20,100,70}},
    {{18,10,10},{28,16,16},{240,225,220},{140,100,95},{55,35,32},{200,80,60},{130,45,35}},
    {{14,14,10},{22,22,16},{245,240,220},{120,115,95},{50,48,35},{230,190,80},{160,125,45}},
    {{12,14,20},{20,24,32},{235,240,250},{105,115,135},{36,44,58},{80,160,255},{45,110,200}},
    {{8,20,14},{14,30,20},{240,245,240},{130,155,140},{30,60,40},{220,40,60},{245,205,40}}};
static const char* G2BSKN[9]={"MELIOR","SPECTRUM","LAGOON","CANDY","GHOST","MAROON","GOLD","STUDIO","SURI"};
static inline Color g2bsk(int sk,int r){if(sk<0)sk=0;if(sk>8)sk=8;const float*c=G2BSK[sk][r];return Color((int)c[0],(int)c[1],(int)c[2]);}

START_NAMESPACE_DISTRHO

class PadsUI : public UI {
public:
    PadsUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT) {
        loadSharedResources();
        const float mins[6]={-12,0.05f,-12,-12,-24,0};
        const float maxs[6]={12,2,12,12,12,26};
        const float defs[6]={0,1,0,0,0,9};
        const char* lbl[6]={"TUNE","DECAY","LO","HI","OUTPUT","DIMENTIES"};
        std::memcpy(fMin,mins,sizeof(mins)); std::memcpy(fMax,maxs,sizeof(maxs));
        std::memcpy(fValue,defs,sizeof(defs)); std::memcpy(fLabel,lbl,sizeof(lbl));
        fDrag=-1; fLastY=0.f;
    }
protected:
    // params: 0 BANK,1 TUNE,2 DECAY,3 LO,4 HI,5 LEVEL,6 AAN,7 GALAXY,8 SFEER,9 DICE
    static int paramOf(int slot){ return slot==0?1:(slot==1?2:(slot==2?3:(slot==3?4:(slot==4?5:7)))); }
    void parameterChanged(uint32_t i, float v) override {
        if(i==0) fBank=(int)(v+0.5f);
        else if(i>=1&&i<=5) fValue[i-1]=v;
        else if(i==6) fOn=(int)(v+0.5f);
        else if(i==7) fValue[5]=v;
        else if(i==8) fSfeer=(int)(v+0.5f);
        repaint();
    }
    void stateChanged(const char* key, const char* value) override {
        if(std::strcmp(key,"scope")==0){
            const char* p=value;
            while(*p&&*p!='|'){char* e=nullptr;float v=strtof(p,&e);if(e==p)break;fScope[fScopePos]=v;fScopePos=(fScopePos+1)%kScopeSize;p=(*e==';')?e+1:e;}
            if(*p=='|'){p++;for(int i=0;i<16;i++){char* e=nullptr;float v=strtof(p,&e);if(e==p)break;fHit[i]=v;p=(*e==';')?e+1:e;}}
            repaint();
        }
    }
    void onNanoDisplay() override {
        const float W=getWidth(),H=getHeight();
        const int skin=fSkin;
        beginPath(); rect(0,0,W,H); fillColor(g2bsk(skin,0)); fill();
        fontSize(20.f); textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        fillColor(g2bsk(skin,5)); text(18.f,22.f,"G2B SURINAAMS",nullptr);
        fontSize(11.5f); fillColor(g2bsk(skin,3));
        text(18.f,41.f,"jouw eigen Surinaamse klanken — 9 banks x 16 pads",nullptr);
        for(int i=0;i<9;i++){
            float dx=W-18.f-(8-i)*14.f, dy=18.f;
            beginPath(); circle(dx,dy,4.5f);
            fillColor(g2bsk(i,5)); fill();
            if(i==skin){strokeColor(g2bsk(skin,2));strokeWidth(1.2f);stroke();}
        }
        // bank-kiezer (smaller, pills rechts ervan)
        float ix=18.f, iy=54.f, iw=W-36.f-80.f, ih=46.f;
        beginPath(); roundedRect(ix,iy,iw,ih,6.f);
        fillColor(g2bsk(skin,1)); fill(); strokeColor(g2bsk(skin,4)); strokeWidth(1.f); stroke();
        fontSize(22.f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
        fillColor(g2bsk(skin,5));
        text(ix+24.f,iy+ih/2,"<",nullptr); text(ix+iw-24.f,iy+ih/2,">",nullptr);
        fontSize(22.f); fillColor(g2bsk(skin,2));
        const char* bn=(fBank>=0&&fBank<kBankNamesUI_N)?kBankNamesUI[fBank]:"BANK";
        text(ix+iw/2,iy+18.f,bn,nullptr);
        fontSize(9.5f); fillColor(g2bsk(skin,3));
        char nb[48]; snprintf(nb,sizeof nb,"%d / %d",fBank+1,kBankNamesUI_N);
        text(ix+iw/2,iy+36.f,nb,nullptr);
        // pills rechts: DICE / SFEER / AAN
        float px=W-66.f;
        for(int k=0;k<3;k++){
            float py=54.f+k*25.f;
            beginPath(); roundedRect(px,py,52.f,20.f,10.f);
            if(k==2){fillColor(fOn?g2bsk(skin,5):g2bsk(skin,4));fill();fillColor(fOn?g2bsk(skin,0):g2bsk(skin,2));fontSize(10.f);text(px+26.f,py+10.f,"AAN",nullptr);}
            else{fillColor(g2bsk(skin,1));fill();strokeColor(g2bsk(skin,4));strokeWidth(1.f);stroke();fillColor(g2bsk(skin,3));fontSize(9.f);
                 text(px+26.f,py+10.f,k==0?"DICE":(fSfeer==0?"DARK":(fSfeer==2?"LICHT":"NEUTR")),nullptr);}
        }
        // pad-grid 4x4
        float gx=18.f, gy=110.f, gw=iw, gh=110.f;
        float cw=gw/4.f, ch=gh/4.f;
        const char** padNames=(fBank>=0&&fBank<kBankNamesUI_N)?kPadNamesAll[fBank]:nullptr;
        for(int r=0;r<4;r++)for(int c=0;c<4;c++){
            int idx=r*4+c;
            float x0=gx+c*cw+2.f, y0=gy+r*ch+2.f;
            bool hit=fHit[idx]>0.05f;
            beginPath(); roundedRect(x0,y0,cw-4.f,ch-4.f,4.f);
            fillColor(hit?g2bsk(skin,5):g2bsk(skin,1)); fill();
            strokeColor(g2bsk(skin,4)); strokeWidth(1.f); stroke();
            fontSize(8.5f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(hit?g2bsk(skin,0):g2bsk(skin,3));
            if(padNames) text(x0+(cw-4.f)/2,y0+(ch-4.f)/2,padNames[idx],nullptr);
            fontSize(7.f); fillColor(hit?g2bsk(skin,0):g2bsk(skin,4));
            char nn[8]; snprintf(nn,sizeof nn,"%d",36+idx);
            text(x0+(cw-4.f)/2,y0+ch-8.f,nn,nullptr);
        }
        // scope mini rechts van grid
        float sx=gx+gw+8.f, sy=gy, sw=W-18.f-sx, sh=gh;
        beginPath(); rect(sx,sy,sw,sh); fillColor(g2bsk(skin,1)); fill();
        strokeColor(g2bsk(skin,4)); strokeWidth(1.f); stroke();
        beginPath();
        for(int i=0;i<kScopeSize;i++){
            int idx=(fScopePos+i)%kScopeSize;
            float x=sx+sw*i/(float)kScopeSize, y=sy+sh/2 - fScope[idx]*sh*0.45f;
            if(i==0)moveTo(x,y);else lineTo(x,y);
        }
        strokeColor(g2bsk(skin,5)); strokeWidth(1.2f); stroke();
        // 6 knoppen
        const int nk=6; float step=(W-40)/nk;
        for(int i=0;i<nk;i++){
            float cx=20+step*(i+0.5f), cy=246.f, r=18.f;
            drawKnob(cx,cy,r,fMin[i],fMax[i],fValue[i],skin);
            fontSize(8.5f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(g2bsk(skin,3)); text(cx,cy+r+10.f,fLabel[i],nullptr);
            char vb[24];
            if(i==0)snprintf(vb,sizeof vb,"%+.0fst",fValue[i]);
            else if(i==1)snprintf(vb,sizeof vb,"%.2f",fValue[i]);
            else if(i>=2&&i<=4)snprintf(vb,sizeof vb,"%+.0f",fValue[i]);
            else if(i==5)snprintf(vb,sizeof vb,"%.0f/27",fValue[i]+1.f);
            else snprintf(vb,sizeof vb,"%.1f",fValue[i]);
            fillColor(g2bsk(skin,2)); text(cx,cy+r+20.f,vb,nullptr);
        }
        fontSize(9.f); textAlign(ALIGN_LEFT|ALIGN_MIDDLE); fillColor(g2bsk(skin,3));
        text(18.f,H-6.f,G2BSKN[skin],nullptr);
    }
    bool onMouse(const MouseEvent& ev) override {
        const float W=getWidth(),H=getHeight();
        if(ev.press){
            float x=ev.pos.getX(),y=ev.pos.getY();
            for(int i=0;i<9;i++){float dx=W-18.f-(8-i)*14.f;if(std::fabs(x-dx)<8&&std::fabs(y-18)<10){fSkin=i;repaint();return true;}}
            float ix=18.f,iy=54.f,iw=W-36.f-80.f,ih=46.f;
            if(y>=iy&&y<=iy+ih){
                if(x<ix+48){setParameterValue(0,fBank<=0?kBankNamesUI_N-1:fBank-1);return true;}
                if(x>ix+iw-48){setParameterValue(0,fBank>=kBankNamesUI_N-1?0:fBank+1);return true;}
            }
            float px=W-66.f;
            if(x>=px&&x<=px+52){
                if(y>=54&&y<=74){setParameterValue(9,1.f);return true;}
                if(y>=79&&y<=99){setParameterValue(8,(float)((fSfeer+1)%3));return true;}
                if(y>=104&&y<=124){setParameterValue(6,fOn?0.f:1.f);return true;}
            }
            const int nk=6;float step=(W-40)/nk;
            for(int i=0;i<nk;i++){float cx=20+step*(i+0.5f),cy=246.f;
                if(std::fabs(x-cx)<24&&std::fabs(y-cy)<24){fDrag=i;fLastY=y;return true;}}
        } else fDrag=-1;
        return false;
    }
    bool onMotion(const MotionEvent& ev) override {
        if(fDrag>=0){
            float dy=fLastY-ev.pos.getY(); fLastY=ev.pos.getY();
            int p=paramOf(fDrag);
            float nv=fValue[fDrag]+dy*(fMax[fDrag]-fMin[fDrag])/200.f;
            if(nv<fMin[fDrag])nv=fMin[fDrag]; if(nv>fMax[fDrag])nv=fMax[fDrag];
            setParameterValue(p,nv);
        }
        return fDrag>=0;
    }
private:
    void drawKnob(float cx,float cy,float r,float mn,float mx,float v,int skin){
        beginPath(); circle(cx,cy,r); fillColor(g2bsk(skin,1)); fill();
        strokeColor(g2bsk(skin,4)); strokeWidth(1.f); stroke();
        float t=(v-mn)/(mx-mn); if(t<0)t=0;if(t>1)t=1;
        float a0=0.75f*M_PI, a=a0+t*1.5f*M_PI;
        beginPath(); moveTo(cx,cy); lineTo(cx+cosf(a)*r*0.8f,cy+sinf(a)*r*0.8f);
        strokeColor(g2bsk(skin,5)); strokeWidth(2.2f); stroke();
        beginPath(); circle(cx,cy,r*0.28f); fillColor(g2bsk(skin,4)); fill();
    }
    static const int kScopeSize=512;
    float fScope[kScopeSize]={}; int fScopePos=0;
    float fMin[6],fMax[6],fValue[6]; const char* fLabel[6];
    int fBank=0,fOn=1,fSkin=0,fDrag=-1,fSfeer=1; float fLastY=0;
    float fHit[16]={};
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PadsUI)
};

UI* createUI(){return new PadsUI();}
END_NAMESPACE_DISTRHO
