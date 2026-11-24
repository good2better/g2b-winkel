#!/usr/bin/env python3
"""Regenereert de demo-lijst in portfolio.html uit tools/ + media/.
Gebruik: python3 tools/portfolio-sync.py  (daarna commit+push)"""
import re,os,json
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CURATED=[  # (pad, naam, tag, beschrijving) — alleen goede, werkende demo's
("tools/melodie/","G2B MELODIE","live demo","AI-melodie-fabriek: 16-maten grid, blok-selectie, eigen synth."),
("tools/basfabriek/","G2B BASFABRIEK","live demo","Baslijn-generator met klik-bewerking en live audio."),
("tools/werkstation-web/","G2B WERKSTATION","live demo","Multi-track + GIO-brein + release-pakket."),
("tools/gaw/","G2B-GAW","live demo","8-track DAW in de browser."),
("tools/factuurmaker/","FACTUURMAKER","live demo","Factuur als PDF in 1 minuut."),
("tools/crmfabriek/","CRM-FABRIEK","live demo","Leads-bord: Nieuw→Verkocht, CSV in/uit."),
("tools/imagelab-web/imagelab.html","IMAGELAB","live demo","AI-beeldbewerking met node-graph."),
("tools/menger-web/","MENGER-web","live demo","Mix & master in browser — eigen DSP-keten."),
("tools/laagbaas/","LAAGBAAS","live demo","Master & stem-routing."),
("tools/voicetag/","VOICETAG-STUDIO","live demo","Je naam als pro-voice-tag."),
("tools/shorts-web/","SHORTS-KNIPPER","live demo","Video → 9:16 short via ffmpeg.wasm."),
("tools/coverart/","COVER-ART FABRIEK","live demo","Beat-covers in 1 klik."),
("airdrums.html","AIRDRUMS","live demo","Drummen met je camera — motion-tracking."),
("schoolspelen/","SCHOOLSPELEN","live demo","Offline educatieve games voor kinderen."),
("gsbooks/","GSBOOKS","live demo","Digitale kinderboeken met audio."),
]
VIDEOS=[("melodie-bas-club-promo.mp4","Melodie + Basfabriek + Club","promo-montage"),
("aura-dicesynth-promo.mp4","AURA Dice-Synth","promo"),("aura-midiforge-demo.mp4","AURA MIDI-Forge","demo"),
("airdrums-uitleg.mp4","AirDrums uitleg","video"),("agency1-uitleg.mp4","Agency-tool","uitleg"),
("ritmosbundle-promo.mp4","Ritmos-Bundle","promo")]
def mk(pad,naam,tag,desc):
    return f'<a class="demo" href="https://good2better.com/{pad}" target="_blank"><b>{naam}</b> — {desc}<span class="badge">{tag}</span></a>'
demos="\n".join(mk(*x) for x in CURATED)
vids="\n".join(f'<a class="demo" href="https://good2better.com/media/{v}" target="_blank"><b>{n}</b> — video <span class="badge">{t}</span></a>' for v,n,t in VIDEOS)
s=open(f"{ROOT}/portfolio.html").read()
s=re.sub(r'<!-- DEMOS-START -->.*?<!-- DEMOS-EIND -->',
 f'<!-- DEMOS-START -->\n<div class="sec"><h2>Bewijs — probeer het zelf, live (niets te downloaden)</h2>\n{demos}\n</div>\n<!-- DEMOS-EIND -->',s,flags=re.S)
s=re.sub(r'(<div class="sec"><h2>Video-demo\'s</h2>).*?(</div>)',r'\1\n'+vids+r'\2',s,flags=re.S,count=1)
open(f"{ROOT}/portfolio.html","w").write(s)
print(f"synced: {len(CURATED)} demos, {len(VIDEOS)} videos")
