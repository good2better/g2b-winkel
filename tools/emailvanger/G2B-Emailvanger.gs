// G2B E-mailvanger v2 — Google Apps Script (gratis, eigen Google-account)
// Zet e-mailadressen uit de winkel in een Google Sheet.
// Gebruik: deploy als Web app → uitvoeren als "ik" → toegang "iedereen" → plak de /exec URL
// in EV_SCRIPT_URL op index.html, beatmaker.html, gratis.html, briefing.html, afmelden.html.
//
// Login-pogingen (bron begint met "vriend-apps-" of "club-") gaan naar een eigen tab "logins",
// zodat ze niet tussen de e-mailadressen staan.
//
// v2 nieuw:
//  - bron "uitschrijving" verwijdert het adres meteen uit tab "lijst" (AVG-proof, geen handwerk)
//    en logt het in tab "uitgeschreven".
//  - echte inschrijvingen (winkel/starter-pack/beatmaker) krijgen een bevestigingsmail
//    mét uitschrijf-link erin.
//  - bron "lite-<product>" = geverifieerde download: de link wordt naar het ingevulde
//    adres gemaild (nep-mail = geen download). POST-body bevat dan ook d.link.
const SITE = "https://good2better.com";
const AFMELD = SITE + "/afmelden.html";
const VAN = "g.sandvliet88@gmail.com";
const NAAM = "Good2Better";

function doPost(e) {
  const d = JSON.parse(e.postData.contents || "{}");
  const ss = SpreadsheetApp.getActiveSpreadsheet();
  const bron = d.bron || "";
  const email = (d.email || "").trim().toLowerCase();

  // 1) LOGIN-pogingen → eigen tab "logins"
  const isLogin = /^vriend-apps-|^club-/.test(bron);
  if (isLogin) {
    let lg = ss.getSheetByName("logins");
    if (!lg) {
      lg = ss.insertSheet("logins");
      lg.appendRow(["code", "tijd", "resultaat", "apparaat"]);
    }
    lg.appendRow([email, d.tijd || new Date().toISOString(),
      /-ok$/.test(bron) ? "goed" : "fout", d.apparaat || ""]);
    return ok();
  }

  // 2) UITSCHRIJVING → adres verwijderen uit "lijst" + loggen in "uitgeschreven"
  if (bron === "uitschrijving") {
    const sh = ss.getSheetByName("lijst");
    let verwijderd = 0;
    if (sh) {
      const vals = sh.getDataRange().getValues(); // rij 1 = koppen
      for (let i = vals.length - 1; i >= 1; i--) {
        if (String(vals[i][0]).trim().toLowerCase() === email) {
          sh.deleteRow(i + 1);
          verwijderd++;
        }
      }
    }
    let uit = ss.getSheetByName("uitgeschreven");
    if (!uit) {
      uit = ss.insertSheet("uitgeschreven");
      uit.appendRow(["email", "tijd"]);
    }
    uit.appendRow([email, d.tijd || new Date().toISOString()]);
    if (email && email.indexOf("@") > 0) {
      MailApp.sendEmail(email, NAAM + " — je bent uitgeschreven",
        "Hoi!\n\nJe e-mailadres is van de G2B-lijst gehaald (" + verwijderd + " inschrijving(en) verwijderd).\nJe krijgt geen mails meer van ons.\n\nBedacht je het anders? Schrijf je weer in op " + SITE + "\n\nGroet,\n" + NAAM);
    }
    return ok();
  }

  // 3) INSCHRIJVING / briefing → rij toevoegen aan "lijst"
  let sh = ss.getSheetByName("lijst");
  if (!sh) {
    sh = ss.insertSheet("lijst");
    sh.appendRow(["email", "bron", "tijd", "extra"]);
  }
  sh.appendRow([email, bron, d.tijd || new Date().toISOString(),
    d.extra ? JSON.stringify(d.extra) : ""]);

  // 5) LITE-download verificatie → link mailen (echt adres = echt lead)
  if (/^lite-/.test(bron) && email && email.indexOf("@") > 0 && d.link) {
    const prod = (d.extra && d.extra.product) ? d.extra.product : bron.replace(/^lite-/, "");
    MailApp.sendEmail(email, NAAM + " — jouw G2B downloadlink ✅",
      "Hoi!\n\nHier is je downloadlink voor " + prod + ":\n\n" + d.link +
      "\n\nVeel plezier ermee — en check de winkel voor meer: " + SITE +
      "\n\nGeen fan van mails? Uitschrijven kan altijd: " + AFMELD +
      "\n\nGroet,\n" + NAAM + "\nGood2Better High Quality Media");
  }

  // 4) Bevestigingsmail alleen bij echte nieuwsbrief-inschrijvingen
  const isSignup = (bron === "winkel" || bron === "starter-pack" || bron === "beatmaker");
  if (isSignup && email && email.indexOf("@") > 0) {
    MailApp.sendEmail(email, NAAM + " — je staat op de lijst ✅",
      "Hoi!\n\nLeuk dat je je hebt ingeschreven via " + bron + ". Je krijgt vanaf nu G2B-nieuws: nieuwe packs, plugins en beats — geen spam.\n\nBekijk de winkel: " + SITE + "\n\nJe kunt je altijd uitschrijven: " + AFMELD + "\n\nGroet,\n" + NAAM + "\nGood2Better High Quality Media");
  }
  return ok();
}

function ok() {
  return ContentService.createTextOutput(JSON.stringify({ ok: true }))
    .setMimeType(ContentService.MimeType.JSON);
}

// TEST-FUNCTIE: draai deze ÉÉN keer via de "Uitvoeren"-knop in de editor.
// Google vraagt dan om toestemming (sheet + mail versturen) — klik alles goed.
// Daarna werkt de web-app automatisch. Je vindt een test-rij in tab "lijst"
// en een test-mail in je inbox.
function testAlles() {
  const ss = SpreadsheetApp.getActiveSpreadsheet();
  let sh = ss.getSheetByName("lijst");
  if (!sh) {
    sh = ss.insertSheet("lijst");
    sh.appendRow(["email", "bron", "tijd", "extra"]);
  }
  sh.appendRow(["test@g2b", "test", new Date().toISOString(), ""]);
  MailApp.sendEmail(Session.getActiveUser().getEmail(), NAAM + " — test werkt ✅",
    "Deze test-mail bewijst dat je G2B e-mailscript werkt: sheet + mail + uitschrijven zijn klaar.");
  Logger.log("TEST OK: rij toegevoegd + mail verstuurd");
}
