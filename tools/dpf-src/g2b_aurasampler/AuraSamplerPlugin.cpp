/* G2B AURA SAMPLER — laadt de 3 AURA-sample-packs (SURI / INSTR / BASS-DRUM) en
 * maakt ze speelbaar met groen stapel-grid (meerdere sounds tegelijk), Toon/Lengte,
 * Knip <= 20% en C2/C3 & C4 root-notes uit bestandsnamen (_nNN of categorie-default).
 * Pack-root: AURA-PACKS.txt naast de plugin/exe (of env G2B_AURA_ROOT). */
#include "DistrhoPlugin.hpp"
#include "../g2b_dnasampler/dna_shared.h"
std::vector<std::string>& dna_names_list(){static std::vector<std::string> v;return v;}
#include "../g2b_std.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <set>
#include <algorithm>
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>

START_NAMESPACE_DISTRHO

struct IVoice{bool on=false;int note=0;float vel=0.f,rel=0.f,len=0.f;
    std::vector<int16_t> smp;uint32_t slen=0;float spos=0.f,sstep=0.f;};

static std::vector<std::string> g_roots(){
    std::vector<std::string> roots;
    const char* env=getenv("G2B_AURA_ROOT");
    if(env&&*env)roots.push_back(env);
    for(const char* p:{"AURA-PACKS.txt","C:/G2B/AURA-PACKS.txt","D:/G2B/AURA-PACKS.txt",
                      "C:/Program Files/Common Files/VST3/AURA-PACKS.txt"}){
        std::ifstream f(p);std::string line;
        while(std::getline(f,line)){
            if(line.size()&&line.back()=='\r')line.pop_back();
            if(line.size()&&line[0]!='#')roots.push_back(line);
        }
    }
    // eigen mappen opgeslagen via de MAP-knop in de UI
    for(const char* p:{"G2B-MAP.txt","C:/G2B/G2B-MAP.txt","D:/G2B/G2B-MAP.txt",
                      "C:/Program Files/Common Files/VST3/G2B-MAP.txt"}){
        std::ifstream f(p);std::string line;
        while(std::getline(f,line)){
            if(line.size()&&line.back()=='\r')line.pop_back();
            if(line.size()&&line[0]!='#')roots.push_back(line);
        }
    }
    // standaard bank-mappen: werkt direct zonder AURA-PACKS.txt
    for(const char* p:{"wav","AURA","../AURA","../../AURA","AURA-PACKS",
                       "C:/G2B","D:/G2B",
                       "C:/G2B/AURA","D:/G2B/AURA",
                       "C:/G2B/COMMONFILES/AURA","D:/G2B/COMMONFILES/AURA"}){
        DIR* d=opendir(p);if(d){closedir(d);roots.push_back(p);}
    }
    return roots;
}
static void save_map_path(const std::string& path){
    // schrijf naar de eerste schrijfbare G2B-map; anders naast de plugin
    const char* dirs[]={"C:/G2B","D:/G2B","."};
    for(const char* dir:dirs){
        std::string f=std::string(dir)+"/G2B-MAP.txt";
        {std::ifstream t(f);std::string l;bool dup=false;
            while(std::getline(t,l)){if(!l.empty()&&l.back()=='\r')l.pop_back();if(l==path){dup=true;break;}}
            if(dup)return;}
        std::ofstream o(f,std::ios::app);
        if(o){o<<path<<"\n";return;}
    }
}
static int name_midi(const std::string& base,int dflt){
    std::string b=base;size_t i=b.rfind(".wav");if(i!=std::string::npos)b=b.substr(0,i);
    for(size_t k=0;k+2<b.size();k++)
        if(b[k]=='_'&&b[k+1]=='n'&&isdigit((unsigned char)b[k+2])){
            int v=atoi(b.c_str()+k+2);if(v>=0&&v<=127)return v;}
    return dflt;
}
static std::string base_of(const std::string& p){
    size_t s=p.find_last_of("/\\");return s==std::string::npos?p:p.substr(s+1);
}
static void scan_dir(const std::string& dir,const std::string& root,const std::string& pack,
                     std::vector<std::string>& full,std::vector<std::string>& disp,std::vector<int>& pk){
    DIR* d=opendir(dir.c_str());if(!d)return;
    struct dirent* e;
    while((e=readdir(d))){
        std::string n=e->d_name;
        if(n=="."||n=="..")continue;
        std::string p=dir+"/"+n;
        struct stat st;
        if(stat(p.c_str(),&st)!=0)continue;
        if(S_ISDIR(st.st_mode)){scan_dir(p,root,pack+"/"+n,full,disp,pk);continue;}
        if(n.size()>4&&(n.substr(n.size()-4)==".wav"||n.substr(n.size()-4)==".WAV")){
            std::string rel=p.size()>root.size()?p.substr(root.size()):p;
            while(!rel.empty()&&(rel[0]=='/'||rel[0]=='\\'))rel.erase(0,1);
            full.push_back(p);disp.push_back(rel);
            pk.push_back(pack.find("suri")!=std::string::npos?0:(pack.find("instr")!=std::string::npos?1:(pack.find("bass")!=std::string::npos?2:0)));
        }
    }
    closedir(d);
}
static bool load_wav(const std::string& path,std::vector<int16_t>& out,int& rate){
    std::ifstream f(path,std::ios::binary);if(!f)return false;
    char hdr[12];f.read(hdr,12);
    if(f.gcount()<12||memcmp(hdr,"RIFF",4)||memcmp(hdr+8,"WAVE",4))return false;
    int ch=0,bps=0,sr=0;std::vector<char> pcm;bool isFloat=false;
    while(f){
        char id[4];uint32_t sz;f.read(id,4);if(f.gcount()<4)break;
        f.read((char*)&sz,4);if(f.gcount()<4)break;
        if(!memcmp(id,"fmt ",4)){
            char fmt[40]={0};f.read(fmt,std::min(sz,(uint32_t)40));
            uint16_t tag;memcpy(&tag,fmt,2);isFloat=(tag==3);
            memcpy(&ch,fmt+2,2);memcpy(&sr,fmt+4,4);memcpy(&bps,fmt+14,2);
            if(sz>40)f.seekg(sz-40,std::ios::cur);
        }else if(!memcmp(id,"data",4)){pcm.resize(sz);f.read(pcm.data(),sz);}
        else f.seekg(sz,std::ios::cur);
    }
    if(pcm.empty()||!ch||!sr)return false;
    rate=sr;size_t frames=pcm.size()/(bps/8)/ch;out.resize(frames);
    for(size_t i=0;i<frames;i++){
        int32_t acc=0;
        for(int c=0;c<ch;c++){
            size_t o=(i*ch+c)*(bps/8);
            if(bps==16&&!isFloat){int16_t v;memcpy(&v,pcm.data()+o,2);acc+=v;}
            else if(bps==24){int32_t v=(uint8_t)pcm[o]|((uint8_t)pcm[o+1]<<8)|((int8_t)pcm[o+2]<<16);acc+=v>>8;}
            else if(bps==32&&isFloat){float v;memcpy(&v,pcm.data()+o,4);acc+=(int32_t)(v*30000.f);}
            else if(bps==8){acc+=((int)(uint8_t)pcm[o]-128)<<8;}
        }
        acc/=ch;if(acc>32767)acc=32767;if(acc<-32768)acc=-32768;
        out[i]=(int16_t)acc;
    }
    return !out.empty();
}

class AuraPlugin:public Plugin{
public:
    AuraPlugin():Plugin(kNumParams,0,2){scan();}
protected:
    const char* getLabel()const override{return "G2B AURA Sampler";}
    const char* getMaker()const override{return "Good2Better";}
    const char* getLicense()const override{return "G2B";}
    uint32_t getVersion()const override{return d_version(6,0,0);}
    int64_t getUniqueId()const override{return d_cconst('G','2','A','S');}
    enum{kPack=0,kSnd,kKracht,kLevel,kOn,kToon,kLengte,kKnip,kDice,kTgl,kWis,kGalaxy,kSfeer,kNumParams};
    void initParameter(uint32_t i,Parameter& p)override{
        p.ranges.def=0.f;p.hints=kParameterIsAutomatable;
        switch(i){
        case kPack:p.name="PACK";p.symbol="pack";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=2;break;
        case kSnd:p.name="SOUND";p.symbol="snd";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=127;break;
        case kKracht:p.name="KRACHT";p.symbol="kracht";p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.8f;break;
        case kLevel:p.name="OUTPUT";p.symbol="level";p.ranges.min=-24;p.ranges.max=12;p.ranges.def=0;p.unit="dB";break;
        case kOn:p.name="Aan";p.symbol="aan";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;break;
        case kToon:p.name="TOON";p.symbol="toon";p.ranges.min=-12;p.ranges.max=12;p.ranges.def=0;p.unit="st";break;
        case kLengte:p.name="LENGTE";p.symbol="lengte";p.ranges.min=0.1f;p.ranges.max=4.f;p.ranges.def=1.f;break;
        case kKnip:p.name="KNIP";p.symbol="knip";p.ranges.min=0;p.ranges.max=0.2f;p.ranges.def=0;break; // hard ≤20%
        case kDice:p.name="DICE";p.symbol="dice";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;break;
        case kTgl:p.name="CEL";p.symbol="tgl";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=512;break;
        case kWis:p.name="WIS";p.symbol="wis";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;break;
        case kGalaxy:p.name="DIMENTIES";p.symbol="dim";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=26;p.ranges.def=9;break;
        case kSfeer:p.name="Sfeer";p.symbol="sfeer";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=2;p.ranges.def=1;break;
        }
    }
    float getParameterValue(uint32_t i)const override{
        switch(i){case kPack:return fPack;case kSnd:return fSnd;case kKracht:return fKracht;
            case kLevel:return fLevel;case kOn:return fOn;case kToon:return fToon;
            case kLengte:return fLengte;case kKnip:return fKnip;case kDice:return fDice;
            case kTgl:return fTgl;case kWis:return fWis;case kGalaxy:return fGalaxyV;
            case kSfeer:return fSfeerV;}return 0.f;
    }
    void setParameterValue(uint32_t i,float v)override{
        switch(i){
        case kPack:fPack=v;buildIdx();break;
        case kSnd:fSnd=v;loadSnd((int)v);break;
        case kKracht:fKracht=v;break;case kLevel:fLevel=v;break;case kOn:fOn=v;break;
        case kToon:fToon=v;break;case kLengte:fLengte=v;break;
        case kKnip:fKnip=v>0.2f?0.2f:v;break; // nooit boven 20%
        case kDice:fDice=v;if(v>0.5f&&!fIdx.empty()){fRnd=fRnd*1664525u+1013904223u;
            int nv=fIdx[(fRnd>>9)%fIdx.size()];loadSnd(nv);fSnd=(float)nv;}fDice=0.f;break;
        case kTgl:if(v>0){int idx=(int)v-1;toggle(idx);}fTgl=v;break;
        case kWis:if(v>0.5f)fSel.clear();fWis=v;break;
        case kGalaxy:fGalaxyV=v;fGal.setPos((int)(v+0.5f));break;
        case kSfeer:fSfeerV=v;fSf.setMode((int)(v+0.5f));break;
        }
    }
    void initState(uint32_t index,String& name,String& def)override{
        if(index==0){name="scope";def="";}
        else if(index==1){name="sel";std::string st=selState();def=st.c_str();}
        else{name="instrnames";def="";
            int lim=std::min<int>(512,(int)fDisp.size());
            for(int i=0;i<lim;i++){def+=fDisp[i].c_str();if(i<lim-1)def+=";";}}
    }
    void setState(const char* key,const char* value)override{
        if(!std::strcmp(key,"sel")){fSel.clear();const char*p=value;
            while(*p){char*e=nullptr;long v=strtol(p,&e,10);if(e==p)break;
                if(v>=0)fSel.insert((int)v);p=(*e==';')?e+1:e;}}
        else if(!std::strcmp(key,"map")){
            std::string path=value?value:"";
            while(!path.empty()&&(path.back()=='/'||path.back()=='\\'||path.back()==' '))path.pop_back();
            DIR* d=opendir(path.c_str());
            if(!d){setState("mapmsg","map niet gevonden");return;}
            closedir(d);
            save_map_path(path);scan();
            fNamesSent=false;fCur=-1;fSel.clear();
            std::string m="ok — ";m+=std::to_string((int)fDisp.size());m+=" wavs totaal";
            setState("mapmsg",m.c_str());
        }
    }
    void run(const float**,float** out,uint32_t frames,const MidiEvent* ev,uint32_t evc)override{
        const float sr=getSampleRate();
        memset(out[0],0,frames*sizeof(float));memset(out[1],0,frames*sizeof(float));
        if(fOn<0.5f)return;
        const float outG=powf(10.f,fLevel*0.05f);
        const float toonMul=powf(2.f,fToon/12.f);
        uint32_t pos=0;
        for(uint32_t e=0;e<=evc;e++){
            uint32_t t=(e<evc)?ev[e].frame:frames;
            while(pos<t){
                float s=0;
                for(auto& v:fV)if(v.on){
                    v.len+=1.f/sr;float w=0;
                    if(v.slen){
                        uint32_t i=(uint32_t)v.spos;
                        if(i>=v.slen-1||v.len>=fLengte){v.on=false;continue;}
                        float fr=v.spos-i;
                        w=(v.smp[i]+fr*(v.smp[i+1]-v.smp[i]))/32768.f;
                        v.spos+=v.sstep*toonMul;
                    }
                    float env=1.f;
                    if(v.rel>0.f){env=v.rel;v.rel-=1.f/(sr*0.35f);if(v.rel<=0.f){v.on=false;continue;}}
                    s+=w*env*v.vel;
                }
                // Knip ≤20%: gate-tremolo op 8Hz, diepte = knip*5 (max 100%)
                if(fKnip>0.001f){fKnipPh+=8.f/sr;float sq=(fKnipPh-(int)fKnipPh)<0.5f?0.f:1.f;
                    s*=1.f-fKnip*5.f*(1.f-sq)*0.5f;}
                s*=outG*0.45f;
                if(s>0.85f)s=0.85f+(s-0.85f)*0.25f;else if(s<-0.85f)s=-0.85f+(s+0.85f)*0.25f;
                out[0][pos]=s;out[1][pos]=s;
                if(++fDecim>=3){fDecim=0;if(fScopeN<40)fScope[fScopeN++]=s;}
                ++pos;
            }
            if(e<evc){
                const MidiEvent& m=ev[e];
                if(m.size>=3&&(m.data[0]&0xF0)==0x90&&m.data[2]>0){
                    // stapel: alle geselecteerde sounds spelen tegelijk (of huidige als niets gekozen)
                    if(fStackBufs.empty())ensureStack();
                    for(size_t b=0;b<fStackBufs.size();b++){
                        bool ok=false;
                        for(auto& v:fV)if(!v.on){ok=true;
                            v.on=true;v.note=m.data[1];v.rel=0.f;v.len=0.f;
                            v.vel=(m.data[2]/127.f)*(0.4f+0.6f*fKracht);
                            v.smp=fStackBufs[b];v.slen=(uint32_t)v.smp.size();v.spos=0.f;
                            float f0=440.f*powf(2.f,(m.data[1]-69)/12.f),fs=440.f*powf(2.f,(fStackSrc[b]-69)/12.f);
                            v.sstep=(f0/fs)*((float)fStackRates[b]/sr);
                            break;}
                        if(!ok)break;
                    }
                }else if(m.size>=3&&((m.data[0]&0xF0)==0x80||((m.data[0]&0xF0)==0x90&&m.data[2]==0))){
                    for(auto& v:fV)if(v.on&&v.note==m.data[1]&&v.rel==0.f)v.rel=1.f;
                }
            }
        }
        if(!fNamesSent){
            fNamesSent=true;
            std::string list;int lim=std::min<int>(512,(int)fDisp.size());
            for(int i=0;i<lim;i++){list+=fDisp[i];if(i<lim-1)list+=";";}
            setState("instrnames",list.c_str());
            setState("sel",selState().c_str());
        }
        if(fScopeN>=40){
            char buf[256];int p=0;
            for(uint32_t s2=0;s2<fScopeN&&p<230;++s2)p+=snprintf(buf+p,sizeof(buf)-p,"%.3f;",fScope[s2]);
            buf[p]=0;setState("scope",buf);fScopeN=0;
        }
        g2b_post(nullptr,out,frames,sr,fOn,100.f,fGal,&fSf,false);
    }
private:
    std::string selState(){std::string s;for(int i:fSel){s+=std::to_string(i)+";";}return s;}
    void scan(){
        fFull.clear();fDisp.clear();fPackOf.clear();
        for(const std::string& r:g_roots())scan_dir(r,r,"",fFull,fDisp,fPackOf);
        std::vector<size_t> ix(fDisp.size());
        for(size_t i=0;i<ix.size();i++)ix[i]=i;
        std::sort(ix.begin(),ix.end(),[&](size_t a,size_t b){return fDisp[a]<fDisp[b];});
        std::vector<std::string> a(fDisp),b(fFull);std::vector<int> c(fPackOf);
        for(size_t i=0;i<ix.size();i++){fDisp[i]=a[ix[i]];fFull[i]=b[ix[i]];fPackOf[i]=c[ix[i]];}
        {std::set<std::string> seen;size_t w=0;
         for(size_t i=0;i<fFull.size();i++)if(seen.insert(fFull[i]).second){
            fFull[w]=fFull[i];fDisp[w]=fDisp[i];fPackOf[w]=fPackOf[i];w++;}
         fFull.resize(w);fDisp.resize(w);fPackOf.resize(w);}
        if(fDisp.empty()){fDisp.push_back("(geen wavs — zie LEESMIJ)");fFull.push_back("");fPackOf.push_back(0);}
        buildIdx();
        dna_names_list()=fDisp;
        if(fCur<0){for(size_t i=0;i<fFull.size();i++)if(!fFull[i].empty()){loadSnd((int)i);break;}}
    }
    void buildIdx(){
        fIdx.clear();
        for(int i=0;i<(int)fPackOf.size();i++)if(fPackOf[i]==(int)(fPack+0.5f))fIdx.push_back(i);
        if(fIdx.empty())for(int i=0;i<(int)fPackOf.size();i++)fIdx.push_back(i);
        fStackBufs.clear();
    }
    int rootOf(int idx){
        // root uit _nNN in bestandsnaam; anders pack-default: bass=36 (C2), anders=60 (C4)
        int d=(fPackOf[idx]==2)?36:60;
        return name_midi(base_of(fFull[idx]),d);
    }
    void loadSnd(int idx){
        if(idx<0||idx>=(int)fFull.size()||fFull[idx].empty())return;
        fCur=idx;fSrc=rootOf(idx);
        if(!load_wav(fFull[idx],fBuf,fRate))fBuf.clear();
        fStackBufs.clear();
    }
    void toggle(int idx){
        if(idx<0||idx>=(int)fFull.size())return;
        if(fSel.count(idx))fSel.erase(idx);else fSel.insert(idx);
        fStackBufs.clear();
        setState("sel",selState().c_str());
    }
    void ensureStack(){
        fStackBufs.clear();fStackSrc.clear();fStackRates.clear();
        std::vector<int> use(fSel.begin(),fSel.end());
        if(use.empty()&&fCur>=0)use.push_back(fCur);
        for(int idx:use){
            std::vector<int16_t> buf;int rate=22050;
            if(load_wav(fFull[idx],buf,rate)){fStackBufs.push_back(buf);fStackSrc.push_back(rootOf(idx));fStackRates.push_back(rate);}
        }
    }
    IVoice fV[24]={};
    std::vector<std::string> fFull,fDisp;std::vector<int> fPackOf,fIdx;
    std::vector<int16_t> fBuf;
    std::vector<std::vector<int16_t>> fStackBufs;std::vector<int> fStackSrc,fStackRates;
    std::set<int> fSel;
    int fCur=-1,fSrc=60,fRate=22050;bool fNamesSent=false;
    float fPack=0.f,fSnd=0.f,fKracht=0.8f,fLevel=0.f,fOn=1.f,fToon=0.f,fLengte=1.f,fKnip=0.f,
          fDice=0.f,fTgl=0.f,fWis=0.f,fGalaxyV=9.f,fSfeerV=1.f,fKnipPh=0.f;
    unsigned fRnd=7654321;
    Galaxy fGal;Sfeer fSf;
    float fScope[40];uint32_t fScopeN=0;int fDecim=0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuraPlugin)
};
Plugin* createPlugin(){return new AuraPlugin();}
END_NAMESPACE_DISTRHO
