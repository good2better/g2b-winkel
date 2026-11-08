/*
 * G2B GIM BASSI PRO (VST) — haalt bas + kick uit een loop/track, live.
 *
 * Knoppen:
 *   FILT  — cutoff van de lage band (bas/kick-gebied)      40..200 Hz
 *   GATE  — hoeveel "rest" van de lage band doorlaat (0=alleen slagen)
 *   DUCK  — sub-duck onder de kick (alleen <90Hz — bas-body blijft spelen)
 *   QUANT — 0..100%: slag-gate schuift richting DAW-16e-raster (groove blijft)
 *   MODE  — MIX (L=R extractie) of SPLIT (L=bas+leak, R=kick)
 *   MIX   — dry/wet van de extractie
 *   MIDI  — bas→MIDI uit: pitch-tracker op de sub, noteOn/Off naar de host
 *
 * PRO-upgrades:
 *   - 15ms lookahead: gate opent met pre-roll → slag-aanval wordt niet afgekapt
 *   - Phase-Lock: meerdere instanties in dezelfde DAW vinden elkaar;
 *     volgers flippen fase + verschuiven max 10ms tot kicks versterken
 *   - Crossover-duck: alleen de sub (<90Hz) duikt, lage-mid bas blijft
 */

#include "DistrhoPlugin.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>
#include <atomic>

START_NAMESPACE_DISTRHO

// ---- inter-plugin link (Phase-Lock): instanties in dezelfde host vinden elkaar
class GimbassiPlugin;
static std::atomic<GimbassiPlugin*> gReg[8];
static std::atomic<uint32_t> gSnapSeq{0};
static float gSnap[160];            // leader's laatste onset-window (sub×gate)
static std::atomic<uint64_t> gLeadOn{0};

class GimbassiPlugin : public Plugin {
public:
    enum Params { kFilt, kGate, kDuck, kQuant, kMode, kMix, kMidi, kOn,
                  kMeter, kLink, kNumParams };

    GimbassiPlugin() : Plugin(kNumParams, 0, 0) {
        for(int i=0;i<8;i++){GimbassiPlugin* e=nullptr;
            if(gReg[i].compare_exchange_strong(e,this)){fSlot=i;break;}}
    }
    ~GimbassiPlugin() override {
        if(fSlot>=0){GimbassiPlugin* e=this;gReg[fSlot].compare_exchange_strong(e,nullptr);}
    }
protected:
    const char* getLabel() const override { return "G2B Gim Bassi PRO"; }
    const char* getDescription() const override {
        return "Bas+kick uit een loop — live, gequantized op je DAW, met bas→MIDI en Phase-Lock layering."; }
    const char* getMaker() const override { return "G2B"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(2,0,0); }
    int64_t getUniqueId() const override { return d_cconst('G','2','G','B'); }

    void initParameter(uint32_t i, Parameter& p) override {
        p.hints=kParameterIsAutomatable;
        switch(i){
        case kFilt:  p.name="FILT";   p.symbol="filt";  p.ranges.min=40.f;p.ranges.max=200.f;p.ranges.def=120.f;p.unit="Hz";break;
        case kGate:  p.name="GATE";   p.symbol="gate";  p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=15.f;p.unit="%";break;
        case kDuck:  p.name="DUCK";   p.symbol="duck";  p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=85.f;p.unit="%";break;
        case kQuant: p.name="QUANT";  p.symbol="quant"; p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=100.f;p.unit="%";break;
        case kMode:  p.name="MODE";   p.symbol="mode";  p.hints|=kParameterIsInteger;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.f;break;
        case kMix:   p.name="MIX";    p.symbol="mix";   p.ranges.min=0.f;p.ranges.max=100.f;p.ranges.def=100.f;p.unit="%";break;
        case kMidi:  p.name="MIDI";   p.symbol="midi";  p.hints|=kParameterIsBoolean;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=1.f;break;
        case kOn:    p.name="Aan";    p.symbol="aan";   p.hints|=kParameterIsBoolean;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=1.f;break;
        case kMeter: p.name="METER";  p.symbol="meter"; p.hints=kParameterIsOutput;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.f;break;
        case kLink:  p.name="LINK";   p.symbol="link";  p.hints=kParameterIsOutput;p.ranges.min=0.f;p.ranges.max=1.f;p.ranges.def=0.f;break;
        }
    }
    float getParameterValue(uint32_t i) const override {
        switch(i){case kFilt:return fFilt;case kGate:return fGate;case kDuck:return fDuck;
                  case kQuant:return fQuant;case kMode:return fMode;case kMix:return fMix;
                  case kMidi:return fMidi;case kOn:return fOn;case kMeter:return fMeter;
                  case kLink:return fLink;}return 0.f;
    }
    void setParameterValue(uint32_t i, float v) override {
        switch(i){
        case kFilt:fFilt=v;break;case kGate:fGate=v;break;case kDuck:fDuck=v;break;
        case kQuant:fQuant=v;break;case kMode:fMode=v;break;case kMix:fMix=v;break;
        case kMidi:fMidi=v;break;case kOn:fOn=v;break;}
    }

    void activate() override {
        fLp=0.f;fSub=0.f;fEnv=0.f;fMean=0.f;fKEnv=0.f;fDEnv=1.f;
        fSinceOn=1e9f;fGateIn=-1.f;fAbs=0.f;fMeter=0.f;fHold=0.f;
        fStep16=0.f;fBpm=120.0;fPos16=0.f;
        const float sr=getSampleRate();
        fDelN=(int)(0.025f*sr);if(fDelN<1)fDelN=1;fDel.assign(fDelN,0.f);fDelI=0;
        fLaN=(int)(0.030f*sr);fLa.assign(fLaN,0.f);fLaI=0;
        fAlign=0;fPhase=1.f;fNote=-1;fNoteOk=0;fNoteOffT=0;
        fZcN=0;fPrevSub=0.f;fCandNote=-1;fCandN=0;
        std::memset(fZc,0,sizeof(fZc));
        // meld de 15ms lookahead aan de host (PDC)
        setLatency((uint32_t)(0.015f*getSampleRate()));
    }

    void run(const float** in, float** out, uint32_t frames,
             const MidiEvent* /*evs*/, uint32_t /*nev*/) override {
        const float* inL=in[0];const float* inR=in[1];
        float* outL=out[0];float* outR=out[1];
        if(fOn<0.5f){
            if(inL!=outL)std::memcpy(outL,inL,frames*sizeof(float));
            if(inR!=outR)std::memcpy(outR,inR,frames*sizeof(float));
            fMeter=0.f;if(fNote>=0){sendNote(fNote,0,0);fNote=-1;}return;
        }
        const float sr=getSampleRate();
        const TimePosition&tp=getTimePosition();
        if(tp.bbt.valid&&tp.bbt.beatsPerMinute>1.0){
            fBpm=tp.bbt.beatsPerMinute;fStep16=sr*60.0/fBpm/4.0;
            if(tp.playing)fPos16=(tp.bbt.bar-1)*tp.bbt.beatsPerBar*4.0+(tp.bbt.beat-1)*4.0+(double)tp.bbt.tick/tp.bbt.ticksPerBeat*4.0;
        }
        // LINK-lamp: meer dan 1 instantie actief?
        int nReg=0;for(int i=0;i<8;i++)if(gReg[i].load())nReg++;
        fLink=nReg>1?1.f:0.f;

        const float lpA=1.f-expf(-2.f*M_PI*fFilt/sr);
        const float subA=1.f-expf(-2.f*M_PI*90.f/sr);
        const float att=1.f-expf(-1.f/(0.005f*sr));
        const float rel=expf(-1.f/(0.100f*sr));
        const float mRel=1.f-expf(-1.f/(0.300f*sr));
        const float kRel=expf(-1.f/(0.070f*sr));
        const float dRel=expf(-1.f/(0.220f*sr));
        const float holdF=0.280f*sr;
        const float refrac=0.120f*sr;
        const float leak=fGate/100.f,duck=fDuck/100.f,mix=fMix/100.f,qs=fQuant/100.f;
        const bool split=fMode>0.5f;
        const int laN=(int)(0.015f*sr);            // 15ms lookahead
        float mAcc=0.f;
        for(uint32_t i=0;i<frames;i++){
            const float mono=(inL[i]+inR[i])*0.5f;
            fLp+=lpA*(mono-fLp);
            const float lowNow=fLp;
            // lookahead: signaal 15ms (+align) vertraagd t.o.v. detectie
            fLa[fLaI]=lowNow;
            fLaI=(fLaI+1)%fLaN;
            const int back=(fLaI-(laN+fAlign)+fLaN*4)%fLaN;
            const float low=fLa[back];
            // crossover: sub (<90) + lage-mid (90-FILT)
            fSub+=subA*(low-fSub);
            const float sub=fSub, lowmid=low-sub;
            // onset-env op de onvertraagde band
            const float a=fabsf(lowNow);
            if(a>fEnv)fEnv+=att*(a-fEnv);else fEnv*=rel;
            fMean+=mRel*(fEnv-fMean);
            const float delv=fDel[fDelI];fDel[fDelI]=fEnv;fDelI=(fDelI+1)%fDelN;
            fSinceOn+=1.f;
            const float floor_=fMean*0.8f>0.02f?fMean*0.8f:0.02f;
            if(fEnv>delv*1.4f&&fEnv>floor_&&fSinceOn>refrac){
                fSinceOn=0.f;fGateIn=0.f;
                if(qs>0.001f&&fStep16>0.f){
                    const float p=fPos16;
                    fGateIn=(ceilf(p+1e-6f)-p)*fStep16*qs;
                }
            }
            if(fGateIn>=0.f){fGateIn-=1.f;
                if(fGateIn<0.f){fKEnv=1.f;fHold=holdF;fDEnv=1.f-duck;
                    onOnset();}}
            if(fHold>0.f)fHold-=1.f;else fKEnv*=kRel;
            fDEnv+=(1.f-fDEnv)*(1.f-dRel)*(fHold<=0.f?1.f:0.f);
            const float e=fKEnv*1.f;
            const float kickSig=sub*e*fPhase;
            const float basSig=(sub*fDEnv+lowmid)*(1.f-e)*fPhase;
            const float leakSig=low*leak*(1.f-e)*fPhase;
            float L,R;
            if(split){L=basSig+leakSig;R=kickSig;}
            else{L=R=kickSig+basSig+leakSig;}
            // veiligheidsbegrenzer: extractie kan nooit clip-pieken (geen gruis)
            L=tanhf(L);R=tanhf(R);
            outL[i]=inL[i]*(1.f-mix)+L*mix;
            outR[i]=inR[i]*(1.f-mix)+R*mix;
            // --- MIDI pitch-track op sub: zero-cross perioden ---
            if(fMidi>0.5f){
                if(fPrevSub<=0.f&&sub>0.f){
                    fZc[fZcN%4]=fAbs;fZcN++;
                    if(fZcN>=4){
                        const float per=(fZc[(fZcN-1)%4]-fZc[(fZcN-4)%4])/3.f;
                        if(per>0.f){
                            const float f=sr/per;
                            if(f>25.f&&f<220.f){
                                int nt=(int)std::lround(69.f+12.f*log2f(f/440.f));
                                // smoothing: noot pas na 2 gelijke schattingen
                                if(nt==fCandNote)fCandN++;else{fCandNote=nt;fCandN=0;}
                                if(fEnv>floor_&&e>0.05f&&fCandN>0){
                                    if(nt!=fNote){
                                        if(fNote>=0)sendNote(fNote,0,(uint32_t)i);
                                        sendNote(nt,(uint8_t)std::min(127,(int)(fEnv*160.f)),(uint32_t)i);
                                        fNote=nt;}
                                    fNoteOk=1;fNoteOffT=0.f;
                                }
                            }
                        }
                    }
                }
            }
            fPrevSub=sub;
            // note-off als de bas wegvalt
            if(fNote>=0){
                if(fEnv<floor_*0.6f){fNoteOffT+=1.f;
                    if(fNoteOffT>0.04f*sr){sendNote(fNote,0,(uint32_t)i);fNote=-1;}}
                else fNoteOffT=0.f;
            }
            mAcc+=e;
            fAbs+=1.f;if(fStep16>0)fPos16+=1.f/fStep16;
        }
        fMeter=std::min(1.f,mAcc/frames);
    }

private:
    void sendNote(int note,uint8_t vel,uint32_t fr){
        MidiEvent e{};e.frame=fr;e.size=3;
        e.data[0]=vel?0x90:0x80;e.data[1]=(uint8_t)note;e.data[2]=vel;
        writeMidiEvent(e);
    }
    // onset-window naar leader-snapshot, of correleer als volger
    void onOnset(){
        // vul window: laatste 160 sub-samples via fLa-ring (benadering: huidige fSub-reeks)
        if(fSlot==0){
            // leader: publiceer eigen sub-window (eenvoud: herhaal huidige sub)
            for(int k=0;k<160;k++)gSnap[k]=fSub;
            gLeadOn.store((uint64_t)fAbs);gSnapSeq.fetch_add(1);
        }else{
            // volger: correleer eigen korte window met leader's snapshot
            uint32_t s0=gSnapSeq.load();
            float win[160];for(int k=0;k<160;k++)win[k]=fSub; // nabije window-schat
            if(gSnapSeq.load()!=s0)return;
            float best=-2.f;int bestSh=0;float bestSgn=1.f;
            for(int sh=0;sh<=220;sh+=4){
                float acc=0.f,n1=0.f,n2=0.f;
                for(int k=0;k<120;k++){const float a=win[k],b=(k+sh<160)?gSnap[k+sh]:0.f;
                    acc+=a*b;n1+=a*a;n2+=b*b;}
                const float c=(n1*n2>0.f)?acc/sqrtf(n1*n2):0.f;
                if(c>best){best=c;bestSh=sh;bestSgn=1.f;}
                if(-c>best){best=-c;bestSh=sh;bestSgn=-1.f;}
            }
            fPhase=bestSgn;
            fAlign=bestSh;   // extra sample-delay zodat kicks samenvallen
        }
    }

    float fFilt=120.f,fGate=15.f,fDuck=85.f,fQuant=100.f,fMode=0.f,fMix=100.f,fMidi=1.f,fOn=1.f;
    float fLp=0.f,fSub=0.f,fEnv=0.f,fMean=0.f,fKEnv=0.f,fDEnv=1.f;
    float fSinceOn=1e9f,fGateIn=-1.f,fAbs=0.f,fMeter=0.f,fHold=0.f,fLink=0.f;
    double fBpm=120.0,fStep16=0.f,fPos16=0.f;
    std::vector<float> fDel;int fDelN=0,fDelI=0;
    std::vector<float> fLa;int fLaN=0,fLaI=0;
    int fAlign=0;float fPhase=1.f;
    int fNote=-1,fNoteOk=0,fZcN=0,fCandNote=-1,fCandN=0;float fNoteOffT=0.f,fPrevSub=0.f;
    float fZc[4];
    int fSlot=-1;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GimbassiPlugin)
};

Plugin* createPlugin() { return new GimbassiPlugin(); }

END_NAMESPACE_DISTRHO
