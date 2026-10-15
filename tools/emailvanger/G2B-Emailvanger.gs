// G2B E-mailvanger — Google Apps Script (gratis, eigen Google-account)
// Zet e-mailadressen uit de winkel in een Google Sheet.
// Gebruik: deploy als Web app → uitvoeren als "ik" → toegang "iedereen" → plak de /exec URL
// in EV_SCRIPT_URL op index.html en beatmaker.html.
function doPost(e) {
  const d = JSON.parse(e.postData.contents || "{}");
  const ss = SpreadsheetApp.getActiveSpreadsheet();
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
