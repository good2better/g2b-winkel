/* G2B Instrumenten — UI: instrument-kiezer + knoppenrij + scope. */
#include "orkest_names.h"
#include "DistrhoUI.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>

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

static const int kNInstr = 19;

class InstrUI : public UI {
public:
    InstrUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT) {
        loadSharedResources();
        static const float mins[5]={0,0,-24,0,0};
        static const float maxs[5]={1,1,12,26,2};
        static const float defs[5]={0,0.8f,0,9,1};
        static const char* lbl[5]={"KRACHT","OUTPUT","DIMENTIES","SFEER","SKIN"};
        std::memcpy(fMin,mins,sizeof(mins)); std::memcpy(fMax,maxs,sizeof(maxs));
        std::memcpy(fValue,defs,sizeof(defs)); std::memcpy(fLabel,lbl,sizeof(lbl));
        fDrag=-1; fLastY=0.f; srand((unsigned)time(nullptr));
        for(int i=0;i<kNInstr;i++){strncpy(fNames[i],kInstrNamesUI[i],15);fNames[i][15]=0;}
    }
protected:
    // params: 0 INSTR,1 KRACHT,2 LEVEL,3 AAN,4 GALAXY,5 SFEER,6 DICE
    static int paramOf(int slot){ // 0 KRACHT->1, 1 OUTPUT->2, 2 DIMENTIES->4, 3 SFEER->5, 4 SKIN(ui-only -> skinstate)
        return slot==0?1:(slot==1?2:(slot==2?4:(slot==3?5:-1)));
    }
    void parameterChanged(uint32_t i, float v) override {
        if(i==0) fInstr=(int)(v+0.5f);
        else if(i==1) fValue[0]=v;
        else if(i==2) fValue[1]=v;
        else if(i==3) fOn=(int)(v+0.5f);
        else if(i==4) fValue[2]=v;
        else if(i==5) fValue[3]=v;
        repaint();
    }
    void stateChanged(const char* key, const char* value) override {
        if(std::strcmp(key,"instrnames")==0){
            int i=0; const char* p=value;
            while(*p&&i<kNInstr){const char* sc=strchr(p,';');int n=sc?(int)(sc-p):(int)strlen(p); if(n>15)n=15;memcpy(fNames[i],p,n);fNames[i][n]=0;i++;p=sc?sc+1:p+n;if(!sc)break;}
            repaint();
        } else if(std::strcmp(key,"scope")==0){
            const char* p=value;
            while(*p){char* e=nullptr;float v=strtof(p,&e);if(e==p)break;fScope[fScopePos]=v;fScopePos=(fScopePos+1)%kScopeSize;p=(*e==';')?e+1:e;}
            repaint();
        }
    }
    void onNanoDisplay() override {
        const float W=getWidth(),H=getHeight();
        const int skin=fSkin;
        beginPath(); rect(0,0,W,H); fillColor(g2bsk(skin,0)); fill();
        fontSize(20.f); textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        fillColor(g2bsk(skin,5)); text(18.f,22.f,"G2B INSTRUMENTEN",nullptr);
        fontSize(11.5f); fillColor(g2bsk(skin,3));
        text(18.f,41.f,"jouw eigen klanken — speelbaar over heel het keyboard",nullptr);
        // skin dots rechtsboven
        for(int i=0;i<8;i++){
            float dx=W-18.f-(7-i)*14.f, dy=18.f;
            beginPath(); circle(dx,dy,4.5f);
            fillColor(g2bsk(i,5)); fill();
            if(i==skin){strokeColor(g2bsk(skin,2));strokeWidth(1.2f);stroke();}
        }
        // instrument-kiezer (midden boven)
        float ix=18.f, iy=54.f, iw=W-36.f, ih=52.f;
        beginPath(); roundedRect(ix,iy,iw,ih,6.f);
        fillColor(g2bsk(skin,1)); fill(); strokeColor(g2bsk(skin,4)); strokeWidth(1.f); stroke();
        fontSize(12.f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
        // < > pijlen
        fillColor(g2bsk(skin,5)); fontSize(22.f);
        text(ix+24.f,iy+ih/2,"<",nullptr); text(ix+iw-24.f,iy+ih/2,">",nullptr);
        fontSize(24.f); fillColor(g2bsk(skin,2));
        text(ix+iw/2,iy+20.f,fNames[fInstr][0]?fNames[fInstr]:"INSTRUMENT",nullptr);
        fontSize(10.f); fillColor(g2bsk(skin,3));
        char nb[32]; snprintf(nb,sizeof nb,"%d / %d — DICE = willekeurig",fInstr+1,kNInstr);
        text(ix+iw/2,iy+40.f,nb,nullptr);
        // scope
        float sx=18.f, sy=118.f, sw=W-36.f, sh=44.f;
        beginPath(); rect(sx,sy,sw,sh); fillColor(g2bsk(skin,1)); fill();
        strokeColor(g2bsk(skin,4)); strokeWidth(1.f); stroke();
        beginPath();
        for(int i=0;i<kScopeSize;i++){
            int idx=(fScopePos+i)%kScopeSize;
            float x=sx+sw*i/(float)kScopeSize, y=sy+sh/2 - fScope[idx]*sh*0.45f;
            if(i==0)moveTo(x,y);else lineTo(x,y);
        }
        strokeColor(g2bsk(skin,5)); strokeWidth(1.2f); stroke();
        // knoppenrij (4 knoppen + SKIN-label)
        const int nk=4; float step=(W-40)/nk;
        for(int i=0;i<nk;i++){
            float cx=20+step*(i+0.5f), cy=214.f, r=24.f;
            drawKnob(cx,cy,r,fMin[i],fMax[i],fValue[i],skin);
            fontSize(9.5f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(g2bsk(skin,3)); text(cx,cy+r+14.f,fLabel[i],nullptr);
            char vb[32];
            if(i==0)snprintf(vb,sizeof vb,"%d%%",(int)(fValue[i]*100+0.5f));
            else if(i==1)snprintf(vb,sizeof vb,"%.1f dB",fValue[i]);
            else if(i==2)snprintf(vb,sizeof vb,"%.0f/27",fValue[i]+1.f);
            else snprintf(vb,sizeof vb,"%s",fValue[i]<0.5f?"DARK":(fValue[i]>1.5f?"LICHT":"NEUTRAAL"));
            fillColor(g2bsk(skin,2)); text(cx,cy+r+26.f,vb,nullptr);
        }
        // AAN pill rechtsonder
        float ax=W-66.f, ay=H-30.f;
        beginPath(); roundedRect(ax,ay,52.f,20.f,10.f);
        fillColor(fOn?g2bsk(skin,5):g2bsk(skin,4)); fill();
        fontSize(10.f); fillColor(fOn?g2bsk(skin,0):g2bsk(skin,2));
        text(ax+26.f,ay+10.f,"AAN",nullptr);
        // DICE pill
        beginPath(); roundedRect(ax,ay-28.f,52.f,20.f,10.f);
        fillColor(g2bsk(skin,1)); fill(); strokeColor(g2bsk(skin,4));strokeWidth(1.f);stroke();
        fillColor(g2bsk(skin,3)); text(ax+26.f,ay-18.f,"DICE",nullptr);
        // skin naam
        fontSize(9.f); textAlign(ALIGN_LEFT|ALIGN_MIDDLE); fillColor(g2bsk(skin,3));
        text(18.f,H-12.f,G2BSKN[skin],nullptr);
    }
    bool onMouse(const MouseEvent& ev) override {
        const float W=getWidth(),H=getHeight();
        if(ev.press){
            float x=ev.pos.getX(),y=ev.pos.getY();
            // skin dots
            for(int i=0;i<8;i++){float dx=W-18.f-(7-i)*14.f;if(std::fabs(x-dx)<8&&std::fabs(y-18)<10){fSkin=i;repaint();return true;}}
            // instrument pijlen
            float ix=18.f,iy=54.f,iw=W-36.f,ih=52.f;
            if(y>=iy&&y<=iy+ih){
                if(x<ix+48){fInstr=(fInstr<=0)?kNInstr-1:fInstr-1;setParameterValue(0,(float)fInstr);repaint();return true;}
                if(x>ix+iw-48){fInstr=(fInstr>=kNInstr-1)?0:fInstr+1;setParameterValue(0,(float)fInstr);repaint();return true;}
            }
            // AAN / DICE pills
            float ax=W-66.f,ay=H-30.f;
            if(x>=ax&&x<=ax+52&&y>=ay&&y<=ay+20){fOn=fOn?0:1;setParameterValue(3,(float)fOn);repaint();return true;}
            if(x>=ax&&x<=ax+52&&y>=ay-28&&y<=ay-8){fInstr=(int)(rand()/(RAND_MAX+1.0)*kNInstr);setParameterValue(0,(float)fInstr);repaint();return true;}
            // knobs
            const int nk=4;float step=(W-40)/nk;
            for(int i=0;i<nk;i++){float cx=20+step*(i+0.5f),cy=214.f;
                if(std::fabs(x-cx)<28&&std::fabs(y-cy)<28){fDrag=i;fLastY=y;return true;}}
        } else fDrag=-1;
        return false;
    }
    bool onMotion(const MotionEvent& ev) override {
        if(fDrag>=0){
            float dy=fLastY-ev.pos.getY(); fLastY=ev.pos.getY();
            int p=paramOf(fDrag); if(p<0)return true;
            float nv=fValue[fDrag]+dy*(fMax[fDrag]-fMin[fDrag])/200.f;
            if(nv<fMin[fDrag])nv=fMin[fDrag]; if(nv>fMax[fDrag])nv=fMax[fDrag];
            fValue[fDrag]=nv;setParameterValue(p,nv);repaint();
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
        strokeColor(g2bsk(skin,5)); strokeWidth(2.4f); stroke();
        beginPath(); circle(cx,cy,r*0.28f); fillColor(g2bsk(skin,4)); fill();
    }
    static const int kScopeSize=512;
    float fScope[kScopeSize]={}; int fScopePos=0;
    float fMin[5],fMax[5],fValue[5]; const char* fLabel[5];
    int fInstr=0,fOn=1,fSkin=0,fDrag=-1; float fLastY=0;
    char fNames[kNInstr][16];
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstrUI)
};

UI* createUI(){return new InstrUI();}
END_NAMESPACE_DISTRHO
