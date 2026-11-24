/* G2B-EXPORT-GATE — .mid/.wav/.zip downloads alleen voor kopers.
   Gebruik: <script src="/js/g2b-export.js"></script> en wrap elke download met
   if(!G2BExport.ok('slug'))return; — gratis oefenen mag, exporteren is betaald.
   Code-formaat: G2B-<SLUG>-<6 tekens> met hash-check (1/512 willekeurig raakt). */
const G2BExport = (() => {
  const h = s => { let x = 2166136261 >>> 0; for (const c of s) { x ^= c.charCodeAt(0); x = Math.imul(x, 16777619); } return x >>> 0; };
  const geldig = (slug, code) => {
    code = (code || '').trim().toUpperCase();
    if (slug === 'aura' && (code === 'AURA-V6' || code === 'AURA')) return true;
    const m = code.match(/^G2B-([A-Z]{2,10})-([A-Z0-9]{6})$/);
    // G2B-CLUB-… is de meestersleutel (club-lidmaatschap): werkt op álle modules
    return !!m && (m[1] === (slug || '').toUpperCase() || m[1] === 'CLUB') && h(code) % 512 === 42;
  };
  const sleutel = slug => 'g2b_lic_' + slug;
  return {
    geldig,
    ok(slug = 'aura') {
      try {
        if (localStorage.getItem(sleutel(slug)) === 'ok') return true;
        const q = new URLSearchParams(location.search).get('licentie');
        if (q && geldig(slug, q)) { localStorage.setItem(sleutel(slug), 'ok'); return true; }
        const oud = localStorage.getItem('g2b_aura_licentie');
        if (oud && oud !== 'ok' && geldig(slug, oud)) { localStorage.setItem(sleutel(slug), 'ok'); return true; }
      } catch (e) {}
      const code = prompt('Exporteren is voor kopers.\nHeb je een licentiecode (G2B-' + slug.toUpperCase() + '-…)? Vul hem in — anders: koop de module op good2better.com');
      if (code && geldig(slug, code)) {
        try { localStorage.setItem(sleutel(slug), 'ok'); } catch (e) {}
        return true;
      }
      return false;
    }
  };
})();
