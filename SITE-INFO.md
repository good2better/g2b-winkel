# SITE-INFO — product-sessie recepten & valkuilen

Eigen notitieboek van de product-sessie (winkel/pakketten/previews).
Niet verwijderen; sessies werken dit bij.

## Upload-patroon (GEEN git push — parallelle sessies)
- Altijd via GitHub **Git Data API**: blob → tree (base_tree=HEAD tree) →
  commit → PATCH refs/heads/main → daarna ALTIJD
  `gh api commits/<sha> --jq '.files[].filename'` verifiëren.
- Blobs via `--input /tmp/blobbody.json` (json met base64 content) — NOOIT
  `-f content=` (argv-limit >~100KB).
- Temp-files per run uniek; bij crash = oud tree.json → stille rollback
  (gebeurd: products.json deels teruggezet).

## products.json
- indent=1 (`json.dump(...,indent=1)`) — indent=2 herschrijft alles.
- Plugin-entry velden: id,titel,soort:"digitaal",beschrijving,prijs,
  links{koop:""},groep:"software",cover,demo/video(mp4),badge.
- koop:"" → "binnenkort"-knop; gratis = directe zip-url in koop.

## d/-dirs
- hash = 14-hex (zelf kiezen, uniek). info.json: bestanden[]-lijst
  → bedankt-pagina knoppen; >100MB via Release-asset "url" ipv "bestand".

## Plugin-zip anatomie
- `G2B <Naam>/<Naam>.vst3/` (map) + `<Naam>.clap` + `source/*.{cpp,h}` +
  INSTALLEREN.txt (+ LICENTIE/HANDLEIDING bij packs).
- Herbouwen = unzip → basename-replace uit ~/dpf/bin + ~/dpf/plugins → rezip.
  Bin-map: lowercase `.clap` + inner `.vst3` PE; `G2B X.vst3`→`g2b_x.vst3`.

## Previews
- Plugin-demo = cover.png + mp3 → mp4 (`-loop 1 -framerate 1`, crf 26,
  +faststart, aac 160k). Beat-preview = `preview:`-veld mp3 (soort audio).
- Audio-render via Python-mirror van de engine (~/work/glider-prev/render.py)
  — zelfde kPat/kBas/kits/pump; volumedetect-check op elk segment.

## Engine (devin-test) is van de engine-sessie — NIET aanpassen.
- Donder Glider VST-lijn = eigen tech (~/dpf/plugins/*, mingw64 build).
- PRO-versie = privé ~/G2B-Eigen/, NOOIT in de winkel.
