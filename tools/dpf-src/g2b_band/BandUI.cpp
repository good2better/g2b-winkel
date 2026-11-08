/*
 * G2B BAND — UI (NanoVG): 3 modus-tabs (SPEL / VERVOLG / ARRANGEMENT),
 * genre-pills · status-scope · actie-knoppen · knoppen per modus.
 * Param-indexen moeten gelijk zijn aan BandPlugin.cpp.
 */
#include "DistrhoUI.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

static const float G2BSK[8][7][3]={
    {{13,15,14},{23,26,25},{232,236,234},{100,110,103},{42,47,45},{47,191,113},{29,122,72}},
    {{14,14,22},{22,22,34},{230,232,255},{110,110,150},{40,40,58},{120,110,255},{70,60,170}},
    {{10,20,22},{16,30,32},{225,245,245},{95,130,130},{30,55,55},{40,200,210},{20,120,130}},
    {{24,14,20},{34,20,28},{250,230,240},{150,105,125},{60,38,50},{255,110,180},{180,60,120}},
    {{228,230,232},{240,242,244},{30,34,38},{105,112,118},{205,210,214},{40,140,100},{20,100,70}},
    {{18,10,10},{28,16,16},{240,225,220},{140,100,95},{55,35,32},{200,80,60},{130,45,35}},
    {{14,14,10},{22,22,16},{245,240,220},{120,115,95},{50,48,35},{230,190,80},{160,125,45}},
    {{12,14,20},{20,24,32},{235,240,250},{105,115,135},{36,44,58},{80,160,255},{45,110,200}}};
static inline Color g2bsk(int sk,int r){if(sk<0)sk=0;if(sk>7)sk=7;const float*c=G2BSK[sk][r];return Color((int)c[0],(int)c[1],(int)c[2]);}

START_NAMESPACE_DISTRHO

#ifdef G2B_GIO
static const int kNumGenres=14;static const int kPerRow=7;
#else
static const int kNumGenres=10;static const int kPerRow=5;
#endif
static const char* kGenreNames[14]={"TRAP","DRILL","BOOMBAP","RNB","HOUSE","DEMBOW","KAWINA",
    "LOFI","SOUL","POP","AMBIENT","SOCA","DANCEHALL","ZOUK"};
static const char* kModeNames[3]={"SPEL","VERVOLG","ARRANGEMENT"};
static const char* kModeSub[3]={
    "speel akkoorden → hij speelt live bass + drums mee",
    "luistert naar je halve beat → schrijft de tweede helft",
    "jouw loop of band → heel nummer: INTRO · BUILD · DROP · BREAK · OUTRO"};
static const char* kKeyNames[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static const char* kKlasNames[6]={"—","DRUMS","MELODIE","ONE-SHOT","DRUMS+MELODIE","BAND"};

enum { P_VOLH=0,P_DURF,P_HEFT,P_MENS,P_SWING,P_LEN,P_VAR,
       P_DICE,P_GENRE,P_MODE,P_ON,P_KRACHT,
       P_SPEL,P_LUIS,P_GEN,P_MID,P_REF,P_MIDIOUT,
       P_LEVEL,P_OKEY,P_OCONF,P_ONOTE,P_OBAR,P_OBPM,P_OSTAT,P_OKLAS,P_OHITS,
       P_OV0,P_OV1,P_OV2,P_OV3,P_OV4,P_OV5,P_OV6,P_OGENRE,P_OSPEL,P_OBUS };

struct KnobDef{int param;const char*label;float mn,mx;int kind;}; // kind 0=% 1=maten
static const KnobDef kKnobs[3][4]={
    {{P_VOLH,"VOLHEID",0,100,0},{P_MENS,"MENS",0,100,0},{P_SWING,"SWING",0,100,0},{-1,"",0,1,0}},
    {{P_DURF,"DURF",0,100,0},{P_MENS,"MENS",0,100,0},{P_SWING,"SWING",0,100,0},{-1,"",0,1,0}},
    {{P_HEFT,"HEFT",0,100,0},{P_MENS,"MENS",0,100,0},{P_LEN,"LENGTE",0,3,1},{P_VAR,"VAR",0,100,0}}};

class G2BBandUI : public UI
{
public:
    G2BBandUI():UI(DISTRHO_UI_DEFAULT_WIDTH,DISTRHO_UI_DEFAULT_HEIGHT){
        loadSharedResources();
        std::memset(fScope,0,sizeof(fScope));
    }
protected:
    void parameterChanged(uint32_t index,float value) override{
        switch(index){
        case P_VOLH:case P_OV0:fKnob[P_VOLH]=value;break;
        case P_DURF:case P_OV1:fKnob[P_DURF]=value;break;
        case P_HEFT:case P_OV2:fKnob[P_HEFT]=value;break;
        case P_MENS:case P_OV3:fKnob[P_MENS]=value;break;
        case P_SWING:case P_OV4:fKnob[P_SWING]=value;break;
        case P_LEN:case P_OV5:fKnob[P_LEN]=value;break;
        case P_VAR:case P_OV6:fKnob[P_VAR]=value;break;
        case P_GENRE:case P_OGENRE:fGenre=(int)(value+0.5f);break;
        case P_MODE:fMode=(int)(value+0.5f);break;
        case P_ON:fOn=value;break;
        case P_SPEL:case P_OSPEL:fSpel=value;break;
        case P_LUIS:fLuist=value;break;
        case P_LEVEL:fScope[fSP]=value;fSP=(fSP+1)%kSS;fLevel=value;break;
        case P_OKEY:fKey=(int)floorf(value+0.5f);break;
        case P_OCONF:fConf=value;break;
        case P_OBPM:fBpm=value;break;
        case P_OSTAT:fStat=(int)(value+0.5f);break;
        case P_OKLAS:fKlas=(int)(value+0.5f);break;
        case P_OHITS:fHits=(int)(value+0.5f);break;
        case P_OBAR:fBar=(int)(value+0.5f);break;
        case P_OBUS:fBusSync=value;break;
        default:break;}
        repaint();
    }

    void tabRect(int i,float&x,float&y,float&w,float&h){const float W=getWidth();
        w=(W-36-2*8)/3.f;h=22;x=18+i*(w+8);y=50;}
    void pillRect(int i,float&x,float&y,float&w,float&h){const float W=getWidth();
        int pr=kPerRow;int row=i/pr,col=i%pr;
        w=(pr==7?58.f:86.f);h=20;const float gap=8,rowW=pr*w+(pr-1)*gap;
        x=(W-rowW)*0.5f+col*(w+gap);y=row==0?82.f:106.f;}
    int btnCount()const{return fMode==0?2:4;}
    const char* btnName(int i)const{
        if(fMode==0){static const char*b[2]={"SPEL","↓MID"};return b[i];}
        if(fMode==1){static const char*b[4]={"LUISTER","GEN","SPEEL","↓MID"};return b[i];}
        static const char*b[4]={"LUISTER","GEN","REFINE","↓ARR"};return b[i];}
    void btnRect(int i,float&x,float&y,float&w,float&h){x=20+i*86.f;y=172;w=78;h=20;}
    void knobPos(int i,float&cx,float&cy){const float W=getWidth();const float step=(W-180)/4.f;
        cx=20+step*(i+0.5f);cy=245;}

    void onNanoDisplay() override{
        const float W=getWidth(),H=getHeight();const int L=fLight;
        beginPath();rect(0,0,W,H);fillColor(g2bsk(L,0));fill();
        fontSize(22);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);fillColor(g2bsk(L,5));
        text(18,22,"G2B BAND"
#ifdef G2B_GIO
        " GIO"
#endif
        ,nullptr);
        fontSize(9.5f);fillColor(g2bsk(L,3));
        text(20,40,kModeSub[fMode<0?0:(fMode>2?2:fMode)],nullptr);
        if(fBusSync>0.5f){fontSize(11);fillColor(g2bsk(L,6));
            text(W-100,22,"SYNC â MIDI",nullptr);}

        for(int i=0;i<3;i++){float x,y,w,h;tabRect(i,x,y,w,h);const bool on=i==fMode;
            beginPath();roundedRect(x,y,w,h,6);fillColor(on?g2bsk(L,5):g2bsk(L,1));fill();
            strokeColor(on?g2bsk(L,6):g2bsk(L,4));strokeWidth(1.5f);stroke();
            fontSize(11.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(on?g2bsk(L,0):g2bsk(L,2));
            char b[32];std::snprintf(b,sizeof(b),"%d · %s",i+1,kModeNames[i]);
            text(x+w*0.5f,y+h*0.5f,b,nullptr);}

        for(int i=0;i<kNumGenres;i++){float x,y,w,h;pillRect(i,x,y,w,h);
            const bool on=fGenre==i;
            beginPath();roundedRect(x,y,w,h,5);fillColor(on?g2bsk(L,5):g2bsk(L,1));fill();
            strokeColor(g2bsk(L,4));strokeWidth(1);stroke();
            fontSize(10.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(on?g2bsk(L,0):g2bsk(L,2));
            text(x+w*0.5f,y+h*0.5f,kGenreNames[i],nullptr);}

        const float sx=18,sy=136,sw=W-36,sh=30;
        beginPath();rect(sx,sy,sw,sh);fillColor(L==4?Color(30,34,38):Color(8,10,9));fill();
        strokeColor(g2bsk(L,4));strokeWidth(1);stroke();
        beginPath();
        for(int i=0;i<kSS-1;i++){const int i0=(fSP+i)%kSS;
            const float x0=sx+(float)i/(kSS-1)*sw;
            float y0=sy+sh-1-fScope[i0]*(sh-2);if(y0<sy+1)y0=sy+1;
            if(i==0)moveTo(x0,y0);else lineTo(x0,y0);}
        strokeColor(g2bsk(L,5));strokeWidth(1.5f);stroke();
        {char lb[200];statusText(lb,sizeof(lb));
         fontSize(10.5f);textAlign(ALIGN_RIGHT|ALIGN_MIDDLE);fillColor(g2bsk(L,2));
         text(sx+sw-8,sy+sh*0.5f,lb,nullptr);}

        for(int i=0;i<btnCount();i++){float x,y,w,h;btnRect(i,x,y,w,h);
            bool live=(i==0&&fMode!=0&&fLuist>0.5f)||(fMode==0&&i==0&&fSpel>0.5f)
                      ||(fMode==1&&i==2&&fSpel>0.5f);
            beginPath();roundedRect(x,y,w,h,4);fillColor(live?g2bsk(L,5):g2bsk(L,1));fill();
            strokeColor(g2bsk(L,4));strokeWidth(1);stroke();
            fontSize(10);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(live?g2bsk(L,0):g2bsk(L,2));
            text(x+w*0.5f,y+h*0.5f,btnName(i),nullptr);}

        for(int i=0;i<4;i++){const KnobDef&k=kKnobs[fMode][i];if(k.param<0)continue;
            float cx,cy;knobPos(i,cx,cy);const float R=30;
            float val=fKnob[k.param];float norm=(val-k.mn)/(k.mx-k.mn);if(norm<0)norm=0;if(norm>1)norm=1;
            const float ang=-2.4f+norm*4.8f;
            beginPath();circle(cx,cy,R);fillColor(g2bsk(L,1));fill();
            strokeColor(g2bsk(L,4));strokeWidth(2);stroke();
            beginPath();arc(cx,cy,R-6,-2.4f-1.5708f,ang-1.5708f,CW);
            strokeColor(g2bsk(L,5));strokeWidth(5);stroke();
            beginPath();moveTo(cx,cy);lineTo(cx+std::sin(ang)*(R-12),cy-std::cos(ang)*(R-12));
            strokeColor(g2bsk(L,2));strokeWidth(3.5f);stroke();
            fontSize(11.5f);fillColor(g2bsk(L,3));textAlign(ALIGN_CENTER|ALIGN_TOP);
            text(cx,cy+R+8,k.label,nullptr);
            char buf[24];int iv=(int)floorf(val+0.5f);
            if(k.kind==1){static const int bl[4]={2,4,8,16};std::snprintf(buf,sizeof(buf),"%d maten",bl[iv<0?0:(iv>3?3:iv)]);}
            else std::snprintf(buf,sizeof(buf),"%.0f %%",val);
            fontSize(13);fillColor(g2bsk(L,2));text(cx,cy+R+23,buf,nullptr);}

        {const float mx=W-130,my=200,mw=112,mh=110;
         beginPath();roundedRect(mx,my,mw,mh,8);fillColor(g2bsk(L,1));fill();
         strokeColor(g2bsk(L,4));strokeWidth(1);stroke();
         fontSize(11);textAlign(ALIGN_CENTER|ALIGN_TOP);fillColor(g2bsk(L,3));
         text(mx+mw*0.5f,my+8,"LEVEL",nullptr);
         float fh=fLevel;if(fh<0)fh=0;if(fh>1)fh=1;
         const float bx=mx+14,bw2=14,bh2=mh-40;
         beginPath();rect(bx,my+24,bw2,bh2);fillColor(L==4?Color(30,34,38):Color(8,10,9));fill();
         beginPath();rect(bx,my+24+bh2*(1-fh),bw2,bh2*fh);
         fillColor(fh>0.85f?Color(220,80,60):g2bsk(L,5));fill();
         fontSize(9);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);fillColor(g2bsk(L,3));
         char b[48];std::snprintf(b,sizeof(b),"%.0f BPM",fBpm);text(mx+38,my+36,b,nullptr);
         if(fKey>=0){std::snprintf(b,sizeof(b),"%s%s",kKeyNames[fKey%12],fKey>=12?" min":" maj");text(mx+38,my+54,b,nullptr);}
         if(fMode==1&&fBar>=0)std::snprintf(b,sizeof(b),"stap %d/16",fBar+1);
         else std::snprintf(b,sizeof(b),"%d noten",fHits);
         text(mx+38,my+72,b,nullptr);}

        fontSize(10.5f);fillColor(g2bsk(L,3));textAlign(ALIGN_CENTER|ALIGN_BOTTOM);
        static const char* hint[3]={
            "SPEL aan → speel akkoorden op je MIDI-keyboard → bass + drums spelen mee · ↓MID bewaart de band",
            "LUISTER → speel je halve beat → GEN → hij schrijft de 2e helft · SPEEL = luister · ↓MID bewaart",
            "LUISTER → je loop (audio) erin · of GEN = maak er een nummer van je SPEL/VERVOLG · ↓ARR = heel arrangement"};
        text(W*0.5f,H-8,hint[fMode<0?0:(fMode>2?2:fMode)],nullptr);

        {const float dy=172;
         beginPath();roundedRect(W-130,dy,50,20,4);fillColor(g2bsk(L,1));fill();
         strokeColor(g2bsk(L,4));strokeWidth(1);stroke();
         fontSize(9.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);fillColor(g2bsk(L,2));
         text(W-105,dy+10,"DICE",nullptr);
         const float tx=W-40;
         beginPath();roundedRect(tx,14,26,17,4);fillColor(g2bsk(L,1));fill();
         strokeColor(g2bsk(L,4));strokeWidth(1);stroke();
         fillColor(g2bsk(L,2));beginPath();arc(tx+13,22.5f,4.2f,0,6.2832f,CW);fill();
         const bool aan=fOn>0.5f;
         beginPath();roundedRect(W-96,13,52,18,9);fillColor(aan?g2bsk(L,5):g2bsk(L,1));fill();
         strokeColor(g2bsk(L,4));strokeWidth(1);stroke();
         fontSize(10.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
         fillColor(aan?g2bsk(L,0):g2bsk(L,2));text(W-70,22,aan?"AAN":"UIT",nullptr);}
    }

    void statusText(char*lb,size_t n){
        if(fMode==0){
            if(fStat==4){std::snprintf(lb,n,".mid opgeslagen in Music/G2B");return;}
            if(fStat==5){std::snprintf(lb,n,"speel eerst met SPEL aan");return;}
            if(fSpel>0.5f||fStat==3){
                if(fConf>0)std::snprintf(lb,n,"SPEELT MEE · %s%s · %.0f%%",kKeyNames[fKey<0?9:fKey%12],fKey>=12?" min":" maj",fConf);
                else std::snprintf(lb,n,"SPEELT MEE · akkoord zoeken — speel 3+ noten");
                return;}
            std::snprintf(lb,n,"klaar — druk SPEL en speel akkoorden");return;
        }
        if(fMode==1){static const char*s[6]={"klaar — druk LUISTER","LUISTERT… speel je halve beat","denkt na…","SPEELT je vervolg",".mid opgeslagen in Music/G2B","te weinig noten — langer spelen"};
            std::snprintf(lb,n,"%s",s[fStat<0?0:(fStat>5?5:fStat)]);return;}
        static const char*s[6]={"klaar — LUISTER voor loop · GEN voor je band","LUISTERT… speel je audio-loop","analyseert…","LOOP KLAAR — speelt mee op PLAY",".mid + MIX-kaart opgeslagen","te weinig geluid — harder/langer"};
        const char*st=s[fStat<0?0:(fStat>5?5:fStat)];
        if(fStat==3&&fKlas>0)std::snprintf(lb,n,"%s · %s",st,kKlasNames[fKlas<0?0:(fKlas>5?5:fKlas)]);
        else std::snprintf(lb,n,"%s",st);
    }

    bool onMouse(const MouseEvent&ev) override{
        if(!ev.press&&fBtnHeld>=0){setParameterValue(fBtnHeld,0.0f);fBtnHeld=-1;}
        if(!ev.press&&fBtnHeld>=0){setParameterValue(fBtnHeld,0.0f);fBtnHeld=-1;}
        if(ev.button!=1)return false;
        const float W=getWidth();
        const float px=(float)(ev.pos.getX()/getScaleFactor()),py=(float)(ev.pos.getY()/getScaleFactor());
        if(ev.press){
            if(px>=W-96&&px<=W-44&&py>=13&&py<=31){fOn=fOn>0.5f?0:1;setParameterValue(P_ON,fOn);repaint();return true;}
            if(px>=W-40&&px<=W-14&&py>=14&&py<=31){fLight=(fLight+1)%8;repaint();return true;}
            if(px>=W-130&&px<=W-80&&py>=172&&py<=192){setParameterValue(P_DICE,1);fBtnHeld=P_DICE;fBtnHeld=P_DICE;;repaint();return true;}
            for(int i=0;i<3;i++){float x,y,w,h;tabRect(i,x,y,w,h);
                if(px>=x&&px<=x+w&&py>=y&&py<=y+h){fMode=i;fLuist=0;fSpel=0;fStat=0;
                    setParameterValue(P_MODE,(float)i);repaint();return true;}}
            for(int i=0;i<kNumGenres;i++){float x,y,w,h;pillRect(i,x,y,w,h);
                if(px>=x&&px<=x+w&&py>=y&&py<=y+h){fGenre=i;setParameterValue(P_GENRE,(float)i);repaint();return true;}}
            for(int i=0;i<btnCount();i++){float x,y,w,h;btnRect(i,x,y,w,h);
                if(px>=x&&px<=x+w&&py>=y&&py<=y+h){
                    if(fMode==0){
                        if(i==0){fSpel=fSpel>0.5f?0:1;setParameterValue(P_SPEL,fSpel);}
                        else setParameterValue(P_MID,1);
                    }else if(fMode==1){
                        if(i==0){fLuist=fLuist>0.5f?0:1;setParameterValue(P_LUIS,fLuist);}
                        else if(i==1)setParameterValue(P_GEN,1);
                        else if(i==2){fSpel=fSpel>0.5f?0:1;setParameterValue(P_SPEL,fSpel);}
                        else setParameterValue(P_MID,1);
                    }else{
                        if(i==0){fLuist=fLuist>0.5f?0:1;setParameterValue(P_LUIS,fLuist);}
                        else if(i==1)setParameterValue(P_GEN,1);
                        else if(i==2)setParameterValue(P_REF,1);
                        else setParameterValue(P_MID,1);
                    }
                    repaint();return true;}}
            for(int i=0;i<4;i++){const KnobDef&k=kKnobs[fMode][i];if(k.param<0)continue;
                float cx,cy;knobPos(i,cx,cy);const float dx=px-cx,dy=py-cy;
                if(dx*dx+dy*dy<=30*30+140){fDrag=i;fLastY=py;editParameter(k.param,true);return true;}}
        }else if(fDrag>=0){editParameter(kKnobs[fMode][fDrag].param,false);fDrag=-1;}
        return true;
    }
    bool onMotion(const MotionEvent&ev) override{
        if(fDrag<0)return false;
        const KnobDef&k=kKnobs[fMode][fDrag];
        const float y=(float)(ev.pos.getY()/getScaleFactor());const float dy=fLastY-y;fLastY=y;
        float v=fKnob[k.param]+dy*(k.mx-k.mn)/240.f;if(v<k.mn)v=k.mn;if(v>k.mx)v=k.mx;
        fKnob[k.param]=v;setParameterValue(k.param,(k.kind>=1)?floorf(v+0.5f):v);repaint();return true;
    }
    bool onScroll(const ScrollEvent&ev) override{
        const float px=(float)(ev.pos.getX()/getScaleFactor()),py=(float)(ev.pos.getY()/getScaleFactor());
        for(int i=0;i<4;i++){const KnobDef&k=kKnobs[fMode][i];if(k.param<0)continue;
            float cx,cy;knobPos(i,cx,cy);const float dx=px-cx,dy=py-cy;
            if(dx*dx+dy*dy<=30*30+140){
                float step=k.kind>=1?1.f:(k.mx-k.mn)/100.f*5.f;
                float v=fKnob[k.param]+(ev.delta.getY()>0?step:-step);if(v<k.mn)v=k.mn;if(v>k.mx)v=k.mx;
                fKnob[k.param]=v;editParameter(k.param,true);setParameterValue(k.param,v);editParameter(k.param,false);
                repaint();return true;}}
        return true;
    }
private:
    static const int kSS=256;
    float fKnob[7]={55,35,50,35,20,1,30};
    float fScope[kSS];int fSP=0;
    int fGenre=0,fMode=0,fLight=0,fDrag=-1;float fLastY=0;
    float fOn=1,fSpel=0,fLuist=0,fLevel=0,fBpm=120,fConf=0;
    int fKey=-1,fStat=0,fKlas=0,fHits=0,fBar=0;float fBusSync=0.f;
    int fBtnHeld=-1;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BBandUI)
};

UI* createUI(){return new G2BBandUI();}
END_NAMESPACE_DISTRHO
