#!/usr/bin/env python3
"""brein4.json → brein_data.h (statische C++-tabellen).
Voor elke kit: ritme-parts (note, step-probs, vel) + mel-parts (pc/grid/vel/dur/base/clus).
Compact: strings→offsets, floats→u16 permille."""
import json, struct

B = json.load(open('/tmp/build/brein4.json'))

names = []        # alle part-namen + kit-ids + genres
def n_idx(s):
    if s not in names: names.append(s)
    return names.index(s)

kits = []         # (genre_idx, kit_idx, bpm, n_parts, parts_off, n_mel, mel_off)
parts = []        # (name_idx, n_notes, notes_off, bars)
pn = []           # (note, n_steps, steps_off, vel, dur)
steps = []        # u16 permille op 16-grid
mel = []          # (name_idx, bars, base, vel, dur, clus, n_pc, pc_off, n_grid, grid_off)
pcs = []          # u16 counts pitchclass 0-11 → later permille
grids = []        # u16 counts op 16
genre_names = []
gkits = []        # (n_kits, kits_off) per genre

for g in sorted(B):
    s = B[g]
    r = s.get('ritmes') or {}
    gk = []
    for rid in sorted(r):
        rd = r[rid]
        po = len(parts)
        for part in sorted(rd.get('parts', {})):
            pd = rd['parts'][part]
            no = len(pn)
            for nstr, nd in sorted(pd['notes'].items(), key=lambda x: int(x[0])):
                so = len(steps)
                tot = pd['bars']
                for st in range(16):
                    v = nd['steps'].get(str(st), 0)
                    steps.append(int(min(65535, v * 1000 / max(1, tot))))
                vm = int(sum(nd['vel']) / len(nd['vel']))
                pn.append((int(nstr), so, vm))
            parts.append((n_idx(part), no, len(pn) - no, int(pd['bars'])))
        mo = len(mel)
        for part in sorted(rd.get('melparts') or {}):
            pd = rd['melparts'][part]
            pco = len(pcs)
            for pc in range(12): pcs.append(pd['pc'].get(str(pc), 0))
            go = len(grids)
            for st in range(16): grids.append(pd['grid'].get(str(st), 0))
            bm = sorted(pd['base'])[len(pd['base']) // 2] if pd['base'] else 60
            vm = int(sum(pd['vel']) / len(pd['vel'])) if pd['vel'] else 80
            dm = max(1, int(sum(pd['dur']) / len(pd['dur']))) if pd['dur'] else 120
            mel.append((n_idx(part), int(pd['bars']), bm, vm, dm,
                        min(255, int(pd.get('clus', 1) * 10)), pco, go))
        gk.append((n_idx(rid), int(rd.get('bpm') or 0),
                   po, len(parts) - po, mo, len(mel) - mo))
    genre_names.append(g)
    gkits.append((len(kits), len(gk)))
    kits.extend(gk)

out = open('/tmp/brein_data.h', 'w')
o = out.write
o('// AUTOGEN uit brein4.json — niet handmatig editen\n#pragma once\n#include <stdint.h>\n')
o(f'static const int kNGenres={len(genre_names)};\n')
o('static const char* kGenreNames[]={' + ','.join(f'"{g}"' for g in genre_names) + '};\n')
o('static const char* kNames[]={' + ','.join('"%s"' % s.replace('"','') for s in names) + '};\n')
o('struct BKit{uint16_t name,bpm,parts_off,n_parts,mel_off,n_mel;};\n')
o('static const BKit kKits[]={' + ','.join(
    f'{{{n},{b},{po},{np},{mo},{nm}}}' for n,b,po,np,mo,nm in kits) + '};\n')
o('struct BGK{uint16_t off,n;};\n')
o('static const BGK kGenreKits[]={' + ','.join(f'{{{o_},{n}}}' for o_,n in gkits) + '};\n')
o('struct BPart{uint16_t name,notes_off,n_notes;uint8_t bars;};\n')
o('static const BPart kParts[]={' + ','.join(f'{{{n},{o_},{c},{b}}}' for n,o_,c,b in parts) + '};\n')
o('struct BPNote{uint8_t note;uint16_t steps_off;uint8_t vel;};\n')
o('static const BPNote kPNotes[]={' + ','.join(f'{{{n},{o_},{v}}}' for n,o_,v in pn) + '};\n')
o('static const uint16_t kSteps[]={' + ','.join(str(s) for s in steps) + '};\n')
o('struct BMel{uint16_t name;uint8_t bars,base,vel,clus;uint16_t dur;uint32_t pc_off,grid_off;};\n')
o('static const BMel kMels[]={' + ','.join(f'{{{n},{b},{ba},{v},{c},{d},{po},{go}}}' for n,b,ba,v,d,c,po,go in mel) + '};\n')
o('static const uint16_t kPcs[]={' + ','.join(str(s) for s in pcs) + '};\n')
o('static const uint16_t kGrids[]={' + ','.join(str(s) for s in grids) + '};\n')
out.close()
print('names', len(names), 'kits', len(kits), 'parts', len(parts),
      'pnotes', len(pn), 'mels', len(mel))
