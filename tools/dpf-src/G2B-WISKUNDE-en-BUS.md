# G2B Wiskunde & BUS — interne dev-notitie

Doel: elke toekomstige sessie kan direct doorbouwen zonder de logica te herontdekken.

## Genre-cultuur wiskunde (in g2b_geluidmidi `genAntwoord`, g2b_band, g2b_brein_gio)

- **Grid**: 64 stappen = 4 bars van 16 (16e-noten). `fStep16` = samples per 16e.
- **Toonsoort-lock**: `scaleDeg(deg)` projecteert elke graad op {0,2,3,5,7,8,10} (mineur) of majeur —
  `60+fAKey+scaleDeg(deg)` kan wiskundig geen valse noot geven.
- **HEFTIG-morph**: <33 echo (exacte kopie van call_), 33-66 dialoog (morph 0..1: echo fade-out +
  gatenvuller fade-in), >66 syncopen-gatenvuller (`callOcc[]` markeert jouw stappen; lege stappen
  krijgen +0.45 kans, ghost-vel 20-40).
- **Tresillo (KAWINA=6)**: vel×1.25 op stappen {0,3,6,8,11,14} mod 16.
- **Dembow-lock (DEMBOW=5)**: stap 3+11 geforceerd (vel-boost), stap 4,5,12,13 ×0.12 (bas-ruimte).
- **Interval-lock**: gegenereerde noot max ±4 semitonen van laatste input-noot (octaaf-pull ±7).
- **Humanize-curve**: `vel *= 0.80+0.35·sin(π·step/64)` — opbouw naar frase-midden.
- **Ademende jitter**: export-ticks `±(rand%11-5) + 3·sin(2π·tick/7680)` — ademt over 4 bars.

## Cross-plugin BUS (`g2b_bus.h`, shared memory)

- Windows: `CreateFileMapping Local\G2BMixerBus`; Linux: `shm_open /g2b_mixer_bus`. Lock-free,
  alleen `volatile` reads/writes — nooit blokkeren in processBlock.
- Layout: magic 'G2B2' + 32 slots (mixer-energie) + masterE/mask + **midi-regio** (achteraan,
  offsets van oudere members ongewijzigd → oude binaries blijven compatibel).
- MIDI-regio: `midiMagic 'G2BM'`, `midiSeq` (versie-teller), `midiGenre`, `midiKeyMin`
  (root +12 bij mineur), `midiCount`, `midiAt16/midiNote/midiVel[256]`.
- **Publisher**: GELUID→MIDI `pubBus_()` na elke `genAntwoord()` (ook via LAAD).
- **Consumer**: BAND `generateArr` leest de regio → `busMel` wordt melodie-seed dat over
  64-stappen blokken door het hele arrangement herhaalt (vel opbouw 0.75→1.25).
  Genre-pill wordt mee-overgenomen als geldig. `fBusSync` (kOBus) toont "SYNC ↔ MIDI" in de UI.

## Sectie-engine (BAND `writeArr`)

- Secties INTRO,BUILD,DROP,BREAK,OUTRO met `kSL` lengte-multipliers; `secOK` schaalt vel per sectie.
- **KLOOF-KNIP**: noten in laatste 4×120 ticks voor DROP-start met vel>45 worden geskipt —
  spooknoten (≤45) mogen blijven → authentieke break voor de drop + snare-roll 60→116.

## Build-regels (verkorst)

- Toolchain wisselen? ALTIJD `rm -rf build/<plug> build/dgl build/libdgl-*` — gemengde .o
  objecten geven `undefined reference`.
- Disk loopt vol bij ~30+ plugin builds op 124G: ruim eerst op, of bouw in batches.
