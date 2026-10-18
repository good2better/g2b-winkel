// G2B Echo-Kamer UI — XY-pad (tijd x feedback) + knobs + toggles + skin.
#include "DistrhoUI.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

START_NAMESPACE_DISTRHO

class EchoUI : public UI
{
public:
    EchoUI() : UI(560,400)
    {
        loadSharedResources();
        static const float mins[K]={0,0,0,1000,20,0,0}, maxs[K]={100,1000,95,18000,2000,26,15};
        static const float defs[K]={40,375,40,12000,120,13,0};
        static const char*  lbl[K]={"KRACHT","TIJD","FEEDBACK","LOWPASS","HIGHPASS","DIMENTIES","PRESET"};
        static const int    pids[K]={1,2,3,6,7,8,10};
        std::memcpy(fMin,mins,sizeof(mins)); std::memcpy(fMax,maxs,sizeof(maxs));
        std::memcpy(fValue,defs,sizeof(defs)); std::memcpy(fLabel,lbl,sizeof(lbl));
        std::memcpy(fParamId,pids,sizeof(pids));
        fDrag=-1; fOn=1; fPP=0; fSkin=0; fXY=false;
    }

protected:
    void parameterChanged(uint32_t index, float value) override
    {
        for(int i=0;i<K;i++) if((int)index==fParamId[i]){fValue[i]=value;repaint();return;}
        if(index==0) fOn=value; else if(index==4) fPP=value; else if(index==9) fSkin=(int)value;
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

        // kop
        fontSize(15); textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        fillColor(C(ac)); text(16,22,"G2B ECHO-KAMER",nullptr);
        fillColor(C(mu)); fontSize(11); text(160,22,"stereo delay · pingpong · ducking",nullptr);

        // toggles rechts
        fBtnY=14;
        beginPath(); roundedRect(W-164,fBtnY-8,68,26,12); fillColor(fOn>0.5f?C(ad):C(pn)); fill();
        fillColor(C(fg)); fontSize(10); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
        text(W-130,fBtnY+5,fOn>0.5f?"AAN":"UIT",nullptr);
        beginPath(); roundedRect(W-88,fBtnY-8,72,26,12); fillColor(fPP>0.5f?C(ac):C(pn)); fill();
        fillColor(fPP>0.5f?C(bg):C(fg)); text(W-52,fBtnY+5,"PINGPONG",nullptr);

        // echo-tail visualisatie: 8 herhalingen decay volgens feedback
        const float ey=64, eh=44;
        beginPath(); roundedRect(16,ey,W-32,eh,8); fillColor(C(pn)); fill();
        const float fb=fValue[2]/95.f;
        for(int i=0;i<8;i++){
            const float h=(eh-12)*std::pow(fb,i)*(0.7f+0.3f*std::sin(i*1.7f+1));
            const float x=30+i*(W-72)/7.f;
            beginPath(); rect(x,ey+eh-6-h,8,h);
            fillColor(C(ac,1.f-i*0.1f)); fill();
        }

        // XY-pad
        fXX=16;fXY_=ey+eh+14;fXW=170;fXH=150;
        beginPath(); roundedRect(fXX,fXY_,fXW,fXH,10);
        fillColor(C(pn)); fill(); strokeColor(C(cd)); strokeWidth(1); stroke();
        const float nx=(fValue[1]-20)/980.f, ny=1-fb;
        const float px=fXX+nx*fXW, py=fXY_+ny*fXH;
        beginPath(); circle(px,py,7); fillColor(C(ac)); fill();
        strokeColor(C(ac,0.4f)); strokeWidth(1);
        beginPath(); moveTo(fXX,py); lineTo(fXX+fXW,py); stroke();
        beginPath(); moveTo(px,fXY_); lineTo(px,fXY_+fXH); stroke();
        fillColor(C(mu)); fontSize(9); textAlign(ALIGN_CENTER|ALIGN_TOP);
        text(fXX+fXW/2,fXY_+fXH+6,"XY · tijd → feedback ↑",nullptr);
        char st[48]; std::snprintf(st,48,"%d ms · %d%%",(int)fValue[1],(int)fValue[2]);
        fillColor(C(fg)); fontSize(10); text(fXX+fXW/2,fXY_+fXH+18,st,nullptr);

        // knobs: 3 x 2 grid rechts van XY
        fKnobX=210; fKnobY=fXY_+24;
        for(int i=0;i<K;i++){
            const float cx=fKnobX+(i%3)*116, cy=fKnobY+(i/3)*88;
            beginPath(); circle(cx,cy,22); fillColor(C(pn)); fill();
            const float a=-2.35f+(fValue[i]-fMin[i])/(fMax[i]-fMin[i])*4.7f;
            beginPath(); moveTo(cx,cy); lineTo(cx+19*std::cos(a),cy+19*std::sin(a));
            strokeColor(C(ac)); strokeWidth(3); stroke();
            fillColor(C(mu)); fontSize(9); textAlign(ALIGN_CENTER|ALIGN_TOP);
            text(cx,cy+28,fLabel[i],nullptr);
            fillColor(C(fg)); fontSize(10);
            char v[16];
            if(i==5) std::snprintf(v,16,"%d",(int)fValue[i]+1);
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
        if(!ev.press){if(fDrag>=0&&!fMoved){int i=fDrag;float mn=fMin[i],mx=fMax[i],st=(mx-mn)<=32?1.f:(mx-mn)/20.f;float v=fValue[i]+st;if(v>mx)v=mn;spv(fParamId[i],v);fValue[i]=v;repaint();}fDrag=-1;fXY=false;fMoved=false;return true;}
        // toggles
        if((float)(ev.pos.getY()/getScaleFactor())>=fBtnY-8&&(float)(ev.pos.getY()/getScaleFactor())<=fBtnY+18){
            if((float)(ev.pos.getX()/getScaleFactor())>=W-164&&(float)(ev.pos.getX()/getScaleFactor())<=W-96){const float v=fOn>0.5f?0.f:1.f;spv(0,v);fOn=v;repaint();return true;}
            if((float)(ev.pos.getX()/getScaleFactor())>=W-88){const float v=fPP>0.5f?0.f:1.f;spv(4,v);fPP=v;repaint();return true;}
        }
        // XY
        if(((float)(ev.pos.getX()/getScaleFactor())>=fXX&&(float)(ev.pos.getX()/getScaleFactor())<=fXX+fXW&&(float)(ev.pos.getY()/getScaleFactor())>=fXY_&&(float)(ev.pos.getY()/getScaleFactor())<=fXY_+fXH)){ fXY=true; xySet(ev.pos); return true; }
        // knobs
        for(int i=0;i<K;i++){
            const float cx=fKnobX+(i%3)*116, cy=fKnobY+(i/3)*88;
            const float dx=(float)(ev.pos.getX()/getScaleFactor())-cx, dy=(float)(ev.pos.getY()/getScaleFactor())-cy;
            if(dx*dx+dy*dy<26*26){fDrag=i;fLastY=(float)(ev.pos.getY()/getScaleFactor());fMoved=false;return true;}
        }
        // skin dots
        if((float)(ev.pos.getY()/getScaleFactor())>=fSkinY&&(float)(ev.pos.getY()/getScaleFactor())<=fSkinY+32)
            for(int i=0;i<8;i++) if(std::fabs((float)(ev.pos.getX()/getScaleFactor())-(66+i*13))<8){spv(9,(float)i);fSkin=i;repaint();return true;}
        return true;
    }
    bool onMotion(const MotionEvent& ev) override
    {
        if(fXY){ xySet(ev.pos); return true; }
        if(fDrag>=0){fMoved=true;
            const float dv=(fLastY-(float)(ev.pos.getY()/getScaleFactor()))*(fMax[fDrag]-fMin[fDrag])/150.f;
            fLastY=(float)(ev.pos.getY()/getScaleFactor());
            float v=fValue[fDrag]+dv;
            if(v<fMin[fDrag])v=fMin[fDrag]; if(v>fMax[fDrag])v=fMax[fDrag];
            spv(fParamId[fDrag],v);
            fValue[fDrag]=v; repaint();   // standalone echoot niet terug — zelf spiegelen
            return true;
        }
        return false;
    }
private:
    static const int K=7;
    void xySet(const Point<double>& p){
        float nx=(p.getX()-fXX)/fXW; if(nx<0)nx=0;if(nx>1)nx=1;
        float ny=(p.getY()-fXY_)/fXH; if(ny<0)ny=0;if(ny>1)ny=1;
        spv(2,20+nx*980);       // tijd
        spv(3,(1-ny)*95);       // feedback
        fValue[1]=20+nx*980; fValue[2]=(1-ny)*95; repaint();
    }
    float fMin[K],fMax[K],fValue[K]; const char* fLabel[K]; int fParamId[K];
    int fDrag; bool fMoved=false; float fLastY; float fOn,fPP,fSkin; bool fXY;
    float fBtnY=14,fKnobX=0,fKnobY=0,fSkinY=0; float fXX=0,fXY_=0,fXW=0,fXH=0;
    void spv(uint32_t i,float v){setParameterValue(i,v);parameterChanged(i,v);}
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EchoUI)
};

UI* createUI() { return new EchoUI(); }
END_NAMESPACE_DISTRHO
