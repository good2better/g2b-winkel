/*
 * g2b_std.h — G2B vaste-knoppen DSP-blok (BOUWREGEL A)
 * Zelfde Galaxy-engine als de oude lijn (bron: backend/app/galaxy_fx.py).
 * Include in een plugin:  #include "../g2b_std.h"
 * en in run() achter de eigen DSP:  g2b_post(inputs, outputs, n, sr, fOn, fKr, galaxy, sfeer)
 */
#pragma once

#include <cmath>
#include <cstring>
#include <cstdint>

START_NAMESPACE_DISTRHO

// ---- GALAXY (27 dimensies) — zelfde math als de oude plugin-lijn ----
struct GxShifter {
    static const int N=2048; float buf[N]; int wp=0; float ph=0;
    void reset(){std::memset(buf,0,sizeof buf);wp=0;ph=0;}
    float tick(float in,float rate){
        buf[wp]=in; float acc=0;
        for(int t=0;t<2;t++){
            float p=ph+0.5f*t; if(p>=1.f)p-=1.f;
            float rd=(float)wp-p*(float)N;
            int i0=(int)floorf(rd); float fr=rd-i0;
            float a=buf[((i0%N)+N)%N],b=buf[(((i0+1)%N)+N)%N];
            float w=p<0.5f?p*2.f:2.f-p*2.f;
            acc+=(a+(b-a)*fr)*w;
        }
        ph+=(1.f-rate)/(float)N; if(ph<0)ph+=1.f; if(ph>=1.f)ph-=1.f;
        wp=(wp+1)%N;
        return acc;
    }
};
struct Galaxy { Galaxy(){reset();}
    GxShifter sh[4];
    static const int CN=2200;
    float cb[4][CN]; int cw[4];
    float ap[2][600]; int aw[2];
    float lp[2];
    int gpos=-1; float grate=2.f,gfb=0.5f,gfc=6500.f,gwet=0.6f,gdd=0.2f; int gmode=2;
    void reset(){for(int i=0;i<4;i++)sh[i].reset();std::memset(cb,0,sizeof cb);std::memset(ap,0,sizeof ap);
                 std::memset(cw,0,sizeof cw);std::memset(aw,0,sizeof aw);lp[0]=lp[1]=0;}
    void setPos(int pos){
        if(pos==gpos)return; gpos=pos;
        gmode=pos/9; int dim=pos%9;
        uint32_t sd=(gmode==0?10007u:(gmode==1?20011u:30017u))*9u+dim;
        auto rng=[&](){sd^=sd<<13;sd^=sd>>17;sd^=sd<<5;return (sd&0xFFFFFFu)/16777216.f;};
        grate=powf(2.f,(gmode==0?-12.f:12.f)/12.f);
        gdd=(gmode==0?0.20f:(gmode==1?0.16f:0.24f))*(0.8f+0.4f*rng());
        gfb=(gmode==0?0.50f:(gmode==1?0.40f:0.55f))*(0.85f+0.3f*rng()); if(gfb>0.72f)gfb=0.72f;
        gfc=(gmode==0?2600.f:(gmode==1?9000.f:6500.f))*(0.85f+0.35f*rng());
        gwet=(gmode==0?0.55f:(gmode==1?0.50f:0.62f));
    }
    void tick(float& L,float& R,float sr){
        if(gpos<=0)return; // stand 0 = helemaal geen effect (clean)
        float in[2]={L,R},shim[2]={0,0};
        for(int c=0;c<2;c++){
            float x=sh[c].tick(in[c],grate);
            if(gmode==2)x+=0.6f*sh[2+c].tick(in[c],1.4983f);
            float a=expf(-6.283f*(gmode==0?gfc:320.f)/sr);
            lp[c]=(1.f-a)*x+a*lp[c];
            x=(gmode==0)?lp[c]:(x-lp[c]);
            int dd=(int)(gdd*sr); if(dd<64)dd=64; if(dd>CN-8)dd=CN-8;
            float tail=0;
            for(int k=0;k<4;k++){
                int rp=(cw[k]-dd-(dd*k/4)+CN*8)%CN;
                float e=cb[k][rp]; tail+=e*0.25f;
                cb[k][cw[k]]=x+e*gfb; cw[k]=(cw[k]+1)%CN;
            }
            const float g=0.5f;
            float av=ap[c][aw[c]];
            float y=-g*tail+av;
            ap[c][aw[c]]=tail+g*y; aw[c]=(aw[c]+1)%600;
            shim[c]=y;
        }
        L=in[0]+gwet*shim[0]*0.10f; R=in[1]+gwet*shim[1]*0.10f;
        if(L>0.98f)L=0.98f; else if(L<-0.98f)L=-0.98f;
        if(R>0.98f)R=0.98f; else if(R<-0.98f)R=-0.98f;
    }
};

// ---- SFEER (3 standen toon voor muziek-plugins): 0=dark,1=neutraal,2=licht ----
struct Sfeer {
    float lp[2]={0,0}, hp[2]={0,0};
    int mode=1;
    void setMode(int m){mode=m;}
    void tick(float& L,float& R,float sr){
        if(mode==1)return;
        // dark: sterker laagfilter; licht: high-shelf tilt
        float fc=(mode==0)?1800.f:9000.f;
        float a=expf(-6.283f*fc/sr);
        lp[0]=(1.f-a)*L+a*lp[0]; lp[1]=(1.f-a)*R+a*lp[1];
        if(mode==0){ L=lp[0]*0.92f+L*0.08f; R=lp[1]*0.92f+R*0.08f; }
        else       { float hL=L-lp[0],hR=R-lp[1]; L+=hL*0.55f; R+=hR*0.55f; }
    }
};

// ---- helpers ----
// bypass: kopieer input → output (FX). Instrumenten: gebruik memset zelf.
static inline void g2b_bypass(const float** in, float** out, uint32_t n){
    for(int c=0;c<2;c++)
        if(in[c]!=out[c]) std::memcpy(out[c],in[c],n*sizeof(float));
}
// kracht: wet/dry-mix van bewerkte output met droge input (FX-plugins).
static inline void g2b_kracht(const float** in, float** out, uint32_t n, float kr){
    float wet=kr/100.f; if(wet>1.f)wet=1.f; if(wet<0.f)wet=0.f;
    if(wet>=0.999f)return;
    float dry=1.f-wet;
    for(uint32_t i=0;i<n;i++){
        for(int c=0;c<2;c++) out[c][i]=in[c][i]*dry+out[c][i]*wet;
    }
}
// kracht als output-level (instrumenten, geen audio-in)
static inline void g2b_kracht_level(float** out, uint32_t n, float kr){
    float g=kr/100.f; if(g>1.f)g=1.f; if(g<0.f)g=0.f;
    if(g>=0.999f)return;
    for(uint32_t i=0;i<n;i++){ out[0][i]*=g; out[1][i]*=g; }
}
// universele post-keten: AAN/UIT (FX) → sfeer → galaxy → kracht
static inline void g2b_post(const float** in, float** out, uint32_t n, float sr,
                            float on, float kr, Galaxy& gal, Sfeer* sf=nullptr, bool hasInput=true){
    if(on<0.5f){
        if(hasInput) g2b_bypass(in,out,n);
        else { std::memset(out[0],0,n*sizeof(float)); std::memset(out[1],0,n*sizeof(float)); }
        return;
    }
    if(sf) sf->tick(out[0][0],out[1][0],0); // placeholder — echte tick per sample hieronder
    for(uint32_t i=0;i<n;i++){
        float L=out[0][i],R=out[1][i];
        if(sf) sf->tick(L,R,sr);
        gal.tick(L,R,sr);
        out[0][i]=L; out[1][i]=R;
    }
    if(hasInput) g2b_kracht(in,out,n,kr); else g2b_kracht_level(out,n,kr);
}

END_NAMESPACE_DISTRHO
