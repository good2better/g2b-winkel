// G2B Sleutel-Helper — toondata: key-detectie, toonladders, akkoorden, G2B-suggesties.
// Krumhansl-Schmuckler profielen (publiek-domein onderzoeksconstanten).
// © G2B (good2better) — MIT.
#pragma once
#include <stdint.h>

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

// diatonische akkoordkwaliteit per trap (0 maj, 1 min, 2 dim)
static const int kQualMaj[7] = {0,1,1,0,0,1,2};
static const int kQualMin[7] = {1,2,0,1,1,0,0};

// G2B-pack suggesties per focus × stemming — wijst naar ECHTE library-families
// focus: 0 ALLES 1 BAS-808 2 AKKOORDEN 3 MELODIE · mode: 0 majeur 1 mineur
static const char* kG2BSug[4][2] = {
 /*ALLES*/      {"→ packs: house / soca / afro / uplifting  ·  08-loops: uplift_loop116",
                "→ packs: drill / phonk / urban-drill / cinematic  ·  08-loops: cinematic_loop92, drama_loop70"},
 /*BAS-808*/    {"→ 02-bass: bass_sub_A..G + bass_pluck  ·  bubbling: bass_bubble",
                "→ 02-bass: bass_glide_drill / bass_glide_trap + subs  ·  drill 808-glide packs"},
 /*AKKOORDEN*/  {"→ 06-keys: montuno, stabs, motif_uplift_F, motif_soca_lead",
                "→ 06-keys: motif_epic_Cmin, motif_dark_piano, motif_drama_Emin"},
 /*MELODIE*/    {"→ 06-keys: motif_uplift_F, motif_bell_A, motif_feest_A, motif_chill_G",
                "→ 06-keys: motif_cine_Dmin, motif_mysterie_D, motif_lofi_Amin, motif_trailer_rise"},
};

