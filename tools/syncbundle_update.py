#!/usr/bin/env python3
"""Houdt de SYNC-BUNDLE actueel: verzamelt alle audio-track downloads
uit d/<id>/info.json van producten met soort=audio, en schrijft ze naar
d/syncbundle/info.json + updatet de telling in products.json.
Draai: python3 tools/syncbundle_update.py  (vanuit repo-root)"""
import json, os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
P = json.load(open(os.path.join(ROOT, 'products.json')))
bundel = []
for pr in P['producten']:
    if pr.get('soort') != 'audio':
        continue
    dd = pr.get('download_dir')
    if not dd or dd == 'd/syncbundle':
        continue
    ij = os.path.join(ROOT, dd, 'info.json')
    if not os.path.exists(ij):
        print('!! geen info.json:', dd)
        continue
    info = json.load(open(ij))
    titel = pr.get('titel', pr['id'])
    for b in info.get('bestanden', []):
        bundel.append({'naam': f"{titel} — {b['naam']}", 'url': b['url']})
    if 'url' in info:  # oud schema: losse zip
        bundel.append({'naam': f"{titel} — {info.get('bestand_release','download')}", 'url': info['url']})
# dedupe op url
seen = set(); bundel = [b for b in bundel if not (b['url'] in seen or seen.add(b['url']))]
out = {'titel': 'G2B Sync-Bundle — alle tracks + stems', 'bestanden': bundel}
json.dump(out, open(os.path.join(ROOT, 'd/syncbundle/info.json'), 'w'), ensure_ascii=False, indent=1)
ntracks = len({pr['id'] for pr in P['producten'] if pr.get('soort') == 'audio'})
for pr in P['producten']:
    if pr.get('id') == 'syncbundle':
        import re
        pr['beschrijving'] = re.sub(r'\d+ beats', f'{ntracks} beats', pr['beschrijving'])
        pr['beschrijving'] = re.sub(r'\d+ downloads', f'{len(bundel)} downloads', pr['beschrijving'])
json.dump(P, open(os.path.join(ROOT, 'products.json'), 'w'), ensure_ascii=False, indent=1)
print(f'bundel: {ntracks} tracks -> {len(bundel)} bestanden')
