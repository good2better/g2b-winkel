/* G2B DNA-SAMPLER — laadt je eigen one-shot mappen (MIJN-DNA-SOUNDS-G2B) en maakt
 * ze speelbaar. PRIVÉ — nooit verkopen.
 * Modi: CHROMATIC = gekozen sound uitgerekt over het keyboard.
 *       KIT = alle sounds uit DEZELFDE map op opeenvolgende toetsen (vanaf C1/36).
 * Gesorteerd: submap/bestandsnaam. Bron-noot uit bestandsnaam (n60 / _60_) anders 60.
 * Root-mappen: DNA-MAP.txt naast de plugin/exe (of env G2B_DNA_ROOT).
 */
#include "DistrhoPlugin.hpp"
#include "dna_shared.h"
std::vector<std::string>& dna_names_list(){static std::vector<std::string> v;return v;}
#include "../g2b_std.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>

START_NAMESPACE_DISTRHO

struct IVoice{bool on=false;int note=0;float vel=0.f,t=0.f,rel=0.f;
    std::vector<int16_t> smp;uint32_t slen=0;float spos=0.f,sstep=0.f;};

static std::vector<std::string> g_roots(){
    std::vector<std::string> roots;
    const char* env=getenv("G2B_DNA_ROOT");
    if(env&&*env)roots.push_back(env);
    for(const char* p:{"DNA-MAP.txt","C:/G2B/DNA-MAP.txt","D:/G2B/DNA-MAP.txt",
                      "C:/Program Files/Common Files/VST3/DNA-MAP.txt"}){
        std::ifstream f(p);std::string line;
        while(std::getline(f,line)){
            if(line.size()&&line.back()=='\r')line.pop_back();
            if(line.size()&&line[0]!='#')roots.push_back(line);
        }
    }
    return roots;
}
static int name_midi(const std::string& base){
    std::string b=base;size_t i=b.rfind(".wav");if(i!=std::string::npos)b=b.substr(0,i);
    for(size_t k=0;k+1<b.size();k++)
        if(b[k]=='n'&&isdigit((unsigned char)b[k+1])){int v=atoi(b.c_str()+k+1);if(v>=0&&v<=127)return v;}
    for(size_t k=0;k+3<b.size();k++)
        if(b[k]=='_'&&isdigit((unsigned char)b[k+1])&&isdigit((unsigned char)b[k+2])&&b[k+3]=='_'){
            int v=atoi(b.c_str()+k+1);if(v>=0&&v<=127)return v;}
    return 60;
}
static std::string base_of(const std::string& p){
    size_t s=p.find_last_of("/\\");return s==std::string::npos?p:p.substr(s+1);
}
static std::string dir_of(const std::string& p){
    size_t s=p.find_last_of("/\\");return s==std::string::npos?std::string():p.substr(0,s);
}
static void scan_dir(const std::string& dir,const std::string& root,
                     std::vector<std::string>& full,std::vector<std::string>& disp){
    DIR* d=opendir(dir.c_str());if(!d)return;
    struct dirent* e;
    while((e=readdir(d))){
        std::string n=e->d_name;
        if(n=="."||n=="..")continue;
        std::string p=dir+"/"+n;
        struct stat st;
        if(stat(p.c_str(),&st)!=0)continue;
        if(S_ISDIR(st.st_mode)){scan_dir(p,root,full,disp);continue;}
        if(n.size()>4&&(n.substr(n.size()-4)==".wav"||n.substr(n.size()-4)==".WAV")){
            std::string rel=p.size()>root.size()?p.substr(root.size()):p;
            while(!rel.empty()&&(rel[0]=='/'||rel[0]=='\\'))rel.erase(0,1);
            std::string dd=root;size_t s2=dd.find_last_of("/\\");
            if(s2!=std::string::npos)dd=dd.substr(s2+1);
            std::string dsp=dd+"/"+rel;
            if(dsp.size()>60)dsp=dsp.substr(0,57)+"...";
            full.push_back(p);disp.push_back(dsp);
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

class SamplerPlugin:public Plugin{
public:
    SamplerPlugin():Plugin(kNumParams,0,2){scan();}
protected:
    const char* getLabel()const override{return "G2B DNA-Sampler";}
    const char* getMaker()const override{return "Good2Better";}
    const char* getLicense()const override{return "G2B";}
    uint32_t getVersion()const override{return d_version(1,0,0);}
    int64_t getUniqueId()const override{return d_cconst('G','2','D','S');}
    enum{kSnd=0,kKracht,kLevel,kOn,kGalaxy,kSfeer,kDice,kKit,kNumParams};
    void initParameter(uint32_t i,Parameter& p)override{
        p.ranges.def=0.f;p.hints=kParameterIsAutomatable;
        switch(i){
        case kSnd:p.name="SOUND";p.symbol="snd";p.hints|=kParameterIsInteger;
            p.ranges.min=0;p.ranges.max=(float)std::max<int>(1,(int)fFiles.size()-1);break;
        case kKracht:p.name="KRACHT";p.symbol="kracht";p.ranges.min=0;p.ranges.max=1;p.ranges.def=0.8f;break;
        case kLevel:p.name="OUTPUT";p.symbol="level";p.ranges.min=-24;p.ranges.max=12;p.ranges.def=0;p.unit="dB";break;
        case kOn:p.name="Aan";p.symbol="aan";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=1;break;
        case kGalaxy:p.name="DIMENTIES";p.symbol="dim";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=26;p.ranges.def=9;break;
        case kSfeer:p.name="Sfeer";p.symbol="sfeer";p.hints|=kParameterIsInteger;p.ranges.min=0;p.ranges.max=2;p.ranges.def=1;break;
        case kDice:p.name="DICE";p.symbol="dice";p.hints|=kParameterIsInteger|kParameterIsTrigger;p.ranges.min=0;p.ranges.max=1;break;
        case kKit:p.name="KIT";p.symbol="kit";p.hints|=kParameterIsBoolean;p.ranges.min=0;p.ranges.max=1;p.ranges.def=0;break;
        }
    }
    float getParameterValue(uint32_t i)const override{
        switch(i){case kSnd:return fSnd;case kKracht:return fKracht;case kLevel:return fLevel;
                  case kOn:return fOn;case kGalaxy:return fGalaxyV;case kSfeer:return fSfeerV;
                  case kDice:return fDice;case kKit:return fKit;}return 0.f;
    }
    void setParameterValue(uint32_t i,float v)override{
        switch(i){
        case kSnd:if((int)v!=fCur)loadSnd((int)v);fSnd=v;break;
        case kKracht:fKracht=v;break;case kLevel:fLevel=v;break;case kOn:fOn=v;break;
        case kGalaxy:fGalaxyV=v;fGal.setPos((int)(v+0.5f));break;
        case kSfeer:fSfeerV=v;fSf.setMode((int)(v+0.5f));break;
        case kDice:fDice=v;if(v>0.5f){fRnd=fRnd*1664525u+1013904223u;
            int nv=(fRnd>>9)%(int)fFiles.size();loadSnd(nv);fSnd=(float)nv;}fDice=0.f;break;
        case kKit:fKit=v;if(v>0.5f)buildKit();break;
        }
    }
    void initState(uint32_t index,String& name,String& def)override{
        if(index==0){name="scope";def="";}
        else{name="instrnames";def="";
            int lim=std::min<int>(512,(int)fFiles.size());
            for(int i=0;i<lim;i++){def+=fFiles[i].c_str();if(i<lim-1)def+=";";}}
    }
    void setState(const char*,const char*)override{}
    void run(const float**,float** out,uint32_t frames,const MidiEvent* ev,uint32_t evc)override{
        const float sr=getSampleRate();
        memset(out[0],0,frames*sizeof(float));memset(out[1],0,frames*sizeof(float));
        if(fOn<0.5f)return;
        const float outG=powf(10.f,fLevel*0.05f);
        uint32_t pos=0;
        for(uint32_t e=0;e<=evc;e++){
            uint32_t t=(e<evc)?ev[e].frame:frames;
            while(pos<t){
                float s=0;
                for(auto& v:fV)if(v.on){
                    v.t+=1.f/sr;float w=0;
                    if(v.slen){
                        uint32_t i=(uint32_t)v.spos;
                        if(i>=v.slen-1){v.on=false;continue;}
                        float fr=v.spos-i;
                        w=(v.smp[i]+fr*(v.smp[i+1]-v.smp[i]))/32768.f;
                        v.spos+=v.sstep;
                    }
                    float env=1.f;
                    if(v.rel>0.f){env=v.rel;v.rel-=1.f/(sr*0.35f);if(v.rel<=0.f){v.on=false;continue;}}
                    s+=w*env*v.vel;
                }
                s*=outG*0.45f;
                if(s>0.85f)s=0.85f+(s-0.85f)*0.25f;else if(s<-0.85f)s=-0.85f+(s+0.85f)*0.25f;
                out[0][pos]=s;out[1][pos]=s;
                if(++fDecim>=3){fDecim=0;if(fScopeN<40)fScope[fScopeN++]=s;}
                ++pos;
            }
            if(e<evc){
                const MidiEvent& m=ev[e];
                if(m.size>=3&&(m.data[0]&0xF0)==0x90&&m.data[2]>0){
                    for(auto& v:fV)if(!v.on){
                        if(fKit>0.5f){
                            int ki=(int)m.data[1]-36;
                            if(ki<0||ki>=(int)fKitBufs.size()||fKitBufs[ki].empty())break;
                            v.on=true;v.note=m.data[1];v.rel=0.f;
                            v.vel=(m.data[2]/127.f)*(0.4f+0.6f*fKracht);
                            v.t=0;v.smp=fKitBufs[ki];v.slen=(uint32_t)v.smp.size();v.spos=0.f;
                            v.sstep=(float)fKitRates[ki]/sr;  // geen transpose — kit speelt as-is
                        }else{
                            if(fBuf.empty())break;
                            v.on=true;v.note=m.data[1];v.rel=0.f;
                            v.vel=(m.data[2]/127.f)*(0.4f+0.6f*fKracht);
                            v.t=0;v.smp=fBuf;v.slen=(uint32_t)v.smp.size();v.spos=0.f;
                            float f0=440.f*powf(2.f,(m.data[1]-69)/12.f),fs=440.f*powf(2.f,(fSrc-69)/12.f);
                            v.sstep=(f0/fs)*((float)fRate/sr);
                        }
                        break;
                    }
                }else if(m.size>=3&&((m.data[0]&0xF0)==0x80||((m.data[0]&0xF0)==0x90&&m.data[2]==0))){
                    for(auto& v:fV)if(v.on&&v.note==m.data[1]&&v.rel==0.f)v.rel=1.f;
                }
            }
        }
        if(!fNamesSent){
            fNamesSent=true;
            std::string list;int lim=std::min<int>(512,(int)fFiles.size());
            for(int i=0;i<lim;i++){list+=fFiles[i];if(i<lim-1)list+=";";}
            setState("instrnames",list.c_str());
        }
        if(fScopeN>=40){
            char buf[256];int p=0;
            for(uint32_t s2=0;s2<fScopeN&&p<230;++s2)p+=snprintf(buf+p,sizeof(buf)-p,"%.3f;",fScope[s2]);
            buf[p]=0;setState("scope",buf);fScopeN=0;
        }
        g2b_post(nullptr,out,frames,sr,fOn,100.f,fGal,&fSf,false);
    }
private:
    void scan(){
        fFiles.clear();fFull.clear();
        for(const std::string& r:g_roots())scan_dir(r,r,fFull,fFiles);
        std::vector<size_t> ix(fFiles.size());
        for(size_t i=0;i<ix.size();i++)ix[i]=i;
        std::sort(ix.begin(),ix.end(),[&](size_t a,size_t b){return fFiles[a]<fFiles[b];});
        std::vector<std::string> a(fFiles),b(fFull);
        for(size_t i=0;i<ix.size();i++){fFiles[i]=a[ix[i]];fFull[i]=b[ix[i]];}
        if(fFiles.empty()){fFiles.push_back("(geen wavs gevonden — zie LEESMIJ)");fFull.push_back("");}
        dna_names_list()=fFiles;
    }
    void loadSnd(int idx){
        if(idx<0||idx>=(int)fFull.size()||fFull[idx].empty()){fBuf.clear();return;}
        fCur=idx;fSrc=name_midi(base_of(fFull[idx]));
        if(!load_wav(fFull[idx],fBuf,fRate))fBuf.clear();
    }
    void buildKit(){
        fKitBufs.clear();fKitRates.clear();
        if(fCur<0||fCur>=(int)fFull.size())return;
        std::string dir=dir_of(fFull[fCur]);
        std::vector<int> idx;
        for(int i=0;i<(int)fFull.size();i++)
            if(dir_of(fFull[i])==dir)idx.push_back(i);
        int cap=std::min<int>(48,(int)idx.size());
        for(int k=0;k<cap;k++){
            std::vector<int16_t> buf;int rate=22050;
            if(load_wav(fFull[idx[k]],buf,rate)){fKitBufs.push_back(buf);fKitRates.push_back(rate);}
            else{fKitBufs.push_back({});fKitRates.push_back(rate);}
        }
    }
    IVoice fV[16]={};
    std::vector<std::string> fFiles,fFull;
    std::vector<int16_t> fBuf;
    std::vector<std::vector<int16_t>> fKitBufs;
    std::vector<int> fKitRates;
    int fCur=-1,fSrc=60,fRate=22050;bool fNamesSent=false;
    float fSnd=0.f,fKracht=0.8f,fLevel=0.f,fOn=1.f,fGalaxyV=9.f,fSfeerV=1.f,fDice=0.f,fKit=0.f;
    unsigned fRnd=1234567;
    Galaxy fGal;Sfeer fSf;
    float fScope[40];uint32_t fScopeN=0;int fDecim=0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerPlugin)
};
Plugin* createPlugin(){return new SamplerPlugin();}
END_NAMESPACE_DISTRHO
