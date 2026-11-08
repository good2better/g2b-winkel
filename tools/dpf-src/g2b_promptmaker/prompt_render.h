/* G2B Prompt-Maker — offline renderer: rendert één sound (param-set) naar stereo-float,
 * met dezelfde DSP als de plugin run() (shapeOSC + SVF + pingpong-delay + reverb + tilt + mono-laag)
 * plus de browser polish (18kHz LP, zachte drive, staart-gate, normalize 0.9).
 * Gebruikt door de UI voor wav-export. */
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "dicesynth_roll.h"

namespace g2bpm {

static inline float shapeOSC(float ph,float w){
    const float s=sinf(ph*6.2831853f);
    const float t=2.f*fabsf(2.f*(ph-floorf(ph))-1.f)-1.f;
    const float saw=2.f*(ph-floorf(ph))-1.f;
    const float sq=(ph-floorf(ph))<0.5f?1.f:-1.f;
    const float seg=w*3.f; const int i=(int)seg; const float f=seg-i;
    float a,b;
    switch(i<3?i:3){case 0:a=s;b=t;break;case 1:a=t;b=saw;break;case 2:a=saw;b=sq;break;default:a=sq;b=sq;}
    return a+(b-a)*f;
}

// render één noot (of akkoord) van sec seconden op sampleRate sr in L/R (beller alloceert)
static inline void renderShot(const float* fP,int note,float vel,float sec,float sr,float* L,float* R){
    struct V{bool on=false,rel=false;int note=0;float vel=0,t=0,startDel=0;
        float ph1=0,ph2=0,f0=0,ratio2=1,pan=0.5f,amp=0;int st=0;float lo=0,ba=0;uint32_t rs=0x12345;};
    V v[12];
    const int n=(int)(sec*sr);
    const float* fSm=fP; // offline: params direct (geen smoothing)
    const float atkT=0.001f*powf(5000.f,fSm[kAtk]);
    const float decT=0.02f*powf(200.f,fSm[kDec]);
    const float relT=0.005f*powf(1200.f,fSm[kRel]);
    const float rAtk=1.f/(atkT*sr),rDec=1.f/(decT*sr),rRel=1.f/(relT*sr);
    const float susL=fSm[kSus];
    const float baseCut=80.f*powf(200.f,fSm[kCut]);
    const float res=fSm[kRes];
    const float fenvA=fSm[kFEnv]*5.f;
    const float lfoRate=0.05f*powf(400.f,fSm[kLfoR]);
    const float lfoDep=fSm[kLfoD]*4.f;
    const float penvSemis=(fSm[kPEnv]-0.5f)*48.f;
    const float drvK=1.f+fSm[kDrive]*6.f;
    const float drvN=1.f/(1.f+fSm[kDrive]*1.2f);
    const float w2p=fSm[kOsc2W],o1w=fSm[kOsc1W];
    const int oct2=(int)(fSm[kO2Oct]*4.f+0.5f)-2;
    const float detC=(fSm[kO2Det]-0.5f)*100.f;
    const float r2=powf(2.f,(12.f*oct2+detC/100.f)/12.f);
    const float o2l=fSm[kO2Lvl],nL=fSm[kNoise];
    const float revM=fSm[kRev],dlyM=fSm[kDly];
    const float gain=fSm[kGain]*1.4f;
    static const float kDivs[4]={1.f,0.5f,0.75f,0.25f};
    const int dIdx=(int)(fSm[kDlyDiv]*3.f+0.5f);
    const float dlyS=(60.f/120.f)*kDivs[dIdx]*sr;
    const float dlyFb=fSm[kDlyFb]*0.75f;
    const float warmLo=0.7f+0.6f*fSm[kWarm],warmHi=1.3f-0.6f*fSm[kWarm];
    const float tiltA=1.f-expf(-6.2831853f*850.f/sr);
    const float tiltLo=1.f-expf(-6.2831853f*150.f/sr);
    // note-on zoals de plugin: akkoord-shapes + strum
    {
        static const int shapes[4][5]={{0,3,7,10,-1},{0,5,7,-1,-1},{0,7,12,-1,-1},{0,7,12,19,-1}};
        const int mode=(int)(fP[kMode]*4.f+0.5f);
        const bool ch=fP[kChord]>0.5f&&mode!=3&&mode!=4;
        const int sh=(int)(fP[kChordShape]*3.f+0.5f);
        const float strumS=(ch?fP[kStrum]*0.060f:0.f)*sr;
        int vi=0;
        for(int ci=0;ci<5;ci++){
            const int iv=ch?shapes[sh][ci]:ci==0?0:-1;
            if(iv<0)break;
            V*v0=&v[vi++]; v0->on=true; v0->note=note+iv;
            v0->vel=vel;
            v0->startDel=powf((float)ci,1.4f)*strumS;
            v0->f0=440.f*powf(2.f,(v0->note-69)/12.f);
            v0->ratio2=r2; v0->pan=0.5f+((ci&1)?0.18f:-0.18f)*(ci>0?1.f:0.f);
        }
    }
    const int relAt=(int)(n*0.55f);
    // delay buffers
    static float dL[2*192000],dR[2*192000]; int dW=0;
    std::memset(dL,0,sizeof dL); std::memset(dR,0,sizeof dR);
    // reverb
    static float rev[2][4][1800]; int revP[2][4]={{}};
    static const int revL[4]={1116,1188,1277,1356};
    static float all[2][2][600]; int allP[2][2]={{}};
    static const int allL[2]={225,556};
    std::memset(rev,0,sizeof rev); std::memset(all,0,sizeof all);
    auto revTick=[&](float in,int ch){
        float acc=0;
        for(int c=0;c<4;c++){float*ln=rev[ch][c];int Ln=revL[c];
            float y=ln[revP[ch][c]];ln[revP[ch][c]]=in*0.35f+y*0.62f;revP[ch][c]=(revP[ch][c]+1)%Ln;acc+=y;}
        acc*=0.25f;
        for(int a=0;a<2;a++){float*ln=all[ch][a];int Ln=allL[a];
            float buf=ln[allP[ch][a]];float y=-acc+buf;ln[allP[ch][a]]=acc+buf*0.5f;allP[ch][a]=(allP[ch][a]+1)%Ln;acc=y;}
        return acc;
    };
    float lfoPh=0,tiltL=0,tiltR=0,loL=0,loR=0;
    for(int i=0;i<n;i++){
        if(i==relAt)for(auto&v0:v)if(v0.on&&v0.st!=3)v0.st=3;
        lfoPh+=lfoRate/sr; if(lfoPh>=1.f)lfoPh-=1.f;
        const float lfoV=sinf(lfoPh*6.2831853f);
        float sl=0,sr2=0; int nV=0;
        for(auto&vc:v){
            if(!vc.on)continue;
            if(vc.startDel>0){vc.startDel-=1.f;continue;}
            nV++;
            vc.t+=1.f/sr;
            switch(vc.st){
                case 0: vc.amp+=rAtk; if(vc.amp>=1.f){vc.amp=1.f;vc.st=1;}break;
                case 1: vc.amp-=rDec; if(vc.amp<=susL){vc.amp=susL;vc.st=2;}break;
                case 3: vc.amp-=rRel; if(vc.amp<=0.f){vc.amp=0;vc.on=false;}break;
                default:break;
            }
            if(!vc.on)continue;
            const float penv=powf(2.f,penvSemis*expf(-vc.t*10.f)/12.f);
            vc.ph1+=vc.f0*penv/sr; if(vc.ph1>=1.f)vc.ph1-=1.f;
            vc.ph2+=vc.f0*vc.ratio2*penv/sr; if(vc.ph2>=1.f)vc.ph2-=1.f;
            float x=shapeOSC(vc.ph1,o1w)+o2l*shapeOSC(vc.ph2,w2p);
            vc.rs=vc.rs*1664525u+1013904223u;
            x+=nL*(((vc.rs>>9)&0xFFFF)/32768.f-1.f);
            x*=vc.amp*vc.vel;
            const float fcv=baseCut*powf(2.f,vc.amp*fenvA+lfoV*lfoDep);
            float fc=fcv>16000.f?16000.f:(fcv<40.f?40.f:fcv);
            const float fcof=2.f*sinf(3.1415926f*fc/sr);
            const float q=1.f-res*0.85f;
            vc.lo+=fcof*vc.ba;
            const float hi=x-vc.lo-q*vc.ba;
            vc.ba+=fcof*hi;
            const float y=vc.lo*0.8f;
            sl+=y*(1.f-vc.pan); sr2+=y*vc.pan;
        }
        const float vNorm=1.f/sqrtf(nV>0?(float)nV:1.f);
        sl*=vNorm; sr2*=vNorm;
        sl=tanhf(sl*drvK)*drvN; sr2=tanhf(sr2*drvK)*drvN;
        int dS=(int)dlyS; if(dS>383999)dS=383999; if(dS<1)dS=1;
        float ddR=dR[(dW+2*192000-dS)%(2*192000)];
        float ddL=dL[(dW+2*192000-dS)%(2*192000)];
        dL[dW]=sl+ddR*dlyFb; dR[dW]=sr2+ddL*dlyFb;
        sl+=ddL*dlyM; sr2+=ddR*dlyM;
        float rl=revTick(sl,0),rr=revTick(sr2,1);
        sl+=(rl-sl)*revM; sr2+=(rr-sr2)*revM;
        tiltL+=tiltA*(sl-tiltL); tiltR+=tiltA*(sr2-tiltR);
        sl=sl*warmHi+tiltL*(warmLo-warmHi); sr2=sr2*warmHi+tiltR*(warmLo-warmHi);
        loL+=tiltLo*(sl-loL); loR+=tiltLo*(sr2-loR);
        const float loM=0.5f*(loL+loR);
        sl=loM+(sl-loL); sr2=loM+(sr2-loR);
        L[i]=tanhf(sl*gain); R[i]=tanhf(sr2*gain);
        dW=(dW+1)%(2*192000);
    }
    // ---- polish (browser-versie): 18kHz LP + staart-gate + normalize 0.9 ----
    const float lpA=1.f-expf(-6.2831853f*18000.f/sr);
    float lpL=0,lpR=0;
    for(int i=0;i<n;i++){
        lpL+=lpA*(L[i]-lpL); lpR+=lpA*(R[i]-lpR);
        L[i]=lpL; R[i]=lpR;
    }
    // staart-gate: zoek waar het signaal echt stil wordt (<-55dB RMS), snijd de rest weg + 100ms fade
    {
        const int w=(int)(0.01f*sr); // 10ms ramen
        int last=n-1;
        for(int e=n-w;e>0;e-=w){
            float acc=0;
            for(int i=e;i<e+w;i++){acc+=L[i]*L[i]+R[i]*R[i];}
            if(sqrtf(acc/(2.f*w))>0.0018f){last=e+w;break;}
            last=e;
        }
        const int fade=(int)(0.10f*sr);
        const int f0=last-fade>0?last-fade:0;
        for(int i=f0;i<last;i++){
            const float g=1.f-(float)(i-f0)/(last-f0);
            L[i]*=g;R[i]*=g;
        }
        for(int i=last;i<n;i++){L[i]=0;R[i]=0;}
    }
    float pk=0.0001f;
    for(int i=0;i<n;i++){const float a=fabsf(L[i]),b=fabsf(R[i]);if(a>pk)pk=a;if(b>pk)pk=b;}
    const float norm=0.9f/pk;
    if(pk<0.95f||pk>0.9f)for(int i=0;i<n;i++){L[i]*=norm;R[i]*=norm;}
}

// schrijf 16-bit stereo WAV; geeft 0 bij ok
static inline int writeWav(const char* path,const float* L,const float* R,int n,float sr){
    FILE* f=std::fopen(path,"wb");
    if(!f)return -1;
    const int dataN=n*4, fileN=36+dataN;
    auto w32=[&](uint32_t v){std::fwrite(&v,4,1,f);};
    auto w16=[&](uint16_t v){std::fwrite(&v,2,1,f);};
    std::fwrite("RIFF",1,4,f); w32(fileN); std::fwrite("WAVE",1,4,f);
    std::fwrite("fmt ",1,4,f); w32(16); w16(1); w16(2); w32((uint32_t)sr); w32((uint32_t)sr*4); w16(4); w16(16);
    std::fwrite("data",1,4,f); w32(dataN);
    for(int i=0;i<n;i++){
        float l=L[i]>1?1:(L[i]<-1?-1:L[i]), r=R[i]>1?1:(R[i]<-1?-1:R[i]);
        int16_t sl=(int16_t)(l*32767.f),sr2=(int16_t)(r*32767.f);
        std::fwrite(&sl,2,1,f); std::fwrite(&sr2,2,1,f);
    }
    std::fclose(f); return 0;
}

} // namespace g2bpm
