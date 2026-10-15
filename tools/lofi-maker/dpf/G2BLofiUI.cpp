#include "DistrhoUI.hpp"
START_NAMESPACE_DISTRHO
static const int SK[8][7][3]={
 {16,14,40, 120,90,220, 200,180,255},{10,26,18, 40,160,110, 160,240,200},
 {26,16,14, 200,110,70, 255,200,160},{14,20,30, 70,140,200, 170,220,255},
 {30,24,10, 210,160,60, 255,220,150},{22,10,26, 180,80,180, 240,160,255},
 {10,16,30, 60,120,220, 150,200,255},{18,18,18, 140,140,140, 230,230,230}};
static const char* LB[8]={"PRESET","CRUSH","GROF","WOBBLE","KRAAK","TONE","KRACHT","DIMS"};
static const int pFor[8]={7,2,3,4,5,6,1,8};
class G2BLofiUI : public UI {
public:
    G2BLofiUI():UI(560,390){loadSharedResources();std::memset(fVal,0,sizeof fVal);fVal[0]=1;fSkin=5;}
protected:
    void parameterChanged(uint32_t i,float v)override{if(i<9)fVal[i]=v;if(i==9)fSkin=(int)v&7;repaint();}
    void onNanoDisplay()override{
        auto C=[](const int*c,float a=1.f){return Color(c[0],c[1],c[2],a);};
        const int(*S)[3]=SK[fSkin];
        beginPath();rect(0,0,getWidth(),getHeight());fillColor(C(S[0]));fill();
        fontSize(15);fillColor(C(S[2]));textAlign(ALIGN_LEFT|ALIGN_TOP);text(14,10,"G2B LOFI-MAKER",nullptr);
        fontSize(10);fillColor(C(S[2],0.55f));text(150,13,"bitcrush · vinyl · wobble",nullptr);
        bool on=fVal[0]>0.5f;
        beginPath();roundedRect(getWidth()-96,fBtnY-8,72,26,13);fillColor(on?C(S[1]):C(S[1],0.3f));fill();
        fillColor(on?Color(255,255,255):C(S[2],0.6f));textAlign(ALIGN_CENTER|ALIGN_MIDDLE);text(getWidth()-60,fBtnY+5,on?"AAN":"UIT",nullptr);
        // vinyl-plaat visual met grooves + label
        float cx=110,cy=155;
        beginPath();circle(cx,cy,80);fillColor(C(S[1],0.25f));fill();
        for(int r=70;r>20;r-=9){beginPath();circle(cx,cy,r);strokeColor(C(S[2],0.3f));strokeWidth(1);stroke();}
        // kraak-stippen op de grooves (animeert)
        float crk=fVal[5]/100.f;
        for(int k=0;k<14;k++){float a=k*0.9+fTick*0.03f;float rr=30+(k%6)*8;
            float x=cx+cosf(a)*rr,y=cy+sinf(a)*rr;
            beginPath();circle(x,y,1.5f+crk*2);fillColor(C(S[2],0.5f));fill();}
        beginPath();circle(cx,cy,16);fillColor(C(S[1]));fill();
        fillColor(Color(255,255,255));fontSize(9);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);text(cx,cy,"G2B",nullptr);
        fTick++;
        char info[48];snprintf(info,48,"crush %d%% · kraak %d%%",(int)fVal[2],(int)fVal[5]);
        fillColor(C(S[2],0.7f));fontSize(10);textAlign(ALIGN_CENTER|ALIGN_TOP);text(cx,cy+88,info,nullptr);
        for(int i=0;i<8;i++){float kx=fKnobX+(i%4)*86,ky=fKnobY+(i/4)*96;
            beginPath();circle(kx,ky,26);fillColor(C(S[1],0.35f));fill();
            beginPath();circle(kx,ky,26);strokeColor(C(S[2],0.25f));strokeWidth(3);stroke();
            float n=knNorm(i),a=-2.4f+n*4.8f;
            beginPath();moveTo(kx,ky);lineTo(kx+cosf(a)*22,ky+sinf(a)*22);strokeColor(C(S[2]));strokeWidth(3);stroke();
            fillColor(C(S[2],0.8f));fontSize(9);textAlign(ALIGN_CENTER|ALIGN_TOP);text(kx,ky+30,LB[i],nullptr);
            char vs[16];snprintf(vs,16,"%d",(int)fVal[pFor[i]]);
            fontSize(10);fillColor(C(S[2]));text(kx,ky+40,vs,nullptr);}
        fillColor(C(S[2],0.5f));fontSize(9);textAlign(ALIGN_LEFT|ALIGN_TOP);text(14,fSkinY+3,"SKIN",nullptr);
        for(int i=0;i<8;i++){beginPath();circle(66+i*13,fSkinY+8,5);fillColor(C(SK[i][1]));fill();
            if(i==fSkin){beginPath();circle(66+i*13,fSkinY+8,7);strokeColor(Color(255,255,255));strokeWidth(1.5f);stroke();}}
    }
    bool onMouse(const MouseEvent&ev)override{
        if(!ev.press){fDrag=-1;return true;}
        int mx=ev.pos.getX(),my=ev.pos.getY();
        if(mx>getWidth()-100&&mx<getWidth()-20&&my>fBtnY-12&&my<fBtnY+22){setParameterValue(0,fVal[0]>0.5f?0.f:1.f);return true;}
        for(int i=0;i<8;i++)if(fabs(mx-(66+i*13))<8&&fabs(my-(fSkinY+8))<10){setParameterValue(9,(float)i);return true;}
        for(int i=0;i<8;i++){float kx=fKnobX+(i%4)*86,ky=fKnobY+(i/4)*96;
            if((mx-kx)*(mx-kx)+(my-ky)*(my-ky)<30*30){fDrag=i;fLastY=my;return true;}}
        return false;
    }
    bool onMotion(const MotionEvent&ev)override{
        if(fDrag<0)return false;
        float dy=fLastY-(float)ev.pos.getY();fLastY=(float)ev.pos.getY();
        float n=knNorm(fDrag)+dy*0.005f;if(n<0)n=0;if(n>1)n=1;
        setParameterValue(pFor[fDrag],n*100.f);
        return true;
    }
private:
    void text(float x,float y,const char*t,std::nullptr_t){if(!t)return;DGL_NAMESPACE::NanoVG::text(x,y,t,nullptr);}
    static const int fKnobX=222,fKnobY=90,fBtnY=18,fSkinY=330;
    float knNorm(int i)const{return fVal[pFor[i]]/100.f;}
    float fVal[9];int fSkin=0,fDrag=-1,fTick=0;float fLastY=0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BLofiUI)
};
UI* createUI(){ return new G2BLofiUI(); }
END_NAMESPACE_DISTRHO
