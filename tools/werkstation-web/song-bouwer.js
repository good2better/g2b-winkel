/* ============================================================
   G2B SONG-BOUWER — vaste-blok arrangement (Intro→Verse→Build→Drop→Break→Drop2)
   Rendert de ECHTE engines van basfabriek + melodie via same-origin iframes.
   Menger: auto-balance -6dB · vocal-dip 2.5kHz · mono <120Hz
   Master: CLUB -9 LUFS / STREAMING -14 LUFS (tanh softclip + limiter)
   ============================================================ */
(function(){
'use strict';
const $=id=>document.getElementById(id);
const st=t=>{const e=$('sbStatus');if(e)e.textContent=t};
/* ---------- secties & matrix ---------- */
const SECTIES=[['INTRO',8],['BUILD',4],['DROP',8],['BREAK',4],['DROP 2',8]]; // 32 maten (v3.2)
let CUR=SECTIES; // actieve sectielijst (Snel-Preview = alleen DROP 8)
const SPOREN=[['VOCAAL','voc'],['KICK','kick'],['SUB','sub'],['GRIT','grit'],['MELODIE','mel'],['AKKOORDEN','akk']];
const DEFAAN={INTRO:{voc:1,mel:1,akk:1},BUILD:{voc:1,grit:1,mel:1,akk:1},DROP:{voc:1,kick:1,sub:1,grit:1,mel:1,akk:1},BREAK:{voc:1,mel:1,akk:1},'DROP 2':{voc:1,kick:1,sub:1,grit:1,mel:1,akk:1}};
const AAN={}; // AAN[sectie][spoor]
for(const[s]of SECTIES){AAN[s]={};for(const[,sp]of SPOREN)AAN[s][sp]=DEFAAN[s][sp]?1:0}
/* ---------- basis-DSP (zelfstandig; geen afhankelijkheid) ---------- */
const SR=44100;
function lop(x,f,sr){const a=1-Math.exp(-6.2832*f/sr);const y=new Float32Array(x.length);let p=0;for(let i=0;i<x.length;i++){p+=a*(x[i]-p);y[i]=p}return y}
function hip(x,f,sr){const a=sr/(sr+6.2832*f);const y=new Float32Array(x.length);let px=0,py=0;for(let i=0;i<x.length;i++){y[i]=a*(py+x[i]-px);px=x[i];py=y[i]}return y}
function bell(x,f0,Q,dB,sr){ // peaking EQ biquad (mono)
 const A=Math.pow(10,dB/40),w=6.2832*f0/sr,al=Math.sin(w)/(2*Q),cw=Math.cos(w);
 const b=[1+al*A,-2*cw,1-al*A],a0=1+al/A,a=[(-2*cw)/a0,(1-al/A)/a0];const bn=[b[0]/a0,b[1]/a0,b[2]/a0];
 const y=new Float32Array(x.length);let x1=0,x2=0,y1=0,y2=0;
 for(let i=0;i<x.length;i++){const v=x[i];y[i]=bn[0]*v+bn[1]*x1+bn[2]*x2-a[0]*y1-a[1]*y2;x2=x1;x1=v;y2=y1;y1=y[i]}return y}
/* ---------- iframe-engines ---------- */
let DW=null,MW=null; // contentWindow van donder/melodie
function mkFrame(src){return new Promise((res,rej)=>{const f=document.createElement('iframe');f.style.cssText='width:0;height:0;border:0;position:absolute;left:-9999px';f.src=src+(src.includes('?')?'&':'?')+'v='+Date.now();f.onload=()=>res(f);f.onerror=rej;document.body.appendChild(f)})}
async function laadEngines(){
 if(DW&&MW)return;
 st('⚙ engines laden (basfabriek + melodie)…');
 const fd=await mkFrame('../basfabriek/index.html'),fm=await mkFrame('../melodie/index.html');
 DW=fd.contentWindow;MW=fm.contentWindow;
 // wacht tot engines geboot zijn (dobbelAlles geeft meteen een patroon)
 for(let i=0;i<80&&!(DW&&DW.renderLoop&&DW.S&&DW.S.notes&&DW.S.notes.length);i++)await new Promise(r=>setTimeout(r,100));
 for(let i=0;i<80&&!(MW&&MW.renderSpoorBuf&&MW.S&&MW.S.mel&&MW.S.mel.length);i++)await new Promise(r=>setTimeout(r,100));
 if(!DW.renderLoop)throw new Error('basfabriek-engine niet gevonden');
 if(!MW.renderSpoorBuf)throw new Error('melodie-engine niet gevonden');
 /* FLIP-select vullen met de genres uit de fabriek */
 try{const fs=$('sbFlip'),ds=DW.document.getElementById('genre');
  if(fs&&ds&&fs.options.length<=1){fs.innerHTML='';
   for(let i=0;i<ds.options.length;i++){const o=document.createElement('option');
    o.value=i;o.textContent='🔀 '+ds.options[i].textContent;fs.appendChild(o)}
   fs.value=ds.selectedIndex||0}}catch(e){}}
/* ---------- stem-tile naar 8 maten ---------- */
function tile8(mono,sd,maten){ // mono Float32Array van `maten` maten → 8 maten
 const bar=Math.round(sd*16*SR);let out=mono;
 if(maten>=8)return out.subarray(0,8*bar);
 const o=new Float32Array(8*bar),n=Math.round(maten*bar);
 const seg=mono.subarray(0,Math.min(n,mono.length));
 for(let b=0;b*n<8*bar;b++){const len=Math.min(seg.length,8*bar-b*n);o.set(seg.subarray(0,len),b*n)}
 return o}
/* ---------- v5.0: VOCAL & HOOK-COACH ---------- */
let VOC=null; // {L,R,env:[rms per 16e],key,bpm}
function pitchOf(x,sr){ /* zelfde autocorrelatie-truc als basfabriek */
 const off=Math.min(Math.floor(.04*sr),Math.floor(x.length*.3)),xx=x.subarray(off);
 let e=0;for(let i=0;i<xx.length;i++)e+=xx[i]*xx[i];if(e<1e-4)return 0;
 const lo=Math.floor(sr/700),hi=Math.min(Math.floor(sr/75),xx.length-2);
 let best=0,bestLag=0;
 for(let lag=lo;lag<=hi;lag++){let c=0;const m=xx.length-lag;
  for(let i=0;i<m;i+=4)c+=xx[i]*xx[i+lag];c/=m;
  if(c>best){best=c;bestLag=lag}}
 return bestLag?sr/bestLag:0}
function keyOf(x,sr){ // pitch-class histogram → beste mineur OF majeur fit
 const hist=new Array(12).fill(0),hop=2048,len=8192;
 for(let o=0;o+len<x.length;o+=hop*4){const f=pitchOf(x.subarray(o,o+len),sr);
  if(f>75&&f<700){let e=0;for(let i=0;i<len;i+=16)e+=x[o+i]*x[o+i];
   hist[Math.round(69+12*Math.log2(f/440))%12+12&11]+=Math.sqrt(e)}}
 const MINOR=[0,2,3,5,7,8,10],MAJOR=[0,2,4,5,7,9,11];
 let bk=-1,bs=-1e9,bm='mineur';
 for(let k=0;k<12;k++)for(const[lad,mod]of[[MINOR,'mineur'],[MAJOR,'majeur']]){
  let sc=0;for(let i=0;i<12;i++)sc+=hist[i]*(lad.includes((i-k+12)%12)?1:-.5);
  if(sc>bs){bs=sc;bk=k;bm=mod}}
 return{k:bk<0?0:bk,m:bm}}
function bpmOf(x,sr){ // onset-envelop → IOI-histogram → bpm 60-180
 const hop=256,n=Math.floor(x.length/hop),env=new Float32Array(n);let pv=0;
 for(let i=0;i<n;i++){let e=0;for(let j=0;j<hop;j++)e+=x[i*hop+j]*x[i*hop+j];
  env[i]=Math.max(0,e-pv);pv=e*.8}
 const hopHz=sr/hop,hist={};
 for(let i=2;i<n-1;i++)if(env[i]>env[i-1]&&env[i]>=env[i+1]&&env[i]>.02){
  for(let j=Math.max(2,i-140);j<i-2;j++)if(env[j]>env[j-1]&&env[j]>=env[j+1]&&env[j]>.02){
   const ioi=(i-j)/hopHz;if(ioi>.3&&ioi<2){const b=Math.round(60/ioi);if(b>=60&&b<=180)hist[b]=(hist[b]||0)+env[i]}break}}
 let bb=0,bv=0;for(const b in hist)if(hist[b]>bv){bv=hist[b];bb=+b}
 return bb||120}
async function laadVocal(bron,retune){ /* bron: File of AudioBuffer; retune=false → alleen VOC zetten (.g2b-load) */
 if(retune===undefined)retune=true;
 st('🎤 vocal laden + analyseren…');
 const ac=window._vocAC||(window._vocAC=new (window.AudioContext||window.webkitAudioContext)());
 let buf=bron.getChannelData?bron:await ac.decodeAudioData(await bron.arrayBuffer());
 let L=buf.getChannelData(0),R=buf.numberOfChannels>1?buf.getChannelData(1):L;
 if(buf.sampleRate!==SR){const r=buf.sampleRate/SR,n2=Math.floor(L.length/r),l2=new Float32Array(n2),r2=new Float32Array(n2);
  for(let i=0;i<n2;i++){const x=i*r,x0=Math.floor(x),t=x-x0;l2[i]=L[x0]*(1-t)+(L[x0+1]||0)*t;r2[i]=R[x0]*(1-t)+(R[x0+1]||0)*t}L=l2;R=r2}
 const mono=Float32Array.from(L,(v,i)=>(v+R[i])*.5);
 const bpm=bpmOf(mono,SR),{k,m}=keyOf(mono,SR);
 /* MATCH-UP stap 3: eerste klemtoon (onset) vinden → op de maatlijn leggen */
 const hop=256,nf=Math.floor(mono.length/hop);let onset=0,pk=0;
 {const e2=new Float32Array(nf),env=new Float32Array(nf);let pv=0;
  for(let i=0;i<nf;i++){let e=0;for(let j=0;j<hop;j++)e+=mono[i*hop+j]*mono[i*hop+j];
   e2[i]=e;env[i]=Math.max(0,e-pv);pv=Math.max(pv*.6,e)}
  for(let i=0;i<nf;i++)pk=Math.max(pk,Math.abs(mono[i*hop]));
  /* anker = eerste energie-STIJGING met niveau: een constante achtergrond of
     adem stijgt niet/staat te laag; een rustige opening stijgt wel → nooit
     doorgesneden. Vindt niets → onset=0 (alles speelt mee, niets weg) */
  /* vloer = mediaan energie van de eerste hops: constante brom/achtergrond
     die vanaf sample 0 meedoet telt niet als "stijging" → kan geen anker */
  const nb=Math.min(30,Math.floor(nf/4)||1),
   be=Array.from(e2.subarray(0,nb)).sort((a,b)=>a-b),
   lvl=Math.max(pk*pk*hop*.015,(be[Math.floor(be.length/2)]||0)*4),
   thR=pk*pk*hop*.01,thE=pk*pk*hop*.015;let i0=nf,fb=nf;
  for(let i=0;i+2<nf;i++){
   if(!(env[i]>thR&&e2[i]>lvl&&e2[i+1]>thE&&e2[i+2]>thE))continue;
   if(fb===nf)fb=i;                 /* eerste kandidaat = fallback */
   /* adem/ruis = breedband zonder toon (hoge nuldoorgang) → overslaan; toon
      gemeten op de hops ná de sprong (hop i kan half-gemengd zijn) */
   let z1=0,z2=0;
   for(let j=(i+1)*hop+1;j<(i+2)*hop;j++)z1+=mono[j]>=0!==mono[j-1]>=0;
   for(let j=(i+2)*hop+1;j<(i+3)*hop;j++)z2+=mono[j]>=0!==mono[j-1]>=0;
   if(z1<0.18*hop||z2<0.18*hop){i0=i;break}}
  /* 's'/'t'-begin hoort bij de take: loop terug tot het begin van de aaneen-
     gesloten ruis-run en neem die mee, maar ALLEEN als hij ≤~116ms is — een
     aanwellende adem is langer (of gescheiden) en ankeren op een piek
     erbinnen zou de consonant halverwege afsnijden */
  let anc=i0;
  if(i0!==nf){let cs=i0;while(cs>0&&e2[cs-1]>lvl)cs--;
   const run=i0-cs;if(run>0&&run<=16)anc=cs}
  onset=i0===nf?(fb===nf?0:fb*hop):anc*hop}
 VOC={L,R,mono,key:k,mod:m,bpm,onset};
 /* engines op de stem afstemmen: bpm + toonsoort doorzetten */
 if(retune){try{const d=DW&&DW.document;if(d){if(d.getElementById('bpm'))d.getElementById('bpm').value=bpm;
  if(d.getElementById('key'))d.getElementById('key').value=k;DW.dobbelAlles&&DW.dobbelAlles()}}catch(e){}
 try{const d=MW&&MW.document;if(d){if(d.getElementById('bpm'))d.getElementById('bpm').value=bpm;
  if(d.getElementById('wortel'))d.getElementById('wortel').value=k;MW.maak&&MW.maak('alles')}}catch(e){}}
 st('🎤 stem gelezen: ~'+bpm+' bpm · '+NOOTNAMEN[k]+' '+m+' — engines passen zich aan; druk 🎬 BOUW');
 if($('sbVocInfo'))$('sbVocInfo').textContent='🎤 '+bpm+'bpm · '+NOOTNAMEN[k]+' '+m+' · '+Math.round(mono.length/SR)+'s'}
const NOOTNAMEN=['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
/* ---------- FX ---------- */
function riser(lenB,sr){ // witruis + HP-sweep omhoog (4 maten BUILD)
 const n=Math.round(lenB*sr),y=new Float32Array(n);
 for(let i=0;i<n;i++)y[i]=(Math.random()*2-1)*.3*(i/n);
 const a=sr/(sr+6.2832*400);let px=0,py=0;
 for(let i=0;i<n;i++){const f=400+5600*(i/n),aa=sr/(sr+6.2832*f);y[i]=aa*(py+y[i]-px);px=y[i];py=y[i];y[i]*=(0.15+0.85*i/n)}
 return y}
function crash(sr){
 const n=Math.round(1.2*sr),y=new Float32Array(n);
 for(let i=0;i<n;i++)y[i]=(Math.random()*2-1)*Math.exp(-5*i/n)*.5;
 return lop(y,6000,sr)}
/* ---------- wav/zip/midi writers ---------- */
function wavBytes(L,R,sr){ // 24-bit stereo PCM (studio-kwaliteit)
 const n=L.length,b=new ArrayBuffer(44+n*6),d=new DataView(b);
 const w=(o,s)=>{for(let i=0;i<s.length;i++)d.setUint8(o+i,s.charCodeAt(i))};
 w(0,'RIFF');d.setUint32(4,36+n*6,1);w(8,'WAVE');w(12,'fmt ');
 d.setUint32(16,16,1);d.setUint16(20,1,1);d.setUint16(22,2,1);
 d.setUint32(24,sr,1);d.setUint32(28,sr*6,1);d.setUint16(32,6,1);d.setUint16(34,24,1);
 w(36,'data');d.setUint32(40,n*6,1);
 const q=v=>{const x=Math.max(-1,Math.min(1,v))*0x7FFFFF;return x<0?x+0x1000000:x};
 for(let i=0;i<n;i++){const l=q(L[i])|0,r=q(R[i])|0,o=44+i*6;
  d.setUint8(o,l&255);d.setUint8(o+1,(l>>8)&255);d.setUint8(o+2,(l>>16)&255);
  d.setUint8(o+3,r&255);d.setUint8(o+4,(r>>8)&255);d.setUint8(o+5,(r>>16)&255)}
 return b}
const crc32=(()=>{const t=new Uint32Array(256);for(let i=0;i<256;i++){let c=i;for(let k=0;k<8;k++)c=c&1?0xEDB88320^(c>>>1):c>>>1;t[i]=c}
 return u=>{let c=0xFFFFFFFF;for(let i=0;i<u.length;i++)c=t[(c^u[i])&255]^(c>>>8);return (c^0xFFFFFFFF)>>>0}})();
function zipStore(files){
 const enc=s=>{const b=new Uint8Array(s.length);for(let i=0;i<s.length;i++)b[i]=s.charCodeAt(i)&255;return b};
 const ns=files.map(f=>enc(f.naam));
 const total=files.reduce((a,f,i)=>a+30+ns[i].length+f.data.length,0);
 const cdLen=files.reduce((a,f,i)=>a+46+ns[i].length,0);
 const all=new Uint8Array(total+cdLen+22);const dv=new DataView(all.buffer);
 let p=0,off;const cent=[];
 files.forEach((f,i)=>{const nb=ns[i],crc=crc32(f.data),sz=f.data.length;
  cent.push(p);
  dv.setUint32(p,0x04034b50,true);dv.setUint16(p+4,20,true);
  dv.setUint16(p+6,0,true);dv.setUint16(p+8,0,true);dv.setUint16(p+10,0,true);dv.setUint16(p+12,0,true);
  dv.setUint32(p+14,crc,true);dv.setUint32(p+18,sz,true);dv.setUint32(p+22,sz,true);
  dv.setUint16(p+26,nb.length,true);dv.setUint16(p+28,0,true);
  p+=30;all.set(nb,p);p+=nb.length;all.set(f.data,p);p+=sz});
 const cdStart=p;
 files.forEach((f,i)=>{const nb=ns[i],crc=crc32(f.data),sz=f.data.length;
  dv.setUint32(p,0x02014b50,true);dv.setUint16(p+4,20,true);dv.setUint16(p+6,20,true);
  dv.setUint16(p+8,0,true);dv.setUint16(p+10,0,true);dv.setUint16(p+12,0,true);dv.setUint16(p+14,0,true);
  dv.setUint32(p+16,crc,true);dv.setUint32(p+20,sz,true);dv.setUint32(p+24,sz,true);
  dv.setUint16(p+28,nb.length,true);dv.setUint16(p+30,0,true);dv.setUint16(p+32,0,true);
  dv.setUint16(p+34,0,true);dv.setUint16(p+36,0,true);dv.setUint32(p+38,0,true);
  dv.setUint32(p+42,cent[i],true);
  p+=46;all.set(nb,p);p+=nb.length});
 dv.setUint32(p,0x06054b50,true);dv.setUint16(p+4,0,true);dv.setUint16(p+6,0,true);
 dv.setUint16(p+8,files.length,true);dv.setUint16(p+10,files.length,true);
 dv.setUint32(p+12,cdLen,true);dv.setUint32(p+16,cdStart,true);dv.setUint16(p+20,0,true);
 return all}
function vlq(n){const b=[n&127];n>>=7;while(n){b.unshift((n&127)|128);n>>=7}return b}
function midiSong(sporen,bpm,secs){ // sporen:[{naam,noten:[{p,s,d,v}],kanaal}] s=16e-steps; secs: [{naam,bar}]
 const us=Math.round(6e7/bpm),tpq=96;
 const meta=[[0,[0xFF,0x51,3,(us>>16)&255,(us>>8)&255,us&255]],
  ...secs.map(s=>[s.bar*4*tpq,[0xFF,0x06,s.naam.length,...[...s.naam].map(c=>c.charCodeAt(0))]]),
  [secs.length?Math.max(...secs.map(s=>s.bar))*4*tpq:0,[0xFF,0x2F,0]]];
 const mTrk=ev=>{const t=ev.slice().sort((a,b)=>a[0]-b[0]);let last=0;const out=[];
  for(const[tick,e]of t){out.push(...vlq(tick-last),...e);last=tick}
  out.push(...vlq(0),0xFF,0x2F,0); // EOT op het einde van de track
  const b=new Uint8Array(8+out.length);const d=new DataView(b.buffer);
  b.set([0x4D,0x54,0x72,0x6B],0);d.setUint32(4,out.length);b.set(out,8);return b};
 const trks=[mTrk(meta)];
 for(const sp of sporen){const ev=[];
  for(const n of sp.noten){const t=Math.round(n.s/4*tpq),e=Math.round((n.s+n.d)/4*tpq);
   ev.push([t,[0x90|sp.kanaal,n.p,n.v&127]],[e,[0x80|sp.kanaal,n.p,0]])}
  trks.push(mTrk(ev))}
 const h=new Uint8Array(14);const dh=new DataView(h.buffer);
 h.set([0x4D,0x54,0x68,0x64],0);dh.setUint32(4,6);dh.setUint16(8,1);dh.setUint16(10,trks.length);dh.setUint16(12,tpq);
 const tot=trks.reduce((a,t)=>a+t.length,0),out=new Uint8Array(14+tot);out.set(h,0);let o=14;
 for(const t of trks){out.set(t,o);o+=t.length}return out}
/* ---------- mix ---------- */
let LAST=null;
async function bouw(snel){
 try{
 CUR=snel?[['DROP',8]]:SECTIES;
 await laadEngines();
 st('⚙ stems renderen via de echte engines…');
 /* donder-stems: renderLoop → kick/sub/grit (mono Float32Arrays) */
 const rD=DW.renderLoop();
 const sdD=rD.sd||(60/120/4),matD=rD.maten||(DW.S&&DW.S.maten)||4;
 let v2=null;
 try{ if(DW.varTrans&&DW.S){const save=JSON.parse(JSON.stringify(DW.S.notes));
   const nv=DW.varTrans(DW.S.notes,1); // Octaaf-Bounce
   if(nv&&nv.length){DW.S.notes=nv;const r2=DW.renderLoop();v2=r2}
   DW.S.notes=save}}catch(e){v2=null}
 const mel=await MW.renderSpoorBuf('mel'),akk=await MW.renderSpoorBuf('akk');
 /* Drop-2-variatie: melodie dobbelt een nieuwe variant van mel+akk */
 let melV2=mel,akkV2=akk;
 try{if(MW.variant&&MW.S){const sm=MW.S.mel,sa=MW.S.akk;
  MW.S.mel=MW.variant(sm,.6);melV2=await MW.renderSpoorBuf('mel');
  MW.S.mel=sm;MW.S.akk=MW.variant(sa,.5);akkV2=await MW.renderSpoorBuf('akk');MW.S.akk=sa}}catch(e){}
 const sdM=mel.sd||sdD,matM=mel.maten||4;
 /* per-spoor buffers van 8 maten */
 const B=k=>tile8(rD[k],sdD,matD),B2=k=>tile8((v2||rD)[k],sdD,matD);
 const barD=Math.round(sdD*16*SR),barM=Math.round(sdM*16*SR);
 const totBars=CUR.reduce((a,[,m])=>a+m,0),N=totBars*barD;
 const T={};for(const[,sp]of SPOREN){T[sp]={L:new Float32Array(N),R:new Float32Array(N)}}
 st('⚙ arrangement + FX plaatsen…');
 /* MATCH-UP: anker = eerste voc-enabled sectie van de VOLLEDIGE song (SECTIES),
   zodat ook een 8-maten preview exact dezelfde vocal-frase op die plek speelt */
 const SOFF={};{let b=0;for(const[s,m]of SECTIES){if(!(s in SOFF))SOFF[s]=b*barD;b+=m}}
 let VOC_BAR0=0;for(const[s]of SECTIES){if(AAN[s]&&AAN[s].voc){VOC_BAR0=SOFF[s];break}}
 let bar=0;
 for(const[sec,m]of CUR){
  const off=bar,isD2=sec==='DROP 2',isDrop=sec==='DROP'||isD2;
  const put=(sp,src,srcR,g)=>{ // src = mono-buffer van ≥m maten
   const t=T[sp],o0=off*barD,n=Math.min(m*barD,src.length);
   for(let i=0;i<n;i++){t.L[o0+i]+=src[i]*g;t.R[o0+i]+=(srcR||src)[i]*g}};
  /* donder-sporen */
  const rd=isD2?B2:B;
  if(AAN[sec].kick)put('kick',rd('kick'),null,1.0);
  if(AAN[sec].sub){let sb=rd('sub');if(sec==='BUILD'){const n2=sb.length,fade=new Float32Array(n2);
    for(let i=0;i<n2;i++)fade[i]=i<n2-barD?1:Math.max(0,1-(i-(n2-barD))/barD);
    sb=Float32Array.from(sb,(x,i)=>x*fade[i])}put('sub',sb,null,1.0)}
  if(AAN[sec].grit){const src2=isD2?(v2||rD):rD;
   put('grit',tile8(src2.gritL||src2.sub,sdD,matD),tile8(src2.gritR||src2.sub,sdD,matD),0.8)}
  /* melodie-sporen */
  const ml=isD2?melV2:mel,ak=isD2?akkV2:akk;
  if(AAN[sec].mel){let l=tile8(ml.L,sdM,matM).slice(),r2=tile8(ml.R,sdM,matM).slice();
   if(sec==='INTRO'){l=lop(l,2500,SR);r2=lop(r2,2500,SR)}put('mel',l,r2,0.9)}
  if(AAN[sec].akk)put('akk',tile8(ak.L,sdM,matM),tile8(ak.R,sdM,matM),0.7);
  /* vocal-spoor: continue opname op de song-tijdlijn — MATCH-UP:
     take-index i hoort op song-positie VOC_BAR0+i-onset (accent op de eerste voc-maat);
     elke sectie leest alleen zijn eigen venster — nooit herstart, geen pickup in gemute delen */
  if(VOC&&AAN[sec].voc){const o0=off*barD,
   base=(sec in SOFF?SOFF[sec]:o0)-VOC_BAR0+(VOC.onset||0),
   n=Math.min(m*barD,Math.max(0,VOC.L.length-base));
   if(n>0)put('voc',VOC.L.subarray(base,base+n),VOC.R.subarray(base,base+n),1.0)}
  /* FX */
  if(sec==='BUILD'){const rz=riser(m*4*sdD,SR);for(let i=0;i<rz.length&&off*barD+i<N;i++){T.grit.L[off*barD+i]+=rz[i]*.4;T.grit.R[off*barD+i]+=rz[i]*.4}}
  if(isDrop){const cr=crash(SR);for(let i=0;i<cr.length&&off*barD+i<N;i++){T.kick.L[off*barD+i]+=cr[i];T.kick.R[off*barD+i]+=cr[i]}}
  bar+=m}
 st('⚙ MENGER: auto-balance + vocal-ruimte + mono<120Hz…');
 LAST={T,N,bpm:+(DW.S&&DW.S.bpm)||120,sdD,matD,matM,totBars};
 mixDown();
 }catch(e){console.error(e);st('❌ '+e.message)}
 return}
/* mix T → L/R: auto-balance + vocal-dip + mono<120Hz + master (ook na vibe-fixers) */
function mixDown(){
 const{T,N}=LAST;
 for(const[,sp]of SPOREN){const t=T[sp];let pk=0;for(let i=0;i<N;i++){const a=Math.abs(t.L[i]),b=Math.abs(t.R[i]);if(a>pk)pk=a;if(b>pk)pk=b}
  if(pk>0){const g=Math.min(1,0.5/pk);for(let i=0;i<N;i++){t.L[i]*=g;t.R[i]*=g}}}
 for(const sp of['mel','grit']){T[sp].L=bell(T[sp].L,2500,1.2,-2.5,SR);T[sp].R=bell(T[sp].R,2500,1.2,-2.5,SR)}
 /* v5.0 vocal-pocket + call&response: stem vooraan + bas antwoordt in de rust */
 if(VOC){const seg=Math.round(LAST.sdD*SR);let bar=0;
  // presence EQ + lichte kamerklank op de stem
  T.voc.L=bell(T.voc.L,3000,1.0,2.0,SR);T.voc.R=bell(T.voc.R,3000,1.0,2.0,SR);
  const dl=Math.round(.31*SR),wet=.16;
  for(const ch of['L','R']){const src=T.voc[ch].slice();
   for(let i=dl;i<N;i++)T.voc[ch][i]+=src[i-dl]*wet}
  for(const[sec,m]of CUR){const isDrop=sec==='DROP'||sec==='DROP 2';
   if(AAN[sec].voc)for(let s=0;s<m*16;s++){const a=(bar*16+s)*seg,b=Math.min(N,a+seg);let e=0;
    for(let i=a;i<b;i++)e+=T.voc.L[i]*T.voc.L[i]+T.voc.R[i]*T.voc.R[i];
    e=Math.sqrt(e/(2*Math.max(1,b-a))||1e-9);
    if(e>.02){ // stem klinkt → mel+akk zakken -2.5dB (pocket)
     for(const sp of['mel','akk'])for(const ch of['L','R'])for(let i=a;i<b;i++)T[sp][ch][i]*=.75}
    else if(isDrop){ // stem stopt → bas/grit antwoorden (call & response)
     for(const ch of['L','R'])for(let i=a;i<b;i++){T.sub[ch][i]*=1.15;T.grit[ch][i]*=1.2}}}
   bar+=m}}
 /* prio-mixbus: kick duckt sub/grit/akk op elke aanslag — de beat pompt, nooit modderig */
 {const tau=Math.round(.09*SR);let duck=1,prevK=0;
  for(let i=0;i<N;i++){const k=Math.abs(T.kick.L[i])+Math.abs(T.kick.R[i]);
   if(k>.18&&prevK<=.18)duck=Math.min(duck,.38);else duck+=(1-duck)*(1-Math.exp(-1/tau));
   prevK=k;const g2=.45+.55*duck; // onderin blijft 45% body — pompt zonder te verdwijnen
   for(const sp of['sub','grit','akk']){T[sp].L[i]*=g2;T[sp].R[i]*=g2}}}
 const L=new Float32Array(N),R=new Float32Array(N);
 for(const[,sp]of SPOREN){const t=T[sp];for(let i=0;i<N;i++){L[i]+=t.L[i];R[i]+=t.R[i]}}
 const loL=lop(L,120,SR),loR=lop(R,120,SR);
 for(let i=0;i<N;i++){const m2=(loL[i]+loR[i])*0.5;L[i]+=m2-loL[i];R[i]+=m2-loR[i]}
 const preset=$('sbMaster')?$('sbMaster').value:'club';
 const target=preset==='stream'?-14:-9,ceil=preset==='stream'?0.891:0.966;
 for(let i=0;i<N;i++){L[i]=Math.tanh(L[i]);R[i]=Math.tanh(R[i])}
 let ss=0;for(let i=0;i<N;i++)ss+=L[i]*L[i]+R[i]*R[i];
 const lufs=10*Math.log10(ss/(2*N)+1e-12)-0.691,g=Math.pow(10,(target-lufs)/20);
 for(let i=0;i<N;i++){L[i]=Math.tanh(L[i]*g/ceil)*ceil;R[i]=Math.tanh(R[i]*g/ceil)*ceil}
 LAST.L=L;LAST.R=R;
 if(window._pv){const pv=window._pv;window._pv=null;for(const v of pv){try{SONG.vibe(v)}catch(e){}}}
 st(LAST.totBars===8&&CUR!==SECTIES?'👂 snel-preview klaar (8 maten) — druk 🎬 voor de volledige song':'✅ klaar — '+LAST.totBars+' maten · '+preset.toUpperCase()+' '+target+' LUFS · ~'+Math.round(N/SR)+'s');
 $('sbExport').style.display='flex';
 if($('sbAu')){const b=new Blob([wavBytes(LAST.L,LAST.R,SR)],{type:'audio/wav'});$('sbAu').src=URL.createObjectURL(b)} // uit LAST — dan speelt de player ook de vibe-bewerkte mix af na _pv-replay
}
/* ---------- exports ---------- */
function dlBlob(b,fn){const u=URL.createObjectURL(b),a=document.createElement('a');a.href=u;a.download=fn;a.click();setTimeout(()=>URL.revokeObjectURL(u),5000);return u}
async function expMaster(){if(!LAST)return;dlBlob(new Blob([wavBytes(LAST.L,LAST.R,SR)],{type:'audio/wav'}),'g2b-song-master.wav')}
async function expStems(){
 const files=[];
 for(const[nm,sp]of SPOREN){const t=LAST.T[sp];
  files.push({naam:'g2b-song_'+sp+'.wav',data:new Uint8Array(wavBytes(t.L,t.R,SR))})}
 dlBlob(new Blob([zipStore(files)],{type:'application/zip'}),'g2b-song-stems.zip')}
function midBytes(){const noten=[];
 try{
  /* arrangement-uitwerking: elke loop wordt getild over de secties waar zijn vakje aan staat */
  const tile=(src,cell,loopSteps,map)=>{const out=[];let bar=0;
   for(const[sec,m]of SECTIES){if(AAN[sec][cell])for(let rep=0;rep<m*16;rep+=loopSteps)
    for(const n of src){const s2=bar*16+rep+n.s;if(s2<(bar+m)*16)out.push(map(n,s2))}
    bar+=m}return out};
  const loopD=Math.round((LAST.matD||4)*16),loopM=Math.round((LAST.matM||4)*16);
  noten.push({naam:'bas',kanaal:0,noten:tile(DW.S.notes||[],'sub',loopD,(n,s)=>({p:n.p,s,d:n.d,v:n.v||80}))});
  noten.push({naam:'kick',kanaal:9,noten:tile(DW.S.kicks||[],'kick',loopD,(k,s)=>({p:36,s,d:1,v:100}))});
  noten.push({naam:'melodie',kanaal:1,noten:tile(MW.S.mel||[],'mel',loopM,(n,s)=>({p:n.p,s,d:n.d,v:n.v||80}))});
  const akkSrc=[];(MW.S.akk||[]).forEach(c=>{(c.ns||c.notes||[c]).forEach(p=>akkSrc.push({p:p.p!==undefined?p.p:p,s:c.s||0,d:c.d||4}))});
  noten.push({naam:'akkoorden',kanaal:2,noten:tile(akkSrc,'akk',loopM,(n,s)=>({p:n.p,s,d:n.d,v:70}))});
 }catch(e){console.error(e)}
 const secs=SECTIES.reduce((a,[nm,m],i)=>{a.push({naam:nm,bar:a._b||0});a._b=(a._b||0)+m;return a},[]).map(s=>({naam:s.naam,bar:s.bar}));
 return midiSong(noten,LAST.bpm,secs)}
async function expMid(){
 if(!LAST){st('⚙ bouw eerst een song (🎬)');return}
 dlBlob(new Blob([midBytes()],{type:'audio/midi'}),'g2b-song.mid')}
/* 1-klik RELEASE-PACK: master + alle stems + .mid + licentie in één zip */
async function expPack(){
 if(!LAST){st('⚙ bouw eerst een song (🎬)');return}
 const files=[{naam:'g2b-song_MASTER.wav',data:new Uint8Array(wavBytes(LAST.L,LAST.R,SR))}];
 for(const[nm,sp]of SPOREN){const t=LAST.T[sp];
  files.push({naam:'stems/g2b-song_'+sp+'.wav',data:new Uint8Array(wavBytes(t.L,t.R,SR))})}
 try{files.push({naam:'g2b-song.mid',data:new Uint8Array(midBytes())})}catch(e){console.warn('midi overgeslagen',e)}
 files.push({naam:'LICENTIE.txt',data:new TextEncoder().encode(
  'G2B Werkstation — RELEASE-PACK\n\nAlle audio in deze zip is 100% door jouw eigen engines en/of samples gemaakt.\nJij bezit alle rechten — commercieel gebruik, verkoop en streaming toegestaan.\nGeen derden-samples, geen copyright-claims.\n\nGood2Better High Quality Media · KvK 95199926')});
 dlBlob(new Blob([zipStore(files)],{type:'application/zip'}),'g2b-song-RELEASE-PACK.zip');
 st('📦 RELEASE-PACK gedownload — master + '+SPOREN.length+' stems + midi + licentie in één zip')}
/* ---------- .G2B universeel project-bestand ---------- */
const _safeAssign=(dst,src)=>{for(const k of Object.keys(src||{}))if(k!=='__proto__'&&k!=='constructor')dst[k]=src[k]};
const g2bSel=(w,ids)=>{const o={};try{for(const i of ids){const el=w.document.getElementById(i);
  if(el)o[i]=el.type==='checkbox'?!!el.checked:el.value}}catch(e){}return o};
async function g2bSave(){
 if(!DW||!MW){st('⚙ eerst 🎬 BOUW (dan laden de engines mee)');return}
 const d={v:1,ts:Date.now()};
 d.donder={ui:g2bSel(DW,['genre','bpm','key','maten','bas','sound','laag','dubbel','stap']),
  S:{notes:DW.S.notes,kicks:DW.S.kicks,locks:DW.S.locks,sound:DW.S.sound,mute:DW.S.mute}};
 d.melodie={ui:g2bSel(MW,['genre','bpm','maten','ladder','wortel','klank','voicing','arp','strum','hum','mstapv','laag']),
  S:{mel:MW.S.mel,akk:MW.S.akk,bas:MW.S.bas},AAN:MW.AAN,VOL:MW.VOL,LOCK:MW.LOCK};
 d.matrix=AAN;d.master=$('sbMaster')?$('sbMaster').value:'club';
 d.vibes=LAST&&LAST.vibes?LAST.vibes.slice():[];
 const jb=new TextEncoder().encode(JSON.stringify(d));
 const voc=VOC?new Uint8Array(wavBytes(VOC.L,VOC.R,SR)):null;
 const all=new Uint8Array(8+jb.length+(voc?voc.length:0));const dv=new DataView(all.buffer);
 all.set([0x47,0x32,0x42,0x21],0);dv.setUint32(4,jb.length,true);all.set(jb,8);
 if(voc)all.set(voc,8+jb.length);
 dlBlob(new Blob([all],{type:'application/octet-stream'}),'g2b-project.g2b');
 st('💾 project opgeslagen als .g2b — neem hem mee naar je PC-app of andere browser')}
async function g2bOpen(file){
 const ab=await file.arrayBuffer(),u=new Uint8Array(ab),dv=new DataView(ab);
 if(u.length<8||u[0]!==0x47||u[1]!==0x32||u[2]!==0x42){st('⚠ geen .g2b bestand');return}
 st('📂 .g2b lezen + engines laden…');await laadEngines();
 const jl=dv.getUint32(4,true),d=JSON.parse(new TextDecoder().decode(u.subarray(8,8+jl)));
 const put=(w,ui)=>{if(!ui)return;for(const i in ui){const el=w.document.getElementById(i);
  if(!el)continue;if(el.type==='checkbox')el.checked=!!ui[i];else el.value=ui[i]}};
 if(d.donder){put(DW,d.donder.ui);
  if(d.donder.S)for(const k of['notes','kicks','locks','sound','mute'])if(d.donder.S[k]!==undefined)DW.S[k]=d.donder.S[k]}
 if(d.melodie){put(MW,d.melodie.ui);
  if(d.melodie.S)for(const k of['mel','akk','bas'])if(d.melodie.S[k]!==undefined)MW.S[k]=d.melodie.S[k];
  if(d.melodie.AAN)_safeAssign(MW.AAN,d.melodie.AAN);
  if(d.melodie.VOL)_safeAssign(MW.VOL,d.melodie.VOL);if(d.melodie.LOCK)_safeAssign(MW.LOCK,d.melodie.LOCK)}
 if(d.matrix)for(const[sec]of SECTIES)for(const[,sp]of SPOREN)
  if(Object.prototype.hasOwnProperty.call(d.matrix,sec)&&Object.prototype.hasOwnProperty.call(d.matrix[sec],sp))AAN[sec][sp]=!!d.matrix[sec][sp];
 if(d.master&&$('sbMaster'))$('sbMaster').value=d.master;
 try{DW.teken&&DW.teken()}catch(e){}
 try{MW.teken&&MW.teken();MW.melsToetsen&&MW.melsToetsen()}catch(e){}
 document.querySelectorAll('.sbCell').forEach(b=>{const on=AAN[b.dataset.sec]&&AAN[b.dataset.sec][b.dataset.sp];
  b.style.background=on?'#22c55e':'#0a1812'});
 LAST=null;if($('sbAu'))$('sbAu').src='';
 window._pv=d.vibes||null;
 if(ab.byteLength>8+jl){try{const ac=window._vocAC||(window._vocAC=new (window.AudioContext||window.webkitAudioContext)());
  const buf=await ac.decodeAudioData(u.subarray(8+jl).slice(0).buffer);await laadVocal(buf,false)}catch(e){console.warn(e)}}
 else{VOC=null;if($('sbVocInfo'))$('sbVocInfo').textContent=''}
 st('📂 .g2b geladen — druk 🎬 BOUW om je song terug te renderen')}
/* ---------- vibe-fixers ---------- */
window.SONG={
 save:g2bSave,open:g2bOpen,
 bouw,
 /* 🎤 vocal laden (File of AudioBuffer) */
 vocal:laadVocal,
 /* ✨ MAAK HOOK: alleen de melodie opnieuw (catchy refrein), song herbouwen */
 hook(){try{MW&&MW.maak&&MW.maak('mel')}catch(e){}bouw()},
 /* 🔀 FLIP GENRE: zelfde vocal+key, ander genre → herbouwen */
 flip(g){try{const d=DW&&DW.document&&DW.document.getElementById('genre');
  if(d){d.value=g;DW.dobbelAlles&&DW.dobbelAlles()}bouw()}catch(e){bouw()}},
 /* 📱 HOOK-SNIPPET: sterkste 8 maten (DROP) als 15-30s sociale clip */
 snip(){if(!LAST){st('⚙ bouw eerst een song (🎬)');return}
  const sdB=LAST.sdD*16*SR,a=12*sdB,b=Math.min(LAST.N,20*sdB);
  const L=LAST.L.slice(a,b),R=LAST.R.slice(a,b),n=L.length,f=Math.round(.4*SR);
  for(let i=0;i<f&&i<n;i++){const g=i/f;L[i]*=g;R[i]*=g}
  for(let i=0;i<f&&i<n;i++){const g=i/f;L[n-1-i]*=g;R[n-1-i]*=g}
  let pk=0;for(let i=0;i<n;i++)pk=Math.max(pk,Math.abs(L[i]),Math.abs(R[i]));
  const g=pk>0?.9/pk:1;for(let i=0;i<n;i++){L[i]=Math.tanh(L[i]*g);R[i]=Math.tanh(R[i]*g)}
  dlBlob(new Blob([wavBytes(L,R,SR)],{type:'audio/wav'}),'g2b-hook-snippet.wav');
  st('📱 hook-snippet gedownload — '+Math.round(n/SR)+'s, fade in/out, klaar voor socials')},
 vibe(k){
  if(!LAST){st('⚙ bouw eerst een song (🎬)');return}
  if(k==='draai'){try{DW.dobbelAlles&&DW.dobbelAlles();MW.maak&&MW.maak('mel')}catch(e){}bouw();return}
  const{T,N}=LAST,sd=LAST.sdD;
  if(k==='grit'){ // +15% grit/sub-druk
   const t=T.grit;for(let i=0;i<N;i++){t.L[i]=Math.tanh(t.L[i]*1.35);t.R[i]=Math.tanh(t.R[i]*1.35)}}
  if(k==='groove'){ // +8% swing: elke 2e 16e iets later (op mel/akk/sub)
   const seg=Math.round(sd*SR),off=Math.round(sd*SR*.08);
   for(const sp of['mel','akk','sub'])for(const ch of['L','R']){
    const src=T[sp][ch].slice();
    for(let s=1;s*seg<N;s+=2){
     const b0=s*seg;
     // verschuif oneven 16e `off` later; vul de gat met de staart van de vorige (geen gaten, geen dubbels)
     for(let i=0;i<off&&b0+i<N;i++)T[sp][ch][b0+i]=src[b0-off+i];
     for(let i=off;i<seg&&b0+i<N;i++)T[sp][ch][b0+i]=src[b0+i-off]}}}
  if(k==='rustig'){ // -30% drukke noten: dempte elke 2e 16e op mel/akk
   const seg=Math.round(sd*SR);
   for(const sp of['mel','akk'])for(const ch of['L','R']){
    for(let s=0;s*seg<N;s++)if(s%2===1)for(let i=s*seg;i<(s+1)*seg&&i<N;i++)T[sp][ch][i]*=.55}}
  mixDown();
  if($('sbAu')){const b=new Blob([wavBytes(LAST.L,LAST.R,SR)],{type:'audio/wav'});$('sbAu').src=URL.createObjectURL(b)}
  if(!LAST.vibes)LAST.vibes=[];if(!LAST.vibes.includes(k))LAST.vibes.push(k);
  st('✨ vibe-fixer toegepast — luister + exporteer opnieuw als hij goed is')}};
/* ---------- UI ---------- */
function uiBoot(){
 const card=$('songBouwer');if(!card)return;
 /* matrix */
 const grid=$('sbMatrix');
 let h='<table style="border-collapse:collapse;font-size:11px"><tr><td></td>';
 for(const[s]of SECTIES)h+='<td style="writing-mode:vertical-rl;padding:4px;color:#7ee2a8;font-weight:700">'+s+'</td>';
 h+='</tr>';
 for(const[nm,sp]of SPOREN){h+='<tr><td style="padding:3px 8px 3px 0;color:#9fd8b4;font-weight:700">'+nm+'</td>';
  for(const[sec]of SECTIES){const on=AAN[sec][sp];
   h+='<td style="text-align:center"><button data-sec="'+sec+'" data-sp="'+sp+'" class="sbCell" style="width:26px;height:26px;border-radius:6px;border:1px solid #1d4a33;cursor:pointer;background:'+(on?'#22c55e':'#0a1812')+'"></button></td>'}
  h+='</tr>'}
 grid.innerHTML=h+'</table>';
 grid.querySelectorAll('.sbCell').forEach(b=>b.onclick=()=>{const s=b.dataset.sec,sp=b.dataset.sp;AAN[s][sp]=AAN[s][sp]?0:1;b.style.background=AAN[s][sp]?'#22c55e':'#0a1812'});
 $('sbBouw').onclick=()=>bouw(false);
 const pv=$('sbPrev');if(pv)pv.onclick=()=>bouw(true);
 $('sbWav').onclick=expMaster;$('sbStem').onclick=expStems;$('sbMid').onclick=expMid;
 const pk=$('sbPack');if(pk)pk.onclick=expPack;
 const sv=$('sbSave');if(sv)sv.onclick=g2bSave;
 const op=$('sbOpenFile');if(op)op.onchange=()=>{if(op.files&&op.files[0])g2bOpen(op.files[0]);op.value=''};
 const opb=$('sbOpen');if(opb)opb.onclick=()=>op&&op.click();
 /* .g2b drag & drop op de bouwer-kaart */
 const sc=$('songBouwer');
 if(sc){sc.addEventListener('dragover',e=>{e.preventDefault();sc.style.outline='2px dashed #22c55e'});
  sc.addEventListener('dragleave',()=>sc.style.outline='');
  sc.addEventListener('drop',e=>{e.preventDefault();sc.style.outline='';
   const f=e.dataTransfer&&e.dataTransfer.files&&e.dataTransfer.files[0];
   if(f&&/\.g2b$/i.test(f.name))g2bOpen(f);else if(f)st('⚠ sleep een .g2b bestand')})}
 /* ⠿ sleep-naar-DAW grepen op alle exports */
 [['sbWav','audio/wav','g2b-song-master.wav',()=>new Blob([wavBytes(LAST.L,LAST.R,SR)])],
  ['sbMid','audio/midi','g2b-song.mid',()=>new Blob([midiSong([],120,[])])]].forEach(([id,mt,fn,mk])=>{
  const el=$(id);if(!el)return;el.draggable=true;
  el.addEventListener('dragstart',e=>{if(!LAST)return;const u=URL.createObjectURL(mk());
   e.dataTransfer.setData('DownloadURL',mt+':'+fn+':'+u)})});
 document.querySelectorAll('.sbVibe').forEach(b=>b.onclick=()=>SONG.vibe(b.dataset.k));
 /* ---------- v5.0 vocal & pro-knoppen ---------- */
 const dz=$('sbVocalDrop'),fi=$('sbVocalFile');
 if(dz){dz.ondragover=e=>{e.preventDefault();dz.style.borderColor='#22c55e'};
  dz.ondragleave=()=>dz.style.borderColor='';
  dz.ondrop=e=>{e.preventDefault();dz.style.borderColor='';
   const f=e.dataTransfer.files&&e.dataTransfer.files[0];if(f)window.SONG.vocal(f)};
  dz.onclick=()=>fi&&fi.click()}
 if(fi)fi.onchange=()=>{if(fi.files&&fi.files[0])window.SONG.vocal(fi.files[0])};
 const mic=$('sbVocalMic');
 if(mic){let rec=null;
  mic.onclick=async()=>{
   if(rec&&rec.state==='recording'){rec.stop();return}
   try{
    const ms=await navigator.mediaDevices.getUserMedia({audio:true});
    const ch=[];rec=new MediaRecorder(ms);
    rec.ondataavailable=e=>ch.push(e.data);
    rec.onstop=async()=>{ms.getTracks().forEach(t=>t.stop());mic.textContent='🎙️ opnemen';
     const blob=new Blob(ch,{type:rec.mimeType||'audio/webm'});window.SONG.vocal(blob);rec=null};
    st('🎙 neem je hook op… klik opnieuw om te stoppen');
    mic.textContent='■ STOP';rec.start();
   }catch(e){st('❌ microfoon niet bereikbaar: '+e.message)}}}
 const hk=$('sbHook');if(hk)hk.onclick=()=>SONG.hook();
 const fl=$('sbFlip');if(fl)fl.onchange=()=>SONG.flip(+fl.value);
 const sn=$('sbSnip');if(sn)sn.onclick=()=>SONG.snip();
 /* DAW sleep-grepen */
 const drag=(id,fn,ext,mt)=>{const el=$(id);if(!el)return;el.draggable=true;
  el.addEventListener('dragstart',e=>{const files=[{naam:'g2b-song'+ext,data:new Uint8Array(fn())}];
   const blob=new Blob([ext==='.zip'?zipStore(files):files[0].data.buffer||files[0].data],{type:mt});
   const url=URL.createObjectURL(blob);
   e.dataTransfer.setData('DownloadURL',mt+':g2b-song'+ext+':'+url)})};
 drag('sbWav',()=>wavBytes(LAST.L,LAST.R,SR),'.wav','audio/wav');
}
if(document.body)uiBoot();else window.addEventListener('DOMContentLoaded',uiBoot);
})();
