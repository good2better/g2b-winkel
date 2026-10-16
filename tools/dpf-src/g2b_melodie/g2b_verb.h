#pragma once
#include <string.h>
// Kleine Schroeder-hall voor echte instrumenten (Spitfire-ambience, CPU-licht)
struct G2bVerb {
    static const int BMAX=8192;
    float cbuf[4][BMAX]; int clen[4]={0,0,0,0}; int ci[4]={0,0,0,0};
    float cf[4]={0.42f,0.45f,0.48f,0.50f};
    float abuf[2][1024]; int alen[2]={0,0}; int ai[2]={0,0};
    float af[2]={0.5f,0.5f};
    void init(float sr){ // kam-lengtes ~50/53/58/61ms, allpass ~6/5ms
        static const float ms[4]={50.6f,53.9f,57.9f,61.5f};
        static const float ma[2]={6.4f,4.8f};
        for(int i=0;i<4;i++){clen[i]=(int)(sr*ms[i]/1000.f);if(clen[i]>BMAX)clen[i]=BMAX;ci[i]=0;}
        for(int i=0;i<2;i++){alen[i]=(int)(sr*ma[i]/1000.f);if(alen[i]>1024)alen[i]=1024;ai[i]=0;}
        memset(cbuf,0,sizeof(cbuf)); memset(abuf,0,sizeof(abuf));
    }
    float tick(float x,float wet){
        float y=0;
        for(int i=0;i<4;i++){
            float o=cbuf[i][ci[i]];
            cbuf[i][ci[i]]=x+o*cf[i];
            ci[i]=(ci[i]+1)%clen[i]; y+=o;
        }
        y*=0.25f;
        for(int i=0;i<2;i++){
            float ao=abuf[i][ai[i]];
            abuf[i][ai[i]]=y+ao*af[i];
            ai[i]=(ai[i]+1)%alen[i]; y=ao;
        }
        return x*(1.f-wet)+y*wet;
    }
};
