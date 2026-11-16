#!/usr/bin/env python3
"""G2B shop-eindcontrole — draai: python3 tools/check-shop.py [--stripe]
Controleert dat de winkel klopt: links, covers, downloads, bundels, geheime sleutels.
--stripe haalt ook de echte Stripe-redirects op (heeft STRIPE_RESTRICTED_KEY nodig).
Exit 1 bij fouten; waarschuwingen breken de build niet."""
import json,re,os,sys,urllib.request

ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
fouten=[];warn=[]

def F(x): fouten.append(x)
def W(x): warn.append(x)

data=json.load(open('products.json'))
prods=data['producten']
paid={p['id'] for p in prods if (p.get('links') or {}).get('stripe')}

# 1) per product: cover + bestelbare link + geen publieke download-sleutel
for p in prods:
    l=p.get('links') or {}
    if 'stripe' in l and not l['stripe'].startswith('https://buy.stripe.com/'):
        F(f"{p['id']}: stripe-link is geen buy.stripe.com")
    if 'download' in l:
        if p['id'] in paid: F(f"{p['id']}: betaald product mag links.download niet publiek tonen")
        if 'bedankt.html?k=' in l['download']:
            k=l['download'].split('k=')[1]
            if not os.path.isdir(f'd/{k}'): F(f"{p['id']}: d/{k} bestaat niet")
    cov=p.get('cover')
    if cov and not cov.startswith('http') and not os.path.exists(cov):
        W(f"{p['id']}: cover {cov} mist")
    for key in ('browser','demo','video'):
        v=l.get(key) or p.get(key)
        if v and not v.startswith('http') and not os.path.exists(v):
            W(f"{p['id']}: {key} {v} mist lokaal")

# 2) leverings-mappen: elke info.json controleerbaar, urls/files bestaan
used_k=set()
for p in prods:
    for v in (p.get('links') or {}).values():
        used_k|=set(re.findall(r'k=([A-Za-z0-9_-]+)',str(v)))
for kd in sorted(os.listdir('d')):
    ij=f'd/{kd}/info.json'
    if not os.path.exists(ij):
        if os.listdir('d/'+kd): W(f'd/{kd}: geen info.json')
        continue
    try: info=json.load(open(ij))
    except Exception as e: F(f'd/{kd}: info.json kapot ({e})');continue
    ent=info.get('bestanden') or ([info] if info.get('bestand') or info.get('url') else [])
    for b in ent:
        if b.get('bestand'):
            if not os.path.exists(f'd/{kd}/{b["bestand"]}'):
                F(f'd/{kd}: bestand {b["bestand"]} mist')
        elif b.get('url'):
            u=b['url']
            if u.startswith('d/') and not os.path.exists(u):
                F(f'd/{kd}: lokale url {u} mist')

# 3) voorspelbare sleutels = publieke downloads van betaalde spullen
for k in used_k:
    if not re.match(r'^[0-9a-f]{8,}(-[a-z0-9]+)?$|^x[0-9a-f]{16,}$',k):
        pid=[p['id'] for p in prods if p['id']==k or any(k in str(v) for v in (p.get('links') or {}).values())]
        if any(x in paid for x in pid): F(f'gokbare sleutel voor betaald product: {k}')

# 4) geen koop-pagina's in de root (die zijn ongated)
for f in os.listdir('.'):
    if f.endswith('-koop.html'): F(f'{f} staat in root — hoort in geheime d/<key>/')

# 4b) gsbooks leverings-mappen + voorspelbare ?p= sleutels
GBD='gsbooks/d'
if os.path.isdir(GBD):
    for kd in sorted(os.listdir(GBD)):
        ij=f'{GBD}/{kd}/info.json'
        try: info=json.load(open(ij))
        except Exception as e: F(f'{ij}: kapot ({e})');continue
        for b in info.get('bestanden') or []:
            if b.get('bestand') and not os.path.exists(f'{GBD}/{kd}/{b["bestand"]}'):
                F(f'{ij}: bestand {b["bestand"]} mist')
        if not re.match(r'^b[0-9a-f]{16,}$',kd):
            W(f'{GBD}/{kd}: geen b-sleutel-formaat')
src=open('gsbooks/bedankt.html').read()
if 'PRODUCTEN' in src: F('gsbooks/bedankt.html: producttabel staat weer in de pagina')

# 5) bundel-consistentie: waarschuw als bundel-item op weggehaald product lijkt
titels=' '.join(p['titel'].lower()+p['id'].lower() for p in prods)
for kd in sorted(os.listdir('d')):
    ij=f'd/{kd}/info.json'
    if not os.path.exists(ij): continue
    try: info=json.load(open(ij))
    except: continue
    for b in info.get('bestanden') or []:
        nm=(b.get('naam') or '').lower()
        for woord in re.findall(r'g2b[- ][a-z]+',nm):
            w=woord.replace(' ','-').replace('g2b ','g2b-')
            if w.replace('-','') not in titels.replace('-','') and 'g2b' in nm:
                W(f'd/{kd}: "{b["naam"][:50]}" lijkt geen live product — bundel-item checken')

# 6) optioneel: Stripe-redirects vs mappen
if '--stripe' in sys.argv:
    key=os.environ.get('STRIPE_RESTRICTED_KEY')
    if not key: W('--stripe zonder STRIPE_RESTRICTED_KEY: overgeslagen')
    else:
        pls=[];after=None
        while True:
            q='payment_links?limit=100&active=true'+(('&starting_after='+after) if after else '')
            r=urllib.request.Request('https://api.stripe.com/v1/'+q,headers={'Authorization':'Bearer '+key})
            page=json.load(urllib.request.urlopen(r));pls+=page['data']
            if not page.get('has_more'): break
            after=page['data'][-1]['id']
        for pl in pls:
            u=((pl.get('after_completion') or {}).get('redirect') or {}).get('url','')
            m=re.search(r'k=([A-Za-z0-9_-]+)',u)
            if m and not os.path.isdir('d/'+m.group(1)):
                F(f'Stripe {pl["id"]}: redirect k={m.group(1)} heeft geen map')
            if m and not re.match(r'^[0-9a-f]{8,}(-[a-z0-9]+)?$|^x[0-9a-f]{16,}$',m.group(1)):
                F(f'Stripe {pl["id"]}: voorspelbare sleutel {m.group(1)}')
            mb=re.search(r'gsbooks/bedankt\.html\?p=([A-Za-z0-9_-]+)',u)
            if mb:
                if not re.match(r'^b[0-9a-f]{16,}$',mb.group(1)):
                    F(f'Stripe {pl["id"]}: voorspelbare boek-sleutel {mb.group(1)}')
                elif not os.path.isdir('gsbooks/d/'+mb.group(1)):
                    F(f'Stripe {pl["id"]}: boek-sleutel {mb.group(1)} heeft geen map')

print(f'✅ 0 fouten, {len(warn)} waarschuwingen' if not fouten else f'❌ {len(fouten)} fouten')
for x in fouten: print('  FOUT:',x)
for x in warn[:30]: print('  warn:',x)
sys.exit(1 if fouten else 0)
