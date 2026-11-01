// G2B Winkel taal-systeem — NL/EN/ES zonder externe dienst.
// Gebruik: <script src="lang.js" defer></script> vóór app.js.
// API: g2bLang(), g2bSetLang(l), g2bT(k), g2bProdI18n(), g2bApplyStatic()
const G2B_I18N = {
// NL = brontaal van de site; deze dict is de eenduidige bron voor scripts.
nl: {
  sub: "Eigen beats, plugins & sound packs — direct van de maker",
  nav_beatmaker: "🎛 MAAK JE EIGEN BEAT — gratis",
  nav_radio: "📻 G2B RADIO",
  nav_library: "🎹 BEATS & PACKS",
  nav_kleding: "👕 KLEDING",
  nav_bouwer: "🧰 STEL JE BUNDEL SAMEN",
  trust1: "🔒 veilig betalen",
  trust2: "⬇ direct downloaden",
  trust3: "🤝 100% van de maker",
  zoek_ph: "🔍 Zoek in de winkel… (beat, plugin, pack, boek)",
  laden: "Winkel wordt geladen…",
  leeg: "De winkel is nog niet gevuld — kom later terug.",
  binnenkort_leeg: "Binnenkort open — de winkel wordt gevuld.",
  alles: "✨ Alles",
  speel_alles: "▶ SPEEL ALLES",
  cat_apps: "🛠️ Apps",
  cat_instrumenten: "🎹 Instrumenten",
  cat_studiotools: "🎚️ Studio-tools",
  cat_bundels: "📦 Bundels",
  cat_sounds: "🔊 Sound packs",
  cat_beats: "🎵 Beats",
  cat_muziek: "🎤 Muziek",
  u_apps: "Complete programma's voor je pc",
  u_instrumenten: "Plugins die muziek MAKEN — echte instrumenten, één knop = één geluid",
  u_studiotools: "Mix & master tools — maak je track af: luider, strakker, breder",
  u_bundels: "Meerdere plugins samen — voordeliger dan los",
  u_sounds: "Losse klanken, loops & drums om in te laden",
  u_beats: "Beats van Giorgio zelf — kies je licentie",
  u_muziek: "Uitgebrachte muziek & tapes",
  lic_mp3: "MP3-lease",
  lic_wav: "WAV-licentie",
  lic_exclusief: "Exclusief",
  koop: "KOOP NU",
  download: "DOWNLOAD",
  binnenkort: "binnenkort",
  steun_t: "💚 STEUN G2B",
  steun_x: "Vind je de winkel goed? Trakteer de maker op een koffie — elke bijdrage gaat direct naar nieuwe plugins en packs.",
  tips_t: "🎛 GRATIS TOOLS DIE WIJ AANRADEN",
  tips_x: "Goede starters naast onze plugins — 100% gratis:",
  tips_vital: "gratis synth",
  tips_lmms: "gratis DAW",
  tips_audacity: "gratis audio-editor",
  tips_partner_pre: "Eigen product of tool?",
  tips_partner: "Vraag een partnerplek aan",
  news_t: "✉ Blijf op de hoogte",
  news_x: "Nieuwe packs, plugins en beats — elke maand. Geen spam.",
  news_ph: "jouw e-mailadres",
  news_btn: "INSCHRIJVEN",
  news_mailto: "Je mailprogramma opent met een kant-en-klaar bericht — druk op versturen.",
  news_ok: "✅ Bedankt! Je staat op de G2B-lijst.",
  news_err: "Even niet gelukt — mail ons via de link hieronder.",
  news_bezig: "Bezig…",
  foot: "Gebouwd door <b>Good2Better</b> — eigen winkel, geen commissie.",
  foot_lic: "📜 Licenties & voorwaarden",
  foot_vragen: "Vragen?",
  // bestel.html
  bestel_doc_titel: "Bestelling — G2B Winkel",
  laden_order: "Even laden",
  laden_order_sub: "Je bestelling wordt klaargezet…",
  fout_t: "Product niet gevonden",
  fout_x: "Deze bestel-link is niet geldig.",
  fout_winkel: "De winkel kon niet geladen worden.",
  terug: "← terug naar de winkel",
  steun_afronden: "Steun afronden",
  bestel_afronden: "Bestelling afronden",
  sub_how: "Zo werkt het — simpel en veilig via Tikkie",
  stap1: "Klik op <b>STUUR BESTELLING</b> — je mailtje naar de winkel wordt automatisch klaargezet. Verstuur hem.",
  stap2: "Je krijgt snel antwoord met een <b>Tikkie-betaallink</b> (betaling via je eigen bank-app, veilig).",
  stap3_steun: "Na betaling sturen wij je een <b>persoonlijk bedankje</b> — jij helpt nieuwe plugins en packs maken.",
  stap3_prod: "Na betaling ontvang je <b>binnen 24 uur je download-link</b> — meestal veel sneller.",
  knop_stuur: "✉ STUUR BESTELLING",
  knop_kopieer: "📋 OF KOPIEER DE BESTELTEKST",
  klein_mail_pre: "Liever direct mailen? Stuur je bestelling naar",
  klein_mail_post: "met de productnaam erbij.",
  kopie_ok: "✓ GEKOPIEERD — plak in je mail/WhatsApp",
  lic_exclusief_tier: "EXCLUSIEVE LICENTIE",
  ord_steun: "Steun",
  ord_bestelling: "Bestelling",
  ord_bij: "bij G2B Winkel",
  ord_product: "Product:",
  ord_prijs: "Prijs:",
  ord_naam: "Mijn naam:",
  ord_email: "Mijn e-mailadres:",
  ord_tikkie: "(G2B stuurt daarna een Tikkie-betaallink.",
  ord_tikkie_steun: "Na betaling volgt een persoonlijk bedankje.)",
  ord_tikkie_prod: "Na betaling komt de download-link binnen 24 uur.)",
  ord_subj_steun: "Steun voor G2B: ",
  ord_subj_bestel: "Bestelling: ",
  // bedankt.html
  laden_dl: "Even laden",
  laden_dl_sub: "Je download wordt opgehaald…",
  dl_fout_t: "Link niet gevonden",
  dl_fout_x: "Deze download-link is niet geldig (meer) of de winkel is nog niet opnieuw gepubliceerd.",
  dl_fout_klein: "Net gekocht en toch deze melding? Mail naar de winkel via de contactgegevens op de site — je betaling is je bewijs, je krijgt je bestand.",
  bedankt_t: "Bedankt voor je aankoop!",
  bedankt_sub: "Je betaling is gelukt — hier is je download.",
  dl_klein: "Bewaar deze link — hij is jouw persoonlijke download. Werkt de knop niet? Rechtermuisknop → \"link opslaan als\".",
  dl_title_pre: "Bedankt — ",
  // gedeeld
  winkel_naam: "G2B WINKEL",
},
en: {
  sub: "Own beats, plugins & sound packs — straight from the maker",
  nav_beatmaker: "🎛 MAKE YOUR OWN BEAT — free",
  nav_radio: "📻 G2B RADIO",
  nav_library: "🎹 BEATS & PACKS",
  nav_kleding: "👕 CLOTHING",
  nav_bouwer: "🧰 BUILD YOUR BUNDLE",
  trust1: "🔒 safe payments",
  trust2: "⬇ instant download",
  trust3: "🤝 100% from the maker",
  zoek_ph: "🔍 Search the shop… (beat, plugin, pack, book)",
  laden: "Loading the shop…",
  leeg: "The shop is not filled yet — come back later.",
  binnenkort_leeg: "Opening soon — the shop is being filled.",
  alles: "✨ All",
  speel_alles: "▶ PLAY ALL",
  cat_apps: "🛠️ Apps",
  cat_instrumenten: "🎹 Instruments",
  cat_studiotools: "🎚️ Studio tools",
  cat_bundels: "📦 Bundles",
  cat_sounds: "🔊 Sound packs",
  cat_beats: "🎵 Beats",
  cat_muziek: "🎤 Music",
  u_apps: "Complete programs for your pc",
  u_instrumenten: "Plugins that MAKE music — real instruments, one knob = one sound",
  u_studiotools: "Mix & master tools — finish your track: louder, tighter, wider",
  u_bundels: "Multiple plugins together — cheaper than separate",
  u_sounds: "Single sounds, loops & drums to load",
  u_beats: "Beats by Giorgio himself — pick your license",
  u_muziek: "Released music & tapes",
  lic_mp3: "MP3 lease",
  lic_wav: "WAV license",
  lic_exclusief: "Exclusive",
  koop: "BUY NOW",
  download: "DOWNLOAD",
  binnenkort: "coming soon",
  steun_t: "💚 SUPPORT G2B",
  steun_x: "Like the shop? Buy the maker a coffee — every contribution goes straight to new plugins and packs.",
  tips_t: "🎛 FREE TOOLS WE RECOMMEND",
  tips_x: "Good starters next to our plugins — 100% free:",
  tips_vital: "free synth",
  tips_lmms: "free DAW",
  tips_audacity: "free audio editor",
  tips_partner_pre: "Your own product or tool?",
  tips_partner: "Request a partner spot",
  news_t: "✉ Stay in the loop",
  news_x: "New packs, plugins and beats — every month. No spam.",
  news_ph: "your e-mail address",
  news_btn: "SIGN UP",
  news_mailto: "Your mail app opens with a ready-made message — hit send.",
  news_ok: "✅ Thanks! You're on the G2B list.",
  news_err: "Didn't work just now — mail us via the link below.",
  news_bezig: "Working…",
  foot: "Built by <b>Good2Better</b> — own shop, no commission.",
  foot_lic: "📜 Licenses & terms",
  foot_vragen: "Questions?",
  // bestel.html
  bestel_doc_titel: "Order — G2B Winkel",
  laden_order: "One moment",
  laden_order_sub: "Your order is being prepared…",
  fout_t: "Product not found",
  fout_x: "This order link is not valid.",
  fout_winkel: "The shop could not be loaded.",
  terug: "← back to the shop",
  steun_afronden: "Finish your support",
  bestel_afronden: "Finish your order",
  sub_how: "How it works — simple and safe via Tikkie",
  stap1: "Click <b>SEND ORDER</b> — your e-mail to the shop is prepared automatically. Send it.",
  stap2: "You will quickly receive a reply with a <b>Tikkie payment link</b> (pay via your own bank app, safe).",
  stap3_steun: "After payment we send you a <b>personal thank-you</b> — you help new plugins and packs get made.",
  stap3_prod: "After payment you receive <b>your download link within 24 hours</b> — usually much faster.",
  knop_stuur: "✉ SEND ORDER",
  knop_kopieer: "📋 OR COPY THE ORDER TEXT",
  klein_mail_pre: "Prefer to mail directly? Send your order to",
  klein_mail_post: "with the product name.",
  kopie_ok: "✓ COPIED — paste in your mail/WhatsApp",
  lic_exclusief_tier: "EXCLUSIVE LICENSE",
  ord_steun: "Support",
  ord_bestelling: "Order",
  ord_bij: "at G2B Winkel",
  ord_product: "Product:",
  ord_prijs: "Price:",
  ord_naam: "My name:",
  ord_email: "My e-mail address:",
  ord_tikkie: "(G2B then sends a Tikkie payment link.",
  ord_tikkie_steun: "A personal thank-you follows after payment.)",
  ord_tikkie_prod: "The download link follows within 24 hours after payment.)",
  ord_subj_steun: "Support for G2B: ",
  ord_subj_bestel: "Order: ",
  // bedankt.html
  laden_dl: "One moment",
  laden_dl_sub: "Fetching your download…",
  dl_fout_t: "Link not found",
  dl_fout_x: "This download link is no longer valid, or the shop has not been republished yet.",
  dl_fout_klein: "Just bought it and still seeing this? Mail the shop via the contact details on the site — your payment is your proof, you will get your file.",
  bedankt_t: "Thanks for your purchase!",
  bedankt_sub: "Your payment succeeded — here is your download.",
  dl_klein: "Keep this link — it is your personal download. Button not working? Right-click → \"save link as\".",
  dl_title_pre: "Thanks — ",
  // gedeeld
  winkel_naam: "G2B WINKEL",
},
es: {
  sub: "Beats, plugins y sound packs propios — directo del creador",
  nav_beatmaker: "🎛 HAZ TU PROPIA BASE — gratis",
  nav_radio: "📻 G2B RADIO",
  nav_library: "🎹 BEATS & PACKS",
  nav_kleding: "👕 ROPA",
  nav_bouwer: "🧰 ARMA TU PAQUETE",
  trust1: "🔒 pago seguro",
  trust2: "⬇ descarga directa",
  trust3: "🤝 100% del creador",
  zoek_ph: "🔍 Buscar en la tienda… (beat, plugin, pack, libro)",
  laden: "Cargando la tienda…",
  leeg: "La tienda aún no está llena — vuelve más tarde.",
  binnenkort_leeg: "Abre pronto — la tienda se está llenando.",
  alles: "✨ Todo",
  speel_alles: "▶ REPRODUCIR TODO",
  cat_apps: "🛠️ Apps",
  cat_instrumenten: "🎹 Instrumentos",
  cat_studiotools: "🎚️ Herramientas de estudio",
  cat_bundels: "📦 Paquetes",
  cat_sounds: "🔊 Sound packs",
  cat_beats: "🎵 Beats",
  cat_muziek: "🎤 Música",
  u_apps: "Programas completos para tu pc",
  u_instrumenten: "Plugins que HACEN música — instrumentos reales, un botón = un sonido",
  u_studiotools: "Herramientas de mezcla y máster — termina tu tema: más fuerte, más limpio, más ancho",
  u_bundels: "Varios plugins juntos — más barato que por separado",
  u_sounds: "Sonidos sueltos, loops y drums para cargar",
  u_beats: "Beats del propio Giorgio — elige tu licencia",
  u_muziek: "Música y tapes publicados",
  lic_mp3: "Licencia MP3",
  lic_wav: "Licencia WAV",
  lic_exclusief: "Exclusiva",
  koop: "COMPRAR",
  download: "DESCARGAR",
  binnenkort: "próximamente",
  steun_t: "💚 APOYA A G2B",
  steun_x: "¿Te gusta la tienda? Invita al creador a un café — cada aporte va directo a nuevos plugins y packs.",
  tips_t: "🎛 HERRAMIENTAS GRATIS QUE RECOMENDAMOS",
  tips_x: "Buenos inicios junto a nuestros plugins — 100% gratis:",
  tips_vital: "synth gratis",
  tips_lmms: "DAW gratis",
  tips_audacity: "editor de audio gratis",
  tips_partner_pre: "¿Producto o herramienta propia?",
  tips_partner: "Pide un espacio de socio",
  news_t: "✉ Mantente al día",
  news_x: "Nuevos packs, plugins y beats — cada mes. Sin spam.",
  news_ph: "tu correo electrónico",
  news_btn: "SUSCRIBIRSE",
  news_mailto: "Tu correo se abre con un mensaje listo — pulsa enviar.",
  news_ok: "✅ ¡Gracias! Ya estás en la lista G2B.",
  news_err: "Ahora no funcionó — escríbenos por el enlace de abajo.",
  news_bezig: "Un momento…",
  foot: "Hecho por <b>Good2Better</b> — tienda propia, sin comisión.",
  foot_lic: "📜 Licencias y condiciones",
  foot_vragen: "¿Dudas?",
  // bestel.html
  bestel_doc_titel: "Pedido — G2B Winkel",
  laden_order: "Un momento",
  laden_order_sub: "Preparando tu pedido…",
  fout_t: "Producto no encontrado",
  fout_x: "Este enlace de pedido no es válido.",
  fout_winkel: "No se pudo cargar la tienda.",
  terug: "← volver a la tienda",
  steun_afronden: "Terminar tu apoyo",
  bestel_afronden: "Terminar tu pedido",
  sub_how: "Cómo funciona — simple y seguro vía Tikkie",
  stap1: "Pulsa <b>ENVIAR PEDIDO</b> — tu correo a la tienda se prepara solo. Envíalo.",
  stap2: "Recibirás pronto una respuesta con un <b>enlace de pago Tikkie</b> (pagas con tu app del banco, seguro).",
  stap3_steun: "Tras el pago te enviamos un <b>agradecimiento personal</b> — ayudas a crear nuevos plugins y packs.",
  stap3_prod: "Tras el pago recibes <b>tu enlace de descarga en 24 horas</b> — normalmente mucho antes.",
  knop_stuur: "✉ ENVIAR PEDIDO",
  knop_kopieer: "📋 O COPIA EL TEXTO DEL PEDIDO",
  klein_mail_pre: "¿Prefieres escribir directo? Envía tu pedido a",
  klein_mail_post: "con el nombre del producto.",
  kopie_ok: "✓ COPIADO — pégalo en tu correo/WhatsApp",
  lic_exclusief_tier: "LICENCIA EXCLUSIVA",
  ord_steun: "Apoyo",
  ord_bestelling: "Pedido",
  ord_bij: "en G2B Winkel",
  ord_product: "Producto:",
  ord_prijs: "Precio:",
  ord_naam: "Mi nombre:",
  ord_email: "Mi correo electrónico:",
  ord_tikkie: "(G2B luego envía un enlace de pago Tikkie.",
  ord_tikkie_steun: "Tras el pago sigue un agradecimiento personal.)",
  ord_tikkie_prod: "El enlace de descarga llega en 24 horas tras el pago.)",
  ord_subj_steun: "Apoyo para G2B: ",
  ord_subj_bestel: "Pedido: ",
  // bedankt.html
  laden_dl: "Un momento",
  laden_dl_sub: "Buscando tu descarga…",
  dl_fout_t: "Enlace no encontrado",
  dl_fout_x: "Este enlace de descarga ya no es válido, o la tienda aún no se ha republicado.",
  dl_fout_klein: "¿Acabas de comprarlo y ves esto? Escribe a la tienda con los datos de contacto del sitio — tu pago es tu prueba, recibirás tu archivo.",
  bedankt_t: "¡Gracias por tu compra!",
  bedankt_sub: "Tu pago salió bien — aquí está tu descarga.",
  dl_klein: "Guarda este enlace — es tu descarga personal. ¿El botón no funciona? Clic derecho → \"guardar enlace como\".",
  dl_title_pre: "Gracias — ",
  // gedeeld
  winkel_naam: "G2B WINKEL",
},
};

let _lang = null;
function g2bLang() {
  if (_lang) return _lang;
  let l = new URLSearchParams(location.search).get("lang")
       || localStorage.getItem("g2b_lang") || "";
  if (!l) {
    const nav = (navigator.language || "nl").slice(0, 2).toLowerCase();
    l = (nav === "en" || nav === "es") ? nav : "nl";
  }
  _lang = (l === "en" || l === "es") ? l : "nl";
  return _lang;
}
function g2bT(k) {
  const l = g2bLang();
  return (G2B_I18N[l] && G2B_I18N[l][k]) || (G2B_I18N.en && G2B_I18N.en[k]) || k;
}
function g2bSetLang(l) {
  _lang = (l === "en" || l === "es") ? l : "nl";
  localStorage.setItem("g2b_lang", _lang);
  try { // ?lang= in de url meenemen zodat een reload de keuze behoudt
    const u = new URL(location.href);
    if (u.searchParams.has("lang")) {
      u.searchParams.set("lang", _lang);
      history.replaceState({}, "", u);
    }
  } catch {}
  document.documentElement.lang = _lang;
  g2bApplyStatic();
  document.querySelectorAll("[data-lang]").forEach(b =>
    b.classList.toggle("aan", b.dataset.lang === _lang));
  if (window.g2bRerender) window.g2bRerender();
}
// statische teksten: data-i18n="key" (tekst), data-i18n-ph="key" (placeholder)
function g2bApplyStatic() {
  document.documentElement.lang = g2bLang();
  document.querySelectorAll("[data-i18n]").forEach(el => {
    const k = el.dataset.i18n;
    if (el.dataset.i18nHtml !== undefined && el.hasAttribute("data-i18n-html"))
      el.innerHTML = g2bT(k);
    else el.textContent = g2bT(k);
  });
  document.querySelectorAll("[data-i18n-ph]").forEach(el =>
    el.placeholder = g2bT(el.dataset.i18nPh));
}
// product-vertalingen laden (eenmaal, gedeeld tussen pagina's)
let _prodI18n = null;
async function g2bProdI18n() {
  if (_prodI18n) return _prodI18n;
  try {
    _prodI18n = await fetch("i18n.json", { cache: "no-store" }).then(r => r.json());
  } catch { _prodI18n = {}; }
  return _prodI18n;
}
function g2bProdField(i18n, id, veld, fallback) {
  const l = g2bLang();
  if (l === "nl") return fallback;
  const t = i18n[id] && i18n[id][l];
  return (t && t[veld]) || fallback;
}
window.addEventListener("DOMContentLoaded", () => {
  g2bApplyStatic();
  document.querySelectorAll("[data-lang]").forEach(b => {
    b.classList.toggle("aan", b.dataset.lang === g2bLang());
    b.addEventListener("click", () => g2bSetLang(b.dataset.lang));
  });
});
