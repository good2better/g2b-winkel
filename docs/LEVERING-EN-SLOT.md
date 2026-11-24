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
