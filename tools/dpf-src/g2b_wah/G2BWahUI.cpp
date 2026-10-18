// G2B Filter-Wah UI — wah-curve visual + 7 knobs + AAN + 8 skins.
#include "DistrhoUI.hpp"
START_NAMESPACE_DISTRHO
static const char* lbl[8]={"SKIN","RATE","DEPTH","RES","SENS","FREQ","KRACHT","DIMS"};
static const int pFor[8]={8,2,3,4,5,6,1,7};
class WahUI:public UI{
public:WahUI():UI(560,400){loadSharedResources();fDrag=-1;fOn=1;fSkin=4;}
protected:
    void parameterChanged(uint32_t index,float v)override{if(index==0)fOn=v;else if(index==8){fSkin=(int)v;fVal[0]=v;}else for(int i=0;i<8;i++)if(pFor[i]==(int)index)fVal[i]=v;repaint();}
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
        fillColor(C(ac)); text(16,22,"G2B FILTER-WAH",nullptr);
        fillColor(C(mu)); fontSize(11); text(165,22,"auto-wah · envelope · veeg",nullptr);
        beginPath(); roundedRect(W-96,fBtnY-8,72,26,12); fillColor(fOn>0.5f?C(ad):C(pn)); fill();
        fillColor(C(fg)); fontSize(10); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
        text(W-60,fBtnY+5,fOn>0.5f?"AAN":"UIT",nullptr); textAlign(ALIGN_LEFT|ALIGN_TOP);
        const float px=16,py=52,pw=188,ph=150;
        beginPath(); roundedRect(px,py,pw,ph,10); fillColor(C(cd,0.5f)); fill(); strokeColor(C(pn)); strokeWidth(1); stroke();
        float res=0.1f+fVal[3]/100*0.85f,depth=fVal[2]/100,sw=0.5f+0.5f*std::sin(6.2832f*0.3f);
        for(int x=0;x<=(int)pw;x+=4){
            float xi=x/(float)pw,f0=0.5f+3.5f*depth*(1-xi);
            float amp=res*2/(0.12f+std::fabs(xi-f0*0.3f));
            float y=py+ph-amp*28;if(y<py+8)y=py+8;if(y>py+ph-4)y=py+ph-4;
            beginPath(); circle(px+x,y,1.6f); fillColor(C(ac)); fill();}
        float cur=px+pw*(0.15f+sw*0.5f);
        beginPath(); circle(cur,py+ph-30,4); fillColor(C(ad)); fill();
        char lb[40];std::snprintf(lb,40,"veeg %.0f · res %.0f",fVal[1],fVal[3]);
        fillColor(C(ac)); fontSize(10); text(px+8,py+ph+16,lb,nullptr);
        // XY-pad: kracht → · dimensies ↑
        fXX=px;fXY_=py+ph+34;fXW=pw;fXH=104;
        beginPath(); roundedRect(fXX,fXY_,fXW,fXH,10);
        fillColor(C(pn)); fill(); strokeColor(C(cd)); strokeWidth(1); stroke();
        { const float nx=fVal[6]/100.f, ny=1.f-fVal[7]/26.f;
          const float pxx=fXX+nx*fXW, pyy=fXY_+ny*fXH;
          beginPath(); circle(pxx,pyy,6); fillColor(C(ac)); fill();
          strokeColor(C(ac,0.35f)); strokeWidth(1);
          beginPath(); moveTo(fXX,pyy); lineTo(fXX+fXW,pyy); stroke();
          beginPath(); moveTo(pxx,fXY_); lineTo(pxx,fXY_+fXH); stroke(); }
        fillColor(C(mu)); fontSize(8.5f); textAlign(ALIGN_CENTER|ALIGN_TOP);
        text(fXX+fXW/2,fXY_+fXH+5,"XY · kracht → dimensies ↑",nullptr);
        textAlign(ALIGN_LEFT|ALIGN_TOP);
        fKnobX=222;fKnobY=90;
        for(int i=0;i<8;i++){
            const float cx=fKnobX+(i%4)*86,cy=fKnobY+(i/4)*96;
            beginPath(); circle(cx,cy,22); fillColor(C(pn)); fill();
            float n=norm(i),a=-2.35f+n*4.7f;
            strokeColor(C(ac)); strokeWidth(3);
            beginPath(); moveTo(cx,cy); lineTo(cx+19*std::cos(a),cy+19*std::sin(a)); stroke();
            fillColor(C(mu)); fontSize(8.5f); textAlign(ALIGN_CENTER|ALIGN_TOP);
            text(cx,cy+28,lbl[i],nullptr);
            char v[24];std::snprintf(v,24,"%.0f",fVal[i]);
            fillColor(C(fg)); fontSize(10); text(cx,cy+40,v,nullptr); textAlign(ALIGN_LEFT|ALIGN_TOP);}
        fillColor(C(mu)); fontSize(9); text(26,H-36,"SKIN",nullptr); fSkinY=H-52;
        for(int i=0;i<8;i++){beginPath(); circle(66+i*13,fSkinY+8,5); fillColor(C(SK[i][5])); fill();}
    }
    bool onMouse(const MouseEvent&ev)override{
        if(!ev.press){if(fDrag>=0&&!fMoved){int i=fDrag;float mx=i==0?7:(i==7?26:100);float st=mx<=32?1:5;float v=fVal[i]+st;if(v>mx)v=0;spv(pFor[i],v);fVal[i]=v;repaint();}fDrag=-1;fXYdrag=false;fMoved=false;return true;}
        if((float)(ev.pos.getY()/getScaleFactor())<42&&(float)(ev.pos.getX()/getScaleFactor())>getWidth()-116){spv(0,fOn>0.5f?0.f:1.f);fOn=fOn>0.5f?0:1;repaint();return true;}
        if((float)(ev.pos.getX()/getScaleFactor())>=fXX&&(float)(ev.pos.getX()/getScaleFactor())<=fXX+fXW&&(float)(ev.pos.getY()/getScaleFactor())>=fXY_&&(float)(ev.pos.getY()/getScaleFactor())<=fXY_+fXH){fXYdrag=true;xySet(ev.pos);return true;}
        for(int i=0;i<8;i++){
            const float cx=fKnobX+(i%4)*86,cy=fKnobY+(i/4)*96;
            const float dx=(float)(ev.pos.getX()/getScaleFactor())-cx,dy=(float)(ev.pos.getY()/getScaleFactor())-cy;
            if(dx*dx+dy*dy<26*26){fDrag=i;fLastY=(float)(ev.pos.getY()/getScaleFactor());fMoved=false;return true;}}
        if((float)(ev.pos.getY()/getScaleFactor())>=fSkinY&&(float)(ev.pos.getY()/getScaleFactor())<=fSkinY+32)
            for(int i=0;i<8;i++) if(std::fabs((float)(ev.pos.getX()/getScaleFactor())-(66+i*13))<8){spv(8,(float)i);fSkin=i;repaint();return true;}
        return false;}
    bool onMotion(const MotionEvent&ev)override{
        if(fXYdrag){xySet(ev.pos);return true;}
        if(fDrag<0)return false;
        float dy=fLastY-(float)(ev.pos.getY()/getScaleFactor());fLastY=(float)(ev.pos.getY()/getScaleFactor());if(dy!=0)fMoved=true;
        float v=fVal[fDrag]+dy*100/120.0f;
        if(v<0)v=0;if(v>100)v=100;if(fDrag==0){if(v>7)v=7;}if(fDrag==7){if(v>26)v=26;}
        fVal[fDrag]=v;spv(pFor[fDrag],v);
        repaint();return true;}
    bool onScroll(const ScrollEvent&)override{return false;}
private:
    float norm(int i){return fVal[i]/(i==0?7:(i==7?26:100));}
    void text(float x,float y,const char*s,std::nullptr_t){if(!s)return;DGL_NAMESPACE::NanoVG::text(x,y,s,nullptr);}
    void xySet(const Point<double>&p){float nx=(p.getX()-fXX)/fXW;if(nx<0)nx=0;if(nx>1)nx=1;float ny=(p.getY()-fXY_)/fXH;if(ny<0)ny=0;if(ny>1)ny=1;float kr=nx*100.f,dim=std::round((1.f-ny)*26.f);spv(1,kr);spv(7,dim);fVal[6]=kr;fVal[7]=dim;repaint();}
    float fVal[8]={4,30,70,60,50,50,100,0};
    int fDrag; float fLastY; float fOn,fSkin; bool fXYdrag=false,fMoved=false;
    float fBtnY=14,fKnobX=0,fKnobY=0,fSkinY=0; float fXX=0,fXY_=0,fXW=0,fXH=0;
    void spv(uint32_t i,float v){setParameterValue(i,v);parameterChanged(i,v);}
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WahUI)};
END_NAMESPACE_DISTRHO
DISTRHO::UI*DISTRHO::createUI(){return new DISTRHO::WahUI();}
