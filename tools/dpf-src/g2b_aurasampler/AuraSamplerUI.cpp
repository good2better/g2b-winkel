/* G2B AURA Sampler — UI: pack-tabs + groen stapel-grid + knoppenrij + scope. */
#include "../g2b_dnasampler/dna_shared.h"
#include "DistrhoUI.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

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
static const char* PACKN[3]={"SURI-PADS","ALL-INSTR","BASS&DRUM"};

START_NAMESPACE_DISTRHO

class AuraUI : public UI {
public:
    AuraUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT) {
        loadSharedResources();
        static const float mins[5]={0,-12,0.1f,0,-24};
        static const float maxs[5]={1,12,4,0.2f,12};
        static const float defs[5]={0.8f,0,1,0,0};
        static const char* lbl[5]={"KRACHT","TOON","LENGTE","KNIP","OUTPUT"};
        std::memcpy(fMin,mins,sizeof(mins)); std::memcpy(fMax,maxs,sizeof(maxs));
        std::memcpy(fValue,defs,sizeof(defs)); std::memcpy(fLabel,lbl,sizeof(lbl));
        fDrag=-1; fLastY=0.f; fPack=0; fPage=0;
        {auto& L=dna_names_list();fNNames=0;
            for(size_t i=0;i<L.size()&&i<512;i++){const std::string&s=L[i];const char*p=s.c_str();size_t n=s.size();
                char nm[32];if(n>20)p+=n-20,n=20;strncpy(nm,p,20);nm[20]=0;
                strncpy(fNames[fNNames],nm,31);fNames[fNNames][31]=0;
                strncpy(fFull[fNNames],s.c_str(),95);fFull[fNNames][95]=0;fNNames++;}}
    }
protected:
    // params: 0 PACK,1 SND,2 KRACHT,3 LEVEL,4 AAN,5 TOON,6 LENGTE,7 KNIP,8 DICE,9 TGL,10 WIS,11 GAL,12 SFEER
    static int paramOf(int slot){return slot==0?2:(slot==1?5:(slot==2?6:(slot==3?7:3)));}
    int packOf(int i){const char*f=fFull[i];
        if(!strncmp(f,"suri",4))return 0;if(!strncmp(f,"instr",5))return 1;
        if(!strncmp(f,"bass",4))return 2;return -1;}
    void parameterChanged(uint32_t i, float v) override {
        if(i==0){fPack=(int)(v+0.5f);fPage=0;}
        else if(i==1){fInstr=(int)(v+0.5f);}
        else if(i==2)fValue[0]=v; else if(i==3)fValue[4]=v;
        else if(i==4)fOn=(int)(v+0.5f);
        else if(i==5)fValue[1]=v; else if(i==6)fValue[2]=v; else if(i==7)fValue[3]=v;
        repaint();
    }
    void stateChanged(const char* key, const char* value) override {
        if(std::strcmp(key,"instrnames")==0){
            int i=0; const char* p=value;
            while(*p&&i<512){const char* sc=strchr(p,';');int n=sc?(int)(sc-p):(int)strlen(p);
                char nm[32];int cn=n>20?20:n;strncpy(nm,p+(n-cn),cn);nm[cn]=0;
                strncpy(fNames[i],nm,31);fNames[i][31]=0;
                strncpy(fFull[i],p,n>95?95:n);fFull[i][n>95?95:n]=0;i++;p=sc?sc+1:p+n;if(!sc)break;}fNNames=i;
            repaint();
        } else if(std::strcmp(key,"sel")==0){
            fSel.clear();const char*p=value;
            while(*p){char*e=nullptr;long v=strtol(p,&e,10);if(e==p)break;fSel.insert((int)v);p=(*e==';')?e+1:e;}
            repaint();
        } else if(std::strcmp(key,"scope")==0){
            const char* p=value;
            while(*p){char* e=nullptr;float v=strtof(p,&e);if(e==p)break;fScope[fScopePos]=v;fScopePos=(fScopePos+1)%kScopeSize;p=(*e==';')?e+1:e;}
            repaint();
        } else if(std::strcmp(key,"mapmsg")==0){
            fMapMsg=value;repaint();
        }
    }
    void onNanoDisplay() override {
        const float W=getWidth(),H=getHeight();
        const int skin=fSkin;
        beginPath(); rect(0,0,W,H); fillColor(g2bsk(skin,0)); fill();
        fontSize(20.f); textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        fillColor(g2bsk(skin,5)); text(18.f,20.f,"G2B AURA SAMPLER",nullptr);
        fontSize(10.5f); fillColor(g2bsk(skin,3));
        text(18.f,38.f,"groene vakken = stapelen · klik nogmaals = uit",nullptr);
        for(int i=0;i<8;i++){float dx=W-18.f-(7-i)*14.f;
            beginPath(); circle(dx,16.f,4.5f); fillColor(g2bsk(i,5)); fill();
            if(i==skin){strokeColor(g2bsk(skin,2));strokeWidth(1.2f);stroke();}}
        // pack-tabs
        for(int p=0;p<3;p++){
            float tx=18.f+p*130.f,ty=48.f;
            beginPath(); roundedRect(tx,ty,120.f,22.f,6.f);
            if(p==fPack){fillColor(g2bsk(skin,5));}else{fillColor(g2bsk(skin,1));}
            fill();strokeColor(g2bsk(skin,4));strokeWidth(1.f);stroke();
            fontSize(10.f);fillColor(p==fPack?g2bsk(skin,0):g2bsk(skin,2));
            text(tx+60.f,ty+11.f,PACKN[p],nullptr);
        }
        // stapel-grid (4 x 6 cellen per pagina)
        fCells.clear();
        std::vector<int> idx;
        for(int i=0;i<fNNames;i++)if(packOf(i)==fPack||packOf(i)<0)idx.push_back(i);
        int cols=4,rows=6,per=cols*rows,pages=(int)(idx.size()+per-1)/per;
        if(fPage>=pages)fPage=0;
        float gx=18.f,gy=78.f,gw=W-36.f,gh=196.f,cw=(gw-3.f*(cols-1))/cols,ch=(gh-5.f*(rows-1))/rows;
        for(size_t k=0;k<idx.size()&&(int)k>=fPage*per&&(int)k<(fPage+1)*per;k++){
            int i=idx[k];int pos=(int)k-fPage*per;int c=pos%cols,r=pos/cols;
            float cx=gx+c*(cw+3.f),cy=gy+r*(ch+5.f);
            bool on=fSel.count(i)||(fSel.empty()&&i==fInstr);
            beginPath(); roundedRect(cx,cy,cw,ch,5.f);
            if(on){fillColor(g2bsk(skin,5));}else{fillColor(g2bsk(skin,1));}
            fill();strokeColor(on?Color(200,255,220):g2bsk(skin,4));strokeWidth(1.f);stroke();
            fontSize(9.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(on?g2bsk(skin,0):g2bsk(skin,3));
            text(cx+cw/2,cy+ch/2,fNames[i],nullptr);
            fCells.push_back({cx,cy,cw,ch,i});
        }
        // pagina-pijlen
        if(pages>1){fontSize(14.f);fillColor(g2bsk(skin,5));textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            char pg[16];snprintf(pg,sizeof pg,"< %d/%d >",fPage+1,pages);
            text(W-40.f,gy+gh+14.f,pg,nullptr);}
        // scope
        float sx=18.f, sy=296.f, sw=W-36.f, sh=50.f;
        beginPath(); rect(sx,sy,sw,sh); fillColor(g2bsk(skin,1)); fill();
        strokeColor(g2bsk(skin,4)); strokeWidth(1.f); stroke();
        beginPath();
        for(int i=0;i<kScopeSize;i++){
            int idx2=(fScopePos+i)%kScopeSize;
            float x=sx+sw*i/(float)kScopeSize, y=sy+sh/2 - fScope[idx2]*sh*0.45f;
            if(i==0)moveTo(x,y);else lineTo(x,y);
        }
        strokeColor(g2bsk(skin,5)); strokeWidth(1.2f); stroke();
        // knoppenrij (5 knobs)
        const int nk=5; float step=(W-40)/nk;
        for(int i=0;i<nk;i++){
            float cx=20+step*(i+0.5f), cy=400.f, r=26.f;
            drawKnob(cx,cy,r,fMin[i],fMax[i],fValue[i],skin);
            fontSize(9.5f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(g2bsk(skin,3)); text(cx,cy+r+15.f,fLabel[i],nullptr);
            char vb[32];
            if(i==0)snprintf(vb,sizeof vb,"%d%%",(int)(fValue[i]*100+0.5f));
            else if(i==1)snprintf(vb,sizeof vb,"%+.1f st",fValue[i]);
            else if(i==2)snprintf(vb,sizeof vb,"%.1f s",fValue[i]);
            else if(i==3)snprintf(vb,sizeof vb,"%d%%",(int)(fValue[i]*100+0.5f));
            else snprintf(vb,sizeof vb,"%.1f dB",fValue[i]);
            fillColor(g2bsk(skin,2)); text(cx,cy+r+27.f,vb,nullptr);
        }
        // eigen-map invoer links + pills WIS/DICE/AAN/MAP rechts
        float ay=H-40.f;
        {
            float bx=18.f,bw=W-270.f;
            beginPath(); roundedRect(bx,ay,bw,22.f,6.f);
            fillColor(g2bsk(skin,1)); fill();
            strokeColor(fTyping?g2bsk(skin,5):g2bsk(skin,4));strokeWidth(1.4f);stroke();
            fontSize(10.f);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
            if(!fMapMsg.empty()){fillColor(g2bsk(skin,5));text(bx+8.f,ay+11.f,fMapMsg.c_str(),nullptr);}
            else if(fMapPath.empty()){fillColor(g2bsk(skin,3));text(bx+8.f,ay+11.f,"MAP = mapvenster, of klik + typ pad  ·  < > = geluid bladeren",nullptr);}
            else{fillColor(g2bsk(skin,2));text(bx+8.f,ay+11.f,fMapPath.c_str(),nullptr);}
        }
        // ‹ › bladeren door geluiden (binnen de gekozen pack)
        {
            const char* ar[2]={"<",">"};
            for(int a=0;a<2;a++){
                float ax=W-140.f+a*34.f;
                beginPath(); roundedRect(ax,ay,28.f,22.f,11.f);
                fillColor(g2bsk(skin,1)); fill();
                strokeColor(g2bsk(skin,4));strokeWidth(1.f);stroke();
                fontSize(12.f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
                fillColor(g2bsk(skin,5));text(ax+14.f,ay+11.f,ar[a],nullptr);
                textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
            }
        }
        for(int p=0;p<4;p++){
            float px=W-60.f-p*62.f;
            const char*t=p==0?"AAN":(p==1?"DICE":(p==2?"WIS":"MAP"));
            bool on=p==0?fOn:false;
            beginPath(); roundedRect(px,ay,54.f,22.f,11.f);
            fillColor(on?g2bsk(skin,5):g2bsk(skin,1)); fill();
            strokeColor(g2bsk(skin,4));strokeWidth(1.f);stroke();
            fontSize(10.f);fillColor(on?g2bsk(skin,0):g2bsk(skin,3));
            text(px+27.f,ay+11.f,t,nullptr);
        }
        fontSize(9.f); textAlign(ALIGN_LEFT|ALIGN_MIDDLE); fillColor(g2bsk(skin,3));
        char st[64];snprintf(st,sizeof st,"%d geselecteerd — %s",(int)fSel.size(),G2BSKN[skin]);
        text(18.f,H-12.f,st,nullptr);
    }
    bool onMouse(const MouseEvent& ev) override {
        const float W=getWidth(),H=getHeight();
        if(ev.press){
            float x=ev.pos.getX(),y=ev.pos.getY();
            for(int i=0;i<8;i++){float dx=W-18.f-(7-i)*14.f;if(std::fabs(x-dx)<8&&std::fabs(y-16)<10){fSkin=i;repaint();return true;}}
            // pack-tabs
            if(y>=48&&y<=70){for(int p=0;p<3;p++){float tx=18.f+p*130.f;if(x>=tx&&x<=tx+120){setParameterValue(0,(float)p);return true;}}}
            // grid-cellen → toggle via param 9 (idx+1)
            for(auto&c:fCells)if(x>=c.x&&x<=c.x+c.w&&y>=c.y&&y<=c.y+c.h){
                setParameterValue(9,(float)c.i+1.f);if(fSel.count(c.i))fSel.erase(c.i);else fSel.insert(c.i);repaint();return true;}
            // pagina-pijlen
            if(x>W-70&&y>270&&y<295){fPage++;repaint();return true;}
            // eigen-map invoer + pills
            float ay=H-40.f;
            if(y>=ay&&y<=ay+22){
                if(x>=W-60&&x<=W-6){setParameterValue(4,fOn?0.f:1.f);fTyping=false;return true;}
                if(x>=W-122&&x<=W-68){setParameterValue(8,1.f);fTyping=false;return true;}
                if(x>=W-184&&x<=W-130){setParameterValue(10,1.f);fSel.clear();fTyping=false;repaint();return true;}
                if(x>=W-140&&x<=W-112){setParameterValue(13,1.f);fTyping=false;return true;}
                if(x>=W-106&&x<=W-76){setParameterValue(14,1.f);fTyping=false;return true;}
                if(x>=W-246&&x<=W-192){openFileBrowser();return true;}
                if(x>=18&&x<=W-270){fTyping=true;fMapMsg.clear();repaint();return true;}
            }
            if(fTyping){fTyping=false;repaint();}
            // knobs
            const int nk=5;float step=(W-40)/nk;
            for(int i=0;i<nk;i++){float cx=20+step*(i+0.5f),cy=400.f;
                if(std::fabs(x-cx)<30&&std::fabs(y-cy)<30){fDrag=i;fLastY=y;return true;}}
        } else fDrag=-1;
        return false;
    }
    void laadMap(){
        if(fMapPath.empty()){fMapMsg="typ eerst een pad, bv D:\\Samples";repaint();return;}
        setState("map",fMapPath.c_str());fMapMsg="laden…";fTyping=false;repaint();
    }
    // verkenner: klik één wav aan → hele map wordt gescand (geen typen nodig)
    void uiFileBrowserSelected(const char* filename) override {
        std::string fn(filename);
        const size_t sep=fn.find_last_of("/\\");
        fMapPath=(sep==std::string::npos)?".":fn.substr(0,sep);
        setState("map",fMapPath.c_str());fMapMsg="laden…";fTyping=false;repaint();
    }
    bool onKeyboard(const KeyboardEvent& ev) override {
        if(!fTyping||!ev.press)return false;
        if(ev.key==kKeyEnter){laadMap();return true;}
        if(ev.key==kKeyEscape){fTyping=false;repaint();return true;}
        if(ev.key==kKeyBackspace){
            while(!fMapPath.empty()&&((unsigned char)fMapPath.back()&0xC0)==0x80)fMapPath.pop_back();
            if(!fMapPath.empty())fMapPath.pop_back();
            repaint();return true;}
        return true;
    }
    bool onCharacterInput(const CharacterInputEvent& ev) override {
        if(!fTyping)return false;
        if(ev.character>=32&&fMapPath.size()<400){fMapPath+=ev.string;repaint();}
        return true;
    }
    bool onMotion(const MotionEvent& ev) override {
        if(fDrag>=0){
            float dy=fLastY-ev.pos.getY(); fLastY=ev.pos.getY();
            int p=paramOf(fDrag);
            float nv=fValue[fDrag]+dy*(fMax[fDrag]-fMin[fDrag])/200.f;
            if(nv<fMin[fDrag])nv=fMin[fDrag]; if(nv>fMax[fDrag])nv=fMax[fDrag];
            if(p==7&&nv>0.2f)nv=0.2f; // knip hard ≤20%
            setParameterValue(p,nv);fValue[fDrag]=nv;
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
    struct Cell{float x,y,w,h;int i;};
    static const int kScopeSize=512;
    float fScope[kScopeSize]={}; int fScopePos=0;
    float fMin[5],fMax[5],fValue[5]; const char* fLabel[5];
    int fInstr=0,fOn=1,fSkin=0,fDrag=-1,fPack=0,fPage=0; float fLastY=0;
    char fNames[512][32];char fFull[512][96];int fNNames=1;
    std::set<int> fSel;std::vector<Cell> fCells;
    std::string fMapPath,fMapMsg;bool fTyping=false;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuraUI)
};

UI* createUI(){return new AuraUI();}
END_NAMESPACE_DISTRHO
