// G2B Chorus UI — LFO-golf visual + 7 knobs + AAN + 8 skins (zelfde stijl als Glue-Comp).
#include "DistrhoUI.hpp"
START_NAMESPACE_DISTRHO
static const char* lbl[7]={"RATE","DEPTH","DELAY","FEED","WIDTH","KRACHT","DIMENTIES"};
static const int pFor[7]={2,3,4,5,6,1,7};
static const char* dims3(int d){if(d<9)return "clean";if(d<18)return "warm";return "thick";}
class ChorusUI:public UI{
public:ChorusUI():UI(560,400){loadSharedResources();fDrag=-1;fOn=1;fSkin=2;}
protected:
    void parameterChanged(uint32_t index,float v)override{if(index==0)fOn=v;else if(index==8)fSkin=(int)v;else for(int i=0;i<7;i++)if(pFor[i]==(int)index)fVal[i]=v;repaint();}
    void uiIdle()override{fT+=0.06f;repaint();}
    void onNanoDisplay()override{
        const float W=getWidth(),H=getHeight();
        static const int SK[8][7][3]={
            {{10,14,20},{18,26,36},{235,240,255},{105,115,160},{36,48,70},{80,140,255},{50,90,200}},
            {{14,14,22},{22,22,34},{230,232,255},{110,110,150},{40,40,58},{120,110,255},{70,60,170}},
            {{10,20,22},{16,30,32},{225,245,245},{95,130,130},{30,55,55},{40,200,210},{20,120,130}},
            {{24,14,10},{36,22,16},{250,230,215},{160,110,90},{60,35,25},{240,140,60},{150,80,30}},
            {{20,10,22},{32,16,36},{245,225,245},{150,100,150},{55,30,58},{200,90,220},{120,50,140}},
            {{8,14,24},{14,24,40},{215,235,255},{90,120,160},{25,45,70},{60,150,255},{30,90,180}},
            {{24,20,8},{38,32,14},{255,245,215},{165,140,80},{70,55,25},{235,190,70},{150,115,30}},
            {{15,15,15},{28,28,28},{240,240,240},{120,120,120},{48,48,48},{235,60,60},{140,30,30}},
        };
        const int s=(int)fSkin&7;
        const int *bg=SK[s][0],*pn=SK[s][1],*fg=SK[s][2],*mu=SK[s][3],*cd=SK[s][4],*ac=SK[s][5],*ad=SK[s][6];
        auto C=[](const int*c,float a=1.f){return Color(c[0],c[1],c[2],a);};
        beginPath(); rect(0,0,W,H); fillColor(C(bg)); fill();
        fillColor(C(ac)); text(16,22,"G2B CHORUS",nullptr);
        fillColor(C(mu)); fontSize(11); text(150,22,"stereo breedte · lfo-taps",nullptr);
        beginPath(); roundedRect(W-96,fBtnY-8,72,26,12); fillColor(fOn>0.5f?C(ad):C(pn)); fill();
        fillColor(C(fg)); fontSize(10); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
        text(W-60,fBtnY+5,fOn>0.5f?"AAN":"UIT",nullptr); textAlign(ALIGN_LEFT|ALIGN_TOP);
        // LFO-visual links
        const float px=16,py=52,pw=188,ph=150;
        beginPath(); roundedRect(px,py,pw,ph,10); fillColor(C(cd,0.5f)); fill(); strokeColor(C(pn)); strokeWidth(1); stroke();
        float dep=fVal[1]/100,rt=fVal[0];
        for(int ch=0;ch<2;ch++){
            beginPath();
            for(int x=0;x<(int)pw;x+=4){float t=x/(float)pw*6.2831853f*2*(0.2f+rt*0.35f)+fT+ch*1.5707f;
                float y=py+ph*0.5f-(std::sin(t)*0.5f*dep*ph*0.7f);
                if(x==0)moveTo(px+x,y);else lineTo(px+x,y);}
            strokeColor(ch?C(mu,0.6f):C(ac)); strokeWidth(2); stroke();}
        char lb[48];std::snprintf(lb,48,"%.1f Hz · depth %.0f%%",rt,dep*100);
        fillColor(C(ac)); fontSize(10); text(px+8,py+ph+16,lb,nullptr);
        // knobs 4+3
        fKnobX=222;fKnobY=90;
        for(int i=0;i<7;i++){
            const float cx=fKnobX+(i%4)*86,cy=fKnobY+(i/4)*96;
            beginPath(); circle(cx,cy,22); fillColor(C(pn)); fill();
            float n=norm(i),a=-2.35f+n*4.7f;
            strokeColor(C(ac)); strokeWidth(3);
            beginPath(); moveTo(cx,cy); lineTo(cx+19*std::cos(a),cy+19*std::sin(a)); stroke();
            fillColor(C(mu)); fontSize(8.5f); textAlign(ALIGN_CENTER|ALIGN_TOP);
            text(cx,cy+28,lbl[i],nullptr);
            char v[24];
            if(i==0)std::snprintf(v,24,"%.1f",fVal[i]);else if(i==6)std::snprintf(v,24,"%s",dims3((int)fVal[i]));else std::snprintf(v,24,"%.0f",fVal[i]);
            fillColor(C(fg)); fontSize(10); text(cx,cy+40,v,nullptr); textAlign(ALIGN_LEFT|ALIGN_TOP);}
        // skin-swatcher
        fillColor(C(mu)); fontSize(9); text(26,H-36,"SKIN",nullptr); fSkinY=H-52;
        for(int i=0;i<8;i++){beginPath(); circle(66+i*13,fSkinY+8,5); fillColor(C(SK[i][5])); fill();}
    }
    bool onMouse(const MouseEvent&ev)override{
        if(!ev.press){fDrag=-1;return true;}
        if(ev.pos.getY()<42&&ev.pos.getX()>getWidth()-116){setParameterValue(0,fOn>0.5f?0.f:1.f);fOn=fOn>0.5f?0:1;repaint();return true;}
        for(int i=0;i<7;i++){
            const float cx=fKnobX+(i%4)*86,cy=fKnobY+(i/4)*96;
            const float dx=ev.pos.getX()-cx,dy=ev.pos.getY()-cy;
            if(dx*dx+dy*dy<26*26){fDrag=i;fLastY=ev.pos.getY();return true;}}
        if(ev.pos.getY()>=fSkinY&&ev.pos.getY()<=fSkinY+32)
            for(int i=0;i<8;i++) if(std::fabs(ev.pos.getX()-(66+i*13))<8){setParameterValue(8,(float)i);fSkin=i;repaint();return true;}
        return false;}
    bool onMotion(const MotionEvent&ev)override{
        if(fDrag<0)return false;
        float dy=fLastY-ev.pos.getY();fLastY=ev.pos.getY();
        float v=fVal[fDrag]+dy*(maxOf(fDrag)-fMin[fDrag])/120.0f;
        float mx=maxOf(fDrag);
        if(v<fMin[fDrag])v=fMin[fDrag]; if(v>mx)v=mx;
        fVal[fDrag]=v;setParameterValue(pFor[fDrag],v);
        repaint();return true;}
    bool onScroll(const ScrollEvent&)override{return false;}
private:
    float norm(int i){float mx=maxOf(i);return mx>fMin[i]?(fVal[i]-fMin[i])/(mx-fMin[i]):0;}
    float maxOf(int i){const float m[7]={8,100,40,95,100,100,26};return m[i];}
    void text(float x,float y,const char*s,std::nullptr_t){if(!s)return;DGL_NAMESPACE::NanoVG::text(x,y,s,nullptr);}
    float fVal[7]={0.8f,50,15,0,70,70,0},fMin[7]={0.05f,0,5,0,0,0,0};
    int fDrag; float fLastY; float fOn,fSkin;float fT=0;
    float fBtnY=14,fKnobX=0,fKnobY=0,fSkinY=0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChorusUI)};
END_NAMESPACE_DISTRHO
DISTRHO::UI*DISTRHO::createUI(){return new DISTRHO::ChorusUI();}
