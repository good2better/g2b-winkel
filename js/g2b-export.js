/* G2B-EXPORT-GATE — .mid/.wav/.zip downloads alleen voor kopers.
   Gebruik: <script src="/js/g2b-export.js"></script> en wrap elke download met
   if(!G2BExport.ok())return; — gratis oefenen mag, exporteren is betaald. */
const G2BExport = {
  ok() {
    try {
      var v = localStorage.getItem('g2b_aura_licentie');
      if (v === 'ok' || (v && v.length >= 4)) return true;
      var q = new URLSearchParams(location.search).get('licentie');
      if (q && q.length >= 6) { localStorage.setItem('g2b_aura_licentie', 'ok'); return true; }
    } catch (e) {}
    const code = prompt('Exporteren (.mid/.wav) is voor kopers.\nHeb je een licentiecode? Vul hem hier in — anders: koop de module op good2better.com');
    if (code && code.trim().length >= 6) {
      try { localStorage.setItem('g2b_aura_licentie', 'ok'); } catch (e) {}
      return true;
    }
    return false;
  }
};
