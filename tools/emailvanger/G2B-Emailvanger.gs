// G2B E-mailvanger — Google Apps Script (gratis, eigen Google-account)
// Zet e-mailadressen uit de winkel in een Google Sheet.
// Gebruik: deploy als Web app → uitvoeren als "ik" → toegang "iedereen" → plak de /exec URL
// in EV_SCRIPT_URL op index.html en beatmaker.html.
//
// Login-pogingen (bron begint met "vriend-apps-" of "club-") gaan naar een eigen tab "logins",
// zodat ze niet tussen de e-mailadressen staan.
function doPost(e) {
  const d = JSON.parse(e.postData.contents || "{}");
  const ss = SpreadsheetApp.getActiveSpreadsheet();
  const isLogin = /^vriend-apps-|^club-/.test(d.bron || "");
  if (isLogin) {
    let lg = ss.getSheetByName("logins");
    if (!lg) {
      lg = ss.insertSheet("logins");
      lg.appendRow(["code", "tijd", "resultaat", "apparaat"]);
    }
    lg.appendRow([d.email || "", d.tijd || new Date().toISOString(),
      /-ok$/.test(d.bron) ? "goed" : "fout", d.apparaat || ""]);
    return ContentService.createTextOutput(JSON.stringify({ ok: true }))
      .setMimeType(ContentService.MimeType.JSON);
  }
  let sh = ss.getSheetByName("lijst");
  if (!sh) {
    sh = ss.insertSheet("lijst");
    sh.appendRow(["email", "bron", "tijd", "extra"]);
  }
  sh.appendRow([d.email || "", d.bron || "", d.tijd || new Date().toISOString(),
    d.extra ? JSON.stringify(d.extra) : ""]);
  return ContentService.createTextOutput(JSON.stringify({ ok: true }))
    .setMimeType(ContentService.MimeType.JSON);
}
