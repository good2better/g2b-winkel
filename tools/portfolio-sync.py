#!/usr/bin/env python3
"""Regenereert de demo-lijst in portfolio.html uit tools/ + media/.
Gebruik: python3 tools/portfolio-sync.py  (daarna commit+push)"""
import re,os,json
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ZAKELIJK=[  # eerst: zakelijke tools (MKB-beslissers)
("tools/leadradar/","B2B LEAD- & ACQUISITIE-MACHINE","live demo","Complete klant-machine: pipeline-radar, dagelijkse actielijst, offerte-PDF maker en opvolg-scripts — met demo-data."),
("tools/crmfabriek/","CRM-FABRIEK","live demo","Leads-bord Nieuw→Verkocht met CSV in/uit — eigen beheer, geen abonnement."),
("tools/factuurmaker/","FACTUURMAKER","live demo","Factuur als nette PDF in 1 minuut — lokaal, privacy-proof."),
("tools/imagelab-web/imagelab.html","IMAGELAB","live demo","AI-beeldbewerking met node-graph pipeline — lokaal, CPU-vriendelijk."),
("tools/shorts-web/","SHORTS-KNIPPER","live demo","Video → 9:16 short via ffmpeg.wasm — draait 100% client-side."),
("tools/voicetag/","VOICETAG-STUDIO","live demo","Je naam als pro-voice-tag: TTS + FX-keten in de browser."),
("tools/coverart/","COVER-ART FABRIEK","live demo","Beat-covers in 1 klik — eigen layout-engine."),
]
AUDIO=[  # daarna: audio, media & educatie
("tools/melodie/","G2B MELODIE","live demo","AI-melodie-fabriek: 16-maten grid, blokken per-stap aan/uit, octaaf per spoor, leert stijl uit .mid-drops, eigen synth-engine."),
("tools/basfabriek/","G2B BASFABRIEK","live demo","Baslijn-generator met klik-bewerking, mute per laag en live audio — eigen compositie-engine."),
("tools/werkstation-web/","G2B WERKSTATION","live demo","Complete studio: multi-track + GIO commando-brein + prio-mixbus + 1-klik release-pack + .G2B projectbestand."),
("tools/gaw/","G2B-GAW","live demo","8-track DAW in de browser met AI-sliders en sync-modus — geen installatie."),
("tools/menger-web/","MENGER-web","live demo","Mix & master in browser — eigen DSP-keten met prio-mixbus en gain-staging."),
("tools/laagbaas/","LAAGBAAS","live demo","Master & stem-routing met eigen leveling-engine."),
("airdrums.html","AIRDRUMS","live demo","Drummen met je camera — motion-tracking zonder extra hardware."),
("schoolspelen/","SCHOOLSPELEN","live demo","Offline educatieve games voor kinderen — werkt zonder internet."),
("gsbooks/","GSBOOKS","live demo","Digitale kinderboeken met ingesproken audio."),
]
CURATED=ZAKELIJK+AUDIO
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
demos=("<h3 style='color:#3ef26d;font-size:14px;margin:14px 0 4px'>Zakelijk & automatisering — geen abonnementen, data blijft van jou <a href='#contact' onclick=\"vul('Acquisitie-systeem of zakelijke tool')\" style='color:#efb42c;font-size:12px'>→ vraag demo voor mijn bedrijf</a></h3>\n"
 +"\n".join(mk(*x) for x in ZAKELIJK)
 +"\n<h3 style='color:#3ef26d;font-size:14px;margin:22px 0 4px'>Audio, media & educatie — bewijs van snelheid en real-time techniek <a href='#contact' onclick=\"vul('Audio- of media-tool')\" style='color:#efb42c;font-size:12px'>→ vraag demo</a></h3>\n"
 +"\n".join(mk(*x) for x in AUDIO))
vids="\n".join(f'<a class="demo" href="https://good2better.com/media/{v}" target="_blank"><b>{n}</b> — video <span class="badge">{t}</span></a>' for v,n,t in VIDEOS)
s=open(f"{ROOT}/portfolio.html").read()
s=re.sub(r'<!-- DEMOS-START -->.*?<!-- DEMOS-EIND -->',
 f'<!-- DEMOS-START -->\n<div class="sec"><h2>Bewijs — probeer het zelf, live (niets te downloaden)</h2>\n{demos}\n</div>\n<!-- DEMOS-EIND -->',s,flags=re.S)
s=re.sub(r'(<div class="sec"><h2>Video-demo\'s</h2>).*?(</div>)',r'\1\n'+vids+r'\2',s,flags=re.S,count=1)
open(f"{ROOT}/portfolio.html","w").write(s)
print(f"synced: {len(CURATED)} demos, {len(VIDEOS)} videos")
