/*
 * G2B KOOR+KEY — UI (NanoVG): 3 knoppen + koor-kiezer + key-display + podium.
 */

#include "DistrhoUI.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

enum { P_ON,P_PRESET,P_KEY,P_BREED,P_KRACHT,P_DETUNE,P_OUTKEY };
static const char* kPresetNam[4]={"DUO","TRIAK","GOSPEL","CATHEDRAL"};
static const char* kNoteNam[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

START_NAMESPACE_DISTRHO

class KoorUI : public UI
{
public:
    KoorUI() : UI(560,320){}

protected:
    void parameterChanged(uint32_t i,float v) override {
        switch(i){
        case P_ON:fOn=v>0.5f;break; case P_PRESET:fPreset=(int)(v+.5f);break;
        case P_KEY:fKey=(int)(v+.5f);break; case P_BREED:fBreed=v;break;
        case P_KRACHT:fKracht=v;break; case P_DETUNE:fDetune=v;break;
        case P_OUTKEY:fOutKey=(int)(v+.5f);break;}
        repaint();
    }

    void onNanoDisplay() override {
        const float W=getWidth(),H=getHeight();
        beginPath();rect(0,0,W,H);fillColor(Color(10,14,11));fill();

        fontSize(20.f);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        fillColor(Color(61,220,132));text(18.f,22.f,"G2B KOOR+KEY",nullptr);
        fontSize(11.f);fillColor(Color(138,170,152));
        text(18.f,40.f,"jouw stem → heel koor · hoort de toonsoort",nullptr);

        // key-display rechtsboven
        {
            const float nx=W-180.f,ny=12.f;
            beginPath();roundedRect(nx,ny,166.f,34.f,6.f);
            fillColor(Color(16,24,18));fill();strokeColor(Color(44,74,56));strokeWidth(1.f);stroke();
            fontSize(9.f);fillColor(Color(138,170,152));textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            char kt[32];
            if(fKey>0){const int k=fKey-1;std::snprintf(kt,32,"TOONSOORT: %s%s",kNoteNam[k%12],k/12?"m":"");}
            else if(fOutKey>=0){const int k=fOutKey/2;std::snprintf(kt,32,"GEHOORD: %s%s",kNoteNam[k],fOutKey%2?"m":"");}
            else std::snprintf(kt,32,"AUTO — speel MIDI om te leren");
            fontSize(11.f);fillColor(Color(223,255,233));
            text(nx+83.f,ny+12.f,"TOONSOORT",nullptr);
            fontSize(13.f);text(nx+83.f,ny+26.f,kt,nullptr);
        }

        // preset-knoppen
        fontSize(10.f);
        for(int p=0;p<4;p++){
            const float x=18.f+p*128.f,y=58.f,w=120.f,h=24.f;
            beginPath();roundedRect(x,y,w,h,6.f);
            fillColor(p==fPreset?Color(61,220,132):Color(16,24,18));fill();
            strokeColor(Color(44,74,56));strokeWidth(1.f);stroke();
            fillColor(p==fPreset?Color(4,20,10):Color(191,232,206));
            textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            text(x+w/2.f,y+h/2.f,kPresetNam[p],nullptr);
        }
        fPresetRects[0]=18.f;fPresetRects[1]=58.f;fPresetRects[2]=128.f;fPresetRects[3]=24.f;

        // podium
        {
            const float sx=18.f,sy=96.f,sw=W-36.f,sh=92.f;
            beginPath();roundedRect(sx,sy,sw,sh,10.f);
            fillColor(Color(13,20,15));fill();strokeColor(Color(30,46,36));strokeWidth(1.f);stroke();
            fontSize(9.f);fillColor(Color(90,120,100));textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            text(sx+sw/2.f,sy+sh-10.f,"PODIUM",nullptr);
            // lead midden
            drawDot(sx+sw/2.f,sy+30.f,true,"LEAD");
            const int nv=fPreset==0?1:(fPreset==1?2:(fPreset==2?3:4));
            for(int v=0;v<nv;v++){
                const float pan=(v%2?1.f:-1.f)*(0.35f+0.18f*v)*fBreed;
                drawDot(sx+sw/2.f+pan*sw*0.42f,sy+48.f,false,
                        v==0?"+3/+4":(v==1?"+7":(v==2?"+12":"-12")));
            }
        }

        // knoppen: BREEDTE KRACHT DETUNE AAN
        drawKnob(90.f,230.f,"BREEDTE",fBreed,0);
        drawKnob(240.f,230.f,"KRACHT",fKracht,1);
        drawKnob(390.f,230.f,"DETUNE",fDetune,2);
        {
            const float ax=W-70.f,ay=216.f;
            beginPath();roundedRect(ax,ay,54.f,30.f,6.f);
            fillColor(fOn?Color(61,220,132):Color(42,47,45));fill();
            fontSize(11.f);fillColor(fOn?Color(4,20,10):Color(138,170,152));
            textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            text(ax+27.f,ay+15.f,fOn?"AAN":"UIT",nullptr);
        }
        fontSize(9.f);fillColor(Color(80,100,88));
        textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        text(18.f,H-14.f,"tip: stuur MIDI van je track/piano naar deze plugin → hij leert de toonsoort zelf",nullptr);
    }

    bool onMouse(const MouseEvent& ev) override {
        if(ev.press&&ev.button==1){
            // preset-klik
            for(int p=0;p<4;p++){
                const float x=18.f+p*128.f;
                if(ev.pos.getX()>=x&&ev.pos.getX()<=x+120.f&&ev.pos.getY()>=58.f&&ev.pos.getY()<=82.f){
                    editParameter(P_PRESET,true);setParameterValue(P_PRESET,(float)p);editParameter(P_PRESET,false);
                    fPreset=p;repaint();return true;
                }
            }
            const float ax=getWidth()-70.f;
            if(ev.pos.getX()>=ax&&ev.pos.getX()<=ax+54.f&&ev.pos.getY()>=216.f&&ev.pos.getY()<=246.f){
                fOn=!fOn;editParameter(P_ON,true);setParameterValue(P_ON,fOn?1.f:0.f);editParameter(P_ON,false);
                repaint();return true;
            }
            // knop-drag start
            fDrag=knobAt(ev.pos.getX(),ev.pos.getY());fDragY=ev.pos.getY();
            if(fDrag>=0)return true;
        }
        if(!ev.press&&fDrag>=0){fDrag=-1;return true;}
        return UI::onMouse(ev);
    }
    bool onMotion(const MotionEvent& ev) override {
        if(fDrag>=0){
            float* tgt=fDrag==0?&fBreed:(fDrag==1?&fKracht:&fDetune);
            const uint32_t pr=fDrag==0?P_BREED:(fDrag==1?P_KRACHT:P_DETUNE);
            *tgt=std::fmin(1.f,std::fmax(0.f,*tgt-(ev.pos.getY()-fDragY)/150.f));
            fDragY=ev.pos.getY();
            editParameter(pr,true);setParameterValue(pr,*tgt);editParameter(pr,false);
            repaint();return true;
        }
        return UI::onMotion(ev);
    }
    bool onScroll(const ScrollEvent& ev) override {
        const int k=knobAt(ev.pos.getX(),ev.pos.getY());
        if(k>=0){
            float* tgt=k==0?&fBreed:(k==1?&fKracht:&fDetune);
            const uint32_t pr=k==0?P_BREED:(k==1?P_KRACHT:P_DETUNE);
            *tgt=std::fmin(1.f,std::fmax(0.f,*tgt+ev.delta.getY()*0.05f));
            editParameter(pr,true);setParameterValue(pr,*tgt);editParameter(pr,false);
            repaint();return true;
        }
        return UI::onScroll(ev);
    }

private:
    bool fOn=true;int fPreset=1,fKey=0,fOutKey=-1;
    float fBreed=.7f,fKracht=.6f,fDetune=.5f;
    int fDrag=-1;float fDragY=0,fPresetRects[4]={0,0,0,0};

    int knobAt(float x,float y){
        const float kx[3]={90.f,240.f,390.f};
        for(int i=0;i<3;i++)if(std::fabs(x-kx[i])<34.f&&std::fabs(y-230.f)<34.f)return i;
        return -1;
    }
    void drawDot(float x,float y,bool lead,const char* lbl){
        beginPath();arc(x,y,lead?11.f:8.f,0.f,6.2832f,CW);
        fillColor(lead?Color(245,166,35):Color(61,220,132));fill();
        fontSize(8.f);fillColor(Color(138,170,152));textAlign(ALIGN_CENTER|ALIGN_TOP);
        text(x,y+12.f,lbl,nullptr);
    }
    void drawKnob(float x,float y,const char* lbl,float v,int){
        const float r=26.f;
        beginPath();arc(x,y,r,0.f,6.2832f,CW);
        fillColor(Color(16,24,18));fill();strokeColor(Color(44,74,56));strokeWidth(1.5f);stroke();
        const float a=-2.35f+v*4.7f;
        beginPath();moveTo(x,y);lineTo(x+std::cos(a-1.5708f)*r*0.8f,y+std::sin(a-1.5708f)*r*0.8f);
        strokeColor(Color(61,220,132));strokeWidth(3.f);stroke();
        fontSize(9.f);fillColor(Color(138,170,152));textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
        text(x,y+r+12.f,lbl,nullptr);
        char vb[12];std::snprintf(vb,12,"%d%%",(int)(v*100.f));
        fillColor(Color(223,255,233));text(x,y,vb,nullptr);
    }
};

UI* createUI(){ return new KoorUI(); }

END_NAMESPACE_DISTRHO
