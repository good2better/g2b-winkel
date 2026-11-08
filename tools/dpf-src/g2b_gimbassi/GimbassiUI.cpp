/*
 * G2B GIM BASSI PRO — UI (NanoVG): 6 knoppen + SLAG-lampje.
 * Sleep omhoog/omlaag = draaien · dubbelklik = reset · QUANT/MODE zijn toggles.
 */

#include "DistrhoUI.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

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
static inline Color g2bsk(int sk,int r){if(sk<0)sk=0;if(sk>7)sk=7;const float*c=G2BSK[sk][r];return Color((int)c[0],(int)c[1],(int)c[2]);}

START_NAMESPACE_DISTRHO

class GimbassiUI : public UI {
public:
    GimbassiUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT) {
        loadSharedResources();
        static const float mn[kN]={40,0,0,0,0,0};
        static const float mx[kN]={200,100,100,100,1,100};
        static const float df[kN]={120,15,85,100,0,100};
        static const char* lb[kN]={"FILT","GATE","DUCK","QUANT","MODE","MIX"};
        std::memcpy(fMin,mn,sizeof(mn));std::memcpy(fMax,mx,sizeof(mx));
        std::memcpy(fVal,df,sizeof(df));std::memcpy(fDef,df,sizeof(df));
        std::memcpy(fLbl,lb,sizeof(lb));
    }
protected:
    void parameterChanged(uint32_t i,float v) override {
        if(i<kN)fVal[i]=v;else if(i==6)fMidi=v;else if(i==8)fMeter=v;else if(i==9)fLink=v;repaint();
    }
    void onNanoDisplay() override {
        const float W=getWidth(),H=getHeight();
        beginPath();rect(0,0,W,H);fillColor(g2bsk(0,0));fill();
        fontSize(21.f);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);fillColor(g2bsk(0,5));
        text(18.f,20.f,"G2B GIM BASSI",nullptr);
        fontSize(10.f);fillColor(g2bsk(0,3));
        text(18.f,38.f,"bas + kick eruit — live op je track",nullptr);
        fontSize(13.f);fillColor(Color(232,182,76));
        textAlign(ALIGN_RIGHT|ALIGN_MIDDLE);text(W-18.f,20.f,"PRO",nullptr);
        // slag-lamp + link-lamp + MIDI-pill rechts
        const float lx=W-26.f,ly=44.f;
        beginPath();circle(lx,ly,6.f);
        fillColor(fMeter>0.05f?g2bsk(0,5):g2bsk(0,4));fill();
        fontSize(8.f);fillColor(g2bsk(0,3));textAlign(ALIGN_RIGHT|ALIGN_MIDDLE);
        text(lx-10.f,ly,"SLAG",nullptr);
        beginPath();circle(lx,ly+20.f,6.f);
        fillColor(fLink>0.5f?Color(232,182,76):g2bsk(0,4));fill();
        fillColor(g2bsk(0,3));text(lx-10.f,ly+20.f,"LINK",nullptr);
        // MIDI pill
        beginPath();roundedRect(lx-64.f,ly+32.f,62.f,16.f,8.f);
        fillColor(fMidi>0.5f?g2bsk(0,5):g2bsk(0,4));fill();
        fontSize(9.f);fillColor(fMidi>0.5f?g2bsk(0,0):g2bsk(0,2));
        textAlign(ALIGN_CENTER|ALIGN_MIDDLE);text(lx-33.f,ly+40.f,"MIDI",nullptr);
        // 6 knoppen
        const float cy=H*0.52f,gap=W/6.f;
        for(int i=0;i<kN;i++){
            const float cx=gap*(i+0.5f),kr=34.f;
            const bool tgl=(i==4);
            beginPath();circle(cx,cy,kr);fillColor(g2bsk(0,1));fill();
            beginPath();circle(cx,cy,kr);strokeColor(g2bsk(0,4));strokeWidth(2.f);stroke();
            const float norm=(fVal[i]-fMin[i])/(fMax[i]-fMin[i]);
            const float a0=2.35f,a1=2.35f+norm*3.9f;
            beginPath();arc(cx,cy,kr-4.f,a0-1.57f,a1-1.57f,CW);
            strokeColor(g2bsk(0,5));strokeWidth(4.f);stroke();
            // pointer
            const float pa=a1-1.57f;
            beginPath();moveTo(cx+std::cos(pa)*kr*0.55f,cy+std::sin(pa)*kr*0.55f);
            lineTo(cx+std::cos(pa)*kr*0.85f,cy+std::sin(pa)*kr*0.85f);
            strokeColor(g2bsk(0,2));strokeWidth(3.f);stroke();
            // label + waarde
            fontSize(10.f);fillColor(g2bsk(0,3));textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            text(cx,cy+kr+16.f,fLbl[i],nullptr);
            char buf[24];
            if(tgl)std::snprintf(buf,sizeof(buf),"%s",fVal[i]>0.5f?"SPLIT":"MIX");
            else if(i==0)std::snprintf(buf,sizeof(buf),"%.0f Hz",fVal[i]);
            else std::snprintf(buf,sizeof(buf),"%.0f%%",fVal[i]);
            fontSize(11.f);fillColor(g2bsk(0,5));text(cx,cy+kr+30.f,buf,nullptr);
        }
        fontSize(8.5f);fillColor(g2bsk(0,3));textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
        text(W/2.f,H-10.f,"SPLIT: L=bas R=kick · QUANT% volgt DAW-tempo · MIDI=bas-noten uit",nullptr);
    }
    bool onMouse(const MouseEvent& ev) override {
        const float W=getWidth(),H=getHeight(),cy=H*0.52f,gap=W/6.f;
        if(ev.press&&ev.button==kMouseButtonLeft){
            for(int i=0;i<kN;i++){const float cx=gap*(i+0.5f);
                if(std::hypot(ev.pos.getX()-cx,ev.pos.getY()-cy)<38.f){
                    fDrag=i;fLastY=ev.pos.getY();
                    if(i==4){fVal[i]=fVal[i]>0.5f?0.f:1.f;setParameterValue(i,fVal[i]);fDrag=-1;repaint();}
                    return true;}}
            // MIDI-pill rechts
            const float lx=W-26.f,ly=44.f;
            if(ev.pos.getX()>lx-64.f&&ev.pos.getX()<lx-2.f&&ev.pos.getY()>ly+32.f&&ev.pos.getY()<ly+48.f){
                fMidi=fMidi>0.5f?0.f:1.f;setParameterValue(6,fMidi);repaint();return true;}
            return false;
        }
        if(fDrag>=0&&ev.button==kMouseButtonLeft&&!ev.press){
            const float dy=fLastY-ev.pos.getY();fLastY=ev.pos.getY();
            float nv=fVal[fDrag]+dy*(fMax[fDrag]-fMin[fDrag])/180.f;
            nv=std::max(fMin[fDrag],std::min(fMax[fDrag],nv));
            fVal[fDrag]=nv;setParameterValue(fDrag,nv);repaint();return true;
        }
        if(!ev.press){fDrag=-1;}
        return UI::onMouse(ev);
    }
private:
    static const int kN=6;
    float fMin[kN],fMax[kN],fVal[kN],fDef[kN];const char* fLbl[kN];
    float fMeter=0.f,fLink=0.f,fMidi=1.f;int fDrag=-1;float fLastY=0.f;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GimbassiUI)
};

UI* createUI() { return new GimbassiUI(); }

END_NAMESPACE_DISTRHO
