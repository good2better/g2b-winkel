import wave, os, re, struct, audioop, glob
SRC=os.path.expanduser("~/work/MIJN-DNA-SOUNDS-G2B-VOL1/DRUMS/kicks")
SRC2=os.path.expanduser("~/work/MIJN-DNA-SOUNDS-G2B/KICKS")
OUT="bassdrum_data.h"; OUTN="bassdrum_names.h"
# genre-bank -> file-prefix
BANKS={
 "BOOMBAP":"boombapdrum","TRAP":"trapdrum","DRILL":"drilldrum","RNB":"rnbdrum",
 "AFROBEAT":"afrobeatdrum","PHONK":"phonkdrum","LOFI":"lofidrums","FUNK":"funkdrums",
 "EPIC":"epicdrums","DROOM":"dromerigdrums","POP":"popdrums","ROCKABILLY":"rockabillydrums",
 "SOUTHERN":"southerndrums","CORPORATE":"corporatedrums","REGGAETON":"reggaetondrums",
 "TRAPLOOP":"TRAP-LOOP"}
def load(fn):
    w=wave.open(fn); n=w.getnframes(); ch=w.getnchannels(); sw=w.getsampwidth(); fr=w.getframerate()
    raw=w.readframes(n); w.close()
    if sw==2: x=raw
    elif sw==3: x=audioop.lin2lin(raw,3,2)
    else: x=audioop.lin2lin(raw,sw,2)
    if ch>1: x=audioop.tomono(x,2,0.5,0.5)
    if fr!=22050: x,_=audioop.ratecv(x,2,1,fr,22050,None)
    return struct.unpack("<%dh"%(len(x)//2),x)[:33075]
hdr=["// generated","#pragma once","#include <cstdint>",""]
names=["// generated","#pragma once",""]
meta=[]; allid=0
files=sorted(os.listdir(SRC))
if os.path.isdir(SRC2): files+=[("SRC2|"+f) for f in sorted(os.listdir(SRC2))]
for bank,pref in BANKS.items():
    fs=[f for f in files if f.startswith(pref)][:16]
    if len(fs)<8: continue
    ent=[]
    for i,f in enumerate(fs):
        path=os.path.join(SRC2,f[5:]) if f.startswith("SRC2|") else os.path.join(SRC,f)
        a=load(path); nm="kKick_%d_%d"%(allid,i)
        hdr.append("static const int16_t %s[%d]={"%(nm,len(a)))
        row=[]
        for v in a:
            row.append(str(v))
            if len(row)>=20: hdr.append(",".join(row)+",");row=[]
        if row: hdr.append(",".join(row))
        hdr.append("};")
        lbl=re.sub(r'\.wav$','',re.sub(r'^SRC2\|','',f))[:20]
        ent.append((nm,len(a),lbl))
    meta.append((bank,ent)); allid+=1
hdr.append("struct PadS{const int16_t*d;uint32_t n;};")
hdr.append("struct PadBank{const char*name;PadS p[16];};")
hdr.append("static const PadBank kPadBanks[]={")
for b,ent in meta:
    e=ent+[ent[-1]]*(16-len(ent))
    hdr.append('{"%s",{%s}},'%(b,",".join('{%s,%d}'%(s,n) for s,n,l in e)))
hdr.append("};")
hdr.append("static const int kNBanks=%d;"%len(meta))
names.append("static const char* kBankNamesUI[]={"+",".join('"%s"'%b for b,e in meta)+"};")
names.append("static const int kBankNamesUI_N=%d;"%len(meta))
for b,ent in meta:
    e=ent+[ent[-1]]*(16-len(ent))
    names.append("static const char* kPadNames_%s[16]={%s};"%(b,",".join('"%s"'%l for s,n,l in e)))
names.append("static const char** kPadNamesAll[]={"+",".join('kPadNames_%s'%b for b,e in meta)+"};")
open(OUT,"w").write("\n".join(hdr)); open(OUTN,"w").write("\n".join(names))
print("banks",len(meta))
