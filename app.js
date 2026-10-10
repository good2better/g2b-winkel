// G2B Muziekwinkel — publieke winkelpagina (statisch, geen cookies,
// geen trackers; iframe-ready voor Beezer).
import WaveSurfer from "./vendor/wavesurfer.esm.js";

const lijst = document.getElementById("lijst");
document.getElementById("jaar").textContent = " · " + new Date().getFullYear();

const LICENTIE_NAMEN = {
  mp3: "MP3-lease", wav: "WAV-licentie", exclusief: "Exclusief",
};

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
    lijst.innerHTML = `<div class="leeg">De winkel is nog niet
      gevuld — kom later terug.</div>`;
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

  // categorie per product: boeken eerst (mooiste covers!), dan plugins,
  // sound-packs en als laatste de beats.
  const CAT = p => p.groep === "apps" ? "apps"
    : p.soort === "audio" ? "beats"
    : (p.groep === "software" || p.groep === "plugins") ? "plugins"
    : p.groep === "sounds" ? "sounds"
    : p.groep === "muziek" ? "muziek"
    : "boeken";
  const CAT_NAAM = {
    apps: "🛠️ Apps", boeken: "📚 Boeken", plugins: "🔌 Plugins",
    sounds: "🔊 Sound packs", muziek: "🎤 Muziek", beats: "🎵 Beats",
  };
  const CAT_VOLGORDE = ["apps", "boeken", "plugins", "sounds", "muziek", "beats"];

  // binnen Boeken: serie-volgorde 1→10 met e-boek direct achter zijn
  // kleurboek, daarna verzamel-/avonturen-bundels; onbekende titels alfabetisch.
  const BOEK_VOLGORDE = [
    "pippa", "sunny", "worteltaart", "bram", "flip", "vonk",
    "ravi", "bodhi", "ollie", "sterre", "avonturen", "verzamel",
  ];
  const boekIndex = t => {
    const s = (t || "").toLowerCase();
    const i = BOEK_VOLGORDE.findIndex(k => s.includes(k));
    return i < 0 ? BOEK_VOLGORDE.length : i;
  };
  const boekScore = p =>
    boekIndex(p.titel) * 10 + (/e-boek/i.test(p.titel) ? 1 : 0);

  const prods = [...(data.producten || [])].sort((a, b) =>
    (CAT_VOLGORDE.indexOf(CAT(a)) - CAT_VOLGORDE.indexOf(CAT(b))) ||
    (CAT(a) === "boeken" ? boekScore(a) - boekScore(b)
                         : (a.titel || "").localeCompare(b.titel || "")));
  window._prods = prods;
  if (!prods.length) {
    lijst.innerHTML = `<div class="leeg">Binnenkort open —
      de winkel wordt gevuld.</div>`;
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
             ▶ SPEEL ALLES</button>` : "");
  }

  // categorie-filterknoppen (alleen categorieën die voorkomen)
  const aanwezig = CAT_VOLGORDE.filter(c =>
    prods.some(p => CAT(p) === c));
  const catsEl = document.getElementById("cats");
  if (aanwezig.length > 1) {
    catsEl.innerHTML =
      `<button class="groepbtn alles" data-cat="">✨ Alles (${prods.length})</button>` +
      aanwezig.map(c =>
        `<button class="groepbtn" data-cat="${c}">${CAT_NAAM[c]} ` +
        `(${prods.filter(p => CAT(p) === c).length})</button>`).join("");
  }

  lijst.innerHTML = prods.map(p => {
    const links = p.links || {};
    const licenties = ["mp3", "wav", "exclusief"]
      .filter(k => links[k]);
    const koopHref = licenties.length ? links[licenties[0]] : (links.koop || "");
    return `<div class="card" data-id="${esc(p.id)}" data-cat="${CAT(p)}">
      <div class="top">
        ${p.cover
          ? `<img class="cover" src="${esc(p.cover)}" alt="">`
          : ""}
        <div class="info">
          <div class="titel">${esc(p.titel)}</div>
          ${p.prijs ? `<div class="prijs">${esc(p.prijs)}</div>` : ""}
          ${p.beschrijving
            ? `<div class="beschr">${esc(p.beschrijving)}</div>` : ""}
        </div>
      </div>
      ${p.soort === "audio" ? `
      <div class="player">
        <button class="playbtn" data-play="${esc(p.id)}">▶</button>
        <div class="wave" data-wave="${esc(p.id)}"></div>
        <span class="duur">${fmtDuur(p.duur)}</span>
      </div>` : ""}
      ${p.video ? `
      <video class="promo" src="${esc(p.video)}" controls playsinline
             preload="metadata"></video>` : ""}
      <div class="koop-blok">
        ${licenties.map((k, i) =>
          `<button class="licentie${i === 0 ? " on" : ""}"
             data-lic="${k}" data-id="${esc(p.id)}">
             ${LICENTIE_NAMEN[k]}${p.prijzen?.[k]
               ? ` · ${esc(p.prijzen[k])}` : ""}</button>`).join("")}
        ${koopHref
          ? `<a class="koop" data-buy="${esc(p.id)}" target="_blank"
               rel="noopener" href="${esc(koopHref)}">KOOP NU</a>`
          : `<span class="koop uit">binnenkort</span>`}
      </div>
    </div>`;
  }).join("");
}

// licentie-keuze → koopknop wisselt
lijst.addEventListener("click", e => {
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
  const play = e.target.closest("[data-play]");
  if (play) { afspeellijst = null; togglePlay(play.dataset.play); }
});

// categorie-filter: toon alleen kaarten van de gekozen categorie
document.getElementById("cats").addEventListener("click", e => {
  const knop = e.target.closest(".groepbtn");
  if (!knop) return;
  const cat = knop.dataset.cat;
  document.querySelectorAll("#cats .groepbtn").forEach(b =>
    b.classList.toggle("alles", b === knop));
  document.querySelectorAll("#lijst .card").forEach(card => {
    card.style.display = (!cat || card.dataset.cat === cat) ? "" : "none";
  });
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

laden();
