/* G2B AURA v6.0 — brain-memory.js — "Mijn Brein" patroonopslag.
   Bewaart patronen per app in localStorage + IndexedDB, met een werkende
   'Wis Geheugen / Reset'-knop die ALLES leegt (Wet 7). */
const BrainMemory = {
  KEY: 'g2b_aura_brain',

  load() { try { return JSON.parse(localStorage.getItem(this.KEY) || '{}'); } catch { return {}; } },
  save(map) { localStorage.setItem(this.KEY, JSON.stringify(map)); },

  remember(app, naam, data) {
    const m = this.load();
    (m[app] = m[app] || {})[naam] = { data, ts: Date.now() };
    this.save(m);
  },
  recall(app) { return this.load()[app] || {}; },

  /* Wis ALLES: localStorage + IndexedDB databases volledig leeg */
  async wisAlles() {
    localStorage.removeItem(this.KEY);
    localStorage.clear();
    try {
      if (indexedDB.databases) {
        for (const d of await indexedDB.databases()) {
          if (d.name) await new Promise(r => { const q = indexedDB.deleteDatabase(d.name);
            q.onsuccess = q.onerror = q.onblocked = () => r(); });
        }
      }
    } catch (e) {}
    return true;
  },

  /* Voeg een Wis Geheugen-knop toe aan een element */
  knop(el, onDone) {
    const b = document.createElement('button');
    b.className = 'danger'; b.textContent = '🧠 Wis Geheugen / Reset';
    b.onclick = async () => {
      if (confirm('Alles uit Mijn Brein wissen? Alle opgeslagen patronen gaan weg.')) {
        await this.wisAlles(); if (onDone) onDone();
        b.textContent = '✓ Geheugen gewist'; setTimeout(() => b.textContent = '🧠 Wis Geheugen / Reset', 1500);
      }
    };
    el.appendChild(b); return b;
  }
};
