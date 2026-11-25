/* G2B-GATE v1 — pagina-slot voor leden/privé/vriend-apps.
   Gebruik: <script src="/js/g2b-gate.js" data-tier="leden"></script>
   Tiers: mijn = alleen Giorgio · inner = Giorgio + vrienden · leden = + club-leden.
   Checkt eerst localStorage 'g2b_aura_licentie' (al ingelogd? → direct door).
   Bij goede code wordt die daar opgeslagen → volgende apps openen stil. */
(function () {
  var HASHES = {
    mijn:   ['a6027a131916280eab4a9f091cf28b36eb1dc59285023ce8c2403a76f24ff982'],
    vriend: ['70f0dc433307319601a029cee9e9e03d98a6d0678bc70a704f810c0303f36842',
             'a7e537756133cb285d3fa7c170b680623a851a203ea520d052f6c514c2e0c306'],
    club:   ['9d61007b8a32bcce2ae1c1d8b35a5d90457ec99875c1fd33122c72419a1940e7','812a199d4a4024b7a5449886a886c464e3113868277798accb8aa8e6426be87d','57907b5fa536b2a1a5c8b836b59244e2c0c49bc7e789711599d1bb9fa24530bf','3ff318d111b2245269d1e17c58cac35a4fc64d47a992191e609412da86e28194'],
    prod:   ['35d48104a2c5f22b42ed0c829df3a466e00b5aecbd77debc90b59f91b7c20916','f20a6b30af1aa2249787b0aa0d3e375f2fe57ca25dc39a6e0b5774cae0f788fb','d55765eb7b66f780e2ae34c0702ce140ed887259067229836e53a0690ec86945','f727e344ebae061cac250ec604dd2a575c8e25d02d845b10f3cb209f1cfe0619','cc05223989168da35d2d6b66cc5c92babe5873ae5a7ea445e9dc8019e97d63c6','8172b0fc783fd6647a014565def50e7fa7adde4b5d4a309380ef2b8873124512','2fced2786530c9cb9f32170bff51efe9c6a91807dda2b226f1b5ab877b716c85','c4337391b00fd456db70874573248fdb55e83f604a85518d9dc4d3c82dab5786','f891cc26f01d455607a304a807526f17ba182ad46741babee48c5578ab5e03c2']
  };
  /* CLUB = maand-code: g2b-<8 hex uit sha256(salt+JJJJMM)> — huidige + vorige maand geldig.
     Zo verloopt een gedeelde/opgezegde code vanzelf; nieuwe code staat op club-code.html. */
  var CLUB_SALT = 'g2bklub::rot::v1::';
  var tier = (document.currentScript && document.currentScript.dataset.tier) || 'inner';
  var ALLOWED = tier === 'mijn' ? HASHES.mijn
    : HASHES.mijn.concat(HASHES.vriend).concat(HASHES.club);

  function clubMonths() {
    var n = new Date(), out = [];
    for (var i = 0; i < 2; i++) {
      var m = new Date(n.getFullYear(), n.getMonth() - i, 1);
      out.push(m.getFullYear() * 100 + (m.getMonth() + 1));
    }
    return out;
  }
  async function isClubCode(v) {
    for (var ym of clubMonths()) {
      var want = 'g2b-' + (await hex(CLUB_SALT + ym)).slice(0, 8);
      if (v === want) return true;
    }
    return false;
  }

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
      if (ALLOWED.indexOf(h) >= 0 || HASHES.prod.indexOf(await hex(v.toUpperCase())) >= 0 || (tier === 'leden' && await isClubCode(v))) {
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
      if (v && v !== 'ok' && v.length >= 4)
        return ALLOWED.indexOf(await hex(v)) >= 0 || HASHES.prod.indexOf(await hex(v.toUpperCase())) >= 0 || (tier === 'leden' && await isClubCode(v));
      if (sessionStorage.getItem('g2b_gate_' + tier) === '1') return true;
    } catch (e) {}
    return false;
  }

  /* data-demo="1": pagina open als demo (geen blokkade) — kleine banner +
     code-veld rechtsboven voor wie de volledige versie heeft. Export/download
     blijft apart op slot via tools/exportslot.js. */
  var demo = document.currentScript && document.currentScript.dataset.demo === '1';
  function showDemoBar() {
    var b = document.createElement('div');
    b.style.cssText = 'position:fixed;left:0;right:0;bottom:0;z-index:99998;background:#0d1a12;border-top:1px solid #2f5b41;color:#9fc;display:flex;gap:8px;align-items:center;justify-content:center;padding:7px 12px;font:12px sans-serif;flex-wrap:wrap';
    b.innerHTML = '<span>👀 DEMO — alles werkt, alleen <b>exporteren/downloaden</b> vraagt je licentiecode</span>' +
      '<input id="g2bdemo" placeholder="code" style="padding:4px 8px;border-radius:6px;border:1px solid #2f5b41;background:#08110b;color:#fff;width:130px">' +
      '<button id="g2bdemogo" style="padding:4px 10px;border:0;border-radius:6px;background:#2fbf71;color:#04120a;font-weight:700;cursor:pointer">ontgrendel</button>';
    document.body.appendChild(b);
    b.querySelector('#g2bdemogo').onclick = async function () {
      var v = b.querySelector('#g2bdemo').value.trim().toLowerCase(); if (!v) return;
      var h = await hex(v);
      if (ALLOWED.indexOf(h) >= 0 || await isClubCode(v)) {
        try { localStorage.setItem('g2b_aura_licentie', v); } catch (e) {}
        b.innerHTML = '<span style="color:#35d07f">✓ volledige versie ontgrendeld</span>';
        return;
      }
      /* product-code van een fabriek (uit exportslot.js) → export vrij */
      var hit = window.G2B_EXPORT_WHICH ? window.G2B_EXPORT_WHICH(v) : '';
      if (hit === 'MASTER') hit = '';
      if (hit) {
        try { sessionStorage.setItem('g2b_export_' + hit, '1'); } catch (e) {}
        b.innerHTML = '<span style="color:#35d07f">✓ export ontgrendeld voor ' + hit + '</span>';
      } else { b.querySelector('#g2bdemo').value = ''; b.querySelector('#g2bdemo').placeholder = 'code klopt niet'; }
    };
  }
  function boot() {
    tryStored().then(function (ok) {
      if (ok) return;
      if (demo) { if (!sessionStorage.getItem('g2b_demo_bar')) showDemoBar(); }
      else show();
    });
  }
  if (document.body) boot(); else document.addEventListener('DOMContentLoaded', boot);
})();
