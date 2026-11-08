/*
 * G2B MIXER — UI (NanoVG): 6 knoppen + 6 instrument-profielen + MAAK MIX-KLAAR.
 * Sleep = draaien, scroll = fijn, profiel-pil = startpunt, MIX-KLAAR luistert 5s.
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
static inline Color g2bsk(int sk,int r){if(sk<0)sk=0;if(sk>7)sk=7;const float*c=G2BSK[sk][r];return Color((int)c[0],(int)c[1],(int)c[2]);}

START_NAMESPACE_DISTRHO

static const int kNumProfs = 6;
static const char* kProfNames[kNumProfs] = {"DRUMS","808/BASS","KEYS","MELODIE","PERC","FX/LOOP"};
// UI-kopie: HP TILT GLUE WARMTE
static const float kUiProfParams[kNumProfs][4] = {
    {40.f,0.f,55.f,35.f},{20.f,-15.f,65.f,45.f},{60.f,5.f,40.f,30.f},
    {80.f,10.f,45.f,25.f},{100.f,15.f,35.f,20.f},{50.f,0.f,50.f,40.f}};

class G2BMixerUI : public UI
{
public:
    G2BMixerUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        loadSharedResources();
        static const float mins[6] = {-24,20,-50,0,0,-24};
        static const float maxs[6] = {24,400,50,100,100,24};
        static const float defs[6] = {0,40,0,55,35,0};
        static const char* lbl[6]  = {"TRIM","HP","TILT","GLUE","WARMTE","LUID"};
        static const char* unt[6]  = {"dB","Hz","%","%","%","dB"};
        std::memcpy(fMin,mins,sizeof(mins));
        std::memcpy(fMax,maxs,sizeof(maxs));
        std::memcpy(fValue,defs,sizeof(defs));
        std::memcpy(fTarget,defs,sizeof(defs));
        std::memcpy(fLabel,lbl,sizeof(lbl));
        std::memcpy(fUnit,unt,sizeof(unt));
        std::memset(fScope,0,sizeof(fScope));
        fScopePos=0;fDrag=-1;fLastY=0.f;fProf=0;
        applyUiProf(0);
        for(int p=0;p<6;++p)fValue[p]=fTarget[p];
    }
protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if(index<6) fTarget[index]=value;
        else if(index==7){
            fProf=(int)(value+0.5f);if(fProf<0)fProf=0;if(fProf>5)fProf=5;
            fTarget[1]=kUiProfParams[fProf][0];
            fTarget[2]=kUiProfParams[fProf][1];
            fTarget[3]=kUiProfParams[fProf][2];
            fTarget[4]=kUiProfParams[fProf][3];
        }
        else if(index==8) fOn=value;
        else if(index==9) fKracht=value;
        else if(index==11) fLufsM=value;
        else if(index==12) fLufsI=value;
        else if(index==13){fScope[fScopePos]=value;fScopePos=(fScopePos+1)%kScopeSize;}
        else if(index==14) fGr=value;
        else if(index==15) fBusy=value;
        else if(index==16) fRuimte=value;
        else if(index==17) fBusN=value;
        else if(index==18) fPrio=value;
        repaint();
    }

    void onNanoDisplay() override
    {
        const float W=getWidth(),H=getHeight();
        beginPath();rect(0,0,W,H);fillColor(g2bsk(fLight,0));fill();

        fontSize(22.0f);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        fillColor(g2bsk(fLight,5));
        text(18.0f,22.0f,"G2B MIXER v2",nullptr);
        fontSize(9.0f);fillColor(g2bsk(fLight,3));
        text(20.0f,40.0f,"slimme channel-strip — één spoor klaarmaken voor je mix",nullptr);

        // profiel-pills (1 rij van 6)
        for(int i=0;i<kNumProfs;++i){
            const float bw=104.0f,bh=20.0f,gap=8.0f;
            const float rowW=kNumProfs*bw+(kNumProfs-1)*gap;
            const float bx=(W-rowW)*0.5f+i*(bw+gap);
            const float by=58.0f;
            const bool on=(i==fProf);
            beginPath();roundedRect(bx,by,bw,bh,5.0f);
            if(on){fillColor(g2bsk(fLight,5));fill();}
            else{fillColor(g2bsk(fLight,1));fill();}
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            fontSize(10.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(on?g2bsk(fLight,0):g2bsk(fLight,2));
            text(bx+bw*0.5f,by+bh*0.5f,kProfNames[i],nullptr);
        }

        // scope
        const float sx=18.0f,sy=88.0f,sw=W-36.0f,sh=28.0f;
        beginPath();rect(sx,sy,sw,sh);
        fillColor(fLight==4?Color(30,34,38):Color(8,10,9));fill();
        strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
        beginPath();
        const float mid=sy+sh*0.5f;
        for(int i=0;i<kScopeSize-1;++i){
            const int i0=(fScopePos+i)%kScopeSize,i1=(fScopePos+i+1)%kScopeSize;
            const float x0=sx+(float)i/(kScopeSize-1)*sw,x1=sx+(float)(i+1)/(kScopeSize-1)*sw;
            float y0=sy+sh-1.0f-fScope[i0]*(sh-2.0f),y1=sy+sh-1.0f-fScope[i1]*(sh-2.0f);
            if(y0<sy+1)y0=sy+1;if(y0>sy+sh-1)y0=sy+sh-1;
            if(y1<sy+1)y1=sy+1;if(y1>sy+sh-1)y1=sy+sh-1;
            if(i==0)moveTo(x0,y0);else lineTo(x0,y0);lineTo(x1,y1);
        }
        strokeColor(g2bsk(fLight,5));strokeWidth(1.5f);stroke();

        {
            char lb[64];
            std::snprintf(lb,sizeof(lb),"LUFS %.1f  |  %.1f",fLufsM,fLufsI);
            fontSize(10.5f);textAlign(ALIGN_RIGHT|ALIGN_TOP);
            fillColor(g2bsk(fLight,2));
            text(sx+sw-8.0f,sy+4.0f,lb,nullptr);
            fontSize(8.5f);fillColor(g2bsk(fLight,3));
            text(sx+sw-8.0f,sy+16.0f,"live    gemiddeld",nullptr);
        }

        // MAAK MIX-KLAAR knop (grote groene knop rechtsonder scope)
        {
            const float bx=W-208.0f,by=124.0f,bw=190.0f,bh=26.0f;
            const bool busy=fBusy>0.5f;
            beginPath();roundedRect(bx,by,bw,bh,7.0f);
            fillColor(busy?g2bsk(fLight,5):g2bsk(fLight,1));fill();
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            fontSize(11.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(busy?g2bsk(fLight,0):g2bsk(fLight,2));
            text(bx+bw*0.5f,by+bh*0.5f,busy?"LUISTEREN…":"MAAK MIX-KLAAR",nullptr);
        }

        // KRACHT dry/wet slider
        {
            const float kx=108.0f,ky=133.0f,kw=170.0f,kh=8.0f;
            fontSize(9.5f);fillColor(g2bsk(fLight,3));
            textAlign(ALIGN_LEFT|ALIGN_TOP);
            char kb[24];std::snprintf(kb,sizeof(kb),"KRACHT %.0f %%",fKracht);
            text(20.0f,131.0f,kb,nullptr);
            beginPath();roundedRect(kx,ky,kw,kh,4.0f);
            fillColor(g2bsk(fLight,1));fill();
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            beginPath();roundedRect(kx,ky,kw*(fKracht/100.f),kh,4.0f);
            fillColor(g2bsk(fLight,5));fill();
        }

        // RUIMTE slider (v2: ontmaskeren tussen sporen) + BUS-teller
        {
            const float kx=108.0f,ky=157.0f,kw=170.0f,kh=8.0f;
            fontSize(9.5f);fillColor(g2bsk(fLight,3));
            textAlign(ALIGN_LEFT|ALIGN_TOP);
            char kb[40];std::snprintf(kb,sizeof(kb),"RUIMTE %.0f %%",fRuimte);
            text(20.0f,155.0f,kb,nullptr);
            beginPath();roundedRect(kx,ky,kw,kh,4.0f);
            fillColor(g2bsk(fLight,1));fill();
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            beginPath();roundedRect(kx,ky,kw*(fRuimte/100.f),kh,4.0f);
            fillColor(fBusN>0?Color(90,160,255):g2bsk(fLight,3));fill();
            char bb[24];std::snprintf(bb,sizeof(bb),fBusN>0?"BUS: %d sporen":"BUS: alleen",(int)fBusN);
            fontSize(9.0f);fillColor(fBusN>0?Color(90,160,255):g2bsk(fLight,3));
            text(284.0f,155.0f,bb,nullptr);
        }

        // PRIO-pills: hoe belangrijk is dit spoor (duk-volgorde op de bus)
        {
            static const char* pn[3]={"HOOG","MIDDEN","LAAG"};
            fontSize(9.5f);fillColor(g2bsk(fLight,3));
            textAlign(ALIGN_LEFT|ALIGN_TOP);
            text(20.0f,175.0f,"PRIO",nullptr);
            for(int i=0;i<3;++i){
                const float bx=62.0f+i*58.0f,by=172.0f,bw=52.0f,bh=14.0f;
                const bool on=((int)(fPrio+0.5f)==i);
                beginPath();roundedRect(bx,by,bw,bh,4.0f);
                if(on){fillColor(g2bsk(fLight,5));fill();}
                else{fillColor(g2bsk(fLight,1));fill();}
                strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
                fontSize(8.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
                fillColor(on?g2bsk(fLight,0):g2bsk(fLight,2));
                text(bx+bw*0.5f,by+bh*0.5f,pn[i],nullptr);
            }
            fontSize(8.5f);fillColor(g2bsk(fLight,3));
            textAlign(ALIGN_LEFT|ALIGN_TOP);
            text(240.0f,175.0f,"HOOG dukit nooit · LAAG wijkt voor alles",nullptr);
        }

        // 6 knoppen
        const float knArea=W-140.0f;
        const float step=(knArea-40.0f)/6;
        for(int i=0;i<6;++i){
            const float cx=20.0f+step*(i+0.5f);
            const float cy=215.0f;
            const float R=26.0f;
            const float norm=(fValue[i]-fMin[i])/(fMax[i]-fMin[i]);
            const float ang=-2.4f+norm*4.8f;
            beginPath();circle(cx,cy,R);
            fillColor(g2bsk(fLight,1));fill();
            strokeColor(g2bsk(fLight,4));strokeWidth(2.0f);stroke();
            beginPath();
            arc(cx,cy,R-6.0f,-2.4f-1.5708f,ang-1.5708f,CW);
            strokeColor(g2bsk(fLight,5));strokeWidth(5.0f);stroke();
            beginPath();moveTo(cx,cy);
            lineTo(cx+std::sin(ang)*(R-10.0f),cy-std::cos(ang)*(R-10.0f));
            strokeColor(g2bsk(fLight,2));strokeWidth(3.0f);stroke();
            fontSize(10.5f);fillColor(g2bsk(fLight,3));
            textAlign(ALIGN_CENTER|ALIGN_TOP);
            text(cx,cy+R+8.0f,fLabel[i],nullptr);
            fontSize(11.5f);fillColor(g2bsk(fLight,2));
            char buf[24];
            if(i==0||i==5)std::snprintf(buf,sizeof(buf),"%+.1f dB",fValue[i]);
            else if(i==1)std::snprintf(buf,sizeof(buf),"%.0f Hz",fValue[i]);
            else std::snprintf(buf,sizeof(buf),"%+.0f %%",fValue[i]);
            text(cx,cy+R+24.0f,buf,nullptr);
        }

        // meter-blok rechts: LEVEL + GR
        {
            const float mx=W-120.0f,my=168.0f,mw=102.0f,mh=112.0f;
            beginPath();roundedRect(mx,my,mw,mh,8.0f);
            fillColor(g2bsk(fLight,1));fill();
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            fontSize(11.0f);textAlign(ALIGN_CENTER|ALIGN_TOP);
            fillColor(g2bsk(fLight,3));
            text(mx+mw*0.5f,my+8.0f,"LEVEL",nullptr);
            float mm=fLufsM;if(mm<-24.f)mm=-24.f;if(mm>0.f)mm=0.f;
            const float fh=(mm+24.f)/24.f;
            const float bx=mx+14.0f,bw2=14.0f,bh2=mh-40.0f;
            beginPath();rect(bx,my+22.0f,bw2,bh2);
            fillColor(fLight==4?Color(30,34,38):Color(8,10,9));fill();
            beginPath();rect(bx,my+22.0f+bh2*(1.f-fh),bw2,bh2*fh);
            fillColor(fh>0.85f?Color(220,80,60):g2bsk(fLight,5));fill();
            // doel-lijn -12 LUFS
            const float gy=my+22.0f+bh2*(1.f-(12.f/24.f));
            beginPath();rect(bx-3.0f,gy,bw2+6.0f,1.5f);
            fillColor(230,200,80);fill();
            // GR-balk
            const float gx=bx+24.0f,gw=10.0f;
            beginPath();rect(gx,my+22.0f,gw,bh2);
            fillColor(fLight==4?Color(30,34,38):Color(8,10,9));fill();
            const float gr=1.f-fGr;
            beginPath();rect(gx,my+22.0f,gw,bh2*gr);
            fillColor(220,140,60);fill();
            char lb[32];
            std::snprintf(lb,sizeof(lb),"%.1f",fLufsM);
            fontSize(11.0f);fillColor(g2bsk(fLight,2));
            text(mx+mw*0.5f+18.0f,my+30.0f,lb,nullptr);
            fontSize(8.5f);fillColor(g2bsk(fLight,3));
            text(mx+mw*0.5f+18.0f,my+46.0f,"doel -12",nullptr);
            text(mx+mw*0.5f+18.0f,my+60.0f,"GR",nullptr);
        }

        fontSize(10.5f);fillColor(g2bsk(fLight,3));
        textAlign(ALIGN_CENTER|ALIGN_BOTTOM);
        text(W*0.5f,H-8.0f,"zet op elk spoor → MAAK MIX-KLAAR → RUIMTE = sporen maken ruimte voor elkaar",nullptr);

        // DICE + AAN + skin
        {
            const float dy=124.0f;
            beginPath();roundedRect(W-256.0f,dy,44.0f,18.0f,4.0f);
            fillColor(g2bsk(fLight,1));fill();
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            fontSize(9.0f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(g2bsk(fLight,2));
            text(W-234.0f,dy+9.0f,"DICE",nullptr);

            const float tx=W-40.0f;
            beginPath();roundedRect(tx,34.0f,26.0f,17.0f,4.0f);
            fillColor(g2bsk(fLight,1));fill();
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            {
                const float cx=tx+13.0f,cy=42.5f;
                if(fLight>0){fillColor(g2bsk(fLight,2));
                    beginPath();arc(cx+1.5f,cy,5.0f,0.f,6.2832f,CW);fill();
                    fillColor(g2bsk(fLight,1));
                    beginPath();arc(cx-1.8f,cy,4.4f,0.f,6.2832f,CW);fill();}
                else{fillColor(g2bsk(fLight,2));
                    beginPath();arc(cx,cy,4.2f,0.f,6.2832f,CW);fill();
                    strokeColor(g2bsk(fLight,2));strokeWidth(1.2f);
                    for(int r=0;r<8;++r){const float a=r*0.7854f;
                        beginPath();moveTo(cx+cosf(a)*5.6f,cy+sinf(a)*5.6f);
                        lineTo(cx+cosf(a)*7.4f,cy+sinf(a)*7.4f);stroke();}}
            }
            const bool aan=fOn>0.5f;
            beginPath();roundedRect(W-96.0f,33.0f,52.0f,18.0f,9.0f);
            fillColor(aan?g2bsk(fLight,5):g2bsk(fLight,1));fill();
            strokeColor(g2bsk(fLight,4));strokeWidth(1.0f);stroke();
            fontSize(10.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(aan?g2bsk(fLight,0):g2bsk(fLight,2));
            text(W-70.0f,42.0f,aan?"AAN":"UIT",nullptr);
        }
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if(!ev.press&&fBtnHeld>=0){setParameterValue(fBtnHeld,0.0f);fBtnHeld=-1;}
        if(ev.button!=1)return false;
        const float W=getWidth();
        const float px=(float)(ev.pos.getX()/getScaleFactor());
        const float py=(float)(ev.pos.getY()/getScaleFactor());
        if(ev.press){
            if(px>=W-96.0f&&px<=W-44.0f&&py>=33.0f&&py<=51.0f)
            {fOn=fOn>0.5f?0.f:1.f;setParameterValue(8,fOn);repaint();return true;}
            if(px>=W-256.0f&&px<=W-212.0f&&py>=124.0f&&py<=142.0f)
            {setParameterValue(6,1.0f);fBtnHeld=6;;repaint();return true;}
            if(px>=W-208.0f&&px<=W-18.0f&&py>=124.0f&&py<=150.0f)
            {setParameterValue(10,1.0f);fBtnHeld=10;;repaint();return true;}
            if(px>=W-40.0f&&px<=W-14.0f&&py>=34.0f&&py<=51.0f)
            {fLight=(fLight+1)%8;repaint();return true;}
            if(px>=108.0f&&px<=278.0f&&py>=129.0f&&py<=145.0f)
            {float v=(px-108.0f)/170.0f*100.f;if(v<0)v=0;if(v>100)v=100;
             fKracht=v;setParameterValue(9,v);repaint();fDragKr=true;return true;}
            if(px>=108.0f&&px<=278.0f&&py>=153.0f&&py<=169.0f)
            {float v=(px-108.0f)/170.0f*100.f;if(v<0)v=0;if(v>100)v=100;
             fRuimte=v;setParameterValue(16,v);repaint();fDragRm=true;return true;}
            for(int i=0;i<3;++i){
                const float bx=62.0f+i*58.0f;
                if(px>=bx&&px<=bx+52.0f&&py>=172.0f&&py<=186.0f)
                {fPrio=(float)i;setParameterValue(18,fPrio);repaint();return true;}
            }
            for(int i=0;i<kNumProfs;++i){
                const float bw=104.0f,bh=20.0f,gap=8.0f;
                const float rowW=kNumProfs*bw+(kNumProfs-1)*gap;
                const float bx=(W-rowW)*0.5f+i*(bw+gap);
                const float by=58.0f;
                if(px>=bx&&px<=bx+bw&&py>=by&&py<=by+bh)
                {applyUiProf(i);return true;}
            }
            const float knArea=W-140.0f;
            const float step=(knArea-40.0f)/6;
            for(int i=0;i<6;++i){
                const float cx=20.0f+step*(i+0.5f),cy=215.0f;
                const float dx=px-cx,dy=py-cy;
                if(dx*dx+dy*dy<=26.0f*26.0f+130.0f)
                {fDrag=i;fLastY=py;editParameter(i,true);return true;}
            }
        }else if(fDrag>=0){editParameter(fDrag,false);fDrag=-1;}
        else if(fDragKr){fDragKr=false;}
        else if(fDragRm){fDragRm=false;}
        return true;
    }
    bool onMotion(const MotionEvent& ev) override
    {
        if(fDragKr){
            const float px=(float)(ev.pos.getX()/getScaleFactor());
            float v=(px-108.0f)/170.0f*100.f;if(v<0)v=0;if(v>100)v=100;
            fKracht=v;setParameterValue(9,v);repaint();return true;
        }
        if(fDragRm){
            const float px=(float)(ev.pos.getX()/getScaleFactor());
            float v=(px-108.0f)/170.0f*100.f;if(v<0)v=0;if(v>100)v=100;
            fRuimte=v;setParameterValue(16,v);repaint();return true;
        }
        if(fDrag<0)return false;
        const float dy=fLastY-(float)(ev.pos.getY()/getScaleFactor());
        fLastY=(float)(ev.pos.getY()/getScaleFactor());
        const float range=fMax[fDrag]-fMin[fDrag];
        float v=fValue[fDrag]+dy*range/240.0f;
        if(v<fMin[fDrag])v=fMin[fDrag];if(v>fMax[fDrag])v=fMax[fDrag];
        fValue[fDrag]=v;fTarget[fDrag]=v;
        setParameterValue(fDrag,v);
        repaint();
        return true;
    }
    bool onScroll(const ScrollEvent& ev) override
    {
        const float W=getWidth();
        const float px=(float)(ev.pos.getX()/getScaleFactor());
        const float py=(float)(ev.pos.getY()/getScaleFactor());
        const float knArea=W-140.0f;
        const float step=(knArea-40.0f)/6;
        for(int i=0;i<6;++i){
            const float cx=20.0f+step*(i+0.5f),cy=215.0f;
            const float dx=px-cx,dy=py-cy;
            if(dx*dx+dy*dy<=26.0f*26.0f+130.0f){
                const float range=fMax[i]-fMin[i];
                float v=fValue[i]+ev.delta.getY()*range/100.0f;
                if(v<fMin[i])v=fMin[i];if(v>fMax[i])v=fMax[i];
                fValue[i]=v;fTarget[i]=v;
                editParameter(i,true);setParameterValue(i,v);editParameter(i,false);
                repaint();return true;
            }
        }
        return true;
    }
private:
    void applyUiProf(int i){
        if(i<0)i=0;if(i>5)i=5;
        fProf=i;
        setParameterValue(7,(float)i);
        fTarget[1]=kUiProfParams[i][0];setParameterValue(1,fTarget[1]);
        fTarget[2]=kUiProfParams[i][1];setParameterValue(2,fTarget[2]);
        fTarget[3]=kUiProfParams[i][2];setParameterValue(3,fTarget[3]);
        fTarget[4]=kUiProfParams[i][3];setParameterValue(4,fTarget[4]);
        repaint();
    }
public:
    void uiIdle() override{
        bool moved=false;
        for(int p=0;p<6;++p){const float d=fTarget[p]-fValue[p];
            if(d>0.35f||d<-0.35f){fValue[p]+=d*0.25f;moved=true;}
            else if(d!=0.f){fValue[p]=fTarget[p];moved=true;}}
        if(moved)repaint();
    }
private:
    static const int kScopeSize=256;
    float fValue[6],fMin[6],fMax[6],fTarget[6];
    const char* fLabel[6];const char* fUnit[6];
    float fOn=1.f,fKracht=100.f,fBusy=0.f,fGr=1.f;
    float fScope[kScopeSize];int fScopePos;
    float fLufsM=-99.0f,fLufsI=-99.0f;
    float fRuimte=60.f,fBusN=0.f,fPrio=1.f;
    int fDrag;float fLastY;int fProf;int fLight=0;bool fDragKr=false;bool fDragRm=false;
    int fBtnHeld=-1;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BMixerUI)
};

UI* createUI() { return new G2BMixerUI(); }

END_NAMESPACE_DISTRHO
