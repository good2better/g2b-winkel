/* G2B Dice-Synth — gedeelde dobbelsteen-logica (plugin + UI). */
#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>

enum {
    kMode, kDice, kMutate, kUndoB, kRedoB,
    kOsc1W, kOsc2W, kO2Oct, kO2Det, kO2Lvl, kNoise, kPEnv,
    kAtk, kDec, kSus, kRel,
    kCut, kRes, kFEnv,
    kLfoR, kLfoD,
    kDrive, kRev, kDly, kDlyDiv, kDlyFb, kWarm, kGain,
    kChord, kChordShape, kStrum, kHuman,
    kLockOsc, kLockEnv, kLockFilt, kLockFx,
    kOn,
    kParamCount
};

struct RollR { int p; float lo[5]; float hi[5]; };
static const RollR kRollTable[] = {
    {kOsc1W,  {0.3f,0.2f,0.0f,0.0f,0.4f},{0.9f,0.8f,1.0f,0.35f,1.0f}},
    {kOsc2W,  {0.2f,0.1f,0.0f,0.0f,0.3f},{0.9f,1.0f,1.0f,0.5f,1.0f}},
    {kO2Oct,  {0.45f,0.4f,0.3f,0.45f,0.35f},{0.6f,0.7f,0.8f,0.55f,0.7f}},
    {kO2Det,  {0.4f,0.45f,0.2f,0.5f,0.3f},{0.6f,1.0f,1.0f,0.55f,0.8f}},
    {kO2Lvl,  {0.3f,0.4f,0.3f,0.0f,0.3f},{0.8f,0.9f,1.0f,0.35f,0.8f}},
    {kNoise,  {0.0f,0.0f,0.15f,0.0f,0.1f},{0.08f,0.15f,0.5f,0.05f,0.6f}},
    {kPEnv,   {0.5f,0.5f,0.4f,0.5f,0.0f},{0.5f,0.55f,0.7f,0.5f,0.45f}},
    {kAtk,    {0.0f,0.35f,0.3f,0.0f,0.0f},{0.06f,0.8f,0.9f,0.02f,0.01f}},
    {kDec,    {0.15f,0.4f,0.5f,0.2f,0.05f},{0.5f,0.7f,0.9f,0.5f,0.25f}},
    {kSus,    {0.5f,0.6f,0.7f,0.7f,0.0f},{0.9f,1.0f,1.0f,1.0f,0.15f}},
    {kRel,    {0.1f,0.4f,0.5f,0.05f,0.03f},{0.3f,0.8f,0.95f,0.2f,0.15f}},
    {kCut,    {0.4f,0.25f,0.2f,0.15f,0.5f},{0.9f,0.6f,0.8f,0.4f,1.0f}},
    {kRes,    {0.1f,0.2f,0.3f,0.0f,0.3f},{0.4f,0.5f,0.7f,0.3f,0.8f}},
    {kFEnv,   {0.0f,0.2f,0.4f,0.0f,0.5f},{0.4f,0.6f,0.9f,0.2f,1.0f}},
    {kLfoR,   {0.1f,0.05f,0.02f,0.0f,0.1f},{0.4f,0.25f,0.2f,0.15f,0.5f}},
    {kLfoD,   {0.0f,0.3f,0.5f,0.0f,0.0f},{0.3f,0.7f,1.0f,0.1f,0.4f}},
    {kDrive,  {0.1f,0.0f,0.2f,0.3f,0.4f},{0.5f,0.3f,0.6f,0.7f,0.9f}},
    {kRev,    {0.1f,0.4f,0.6f,0.0f,0.1f},{0.4f,0.8f,0.95f,0.15f,0.5f}},
    {kDly,    {0.1f,0.2f,0.3f,0.0f,0.1f},{0.35f,0.5f,0.7f,0.15f,0.4f}},
    {kDlyDiv, {0.0f,0.0f,0.3f,0.0f,0.0f},{0.5f,0.8f,1.0f,0.3f,0.8f}},
    {kDlyFb,  {0.2f,0.3f,0.3f,0.0f,0.2f},{0.5f,0.6f,0.7f,0.2f,0.6f}},
    {kWarm,   {0.3f,0.4f,0.3f,0.5f,0.2f},{0.7f,0.8f,0.8f,0.9f,0.7f}},
};
static const int kRollCount = sizeof(kRollTable)/sizeof(kRollTable[0]);

static inline bool g2bInGroup(int p,int g){
    switch(g){
        case 0: return p==kOsc1W||p==kOsc2W||p==kO2Oct||p==kO2Det||p==kO2Lvl||p==kNoise||p==kPEnv;
        case 1: return p==kAtk||p==kDec||p==kSus||p==kRel;
        case 2: return p==kCut||p==kRes||p==kFEnv||p==kLfoR||p==kLfoD;
        default:return p==kDrive||p==kRev||p==kDly||p==kDlyDiv||p==kDlyFb||p==kWarm;
    }
}

// schrijf gerolde waarden in out[] (lengte kParamCount). mode/locks uit fP.
static inline void g2bRollDice(float* fP,uint32_t& rnd,bool mutate){
    const int mode=(int)(fP[kMode]*4.f+0.5f);
    const bool lO=fP[kLockOsc]>0.5f,lE=fP[kLockEnv]>0.5f,lF=fP[kLockFilt]>0.5f,lX=fP[kLockFx]>0.5f;
    for(int t=0;t<kRollCount;t++){
        const RollR&r=kRollTable[t];
        if(lO&&g2bInGroup(r.p,0))continue;
        if(lE&&g2bInGroup(r.p,1))continue;
        if(lF&&g2bInGroup(r.p,2))continue;
        if(lX&&g2bInGroup(r.p,3))continue;
        const float lo=r.lo[mode],hi=r.hi[mode];
        rnd=rnd*1664525u+1013904223u;
        const float u=((rnd>>8)&0xFFFFFF)/16777216.f;
        float v=mutate?fP[r.p]+(u-0.5f)*0.16f:lo+(hi-lo)*u;
        if(v<lo)v=lo; if(v>hi)v=hi;
        fP[r.p]=v;
    }
}
static inline void g2bDiceName(int mode,uint32_t& rnd,char* out){
    static const char* kAdj[]={"Cyber","Neon","Dusty","Ghost","Liquid","Toxic","Galactic","Vintage","Crystal","Solar","Midnight","Electric","Velvet","Cosmic","Stealth"};
    static const char* kNoun[5][8]={
        {"Lead","Key","Arp","Edge","Spark","Line","Bell","Wire"},
        {"Pad","Cloud","Field","Halo","Drift","Veil","Sky","Bloom"},
        {"Drone","Texture","Air","Static","Mist","Rift","Shade","Echo"},
        {"Sub","Bass","Abyss","Core","Leviathan","Weight","Floor","Thump"},
        {"Impact","Hit","Slam","Strike","Punch","Burst","Knock","Crash"}};
    auto n=[&](int m){rnd=rnd*1664525u+1013904223u;return ((rnd>>8)&0xFFFFFF)/16777216.f*(float)m;};
    std::snprintf(out,63,"%s %s %03d",kAdj[(int)n(15)%15],kNoun[mode][(int)n(8)%8],(int)n(999));
}
