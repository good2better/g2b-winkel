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
          "lofi","rnb","dark","winti","bubbling","slagritme","slagritme-keys",
          "808","drumkit","keys","orkest"]
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
            maxt = max(t for t,_,_,_,_ in notes)
            bars = max(1, int(maxt / (16*tpq/4) + 0.999))
            if bars > 1024:        # corrupte tijd-waarden → niet meetellen
                continue
            nfiles += 1
            s = B.setdefault(g, {"files":0,"bars":0,"drums":{}, "mel":{},
                                 "swing":[], "fills":0, "fillbars":0})
            s["files"] += 1
            s["bars"] += bars
            step_t = tpq / 4
            mel = [n for n in notes if n[1] != 9]
            allnd = (all(n[2] in DRUM_NAMES for n in notes) if notes else False) or g.startswith("slagritme")
            drm = [n for n in notes if n[1] == 9 or allnd]
            base=os.path.basename(path).rsplit('.',1)[0]
            partd=None; rpart=None; rmel=None
            import re as _re
            dirn=os.path.basename(dirp).lower()
            is_kit = dirn != g and not dirn.startswith('_') and dirp != root
            if is_kit:
                partd=s.setdefault("parts",{}).setdefault(base,{"bars":bars,"notes":{}})
                rid=_re.sub(r'\W+','-',dirn).strip('-')
                bpm_m=_re.search(r'bpm[\s_-]?(\d+)', dirn)
                rd=s.setdefault("ritmes",{}).setdefault(rid,{"bpm":int(bpm_m.group(1)) if bpm_m else 0,"parts":{},"melparts":{}})
                rpart=rd["parts"].setdefault(base,{"bars":bars,"notes":{}})
                rmel=rd["melparts"].setdefault(base,{"bars":bars,"pc":{},"grid":{},"vel":[],"dur":[],"base":[],"clus":0.0,"clusn":0})
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
                if partd is not None:
                    for _pd in (partd, rpart):
                        if _pd is None: continue
                        nd=_pd["notes"].setdefault(str(note),{"steps":{},"vel":[]})
                        nd["steps"][k]=nd["steps"].get(k,0)+1; nd["vel"].append(vel)
                # fill-detectie: snare-hits in laatste 4 stappen van een bar
                if nm in ("snare","snare2") and step >= 12:
                    s["fills"] += 1
            if drm:
                s["fillbars"] += max(1, bars)
            if rmel is not None and mel:
                # akkoord-dichtheid: gemiddelde noten per aanslag
                _ts = {}
                for t,ch,note,vel,dur in mel: _ts[t] = _ts.get(t,0)+1
                rmel["clus"] = (rmel["clus"]*rmel["clusn"] + sum(_ts.values())) / (rmel["clusn"] + len(_ts))
                rmel["clusn"] += len(_ts)
            for t,ch,note,vel,dur in mel:
                st = round(t/step_t); step = st % 16
                m = s["mel"].setdefault("g", {"deg":{}, "grid":{}, "ivl":{}})
                m["grid"][str(step)] = m["grid"].get(str(step),0) + 1
                m["deg"][str(note % 12)] = m["deg"].get(str(note % 12),0) + 1
                if rmel is not None:
                    rmel["pc"][str(note%12)] = rmel["pc"].get(str(note%12),0)+1
                    rmel["grid"][str(step)] = rmel["grid"].get(str(step),0)+1
                    rmel["vel"].append(vel); rmel["dur"].append(dur); rmel["base"].append(note)
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
    g = B.get(genre) or B.get(genre.split("#")[0]) or B.get("misc") or next(iter(B.values()))
    rnd = random.random
    notes = []
    NAME2NOTE = {v:k for k,v in DRUM_NAMES.items()}
    NAME2NOTE.update({"perc":66,"timbale":65,"conga_s":62,"bongo":60,"agogo":68,
                      "guiro":73,"cajon":76,"vibraslap":81,"pedal":44,"snare2":40})
    if g.get("parts"):
        ritmes = g.get("ritmes")
        if ritmes:
            rid = genre.split("#",1)[1] if "#" in genre else random.choice(list(ritmes))
            rid = rid if rid in ritmes else random.choice(list(ritmes))
            parts = ritmes[rid]["parts"]; rbpm = ritmes[rid].get("bpm") or bpm
            print(f"  ensemble {rid} ({len(parts)} parts, orig {rbpm}bpm)")
        else:
            parts = g["parts"]
        for part, pd in parts.items():
            for nstr, nd in pd["notes"].items():
                probs={k:v/pd["bars"] for k,v in nd["steps"].items()}
                anchors=sorted(probs,key=probs.get,reverse=True)[:2]
                for bar in range(bars):
                    for step in range(16):
                        p=probs.get(str(step),0)
                        if str(step) in anchors or rnd()<p*0.9:
                            vel=int(sum(nd["vel"])/len(nd["vel"]) + rnd()*10-5)
                            notes.append(((bar*16+step)*120,120,9,int(nstr),max(30,min(127,vel))))
        # melodische lagen van dezelfde kit (bass/keys/melodie op ch!=9)
        for part, pd in ((ritmes[rid].get("melparts") if ritmes else {}) or {}).items():
            tot=pd["bars"]
            base_n = sorted(pd["base"])[len(pd["base"])//2] if pd["base"] else 60
            dur_n = max(30,int(sum(pd["dur"])/len(pd["dur"]))) if pd["dur"] else 120
            vm = int(sum(pd["vel"])/len(pd["vel"])) if pd["vel"] else 80
            for bar in range(bars):
                for step in range(16):
                    if rnd() < (pd["grid"].get(str(step),0)/tot)*0.9:
                        kk = max(1, min(4, round(pd.get("clus",1))))
                        pcs = [int(_pick(pd["pc"], rnd)) for _ in range(kk)]
                        for pcv in set(pcs):
                            n = base_n - base_n%12 + pcv
                            notes.append(((bar*16+step)*120, dur_n, 0, max(21,min(108,n)), max(30,min(127,int(vm+rnd()*10-5)))))
        notes.sort()
        write_midi(out, notes_to_events(notes), tempo=int(60e6/bpm), name=f"{genre}_gen")
        print(f"{out}: {len(notes)} noten ({len(parts)} parts) uit brein[{genre}] @ {bpm}bpm")
        return
    core = {nm:(NAME2NOTE.get(nm, 60+abs(hash(nm))%20), 9) for nm in g["drums"]}
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
