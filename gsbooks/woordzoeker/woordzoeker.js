/* woordzoeker.js — woordzoeker-generator voor GS Books.
 * maakPuzzel(woorden, rijen, kolommen, seed) -> {raster, geplaatst:[{woord,r,c,dr,dc}]}
 * Kidvriendelijk: alleen -> (rechts), v (beneden), \ (diagonaal).
 * Deterministisch via seed (zelfde woorden+seed = zelfde puzzel = printbaar).
 */
(function (root) {
  "use strict";

  function mulberry32(seed) {
    let a = seed >>> 0;
    return function () {
      a |= 0; a = (a + 0x6D2B79F5) | 0;
      let t = Math.imul(a ^ (a >>> 15), 1 | a);
      t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }

  const RICHTINGEN = [[0, 1], [1, 0], [1, 1]];   // rechts, beneden, diagonaal
  const ABC = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  function norm(woord) {
    return woord.toUpperCase()
      .replace(/[ÉÈÊË]/g, "E").replace(/[ÀÁÂÄ]/g, "A")
      .replace(/[ÏÎ]/g, "I").replace(/[ÖÔÓ]/g, "O")
      .replace(/[ÜÛÚ]/g, "U").replace(/Ç/g, "C")
      .replace(/[^A-Z]/g, "");
  }

  function leegRaster(r, k) {
    return Array.from({ length: r }, () => Array(k).fill(""));
  }

  function past(raster, woord, r, c, dr, dc) {
    const R = raster.length, K = raster[0].length;
    const re = r + dr * (woord.length - 1), ce = c + dc * (woord.length - 1);
    if (re < 0 || re >= R || ce < 0 || ce >= K) return false;
    for (let i = 0; i < woord.length; i++) {
      const cel = raster[r + dr * i][c + dc * i];
      if (cel !== "" && cel !== woord[i]) return false;
    }
    return true;
  }

  function zet(raster, woord, r, c, dr, dc) {
    for (let i = 0; i < woord.length; i++)
      raster[r + dr * i][c + dc * i] = woord[i];
  }

  function maakPuzzel(woorden, rijen = 12, kolommen = 12, seed = 42) {
    const rand = mulberry32(seed);
    const netjes = woorden.map(norm).filter(w => w.length >= 2 && w.length <= Math.max(rijen, kolommen));
    netjes.sort((a, b) => b.length - a.length);      // langste eerst plaatsen
    const raster = leegRaster(rijen, kolommen);
    const geplaatst = [];
    const over = [];

    for (const woord of netjes) {
      let ok = false;
      for (let pog = 0; pog < 400 && !ok; pog++) {
        const [dr, dc] = RICHTINGEN[Math.floor(rand() * RICHTINGEN.length)];
        const r = Math.floor(rand() * rijen);
        const c = Math.floor(rand() * kolommen);
        if (past(raster, woord, r, c, dr, dc)) {
          zet(raster, woord, r, c, dr, dc);
          geplaatst.push({ woord, r, c, dr, dc });
          ok = true;
        }
      }
      if (!ok) over.push(woord);
    }
    for (let r = 0; r < rijen; r++)
      for (let c = 0; c < kolommen; c++)
        if (raster[r][c] === "")
          raster[r][c] = ABC[Math.floor(rand() * ABC.length)];
    return { raster, geplaatst, over, rijen, kolommen };
  }

  root.Woordzoeker = { maakPuzzel, norm };
})(typeof window !== "undefined" ? window : module.exports);
