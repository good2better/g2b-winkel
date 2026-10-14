// G2B Brein UI — patroon-brein scherm
// genre-pills (5) + kit-kiezer (◀/▶ + naam) + GENEREER hero-knop → MIDI-out
// vaste G2B-balk: dice + speel + ↓MID + skin
#include "DistrhoUI.hpp"
#include "brein_data.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

START_NAMESPACE_DISTRHO

class BreinUI : public UI
{
public:
    BreinUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        loadSharedResources();
        static const float mins[4] ={1,0,0,0};
        static const float maxs[4] ={16,255,1,2};
        static const float defs[4] ={4,7,.85f,1};
        static const char* lbl[4]  ={"BARS","SEED","KRACHT","SWING"};
        std::memcpy(fMin, mins, sizeof(mins));
        std::memcpy(fMax, maxs, sizeof(maxs));
        std::memcpy(fValue, defs, sizeof(defs));
        std::memcpy(fLabel, lbl, sizeof(lbl));
        static const int pids[4]={2,3,4,5};
        std::memcpy(fParamId, pids, sizeof(pids));
        fDrag=-1; fLastY=0; fStep=-1; fPlay=1;
    }

protected:
    void parameterChanged(uint32_t index, float value) override
    {
        for(int i=0;i<4;i++) if(index==fParamId[i]){fValue[i]=value;repaint();return;}
        if(index==0){fGenre=(int)value;fKit=0;}
        else if(index==1) fKit=(int)value;
        else if(index==6) fDrums=value;
        else if(index==7) fMel=value;
        else if(index==8) fMidi=value;
        else if(index==9) fPlay=value;
        else if(index==13)fLight=value;
        repaint();
    }
    void stateChanged(const char* key, const char* value) override
    {
        if(std::strcmp(key,"step")==0) fStep=std::atoi(value);
        else if(std::strcmp(key,"kit")==0){ std::snprintf(fKitMsg,sizeof(fKitMsg),"%s",value); }
        else if(std::strcmp(key,"midifile")==0){ std::snprintf(fMidMsg,sizeof(fMidMsg),"%s",value); fMidT=400; }
        repaint();
    }
    void uiIdle() override { repaint(); }

    void onNanoDisplay() override
    {
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
        text(18,22,"G2B BREIN",nullptr);
        fontSize(9.0f); fillColor(gy[0],gy[1],gy[2]);
        text(18,41,"219 geleerde kits → jouw patroon als MIDI (drums ch10 · mel ch1)",nullptr);

        // ---- genre-pills (5) ----
        static const char* GN[5]={"808","KAWINA","KEYS","MISC","TRAP"};
        for(int i=0;i<5;i++){
            float px=14.f+i*80.f, py=54.f;
            beginPath(); roundedRect(px,py,74,20,4);
            if(fGenre==i) fillColor(DG[0],DG[1],DG[2]); else fillColor(pn[0],pn[1],pn[2]);
            fill(); strokeColor(ed[0],ed[1],ed[2]); strokeWidth(1.0f); stroke();
            fontSize(8.5f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(fGenre==i?tx[0]:gy[0],fGenre==i?tx[1]:gy[1],fGenre==i?tx[2]:gy[2]);
            text(px+37,py+10,GN[i],nullptr);
        }
        // ---- GENEREER hero-knop ----
        beginPath(); roundedRect(W-180,48,166,30,6);
        fillColor(DG[0],DG[1],DG[2]); fill(); strokeColor(GR[0],GR[1],GR[2]); strokeWidth(1.5f); stroke();
        fontSize(13.f); fillColor(tx[0],tx[1],tx[2]); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
        text(W-97,63,"GENEREER",nullptr);

        // ---- rij: dice + speel + ↓MID + skin ----
        beginPath(); roundedRect(14,84,26,17,4); fillColor(pn[0],pn[1],pn[2]); fill();
        strokeColor(ed[0],ed[1],ed[2]); strokeWidth(1.f); stroke();
        // dobbelsteen-icoon (geen emoji-font beschikbaar): vlak + 5 stippen
        beginPath(); roundedRect(21,86.5f,12,12,2); fillColor(tx[0],tx[1],tx[2]); fill();
        fillColor(pn[0],pn[1],pn[2]);
        for(int dx=0;dx<3;dx++)for(int dy=0;dy<3;dy++){
            if((dx==1)&&(dy==1)||(dx!=1&&dy!=1)){
                beginPath(); circle(24.f+dx*3.f,89.5f+dy*3.f,1.1f); fill(); } }
        beginPath(); roundedRect(44,84,46,17,4);
        if(fPlay>0.5f) fillColor(DG[0],DG[1],DG[2]); else fillColor(pn[0],pn[1],pn[2]);
        fill(); strokeColor(ed[0],ed[1],ed[2]); strokeWidth(1.f); stroke();
        fontSize(8.5f); fillColor(fPlay>0.5f?tx[0]:gy[0],fPlay>0.5f?tx[1]:gy[1],fPlay>0.5f?tx[2]:gy[2]);
        text(67,92.5f,"SPEEL",nullptr);
        beginPath(); roundedRect(94,84,42,17,4); fillColor(pn[0],pn[1],pn[2]); fill();
        strokeColor(ed[0],ed[1],ed[2]); strokeWidth(1.f); stroke();
        fontSize(8.0f); fillColor(tx[0],tx[1],tx[2]);
        text(115,92.5f,"\xE2\x86\x93MID",nullptr);
        float tlx=W-40.f;
        beginPath(); roundedRect(tlx,84,26,17,4); fillColor(pn[0],pn[1],pn[2]); fill();
        strokeColor(ed[0],ed[1],ed[2]); strokeWidth(1.f); stroke();
        // zon/maan-icoon in plaats van emoji
        if(fLight>0.5f){ beginPath(); circle(tlx+13,92.5f,5.2f); fillColor(GR[0],GR[1],GR[2]); fill();
            beginPath(); circle(tlx+15,91.f,4.f); fillColor(pn[0],pn[1],pn[2]); fill(); }
        else { beginPath(); circle(tlx+13,92.5f,5.f); strokeColor(GR[0],GR[1],GR[2]); strokeWidth(1.f); stroke();
            beginPath(); circle(tlx+13,92.5f,1.4f); fillColor(GR[0],GR[1],GR[2]); fill(); }
        fontSize(7.5f); fillColor(gy[0],gy[1],gy[2]);
        text(tlx+13,110.f,SKN[sk],nullptr);

        // ---- kit-kiezer: ◀ naam ▶ + stats ----
        const BGK& gk=kGenreKits[fGenre<0?0:(fGenre>4?4:fGenre)];
        int kk=fKit<0?0:(fKit>=(int)gk.n?(int)gk.n-1:fKit);
        const BKit& kb=kKits[gk.off+kk];
        float ky=118.f;
        beginPath(); roundedRect(14,ky,26,22,4); fillColor(pn[0],pn[1],pn[2]); fill();
        strokeColor(ed[0],ed[1],ed[2]); strokeWidth(1.f); stroke();
        fontSize(11.f); fillColor(tx[0],tx[1],tx[2]); text(27,ky+11,"\xE2\x97\x80",nullptr);
        beginPath(); roundedRect(44,ky,W-58-44-30,22,4); fillColor(pn[0],pn[1],pn[2]); fill();
        strokeColor(ed[0],ed[1],ed[2]); strokeWidth(1.f); stroke();
        fontSize(9.5f); textAlign(ALIGN_LEFT|ALIGN_MIDDLE); fillColor(tx[0],tx[1],tx[2]);
        {   char kb2[96]; std::snprintf(kb2,sizeof(kb2),"%d/%d  %s",kk+1,(int)gk.n,kNames[kb.name]);
            text(54,ky+11,kb2,nullptr); }
        beginPath(); roundedRect(W-58,ky,26,22,4); fillColor(pn[0],pn[1],pn[2]); fill();
        strokeColor(ed[0],ed[1],ed[2]); strokeWidth(1.f); stroke();
        fontSize(11.f); fillColor(tx[0],tx[1],tx[2]); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
        text(W-45,ky+11,"\xE2\x96\xB6",nullptr);
        fontSize(8.f); textAlign(ALIGN_LEFT|ALIGN_MIDDLE); fillColor(gy[0],gy[1],gy[2]);
        {   char si[80]; std::snprintf(si,sizeof(si),"%d drum-parts · %d mel-lagen · %s%d bpm",(int)kb.n_parts,(int)kb.n_mel,kb.bpm?"":"~",(int)kb.bpm);
            text(46,ky+32,si,nullptr); }

        // ---- 16-step playhead (huidige bar) ----
        float py0=170.f, py1=206.f;
        beginPath(); rect(14,py0,W-28,py1-py0); fillColor(pn[0],pn[1],pn[2]); fill();
        strokeColor(ed[0],ed[1],ed[2]); strokeWidth(1.f); stroke();
        fontSize(7.5f); textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        fillColor(gy[0],gy[1],gy[2]); text(18,py0+8,"playhead",nullptr);
        const int totSteps=(int)fValue[0]*16; const int bar=totSteps?fStep/16:0;
        for(int s=0;s<16;s++){
            float sx=70.f+s*(W-96.f)/16.f;
            beginPath(); rect(sx,py0+16.f,(W-96.f)/16.f-4.f,12.f);
            if(s==(fStep%16)&&fStep>=0) fillColor(GR[0],GR[1],GR[2]); else fillColor(ed[0],ed[1],ed[2]);
            fill();
        }
        fontSize(8.f); textAlign(ALIGN_RIGHT|ALIGN_MIDDLE); fillColor(tx[0],tx[1],tx[2]);
        {   char bb[32]; std::snprintf(bb,sizeof(bb),"bar %d/%d",bar+1,(int)fValue[0]);
            text(W-20,py0+8,bb,nullptr); }
        if(fMidT>0){ textAlign(ALIGN_RIGHT|ALIGN_TOP); fillColor(GR[0],GR[1],GR[2]);
                     text(W-20,py1+4,fMidMsg,nullptr); fMidT--; }
        textAlign(ALIGN_LEFT|ALIGN_TOP); fillColor(gy[0],gy[1],gy[2]);
        if(fKitMsg[0]) text(18,py1+4,fKitMsg,nullptr);

        // ---- 4 knobs: BARS SEED KRACHT SWING ----
        for(int i=0;i<4;i++){
            float cx=40+(W-80)/4.f*(i+0.5f), cy=258.f, R=24;
            float norm=(fValue[i]-fMin[i])/(fMax[i]-fMin[i]); if(norm<0)norm=0; if(norm>1)norm=1;
            float ang=-2.4f+norm*4.8f;
            beginPath(); arc(cx,cy,R,0,6.2832f,CW); fillColor(pn[0],pn[1],pn[2]); fill();
            strokeColor(ed[0],ed[1],ed[2]); strokeWidth(2.f); stroke();
            beginPath(); arc(cx,cy,R-4.f,-2.4f-1.5708f,ang-1.5708f,CW);
            strokeColor(GR[0],GR[1],GR[2]); strokeWidth(3.f); stroke();
            beginPath(); moveTo(cx,cy); lineTo(cx+sinf(ang)*(R-8),cy-cosf(ang)*(R-8));
            strokeColor(tx[0],tx[1],tx[2]); strokeWidth(3.f); stroke();
            fontSize(9.5f); textAlign(ALIGN_CENTER|ALIGN_TOP);
            fillColor(gy[0],gy[1],gy[2]); text(cx,cy+R+8,fLabel[i],nullptr);
            char buf[24];
            if(i==3) std::snprintf(buf,16,"%d%%",(int)(fValue[3]*50.f));
            else if(i==2) std::snprintf(buf,16,"%.0f%%",fValue[2]*100.f);
            else std::snprintf(buf,16,"%.0f",fValue[i]);
            fillColor(tx[0],tx[1],tx[2]); text(cx,cy+R+21,buf,nullptr);
        }

        // ---- laag-schakelaars DRUMS / MEL / MIDI OUT ----
        static const char* tn[3]={"DRUMS","MEL","MIDI OUT"};
        const float tv[3]={fDrums,fMel,fMidi};
        for(int i=0;i<3;i++){
            float px=14.f+i*100.f, py=342.f;
            beginPath(); roundedRect(px,py,92,22,4);
            if(tv[i]>0.5f) fillColor(DG[0],DG[1],DG[2]); else fillColor(pn[0],pn[1],pn[2]);
            fill(); strokeColor(ed[0],ed[1],ed[2]); strokeWidth(1.f); stroke();
            fontSize(9.f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(tv[i]>0.5f?tx[0]:gy[0],tv[i]>0.5f?tx[1]:gy[1],tv[i]>0.5f?tx[2]:gy[2]);
            text(px+46,py+11,tn[i],nullptr);
        }

        // onderregel
        fontSize(8.5f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
        fillColor(gy[0],gy[1],gy[2]);
        text(W/2,H-10,"routeer MIDI-uit naar je drum/keys instrument · ↓MID = .mid naar Muziek\\G2B",nullptr);
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if(ev.press&&ev.button==1){
            const float W=getWidth();
            // genre pills
            for(int i=0;i<5;i++){
                float px=14.f+i*80.f;
                if(ev.pos.getX()>=px&&ev.pos.getX()<=px+74&&ev.pos.getY()>=54&&ev.pos.getY()<=74){
                    fGenre=i;fKit=0;setParameterValue(0,(float)i);setParameterValue(1,0.f);repaint();return true;}}
            // GENEREER
            if(ev.pos.getX()>=W-180&&ev.pos.getX()<=W-14&&ev.pos.getY()>=48&&ev.pos.getY()<=78){
                setParameterValue(10,1.f);return true;}
            // DICE — UI telt zelf random, parameters volgen
            if(ev.pos.getX()>=14&&ev.pos.getX()<=40&&ev.pos.getY()>=84&&ev.pos.getY()<=101){
                fGenre=rand()%5; const BGK& g=kGenreKits[fGenre]; fKit=rand()%g.n;
                fValue[1]=(float)(rand()%256);
                setParameterValue(0,(float)fGenre);setParameterValue(1,(float)fKit);setParameterValue(3,fValue[1]);
                // label direct bijwerken — wacht niet op DSP-state echo
                std::snprintf(fKitMsg,sizeof(fKitMsg),"%s · nieuw patroon",
                              kNames[kKits[g.off+fKit].name]);
                repaint();return true;}
            // SPEEL
            if(ev.pos.getX()>=44&&ev.pos.getX()<=90&&ev.pos.getY()>=84&&ev.pos.getY()<=101){
                fPlay=fPlay>0.5f?0.f:1.f;setParameterValue(9,fPlay);repaint();return true;}
            // ↓MID
            if(ev.pos.getX()>=94&&ev.pos.getX()<=136&&ev.pos.getY()>=84&&ev.pos.getY()<=101){
                setParameterValue(12,1.f);return true;}
            // SKIN
            if(ev.pos.getX()>=W-40&&ev.pos.getX()<=W-14&&ev.pos.getY()>=84&&ev.pos.getY()<=101){
                fLight=((int)fLight+1)%8;setParameterValue(13,fLight);repaint();return true;}
            // kit ◀ ▶
            { const BGK& g=kGenreKits[fGenre<0?0:(fGenre>4?4:fGenre)];
              if(ev.pos.getX()>=14&&ev.pos.getX()<=40&&ev.pos.getY()>=118&&ev.pos.getY()<=140){
                  fKit=fKit-1; if(fKit<0)fKit=(int)g.n-1; setParameterValue(1,(float)fKit);repaint();return true;}
              if(ev.pos.getX()>=W-58&&ev.pos.getX()<=W-32&&ev.pos.getY()>=118&&ev.pos.getY()<=140){
                  fKit=fKit+1; if(fKit>=(int)g.n)fKit=0; setParameterValue(1,(float)fKit);repaint();return true;} }
            // laag-toggles
            for(int i=0;i<3;i++){
                float px=14.f+i*100.f;
                if(ev.pos.getX()>=px&&ev.pos.getX()<=px+92&&ev.pos.getY()>=342&&ev.pos.getY()<=364){
                    float* tv=i==0?&fDrums:(i==1?&fMel:&fMidi);
                    *tv=*tv>0.5f?0.f:1.f; setParameterValue(6+i,*tv); repaint();return true;}}
            // knobs
            for(int i=0;i<4;i++){
                float cx=40+(W-80)/4.f*(i+0.5f),cy=258.f,R=24;
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
    float fValue[4],fMin[4],fMax[4];
    const char* fLabel[4]; int fParamId[4];
    int fDrag; float fLastY;
    int fGenre=0,fKit=0,fStep; float fDrums=1,fMel=1,fMidi=1,fPlay,fLight=0;
    char fMidMsg[96]={0}; int fMidT=0; char fKitMsg[96]={0};
};

UI* createUI(){ return new BreinUI(); }
END_NAMESPACE_DISTRHO
