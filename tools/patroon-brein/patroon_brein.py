"""G2B Patroon-Brein — leert drum/akkoord/melodie-DNA uit .mid-bestanden en
genereert eindeloos NIEUWE originele patronen (geen kopieen, licentie-schoon).

Gebruik:
  python3 patroon_brein.py leer <map-met-midi> <brein.json>
  python3 patroon_brein.py gen  <brein.json> <genre> <bpm> <bars> <uit.mid>
"""
import os, sys, json, random
import mido
sys.path.insert(0, "/tmp/build")
from g2btools import notes_to_events, write_midi

GENRES = ["dembow","reggaeton","drill","boombap","trap","trap140","house","dance",
          "amapiano","kawina","kaseko","samba","cumbia","dancehall","afro","jersey",
          "lofi","rnb","dark","winti","bubbling"]
DRUM_NAMES = {36:"kick",38:"snare",40:"snare2",39:"clap",42:"hat",44:"pedal",46:"ohat",
              49:"crash",51:"ride",70:"shaker",75:"clave",63:"conga",62:"conga_s",
              66:"timbale",68:"agogo",73:"guiro",60:"bongo",76:"cajon",81:"vibraslap"}

def genre_of(path):
    low = path.lower()
    parts = low.replace("\\","/").split("/")
    for p in parts[:-1]:
        if p in GENRES: return p
    base = os.path.basename(low)
    for g in GENRES:
        if base.startswith(g) or f"_{g}_" in base or f"drums_{g}" in base: return g
    return "misc"

def read_notes(path):
    """mid → [(tick,ch,note,vel,dur)] via note_on/off pairing."""
    try: m = mido.MidiFile(path)
    except Exception: return [], 480
    tpq = m.ticks_per_beat or 480
    notes = []
    for tr in m.tracks:
        t = 0; open_n = {}
        for e in tr:
            t += e.time
            if e.type == "note_on" and e.velocity > 0:
                open_n[(e.channel, e.note)] = (t, e.velocity)
            elif e.type in ("note_off",) or (e.type == "note_on" and e.velocity == 0):
                key = (e.channel, e.note)
                if key in open_n:
                    t0, v = open_n.pop(key)
                    notes.append((t0, e.channel, e.note, v, max(1, t - t0)))
    return notes, tpq

def leer(root, out):
    B = {}  # genre -> stats
    nfiles = 0
    for dirp, _, files in os.walk(root):
        for fn in files:
            if not fn.lower().endswith((".mid", ".midi")): continue
            path = os.path.join(dirp, fn)
            g = genre_of(path)
            notes, tpq = read_notes(path)
            if not notes: continue
            nfiles += 1
            s = B.setdefault(g, {"files":0,"bars":0,"drums":{}, "mel":{},
                                 "swing":[], "fills":0, "fillbars":0})
            s["files"] += 1
            maxt = max(t for t,_,_,_,_ in notes)
            bars = max(1, round(maxt / (16*tpq/4)) + (1 if maxt % (16*tpq/4) > 0 else 0))
            bars = max(1, int(maxt / (16*tpq/4) + 0.999))
            s["bars"] += bars
            step_t = tpq / 4
            mel = [n for n in notes if n[1] != 9]
            drm = [n for n in notes if n[1] == 9]
            for t,ch,note,vel,dur in drm:
                st = round(t/step_t)
                step = st % 16; bar = int(st // 16)
                dev = (t - st*step_t)/step_t
                if abs(dev) > 0.02: s["swing"].append(round(dev,3))
                nm = DRUM_NAMES.get(note, "perc")
                d = s["drums"].setdefault(nm, {"steps":{}, "vels":{}})
                k = str(step)
                d["steps"][k] = d["steps"].get(k,0) + 1
                d["vels"].setdefault(k, []).append(vel)
                # fill-detectie: snare-hits in laatste 4 stappen van een bar
                if nm in ("snare","snare2") and step >= 12:
                    s["fills"] += 1
            if drm:
                s["fillbars"] += max(1, bars)
            for t,ch,note,vel,dur in mel:
                st = round(t/step_t); step = st % 16
                m = s["mel"].setdefault("g", {"deg":{}, "grid":{}, "ivl":{}})
                m["grid"][str(step)] = m["grid"].get(str(step),0) + 1
                m["deg"][str(note % 12)] = m["deg"].get(str(note % 12),0) + 1
            # intervallen per file
            meln = sorted([n[2] for n in mel])
            for a,b in zip(meln, meln[1:]):
                iv = b-a
                if abs(iv) <= 24:
                    ivm = s["mel"]["g"]["ivl"]
                    ivm[str(iv)] = ivm.get(str(iv),0)+1
    for g, s in B.items():
        for nm, d in s["drums"].items():
            tot = s["bars"]
            d["prob"] = {k: v/tot for k,v in d["steps"].items()}
            d["vmean"] = {k: round(sum(v)/len(v)) for k,v in d["vels"].items()}
        s["fill_rate"] = round(s["fills"]/max(1,s["fillbars"]),3)
        if s["swing"]:
            s["swing_mean"] = round(sum(s["swing"])/len(s["swing"]),4)
        s.pop("swing",None); s.pop("fills",None); s.pop("fillbars",None)
    json.dump(B, open(out,"w"), indent=1)
    print(f"geleerd uit {nfiles} midis → {len(B)} genres: {sorted(B)}")

def _pick(d, rnd):
    tot = sum(d.values()); r = rnd()*tot
    for k,v in d.items():
        r -= v
        if r <= 0: return k
    return next(iter(d)) if d else "0"

def gen(brain_path, genre, bpm, bars, out):
    B = json.load(open(brain_path))
    g = B.get(genre) or B.get("misc") or next(iter(B.values()))
    rnd = random.random
    notes = []
    core = {"kick":(36,9),"snare":(38,9),"clap":(39,9),"hat":(42,9),"ohat":(46,9),
            "shaker":(70,9),"clave":(75,9),"conga":(63,9),"crash":(49,9),"ride":(51,9)}
    for nm,(note,ch) in core.items():
        d = g["drums"].get(nm)
        if not d: continue
        probs = d["prob"]
        anchors = sorted(probs, key=probs.get, reverse=True)[:3]
        for bar in range(bars):
            for step in range(16):
                p = probs.get(str(step), 0)
                if str(step) in anchors and p >= 0.4: hit = True
                else: hit = rnd() < p*0.9
                if hit:
                    vm = d["vmean"].get(str(step), 70)
                    vel = max(30, min(127, int(vm + rnd()*16 - 8)))
                    jit = int((rnd()-0.5) * g.get("swing_mean",0)*480*2) if g.get("swing_mean") else int((rnd()-0.5)*10)
                    t = (bar*16+step)*120 + jit
                    if t >= 0: notes.append((t,120,ch,note,vel))
    # fills in laatste bar volgens fill_rate
    if rnd() < g.get("fill_rate",0.2)*bars/4:
        for i in range(4):
            notes.append(((bars*16-4+i)*120,60,9,38,75+i*10))
    notes.sort()
    write_midi(out, notes_to_events(notes), tempo=int(60e6/bpm), name=f"{genre}_gen")
    print(f"{out}: {len(notes)} noten uit brein[{genre}] @ {bpm}bpm")

if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "leer": leer(sys.argv[2], sys.argv[3])
    elif cmd == "gen": gen(sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5]), sys.argv[6])
