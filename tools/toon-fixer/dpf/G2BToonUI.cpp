// G2B Toon-Fixer UI — toonsoort/ladder kiezer + knobs + XY + skin.
#include "DistrhoUI.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

START_NAMESPACE_DISTRHO

class ToonUI : public UI
{
public:
    ToonUI() : UI(560,400)
    {
        loadSharedResources();
        static const float mins[K]={0,0,0,-12,0}, maxs[K]={100,11,3,12,26};
        static const float defs[K]={60,0,0,0,9};
        static const char*  lbl[K]={"KRACHT","TOONSOORT","LADDER","SHIFT","DIMENTIES"};
        static const int    pids[K]={1,2,3,4,5};
        std::memcpy(fMin,mins,sizeof(mins)); std::memcpy(fMax,maxs,sizeof(maxs));
        std::memcpy(fValue,defs,sizeof(defs)); std::memcpy(fLabel,lbl,sizeof(lbl));
        std::memcpy(fParamId,pids,sizeof(pids));
        fDrag=-1; fOn=1; fSkin=0; fXY=false;
    }

protected:
    void parameterChanged(uint32_t index, float value) override
    {
        for(int i=0;i<K;i++) if((int)index==fParamId[i]){fValue[i]=value;repaint();return;}
        if(index==0) fOn=value; else if(index==6) fSkin=(int)value;
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
        fillColor(C(ac)); text(16,22,"G2B TOON-FIXER",nullptr);
        fillColor(C(mu)); fontSize(11); text(160,22,"pitch-correctie · toonsoort-snap",nullptr);

        fBtnY=14;
        beginPath(); roundedRect(W-88,fBtnY-8,72,26,12); fillColor(fOn>0.5f?C(ad):C(pn)); fill();
        fillColor(C(fg)); fontSize(10); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
        text(W-52,fBtnY+5,fOn>0.5f?"AAN":"UIT",nullptr);

        // grote toonsoort-display
        static const char* NM[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        static const char* SC[4]={"majeur","mineur","pent maj","pent min"};
        const float dy=56, dh=58;
        beginPath(); roundedRect(16,dy,W-32,dh,10); fillColor(C(pn)); fill();
        fillColor(C(mu)); fontSize(10); textAlign(ALIGN_CENTER|ALIGN_TOP);
        text(W/2,dy+8,"CORRECTIE-LADDER",nullptr);
        char key[32]; std::snprintf(key,32,"%s %s",NM[(int)fValue[1]&11],SC[(int)fValue[2]&3]);
        fillColor(C(ac)); fontSize(24); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
        text(W/2,dy+36,key,nullptr);
        // shift-indicator rechts
        if((int)fValue[3]!=0){
            char sh[24]; std::snprintf(sh,24,"shift %+d st",(int)fValue[3]);
            fillColor(C(fg)); fontSize(11); textAlign(ALIGN_RIGHT|ALIGN_MIDDLE);
            text(W-26,dy+36,sh,nullptr);
        }

        // XY-pad: kracht (x) x dimensie (y)
        fXX=16;fXY_=dy+dh+14;fXW=170;fXH=150;
        beginPath(); roundedRect(fXX,fXY_,fXW,fXH,10);
        fillColor(C(pn)); fill(); strokeColor(C(cd)); strokeWidth(1); stroke();
        const float nx=fValue[0]/100.f, ny=1-fValue[4]/26.f;
        const float px=fXX+nx*fXW, py=fXY_+ny*fXH;
        beginPath(); circle(px,py,7); fillColor(C(ac)); fill();
        strokeColor(C(ac,0.4f)); strokeWidth(1);
        beginPath(); moveTo(fXX,py); lineTo(fXX+fXW,py); stroke();
        beginPath(); moveTo(px,fXY_); lineTo(px,fXY_+fXH); stroke();
        fillColor(C(mu)); fontSize(9); textAlign(ALIGN_CENTER|ALIGN_TOP);
        text(fXX+fXW/2,fXY_+fXH+6,"XY · kracht → dimensie ↑",nullptr);
        const char* dz = fValue[4]<9.f?"NATUURLIJK":(fValue[4]<18.f?"MODERN":"ROBOT");
        char st[48]; std::snprintf(st,48,"%d%% · %s",(int)fValue[0],dz);
        fillColor(C(fg)); fontSize(10); text(fXX+fXW/2,fXY_+fXH+18,st,nullptr);

        // dimensie-band legenda
        fillColor(C(mu)); fontSize(9); textAlign(ALIGN_LEFT|ALIGN_TOP);
        text(20,fXY_+fXH+34,"1-9 natuurlijk · 10-18 modern · 19-27 robot",nullptr);

        // knobs: 3 boven + 2 onder rechts van XY
        fKnobX=226; fKnobY=fXY_+22;
        for(int i=0;i<K;i++){
            const float cx=fKnobX+(i%3)*110, cy=fKnobY+(i/3)*86;
            beginPath(); circle(cx,cy,22); fillColor(C(pn)); fill();
            const float a=-2.35f+(fValue[i]-fMin[i])/(fMax[i]-fMin[i])*4.7f;
            beginPath(); moveTo(cx,cy); lineTo(cx+19*std::cos(a),cy+19*std::sin(a));
            strokeColor(C(ac)); strokeWidth(3); stroke();
            fillColor(C(mu)); fontSize(9); textAlign(ALIGN_CENTER|ALIGN_TOP);
            text(cx,cy+28,fLabel[i],nullptr);
            fillColor(C(fg)); fontSize(10);
            char v[16];
            if(i==1) std::snprintf(v,16,"%s",NM[(int)fValue[i]&11]);
            else if(i==2) std::snprintf(v,16,"%d",(int)fValue[i]+1);
            else if(i==3) std::snprintf(v,16,"%+d",(int)fValue[i]);
            else if(i==4) std::snprintf(v,16,"%d",(int)fValue[i]+1);
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
        if(!ev.press){ fDrag=-1; fXY=false; return true; }
        if(ev.pos.getY()>=fBtnY-8&&ev.pos.getY()<=fBtnY+18){
            if(ev.pos.getX()>=W-88){const float v=fOn>0.5f?0.f:1.f;setParameterValue(0,v);fOn=v;repaint();return true;}
        }
        if((ev.pos.getX()>=fXX&&ev.pos.getX()<=fXX+fXW&&ev.pos.getY()>=fXY_&&ev.pos.getY()<=fXY_+fXH)){ fXY=true; xySet(ev.pos); return true; }
        for(int i=0;i<K;i++){
            const float cx=fKnobX+(i%3)*110, cy=fKnobY+(i/3)*86;
            const float dx=ev.pos.getX()-cx, dy=ev.pos.getY()-cy;
            if(dx*dx+dy*dy<26*26){fDrag=i;fLastY=ev.pos.getY();return true;}
        }
        if(ev.pos.getY()>=fSkinY&&ev.pos.getY()<=fSkinY+32)
            for(int i=0;i<8;i++) if(std::fabs(ev.pos.getX()-(66+i*13))<8){setParameterValue(6,(float)i);fSkin=i;repaint();return true;}
        return true;
    }
    bool onMotion(const MotionEvent& ev) override
    {
        if(fXY){ xySet(ev.pos); return true; }
        if(fDrag>=0){
            float dv=(fLastY-ev.pos.getY())*(fMax[fDrag]-fMin[fDrag])/150.f;
            fLastY=ev.pos.getY();
            // stepped knobs: key/scale/shift/dimensie ronden af
            if(fDrag>=1){ dv = dv>0? std::max(dv,1.f):std::min(dv,-1.f); dv=std::round(dv); }
            float v=fValue[fDrag]+dv;
            if(v<fMin[fDrag])v=fMin[fDrag]; if(v>fMax[fDrag])v=fMax[fDrag];
            setParameterValue(fParamId[fDrag],v);
            fValue[fDrag]=v; repaint();
            return true;
        }
        return false;
    }
private:
    static const int K=5;
    void xySet(const Point<double>& p){
        float nx=(p.getX()-fXX)/fXW; if(nx<0)nx=0;if(nx>1)nx=1;
        float ny=(p.getY()-fXY_)/fXH; if(ny<0)ny=0;if(ny>1)ny=1;
        setParameterValue(1,nx*100);
        setParameterValue(5,std::round((1-ny)*26));
        fValue[0]=nx*100; fValue[4]=std::round((1-ny)*26); repaint();
    }
    float fMin[K],fMax[K],fValue[K]; const char* fLabel[K]; int fParamId[K];
    int fDrag; float fLastY; float fOn,fSkin; bool fXY;
    float fBtnY=14,fKnobX=0,fKnobY=0,fSkinY=0; float fXX=0,fXY_=0,fXW=0,fXH=0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToonUI)
};

UI* createUI() { return new ToonUI(); }
END_NAMESPACE_DISTRHO
