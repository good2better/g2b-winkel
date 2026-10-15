// G2B Glue-Comp UI — compressor-curve visual + 8 knobs + toggles + skin.
#include "DistrhoUI.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

START_NAMESPACE_DISTRHO

class GlueUI : public UI
{
public:
    GlueUI() : UI(560,400)
    {
        loadSharedResources();
        static const float mins[K]={0,-40,1,0.1f,10,0,20,0}, maxs[K]={100,0,20,100,1000,24,300,26};
        static const float defs[K]={100,-18,4,10,150,0,60,8};
        static const char*  lbl[K]={"KRACHT","DREMPEL","RATIO","ATTACK","RELEASE","GAIN","SC-CUT","DIMENTIES"};
        static const int    pids[K]={1,2,3,4,5,6,7,8};
        std::memcpy(fMin,mins,sizeof(mins)); std::memcpy(fMax,maxs,sizeof(maxs));
        std::memcpy(fValue,defs,sizeof(defs)); std::memcpy(fLabel,lbl,sizeof(lbl));
        std::memcpy(fParamId,pids,sizeof(pids));
        fDrag=-1; fOn=1; fSkin=2;
    }

protected:
    void parameterChanged(uint32_t index, float value) override
    {
        for(int i=0;i<K;i++) if((int)index==fParamId[i]){fValue[i]=value;repaint();return;}
        if(index==0) fOn=value; else if(index==9) fSkin=(int)value;
        repaint();
    }
    void uiIdle() override {}

    void onNanoDisplay() override
    {
        const float W=getWidth(), H=getHeight();
        static const int SK[8][7][3]={
            {{13,15,14},{23,26,25},{232,236,234},{100,110,103},{42,47,45},{47,191,113},{29,122,72}},
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

        fontSize(15); textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        fillColor(C(ac)); text(16,22,"G2B GLUE-COMP",nullptr);
        fillColor(C(mu)); fontSize(11); text(150,22,"bus compressor · mix-lijm",nullptr);

        fBtnY=14;
        beginPath(); roundedRect(W-96,fBtnY-8,72,26,12); fillColor(fOn>0.5f?C(ad):C(pn)); fill();
        fillColor(C(fg)); fontSize(10); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
        text(W-60,fBtnY+5,fOn>0.5f?"AAN":"UIT",nullptr);

        // compressor-curve visualisatie links
        fXX=16;fXY_=52;fXW=170;fXH=170;
        beginPath(); roundedRect(fXX,fXY_,fXW,fXH,10);
        fillColor(C(pn)); fill(); strokeColor(C(cd)); strokeWidth(1); stroke();
        // assen: in -60..0 dB links→rechts, uit 0..-60 dB onder→boven
        const float thr=fValue[1], rat=fValue[2], knee=2.0f+(fValue[7]/26.f)*10.f;
        auto mapX=[&](float inDb){return fXX+(inDb+60.f)/60.f*fXW;};
        auto mapY=[&](float outDb){return fXY_+fXH-(outDb+60.f)/60.f*fXH;};
        // 1:1 grijze lijn
        beginPath(); moveTo(mapX(-60),mapY(-60)); lineTo(mapX(0),mapY(0));
        strokeColor(C(mu,0.35f)); strokeWidth(1); stroke();
        // echte curve
        beginPath();
        for(int px=0;px<=170;px++){
            float inDb=-60.f+px/170.f*60.f;
            float over=inDb-thr, outDb=inDb;
            if(over>knee/2.f) outDb=thr+over/rat;
            else if(over>-knee/2.f){float t0=over+knee/2.f; outDb=inDb+(1.f/rat-1.f)*t0*t0/(2.f*knee);}
            if(px==0) moveTo(mapX(inDb),mapY(outDb)); else lineTo(mapX(inDb),mapY(outDb));
        }
        strokeColor(C(ac)); strokeWidth(2.5f); stroke();
        // drempel-markering
        beginPath(); moveTo(mapX(thr),fXY_+4); lineTo(mapX(thr),fXY_+fXH-4);
        strokeColor(C(ad,0.6f)); strokeWidth(1); stroke();
        fillColor(C(mu)); fontSize(9); textAlign(ALIGN_CENTER|ALIGN_TOP);
        text(fXX+fXW/2,fXY_+fXH+6,"curve · in → uit (dB)",nullptr);
        char st[48]; std::snprintf(st,48,"%d dB · %d:1",(int)thr,(int)rat);
        fillColor(C(fg)); fontSize(10); text(fXX+fXW/2,fXY_+fXH+18,st,nullptr);

        // knobs: 4 x 2 grid
        fKnobX=222; fKnobY=90;
        for(int i=0;i<K;i++){
            const float cx=fKnobX+(i%4)*86, cy=fKnobY+(i/4)*96;
            beginPath(); circle(cx,cy,22); fillColor(C(pn)); fill();
            const float a=-2.35f+(fValue[i]-fMin[i])/(fMax[i]-fMin[i])*4.7f;
            beginPath(); moveTo(cx,cy); lineTo(cx+19*std::cos(a),cy+19*std::sin(a));
            strokeColor(C(ac)); strokeWidth(3); stroke();
            fillColor(C(mu)); fontSize(8.5f); textAlign(ALIGN_CENTER|ALIGN_TOP);
            text(cx,cy+28,fLabel[i],nullptr);
            fillColor(C(fg)); fontSize(10);
            char v[16];
            if(i==7) std::snprintf(v,16,"%d",(int)fValue[i]+1);
            else if(i==3||i==5) std::snprintf(v,16,"%d",(int)fValue[i]);
            else if(i==4) std::snprintf(v,16,"%d",(int)fValue[i]);
            else if(i==1) std::snprintf(v,16,"%d",(int)fValue[i]);
            else if(i==2) std::snprintf(v,16,"%.1f",fValue[i]);
            else if(i==6) std::snprintf(v,16,"%d",(int)fValue[i]);
            else std::snprintf(v,16,"%d",(int)fValue[i]);
            text(cx,cy+40,v,nullptr);
        }

        // skin-swatcher onder
        beginPath(); roundedRect(16,H-52,150,32,8); fillColor(C(pn)); fill();
        fillColor(C(mu)); fontSize(9); textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        text(26,H-36,"SKIN",nullptr); fSkinY=H-52;
        for(int i=0;i<8;i++){ beginPath(); circle(66+i*13,H-36,5); fillColor(C(SK[i][5])); fill(); }
    }

    bool onMouse(const MouseEvent& ev) override
    {
        const float W=getWidth();
        if(!ev.press){ fDrag=-1; return true; }
        if(ev.pos.getY()>=fBtnY-8&&ev.pos.getY()<=fBtnY+18&&ev.pos.getX()>=W-96){
            const float v=fOn>0.5f?0.f:1.f;setParameterValue(0,v);fOn=v;repaint();return true;}
        for(int i=0;i<K;i++){
            const float cx=fKnobX+(i%4)*86, cy=fKnobY+(i/4)*96;
            const float dx=ev.pos.getX()-cx, dy=ev.pos.getY()-cy;
            if(dx*dx+dy*dy<26*26){fDrag=i;fLastY=ev.pos.getY();return true;}
        }
        if(ev.pos.getY()>=fSkinY&&ev.pos.getY()<=fSkinY+32)
            for(int i=0;i<8;i++) if(std::fabs(ev.pos.getX()-(66+i*13))<8){setParameterValue(9,(float)i);fSkin=i;repaint();return true;}
        return true;
    }
    bool onMotion(const MotionEvent& ev) override
    {
        if(fDrag>=0){
            const float dv=(fLastY-ev.pos.getY())*(fMax[fDrag]-fMin[fDrag])/150.f;
            fLastY=ev.pos.getY();
            float v=fValue[fDrag]+dv;
            if(v<fMin[fDrag])v=fMin[fDrag]; if(v>fMax[fDrag])v=fMax[fDrag];
            setParameterValue(fParamId[fDrag],v);
            fValue[fDrag]=v; repaint();
            return true;
        }
        return false;
    }
private:
    static const int K=8;
    float fMin[K],fMax[K],fValue[K]; const char* fLabel[K]; int fParamId[K];
    int fDrag; float fLastY; float fOn,fSkin;
    float fBtnY=14,fKnobX=0,fKnobY=0,fSkinY=0; float fXX=0,fXY_=0,fXW=0,fXH=0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlueUI)
};

UI* createUI() { return new GlueUI(); }
END_NAMESPACE_DISTRHO
