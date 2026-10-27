// G2B SONG UI — all-in-one beat-machine scherm
// vaste G2B-balk: presets + dice + auto + dark/light
// 4 live-rijen (DRUMS/808/CHORD/MEL) + pitch-slide visual + 10 knobs (2 rijen)
#include "DistrhoUI.hpp"
#include "SongShared.h"
#include <cstring>
#include <cstdio>
#include <cmath>

START_NAMESPACE_DISTRHO

class SongUI : public UI
{
public:
    SongUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        loadSharedResources();
        static const float mins[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        static const float maxs[19]={26,11,2,1,1,1,1,1,1,1,26,1,2,2,2,2,1,1,5};
        static const float defs[19]={1,9,0,0.8f,0.7f,0.35f,0.55f,0.35f,0.6f,0.6f,9,0.7f,1.f,1.f,1.f,1.f,.25f,.5f,0};
        static const char* lbl[19]  = {"STYLE","KEY","SCALE","DRUMS","808","GLIDE","DROP","DONDER","CHORD","MEL","GALAXY","DENS","SWING","HUMAN","FILL","GELUID","ROOM","TOUCH","1-TOETS"};
        std::memcpy(fMin, mins, sizeof(mins));
        std::memcpy(fMax, maxs, sizeof(maxs));
        std::memcpy(fValue, defs, sizeof(defs));
        std::memcpy(fLabel, lbl, sizeof(lbl));
        static const int pids[19]={0,1,2,3,4,7,8,9,5,6,10,17,18,19,20,21,22,23,24};
        std::memcpy(fParamId, pids, sizeof(pids));
        fDrag=-1; fLastY=0; fStep=-1; fAuto=0; fLight=0; fPreset=0;
        std::memset(fPat,'.',sizeof(fPat)); for(int r=0;r<10;r++)fPat[r][16]=0;
        for(int r=0;r<10;r++)fLaneOn[r]=true; fExtra[0]=fExtra[1]=0;
    }

protected:
    void parameterChanged(uint32_t index, float value) override
    {
        for(int i=0;i<19;i++) if(index==fParamId[i]){fValue[i]=value;break;}
        else if(index==12) fAuto=value;
        else if(index==13) fLight=value;
        else if(index==25) fSugg=value;
        else if(index==14){fPreset=value;int pi=(int)value;fValue[0]=pi;fValue[3]=.8f;fValue[4]=.7f;fValue[8]=.6f;fValue[9]=.6f;fValue[5]=pi==2?.55f:.35f;}
        else if(index==27) fRun=value;
        else if(index==28) fRec=(int)(value+0.5f); // REC wav-opname
        repaint();
    }
    void stateChanged(const char* key, const char* value) override
    {
        if(std::strcmp(key,"step")==0) fStep=std::atoi(value);
        else if(key[0]=='p'&&key[1]=='a'&&key[2]=='t'&&key[3]>='0'&&key[3]<='9'){int r=key[3]-'0';std::snprintf(fPat[r],17,"%s",value);}
        else if(std::strcmp(key,"lanes")==0){for(int r=0;r<10&&value[r];r++)fLaneOn[r]=(value[r]=='1');fExtra[0]=value[10]?value[10]-'0':0;fExtra[1]=value[11]?value[11]-'0':0;}
        else if(std::strcmp(key,"midifile")==0){ std::snprintf(fMidMsg,sizeof(fMidMsg),"%s",value); fMidT=400; }
        repaint();
    }
    void uiIdle() override { repaint(); }

    void onNanoDisplay() override
    {
        // rack-status direct uit de DSP lezen (standalone stuurt geen state terug)
        if(const SongRackView* rv=gSongRackView){
            for(int r=0;r<10;r++){for(int s=0;s<16;s++)fPat[r][s]=rv->mask[r][s]?'x':'.';fLaneOn[r]=rv->mute[r]!=0;}
            fExtra[0]=rv->extra[0];fExtra[1]=rv->extra[1];
            for(int s=0;s<16;s++)fMelDeg[s]=rv->melDeg[s]; for(int b=0;b<8;b++)fBasDeg[b]=rv->basDeg[b];
            for(int i=0;i<19;i++)fValue[i]=rv->prm[fParamId[i]]; // knoppen volgen echte DSP-waardes (ook bij stijl-wissel)
        }
        const float W=getWidth(), H=getHeight();
        static const int SK[8][7][3]={
            {{13,15,14},{23,26,25},{232,236,234},{100,110,103},{42,47,45},{47,191,113},{29,122,72}},
            {{14,14,22},{22,22,34},{230,232,255},{110,110,150},{40,40,58},{120,110,255},{70,60,170}},
            {{10,20,22},{16,30,32},{225,245,245},{95,130,130},{30,55,55},{40,200,210},{20,120,130}},
            {{24,14,20},{34,20,28},{250,230,240},{150,105,125},{60,38,50},{255,110,180},{180,60,120}},
            {{228,230,232},{240,242,244},{30,34,38},{105,112,118},{205,210,214},{40,140,100},{20,100,70}},
            {{18,10,10},{28,16,16},{240,225,220},{140,100,95},{55,35,32},{200,80,60},{130,45,35}},
            {{14,14,10},{22,22,16},{245,240,220},{120,115,95},{50,48,35},{230,190,80},{160,125,45}},
            {{12,14,20},{20,24,32},{235,240,250},{105,115,135},{36,44,58},{80,160,255},{45,110,200}}};
        static const char* SKN[8]={"MELIOR","SPECTRUM","LAGOON","CANDY","GHOST","MAROON","GOLD","STUDIO"};
        const int sk=(int)fLight<0?0:((int)fLight>7?7:(int)fLight);
        const int *bg=SK[sk][0],*pn=SK[sk][1],*tx=SK[sk][2],*gy=SK[sk][3],*ed=SK[sk][4],*GR=SK[sk][5],*DG=SK[sk][6];
        beginPath(); rect(0,0,W,H); fillColor(bg[0],bg[1],bg[2]); fill();

        fontSize(20.0f); textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        fillColor(GR[0],GR[1],GR[2]);
        text(18,22,"G2B SONG",nullptr);
        fontSize(9.0f); fillColor(gy[0],gy[1],gy[2]);
        text(18,41,"all-in-one beat-machine — jouw 808 glijdt echt · MIDI",nullptr);

        // 16 presets: 2 rijen van 8, rechts
        static const char* names[27]={"G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW","DNB","GRIME","AFROT","LATIN","ZOUK","BUBBLE","EDM","POP","FUNKY","SURIN"};
        static const char* MOODS[27]={"energiek","vuur","donker","smooth","dans","zon","hard","oldskool","zweverig","hyper","feest","warm","mellow","groovy","laidback","club","vrolijk","intens","ruw","ritmisch","zomer","sensueel","bubbel","festival","radio","chic","trots"};
        for(int i=0;i<27;i++){
            int prow=i/8, pcol=i%8;
            float bw=44.f, px=W-48.f-((prow<3?8:3)-pcol)*(bw+4.f), py=12.f+prow*20.f;
            beginPath(); roundedRect(px,py,bw,17,4);
            if((int)fPreset==i) fillColor(DG[0],DG[1],DG[2]); else fillColor(pn[0],pn[1],pn[2]);
            fill(); strokeColor(ed[0],ed[1],ed[2]); strokeWidth(1.0f); stroke();
            fontSize(8.0f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor((int)fPreset==i?tx[0]:gy[0],(int)fPreset==i?tx[1]:gy[1],(int)fPreset==i?tx[2]:gy[2]);
            text(px+bw/2,py+8.5f,names[i],nullptr);
        }
        // dice + auto + light
        beginPath(); roundedRect(14,48,26,17,4); fillColor(pn[0],pn[1],pn[2]); fill();
        strokeColor(ed[0],ed[1],ed[2]); strokeWidth(1.f); stroke();
        fontSize(11.f); fillColor(tx[0],tx[1],tx[2]); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
        text(27,56.5f,"\xF0\x9F\x8E\xB2",nullptr);
        beginPath(); roundedRect(44,48,42,17,4);
        if(fAuto>0.5f) fillColor(DG[0],DG[1],DG[2]); else fillColor(pn[0],pn[1],pn[2]);
        fill(); strokeColor(ed[0],ed[1],ed[2]); strokeWidth(1.f); stroke();
        fontSize(8.5f); fillColor(fAuto>0.5f?tx[0]:gy[0],fAuto>0.5f?tx[1]:gy[1],fAuto>0.5f?tx[2]:gy[2]);
        text(65,56.5f,"AUTO",nullptr);
        // EXPORT .MID knop — schrijft het patroon als midi-bestand naar Muziek\\G2B
        beginPath(); roundedRect(90,48,42,17,4); fillColor(pn[0],pn[1],pn[2]); fill();
        strokeColor(ed[0],ed[1],ed[2]); strokeWidth(1.f); stroke();
        fontSize(8.0f); fillColor(tx[0],tx[1],tx[2]);
        text(111,56.5f,"\xE2\x86\x93MID",nullptr);
        // SPEEL/STOP — pauzeert de sequencer (losse exe speelt anders altijd door)
        beginPath(); roundedRect(136,48,42,17,4);
        if(fRun>0.5f) fillColor(DG[0],DG[1],DG[2]); else fillColor(pn[0],pn[1],pn[2]);
        fill(); strokeColor(ed[0],ed[1],ed[2]); strokeWidth(1.f); stroke();
        fontSize(8.5f); fillColor(fRun>0.5f?tx[0]:gy[0],fRun>0.5f?tx[1]:gy[1],fRun>0.5f?tx[2]:gy[2]);
        text(157,56.5f,fRun>0.5f?"SPEEL":"STOP",nullptr);
        // REC — output → wav naast de exe (stems via laag-mutes)
        beginPath(); roundedRect(182,48,32,17,4);
        if(fRec>0) fillColor(220,60,60); else fillColor(pn[0],pn[1],pn[2]);
        fill(); strokeColor(ed[0],ed[1],ed[2]); strokeWidth(1.f); stroke();
        fontSize(8.5f); if(fRec>0) fillColor(255,240,240); else fillColor(gy[0],gy[1],gy[2]);
        text(198,56.5f,"REC",nullptr);

        float tlx=W-40.f;
        beginPath(); roundedRect(tlx,34,26,17,4); fillColor(pn[0],pn[1],pn[2]); fill();
        strokeColor(ed[0],ed[1],ed[2]); strokeWidth(1.f); stroke();
        fontSize(10.f); fillColor(tx[0],tx[1],tx[2]);
        text(tlx+13,42.5f,fLight>0.5f?"\xF0\x9F\x8C\x99":"\xE2\x98\x80",nullptr);
        fontSize(7.5f); fillColor(gy[0],gy[1],gy[2]); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
        text(tlx+13,60.5f,SKN[sk],nullptr);

        // drum-rack: 10 rijen — klik label = mute (+X1/+X2 = geluid kiezen), dobbelsteentje = laag-dice, vakje = stap aan/uit
        static const char* parts[10]={"KICK","SNARE","HAT","CLAP","PERC","+X1","+X2","808","CHORD","MEL"};
        static const char* xnam[6]={"+","TMB","CBL","RIM","SHK","BEL"};
        static const unsigned char pc[10][3]={{47,191,113},{242,120,60},{66,135,245},{235,205,60},{200,120,220},{140,220,190},{255,160,140},{242,155,42},{66,135,245},{235,205,60}};
        if(fEditLane_>=0){ // piano-roll: teken je eigen noten (MEL) of baslijn (808)
            const bool isMel=(fEditLane_==9);
            fontSize(6.5f); textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
            fillColor(pc[fEditLane_][0],pc[fEditLane_][1],pc[fEditLane_][2]);
            text(15,79,fEditLane_==9?"MEL":"808",nullptr);
            fillColor(gy[0],gy[1],gy[2]); fontSize(7.f);
            text(15,95,isMel?"klik = jouw":"klik = jouw",nullptr);
            text(15,106,isMel?"noot zetten":"bas-noot",nullptr);
            text(15,117,"◂ sluit",nullptr);
            const int nC=isMel?16:8; const float cw=(W-96.f)/nC; const float rh=23.75f;
            for(int p=0;p<8;p++){ // p=0 onder (grondtoon), p=7 boven (octaaf)
                float ry=74.f+(7-p)*rh;
                if(p==0){beginPath();rect(74,ry,W-96,rh-0.8f);fillColor(pn[0],pn[1],pn[2]);fill();} // grondtoon-rij iets donkerder
                for(int s2=0;s2<nC;s2++){
                    float bx=74.f+s2*cw;
                    bool hit=isMel?(fPat[9][s2]=='x'&&fMelDeg[s2]==p):(fBasDeg[s2]==p);
                    beginPath(); rect(bx,ry,cw-2.5f,rh-1.5f);
                    if(hit) fillColor(pc[fEditLane_][0],pc[fEditLane_][1],pc[fEditLane_][2]); else fillColor(ed[0],ed[1],ed[2]);
                    fill();
                    if(s2==(isMel?(fStep%16):((fStep%16)/2))){beginPath();rect(bx,ry,cw-2.5f,rh-1.5f);strokeColor(tx[0],tx[1],tx[2]);strokeWidth(0.8f);stroke();}
                }
            }
        }
        else for(int r=0;r<10;r++){
            float y=74.f+r*19.f;
            fontSize(8.5f); textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
            bool on=r<5?fLaneOn[r]:(r<7?fExtra[r-5]>0:fLaneOn[r]);
            fillColor(on?pc[r][0]:ed[0],on?pc[r][1]:ed[1],on?pc[r][2]:ed[2]);
            text(15,y+9.5f,parts[r],nullptr);
            if(r==5||r==6){fontSize(7.f);fillColor(gy[0],gy[1],gy[2]);int xi=fExtra[r-5];text(44,y+9.5f,xnam[xi<0?0:(xi>5?5:xi)],nullptr);fontSize(8.5f);}
            // laag-dice: echt dobbelsteentje (vierkant met 5 stipjes)
            beginPath(); roundedRect(56,y+3.f,13,13,3.f); strokeColor(gy[0],gy[1],gy[2]); strokeWidth(1.f); stroke();
            {const float dx[5]={59.5f,65.5f,59.5f,65.5f,62.5f},dy2[5]={y+6.5f,y+6.5f,y+12.5f,y+12.5f,y+9.5f};
             for(int d=0;d<5;d++){beginPath();circle(dx[d],dy2[d],1.3f);fillColor(gy[0],gy[1],gy[2]);fill();}}
            for(int s=0;s<16;s++){
                float bx=74.f+s*(W-96.f)/16.f;
                beginPath(); rect(bx,y+1.f,(W-96.f)/16.f-4.f,17.f);
                if(fPat[r][s]=='x'&&fLaneOn[r]) fillColor(pc[r][0],pc[r][1],pc[r][2]); else fillColor(ed[0],ed[1],ed[2]);
                fill();
                if(s==(fStep%16)){beginPath();rect(bx,y+1.f,(W-96.f)/16.f-4.f,17.f);strokeColor(tx[0],tx[1],tx[2]);strokeWidth(1.f);stroke();}
            }
            if(r==7||r==9){ // ▸ = piano-roll openen voor deze laag
                fontSize(11.f);textAlign(ALIGN_CENTER|ALIGN_MIDDLE);fillColor(gy[0],gy[1],gy[2]);
                text(W-15,y+9.5f,"\xE2\x96\xB8",nullptr);
            }
        }

        // pitch-slide visual (808-gebied)
        float gy0=272.f, gy1=312.f;
        beginPath(); rect(14,gy0,W-28,gy1-gy0); fillColor(pn[0],pn[1],pn[2]); fill();
        strokeColor(ed[0],ed[1],ed[2]); strokeWidth(1.f); stroke();
        fontSize(7.5f); textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        fillColor(gy[0],gy[1],gy[2]); text(18,gy0+8,"808 slide",nullptr);
        if(fMidT>0){ textAlign(ALIGN_RIGHT|ALIGN_TOP); fillColor(GR[0],GR[1],GR[2]);
                     text(W-20,gy0+3,fMidMsg,nullptr); fMidT--; }
        float gs=0.02f+fValue[5]*0.38f; // zelfde glide-tijd als DSP
        beginPath();
        float x0=70.f,x1=W-20.f,yy=gy1-8.f,yTop=gy0+10.f;
        moveTo(x0,yy);
        for(float x=x0;x<x1;x+=4.f){
            float k=1.f-expf(-(x-x0)/(gs*(x1-x0)*3.f));
            lineTo(x,yy-(yy-yTop)*k);
        }
        strokeColor(GR[0],GR[1],GR[2]); strokeWidth(2.5f); stroke();

        // 10 knobs in 2 rijen van 5
        for(int i=0;i<19;i++){
            int row=i/8, col=i%8;
            float cx=40+(W-80)/8.f*(col+0.5f), cy=340+row*108.f, R=24;
            float norm=(fValue[i]-fMin[i])/(fMax[i]-fMin[i]); if(norm<0)norm=0; if(norm>1)norm=1;
            float ang=-2.4f+norm*4.8f;
            beginPath(); arc(cx,cy,R,0,6.2832f,CW); fillColor(pn[0],pn[1],pn[2]); fill();
            strokeColor(ed[0],ed[1],ed[2]); strokeWidth(2.f); stroke();
            beginPath(); arc(cx,cy,R-4.f,-2.4f-1.5708f,ang-1.5708f,CW);
            strokeColor(i==5?242.f:GR[0],i==5?155.f:GR[1],i==5?42.f:GR[2]); strokeWidth(3.f); stroke();
            beginPath(); moveTo(cx,cy); lineTo(cx+sinf(ang)*(R-8),cy-cosf(ang)*(R-8));
            strokeColor(tx[0],tx[1],tx[2]); strokeWidth(3.f); stroke();
            fontSize(9.5f); textAlign(ALIGN_CENTER|ALIGN_TOP);
            fillColor(i==5?242.f:gy[0],i==5?155.f:gy[1],i==5?42.f:gy[2]); text(cx,cy+R+8,fLabel[i],nullptr);
            static const char* stn[17]={"G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"};
            static const char* kn[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
            char buf[24];
            if(i==0){int si=(int)fValue[0];si=si<0?0:(si>26?26:si);std::snprintf(buf,24,"%s %s",names[si],MOODS[si]);}
            else if(i==1) std::snprintf(buf,16,"%s",kn[(int)fValue[1]]);
            else if(i==2) std::snprintf(buf,16,"%s",fValue[2]<0.5f?"STIJL":(fValue[2]<1.5f?"minor":"major"));
            else if(i==10){int gp=(int)fValue[10];std::snprintf(buf,16,"%s %d",gp<9?"DARK":(gp<18?"LIGHT":"GALAXY"),gp%9+1);}
            else if(i==14){int fl=(int)std::lround(fValue[14]);std::snprintf(buf,16,"%s",fl<1?"UIT":(fl<2?"AAN":"EXTRA"));}
            else if(i==15)std::snprintf(buf,16,"%s",fValue[15]<0.5f?"SYNTH":(fValue[15]<1.5f?"ECHT":"STACK"));
            else if(i==18){int md=(int)std::lround(fValue[18]);const char* mn[6]={"UIT","SMART","BEAT","WALK","RUN","ARP"};std::snprintf(buf,16,"%s",mn[md<0?0:(md>5?5:md)]);}
            else std::snprintf(buf,16,"%.0f%%",fValue[i]*100.f);
            fillColor(tx[0],tx[1],tx[2]); text(cx,cy+R+21,buf,nullptr);
        }
        // onderregel
        fontSize(8.5f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
        fillColor(gy[0],gy[1],gy[2]);
        static const char* rn[8]={"","I","II","III","IV","V","VI","VII"};char sb[80];std::snprintf(sb,80,"MIDI-keyboard = 808 live spelen · volgende: %s",rn[(int)fSugg<0?0:((int)fSugg>7?7:(int)fSugg)]);
        text(W/2,H-10,fSugg>0.5f?sb:"MIDI-keyboard = 808 live spelen · legato = glijdt · DICE = nieuwe beat",nullptr);
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if(ev.press&&ev.button==1){
            const float W=getWidth();
            for(int i=0;i<27;i++){
                int prow=i/8, pcol=i%8; float bw=44.f;
                float px=W-48.f-((prow<3?8:3)-pcol)*(bw+4.f), py=12.f+prow*20.f;
                if(ev.pos.getX()>=px&&ev.pos.getX()<=px+bw&&ev.pos.getY()>=py&&ev.pos.getY()<=py+17){
                    setParameterValue(14,(float)i);fPreset=i;fValue[0]=i;fValue[3]=.8f;fValue[4]=.7f;fValue[8]=.6f;fValue[9]=.6f;fValue[5]=i==2?.55f:.35f;repaint();return true;}}
            if(ev.pos.getX()>=14&&ev.pos.getX()<=40&&ev.pos.getY()>=48&&ev.pos.getY()<=65){
                setParameterValue(11,1.f);return true;}
            if(ev.pos.getX()>=44&&ev.pos.getX()<=86&&ev.pos.getY()>=48&&ev.pos.getY()<=65){
                fAuto=fAuto>0.5f?0.f:1.f;setParameterValue(12,fAuto);repaint();return true;}
            if(ev.pos.getX()>=90&&ev.pos.getX()<=132&&ev.pos.getY()>=48&&ev.pos.getY()<=65){
                setParameterValue(16,1.f);return true;}
            if(ev.pos.getX()>=136&&ev.pos.getX()<=178&&ev.pos.getY()>=48&&ev.pos.getY()<=65){
                fRun=fRun>0.5f?0.f:1.f;setParameterValue(27,fRun);repaint();return true;}
            if(ev.pos.getX()>=182&&ev.pos.getX()<=214&&ev.pos.getY()>=48&&ev.pos.getY()<=65){
                fRec=1-fRec;setParameterValue(28,(float)fRec);repaint();return true;}
            // drum-rack: label = mute (+X1/+X2 = geluid wisselen), dobbelsteentje = laag-dice, vakje = stap aan/uit
            if(ev.pos.getY()>=74&&ev.pos.getY()<=264){
                if(fEditLane_>=0){ // piano-roll open — klik = noot zetten/wissen, ◂/marge = sluiten
                    if(ev.pos.getX()<74){fEditLane_=-1;repaint();return true;}
                    if(fEditLane_==9){
                        int s=(int)((ev.pos.getX()-74.f)/((W-96.f)/16.f));
                        int p=7-(int)((ev.pos.getY()-74.f)/23.75f);
                        if(s>=0&&s<16&&p>=0&&p<8){setParameterValue(26,(float)(210+p*16+s));
                            if(fMelDeg[s]==p){fMelDeg[s]=-1;fPat[9][s]='.';}else{fMelDeg[s]=p;fPat[9][s]='x';}
                            repaint();return true;}}
                    else if(fEditLane_==7){
                        int c=(int)((ev.pos.getX()-74.f)/((W-96.f)/8.f));
                        int p=7-(int)((ev.pos.getY()-74.f)/23.75f);
                        if(c>=0&&c<8&&p>=0&&p<8){setParameterValue(26,(float)(350+p*8+c));
                            fBasDeg[c]=(fBasDeg[c]==p)?-1:p; fPat[7][c*2]=(fBasDeg[c]>=0)?'x':'.';
                            repaint();return true;}}
                    return true;}
                int r=(int)((ev.pos.getY()-74.f)/19.f);
                if(r>=0&&r<10){
                    if((r==7||r==9)&&ev.pos.getX()>=W-24){fEditLane_=r;repaint();return true;} // ▸ = piano-roll
                    if(ev.pos.getX()>=56&&ev.pos.getX()<70){setParameterValue(26,(float)((ev.mod&DGL::kModifierShift)?200+r:170+r));return true;} // laag-dice (SHIFT = terug naar stijl-origineel)
                    if(ev.pos.getX()>=14&&ev.pos.getX()<56){
                        if(r==5||r==6){setParameterValue(26,190.f+(r-5));return true;} // +X: volgende geluid
                        fLaneOn[r]=!fLaneOn[r]; setParameterValue(26,(float)(180+r)); repaint();return true;}
                    if(ev.pos.getX()>=74&&ev.pos.getX()<W-22){
                        int s=(int)((ev.pos.getX()-74.f)/((W-96.f)/16.f));
                        if(s>=0&&s<16){setParameterValue(26,(float)(r*16+s));fPat[r][s]=fPat[r][s]=='x'?'.':'x';repaint();return true;}
                    }
                }
            }
            if(ev.pos.getX()>=W-40&&ev.pos.getX()<=W-14&&ev.pos.getY()>=34&&ev.pos.getY()<=51){
                fLight=((int)fLight+1)%8;setParameterValue(13,fLight);repaint();return true;}
            for(int i=0;i<19;i++){
                int row=i/8, col=i%8;
                float cx=40+(W-80)/8.f*(col+0.5f),cy=340+row*108.f,R=24;
                float dx=ev.pos.getX()-cx,dy=ev.pos.getY()-cy;
                if(dx*dx+dy*dy<(R+8)*(R+8)){fDrag=i;fLastY=ev.pos.getY();return true;}
            }
        }
        if(fDrag>=0&&ev.press==false){fDrag=-1;repaint();return true;}
        return false;
    }
    bool onMotion(const MotionEvent& ev) override
    {
        if(fDrag<0)return false;
        float dy=fLastY-ev.pos.getY(); fLastY=ev.pos.getY();
        float range=fMax[fDrag]-fMin[fDrag];
        float v=fValue[fDrag]+dy*range/200.f;
        if(v<fMin[fDrag])v=fMin[fDrag];if(v>fMax[fDrag])v=fMax[fDrag];
        fValue[fDrag]=v; setParameterValue(fParamId[fDrag],v); repaint(); return true;
    }

private:
    float fValue[19],fMin[19],fMax[19];
    const char* fLabel[19]; int fParamId[19];
    int fDrag; float fLastY; float fSugg=0;
    int fStep; float fAuto,fLight,fPreset; float fRun=1.f;
    int fRec=0;
    char fPat[10][17]; bool fLaneOn[10]; int fExtra[2];
    int fEditLane_=-1; signed char fMelDeg[16]={}; signed char fBasDeg[8]={};
    char fMidMsg[96]={0}; int fMidT=0;
};

UI* createUI(){ return new SongUI(); }
END_NAMESPACE_DISTRHO
