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
class G2BChordUI : public UI {
public:
    G2BChordUI() : UI(560,390){ loadSharedResources(); std::memset(fVal,0,sizeof fVal); }
protected:
    void parameterChanged(uint32_t i,float v) override { if(i<10)fVal[i]=v; if(i==9)fSkin=(int)v&7; repaint(); }
    void uiIdle() override {}
    void onNanoDisplay() override {
        auto C=[](const int*c,float a=1.f){return Color(c[0],c[1],c[2],a);};
        const int(*S)[3]=SK[fSkin];
        beginPath();rect(0,0,getWidth(),getHeight());fillColor(C(S[0]));fill();
        fontSize(15);fillColor(C(S[2]));textAlign(ALIGN_LEFT|ALIGN_TOP);
        text(14,10,"G2B CHORDLAB",nullptr);
        fontSize(10);fillColor(C(S[2],0.55f));
        text(140,13,"akkoord-generator · e-piano",nullptr);
        // AAN pill
        bool on=fVal[0]>0.5f;
        beginPath();roundedRect(getWidth()-96,fBtnY-8,72,26,13);
        fillColor(on?C(S[1]):C(S[1],0.3f));fill();
        fillColor(on?Color(255,255,255):C(S[2],0.6f));
        textAlign(ALIGN_CENTER|ALIGN_MIDDLE);text(getWidth()-60,fBtnY+5,on?"AAN":"UIT",nullptr);
        // akkoord-wiel: huidige progressie als cirkel van 4 akkoorden
        float cx=110,cy=150,R=82;
        int key=(int)fVal[2], sc=(int)fVal[3], pg=(int)fVal[4];
        static const char* NAM[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        static const int SCAL[8][7]={{0,2,4,5,7,9,11},{0,2,3,5,7,8,10},{0,2,3,5,7,9,10},{0,1,3,5,7,8,10},{0,2,4,6,7,9,11},{0,2,4,5,7,9,10},{0,2,3,5,7,8,11},{0,2,3,5,7,9,11}};
        static const int PRG[6][4]={{0,5,3,4},{0,3,4,0},{5,3,0,4},{0,3,5,4},{0,4,5,3},{1,4,0,5}};
        static const char* SCH[8]={"majeur","mineur","dor","phryg","lyd","mixo","harm","mel"};
        static const char* VOI[4]={"triad","7th","9th","sus"};
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
        char info[64];snprintf(info,64,"%s %s · %s",NAM[key],SCH[sc],VOI[(int)fVal[5]]);
        textAlign(ALIGN_CENTER|ALIGN_TOP);text(cx,cy+R+18,info,nullptr);
        fCur=(fCur+1)%400; // pulse-animatie van huidige akkoord
        // knobs
        static const char* LB[8]={"PRESET","TOON","LADDER","PROG","VOIC","TEMPO","KRACHT","DIMS"};
        for(int i=0;i<7;i++){
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
        // skin dots
        fillColor(C(S[2],0.5f));fontSize(9);textAlign(ALIGN_LEFT|ALIGN_TOP);text(14,fSkinY+3,"SKIN",nullptr);
        for(int i=0;i<8;i++){beginPath();circle(66+i*13,fSkinY+8,5);fillColor(C(SK[i][1]));fill();
            if(i==fSkin){beginPath();circle(66+i*13,fSkinY+8,7);strokeColor(Color(255,255,255));strokeWidth(1.5f);stroke();}}
    }
    bool onMouse(const MouseEvent&ev) override {
        if(!ev.press){fDrag=-1;return true;}
        int mx=ev.pos.getX(),my=ev.pos.getY();
        if(mx>getWidth()-100&&mx<getWidth()-20&&my>fBtnY-12&&my<fBtnY+22){setParameterValue(0,fVal[0]>0.5f?0.f:1.f);return true;}
        for(int i=0;i<8;i++)if(fabs(mx-(66+i*13))<8&&fabs(my-(fSkinY+8))<10){setParameterValue(9,(float)i);return true;}
        for(int i=0;i<8;i++){float kx=fKnobX+(i%4)*86,ky=fKnobY+(i/4)*96;
            if((mx-kx)*(mx-kx)+(my-ky)*(my-ky)<30*30){fDrag=i;fLastY=my;return true;}}
        return false;
    }
    bool onMotion(const MotionEvent&ev) override {
        if(fDrag<0)return false;
        float dy=fLastY-(float)ev.pos.getY();fLastY=(float)ev.pos.getY();
        float n=knNorm(fDrag)+dy*0.005f;
        if(n<0)n=0;if(n>1)n=1;
        setParameterValue(knParam(fDrag),knMin(fDrag)+n*(knMax(fDrag)-knMin(fDrag)));
        return true;
    }
private:
    void text(float x,float y,const char*t,std::nullptr_t){if(!t)return;DGL_NAMESPACE::NanoVG::text(x,y,t,nullptr);}
    static const int fKnobX=222, fKnobY=90, fBtnY=18, fSkinY=330;
    int knParam(int i)const{static const int pFor[8]={7,2,3,4,5,6,1,8};return pFor[i];}
    float knMin(int i)const{static const float m[8]={0,0,0,0,0,40,0,0};return m[i];}
    float knMax(int i)const{static const float m[8]={15,11,7,5,3,160,100,26};return m[i];}
    float knNorm(int i)const{int p=knParam(i);return (fVal[p]-knMin(i))/(knMax(i)-knMin(i));}
    void knValStr(int i,char*b)const{
        int p=knParam(i);float v=fVal[p];
        if(i<5)snprintf(b,16,"%d",(int)v);else snprintf(b,16,"%d",(int)v);
    }
    float fVal[10]; int fSkin=0,fDrag=-1,fCur=0;float fLastY=0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BChordUI)
};
UI* createUI(){ return new G2BChordUI(); }
END_NAMESPACE_DISTRHO
