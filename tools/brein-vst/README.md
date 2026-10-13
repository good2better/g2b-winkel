# G2B-Brein VST-kit (C++)

Het 986-midi brein als embedbare C++-engine — klaar voor een DPF-plugin
(`~/dpf/plugins/G2BBrein/`). Gegenereerde noten: (tick,dur,ch,note,vel), 480tpq,
drums op kanaal 10 (ch=9), melodie-lagen kanaal 1 (ch=0) → MIDI-out naar
ieder instrument in REAPER/FL/Ableton.

## Bestanden
- `brein_data.h` — autogen: 219 kits, 977 parts, 977 mel-lagen (146KB)
- `brein_gen.h` — `BreinGen(seed).gen(kitIdx,bars)` → `std::vector<BNote>`
- `gen_brein_data.py` — hergenereert brein_data.h uit brein4.json
- `brein_test.cpp` — native test + SMF-schrijver (geen DPF nodig)

## Native testen (Linux, geen DPF)
```
g++ -O2 -I. brein_test.cpp -o brein_test
./brein_test <genreIdx> <kitIdx> <bars>   # 0=misc 1=808 2=keys 3=kawina 4=trap
```

## DPF-integratie (plugin-sessie)
1. `~/dpf/plugins/G2BBrein/` aanmaken (template = Drums-plugin).
2. `DISTRHO_PLUGIN_WANT_MIDI_OUTPUT 1` + `DISTRHO_PLUGIN_WANT_TIMEPOS 1`.
3. Genre+kit als program-pills (5 genres → kGenreKits), BARS+DICE+SEED knobs.
4. In run(): noten uit gen() → midi-events op exacte frames schedulen
   (recept = DonderGlider midiHit + EXPORT .MID vendored midifile).
5. Bouwen: `make mingw64 -C plugins/G2BBrein -j4` → g2b_brein.vst3+.clap.
