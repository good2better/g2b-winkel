/*
 * G2B Melodie — UI (NanoVG): 7 knoppen + preset-rij + DICE + AUTO + dark/light
 * + melodie-grid (16 stappen, hoogte = noot die echt klinkt).
 */

#include "DistrhoUI.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// G2B skins (8 thema's): bg,pn,tx,gy,ed,GR,DG
static const float G2BSK[8][7][3]={
    {{13,15,14},{23,26,25},{232,236,234},{100,110,103},{42,47,45},{47,191,113},{29,122,72}},
    {{14,14,22},{22,22,34},{230,232,255},{110,110,150},{40,40,58},{120,110,255},{70,60,170}},
    {{10,20,22},{16,30,32},{225,245,245},{95,130,130},{30,55,55},{40,200,210},{20,120,130}},
    {{24,14,20},{34,20,28},{250,230,240},{150,105,125},{60,38,50},{255,110,180},{180,60,120}},
    {{228,230,232},{240,242,244},{30,34,38},{105,112,118},{205,210,214},{40,140,100},{20,100,70}},
    {{18,10,10},{28,16,16},{240,225,220},{140,100,95},{55,35,32},{200,80,60},{130,45,35}},
    {{14,14,10},{22,22,16},{245,240,220},{120,115,95},{50,48,35},{230,190,80},{160,125,45}},
    {{12,14,20},{20,24,32},{235,240,250},{105,115,135},{36,44,58},{80,160,255},{45,110,200}}};
static const char* G2BSKN[8]={"MELIOR","SPECTRUM","LAGOON","CANDY","GHOST","MAROON","GOLD","STUDIO"};
static inline Color g2bsk(int sk,int r){if(sk<0)sk=0;if(sk>7)sk=7;const float*c=G2BSK[sk&7][r];return Color((int)c[0],(int)c[1],(int)c[2]);}

START_NAMESPACE_DISTRHO

static const int kNumPrograms = 17;
static const char* kProgramNames[kNumPrograms] = {
    "G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"
};
static const char* kKeyNames[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

class MelodieUI : public UI
{
public:
    MelodieUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        loadSharedResources();
        static const float mins[10]  = {0,0,0,0,0,0,0,0,0,0};
        static const float maxs[10]  = {5,11,1,2,1,0.5f,1,100,26,2};
        static const float defs[10]  = {0,0,0,1,0.35f,0.08f,0.8f,100,9,1};
        static const char* lbl[10]   = {"PATROON","KEY","SCALE","OCTAVE","TIMBRE","SWING","LEVEL","KRACHT","DIMENTIES","SFEER"};
        std::memcpy(fMin,mins,sizeof(mins));
        std::memcpy(fMax,maxs,sizeof(maxs));
        std::memcpy(fValue,defs,sizeof(defs));
        std::memcpy(fLabel,lbl,sizeof(lbl));
        std::memset(fScope,0,sizeof(fScope));
        for(int i=0;i<16;i++)fDeg[i]=-1;
        fScopePos=0;fDrag=-1;fLastY=0;fProg=0;fStep=-1;fAuto=0;fLight=0;
    }
protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if(index<7)fValue[index]=value;
        else if(index==8)fAuto=value;
        else if(index==9)fProg=(int)(value+0.5f);
        else if(index==10)fOn=(int)(value+0.5f);
        else if(index>=11&&index<=13)fValue[index-4]=value; // 11=KRACHT→7,12=DIMENTIES→8,13=SFEER→9
        repaint();
    }
    void stateChanged(const char* key, const char* value) override
    {
        if(std::strcmp(key,"scope")==0){
            const char* p=value;
            while(*p){
                char* end=nullptr;
                const float v=std::strtof(p,&end);
                if(end==p)break;
                fScope[fScopePos]=v;fScopePos=(fScopePos+1)%kScopeSize;
                p=(*end==';')?end+1:end;
            }
            repaint();
        } else if(std::strcmp(key,"step")==0){
            // "s:deg" — deg -1 = rust
            int s=-1,d=-1;
            if(std::sscanf(value,"%d:%d",&s,&d)==2){fStep=s;if(s>=0&&s<16)fDeg[s]=d;}
            else fStep=atoi(value);
            repaint();
        }
    }
    void onNanoDisplay() override
    {
        const float W=getWidth(),H=getHeight();
        // thema
        const Color bg   = g2bsk(fLight,0);
        const Color cell = g2bsk(fLight,1);
        const Color cellD= g2bsk(fLight,4);
        const Color txt  = g2bsk(fLight,2);
        const Color dim  = g2bsk(fLight,3);
        const Color dim2 = g2bsk(fLight,3);
        const Color edge = g2bsk(fLight,4);
        const Color knob = g2bsk(fLight,1);

        beginPath();rect(0,0,W,H);fillColor(bg);fill();
        fontSize(20.0f);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        fillColor(47,191,113);
        text(18.0f,22.0f,"G2B MELODIE",nullptr);
        fontSize(11.5f);fillColor(dim);
        text(18.0f,41.0f,"melodie-generator · altijd in key · dice = nieuwe hook",nullptr);

        // ☀/🌙 knopje naast titel
        {
            const float tx=W-40.0f,ty=46.0f,tw=26.0f,th=17.0f;
            beginPath();roundedRect(tx,ty,tw,th,4.0f);
            fillColor(cell);fill();strokeColor(edge);strokeWidth(1.0f);stroke();
            fontSize(10.0f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);fillColor(txt);
            text(tx+tw*0.5f,ty+th*0.5f,fLight>0?"🌙":"☀",nullptr);
        }

        // presets rechtsboven in rijen van 8 + DICE + AUTO links
        for(int i=0;i<kNumPrograms;++i){
            const float bw=44.0f,bh=17.0f;
            const int prow=i/8, pcol=i%8;
            const float px=W-48.0f-((prow<2?8:1)-pcol)*(bw+4.0f), py=12.0f+prow*18.0f;
            const bool on=(i==fProg);
            beginPath();roundedRect(px,py,bw,bh,4.0f);
            if(on){fillColor(29,122,72);fill();}
            strokeColor(edge);strokeWidth(1.0f);stroke();
            fontSize(8.0f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(on?Color(232,236,234):dim);
            text(px+bw*0.5f,py+bh*0.5f,kProgramNames[i],nullptr);
        }
        { // AUTO
            const float bw=38.0f,bx=14.0f+30.0f;
            beginPath();roundedRect(bx,48.0f,bw,17.0f,4.0f);
            if(fAuto>0.5f){fillColor(47,191,113);fill();}
            else{fillColor(cell);fill();}
            strokeColor(edge);strokeWidth(1.0f);stroke();
            fontSize(9.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(fAuto>0.5f?Color(13,15,14):dim);
            text(bx+bw*0.5f,56.5f,"AUTO",nullptr);
        }
        { // DICE
            const float bw=26.0f,bx=14.0f;
            beginPath();roundedRect(bx,48.0f,bw,17.0f,4.0f);
            fillColor(cell);fill();strokeColor(edge);strokeWidth(1.0f);stroke();
            fontSize(11.0f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);fillColor(txt);
            text(bx+bw*0.5f,56.5f,"🎲",nullptr);
        }

        // melodie-grid: 16 kolommen, stiphoogte = degree die klonk
        const float gx=18.0f,gy=70.0f,gw=W-36.0f,gh=58.0f;
        beginPath();rect(gx,gy,gw,gh);fillColor(cellD);fill();
        strokeColor(edge);strokeWidth(1.0f);stroke();
        const float cw=gw/16.0f;
        for(int s=0;s<16;s++){
            const float x=gx+s*cw;
            if(s%4==0){ // beat-markering
                beginPath();rect(x,gy,1.0f,gh);fillColor(edge);fill();
            }
            if(fDeg[s]>=0){
                const float hy=gy+gh-6.0f-(fDeg[s]/9.0f)*(gh-12.0f);
                const bool now=(s==fStep);
                beginPath();circle(x+cw*0.5f,hy,now?5.0f:3.5f);
                fillColor(now?Color(232,236,234):Color(47,191,113));fill();
                beginPath();moveTo(x+cw*0.5f,hy);lineTo(x+cw*0.5f,gy+gh-4.0f);
                strokeColor(29,122,72);strokeWidth(1.0f);stroke();
            } else {
                beginPath();rect(x+cw*0.5f-1.5f,gy+gh-5.0f,3.0f,2.0f);
                fillColor(cell);fill();
            }
            if(s==fStep){ // now-kolom
                beginPath();rect(x+1.0f,gy+1.0f,cw-2.0f,gh-2.0f);
                strokeColor(47,191,113);strokeWidth(1.0f);stroke();
            }
        }

        // scope onder grid
        const float sx=18.0f,sy=136.0f,sw=W-36.0f,sh=34.0f;
        beginPath();rect(sx,sy,sw,sh);fillColor(cellD);fill();
        strokeColor(edge);strokeWidth(1.0f);stroke();
        beginPath();
        const float mid=sy+sh*0.5f;
        for(int i=0;i<kScopeSize-1;++i){
            const int i0=(fScopePos+i)%kScopeSize,i1=(fScopePos+i+1)%kScopeSize;
            const float x0=sx+(float)i/(kScopeSize-1)*sw,x1=sx+(float)(i+1)/(kScopeSize-1)*sw;
            float y0=mid-fScope[i0]*sh*0.45f,y1=mid-fScope[i1]*sh*0.45f;
            if(y0<sy+1)y0=sy+1;if(y0>sy+sh-1)y0=sy+sh-1;
            if(y1<sy+1)y1=sy+1;if(y1>sy+sh-1)y1=sy+sh-1;
            if(i==0)moveTo(x0,y0);else lineTo(x0,y0);lineTo(x1,y1);
        }
        strokeColor(47,191,113);strokeWidth(1.5f);stroke();

        // AAN/UIT pil rechtsboven
        {
            const float ax=W-48.0f,ay=12.0f;
            beginPath();roundedRect(ax,ay,44.0f,17.0f,4.0f);
            fillColor(fOn?g2bsk(fLight,5):g2bsk(fLight,4));fill();
            strokeColor(edge);strokeWidth(1.0f);stroke();
            fontSize(8.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(fOn?Color(232,236,234):g2bsk(fLight,3));
            text(ax+22.0f,ay+8.5f,fOn?"AAN":"UIT",nullptr);
        }

        // knoppen
        const int n=10;
        const float step=(W-40.0f)/n;
        for(int i=0;i<n;++i){
            const float cx=20.0f+step*(i+0.5f),cy=215.0f,R=20.0f;
            const float norm=(fValue[i]-fMin[i])/(fMax[i]-fMin[i]);
            const float ang=-2.4f+norm*4.8f;
            beginPath();circle(cx,cy,R);fillColor(knob);fill();
            strokeColor(edge);strokeWidth(2.0f);stroke();
            beginPath();arc(cx,cy,R-4.0f,-2.4f-1.5708f,ang-1.5708f,CW);
            strokeColor(47,191,113);strokeWidth(3.0f);stroke();
            beginPath();moveTo(cx,cy);
            lineTo(cx+std::sin(ang)*(R-8.0f),cy-std::cos(ang)*(R-8.0f));
            strokeColor(txt);strokeWidth(3.0f);stroke();
            fontSize(9.5f);fillColor(dim);textAlign(ALIGN_CENTER|ALIGN_TOP);
            text(cx,cy+R+7.0f,fLabel[i],nullptr);
            fontSize(11.0f);fillColor(txt);
            char buf[24];
            if(i==0)std::snprintf(buf,sizeof(buf),"%s",kProgramNames[(int)fValue[0]<0?0:((int)fValue[0]>5?5:(int)fValue[0])]);
            else if(i==1)std::snprintf(buf,sizeof(buf),"%s",kKeyNames[((int)fValue[1]%12+12)%12]);
            else if(i==2)std::snprintf(buf,sizeof(buf),"%s",(int)fValue[2]==0?"minor":"major");
            else if(i==3)std::snprintf(buf,sizeof(buf),"%+d",(int)fValue[3]-1);
            else if(i==4&&fValue[4]>=0.96f)std::snprintf(buf,sizeof(buf),"PICCOLO");
            else if(i==4&&fValue[4]>=0.92f)std::snprintf(buf,sizeof(buf),"OBOE");
            else if(i==4&&fValue[4]>=0.88f)std::snprintf(buf,sizeof(buf),"BRASS");
            else if(i==4&&fValue[4]>=0.84f)std::snprintf(buf,sizeof(buf),"BASSOON");
            else if(i==4&&fValue[4]>=0.80f)std::snprintf(buf,sizeof(buf),"HARM");
            else if(i==4&&fValue[4]>=0.76f)std::snprintf(buf,sizeof(buf),"OCARINA");
            else if(i==4&&fValue[4]>=0.70f)std::snprintf(buf,sizeof(buf),"GLOCK");
            else if(i==4&&fValue[4]>=0.64f)std::snprintf(buf,sizeof(buf),"FLUIT");
            else if(i==4&&fValue[4]>=0.58f)std::snprintf(buf,sizeof(buf),"SAX");
            else if(i==4&&fValue[4]>=0.52f)std::snprintf(buf,sizeof(buf),"PSALTERY");
            else if(i==4&&fValue[4]>=0.46f)std::snprintf(buf,sizeof(buf),"MARIMBA");
            else if(i==7)std::snprintf(buf,sizeof(buf),"%.0f%%",fValue[i]);
            else if(i==8)std::snprintf(buf,sizeof(buf),"%.0f/26",fValue[i]);
            else if(i==9)std::snprintf(buf,sizeof(buf),"%s",(int)(fValue[i]+0.5f)==0?"DONKER":((int)(fValue[i]+0.5f)==2?"LICHT":"NEUTR"));
            else std::snprintf(buf,sizeof(buf),"%.0f %%",fValue[i]*100.0f);
            text(cx,cy+R+21.0f,buf,nullptr);
        }

        // XY-pad: X = KRACHT (0-100), Y = DIMENTIES (0-26)
        fXX=18.0f;fXY_=282.0f;fXW=W-36.0f;fXH=58.0f;
        {
            const float px=fXX,py=fXY_,pw=fXW,ph=fXH;
            beginPath();roundedRect(px,py,pw,ph,6.0f);
            fillColor(cellD);fill();
            strokeColor(edge);strokeWidth(1.0f);stroke();
            const float nx=fValue[7]/100.0f, ny=fValue[8]/26.0f;
            const float dx=px+nx*pw, dy=py+(1.0f-ny)*ph;
            beginPath();
            moveTo(dx,py);lineTo(dx,py+ph);
            moveTo(px,dy);lineTo(px+pw,dy);
            strokeColor(dim);strokeWidth(0.5f);stroke();
            beginPath();circle(dx,dy,5.0f);
            fillColor(g2bsk(fLight,5));fill();
            fontSize(9.0f);fillColor(dim);
            textAlign(ALIGN_LEFT|ALIGN_TOP);
            text(px+6.0f,py+4.0f,"DIMENTIES ",nullptr);
            textAlign(ALIGN_RIGHT|ALIGN_BOTTOM);
            text(px+pw-6.0f,py+ph-4.0f,"KRACHT →",nullptr);
        }
        fontSize(10.0f);fillColor(dim2);textAlign(ALIGN_CENTER|ALIGN_BOTTOM);
        text(W*0.5f,H-7.0f,"stip = noot die klinkt · 🎲 = nieuwe hook · AUTO = muteert elke 4 bars",nullptr);
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if(ev.button!=1)return false;
        const float W=getWidth();
        if(ev.press){
            // thema-knopje
            if(ev.pos.getX()>=W-40.0f&&ev.pos.getX()<=W-14.0f&&ev.pos.getY()>=46.0f&&ev.pos.getY()<=63.0f){
                fLight=(fLight+1)%8;repaint();return true;
            }
            // presets + dice + auto (zelfde geometrie als draw)
            for(int i=0;i<kNumPrograms;++i){
                const float bw=44.0f,bh=17.0f;
                const int prow=i/8, pcol=i%8;
                const float px=W-48.0f-((prow<2?8:1)-pcol)*(bw+4.0f), py=12.0f+prow*18.0f;
                if(ev.pos.getX()>=px&&ev.pos.getX()<=px+bw&&ev.pos.getY()>=py&&ev.pos.getY()<=py+bh){
                    setParameterValue(9,(float)i);fProg=i;repaint();return true;
                }
            }
            if(ev.pos.getX()>=44.0f&&ev.pos.getX()<=82.0f&&ev.pos.getY()>=48.0f&&ev.pos.getY()<=65.0f){
                fAuto=fAuto>0.5f?0.f:1.f;setParameterValue(8,fAuto);repaint();return true;}
            if(ev.pos.getX()>=14.0f&&ev.pos.getX()<=40.0f&&ev.pos.getY()>=48.0f&&ev.pos.getY()<=65.0f){
                setParameterValue(7,1.0f);repaint();return true;}
            // AAN/UIT pil
            if(ev.pos.getX()>=W-48.0f&&ev.pos.getX()<=W-4.0f&&ev.pos.getY()>=12.0f&&ev.pos.getY()<=29.0f){
                fOn=1-fOn;setParameterValue(10,(float)fOn);repaint();return true;}
            // XY-pad
            if(ev.pos.getX()>=fXX&&ev.pos.getX()<=fXX+fXW&&ev.pos.getY()>=fXY_&&ev.pos.getY()<=fXY_+fXH){
                fXYdrag=true;xySet(ev.pos.getX(),ev.pos.getY());return true;}
        }
        else if(fXYdrag){fXYdrag=false;return true;}
        const float step=(W-40.0f)/10;
        if(ev.press){
            for(int i=0;i<10;++i){
                const float cx=20.0f+step*(i+0.5f),cy=215.0f;
                const float dx=ev.pos.getX()-cx,dy=ev.pos.getY()-cy;
                if(dx*dx+dy*dy<=20.0f*20.0f+80.0f){fDrag=i;fLastY=ev.pos.getY();editParameter(paramOf(i),true);return true;}
            }
        } else if(fDrag>=0){editParameter(paramOf(fDrag),false);fDrag=-1;}
        return true;
    }

    // knop-slots: 0-6 = params 0-6, 7 KRACHT(11), 8 DIMENTIES(12), 9 SFEER(13)
    static int paramOf(int slot){ return slot<7?slot:slot+4; }
    void xySet(double mx,double my){
        float nx=(float)((mx-fXX)/fXW), ny=(float)(1.0-(my-fXY_)/fXH);
        if(nx<0)nx=0;if(nx>1)nx=1;if(ny<0)ny=0;if(ny>1)ny=1;
        const float kr=nx*100.0f, dm=ny*26.0f;
        fValue[7]=kr;fValue[8]=dm;
        editParameter(11,true);setParameterValue(11,kr);editParameter(11,false);
        editParameter(12,true);setParameterValue(12,dm);editParameter(12,false);
        repaint();
    }
    bool onMotion(const MotionEvent& ev) override
    {
        if(fXYdrag){xySet(ev.pos.getX(),ev.pos.getY());return true;}
        if(fDrag<0)return false;
        const float dy=fLastY-ev.pos.getY();fLastY=ev.pos.getY();
        const float range=fMax[fDrag]-fMin[fDrag];
        float v=fValue[fDrag]+dy*range/200.0f;
        if(v<fMin[fDrag])v=fMin[fDrag];if(v>fMax[fDrag])v=fMax[fDrag];
        // integer params afronden
        if(fDrag<=3)v=std::floor(v+0.5f);
        fValue[fDrag]=v;setParameterValue(paramOf(fDrag),v);repaint();return true;
    }
    bool onScroll(const ScrollEvent& ev) override
    {
        const float W=getWidth();const float step=(W-40.0f)/10;
        for(int i=0;i<10;++i){
            const float cx=20.0f+step*(i+0.5f),cy=215.0f;
            const float dx=ev.pos.getX()-cx,dy=ev.pos.getY()-cy;
            if(dx*dx+dy*dy<=20.0f*20.0f+80.0f){
                const float range=fMax[i]-fMin[i];
                float v=fValue[i]+ev.delta.getY()*range/100.0f;
                if(i<=3)v=std::floor(v+0.5f);
                if(v<fMin[i])v=fMin[i];if(v>fMax[i])v=fMax[i];
                fValue[i]=v;const int pi=paramOf(i);editParameter(pi,true);setParameterValue(pi,v);editParameter(pi,false);
                repaint();return true;
            }
        }
        return true;
    }
private:
    static const int kScopeSize=256;
    float fValue[10],fMin[10],fMax[10];
    const char* fLabel[10];
    float fScope[kScopeSize];int fScopePos;
    int fDeg[16];
    int fDrag;float fLastY;int fProg;int fStep;float fAuto;int fLight;
    int fOn=1; bool fXYdrag=false;
    float fXX=18.0f,fXY_=282.0f,fXW=524.0f,fXH=58.0f;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MelodieUI)
};

UI* createUI(){return new MelodieUI();}
END_NAMESPACE_DISTRHO
