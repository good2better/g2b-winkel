/* G2B Prompt-Maker — woorden → synth-settings vertaler (port van de browser PROMPT_MAP/ADJ). */
#pragma once
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <strings.h>
#include "dicesynth_roll.h"

struct WordHit { const char* w; int mode; const char* tag; };
static const WordHit kWordMap[]={
    {"kick",4,"kick"},{"bassdrum",4,"kick"},{"donder",4,"kick"},
    {"808",3,"bass"},{"sub",3,"bass"},{"baseline",3,"bass"},{"bass",3,"bass"},
    {"snare",4,"snare"},{"clap",4,"snare"},{"rim",4,"snare"},
    {"hat",4,"hat"},{"tik",4,"hat"},{"shaker",4,"hat"},
    {"pad",1,"pad"},{"zacht",1,"pad"},{"soft",1,"pad"},{"cloud",1,"pad"},{"veil",1,"pad"},{"warm",1,"pad"},{"lush",1,"pad"},
    {"drone",2,"texture"},{"ambient",2,"texture"},{"atmos",2,"texture"},{"textur",2,"texture"},{"mist",2,"texture"},{"sfeer",2,"texture"},{"ruimte",2,"texture"},
    {"lead",0,"keys"},{"arp",0,"keys"},{"pluck",0,"keys"},{"bell",0,"keys"},{"piano",0,"keys"},{"key",0,"keys"},{"melodie",0,"keys"},
    {"hit",4,"hit"},{"impact",4,"hit"},{"slam",4,"hit"},{"riser",4,"hit"},{"effect",4,"hit"},{"knock",4,"hit"},{"fx",4,"hit"}};
static const int kWordMapN=sizeof(kWordMap)/sizeof(kWordMap[0]);

struct AdjHit { const char* w; int p; float d; };
static const AdjHit kAdjMap[]={
    {"donker",kCut,-0.15f},{"dark",kCut,-0.15f},{"duister",kCut,-0.15f},
    {"donker",kWarm,0.2f},{"dark",kWarm,0.2f},{"duister",kWarm,0.2f},
    {"helder",kCut,0.2f},{"bright",kCut,0.2f},{"sparkl",kCut,0.2f},
    {"helder",kRes,0.1f},{"bright",kRes,0.1f},{"sparkl",kRes,0.1f},
    {"dusty",kCut,-0.2f},{"lofi",kCut,-0.2f},{"lo-fi",kCut,-0.2f},{"vintage",kCut,-0.2f},{"old",kCut,-0.2f},
    {"dusty",kNoise,0.12f},{"lofi",kNoise,0.12f},{"lo-fi",kNoise,0.12f},{"vintage",kNoise,0.12f},{"old",kNoise,0.12f},
    {"dusty",kWarm,0.25f},{"lofi",kWarm,0.25f},{"vintage",kWarm,0.25f},{"old",kWarm,0.25f},
    {"vet",kDrive,0.3f},{"fat",kDrive,0.3f},{"dik",kDrive,0.3f},{"heavy",kDrive,0.3f},
    {"vet",kO2Lvl,0.15f},{"fat",kO2Lvl,0.15f},{"dik",kO2Lvl,0.15f},{"heavy",kO2Lvl,0.15f},
    {"ruimtelijk",kRev,0.3f},{"space",kRev,0.3f},{"zaal",kRev,0.3f},{"hall",kRev,0.3f},
    {"ruimtelijk",kDly,0.2f},{"space",kDly,0.2f},{"zaal",kDly,0.2f},{"hall",kDly,0.2f},
    {"strak",kRev,-0.25f},{"tight",kRev,-0.25f},{"droog",kRev,-0.25f},{"dry",kRev,-0.25f},
    {"strak",kDly,-0.2f},{"tight",kDly,-0.2f},{"droog",kDly,-0.2f},{"dry",kDly,-0.2f},
    {"strak",kRel,-0.2f},{"tight",kRel,-0.2f},{"droog",kRel,-0.2f},{"dry",kRel,-0.2f},
    {"agress",kDrive,0.35f},{"hard",kDrive,0.35f},{"ruw",kDrive,0.35f},{"raw",kDrive,0.35f},{"industrial",kDrive,0.35f},{"techno",kDrive,0.35f},
    {"agress",kCut,0.1f},{"hard",kCut,0.1f},{"ruw",kCut,0.1f},{"raw",kCut,0.1f},{"industrial",kCut,0.1f},{"techno",kCut,0.1f},
    {"agress",kRes,0.15f},{"hard",kRes,0.15f},{"ruw",kRes,0.15f},{"raw",kRes,0.15f},{"industrial",kRes,0.15f},{"techno",kRes,0.15f},
    {"zacht",kAtk,0.25f},{"soft",kAtk,0.25f},{"smooth",kAtk,0.25f},{"mellow",kAtk,0.25f},
    {"zacht",kDrive,-0.2f},{"soft",kDrive,-0.2f},{"smooth",kDrive,-0.2f},{"mellow",kDrive,-0.2f}};
static const int kAdjMapN=sizeof(kAdjMap)/sizeof(kAdjMap[0]);

// chips die onder het veld staan — volgorde = type woorden eerst, dan sfeer
static const char* kChips[]={"kick","808","snare","clap","hat","shaker","bass","sub","pad","drone","ambient","texture","lead","arp","pluck","bell","piano","hit","impact","riser","donker","helder","dusty","lofi","vintage","vet","ruimtelijk","strak","agressief","techno","zacht","mellow"};
static const int kChipsN=sizeof(kChips)/sizeof(kChips[0]);

// zoek eerste type-woord in prompt (substring-match zoals de regex in de browser)
static inline int g2bPromptMode(const char* prompt,const char** tag){
    char low[256]; int i=0;
    for(;prompt[i]&&i<255;i++){char c=prompt[i];low[i]=(c>='A'&&c<='Z')?c+32:c;}
    low[i]=0;
    for(int k=0;k<kWordMapN;k++)
        if(std::strstr(low,kWordMap[k].w)){*tag=kWordMap[k].tag;return kWordMap[k].mode;}
    *tag="keys";return 0;
}
// pas sfeer-woord tweaks toe op params (per kandidaat, lichte random zoals browser)
static inline void g2bPromptTweak(const char* prompt,float* P,uint32_t& rnd){
    char low[256];int i=0;
    for(;prompt[i]&&i<255;i++){char c=prompt[i];low[i]=(c>='A'&&c<='Z')?c+32:c;}
    low[i]=0;
    for(int k=0;k<kAdjMapN;k++)if(std::strstr(low,kAdjMap[k].w)){
        rnd=rnd*1664525u+1013904223u;
        const float u=((rnd>>8)&0xFFFFFF)/16777216.f;
        float v=P[kAdjMap[k].p]+kAdjMap[k].d*(0.7f+u*0.6f);
        if(v<0)v=0;if(v>1)v=1;P[kAdjMap[k].p]=v;}
}
