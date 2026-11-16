// G2B Sleutel-Helper — toondata: key-detectie, toonladders, akkoorden, G2B-suggesties.
// Krumhansl-Schmuckler profielen (publiek-domein onderzoeksconstanten).
// © G2B (good2better) — MIT.
#pragma once
#include <stdint.h>
#include <cstdio>
#include <cstring>

static const char* kNoteNam[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

// Krumhansl-Schmuckler
static const float kProfMaj[12] = {6.35f,2.23f,3.48f,2.33f,4.38f,4.09f,2.52f,5.19f,2.39f,3.66f,2.29f,2.88f};
static const float kProfMin[12] = {6.33f,2.68f,3.52f,5.38f,2.60f,3.53f,2.54f,4.75f,3.98f,2.69f,3.34f,3.17f};

// toonladder-bitmasken (bit i = toonhoogteklasse i semitoon boven de grondtoon)
static const uint16_t kMaskMaj = 0xAB5;      // 0 2 4 5 7 9 11
static const uint16_t kMaskMin = 0x5AD;      // 0 2 3 5 7 8 10  (natur. mineur)
static const uint16_t kMaskMinHarm = 0x9AD;  // 0 2 3 5 7 8 11
static const uint16_t kMaskMinMel = 0xBAD;   // 0 2 3 5 7 9 11
static const uint16_t kMaskPentMaj = 0x295;  // 0 2 4 7 9
static const uint16_t kMaskPentMin = 0x4A9;  // 0 3 5 7 10
static const uint16_t kMaskBlues = 0x4E9;    // 0 3 5 6 7 10

static const int kStepMaj[7] = {0,2,4,5,7,9,11};
static const int kStepMin[7] = {0,2,3,5,7,8,10};
// diatonische akkoordkwaliteit per trap (0 maj, 1 min, 2 dim)
static const int kQualMaj[7] = {0,1,1,0,0,1,2};
static const int kQualMin[7] = {1,2,0,1,1,0,0};

// volgend-akkoord: per trap de 3 meest gebruikte vervolg-trappen (pop/urban/gospel-praktijk)
static const int kNextMaj[7][3] = {{3,4,5},{4,0,3},{5,3,1},{4,0,1},{0,5,3},{3,1,4},{0,5,2}};
static const int kNextMin[7][3] = {{5,3,6},{4,0,6},{5,3,6},{4,0,6},{0,5,3},{6,3,2},{2,0,5}};

// G2B-pack suggesties per focus × stemming — wijst naar ECHTE library-families
// focus: 0 ALLES 1 BAS-808 2 AKKOORDEN 3 MELODIE · mode: 0 majeur 1 mineur
static const char* kG2BSug[4][2] = {
 /*ALLES*/      {"-> packs: house / soca / afro / uplifting  ·  08-loops: uplift_loop116",
                "-> packs: drill / phonk / urban-drill / cinematic  ·  08-loops: cinematic_loop92"},
 /*BAS-808*/    {"-> 02-bass: bass_sub_A..G + bass_pluck  ·  bubbling: bass_bubble",
                "-> 02-bass: bass_glide_drill / bass_glide_trap + subs"},
 /*AKKOORDEN*/  {"-> 06-keys: montuno, stabs, motif_uplift_F, motif_soca_lead",
                "-> 06-keys: motif_epic_Cmin, motif_dark_piano, motif_drama_Emin"},
 /*MELODIE*/    {"-> 06-keys: motif_uplift_F, motif_bell_A, motif_feest_A, motif_chill_G",
                "-> 06-keys: motif_cine_Dmin, motif_mysterie_D, motif_lofi_Amin"},
};

// akkoordcode: root*3+q (q 0 maj, 1 min, 2 dim) — 36 = geen
static inline void g2bChordName(int code, char* out, int n){
    if(code<0||code>35){ out[0]=0; return; }
    const int q=code%3;
    std::snprintf(out,n,"%s%s",kNoteNam[code/3],q==1?"m":q==2?"dim":"");
}

// Suggestie-teksten — gedeeld door plugin en UI zodat de UI ze zelf toont (geen host-state nodig)
static inline void g2bBuildSug(int key,int minor,int focus,float kracht,float dim,
                               char* s1,int n1,char* s2,int n2,char* s3,int n3)
{
    s1[0]=s2[0]=s3[0]=0;
    if(key<0) return;
    if(focus<0||focus>3) focus=0;
    const int depth=1+(int)(dim/3);
    const int* step=minor?kStepMin:kStepMaj;
    if(focus==0||focus==2){
        char tmp[160]="akkoorden: ";
        for(int d=0;d<7&&d<depth;d++){
            const int pc=(key+step[d])%12, q=minor?kQualMin[d]:kQualMaj[d];
            char b[12]; std::snprintf(b,12,"%s%s ",kNoteNam[pc],q==1?"m":q==2?"dim":"");
            std::strcat(tmp,b);
        }
        if(kracht>0.55f){
            const int br[3]={minor?(key+8)%12:(key+3)%12, minor?(key+10)%12:(key+8)%12, minor?(key+3)%12:(key+10)%12};
            char b[40]; std::snprintf(b,40,"· leen: %s %s %s",kNoteNam[br[0]],kNoteNam[br[1]],kNoteNam[br[2]]);
            std::strcat(tmp,b);
        }
        std::snprintf(s1,n1,"%s",tmp);
    }
    if(focus==0||focus==1){
        const int degMin[4]={0,7,5,10}, degMaj[4]={0,7,5,9};
        char b[96]="808/bas grondtonen: ";
        for(int i=0;i<(kracht>0.5f?4:3);i++){
            std::strcat(b,kNoteNam[(key+(minor?degMin[i]:degMaj[i]))%12]); std::strcat(b," ");
        }
        std::strcat(b,"· laag houden: E1-B1");
        std::snprintf(s2,n2,"%s",b);
    }
    if(focus==3)
        std::snprintf(s1,n1,"melodie: %s-pentatoniek = altijd veilig · %s = spanning/kleur",
                      minor?"mineur":"majeur", kNoteNam[(key+(minor?8:11))%12]);
    std::snprintf(s3,n3,"G2B %s",kG2BSug[focus][minor]);
}
