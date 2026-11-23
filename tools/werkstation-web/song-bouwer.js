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
const SPOREN=[['KICK','kick'],['SUB','sub'],['GRIT','grit'],['MELODIE','mel'],['AKKOORDEN','akk']];
const DEFAAN={INTRO:{mel:1,akk:1},BUILD:{grit:1,mel:1,akk:1},DROP:{kick:1,sub:1,grit:1,mel:1,akk:1},BREAK:{mel:1,akk:1},'DROP 2':{kick:1,sub:1,grit:1,mel:1,akk:1}};
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
function mkFrame(src){return new Promise((res,rej)=>{const f=document.createElement('iframe');f.style.cssText='width:0;height:0;border:0;position:absolute;left:-9999px';f.src=src;f.onload=()=>res(f);f.onerror=rej;document.body.appendChild(f)})}
async function laadEngines(){
 if(DW&&MW)return;
 st('⚙ engines laden (basfabriek + melodie)…');
 const fd=await mkFrame('/tools/basfabriek/index.html'),fm=await mkFrame('/tools/melodie/index.html');
 DW=fd.contentWindow;MW=fm.contentWindow;
 // wacht tot engines geboot zijn (dobbelAlles geeft meteen een patroon)
 for(let i=0;i<80&&!(DW&&DW.renderLoop&&DW.S&&DW.S.notes&&DW.S.notes.length);i++)await new Promise(r=>setTimeout(r,100));
 for(let i=0;i<80&&!(MW&&MW.renderSpoorBuf&&MW.S&&MW.S.mel&&MW.S.mel.length);i++)await new Promise(r=>setTimeout(r,100));
 if(!DW.renderLoop)throw new Error('basfabriek-engine niet gevonden');
 if(!MW.renderSpoorBuf)throw new Error('melodie-engine niet gevonden')}
/* ---------- stem-tile naar 8 maten ---------- */
function tile8(mono,sd,maten){ // mono Float32Array van `maten` maten → 8 maten
 const bar=Math.round(sd*16*SR);let out=mono;
 if(maten>=8)return out.subarray(0,8*bar);
 const o=new Float32Array(8*bar),n=Math.round(maten*bar);
 const seg=mono.subarray(0,Math.min(n,mono.length));
 for(let b=0;b*n<8*bar;b++){const len=Math.min(seg.length,8*bar-b*n);o.set(seg.subarray(0,len),b*n)}
 return o}
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
 let off=0;const cent=[],parts=files.map(f=>{const nb=enc(f.naam),crc=crc32(f.data),sz=f.data.length;
  const hdr=[0x50,0x4B,3,4,20,0,0,0,0,0,0,0,0,0,0,0,crc&255,(crc>>8)&255,(crc>>16)&255,(crc>>24)&255,
   sz&255,(sz>>8)&255,(sz>>16)&255,(sz>>24)&255,sz&255,(sz>>8)&255,(sz>>16)&255,(sz>>24)&255,
   nb.length&255,(nb.length>>8)&255,0,0];
  cent.push([nb,off,crc,sz]);off+=hdr.length+nb.length+sz;return[...hdr,...nb]});
 const cd=[];cent.forEach(([nb,o,crc,sz])=>{cd.push(0x50,0x4B,1,2,20,0,20,0,0,0,0,0,0,0,0,0,0,0,
  crc&255,(crc>>8)&255,(crc>>16)&255,(crc>>24)&255,sz&255,(sz>>8)&255,(sz>>16)&255,(sz>>24)&255,
  sz&255,(sz>>8)&255,(sz>>16)&255,(sz>>24)&255,nb.length&255,(nb.length>>8)&255,0,0,0,0,0,0,0,0,0,0,0,0,
  o&255,(o>>8)&255,(o>>16)&255,(o>>24)&255,...nb)});
 const all=new Uint8Array(off+cd.length+22);let p=0;
 parts.forEach(a=>{all.set(a,p);p+=a.length});all.set(cd,p);p+=cd.length;
 const e=[0x50,0x4B,5,6,0,0,0,0,cent.length&255,(cent.length>>8)&255,cent.length&255,(cent.length>>8)&255,
  cd.length&255,(cd.length>>8)&255,(cd.length>>16)&255,(cd.length>>24)&255,off&255,(off>>8)&255,(off>>16)&255,(off>>24)&255,0,0];
 all.set(e,p);return all}
function vlq(n){const b=[n&127];n>>=7;while(n){b.unshift((n&127)|128);n>>=7}return b}
function midiSong(sporen,bpm,secs){ // sporen:[{naam,noten:[{p,s,d,v}],kanaal}] s=16e-steps; secs: [{naam,bar}]
 const us=Math.round(6e7/bpm),tpq=96;
 const meta=[[0,[0xFF,0x51,3,(us>>16)&255,(us>>8)&255,us&255]],
  ...secs.map(s=>[s.bar*4*tpq,[0xFF,0x06,s.naam.length,...[...s.naam].map(c=>c.charCodeAt(0))]]),
  [0,[0xFF,0x2F,0]]];
 const mTrk=ev=>{const t=ev.slice().sort((a,b)=>a[0]-b[0]);let last=0;const out=[];
  for(const[tick,e]of t){out.push(...vlq(tick-last),...e);last=tick}
  const b=new Uint8Array(8+out.length);const d=new DataView(b.buffer);
  b.set([0x4D,0x54,0x72,0x6B],0);d.setUint32(4,out.length);b.set(out,8);return b};
 const trks=[mTrk(meta)];
 for(const sp of sporen){const ev=[];
  for(const n of sp.noten){const t=Math.round(n.s/4*tpq),e=Math.round((n.s+n.d)/4*tpq);
   ev.push([t,[0x90|sp.kanaal,n.p,n.v&127]],[e,[0x80|sp.kanaal,n.p,0]])}
  ev.push([0,[0xFF,0x2F,0]]);trks.push(mTrk(ev))}
 const h=new Uint8Array(14);const dh=new DataView(h.buffer);
 h.set([0x4D,0x54,0x68,0x64],0);dh.setUint32(4,6);dh.setUint16(8,1);dh.setUint16(10,trks.length);dh.setUint16(12,tpq);
 const tot=trks.reduce((a,t)=>a+t.length,0),out=new Uint8Array(14+tot);out.set(h,0);let o=14;
 for(const t of trks){out.set(t,o);o+=t.length}return out}
/* ---------- mix ---------- */
let LAST=null;
async function bouw(){
 try{
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
 const totBars=SECTIES.reduce((a,[,m])=>a+m,0),N=totBars*barD;
 const T={};for(const[,sp]of SPOREN){T[sp]={L:new Float32Array(N),R:new Float32Array(N)}}
 st('⚙ arrangement + FX plaatsen…');
 let bar=0;
 for(const[sec,m]of SECTIES){
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
  /* FX */
  if(sec==='BUILD'){const rz=riser(m*4*sdD,SR);for(let i=0;i<rz.length&&off*barD+i<N;i++){T.grit.L[off*barD+i]+=rz[i]*.4;T.grit.R[off*barD+i]+=rz[i]*.4}}
  if(isDrop){const cr=crash(SR);for(let i=0;i<cr.length&&off*barD+i<N;i++){T.kick.L[off*barD+i]+=cr[i];T.kick.R[off*barD+i]+=cr[i]}}
  bar+=m}
 st('⚙ MENGER: auto-balance + vocal-ruimte + mono<120Hz…');
 LAST={T,N,bpm:+(DW.S&&DW.S.bpm)||120,sdD,totBars};
 mixDown();
 }catch(e){console.error(e);st('❌ '+e.message)}
 return}
/* mix T → L/R: auto-balance + vocal-dip + mono<120Hz + master (ook na vibe-fixers) */
function mixDown(){
 const{T,N}=LAST;
 for(const[,sp]of SPOREN){const t=T[sp];let pk=0;for(let i=0;i<N;i++){const a=Math.abs(t.L[i]),b=Math.abs(t.R[i]);if(a>pk)pk=a;if(b>pk)pk=b}
  if(pk>0){const g=Math.min(1,0.5/pk);for(let i=0;i<N;i++){t.L[i]*=g;t.R[i]*=g}}}
 for(const sp of['mel','grit']){T[sp].L=bell(T[sp].L,2500,1.2,-2.5,SR);T[sp].R=bell(T[sp].R,2500,1.2,-2.5,SR)}
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
 st('✅ klaar — '+LAST.totBars+' maten · '+preset.toUpperCase()+' '+target+' LUFS · ~'+Math.round(N/SR)+'s');
 $('sbExport').style.display='flex';
 if($('sbAu')){const b=new Blob([wavBytes(L,R,SR)],{type:'audio/wav'});$('sbAu').src=URL.createObjectURL(b)}
}
/* ---------- exports ---------- */
function dlBlob(b,fn){const u=URL.createObjectURL(b),a=document.createElement('a');a.href=u;a.download=fn;a.click();setTimeout(()=>URL.revokeObjectURL(u),5000);return u}
async function expMaster(){if(!LAST)return;dlBlob(new Blob([wavBytes(LAST.L,LAST.R,SR)],{type:'audio/wav'}),'g2b-song-master.wav')}
async function expStems(){
 const files=[];
 for(const[nm,sp]of SPOREN){const t=LAST.T[sp];
  files.push({naam:'g2b-song_'+sp+'.wav',data:new Uint8Array(wavBytes(t.L,t.R,SR))})}
 dlBlob(new Blob([zipStore(files)],{type:'application/zip'}),'g2b-song-stems.zip')}
async function expMid(){
 const noten=[];
 try{const sd=LAST.sdD;
  noten.push({naam:'bas',kanaal:0,noten:(DW.S.notes||[]).map(n=>({p:n.p,s:n.s,d:n.d,v:n.v||80}))});
  noten.push({naam:'kick',kanaal:9,noten:(DW.S.kicks||[]).map(k=>({p:36,s:k.s!==undefined?k.s:k,d:1,v:100}))});
  noten.push({naam:'melodie',kanaal:1,noten:(MW.S.mel||[]).map(n=>({p:n.p,s:n.s,d:n.d,v:n.v||80}))});
  const akkN=[];(MW.S.akk||[]).forEach(c=>{(c.ns||c.notes||[c]).forEach(p=>akkN.push({p:p.p!==undefined?p.p:p,s:c.s||0,d:c.d||4,v:70}))});
  noten.push({naam:'akkoorden',kanaal:2,noten:akkN});
 }catch(e){}
 const secs=SECTIES.reduce((a,[nm,m],i)=>{a.push({naam:nm,bar:a._b||0});a._b=(a._b||0)+m;return a},[]).map(s=>({naam:s.naam,bar:s.bar}));
 dlBlob(new Blob([midiSong(noten,LAST.bpm,secs)],{type:'audio/midi'}),'g2b-song.mid')}
/* ---------- vibe-fixers ---------- */
window.SONG={
 bouw, vibe(k){
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
 $('sbBouw').onclick=bouw;
 $('sbWav').onclick=expMaster;$('sbStem').onclick=expStems;$('sbMid').onclick=expMid;
 /* ⠿ sleep-naar-DAW grepen op alle exports */
 [['sbWav','audio/wav','g2b-song-master.wav',()=>new Blob([wavBytes(LAST.L,LAST.R,SR)])],
  ['sbMid','audio/midi','g2b-song.mid',()=>new Blob([midiSong([],120,[])])]].forEach(([id,mt,fn,mk])=>{
  const el=$(id);if(!el)return;el.draggable=true;
  el.addEventListener('dragstart',e=>{if(!LAST)return;const u=URL.createObjectURL(mk());
   e.dataTransfer.setData('DownloadURL',mt+':'+fn+':'+u)})});
 document.querySelectorAll('.sbVibe').forEach(b=>b.onclick=()=>SONG.vibe(b.dataset.k));
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
