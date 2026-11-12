// G2B Muziekwinkel — publieke winkelpagina (statisch, geen cookies,
// geen trackers; iframe-ready voor Beezer).
import WaveSurfer from "./vendor/wavesurfer.esm.js";

const lijst = document.getElementById("lijst");
document.getElementById("jaar").textContent = " · " + new Date().getFullYear();

const T = (typeof g2bT === "function") ? g2bT : k => k;
const LICENTIE_NAMEN = () => ({
  mp3: T("lic_mp3"), wav: T("lic_wav"), exclusief: T("lic_exclusief"),
});
let prodI18n = {};
try { prodI18n = await g2bProdI18n(); } catch { prodI18n = {}; }
const pf = (id, veld, fb) =>
  (typeof g2bProdField === "function" ? g2bProdField(prodI18n, id, veld, fb) : fb);

let huidige = null;                 // de speler die nu speelt
const spelers = new Map();          // id -> WaveSurfer
let afspeellijst = null;            // {ids:[], idx} als er wordt doorgespeeld
let statsUrl = "";                   // optioneel luister-statistiek-endpoint
let accent = "#2fbf71";              // uit products.json (winkel-accentkleur)
const luisterTijd = new Map();      // id -> seconden echt geluisterd

function stuurStats() {
  if (!statsUrl) return;
  for (const [id, sec] of luisterTijd) {
    if (sec > 0) {
      navigator.sendBeacon?.(statsUrl,
        JSON.stringify({ product: id, seconden: Math.round(sec) }));
      luisterTijd.set(id, 0);
    }
  }
}
setInterval(stuurStats, 15000);
document.addEventListener("visibilitychange", () => {
  if (document.visibilityState === "hidden") stuurStats();
});

function esc(s) {
  return String(s || "").replace(/[&<>"']/g, c =>
    ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;",
       "'": "&#39;" }[c]));
}

function fmtDuur(sec) {
  if (!sec) return "";
  const m = Math.floor(sec / 60), s = Math.round(sec % 60);
  return `${m}:${String(s).padStart(2, "0")}`;
}

async function laden() {
  let data;
  try {
    data = await fetch("products.json", { cache: "no-store" })
      .then(r => r.json());
  } catch {
    lijst.innerHTML = `<div class="leeg">${T("leeg")}</div>`;
    return;
  }
  if (data.titel)
    document.getElementById("titel").innerHTML =
      esc(data.titel).replace(/^(\S+)\s/, "$1 ").replace(
        /MUZIEKWINKEL|WINKEL/i, m => `<b>${m.toUpperCase()}</b>`);
  if (data.ondertitel)
    document.getElementById("ondertitel").textContent = data.ondertitel;
  statsUrl = data.stats_url || "";
  accent = data.accent || "#2fbf71";
  document.documentElement.style.setProperty("--accent", accent);
  document.documentElement.style.setProperty(
    "--accent2", data.accent2 || accent);
  document.title = data.titel || "G2B Winkel";

  // categorie per product — plugins opgesplitst in 3 secties:
  // INSTRUMENTEN (makers) → STUDIO-TOOLS (mix/master FX) → BUNDELS.
  const PLUGIN_CAT = {
    instrumenten: ["g2b-donderglider","g2b-donderglider-mini","g2b-akkoord",
      "g2b-melodie","g2b-keys","g2b-drums","g2b-bas","g2b-ocho",
      "g2b-song","g2b-samplex","g2bdren","drenlite","ocholite"],
    bundels: ["g2b-studio-pack","g2b-suite","g2b-fx-pack",
      "g2b-instrument-pack"],
  };
  const pluginCat = p =>
    PLUGIN_CAT.instrumenten.includes(p.id) ? "instrumenten"
    : PLUGIN_CAT.bundels.includes(p.id) ? "bundels"
    : "studiotools";
  const CAT = p => p.groep === "tools" ? "tools"
    : p.groep === "apps" ? "apps"
    : p.soort === "audio" ? "beats"
    : (p.groep === "software" || p.groep === "plugins") ? pluginCat(p)
    : (p.groep === "sounds" || p.groep === "soundpacks") ? "sounds"
    : (p.groep === "bundels" || p.soort === "bundel") ? "bundels"
    : (p.groep === "muziek" || p.groep === "tape") ? "muziek"
    : p.groep === "gratis" ? (p.soort === "digitaal" ? "apps" : "sounds")
    : "sounds";
  const CAT_NAAM = {
    apps: T("cat_apps"),
    instrumenten: T("cat_instrumenten"), studiotools: T("cat_studiotools"),
    bundels: T("cat_bundels"), sounds: T("cat_sounds"),
    muziek: T("cat_muziek"), beats: T("cat_beats"),
    tools: T("cat_tools"),
  };
  const CAT_UITLEG = {
    instrumenten: T("u_instrumenten"),
    studiotools: T("u_studiotools"),
    bundels: T("u_bundels"),
    sounds: T("u_sounds"),
    beats: T("u_beats"),
    muziek: T("u_muziek"),
    apps: T("u_apps"),
    tools: T("u_tools"),
  };
  const CAT_VOLGORDE = ["instrumenten", "studiotools", "bundels", "sounds",
    "beats", "muziek", "apps", "tools"];

  // taal-overlay: vertaalde titel/beschrijving/badge bovenop de NL-tekst
  const rawProds = [...(data.producten || [])];
  const site = prodI18n._site && prodI18n._site[g2bLang()];
  if (site && site.ondertitel)
    document.getElementById("ondertitel").textContent = site.ondertitel;
  rawProds.forEach(p => {
    p._titel = pf(p.id, "titel", p.titel);
    p._beschr = pf(p.id, "beschrijving", p.beschrijving);
    p._badge = pf(p.id, "badge", p.badge);
  });
  const prods = rawProds.filter(p => !p.verborgen)
    .sort((a, b) =>
    (CAT_VOLGORDE.indexOf(CAT(a)) - CAT_VOLGORDE.indexOf(CAT(b))) ||
    ((a.eerst ? 0 : 1) - (b.eerst ? 0 : 1)) ||
    (a.titel || "").localeCompare(b.titel || ""));
  window._prods = prods;

  // UITGELICHT: nieuwste + beste producten als horizontale rij boven de lijst
  const uitgEl = document.getElementById("uitg");
  const uitg = prods.filter(p => p.uitgelicht);
  if (uitgEl && uitg.length) {
    uitgEl.innerHTML =
      `<div class="uitg-kop">⭐ UITGELICHT — nieuw &amp; best</div><div class="uitg-rij">` +
      uitg.map(p => `<button class="uitg-card" data-uitg="${esc(p.id)}">
        ${p.cover ? `<img src="${esc(p.cover)}" alt="" loading="lazy">` : `<div class="uitg-noimg"></div>`}
        <span class="uitg-t">${esc(p._titel)}</span>
        ${p.prijs ? `<span class="uitg-p">${esc(p.prijs)}</span>` : ""}
      </button>`).join("") + `</div>`;
    uitgEl.addEventListener("click", e => {
      const knop = e.target.closest(".uitg-card");
      if (!knop) return;
      document.getElementById("zoek").value = "";
      actieveCat = "";
      document.querySelectorAll("#cats .groepbtn").forEach(b =>
        b.classList.toggle("alles", !b.dataset.cat));
      pasFilterToe();
      document.querySelector(`.card[data-id="${knop.dataset.uitg}"]`)
        ?.scrollIntoView({ behavior: "smooth", block: "center" });
    });
  }
  if (!prods.length) {
    lijst.innerHTML = `<div class="leeg">${T("binnenkort_leeg")}</div>`;
    return;
  }

  // vibe-afspeellijsten: per groep-tag een eigen afspeelknop
  const audio = prods.filter(p => p.soort === "audio" && p.preview);
  const groepen = [...new Set(
    audio.map(p => p.groep).filter(Boolean))];
  const gEl = document.getElementById("groepen");
  if (audio.length > 1 || groepen.length) {
    gEl.style.display = "flex";
    gEl.innerHTML =
      groepen.map(g =>
        `<button class="groepbtn" data-groep="${esc(g)}">▶ ${esc(g)}</button>`
      ).join("") +
      (audio.length > 1
        ? `<button class="groepbtn alles" data-groep="">
             ${T("speel_alles")}</button>` : "");
  }

  // categorie-filterknoppen (alleen categorieën die voorkomen)
  const aanwezig = CAT_VOLGORDE.filter(c =>
    prods.some(p => CAT(p) === c));
  const catsEl = document.getElementById("cats");
  if (aanwezig.length > 1) {
    catsEl.innerHTML =
      `<button class="groepbtn alles" data-cat="">${T("alles")} (${prods.length})</button>` +
      aanwezig.map(c =>
        `<button class="groepbtn" data-cat="${c}">${CAT_NAAM[c]} ` +
        `(${prods.filter(p => CAT(p) === c).length})</button>`).join("");
  }

  let vorigeCat = "";
  const sectieKop = c => {
    if (c === vorigeCat) return "";
    vorigeCat = c;
    const u = CAT_UITLEG[c] ? `<div class="u">${CAT_UITLEG[c]}</div>` : "";
    return `<div class="sectie" data-cat="${c}"><h2>${CAT_NAAM[c]}</h2>${u}</div>`;
  };
  lijst.innerHTML = prods.map(p => {
    const kop = sectieKop(CAT(p));
    const links = p.links || {};
    const licenties = ["mp3", "wav", "exclusief"]
      .filter(k => links[k]);
    const koopHref = licenties.length ? links[licenties[0]] : (links.stripe || links.koop || "");
    return `${kop}<div class="card" data-id="${esc(p.id)}" data-cat="${CAT(p)}">
      <div class="top">
        ${p.cover
          ? `<img class="cover" src="${esc(p.cover)}" alt="">`
          : ""}
        <div class="info">
          <div class="titel"><a href="p.html?id=${esc(p.id)}" style="color:inherit;text-decoration:none">${esc(p._titel)}</a>
            ${p._badge ? ` <span style="display:inline-block;background:linear-gradient(90deg,var(--accent),var(--accent2));color:#111;font-size:11px;font-weight:800;padding:2px 8px;border-radius:20px;vertical-align:middle">${esc(p._badge)}</span>` : ""}</div>
          ${p.prijs ? `<div class="prijs">${esc(p.prijs)}</div>` : ""}
        </div>
      </div>
      ${(p._beschr || p.video || p.specs) ? `
      <details class="meer">
        <summary>${p.video ? "▶ demo & info" : "ℹ info"}</summary>
        ${p._beschr ? `<div class="beschr">${esc(p._beschr)}</div>` : ""}
        ${p.specs ? `<div class="specs">${esc(p.specs)}</div>` : ""}
        ${p.video ? `
        <div class="vid-wrap">
          ${p.video_en || p.video_es ? `<button class="taal" data-taal="${esc(p.id)}"
             title="NL / EN / ES">NL</button>` : ""}
          <video class="promo" data-vid="${esc(p.id)}"
                 data-src="${esc(p.video)}" data-nl="${esc(p.video)}"
                 data-en="${esc(p.video_en || "")}"
                 data-es="${esc(p.video_es || "")}" controls playsinline
                 ${p.cover ? `poster="${esc(p.cover)}"` : ""}
                 preload="none"></video>
          <button class="vidplay" data-vidplay="${esc(p.id)}"
             aria-label="Speel video van ${esc(p._titel)}">▶</button>
        </div>` : ""}
        ${p.uitleg ? `
        <div class="vid-wrap" style="margin-top:8px">
          <span class="vid-label">▶ uitleg · EN stem · NL/ES ondertiteling</span>
          <video class="promo" controls playsinline preload="none"
                 ${p.cover ? `poster="${esc(p.cover)}"` : ""}
                 src="${esc(p.uitleg)}">
            ${p.sub_nl ? `<track kind="subtitles" srclang="nl" label="Nederlands" src="${esc(p.sub_nl)}">` : ""}
            ${p.sub_es ? `<track kind="subtitles" srclang="es" label="Español" src="${esc(p.sub_es)}">` : ""}
          </video>
        </div>` : ""}
      </details>` : ""}
      ${(p.soort === "audio" || p.preview) ? `
      <div class="player">
        <button class="playbtn" data-play="${esc(p.id)}"
          aria-label="Speel preview van ${esc(p._titel)}">▶</button>
        <div class="wave" data-wave="${esc(p.id)}"></div>
        <span class="duur">${fmtDuur(p.duur)}</span>
      </div>` : ""}
      <div class="koop-blok">
        ${licenties.map((k, i) =>
          `<button class="licentie${i === 0 ? " on" : ""}"
             data-lic="${k}" data-id="${esc(p.id)}">
             ${LICENTIE_NAMEN()[k]}${p.prijzen?.[k]
               ? ` · ${esc(p.prijzen[k])}` : ""}</button>`).join("")}
        ${koopHref
          ? `<a class="koop" data-buy="${esc(p.id)}" target="_blank"
               rel="noopener" href="${esc(koopHref)}">${p.prijs === "GRATIS" ? T("download") : T("koop")}</a>`
          : `<span class="koop uit">${T("binnenkort")}</span>`}
        ${links.browser
          ? `<a class="koop browser" href="${esc(links.browser)}"
               title="Gebruik 'm direct in je browser">🌐 browser</a>` : ""}
      </div>
      ${p.prijs && p.prijs !== "GRATIS" && p.soort !== "steun"
        ? `<div class="conditie">Directe digitale levering — bij aankoop ga je akkoord met de <a href="licentie.html">voorwaarden</a> en dat het herroepingsrecht vervalt.</div>`
        : ""}
      ${(p.upsell||[]).map(u => {
        const q = productenById(u);
        return q ? `<a class="upsell" href="#" data-uitg="${esc(u)}">💡 past bij: <b>${esc(q._titel||q.titel)}</b> — ${esc(q.prijs||"")}</a>` : "";
      }).join("")}
    </div>`;
  }).join("");
}

// licentie-keuze → koopknop wisselt
lijst.addEventListener("click", e => {
  const up = e.target.closest(".upsell");
  if (up) {
    e.preventDefault();
    document.getElementById("zoek").value = "";
    actieveCat = "";
    document.querySelectorAll("#cats .groepbtn").forEach(b =>
      b.classList.toggle("alles", !b.dataset.cat));
    pasFilterToe();
    document.querySelector(`.card[data-id="${up.dataset.uitg}"]`)
      ?.scrollIntoView({ behavior: "smooth", block: "center" });
    return;
  }
  const lic = e.target.closest(".licentie");
  if (lic) {
    const card = lic.closest(".card");
    card.querySelectorAll(".licentie").forEach(x =>
      x.classList.remove("on"));
    lic.classList.add("on");
    const p = productenById(lic.dataset.id);
    const btn = card.querySelector("[data-buy]");
    if (btn && p) btn.href = p.links[lic.dataset.lic];
    return;
  }
  const vp = e.target.closest("[data-vidplay]");
  if (vp) {
    const v = vp.closest(".vid-wrap").querySelector("video");
    const l = (typeof g2bLang === "function") ? g2bLang() : "nl";
    v.src = v.dataset[l] || v.dataset.nl;
    v.load(); v.play().catch(() => {});
    vp.remove();
    return;
  }
  const taal = e.target.closest("[data-taal]");
  if (taal) {
    const v = taal.closest(".vid-wrap").querySelector("video");
    const talen = [["nl", v.dataset.nl, "NL"],
                   ["en", v.dataset.en, "EN"],
                   ["es", v.dataset.es, "ES"]].filter(x => x[1]);
    const cur = talen.findIndex(x => v.src.endsWith(x[1]));
    const nxt = talen[(cur + 1) % talen.length];
    const t = v.currentTime;
    v.src = nxt[1]; v.currentTime = t; v.play().catch(() => {});
    taal.textContent = nxt[2];
    return;
  }
  const play = e.target.closest("[data-play]");
  if (play) { afspeellijst = null; togglePlay(play.dataset.play); }
});

// categorie-filter: toon alleen kaarten van de gekozen categorie
let actieveCat = "";
function pasFilterToe() {
  const q = (document.getElementById("zoek").value || "")
    .trim().toLowerCase();
  document.querySelectorAll("#lijst .card").forEach(el => {
    const p = productenById(el.dataset.id);
    const tekst = ((p?.titel || "") + " " + (p?._titel || "") + " " +
      (p?.beschrijving || "") + " " + (p?._beschr || "") +
      " " + (p?.groep || "") + " " + (p?.bpm || "") + " " +
      ((p?.moods || []).join(" "))).toLowerCase();
    const catOk = !actieveCat || el.dataset.cat === actieveCat;
    el.style.display = (catOk && (!q || tekst.includes(q))) ? "" : "none";
  });
  document.querySelectorAll("#lijst .sectie").forEach(el => {
    const catOk = !actieveCat || el.dataset.cat === actieveCat;
    el.style.display = (!q && catOk) ? "" : "none";
  });
}
document.getElementById("zoek").addEventListener("input", pasFilterToe);
document.getElementById("cats").addEventListener("click", e => {
  const knop = e.target.closest(".groepbtn");
  if (!knop) return;
  actieveCat = knop.dataset.cat;
  document.querySelectorAll("#cats .groepbtn").forEach(b =>
    b.classList.toggle("alles", b === knop));
  pasFilterToe();
  window.scrollTo({ top: 0, behavior: "smooth" });
});

// vibe-knoppen: speel alle previews van een groep (of alles) achter elkaar
document.getElementById("groepen").addEventListener("click", e => {
  const g = e.target.closest(".groepbtn");
  if (!g) return;
  const naam = g.dataset.groep;
  const audio = (window._prods || []).filter(
    p => p.soort === "audio" && p.preview);
  const ids = naam
    ? audio.filter(p => p.groep === naam).map(p => p.id)
    : audio.map(p => p.id);
  if (!ids.length) return;
  document.querySelectorAll(".groepbtn").forEach(b =>
    b.classList.remove("speelt"));
  g.classList.add("speelt");
  afspeellijst = { ids, idx: 0 };
  speelVolgende();
});

function speelVolgende() {
  if (!afspeellijst) return;
  const { ids, idx } = afspeellijst;
  if (idx >= ids.length) {
    afspeellijst = null;
    document.querySelectorAll(".groepbtn").forEach(b =>
      b.classList.remove("speelt"));
    return;
  }
  afspeellijst.idx++;
  const id = ids[idx];
  togglePlay(id);                 // start deze; finish → volgende
  const kaart = document.querySelector(`.card[data-id="${id}"]`);
  kaart?.scrollIntoView({ behavior: "smooth", block: "center" });
}

function productenById(id) {
  return (window._prods || []).find(p => p.id === id);
}

function togglePlay(id) {
  let ws = spelers.get(id);
  if (!ws) {
    const p = productenById(id);
    if (!p || !p.preview) return;
    ws = WaveSurfer.create({
      container: document.querySelector(`[data-wave="${id}"]`),
      height: 52, waveColor: "#3a443f", progressColor: accent,
      cursorColor: "#e8ecea", cursorWidth: 2, barWidth: 2,
      barGap: 2, barRadius: 2, normalize: true,
    });
    ws.load(p.preview);
    ws.on("finish", () => {
      knop(id, "▶");
      if (afspeellijst) speelVolgende();
    });
    spelers.set(id, ws);
    // "real view": echte luisterseconden bijhouden (optioneel verstuurd
    // naar stats_url — zie Instellingen in de beheer-app)
    let laatsteTick = 0;
    const origEmit = ws.emit.bind(ws);
    ws.emit = (ev, ...a) => {
      if (ev === "audioprocess") {
        const t = a[0];
        const d = t - laatsteTick;
        if (d > 0 && d < 3)
          luisterTijd.set(id, (luisterTijd.get(id) || 0) + d);
        laatsteTick = t;
      }
      return origEmit(ev, ...a);
    };
    ws.once("ready", () => ws.play());
  } else {
    ws.isPlaying() ? ws.pause() : ws.play();
  }
  ws.on("play", () => {
    if (huidige && huidige !== ws) { huidige.pause(); knop(huidige._id, "▶"); }
    huidige = ws; ws._id = id;
    knop(id, "⏸");
  });
  ws.on("pause", () => knop(id, "▶"));
}

function knop(id, teken) {
  const b = document.querySelector(`[data-play="${id}"]`);
  if (b) b.textContent = teken;
}

// taal-wisselaar: herbouw de hele lijst in de nieuwe taal
window.g2bRerender = () => {
  spelers.forEach(ws => { try { ws.destroy(); } catch {} });
  spelers.clear();
  huidige = null; afspeellijst = null;
  vorigeCat = "";
  laden();
};
let vorigeCat = "";

laden();
