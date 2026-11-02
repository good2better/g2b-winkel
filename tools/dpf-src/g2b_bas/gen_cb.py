import numpy as np, sys
SR=22050
NOTES=[16,18,19,22,24,26,28,30,32,33,37,40,44,47]
NAMES={0:'C',1:'Cs',2:'D',3:'Ds',4:'E',5:'F',6:'Fs',7:'G',8:'Gs',9:'A',10:'As',11:'B'}
def nm(m): return NAMES[m%12]+str(m//12-1)
def bass(f0,secs,hard,seed):
    """Schone bas: gebandlimiterde zaagtand + sub-octaaf, zachte attack, exp decay.
    hard=True -> meer overtonen + langere decay (harde-aanslag laag)."""
    rng=np.random.RandomState(seed)
    n=int(SR*secs)
    t=np.arange(n)/SR
    # kleine pitch-dip aan de start (echte snaar loopt iets in)
    f=f0*(1+0.012*np.exp(-t/0.04))
    ph=2*np.pi*np.cumsum(f)/SR
    # additive: fund + 2e..6e harmonisch met afnemende amp -> warme bas
    harms = [ (1,1.0), (2,0.45 if hard else 0.30), (3,0.18 if hard else 0.10),
              (4,0.10 if hard else 0.05), (5,0.05 if hard else 0.02) ]
    x=np.zeros(n)
    for h,a in harms: x += a*np.sin(h*ph)
    # sub-octaaf voor body
    x += 0.55*np.sin(ph*0.5)
    # zachte attack 8ms + exp decay
    atk=int(0.008*SR)
    env=np.exp(-t/(1.1 if hard else 0.85))
    env[:atk]*=np.linspace(0,1,atk)
    tail=int(0.15*n); env[-tail:]*=np.linspace(1,0,tail)**2
    x=x*env
    # round-robin variatie via seed: lichte fase/harmonische-shift
    x*=1+0.006*np.sin(2*np.pi*3.7*t+seed)
    x=x/(np.abs(x).max()+1e-9)*0.92
    return (x*32767).astype(np.int16)
def emit(arr,name,f):
    f.write(f"static const int16_t {name}[]={{")
    f.write(",".join(str(int(v)) for v in arr)); f.write("};\n")
with open(sys.argv[1],"w") as f:
    f.write("#pragma once\n#include <cstdint>\n#include <cstddef>\n")
    table=[]
    for m in NOTES:
        f0=440.0*2**((m-69)/12.0)
        secs=2.2
        row=[m]
        for lay,hard in (("",False),("_h",True)):
            for rr in range(2):
                a=bass(f0,secs,hard,seed=m*10+rr+(100 if hard else 0))
                tag=f"kCb_{nm(m)}{lay}{'_b' if rr else ''}"
                emit(a,tag,f); row += [len(a),tag]
        table.append(row)
    f.write("struct CbSmp{int midi;uint32_t len;const int16_t* d;uint32_t len2;const int16_t* d2;uint32_t lenH;const int16_t* dH;uint32_t lenH2;const int16_t* dH2;};\n")
    f.write("static const CbSmp kCb[]={"+",".join("{%d,%d,%s,%d,%s,%d,%s,%d,%s}"%(r[0],r[1],r[2],r[3],r[4],r[5],r[6],r[7],r[8]) for r in table)+"};\n")
    f.write("static const int kCbCount=%d;\n"%len(table))
print("OK")
