/* G2B-GATE v1 — pagina-slot voor leden/privé/vriend-apps.
   Gebruik: <script src="/js/g2b-gate.js" data-tier="leden"></script>
   Tiers: mijn = alleen Giorgio · inner = Giorgio + vrienden · leden = + club-leden.
   Checkt eerst localStorage 'g2b_aura_licentie' (al ingelogd? → direct door).
   Bij goede code wordt die daar opgeslagen → volgende apps openen stil. */
(function () {
  var HASHES = {
    mijn:   ['a6027a131916280eab4a9f091cf28b36eb1dc59285023ce8c2403a76f24ff982'],
    vriend: ['f0b9b68f41a25aee79b21cc1e0c1309b9c683b1d3f300d1dca427cd653c157c0',
             'a5e7193175ccdc423ce1a4a31b5856456c4e3cffb147a09a8fd9a608bbbd4a89'],
    club:   ['cff60c87705041c98f762c822d780d4a2076f2c747240a62e1f71f340fca3b34',
             '95b714bba3b80d76c800206ade5dfe20e189ef16d5313ec59d54672fa1efc7cf',
             '22985a253e7a7ca83f454c7c5e4f5ace779b007fe333c70d13a9cf4c007b22d6',
             '9233b46e6bdc591cee681f100d0e360ba9062ec04c16946d62c723bd95a7334d']
  };
  var tier = (document.currentScript && document.currentScript.dataset.tier) || 'inner';
  var ALLOWED = tier === 'mijn' ? HASHES.mijn
    : tier === 'leden' ? HASHES.mijn.concat(HASHES.vriend, HASHES.club)
    : HASHES.mijn.concat(HASHES.vriend);

  function _sha256(ascii) {
    function rr(v, a) { return (v >>> a) | (v << (32 - a)); }
    var maxWord = Math.pow(2, 32), result = '', words = [], bitLen = ascii.length * 8;
    var hash = [], k = [], primeCounter = 0, isComposite = {};
    for (var cand = 2; primeCounter < 64; cand++) {
      if (!isComposite[cand]) {
        for (var i = 0; i < 313; i += cand) isComposite[i] = cand;
        hash[primeCounter] = (Math.pow(cand, 0.5) * maxWord) | 0;
        k[primeCounter++] = (Math.pow(cand, 1 / 3) * maxWord) | 0;
      }
    }
    ascii += '\x80';
    while (ascii.length % 64 - 56) ascii += '\x00';
    for (i = 0; i < ascii.length; i++) {
      var j = ascii.charCodeAt(i);
      if (j >> 8) return;
      words[i >> 2] |= j << ((3 - i % 4) * 8);
    }
    words[words.length] = (bitLen / maxWord) | 0;
    words[words.length] = bitLen;
    for (j = 0; j < words.length;) {
      var w = words.slice(j, j += 16), oldHash = hash;
      hash = hash.slice(0, 8);
      for (i = 0; i < 64; i++) {
        var w15 = w[i - 15], w2 = w[i - 2];
        var a = hash[0], e = hash[4];
        var t1 = hash[7]
          + (rr(e, 6) ^ rr(e, 11) ^ rr(e, 25))
          + ((e & hash[5]) ^ ((~e) & hash[6]))
          + k[i]
          + (w[i] = (i < 16) ? w[i] : (
            w[i - 16]
            + (rr(w15, 7) ^ rr(w15, 18) ^ (w15 >>> 3))
            + w[i - 7]
            + (rr(w2, 17) ^ rr(w2, 19) ^ (w2 >>> 10))
          ) | 0);
        var t2 = (rr(a, 2) ^ rr(a, 13) ^ (rr(a, 22)))
          + ((a & hash[1]) ^ (a & hash[2]) ^ (hash[1] & hash[2]));
        hash = [(t1 + t2) | 0].concat(hash);
        hash[4] = (hash[4] + t1) | 0;
      }
      for (i = 0; i < 8; i++) hash[i] = (hash[i] + oldHash[i]) | 0;
    }
    for (i = 0; i < 8; i++)
      for (j = 3; j + 1; j--) {
        var b = (hash[i] >> (j * 8)) & 255;
        result += ((b < 16) ? 0 : '') + b.toString(16);
      }
    return result;
  }

  async function hex(s) {
    if (crypto.subtle && crypto.subtle.digest) {
      var b = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(s));
      return [].map.call(new Uint8Array(b), function (x) { return x.toString(16).padStart(2, '0'); }).join('');
    }
    return _sha256(s);
  }

  var CSS = '#g2bgate{position:fixed;inset:0;z-index:999999;background:#0a0f0a;display:flex;align-items:center;justify-content:center;padding:24px;font-family:Inter,system-ui,sans-serif}' +
    '#g2bgate .gb{background:#111a12;border:1px solid #1f3a24;border-radius:18px;padding:38px;width:380px;max-width:100%;text-align:center}' +
    '#g2bgate h1{font-size:1.4rem;color:#4ade80;letter-spacing:1px;margin:0}' +
    '#g2bgate p{color:#9db8a4;font-size:.85rem;margin:10px 0 18px}' +
    '#g2bgate input{width:100%;box-sizing:border-box;padding:12px;border-radius:10px;border:1px solid #1f3a24;background:#0b120c;color:#e8f5ec;font-size:15px;text-align:center;letter-spacing:2px}' +
    '#g2bgate button{width:100%;margin-top:12px;padding:13px;background:#2fbf71;color:#04120a;font-weight:800;border:0;border-radius:10px;font-size:15px;cursor:pointer}' +
    '#g2bgate .err{color:#f87171;font-size:.8rem;min-height:1.2em;margin-top:10px;display:none}' +
    '#g2bgate .terug{display:block;margin-top:14px;color:#5c7a63;font-size:.75rem}';

  function show() {
    var st = document.createElement('style'); st.textContent = CSS; document.head.appendChild(st);
    var g = document.createElement('div'); g.id = 'g2bgate';
    g.innerHTML = '<div class="gb"><h1>🔒 G2B</h1><p>' +
      (tier === 'leden' ? 'Club-app — leden gebruiken hun club-code.' : 'Privé-app — alleen voor Giorgio & vrienden.') +
      '<br>Al ingelogd ergens anders? Dan opent dit vanzelf.</p>' +
      '<input id="g2bcode" type="password" placeholder="CODE" autocomplete="off">' +
      '<button id="g2bgo">OPENEN</button><div class="err" id="g2berr">Verkeerde code</div>' +
      '<a class="terug" href="index.html">← terug naar de winkel</a></div>';
    document.body.appendChild(g);
    var inp = g.querySelector('#g2bcode'); inp.focus();
    async function unlock() {
      var v = inp.value.trim().toLowerCase(); if (!v) return;
      var h = await hex(v);
      if (ALLOWED.indexOf(h) >= 0) {
        try { localStorage.setItem('g2b_aura_licentie', v); sessionStorage.setItem('g2b_gate_' + tier, '1'); } catch (e) {}
        g.remove();
      } else { g.querySelector('#g2berr').style.display = 'block'; inp.value = ''; inp.focus(); }
    }
    g.querySelector('#g2bgo').onclick = unlock;
    inp.addEventListener('keydown', function (e) { if (e.key === 'Enter') unlock(); });
  }

  async function tryStored() {
    try {
      var v = (localStorage.getItem('g2b_aura_licentie') || '').trim().toLowerCase();
      if (v && v !== 'ok' && v.length >= 4) return ALLOWED.indexOf(await hex(v)) >= 0;
      if (sessionStorage.getItem('g2b_gate_' + tier) === '1') return true;
    } catch (e) {}
    return false;
  }

  function boot() { tryStored().then(function (ok) { if (!ok) show(); }); }
  if (document.body) boot(); else document.addEventListener('DOMContentLoaded', boot);
})();
