#include "DistrhoUI.hpp"
START_NAMESPACE_DISTRHO
static const int SK[8][7][3]={
 {16,14,40, 120,90,220, 200,180,255},
 {10,26,18, 40,160,110, 160,240,200},
 {26,16,14, 200,110,70, 255,200,160},
 {14,20,30, 70,140,200, 170,220,255},
 {30,24,10, 210,160,60, 255,220,150},
 {22,10,26, 180,80,180, 240,160,255},
 {10,16,30, 60,120,220, 150,200,255},
 {18,18,18, 140,140,140, 230,230,230}};
static const char* SNDNAMES[16]={"E-Piano","Rhodes","Orgel","Strings","Pad","Harp","Pluck","Marimba","Kalimba","Nylon","Steel","Sub Bas","Round Bas","Bell","Vibe","Chime"};
static const char* GRPNAMES[6]={"KEYS","STRINGS","PLUCK","GITAAR","BAS","BELLS"};
static const int GRPFIRST[6]={0,3,6,9,11,13};
static const int GRPCOUNT[6]={3,3,3,2,2,3};
class G2BChordUI : public UI {
public:
    G2BChordUI() : UI(560,430){ loadSharedResources(); std::memset(fVal,0,sizeof fVal); }
protected:
    void parameterChanged(uint32_t i,float v) override { if(i<14)fVal[i]=v; if(i==9)fSkin=(int)v&7; repaint(); }
    void uiIdle() override {}
    void onNanoDisplay() override {
        auto C=[](const int*c,float a=1.f){return Color(c[0],c[1],c[2],a);};
        const int(*S)[3]=SK[fSkin];
        beginPath();rect(0,0,getWidth(),getHeight());fillColor(C(S[0]));fill();
        fontSize(15);fillColor(C(S[2]));textAlign(ALIGN_LEFT|ALIGN_TOP);
        text(14,10,"G2B PRODUCER",nullptr);
        fontSize(10);fillColor(C(S[2],0.55f));
        text(140,13,"deuntjes-machine · AUTO=loopt zelf · ZELF=alleen keyboard",nullptr);
        // AAN pill
        bool on=fVal[0]>0.5f;
        { bool aut=fVal[12]>0.5f;
          beginPath();roundedRect(getWidth()-176,fBtnY-8,72,26,13);
          fillColor(aut?C(S[1]):C(S[1],0.3f));fill();
          fillColor(aut?Color(255,255,255):C(S[2],0.6f));
          textAlign(ALIGN_CENTER|ALIGN_MIDDLE);text(getWidth()-140,fBtnY+5,aut?"AUTO":"ZELF",nullptr); }
        beginPath();roundedRect(getWidth()-96,fBtnY-8,72,26,13);
        fillColor(on?C(S[1]):C(S[1],0.3f));fill();
        fillColor(on?Color(255,255,255):C(S[2],0.6f));
        textAlign(ALIGN_CENTER|ALIGN_MIDDLE);text(getWidth()-60,fBtnY+5,on?"AAN":"UIT",nullptr);
        // akkoord-wiel: huidige progressie als cirkel van 4 akkoorden
        float cx=110,cy=150,R=82;
        int key=(int)fVal[2], sc=(int)fVal[3], pg=(int)fVal[4];
        static const char* NAM[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        static const int SCAL[8][7]={{0,2,4,5,7,9,11},{0,2,3,5,7,8,10},{0,2,3,5,7,9,10},{0,1,3,5,7,8,10},{0,2,4,6,7,9,11},{0,2,4,5,7,9,10},{0,2,3,5,7,8,11},{0,2,3,5,7,9,11}};
        static const int PRG[16][4]={{0,5,3,4},{0,3,4,0},{5,3,0,4},{0,3,5,4},{0,4,5,3},{1,4,0,5},{5,4,0,3},{0,4,3,4},{2,5,0,4},{3,4,0,5},{0,2,3,4},{5,0,3,4},{0,3,0,4},{1,5,3,4},{0,4,5,5},{4,3,0,5}};
        static const char* SCH[8]={"majeur","mineur","dor","phryg","lyd","mixo","harm","mel"};
        static const char* VOI[4]={"triad","7th","9th","sus"};
        static const char* KLN[16]={"E-Piano","Rhodes","Orgel","Strings","Pad","Harp","Pluck","Marimba","Kalimba","Nylon","Steel","SubBas","RoundBas","Bell","Vibe","Chime"};
        for(int k=0;k<4;k++){
            float a=-1.5708f+k*1.5708f;
            float x=cx+cosf(a)*R, y=cy+sinf(a)*R;
            int deg=PRG[pg][k]; int root=(key+SCAL[sc][deg])%12;
            bool mino = (sc==1||sc==2||sc==3||sc==6||sc==7)&&(deg==0||deg==2||deg==5);
            beginPath();circle(x,y,22);fillColor(C(S[1],k==fCur?1.f:0.45f));fill();
            fillColor(Color(255,255,255));fontSize(11);
            char nm[8];snprintf(nm,8,"%s%s",NAM[root],mino?"m":"");
            text(x,y+1,nm,nullptr);
        }
        fillColor(C(S[2],0.7f));fontSize(10);
        char info[64];snprintf(info,64,"%s %s · %s · %s",NAM[key],SCH[sc],VOI[(int)fVal[5]],KLN[(int)fVal[10]%16]);
        textAlign(ALIGN_CENTER|ALIGN_TOP);text(cx,cy+R+18,info,nullptr);
        fCur=(fCur+1)%400;
        // XY-pad: kracht → · dimensies ↑
        fXX=16;fXY_=254;fXW=188;fXH=62;
        beginPath();roundedRect(fXX,fXY_,fXW,fXH,10);fillColor(C(S[1],0.25f));fill();strokeColor(C(S[2],0.25f));strokeWidth(1);stroke();
        { const float nx=fVal[1]/100.f, ny=1.f-fVal[8]/26.f;
          const float pxx=fXX+nx*fXW, pyy=fXY_+ny*fXH;
          beginPath();circle(pxx,pyy,6);fillColor(C(S[1]));fill();
          strokeColor(C(S[1],0.5f));strokeWidth(1);
          beginPath();moveTo(fXX,pyy);lineTo(fXX+fXW,pyy);stroke();
          beginPath();moveTo(pxx,fXY_);lineTo(pxx,fXY_+fXH);stroke(); }
        fillColor(C(S[2],0.6f));fontSize(8.5f);textAlign(ALIGN_CENTER|ALIGN_TOP);
        text(fXX+fXW/2,fXY_+fXH+5,"XY · kracht → dimensies ↑",nullptr);
        // knobs — 9 stuks in raster van 4
        static const char* LB[9]={"PRESET","TOON","LADDER","PROG","VOIC","TEMPO","KRACHT","DIMENTIES","GELUID"};
        for(int i=0;i<9;i++){
            float kx=fKnobX+(i%4)*86, ky=fKnobY+(i/4)*96;
            beginPath();circle(kx,ky,26);fillColor(C(S[1],0.35f));fill();
            beginPath();circle(kx,ky,26);strokeColor(C(S[2],0.25f));strokeWidth(3);stroke();
            float n=knNorm(i),a=-2.4f+n*4.8f;
            beginPath();moveTo(kx,ky);lineTo(kx+cosf(a)*22,ky+sinf(a)*22);strokeColor(C(S[2]));strokeWidth(3);stroke();
            fillColor(C(S[2],0.8f));fontSize(9);textAlign(ALIGN_CENTER|ALIGN_TOP);
            text(kx,ky+30,LB[i],nullptr);
            char vs[16];knValStr(i,vs);
            fontSize(10);fillColor(C(S[2]));text(kx,ky+40,vs,nullptr);
        }
        // klankkiezer — SOORT → GELUID (zoals normale VST's)
        {
          int g=(int)fVal[13]; if(g<0)g=0;if(g>5)g=5;
          int cur=(int)fVal[10];
          fontSize(9);fillColor(C(S[2],0.55f));textAlign(ALIGN_LEFT|ALIGN_TOP);
          text(222,fBrY-12,"SOORT",nullptr);
          text(312,fBrY-12,"GELUID",nullptr);
          for(int i=0;i<6;i++){
            float by=fBrY+i*19;
            bool on=(i==g);
            beginPath();roundedRect(222,by,84,17,4);
            fillColor(on?C(S[1]):C(S[1],0.3f));fill();
            fontSize(9.5f);fillColor(on?Color(255,255,255):C(S[2],0.65f));
            textAlign(ALIGN_CENTER|ALIGN_MIDDLE);text(264,by+8.5f,GRPNAMES[i],nullptr);
          }
          for(int i=0;i<GRPCOUNT[g];i++){
            int snd=GRPFIRST[g]+i; float by=fBrY+i*19;
            bool on=(snd==cur);
            beginPath();roundedRect(312,by,116,17,4);
            fillColor(on?C(S[1]):C(S[1],0.3f));fill();
            fontSize(9.5f);fillColor(on?Color(255,255,255):C(S[2],0.65f));
            textAlign(ALIGN_CENTER|ALIGN_MIDDLE);text(370,by+8.5f,SNDNAMES[snd],nullptr);
          }
        }
        // skin dots
        // dobbelsteen-knop: nieuwe deuntje+klank
        { beginPath();roundedRect(getWidth()-74,fSkinY,60,20,6);fillColor(C(S[1],0.4f));fill();
          strokeColor(C(S[2],0.3f));strokeWidth(1);stroke();
          fontSize(10);fillColor(C(S[2]));textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
          text(getWidth()-44,fSkinY+10,"🎲 GOOI",nullptr); }
        fillColor(C(S[2],0.5f));fontSize(9);textAlign(ALIGN_LEFT|ALIGN_TOP);text(14,fSkinY+3,"SKIN",nullptr);
        for(int i=0;i<8;i++){beginPath();circle(66+i*13,fSkinY+8,5);fillColor(C(SK[i][1]));fill();
            if(i==fSkin){beginPath();circle(66+i*13,fSkinY+8,7);strokeColor(Color(255,255,255));strokeWidth(1.5f);stroke();}}
    }
    bool onMouse(const MouseEvent&ev) override {
        if(!ev.press){if(fDrag>=0&&!fMoved){int i=fDrag;float mn=knMin(i),mx=knMax(i),st=(mx-mn)<=32?1.f:(mx-mn)/20.f;float v=fVal[knParam(i)]+st;if(v>mx)v=mn;setParameterValue(knParam(i),v);fVal[knParam(i)]=v;repaint();}fDrag=-1;fXYdrag=false;fMoved=false;return true;}
        int mx=ev.pos.getX(),my=ev.pos.getY();
        if(mx>getWidth()-100&&mx<getWidth()-20&&my>fBtnY-12&&my<fBtnY+22){float ov=fVal[0]>0.5f?0.f:1.f;setParameterValue(0,ov);fVal[0]=ov;repaint();return true;}
        if(mx>getWidth()-178&&mx<getWidth()-102&&my>fBtnY-12&&my<fBtnY+22){float mv=fVal[12]>0.5f?0.f:1.f;setParameterValue(12,mv);fVal[12]=mv;repaint();return true;}
        if(mx>=fXX&&mx<=fXX+fXW&&my>=fXY_&&my<=fXY_+fXH){fXYdrag=true;xySet(mx,my);return true;}
        for(int i=0;i<8;i++)if(fabs(mx-(66+i*13))<8&&fabs(my-(fSkinY+8))<10){setParameterValue(9,(float)i);return true;}
        if(mx>getWidth()-76&&mx<getWidth()-12&&my>fSkinY-2&&my<fSkinY+22){setParameterValue(11,1.0f);repaint();return true;}
        // klankkiezer: SOORT- of GELUID-pil geklikt
        if(mx>=222&&mx<=306&&my>=fBrY&&my<=fBrY+6*19){int g=(my-(int)fBrY)/19;setParameterValue(13,(float)g);fVal[13]=g;fVal[10]=GRPFIRST[g];repaint();return true;}
        if(mx>=312&&mx<=428&&my>=fBrY&&my<=fBrY+3*19){int g=(int)fVal[13];if(g<0)g=0;if(g>5)g=5;int i=(my-(int)fBrY)/19;if(i<GRPCOUNT[g]){float snd=(float)(GRPFIRST[g]+i);setParameterValue(10,snd);fVal[10]=snd;repaint();return true;}}
        for(int i=0;i<9;i++){float kx=fKnobX+(i%4)*86,ky=fKnobY+(i/4)*96;
            if((mx-kx)*(mx-kx)+(my-ky)*(my-ky)<30*30){fDrag=i;fLastY=my;fMoved=false;return true;}}
        return false;
    }
    bool onMotion(const MotionEvent&ev) override {
        if(fXYdrag){xySet((float)ev.pos.getX(),(float)ev.pos.getY());return true;}
        if(fDrag<0)return false;
        float dy=fLastY-(float)ev.pos.getY();fLastY=(float)ev.pos.getY();if(dy!=0)fMoved=true;
        float n=knNorm(fDrag)+dy*0.005f;
        if(n<0)n=0;if(n>1)n=1;
        float nv=knMin(fDrag)+n*(knMax(fDrag)-knMin(fDrag));setParameterValue(knParam(fDrag),nv);fVal[knParam(fDrag)]=nv;repaint();
        return true;
    }
private:
    void text(float x,float y,const char*t,std::nullptr_t){if(!t)return;DGL_NAMESPACE::NanoVG::text(x,y,t,nullptr);}
    static const int fKnobX=222, fKnobY=90, fBtnY=18, fSkinY=370;
    static constexpr float fBrY=250.0f;   // klankkiezer-paneel
    int knParam(int i)const{static const int pFor[9]={7,2,3,4,5,6,1,8,10};return pFor[i];}
    float knMin(int i)const{static const float m[9]={0,0,0,0,0,40,0,0,0};return m[i];}
    float knMax(int i)const{static const float m[9]={15,11,7,15,3,160,100,26,15};return m[i];}
    float knNorm(int i)const{int p=knParam(i);return (fVal[p]-knMin(i))/(knMax(i)-knMin(i));}
    void knValStr(int i,char*b)const{
        int p=knParam(i);float v=fVal[p];
        if(i==8){snprintf(b,16,"%s",SNDNAMES[(int)v%16]);}
        else snprintf(b,16,"%d",(int)v);
    }
    void xySet(float px,float py){float nx=(px-fXX)/fXW;if(nx<0)nx=0;if(nx>1)nx=1;float ny=(py-fXY_)/fXH;if(ny<0)ny=0;if(ny>1)ny=1;float kr=nx*100.f,dim=std::round((1.f-ny)*26.f);setParameterValue(1,kr);setParameterValue(8,dim);fVal[1]=kr;fVal[8]=dim;repaint();}
    float fVal[14]; int fSkin=0,fDrag=-1,fCur=0;float fLastY=0; bool fXYdrag=false,fMoved=false; float fXX=0,fXY_=0,fXW=0,fXH=0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BChordUI)
};
UI* createUI(){ return new G2BChordUI(); }
END_NAMESPACE_DISTRHO
