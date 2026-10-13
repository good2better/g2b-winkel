#include <cstdlib>
#include <stdio.h>
#include "brein_data.h"
#include "brein_gen.cpp"
static void vl(FILE*f,unsigned n){unsigned char b[8];int i=0;b[i++]=n&127;while(n>>=7)b[i++]=(n&127)|128;while(i)fputc(b[--i],f);}
int main(int argc,char**argv){
  int gi=argc>1?atoi(argv[1]):3, ki=argc>2?atoi(argv[2]):0, bars=argc>3?atoi(argv[3]):4;
  const BGK gk=kGenreKits[gi];
  if(ki>=gk.n)ki=0;
  BreinGen g(42);
  auto ns=g.gen(gk.off+ki,bars);
  printf("genre %s kit %s bars %d -> %d noten\n",kGenreNames[gi],kNames[kKits[gk.off+ki].name],bars,(int)ns.size());
  int c9=0,c0=0;for(auto&n:ns)(n.ch==9?c9:c0)++;
  printf("drum-ch9:%d mel-ch0:%d\n",c9,c0);
  FILE*f=fopen("/tmp/brein_cpp.mid","wb");
  fwrite("MThd",1,4,f);fputc(0,f);fputc(0,f);fputc(0,f);fputc(6,f);fputc(0,f);fputc(0,f);fputc(0,f);fputc(1,f);fputc(0x01,f);fputc(0xE0,f);
  // track: tempo 120 + events + eot
  unsigned char tr[1<<20];unsigned tp=0;
  tr[tp++]=0;tr[tp++]=0xFF;tr[tp++]=0x51;tr[tp++]=3;int us=500000;tr[tp++]=(us>>16)&255;tr[tp++]=(us>>8)&255;tr[tp++]=us&255;
  unsigned prev=0;
  for(auto&n:ns){
    unsigned dt=n.t-prev;prev=n.t;unsigned x=dt;unsigned char b[8];int i=0;b[i++]=x&127;while(x>>=7)b[i++]=(x&127)|128;while(i)tr[tp++]=b[--i];
    tr[tp++]=0x90|n.ch;tr[tp++]=n.note;tr[tp++]=n.vel;
    unsigned dt2=n.dur;prev+=n.dur;i=0;x=dt2;b[i++]=x&127;while(x>>=7)b[i++]=(x&127)|128;while(i)tr[tp++]=b[--i];
    tr[tp++]=0x80|n.ch;tr[tp++]=n.note;tr[tp++]=0;
  }
  tr[tp++]=0;tr[tp++]=0xFF;tr[tp++]=0x2F;tr[tp++]=0;
  fwrite("MTrk",1,4,f);fputc((tp>>24)&255,f);fputc((tp>>16)&255,f);fputc((tp>>8)&255,f);fputc(tp&255,f);fwrite(tr,1,tp,f);fclose(f);
  printf("midi geschreven %d bytes\n",(int)(tp+14));
  return 0;
}
