// G2B Muziekwinkel — publieke winkelpagina (statisch, geen cookies,
// geen trackers; iframe-ready voor Beezer).
import WaveSurfer from "./vendor/wavesurfer.esm.js";

const lijst = document.getElementById("lijst");
document.getElementById("jaar").textContent = " · " + new Date().getFullYear();

const LICENTIE_NAMEN = {
  mp3: "MP3-lease", wav: "WAV-licentie", exclusief: "Exclusieve licentie",
};

let huidige = null;                 // de speler die nu speelt
const spelers = new Map();          // id -> WaveSurfer
let afspeellijst = null;            // {ids:[], idx} als er wordt doorgespeeld
let statsUrl = "";                   // optioneel luister-statistiek-endpoint
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
        /MUZIEKWINKEL/i, "<b>MUZIEKWINKEL</b>");
  if (data.ondertitel)
    document.getElementById("ondertitel").textContent = data.ondertitel;
  statsUrl = data.stats_url || "";
  document.title = data.titel || "G2B Muziekwinkel";

  const prods = data.producten || [];
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

  lijst.innerHTML = prods.map(p => {
    const links = p.links || {};
    const licenties = ["mp3", "wav", "exclusief"]
      .filter(k => links[k]);
    const koopHref = licenties.length ? links[licenties[0]] : (links.koop || "");
    return `<div class="card" data-id="${esc(p.id)}">
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
      ${p.demo ? `
      <video class="demo" src="${esc(p.demo)}" controls
             preload="metadata" playsinline></video>` : ""}
      ${p.soort === "audio" ? `
      <div class="player">
        <button class="playbtn" data-play="${esc(p.id)}">▶</button>
        <div class="wave" data-wave="${esc(p.id)}"></div>
        <span class="duur">${fmtDuur(p.duur)}</span>
      </div>` : ""}
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
      height: 52, waveColor: "#3a443f", progressColor: "#2fbf71",
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
