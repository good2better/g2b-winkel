// G2B Sleutel-Helper UI — toonsoort + veilige noten + hints
// FOCUS-pills · AAN/UIT · WIS · KRACHT/DIMENTIES draaiknoppen · SKIN
#include "DistrhoUI.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

START_NAMESPACE_DISTRHO

class SleutelUI : public UI
{
public:
    SleutelUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        loadSharedResources();
        static const float mins[2]={0,0}, maxs[2]={1,26}, defs[2]={.6f,8};
        static const char* lbl[2]={"KRACHT","DIMENTIES"};
        std::memcpy(fMin,mins,sizeof(mins)); std::memcpy(fMax,maxs,sizeof(maxs));
        std::memcpy(fValue,defs,sizeof(defs)); std::memcpy(fLabel,lbl,sizeof(lbl));
        static const int pids[2]={2,3}; std::memcpy(fParamId,pids,sizeof(pids));
        fDrag=-1; fLastY=0; fOn=1; fFocus=0; fSkin=0; fMask=0; fFoc=0; fConf=0;
        std::snprintf(fKey,sizeof(fKey),"luister…"); fSug1[0]=fSug2[0]=fSug3[0]=0;
    }

protected:
    void parameterChanged(uint32_t index, float value) override
    {
        for(int i=0;i<2;i++) if((int)index==fParamId[i]){fValue[i]=value;repaint();return;}
        if(index==0) fOn=value;
        else if(index==1) fFocus=(int)value;
        else if(index==4) fSkin=(int)value;
        else if(index==8){                       // key-out: -1 of root*2+minor
            static const char* NN[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
            const int v=(int)value;
            if(v<0) std::snprintf(fKey,sizeof(fKey),"luister…");
            else std::snprintf(fKey,sizeof(fKey),"%s %s",NN[v/2],(v&1)?"mineur":"majeur");
        }
        else if(index==9) fConf=(int)value;
        else if(index==10) fMask=(uint32_t)value;
        else if(index==11) fFoc=(uint32_t)value;
        else if(index==13){                        // chord-out: q*16+root
            static const char* NN[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
            static const char* QN[9]={"","","m","dim","aug","sus4","7","maj7","m7"};
            const int v=(int)value;
            if(v==0) fChord[0]=0;
            else std::snprintf(fChord,sizeof(fChord),"%s%s",NN[v&15],QN[v/16]);
        }
        else if(index==14){                        // hint-out: fout*16+goed
            static const char* NN[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
            const int v=(int)value;
            if(v==0) fHint[0]=0;
            else std::snprintf(fHint,sizeof(fHint),"%s ? probeer %s",NN[v/16],NN[v&15]);
        }
        repaint();
    }
    void stateChanged(const char* key, const char* value) override
    {
        if(std::strcmp(key,"ana")==0){
            char tmp[160]; std::snprintf(tmp,sizeof(tmp),"%s",value);
            char* p=std::strtok(tmp,"|");
            if(p){std::snprintf(fKey,sizeof(fKey),"%s",p);}
            if((p=std::strtok(nullptr,"|"))) fConf=std::atoi(p);
            std::strtok(nullptr,"|");
            if((p=std::strtok(nullptr,"|"))) fMask=(uint32_t)std::atoi(p);
            if((p=std::strtok(nullptr,"|"))) fFoc=(uint32_t)std::atoi(p);
        }
        else if(std::strcmp(key,"sug1")==0) std::snprintf(fSug1,sizeof(fSug1),"%s",value);
        else if(std::strcmp(key,"sug2")==0) std::snprintf(fSug2,sizeof(fSug2),"%s",value);
        else if(std::strcmp(key,"sug3")==0) std::snprintf(fSug3,sizeof(fSug3),"%s",value);
        repaint();
    }
    void uiIdle() override {}

    void onNanoDisplay() override
    {
        const float W=getWidth(), H=getHeight();
        static const int SK[8][7][3]={
            {{13,15,14},{23,26,25},{232,236,234},{100,110,103},{42,47,45},{47,191,113},{29,122,72}},
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

        beginPath(); rect(0,0,W,H); fillColor(C(bg)); fill();      // achtergrond

        // ---- kopbalk ----
        fontSize(15); textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        fillColor(C(ac)); text(16,22,"G2B SLEUTEL-HELPER",nullptr);
        fillColor(C(mu)); fontSize(11); text(150,22,"theory-assist · MIDI-thru",nullptr);

        // toonsoort blok
        beginPath(); roundedRect(W-200,8,184,44,8); fillColor(C(pn)); fill();
        fillColor(C(fg)); fontSize(17); 
        textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
        text(W-108,22,fKey,nullptr);
        fontSize(9);
        if(fChord[0]){ fillColor(C(ac)); text(W-108,36,fChord,nullptr); }
        fillColor(C(mu));
        if(fConf>0){ char b[24]; std::snprintf(b,24,"zekerheid %d%%",fConf); text(W-108,47,b,nullptr); }
        else text(W-108,47,"speel noten op je kanaal",nullptr);

        // ---- FOCUS pills ----
        static const char* FP[4]={"ALLES","BAS-808","AKKOORDEN","MELODIE"};
        fPillY=62;
        for(int i=0;i<4;i++){
            const float x=16+i*92;
            beginPath(); roundedRect(x,fPillY,84,24,12);
            fillColor(i==(int)fFocus?C(ac):C(pn)); fill();
            fillColor(i==(int)fFocus?C(bg):C(fg));
            fontSize(10); 
            textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            text(x+42,fPillY+12,FP[i],nullptr);
        }
        // SPEEL + AAN + WIS knoppen rechts
        beginPath(); roundedRect(W-236,fPillY,66,24,12); fillColor(C(pn)); fill();
        fillColor(C(ac)); fontSize(10); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
        text(W-203,fPillY+12,"SPEEL",nullptr);
        beginPath(); roundedRect(W-164,fPillY,68,24,12); fillColor(fOn>0.5f?C(ad):C(pn)); fill();
        fillColor(C(fg)); text(W-130,fPillY+12,fOn>0.5f?"AAN":"UIT",nullptr);
        beginPath(); roundedRect(W-88,fPillY,72,24,12); fillColor(C(pn)); fill();
        fillColor(C(fg)); text(W-52,fPillY+12,"WIS",nullptr);

        // ---- piano-strip (3 octaven, C2..B4) ----
        const float py=104, ph=88, kw=(W-32)/36.f;
        fPianoY=py;
        // witte toetsen
        static const int whites[7]={0,2,4,5,7,9,11};
        int wi=0;
        float wx[36]; int wpc[36];
        for(int o=0;o<3;o++)for(int i=0;i<7;i++){wpc[wi]=whites[i];wx[wi]=16+ (o*7+i)*(kw*12/7); wi++;}
        for(int i=0;i<21;i++){
            const int pc=wpc[i];
            const bool ins=fMask>>pc&1, inf=fFoc>>pc&1;
            beginPath(); rect(wx[i],py,kw*12/7-1,ph);
            fillColor(ins?C(ac,inf?0.95f:0.45f):Color(238,238,236,1.f)); fill();
            strokeColor(C(cd)); strokeWidth(1); stroke();
        }
        // zwarte toetsen
        static const float bo[5]={0.82f,1.85f,3.9f,4.92f,5.95f};
        static const int   bp[5]={1,3,6,8,10};
        for(int o=0;o<3;o++)for(int i=0;i<5;i++){
            const int pc=bp[i]; const float x=16+(o*7)* (kw*12/7) + bo[i]*(kw*12/7)*0.98f;
            const bool ins=fMask>>pc&1, inf=fFoc>>pc&1;
            beginPath(); rect(x,py,kw*12/7*0.62f,ph*0.58f);
            fillColor(ins?C(ad,inf?1.f:0.55f):C(cd)); fill();
        }
        // toonnamen onder de C's
        fillColor(C(mu)); fontSize(8); textAlign(ALIGN_CENTER|ALIGN_TOP);
        for(int o=0;o<3;o++) text(16+o*7*(kw*12/7)+ (kw*12/7)/2, py+ph+4, o==1?"C3":"C",nullptr);

        // buiten-toonsoort hint
        if(fHint[0]){ fillColor(C(ad)); fontSize(11); textAlign(ALIGN_LEFT|ALIGN_TOP);
            text(16,py+ph+8,fHint,nullptr); }

        // ---- suggestie-kaart ----
        const float sy=py+ph+22;
        beginPath(); roundedRect(16,sy,W-32,96,10); fillColor(C(pn)); fill();
        fillColor(C(fg)); fontSize(12); textAlign(ALIGN_LEFT|ALIGN_TOP);
        if(fSug1[0]) text(28,sy+12,fSug1,nullptr);
        if(fSug2[0]) text(28,sy+34,fSug2,nullptr);
        if(fSug3[0]){ fillColor(C(ac)); text(28,sy+62,fSug3,nullptr); }
        else if(fConf>0){ fillColor(C(mu)); fontSize(10);
            text(28,sy+12,"akkoord-, bas- en melodie-hints verschijnen",nullptr);
            text(28,sy+28,"in je DAW-host (VST3/CLAP)",nullptr); }

        // ---- knobs: KRACHT · DIMENTIES ----
        fKnobY=H-64;
        for(int i=0;i<2;i++){
            const float cx=W-190+i*100, cy=fKnobY+18, r=22;
            beginPath(); circle(cx,cy,r); fillColor(C(pn)); fill();
            const float a=-2.35f+ (fValue[i]-fMin[i])/(fMax[i]-fMin[i])*4.7f;
            beginPath(); moveTo(cx,cy); lineTo(cx+r*0.85f*std::cos(a),cy+r*0.85f*std::sin(a));
            strokeColor(C(ac)); strokeWidth(3); stroke();
            fillColor(C(mu)); fontSize(9); 
            textAlign(ALIGN_CENTER|ALIGN_TOP);
            text(cx,cy+r+6,fLabel[i],nullptr);
            fillColor(C(fg)); fontSize(10);
            char v[16];
            if(fMax[i]>2) std::snprintf(v,16,"%d",(int)fValue[i]+1);
            else std::snprintf(v,16,"%d%%",(int)(fValue[i]*100));
            text(cx,cy+r+16,v,nullptr);
        }
        // skin-swatcher
        beginPath(); roundedRect(16,H-52,150,32,8); fillColor(C(pn)); fill();
        fillColor(C(mu)); fontSize(9); textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
        text(26,H-36,"SKIN",nullptr); fSkinX=56;fSkinY=H-52;
        for(int i=0;i<8;i++){ beginPath(); circle(66+i*13,H-36,5); fillColor(C(SK[i][5])); fill(); }
    }

    bool onMouse(const MouseEvent& ev) override
    {
        const float W=getWidth();
        if(!ev.press){ fDrag=-1; return true; }
        // knobs eerst (drag-start)
        for(int i=0;i<2;i++){
            const float cx=W-190+i*100, cy=fKnobY+18;
            const float dx=ev.pos.getX()-cx, dy=ev.pos.getY()-cy;
            if(dx*dx+dy*dy<26*26){fDrag=i;fLastY=ev.pos.getY();return true;}
        }
        // focus pills
        if(ev.pos.getY()>=fPillY&&ev.pos.getY()<=fPillY+24){
            for(int i=0;i<4;i++){const float x=16+i*92;
                if(ev.pos.getX()>=x&&ev.pos.getX()<=x+84){setParameterValue(1,(float)i);fFocus=i;repaint();return true;}}
            if(ev.pos.getX()>=W-236&&ev.pos.getX()<=W-170){setParameterValue(12,1.f);return true;}
            if(ev.pos.getX()>=W-164&&ev.pos.getX()<=W-96){const float v=fOn>0.5f?0.f:1.f;setParameterValue(0,v);fOn=v;repaint();return true;}
            if(ev.pos.getX()>=W-88){setParameterValue(5,1.f);return true;}
        }
        // skin dots
        if(ev.pos.getY()>=fSkinY&&ev.pos.getY()<=fSkinY+32)
            for(int i=0;i<8;i++) if(std::fabs(ev.pos.getX()-(66+i*13))<8){setParameterValue(4,(float)i);fSkin=i;repaint();return true;}
        return true;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        if(fDrag>=0){
            const float dv=(fLastY-ev.pos.getY())*(fMax[fDrag]-fMin[fDrag])/150.f;
            fLastY=ev.pos.getY();
            float v=fValue[fDrag]+dv;
            if(v<fMin[fDrag])v=fMin[fDrag];
            if(v>fMax[fDrag])v=fMax[fDrag];
            setParameterValue(fParamId[fDrag],v);
            return true;
        }
        return false;
    }

private:
    float fMin[2],fMax[2],fValue[2]; const char* fLabel[2]; int fParamId[2];
    int fDrag; float fLastY;
    float fOn,fFocus,fSkin; uint32_t fMask,fFoc; int fConf;
    char fKey[32], fSug1[64], fSug2[64], fSug3[96], fChord[16]={0}, fHint[40]={0};
    float fPillY=62,fPianoY=104,fKnobY=0,fSkinX=0,fSkinY=0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SleutelUI)
};

UI* createUI() { return new SleutelUI(); }

END_NAMESPACE_DISTRHO
