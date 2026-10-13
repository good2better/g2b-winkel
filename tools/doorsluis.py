#!/usr/bin/env python3
"""G2B doorsluis-checker: meet of gegenereerde midis bronmateriaal bevatten.
Importeerbaar: from doorsluis import load_src, check."""
import mido, sys, zipfile, io, glob, os

def bars_of(path_or_bytes):
    m = mido.MidiFile(path_or_bytes) if isinstance(path_or_bytes, str) \
        else mido.MidiFile(file=io.BytesIO(path_or_bytes))
    tpq = m.ticks_per_beat or 480
    out = {}
    for tr in m.tracks:
        t = 0
        for ev in tr:
            t += ev.time
            if ev.type == 'note_on' and ev.velocity > 0:
                st = t / (tpq / 4)
                b, s = int(st) // 16, int(round(st)) % 16
                dr = 1 if (getattr(ev, 'channel', 0) == 9 or 35 <= ev.note <= 81) else 0
                out.setdefault(b, set()).add((dr, ev.note, s))
    return list(out.values())

SRC = None
def load_src():
    global SRC
    if SRC is None:
        SRC = []
        fs = glob.glob('/tmp/midi_bulk/G2B-MIDI-SORT2/**/*.mid', recursive=True)
        for f in fs:
            for b in bars_of(f):
                if b: SRC.append(b)
    return SRC

def check(bars):
    """→ (max_overlap, aantal identieke bars). <6 onsets = geen kopieerbare groove."""
    src = load_src()
    if not bars: return 0.0, 0
    worst, ident = 0.0, 0
    for b in bars:
        if len(b) < 6: continue
        w = 0.0
        for s in src:
            ov = len(b & s) / len(b)
            if ov > w: w = ov
            if w == 1.0 and len(s) <= len(b): break
        if w > worst: worst = w
        # kopie = bijna-gelijke sets (Jaccard), niet "subset van dichtere bar"
        jac = len(b & s) / len(b | s) if (b | s) else 0
        if jac >= 0.85: ident += 1
    return worst, ident

if __name__ == '__main__':
    src = load_src()
    n_src_files = len(glob.glob('/tmp/midi_bulk/G2B-MIDI-SORT2/**/*.mid',recursive=True))
    print(f"bron: {len(src)} bars uit {n_src_files} files")
    for zp in sys.argv[1:]:
        z = zipfile.ZipFile(zp)
        nams = [n for n in z.namelist() if n.lower().endswith(('.mid', '.midi'))]
        print(f"\n=== {os.path.basename(zp)} ({len(nams)} midis)")
        scores = []
        for n in nams:
            bars = bars_of(z.read(n))
            w, ident = check(bars)
            scores.append(w)
            print(f"  {os.path.basename(n):45s} bars={len(bars):2d} max-overlap={w*100:5.1f}% identieke-bars={ident}")
        if scores:
            print(f"  -> gemiddeld {sum(scores)/len(scores)*100:.1f}% · max {max(scores)*100:.1f}%")
