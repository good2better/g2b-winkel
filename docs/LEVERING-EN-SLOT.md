# Hoe verkoop-levering werkt (intern overzicht — NIET aanpassen zonder test)

## Flow
bestel.html?p=<id> → mailto-order of Stripe-link → betaling → Stripe success_url
→ bedankt.html?k=<sleutel> → fetch d/<sleutel>/info.json → download-links.

## Wat klopt er altijd
- d/<sleutel>/ bevat info.json met 'bestanden'[] (elk: naam + url naar github-release zip
  of pad 'd/<sleutel>/<bestand>') + optioneel 'licentie' + 'cover'.
- 'download_dir' in products.json is NIET het leverings-pad — slechts referentie.
- Echte levering loopt via de hash-sleutel in de Stripe success_url.

## Slot-systeem
- js/g2b-gate.js data-tier=mijn/vriend/leden: 🔒 pagina-slot voor tools.
- js/g2b-export.js G2BExport.ok(slug): export-slot, G2B-<SLUG>-XXXXXX + G2B-CLUB-XXXXXX (alle slugs).
- Gekochte tools leveren de ONBLOKKEERDE versie via de download (release-zip), de webversie blijft op slot.
- Gratis-demo's die open moeten blijven (portfolio): leadradar, schoolspelen, gsbooks, koor, lesfabriek.

## Check bij twijfel
python3 -c "import json,os,urllib.request" + HEAD op elke info.json url → 200 = levert.

## Export-slot zakelijke demo-tools (2026-11)
`tools/exportslot.js` — demo open voor bezoekers, export/download/print vraagt code.
`js/g2b-gate.js` met `data-demo="1"` opent de pagina als demo + toont code-balk onderaan.
Codes per tool (ook in info.json → kopers zien hem op bedankt-pagina):
LEAD `G2B-LEAD-L7Q2MW` · CRM `G2B-CRM-C8X4RT` · LIJST `G2B-LIJST-V5N9KD` · MAIL `G2B-MAIL-M2F7HB` · SEQ `G2B-SEQ-S9W4JC` · SHEET `G2B-SHEET-H4R8VG` · SEO `G2B-SEO-E8K5TD` · WATCH `G2B-WATCH-W9C4NB` · FLOW `G2B-FLOW-F7M6PL` · KANAAL `G2B-KANAAL-K4T8DW` · GAME `G2B-GAME-G3J7QF` · FACTUUR `G2B-FACTUUR-D3K91P` · MARK `G2B-MARK-P6T3ZN` · RADAR `G2B-RADAR-R6D2KX` · AGENCY `G2B-AGENCY-A2V9SR` · club-master `G2B-CLUB-KVJF1N`.
Tools: factuurmaker, leadfabriek, crmfabriek, lijstfabriek, mailfabriek, marketingfabriek, sequentiefabriek, sheetfabriek, gamefabriek, leadradar.

## SONIC-AUDIT (nov 2026)
- tool: tools/sonic-audit/ — meten gratis, FIX + WAV/badge-export achter code
- code: SONICAUDIT = G2B-SONIC-A7K3V9 (exportslot.js) · levering: d/sonicaudit

## ENGINE-TOOLS (nov 2026)
- melodie, basfabriek, bibliotheek, midi-bib, laagbaas, menger-web, gaw, gaw-lite: gebruik open (demo), .mid/.wav/stems-downloads achter exportslot
- codes in exportslot.js CODES (MELODIE, BASFABRIEK, BIBLIOTHEEK, MIDIBIB, LAAGBAAS, MENGERWEB, GAW, GAWMINI)
- master-clubcode + member-licentie (g2b_aura_licentie) ontgrendelen alles
