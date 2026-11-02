import numpy as np, sys
SR=22050
def lp(x,c):
    y=np.empty_like(x); a=1-np.exp(-2*np.pi*c/SR); s=0.0
    for i in range(len(x)): s+=a*(x[i]-s); y[i]=s
    return y
def hp(x,c): return x-lp(x,c)
def bp(x,lo,hi): return lp(hp(x,lo),hi)
def noise(n,seed): return np.random.RandomState(seed).uniform(-1,1,n)
def env_exp(n,dec,atk=0.002,tail=0.08):
    t=np.arange(n)/SR; e=np.exp(-t/dec)
    a=int(atk*SR); e[:a]*=np.linspace(0,1,a)
    tl=int(tail*n); e[-tl:]*=np.linspace(1,0,tl)**2
    return e
def sine_drop(f1,f0,n,drop_t):
    t=np.arange(n)/SR; f=f0+(f1-f0)*np.exp(-t/drop_t)
    return np.sin(2*np.pi*np.cumsum(f)/SR)
def kick(n,f1=140,f0=45,seed=1,click=0.35):
    t=np.arange(n)/SR
    x=sine_drop(f1,f0,n,0.035)*env_exp(n,0.38,0.001,0.06)
    x+=click*noise(n,seed+9)*np.exp(-t/0.003)
    return x
def snare(n,seed,body=180,noise_amt=0.7):
    t=np.arange(n)/SR
    x=0.85*np.sin(2*np.pi*body*t)*np.exp(-t/0.07)
    x+=0.25*np.sin(2*np.pi*body*1.7*t)*np.exp(-t/0.05)
    nz=bp(noise(n,seed),1500,5500)
    x+=noise_amt*nz*env_exp(n,0.10,0.001,0.12)
    return x
def hat(n,seed,dec=0.045):
    t=np.arange(n)/SR; x=np.zeros(n)
    # metallisch 808-hat: hoge fixed partialen + tik-ruis
    for h,a in [(7050,0.4),(8500,0.3),(9700,0.2),(11000,0.15),(4230,0.25),(5870,0.2)]:
        x+=a*np.sin(2*np.pi*h*t+seed*h*0.13)
    x+=0.5*hp(noise(n,seed),8000)
    return x*env_exp(n,dec,0.0008,0.15)
def clap(n,seed):
    t=np.arange(n)/SR
    nz=bp(noise(n,seed),900,4200)
    e=np.exp(-t/0.14)
    for b in (0.0,0.012,0.024): e+=np.exp(-(t-b).clip(0)/0.012)*(t>=b)
    return nz*e*0.8
def bell(n,seed,f=880):
    t=np.arange(n)/SR;x=np.zeros(n)
    for h,a in [(1,1),(2.76,0.35),(5.4,0.15),(8.9,0.06)]:
        x+=a*np.sin(2*np.pi*f*h*t+seed)*np.exp(-t/(0.35/h**0.5))
    return x
def clave(n,seed,f=1760):
    t=np.arange(n)/SR
    x=np.sin(2*np.pi*f*t)*env_exp(n,0.028,0.0005,0.15)
    return x+0.4*noise(n,seed)*np.exp(-t/0.002)
def timbale(n,seed,f=230):
    t=np.arange(n)/SR
    x=np.sin(2*np.pi*f*t)*np.exp(-t/0.28)
    x+=0.5*bp(noise(n,seed),2000,7000)*np.exp(-t/0.05)
    return x*env_exp(n,0.5,0.001,0.1)
def gong(n,seed):
    t=np.arange(n)/SR;x=np.zeros(n)
    for h,a in [(52,1),(69,0.7),(92,0.6),(126,0.4),(173,0.3),(231,0.2)]:
        x+=a*np.sin(2*np.pi*h*t+seed*h)*np.exp(-t/1.8)
    return x*0.5
def tom(n,seed,f1,f0):
    return sine_drop(f1,f0,n,0.02)*env_exp(n,0.30,0.001,0.08)+0.3*bp(noise(n,seed),800,3000)*np.exp(-np.arange(n)/SR/0.01)
def agogo(n,seed,f):
    t=np.arange(n)/SR
    return (np.sin(2*np.pi*f*t)+0.5*np.sin(2*np.pi*f*1.48*t))*env_exp(n,0.12,0.0005,0.1)
def cabasa(n,seed):
    t=np.arange(n)/SR; nz=hp(noise(n,seed),4000)
    e=np.zeros(n)
    for b in np.linspace(0,0.02,6): 
        i=int(b*SR); e[i:i+int(0.004*SR)]=np.exp(-np.arange(int(0.004*SR))/SR/0.003)
    return nz*np.convolve(np.ones(20)/20,e,'same')*np.exp(-t/0.06)
def shaker(n,seed):
    t=np.arange(n)/SR
    return hp(noise(n,seed),4500)*env_exp(n,0.07,0.008,0.15)
def crash(n,seed):
    t=np.arange(n)/SR;x=bp(noise(n,seed),3500,9000)*env_exp(n,0.9,0.001,0.15)
    for h,a in [(210,0.15),(340,0.1),(513,0.07)]: x+=a*np.sin(2*np.pi*h*t+seed)*np.exp(-t/0.8)
    return x
def conga(n,seed,f):
    t=np.arange(n)/SR
    x=sine_drop(f*1.06,f,n,0.015)*env_exp(n,0.22,0.001,0.08)
    x+=0.35*bp(noise(n,seed),1000,4500)*np.exp(-t/0.012)
    return x
def perc(n,seed,f=3100):
    return np.sin(2*np.pi*f*np.arange(n)/SR)*env_exp(n,0.05,0.0005,0.12)+0.3*hp(noise(n,seed),5000)*np.exp(-np.arange(n)/SR/0.02)
def darb(n,seed):
    t=np.arange(n)/SR
    x=sine_drop(110,68,n,0.03)*env_exp(n,0.18,0.001,0.08)
    x+=0.4*lp(noise(n,seed),900)*np.exp(-t/0.006)
    return x
DEF=[ # name, len, fn
 ('kD_kick',33075,lambda s:kick(33075,150,45,s)),('kD_snare',26460,lambda s:snare(26460,s)),
 ('kD_hat',11025,lambda s:hat(11025,s)),('kD_ohat',44100,lambda s:hat(44100,s+1,0.32)),
 ('kD_perc',14864,lambda s:perc(14864,s)),('kD_darb',15435,lambda s:darb(15435,s)),
 ('kD_clap',16300,lambda s:clap(16300,s)),('kD_bell',19845,lambda s:bell(19845,s)),
 ('kD_clave',8820,lambda s:clave(8820,s)),
 ('kD_kick2',19845,lambda s:kick(19845,170,55,s,0.5)),('kD_snare2',13230,lambda s:snare(13230,s,200,0.85)),
 ('kD_hat2',7718,lambda s:hat(7718,s,0.03)),('kTim1',45000,lambda s:timbale(45000,s,231)),
 ('kTim2',45000,lambda s:timbale(45000,s+3,196)),('kGong',176400,lambda s:gong(176400,s)),
 ('kBongH1',26000,lambda s:tom(26000,s,300,255)),('kBongH2',26000,lambda s:tom(26000,s+7,295,248)),
 ('kBongL1',22000,lambda s:tom(22000,s,190,158)),('kBongL2',22000,lambda s:tom(22000,s+7,185,152)),
 ('kAgoH',25000,lambda s:agogo(25000,s,1046)),('kAgoL',27000,lambda s:agogo(27000,s,659)),
 ('kTomH1',33000,lambda s:tom(33000,s,190,155)),('kTomH2',33000,lambda s:tom(33000,s+7,185,150)),
 ('kTomL1',33000,lambda s:tom(33000,s,140,105)),('kTomL2',33000,lambda s:tom(33000,s+7,135,100)),
 ('kCab1',16000,lambda s:cabasa(16000,s)),('kCab2',16000,lambda s:cabasa(16000,s+5)),
 ('kShk1',16000,lambda s:shaker(16000,s)),('kShk2',16000,lambda s:shaker(16000,s+5)),
 ('kCr1',66000,lambda s:crash(66000,s)),('kCr2',66000,lambda s:crash(66000,s+11)),
 ('kCj1',20000,lambda s:conga(20000,s,262)),('kCj2',20000,lambda s:conga(20000,s+3,220)),('kCj3',20000,lambda s:conga(20000,s+6,196)),
]
with open(sys.argv[1],"w") as f:
    f.write("#pragma once\n#include <cstdint>\n#include <cstddef>\n")
    for name,n,fn in DEF:
        a=fn(42); a=a/(np.abs(a).max()+1e-9)*0.92; a=(a*32767).astype(np.int16)
        assert len(a)==n
        f.write(f"static const int16_t {name}[]={{"); f.write(",".join(str(int(v)) for v in a)); f.write("};\n")
    f.write("struct DrumSmp{uint32_t len;const int16_t* d;};\n")
    f.write("static const DrumSmp kDrum[]={"+",".join("{%d,%s}"%(n,nm) for nm,n,_ in DEF)+"};\n")
    f.write("static const int kDrumCount=%d;\n"%len(DEF))
    f.write("static const char kDrumAlt[]={9,10,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,13,-1,-1,16,-1,18,-1,-1,-1,22,-1,24,-1,26,-1,28,-1,30,-1,32,-1,-1};\n")
print("OK",len(DEF))
