// Gedeelde rack-status DSP→UI.
// In de standalone JACK-build wordt updateStateValue niet naar de UI
// teruggeleverd; de UI leest dit struct direct uit hetzelfde proces.
#pragma once

struct SongRackView
{
    char mask[10][16]; // vakje aan/uit per rij
    char mute[10];     // laag aan/uit
    int  extra[2];     // +X1/+X2 gekozen geluid (0=uit,1=TMB,2=CBL,3=RIM,4=SHK,5=BEL)
    signed char melDeg[16]; // MEL-roll: toontrap per stap (-1 = engine kiest)
    signed char basDeg[8];  // 808-roll: toontrap per halve-maat-cel (-1 = uit)
    float prm[27];          // echte param-waardes (preset/stijl → knoppen draaien mee)
};

extern SongRackView* gSongRackView;
