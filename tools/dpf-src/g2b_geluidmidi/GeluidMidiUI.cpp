/*
 * G2B GELUID→MIDI — UI (NanoVG): 4 modus-tabs · genre/toonladder-pills · status-scope
 * · actie-knoppen · knoppen die per modus van betekenis wisselen.
 */
#include "DistrhoUI.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>

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
#ifdef G2B_GIO
static const int kNumScl=9;
#else
static const int kNumScl=5;
#endif
static const char* kSclNames[9]={"MAJEUR","MINEUR","PENTA","BLUES","CHROMA","DORIAN","PHRYG","MIXO","HARM"};
static const char* kModeNames[4]={"LOOP","MELODIE","INSTRUMENT","ANTWOORD"};
static const char* kModeSub[4]={
    "speel een geluid/loop → drums + melodie MIDI-loops",
    "neurie of zing een lijn → strakke melodie-MIDI",
    "zing 1 toon / one-shot → bespeelbaar op je keyboard",
    "speel 4 maten MIDI → hij antwoordt 4 maten terug"};
static const char* kKeyNames[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static const char* kKlasNames[5]={"—","DRUMS","MELODIE","ONE-SHOT","DRUMS+MELODIE"};

// param-indexen (moet gelijk zijn aan Plugin)
enum { P_HEFT=0,P_MENS,P_LEN,P_VAR,P_OCT,P_DICE,P_GENRE,P_SCHAAL,P_MODE,
       P_ON,P_LUIS,P_GEN,P_MID,P_REF,P_VRAAG,P_ALOOP,
       P_LEVEL,P_OKLAS,P_OKEY,P_OBPM,P_OSTAT,P_OHITS,P_OFASE,P_OCONF,P_ONOTE,
       P_OHEFT,P_OMENS,P_OLEN,P_OVAR,P_OOCT,P_TERUG,P_LAAD };

struct KnobDef{int param;const char*label;float mn,mx;int kind;}; // kind 0=% 1=bars 2=grid 3=oct
static const KnobDef kKnobs[4][4]={
    {{P_HEFT,"HEFT",0,100,0},{P_MENS,"MENS",0,100,0},{P_LEN,"LENGTE",0,3,1},{P_VAR,"VAR",0,100,0}},
    {{P_HEFT,"GEVOEL",0,100,0},{P_LEN,"GRID",0,2,2},{P_OCT,"OCTAAF",-2,2,3},{-1,"",0,1,0}},
    {{P_OCT,"OCTAAF",-2,2,3},{-1,"",0,1,0},{-1,"",0,1,0},{-1,"",0,1,0}},
    {{P_HEFT,"HEFTIG",0,100,0},{P_MENS,"MENS",0,100,0},{P_VAR,"GEHEUGEN",0,100,0},{-1,"",0,1,0}}};

class G2BGeluidMidiUI : public UI
{
public:
    G2BGeluidMidiUI():UI(DISTRHO_UI_DEFAULT_WIDTH,DISTRHO_UI_DEFAULT_HEIGHT){
        loadSharedResources();
        fV[P_HEFT]=50;fV[P_MENS]=30;fV[P_LEN]=1;fV[P_VAR]=30;fV[P_OCT]=0;
        std::memset(fScope,0,sizeof(fScope));
    }
protected:
    void parameterChanged(uint32_t index,float value) override{
        if(index<=P_OCT)fV[index]=value;
        else if(index==P_GENRE)fGenre=(int)(value+0.5f);
        else if(index==P_SCHAAL)fScl=(int)(value+0.5f);
        else if(index==P_MODE)fMode=(int)(value+0.5f);
        else if(index==P_ON)fOn=value;
        else if(index==P_LUIS)fLuist=value;
        else if(index==P_ALOOP)fALoop=value;
        else if(index==P_LEVEL){fScope[fSP]=value;fSP=(fSP+1)%kSS;fLevel=value;}
        else if(index==P_OKLAS)fKlas=(int)(value+0.5f);
        else if(index==P_OKEY)fKey=(int)floorf(value+0.5f);
        else if(index==P_OBPM)fBpm=value;
        else if(index==P_OSTAT)fStat=(int)(value+0.5f);
        else if(index==P_OHITS)fHits=(int)(value+0.5f);
        else if(index==P_OFASE)fFase=(int)(value+0.5f);
        else if(index==P_OCONF)fConf=value;
        else if(index==P_ONOTE)fNoteUI=(int)(value+0.5f);
        else if(index>=P_OHEFT&&index<=P_OVAR)fV[index-P_OHEFT]=value;
        else if(index==P_OOCT)fV[P_OCT]=value;
        repaint();
    }

    // layout helpers
    void tabRect(int i,float&x,float&y,float&w,float&h){const float W=getWidth();
        w=(W-36-3*8)/4.f;h=22;x=18+i*(w+8);y=50;}
    int pillCount()const{return fMode==1?kNumScl:(fMode==2?0:kNumGenres);}
    int perRow()const{return fMode==1?(kNumScl>5?5:kNumScl):kPerRow;}
    void pillRect(int i,float&x,float&y,float&w,float&h){const float W=getWidth();
        int pr=perRow();int row=i/pr,col=i%pr;
        w=(pr==7?58.f:86.f);h=20;const float gap=8,rowW=pr*w+(pr-1)*gap;
        x=(W-rowW)*0.5f+col*(w+gap);y=row==0?82.f:106.f;}
    int btnCount()const{return fMode==2?2:4;}
    const char* btnName(int i)const{
        if(fMode==3){static const char*b[4]={"LUISTER","VRAAG","LOOP","↓MID"};return b[i];}
        if(fMode==2){static const char*b[2]={"LUISTER","MAAK"};return b[i];}
        static const char*b[4]={"LUISTER","GEN","REFINE","↓MID"};return b[i];}
    void btnRect(int i,float&x,float&y,float&w,float&h){x=20+i*86.f;y=172;w=78;h=20;}
    void knobPos(int i,float&cx,float&cy){const float W=getWidth();const float step=(W-180)/4.f;
        cx=20+step*(i+0.5f);cy=245;}

    void onNanoDisplay() override{
        const float W=getWidth(),H=getHeight();const int L=fLight;
        beginPath();rect(0,0,W,H);fillColor(g2bsk(L,0));fill();
        fontSize(22);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);fillColor(g2bsk(L,5));
        text(18,22,"G2B GELUID→MIDI"
#ifdef G2B_GIO
        " GIO"
#endif
        ,nullptr);
        fontSize(9.5f);fillColor(g2bsk(L,3));
        text(20,40,kModeSub[fMode<0?0:(fMode>3?3:fMode)],nullptr);

        // modus-tabs
        for(int i=0;i<4;i++){float x,y,w,h;tabRect(i,x,y,w,h);const bool on=i==fMode;
            beginPath();roundedRect(x,y,w,h,6);fillColor(on?g2bsk(L,5):g2bsk(L,1));fill();
            strokeColor(on?g2bsk(L,6):g2bsk(L,4));strokeWidth(1.5f);stroke();
            fontSize(11.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(on?g2bsk(L,0):g2bsk(L,2));
            char b[32];std::snprintf(b,sizeof(b),"%d · %s",i+1,kModeNames[i]);
            text(x+w*0.5f,y+h*0.5f,b,nullptr);}

        // pills: genre (LOOP/ANTWOORD) of toonladder (MELODIE)
        const int pc=pillCount();
        if(pc==0){fontSize(11);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);fillColor(g2bsk(L,3));
            text(W*0.5f,100,"INSTRUMENT: LUISTER → zing/speel 1 toon → MAAK → speel hem op je MIDI-keyboard",nullptr);}
        for(int i=0;i<pc;i++){float x,y,w,h;pillRect(i,x,y,w,h);
            const bool on=(fMode==1?fScl:fGenre)==i;
            beginPath();roundedRect(x,y,w,h,5);fillColor(on?g2bsk(L,5):g2bsk(L,1));fill();
            strokeColor(g2bsk(L,4));strokeWidth(1);stroke();
            fontSize(10.5f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(on?g2bsk(L,0):g2bsk(L,2));
            text(x+w*0.5f,y+h*0.5f,fMode==1?kSclNames[i]:kGenreNames[i],nullptr);}

        // scope + status
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
        // live-noot flitser (laatste noot die MIDI-UIT verliet)
        if(fNoteUI>0){char nb[16];std::snprintf(nb,sizeof(nb),"♪ %s%d",kKeyNames[fNoteUI%12],fNoteUI/12-1);
         fontSize(10.5f);textAlign(ALIGN_LEFT|ALIGN_MIDDLE);fillColor(g2bsk(L,5));
         text(sx+8,sy+sh*0.5f,nb,nullptr);}

        // actie-knoppen
        for(int i=0;i<btnCount();i++){float x,y,w,h;btnRect(i,x,y,w,h);
            bool live=(i==0&&(fMode==3?fFase==1:fLuist>0.5f))||(fMode==3&&i==2&&fALoop>0.5f)
                      ||(fMode==3&&i==1&&fFase==3);
            beginPath();roundedRect(x,y,w,h,4);fillColor(live?g2bsk(L,5):g2bsk(L,1));fill();
            strokeColor(g2bsk(L,4));strokeWidth(1);stroke();
            fontSize(10);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(live?g2bsk(L,0):g2bsk(L,2));
            text(x+w*0.5f,y+h*0.5f,btnName(i),nullptr);}

        // TERUG + LAAD knoppen
        if(fMode!=2){const float bx=20+4*86.f;
         beginPath();roundedRect(bx,172,78,20,4);fillColor(g2bsk(L,1));fill();
         strokeColor(g2bsk(L,4));strokeWidth(1);stroke();
         fontSize(10);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);fillColor(g2bsk(L,3));
         text(bx+39,182,"TERUG",nullptr);
         const float lx=bx+84;
         beginPath();roundedRect(lx,172,78,20,4);fillColor(g2bsk(L,1));fill();
         strokeColor(g2bsk(L,4));strokeWidth(1);stroke();
         fontSize(10);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);fillColor(g2bsk(L,3));
         text(lx+39,182,"LAAD",nullptr);}

        // knoppen
        for(int i=0;i<4;i++){const KnobDef&k=kKnobs[fMode][i];if(k.param<0)continue;
            float cx,cy;knobPos(i,cx,cy);const float R=30;
            float val=fV[k.param];float norm=(val-k.mn)/(k.mx-k.mn);if(norm<0)norm=0;if(norm>1)norm=1;
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
            else if(k.kind==2){static const char*g[3]={"1/8","1/16","1/32"};std::snprintf(buf,sizeof(buf),"%s",g[iv<0?0:(iv>2?2:iv)]);}
            else if(k.kind==3)std::snprintf(buf,sizeof(buf),"%+d oct",iv);
            else std::snprintf(buf,sizeof(buf),"%.0f %%",val);
            fontSize(13);fillColor(g2bsk(L,2));text(cx,cy+R+23,buf,nullptr);}

        // LEVEL
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
         char b[40];std::snprintf(b,sizeof(b),"%.0f BPM",fBpm);text(mx+38,my+36,b,nullptr);
         if(fKey>=0){std::snprintf(b,sizeof(b),"%s%s",kKeyNames[fKey%12],fKey>=12?" min":" maj");text(mx+38,my+54,b,nullptr);}
         std::snprintf(b,sizeof(b),"%d noten",fHits);text(mx+38,my+72,b,nullptr);
         if(fConf>0.f){std::snprintf(b,sizeof(b),"%.0f%% zeker",fConf);text(mx+38,my+88,b,nullptr);}}

        fontSize(10.5f);fillColor(g2bsk(L,3));textAlign(ALIGN_CENTER|ALIGN_BOTTOM);
        static const char* hint[4]={
            "LUISTER → speel je geluid → LUISTER uit → loop speelt mee op PLAY · ↓MID = 2 .mid's in Music/G2B",
            "LUISTER → neurie je melodie → LUISTER uit → strak in toonladder · ↓MID = G2B-MELODIE.mid",
            "LUISTER → 1 toon zingen/spelen → MAAK → speel hem op je MIDI-keyboard (plugin = instrument)",
            "LUISTER → speel 4 maten op je keyboard → hij antwoordt · VRAAG = hij begint · LOOP = blijft doorgaan"};
        text(W*0.5f,H-8,hint[fMode<0?0:(fMode>3?3:fMode)],nullptr);

        // DICE + skin + AAN
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
        if(fMode==3){static const char*f[4]={"klaar — druk LUISTER of VRAAG","LUISTERT… speel 4 maten MIDI","ANTWOORDT…","VRAAG — hij speelt, jij antwoordt daarna"};
            char kb[40]="";if(fKey>=0&&fConf>0)std::snprintf(kb,sizeof(kb)," · %s%s · %.0f%% zeker",kKeyNames[fKey%12],fKey>=12?" min":"",fConf);
            std::snprintf(lb,n,"%s%s",f[fFase<0?0:(fFase>3?3:fFase)],kb);return;}
        if(fMode==2){static const char*s[5]={"klaar — druk LUISTER","LUISTERT… zing/speel 1 toon","maakt instrument…","INSTRUMENT KLAAR — speel op je keyboard",""};
            std::snprintf(lb,n,"%s",s[fStat<0?0:(fStat>3?3:fStat)]);return;}
        static const char*s[7]={"klaar — druk LUISTER","LUISTERT… speel je geluid","analyseert…","LOOP SPEELT (op PLAY)",".mid + .wav opgeslagen in Music/G2B","te stil/ruisig — check gain/mic, opnieuw LUISTER","geen IN.mid in Music/G2B — leg er een .mid neer en druk LAAD"};
        const char*st=s[fStat<0?0:(fStat>6?6:fStat)];
        char kb[48]="";
        if(fKey>=0&&fConf>0.f)std::snprintf(kb,sizeof(kb)," · %s%s %.0f%%",kKeyNames[fKey%12],fKey>=12?" min":"",fConf);
        if(fKlas>0)std::snprintf(lb,n,"%s · %s%s",st,kKlasNames[fKlas<0?0:(fKlas>4?4:fKlas)],kb);
        else std::snprintf(lb,n,"%s%s",st,kb);
    }

    bool onMouse(const MouseEvent&ev) override{
        if(!ev.press&&fBtnHeld>=0){setParameterValue(fBtnHeld,0.0f);fBtnHeld=-1;}
        if(ev.button!=1)return false;
        const float W=getWidth();
        const float px=(float)(ev.pos.getX()/getScaleFactor()),py=(float)(ev.pos.getY()/getScaleFactor());
        if(ev.press){
            if(px>=W-96&&px<=W-44&&py>=13&&py<=31){fOn=fOn>0.5f?0:1;setParameterValue(P_ON,fOn);repaint();return true;}
            if(px>=W-40&&px<=W-14&&py>=14&&py<=31){fLight=(fLight+1)%8;repaint();return true;}
            if(px>=W-130&&px<=W-80&&py>=172&&py<=192){setParameterValue(P_DICE,1);fBtnHeld=P_DICE;;repaint();return true;}
            if(fMode!=2&&px>=20+4*86&&px<=20+4*86+78&&py>=172&&py<=192){setParameterValue(P_TERUG,1);fBtnHeld=P_TERUG;repaint();return true;}
            if(fMode!=2&&px>=20+4*86+84&&px<=20+4*86+162&&py>=172&&py<=192){setParameterValue(P_LAAD,1);fBtnHeld=P_LAAD;repaint();return true;}
            for(int i=0;i<4;i++){float x,y,w,h;tabRect(i,x,y,w,h);
                if(px>=x&&px<=x+w&&py>=y&&py<=y+h){fMode=i;fLuist=0;fFase=0;fStat=0;fKlas=0;
                    setParameterValue(P_MODE,(float)i);repaint();return true;}}
            for(int i=0;i<pillCount();i++){float x,y,w,h;pillRect(i,x,y,w,h);
                if(px>=x&&px<=x+w&&py>=y&&py<=y+h){
                    if(fMode==1){fScl=i;setParameterValue(P_SCHAAL,(float)i);}
                    else{fGenre=i;setParameterValue(P_GENRE,(float)i);}
                    repaint();return true;}}
            for(int i=0;i<btnCount();i++){float x,y,w,h;btnRect(i,x,y,w,h);
                if(px>=x&&px<=x+w&&py>=y&&py<=y+h){
                    if(i==0){
                        if(fMode==3){setParameterValue(P_LUIS,1);fFase=fFase==1?2:1;}
                        else{fLuist=fLuist>0.5f?0:1;setParameterValue(P_LUIS,fLuist);}
                    }else if(fMode==3){
                        if(i==1){setParameterValue(P_VRAAG,1);fBtnHeld=P_VRAAG;;fFase=3;}
                        else if(i==2){fALoop=fALoop>0.5f?0:1;setParameterValue(P_ALOOP,fALoop);}
                        else{setParameterValue(P_MID,1);fBtnHeld=P_MID;}
                    }else{
                        if(i==1){setParameterValue(P_GEN,1);fBtnHeld=P_GEN;}
                        else if(i==2){setParameterValue(P_REF,1);fBtnHeld=P_REF;}
                        else{setParameterValue(P_MID,1);fBtnHeld=P_MID;}
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
        float v=fV[k.param]+dy*(k.mx-k.mn)/240.f;if(v<k.mn)v=k.mn;if(v>k.mx)v=k.mx;
        fV[k.param]=v;setParameterValue(k.param,(k.kind>=1)?floorf(v+0.5f):v);repaint();return true;
    }
    bool onScroll(const ScrollEvent&ev) override{
        const float px=(float)(ev.pos.getX()/getScaleFactor()),py=(float)(ev.pos.getY()/getScaleFactor());
        for(int i=0;i<4;i++){const KnobDef&k=kKnobs[fMode][i];if(k.param<0)continue;
            float cx,cy;knobPos(i,cx,cy);const float dx=px-cx,dy=py-cy;
            if(dx*dx+dy*dy<=30*30+140){
                float step=k.kind>=1?1.f:(k.mx-k.mn)/100.f*5.f;
                float v=fV[k.param]+(ev.delta.getY()>0?step:-step);if(v<k.mn)v=k.mn;if(v>k.mx)v=k.mx;
                fV[k.param]=v;editParameter(k.param,true);setParameterValue(k.param,v);editParameter(k.param,false);
                repaint();return true;}}
        return true;
    }
private:
    static const int kSS=256;
    float fV[5]={50,30,1,30,0};
    float fScope[kSS];int fSP=0;
    int fGenre=0,fScl=0,fMode=0,fLight=0,fDrag=-1;float fLastY=0;
    float fOn=1,fLuist=0,fALoop=0,fLevel=0,fBpm=120,fConf=0;
    int fKlas=0,fKey=-1,fStat=0,fHits=0,fFase=0,fNoteUI=0;
    int fBtnHeld=-1;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BGeluidMidiUI)
};

UI* createUI(){return new G2BGeluidMidiUI();}
END_NAMESPACE_DISTRHO
