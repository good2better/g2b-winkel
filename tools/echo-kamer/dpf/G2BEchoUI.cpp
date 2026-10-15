// G2B Echo-Kamer UI — vaste G2B-knoppen + XY-pad (tijd x feedback).
#include "DistrhoPluginInfo.h"
#include "DistrhoUI.hpp"
#include "Extra/Widgets.hpp"
#include <cstdio>

START_NAMESPACE_DISTRHO

static const char* kSkinNames[8]={"groen","paars","blauw","rood","goud","roze","cyan","wit"};
static const uint32_t kSkinCol[8]={0x7ee787ff,0xd2a8ffff,0x79c0ffff,0xff7b72ff,0xf2cc60ff,0xffb3d9ff,0x76e3eaff,0xf0f6fcff};

class G2BEchoUI : public UI {
public:
    G2BEchoUI() : UI(560,400) {
        setGeometryConstraints(560,400,true,true);
        btn_[0]="AAN/UIT"; btn_[1]="PINGPONG";
        names_[0]="KRACHT"; names_[1]="TIJD"; names_[2]="FEEDBACK"; names_[3]="DUCKING";
        names_[4]="LOWPASS"; names_[5]="HIGHPASS"; names_[6]="DIMENTIES"; names_[7]="SKIN";
    }
protected:
    void parameterChanged(uint32_t i,float v) override { v_[i]=v; repaint(); }
    void programLoaded(uint32_t) override {}
    void onDisplay() override {
        const GraphicsContext& g=getGraphicsContext();
        uint32_t sk=(uint32_t)v_[7]%8; uint32_t acc=kSkinCol[sk];
        g.setColor(0x0d1117ff); g.fillRect(0,0,560,400);
        g.setColor(acc); g.fillRect(0,0,560,4);
        // titel
        g.setColor(acc); g.drawText(Rect(14,14,540,40),"G2B ECHO-KAMER",16);
        g.setColor(0x8b949eff); g.drawText(Rect(14,36,540,54),kSkinNames[sk],11);
        // toggle-buttons
        for(int b=0;b<2;++b){
            int x=14+b*110;
            bool on=v_[b==0?0:4]>0.5f;
            g.setColor(on?acc:0x21262dff); g.fillRect(x,64,100,34);
            g.setColor(on?0x0d1117ff:0x8b949eff); g.drawText(Rect(x,64,x+100,98),btn_[b],11);
        }
        // SPEEL/MIDI-buttons (placeholder consistentie)
        g.setColor(0x21262dff); g.fillRect(234,64,100,34); g.fillRect(344,64,100,34);
        g.setColor(0x8b949eff); g.drawText(Rect(234,64,334,98),"PING",11); g.drawText(Rect(344,64,444,98),"PONG",11);
        // knobs rij
        for(int k=0;k<8;++k){
            int x=14+(k%4)*134, y=118+(k/4)*120;
            int pi=k+1; if(pi>=8)pi=8; if(pi>7)pi=7;
            g.setColor(0x8b949eff); g.drawText(Rect(x,y,x+120,y+18),names_[k],11);
            // draaiknop cirkel
            float vv=v_[pi], mn=0, mx=100;
            if(pi==2){mn=20;mx=1000;} if(pi==3){mx=95;} if(pi==5){mn=1000;mx=18000;}
            if(pi==6){mn=20;mx=2000;} if(pi==7||pi==8){mx=26;}
            float n=(vv-mn)/(mx-mn); if(n<0)n=0;if(n>1)n=1;
            g.setColor(0x30363dff); g.fillCircle(x+40,y+56,26);
            g.setColor(acc);
            float ang=-2.35f+n*4.7f;
            g.drawLine(x+40,y+56,x+40+(int)(std::cos(ang)*22),y+56+(int)(std::sin(ang)*22),3);
            char lbl[24]; std::snprintf(lbl,24,"%.0f",vv);
            g.setColor(0xf0f6fcff); g.drawText(Rect(x+10,y+86,x+90,y+104),lbl,11);
        }
        // XY-pad rechts
        g.setColor(0x161b22ff); g.fillRect(560-120,64,104,104);
        g.setColor(acc); g.drawRect(560-120,64,104,104,2);
        float xx=std::fmod(v_[9],32.0f)/31.0f, yy=1-std::floor(v_[9]/32.0f)/31.0f;
        g.fillCircle(440+(int)(xx*104)-120+120-4+ (int)(xx*0), (int)(64+yy*104)-0,5);
        g.setColor(acc); g.fillCircle(440-120+120+(int)(xx*104)-4,(int)(64+yy*104),5);
        g.setColor(0x8b949eff); g.drawText(Rect(440-120+120-0,172,560,188),"XY tijd/feedback",9);
        // status
        g.setColor(0x8b949eff);
        char st[80]; std::snprintf(st,80,"tijd %.0f ms · fb %.0f%% · dim %d/27",v_[2],v_[3],(int)v_[7+0+0+0+0+0+0+0-0]>26?26:(int)v_[6]);
        g.drawText(Rect(14,372,540,392),st,10);
    }
private:
    float v_[10]={};
    const char* btn_[2]; const char* names_[8];
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BEchoUI)
};
UI* createUI(){ return new G2BEchoUI(); }
END_NAMESPACE_DISTRHO
