// G2B actie-instellingen — één plek. Na ACTIE_EIND verdwijnen balk, klokken en bonus vanzelf.
window.G2B_ACTIE = {
  eind: "2026-11-30T23:59:00+01:00",
  bonus: { naam: "🎁 BONUS: G2B Exclusive 808 & Drum Pack (61 one-shots, ZIP)",
           url: "d/b808x7f3k9q2/G2B-EXCLUSIVE-808-DRUM-PACK.zip" },
  loopt() { return Date.now() < new Date(this.eind).getTime(); },
  rest() {
    const s = Math.max(0, Math.floor((new Date(this.eind) - Date.now()) / 1000));
    const d = Math.floor(s / 86400), u = Math.floor(s % 86400 / 3600),
          m = Math.floor(s % 3600 / 60), sec = s % 60, z = n => String(n).padStart(2, "0");
    return `${d}d ${z(u)}u ${z(m)}m ${z(sec)}s`;
  },
};
