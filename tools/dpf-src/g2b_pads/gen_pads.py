import wave, os, re, struct
ROOT=os.path.expanduser("~/work/G2B-SMAAK-SETS-EXTRA")
OUT="pads_data.h"; OUTN="pads_names.h"
# preferred pad order: drums first then bass/melodic
ORDER=["KICK_VCSL","SNARE_VCSL","KLAP_VCSL","RIM_VCSL","HAT_VCSL","OPENHAT_VCSL","CLAVE_VCSL","CONGA_VCSL","SURDO_VCSL","TIMBAL_VCSL","AGOGO","SUB808","BASS","LOGDRUM","STAB","MARIMBA","PLUCK"]
def load(fn):
    w=wave.open(fn); n=w.getnframes(); ch=w.getnchannels(); sw=w.getsampwidth(); fr=w.getframerate()
    raw=w.readframes(n); w.close()
    import audioop
    if sw==2: x=raw
    elif sw==3: x=audioop.lin2lin(raw,3,2)
    else: x=audioop.lin2lin(raw,sw,2)
    if ch>1: x=audioop.tomono(x,2,0.5,0.5)
    if fr!=22050: x,_=audioop.ratecv(x,2,1,fr,22050,None)
    a=struct.unpack("<%dh"%(len(x)//2),x)
    return a[:33075]  # <=1.5s
def key(fn):
    base=re.sub(r'^\d+-','',os.path.basename(fn))
    base=re.sub(r'\.wav$','',base)
    for i,o in enumerate(ORDER):
        if base.startswith(o): return i*10
    return 500
hdr=["// generated","#pragma once","#include <cstdint>",""]
names=["// generated","#pragma once",""]
banks=[d for d in sorted(os.listdir(ROOT)) if os.path.isdir(os.path.join(ROOT,d))]
allid=0
meta=[]
for b in banks:
    all_f=[f for f in os.listdir(os.path.join(ROOT,b)) if f.endswith('.wav')]
    fam={}
    for f in all_f:
        base=re.sub(r'^\d+-','',f)
        fm=re.split(r'_n?\d*\.wav$|_VCSL',base)[0]
        fam.setdefault(fm,[]).append(f)
    fs=[]
    for o in ORDER:
        cands=[f for f in all_f if re.sub(r'^\d+-','',f).startswith(o)]
        cands.sort(key=lambda f:abs(78-int(re.search(r'(\d+)(?=\.wav)',f).group(1))) if re.search(r'(\d+)(?=\.wav)',f) else 99)
        fs+=cands[:2]
        if len(fs)>=16: break
    fs=fs[:16]
    if len(fs)<16:
        for f in all_f:
            if f not in fs: fs.append(f)
            if len(fs)>=16: break
    ent=[]
    for i,f in enumerate(fs):
        a=load(os.path.join(ROOT,b,f))
        nm="kPad_%d_%d"%(allid,i)
        hdr.append("static const int16_t %s[%d]={"%(nm,len(a)))
        row=[]
        for j,v in enumerate(a):
            row.append(str(v))
            if len(row)>=20: hdr.append(",".join(row)+",");row=[]
        if row: hdr.append(",".join(row))
        hdr.append("};")
        label=re.sub(r'\.wav$','',re.sub(r'^\d+-','',f))[:20]
        ent.append((nm,len(a),label))
    meta.append((b,ent)); allid+=1
hdr.append("struct PadS{const int16_t*d;uint32_t n;};")
hdr.append("struct PadBank{const char*name;PadS p[16];};")
hdr.append("static const PadBank kPadBanks[]={")
for b,ent in meta:
    hdr.append('{"%s",{%s}},'%(b,",".join('{%s,%d}'%(s,n) for s,n,l in ent)))
hdr.append("};")
hdr.append("static const int kNBanks=%d;"%len(meta))
names.append("static const char* kBankNamesUI[]={"+",".join('"%s"'%b for b,e in meta)+"};")
names.append("static const int kBankNamesUI_N=%d;"%len(meta))
for b,ent in meta:
    names.append("static const char* kPadNames_%s[16]={%s};"%(b,",".join('"%s"'%l for s,n,l in ent)))
names.append("static const char** kPadNamesAll[]={"+",".join('kPadNames_%s'%b for b,e in meta)+"};")
open(OUT,"w").write("\n".join(hdr)); open(OUTN,"w").write("\n".join(names))
print("banks",len(meta),"total",sum(len(e) for b,e in meta))
