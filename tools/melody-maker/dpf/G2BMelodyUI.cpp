#include "DistrhoUI.hpp"
START_NAMESPACE_DISTRHO
static const int SK[8][7][3]={
 {16,14,40, 120,90,220, 200,180,255},{10,26,18, 40,160,110, 160,240,200},
 {26,16,14, 200,110,70, 255,200,160},{14,20,30, 70,140,200, 170,220,255},
 {30,24,10, 210,160,60, 255,220,150},{22,10,26, 180,80,180, 240,160,255},
 {10,16,30, 60,120,220, 150,200,255},{18,18,18, 140,140,140, 230,230,230}};
static const char* LB[10]={"PRESET","TOON","LADDER","PROG","PATROON","DENS","OCT","SWING","TEMPO","DIMENTIES"};
static const int pFor[10]={9,2,3,4,5,6,7,8,10,11};
static const float pMin[10]={0,0,0,0,0,0,-2,0,60,0};
static const float pMax[10]={15,11,7,5,7,100,2,100,180,26};
static const char* PRESET_NAMES[16]={
 "Pop-Arp C","Emotie Am","Trap-Bas","House-F","Dark-Phryg","Lydian-Dream","LoFi-Dust","Mixo-Groove",
 "Harm-Min","Deep-Bas","Echo-Pluck","Phryg-Fire","Slow-Soul","Anthem-G","Mel-Min-Walk","Drive-Bas"};
class G2BMelodyUI : public UI {
public:
    G2BMelodyUI():UI(560,390){loadSharedResources();std::memset(fVal,0,sizeof fVal);fVal[0]=1;fSkin=1;}
protected:
    void parameterChanged(uint32_t i,float v)override{if(i<13)fVal[i]=v;if(i==12)fSkin=(int)v&7;repaint();}
    void onNanoDisplay()override{
        auto C=[](const int*c,float a=1.f){return Color(c[0],c[1],c[2],a);};
        const int(*S)[3]=SK[fSkin];
        beginPath();rect(0,0,getWidth(),getHeight());fillColor(C(S[0]));fill();
        fontSize(15);fillColor(C(S[2]));textAlign(ALIGN_LEFT|ALIGN_TOP);text(14,10,"G2B MELODY-MAKER",nullptr);
        fontSize(10);fillColor(C(S[2],0.55f));text(160,13,"melodie · baslijn · 16 presets",nullptr);
        bool on=fVal[0]>0.5f;
        beginPath();roundedRect(getWidth()-96,fBtnY-8,72,26,13);fillColor(on?C(S[1]):C(S[1],0.3f));fill();
        fillColor(on?Color(255,255,255):C(S[2],0.6f));textAlign(ALIGN_CENTER|ALIGN_MIDDLE);text(getWidth()-60,fBtnY+5,on?"AAN":"UIT",nullptr);
        // piano-roll visual: 8 stappen met noot-hoogtes
        float px=26,py=52,pw=160,phh=140;
        beginPath();roundedRect(px,py,pw,phh,8);fillColor(C(S[1],0.18f));fill();
        int pat=(int)fVal[5]+((int)fVal[11]>=13?1:0);if(pat>7)pat-=8;
        static const int PAT[8][8]={{0,1,2,1,0,1,2,4},{0,2,4,7,4,2,0,-2},{0,2,4,7,9,7,4,2},{7,4,2,0,-3,0,2,4},
          {0,2,0,4,0,2,4,2},{0,0,4,0,0,4,0,2},{0,0,0,4,0,0,7,0},{0,3,2,5,4,1,2,4}};
        for(int s=0;s<8;s++){
            float nx=px+10+s*18;float h=PAT[pat][s];
            float ny=py+phh*0.5f-h*7;
            beginPath();roundedRect(nx,ny-4,13,8,3);
            fillColor(C(S[2],s==fStep?1.f:0.55f));fill();
        }
        fStep=(fStep+1)%80;
        char info[48];snprintf(info,48,"%s · %d%%",PRESET_NAMES[(int)fVal[9]&15],(int)fVal[6]);
        fillColor(C(S[2],0.75f));fontSize(10);textAlign(ALIGN_CENTER|ALIGN_TOP);text(px+pw/2,py+phh+8,info,nullptr);
        // XY-pad: kracht → · dimensies ↑
        fXX=26;fXY_=214;fXW=160;fXH=100;
        beginPath();roundedRect(fXX,fXY_,fXW,fXH,10);fillColor(C(S[1],0.18f));fill();strokeColor(C(S[2],0.25f));strokeWidth(1);stroke();
        { const float nx=fVal[1]/100.f, ny=1.f-fVal[11]/26.f;
          const float pxx=fXX+nx*fXW, pyy=fXY_+ny*fXH;
          beginPath();circle(pxx,pyy,6);fillColor(C(S[1]));fill();
          strokeColor(C(S[1],0.5f));strokeWidth(1);
          beginPath();moveTo(fXX,pyy);lineTo(fXX+fXW,pyy);stroke();
          beginPath();moveTo(pxx,fXY_);lineTo(pxx,fXY_+fXH);stroke(); }
        fillColor(C(S[2],0.6f));fontSize(8.5f);textAlign(ALIGN_CENTER|ALIGN_TOP);
        text(fXX+fXW/2,fXY_+fXH+5,"XY · kracht → dimensies ↑",nullptr);
        for(int i=0;i<10;i++){float kx=fKnobX+(i%5)*70,ky=fKnobY+(i/5)*96;
            beginPath();circle(kx,ky,22);fillColor(C(S[1],0.35f));fill();
            beginPath();circle(kx,ky,22);strokeColor(C(S[2],0.25f));strokeWidth(3);stroke();
            float n=knNorm(i),a=-2.4f+n*4.8f;
            beginPath();moveTo(kx,ky);lineTo(kx+cosf(a)*18,ky+sinf(a)*18);strokeColor(C(S[2]));strokeWidth(3);stroke();
            fillColor(C(S[2],0.8f));fontSize(8);textAlign(ALIGN_CENTER|ALIGN_TOP);text(kx,ky+26,LB[i],nullptr);
            char vs[16];snprintf(vs,16,"%d",(int)fVal[pFor[i]]);
            fontSize(9);fillColor(C(S[2]));text(kx,ky+36,vs,nullptr);}
        // KRACHT als verticale mini-slider rechts-onder
        fillColor(C(S[2],0.5f));fontSize(9);textAlign(ALIGN_LEFT|ALIGN_TOP);text(14,fSkinY+3,"SKIN",nullptr);
        for(int i=0;i<8;i++){beginPath();circle(66+i*13,fSkinY+8,5);fillColor(C(SK[i][1]));fill();
            if(i==fSkin){beginPath();circle(66+i*13,fSkinY+8,7);strokeColor(Color(255,255,255));strokeWidth(1.5f);stroke();}}
    }
    bool onMouse(const MouseEvent&ev)override{
        if(!ev.press){if(fDrag>=0&&!fMoved){int i=fDrag;float mn=pMin[i],mx=pMax[i],st=(mx-mn)<=32?1.f:(mx-mn)/20.f;float v=fVal[pFor[i]]+st;if(v>mx)v=mn;spv(pFor[i],v);fVal[pFor[i]]=v;repaint();}fDrag=-1;fXYdrag=false;fMoved=false;return true;}
        int mx=ev.pos.getX(),my=ev.pos.getY();
        if(mx>getWidth()-100&&mx<getWidth()-20&&my>fBtnY-12&&my<fBtnY+22){float ov=fVal[0]>0.5f?0.f:1.f;spv(0,ov);fVal[0]=ov;repaint();return true;}
        if(mx>=fXX&&mx<=fXX+fXW&&my>=fXY_&&my<=fXY_+fXH){fXYdrag=true;xySet(mx,my);return true;}
        for(int i=0;i<8;i++)if(fabs(mx-(66+i*13))<8&&fabs(my-(fSkinY+8))<10){spv(12,(float)i);return true;}
        for(int i=0;i<10;i++){float kx=fKnobX+(i%5)*70,ky=fKnobY+(i/5)*96;
            if((mx-kx)*(mx-kx)+(my-ky)*(my-ky)<26*26){fDrag=i;fLastY=my;fMoved=false;return true;}}
        return false;
    }
    bool onMotion(const MotionEvent&ev)override{
        if(fXYdrag){xySet((float)ev.pos.getX(),(float)ev.pos.getY());return true;}
        if(fDrag<0)return false;
        float dy=fLastY-(float)ev.pos.getY();fLastY=(float)ev.pos.getY();if(dy!=0)fMoved=true;
        float n=knNorm(fDrag)+dy*0.005f;if(n<0)n=0;if(n>1)n=1;
        float nv=pMin[fDrag]+n*(pMax[fDrag]-pMin[fDrag]);spv(pFor[fDrag],nv);fVal[pFor[fDrag]]=nv;repaint();
        return true;
    }
private:
    void text(float x,float y,const char*t,std::nullptr_t){if(!t)return;DGL_NAMESPACE::NanoVG::text(x,y,t,nullptr);}
    static const int fKnobX=222,fKnobY=82,fBtnY=18,fSkinY=330;
    float knNorm(int i)const{return (fVal[pFor[i]]-pMin[i])/(pMax[i]-pMin[i]);}
    void xySet(float px,float py){float nx=(px-fXX)/fXW;if(nx<0)nx=0;if(nx>1)nx=1;float ny=(py-fXY_)/fXH;if(ny<0)ny=0;if(ny>1)ny=1;float kr=nx*100.f,dim=std::round((1.f-ny)*26.f);spv(1,kr);spv(11,dim);fVal[1]=kr;fVal[11]=dim;repaint();}
    float fVal[13];int fSkin=0,fDrag=-1,fStep=0;float fLastY=0; bool fXYdrag=false,fMoved=false; float fXX=0,fXY_=0,fXW=0,fXH=0;
    void spv(uint32_t i,float v){setParameterValue(i,v);parameterChanged(i,v);}
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BMelodyUI)
};
UI* createUI(){return new G2BMelodyUI();}
END_NAMESPACE_DISTRHO
