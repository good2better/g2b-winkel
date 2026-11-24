/* G2B licentie-slot — gedeeld door alle browser-modules.
   Demo: alles spelen/bouwen gratis. Export = kopers met licentiecode.
   Code-formaat: G2B-<SLUG>-<6 tekens>, bv. G2B-AURA-K7X2QP — hash-check
   (elke productgroep heeft eigen codes; 1/64 willekeurige codes werkt).
   Bestaande kopers: 'aura-v6' blijft werken voor de AURA-studio. */
const G2BLic = (() => {
  const h = s => { let x = 2166136261 >>> 0; for (const c of s) { x ^= c.charCodeAt(0); x = Math.imul(x, 16777619); } return x >>> 0; };
  const geldig = (slug, code) => {
    code = (code || '').trim().toUpperCase();
    if (slug === 'aura' && code === 'AURA-V6') return true;          // legacy kopers
    if (slug === 'aura' && code === 'AURA') return true;             // legacy kort
    const m = code.match(/^G2B-([A-Z]{2,10})-([A-Z0-9]{6})$/);
    // G2B-CLUB-… is de meestersleutel (club-lidmaatschap): werkt op álle modules
    return !!m && (m[1] === slug.toUpperCase() || m[1] === 'CLUB') && h(code) % 512 === 42;
  };
  const sleutel = slug => 'g2b_lic_' + slug;
  const check = slug => {
    try {
      if (localStorage.getItem(sleutel(slug)) === 'ok') return true;
      const q = new URLSearchParams(location.search).get('licentie');
      if (q && geldig(slug, q)) { localStorage.setItem(sleutel(slug), 'ok'); return true; }
      // oud-opgeslagen code opnieuw valideren (versie-upgrade tolerantie)
      const oud = localStorage.getItem('g2b_aura_licentie');
      if (oud && oud !== 'ok' && geldig(slug, oud)) { localStorage.setItem(sleutel(slug), 'ok'); return true; }
    } catch (e) {}
    return false;
  };
  const vraag = (slug, st) => {
    const code = prompt('Export is voor kopers.\nHeb je een licentiecode (G2B-' + slug.toUpperCase() + '-…)? Vul hem in — anders: koop in de winkel.');
    if (code && geldig(slug, code)) {
      try { localStorage.setItem(sleutel(slug), 'ok'); } catch (e) {}
      if (st) st.textContent = 'licentie ok — export werkt nu';
      return true;
    }
    if (code !== null && st) st.textContent = 'demo-modus: spelen gratis — export alleen voor kopers';
    return false;
  };
  return { check, vraag, geldig };
})();
