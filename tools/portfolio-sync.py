#!/usr/bin/env python3
"""Regenereert de demo-lijst in portfolio.html uit tools/ + media/.
Gebruik: python3 tools/portfolio-sync.py  (daarna commit+push)"""
import re,os,json
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CURATED=[  # (pad, naam, tag, beschrijving) — alleen goede, werkende demo's
("tools/melodie/","G2B MELODIE","live demo","AI-melodie-fabriek: 16-maten grid, blokken per-stap aan/uit, octaaf per spoor, leert stijl uit .mid-drops, eigen synth-engine."),
("tools/basfabriek/","G2B BASFABRIEK","live demo","Baslijn-generator met klik-bewerking, mute per laag en live audio — eigen compositie-engine."),
("tools/werkstation-web/","G2B WERKSTATION","live demo","Complete studio: multi-track + GIO commando-brein + prio-mixbus + 1-klik release-pack + .G2B projectbestand."),
("tools/gaw/","G2B-GAW","live demo","8-track DAW in de browser met AI-sliders en sync-modus — geen installatie."),
("tools/factuurmaker/","FACTUURMAKER","live demo","Factuur als nette PDF in 1 minuut — lokaal, privacy-proof."),
("tools/crmfabriek/","CRM-FABRIEK","live demo","Leads-bord Nieuw→Verkocht met CSV in/uit en offline data — eigen beheer."),
("tools/imagelab-web/imagelab.html","IMAGELAB","live demo","AI-beeldbewerking met node-graph pipeline — lokaal, CPU-vriendelijk."),
("tools/menger-web/","MENGER-web","live demo","Mix & master in browser — eigen DSP-keten met prio-mixbus en gain-staging."),
("tools/laagbaas/","LAAGBAAS","live demo","Master & stem-routing met eigen leveling-engine."),
("tools/voicetag/","VOICETAG-STUDIO","live demo","Je naam als pro-voice-tag: TTS + FX-keten in de browser."),
("tools/shorts-web/","SHORTS-KNIPPER","live demo","Video → 9:16 short via ffmpeg.wasm — draait 100% client-side."),
("tools/coverart/","COVER-ART FABRIEK","live demo","Beat-covers in 1 klik — eigen layout-engine."),
("airdrums.html","AIRDRUMS","live demo","Drummen met je camera — motion-tracking zonder extra hardware."),
("schoolspelen/","SCHOOLSPELEN","live demo","Offline educatieve games voor kinderen — werkt zonder internet."),
("gsbooks/","GSBOOKS","live demo","Digitale kinderboeken met ingesproken audio."),
]
VIDEOS=[("melodie-bas-club-promo.mp4","Melodie + Basfabriek + Club","promo-montage"),
("workstation-demo.mp4","G2B Werkstation","pro demo 48s"),
("g2bagency-demo.mp4","Agency-tool","pro demo 42s"),
("g2bairdrums-demo.mp4","AirDrums","pro demo 40s"),
("g2bmixer-demo.mp4","G2B Mixer","pro demo 32s"),
("aura-dicesynth-promo.mp4","AURA Dice-Synth","promo"),
("gamefabriek-demo.mp4","Game-Fabriek","pro demo 63s"),
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
