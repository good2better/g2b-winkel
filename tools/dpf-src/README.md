# G2B DPF plugin-bronnen

Eigen C++-broncode van alle G2B VST3/CLAP plugins (DPF-framework).
Grote gegenereerde sample-headers (>40 MB, o.a. *_data.h in
g2b_bestof/g2b_instr/g2b_orkest/g2b_gitaar/g2b_wereld/g2b_carib/
g2b_instr3/g2b_song) staan NIET hier — die worden door de
gen-headers stap uit de G2B-sample-packs gebouwd in de
DPF-werkomgeving (/home/ubuntu/dpf/plugins).

Bouwen: in de DPF-checkout `make -C plugins/<dir> mingw64 -j4`
(Windows) of `make -C plugins/<dir> -j4` (Linux). g2b_bus.h is de
gedeelde-geheugen BUS tussen MIXER/MASTER/BAND/GELUID-MIDI.
