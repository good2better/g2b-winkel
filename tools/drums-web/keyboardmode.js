// G2B KEYBOARD-MODUS — gedeeld door de 8 instrumenten.
// 🎹 KEYBOARD aan = pad-klik SELECTEERT de sound (speelt niet af);
// je midi-keyboard speelt die sound dan over het HELE keyboard
// (noot 60 = normale toonhoogte). ● REC neemt je spel op, ⬇ .mid
// exporteert de opname — sleep die in je DAW en neem er direct op.
(function(){
 window.G2BKB={on:false};
 let sel=-1,rec=false,take=[],t0=0;
 const st=document.createElement('style');st.textContent=
 '.pad.sel{outline:3px solid #4cc9f0;outline-offset:-3px;box-shadow:0 0 22px #4cc9f088}'+
 '#kbbar{position:fixed;right:10px;bottom:8px;display:flex;gap:6px;z-index:99}'+
 '#kbbar button{background:#131c2e;color:#e8eef8;border:1px solid #22314d;border-radius:8px;padding:7px 11px;font-size:.72rem;font-weight:700;cursor:pointer}'+
 '#kbbar button.on{background:#16a34a;color:#04120a;border-color:#16a34a}'+
 '#kbbar button.rec{background:#7a1d1d;color:#ffd6d6;border-color:#a04040}';
 document.head.appendChild(st);
 const bar=document.createElement('div');bar.id='kbbar';
 const bKb=document.createElement('button');bKb.textContent='🎹 KEYBOARD';bKb.title='aan = klik een pad om de sound te kiezen, speel hem op je midi-keyboard';
 const bRec=document.createElement('button');bRec.textContent='● REC';bRec.style.display='none';bRec.title='neem je keyboard-spel op';
 const bDl=document.createElement('button');bDl.textContent='⬇ .mid';bDl.style.display='none';bDl.title='download je opname — sleep in je DAW';
 bar.append(bKb,bRec,bDl);document.body.appendChild(bar);
 function padIdx(el){if(el.id&&el.id.startsWith('p'))return +el.id.slice(1);const a=[...document.querySelectorAll('.pad')];return a.indexOf(el)}
 document.addEventListener('pointerdown',e=>{
  if(!G2BKB.on)return;
  const p=e.target.closest&&e.target.closest('.pad');if(!p)return;
  e.stopPropagation();e.preventDefault();
  const i=padIdx(p);if(i<0)return;sel=i;
  document.querySelectorAll('.pad.sel').forEach(x=>x.classList.remove('sel'));
  p.classList.add('sel');p.classList.add('hit');setTimeout(()=>p.classList.remove('hit'),140);
 },true);
 bKb.onclick=()=>{G2BKB.on=!G2BKB.on;bKb.classList.toggle('on',G2BKB.on);
  bRec.style.display=bDl.style.display=G2BKB.on?'':'none';
  if(!G2BKB.on){document.querySelectorAll('.pad.sel').forEach(x=>x.classList.remove('sel'));sel=-1}
  bKb.textContent=G2BKB.on?'🎹 KIES EEN PAD':'🎹 KEYBOARD';};
 bRec.onclick=()=>{rec=!rec;take=[];t0=performance.now();bRec.classList.toggle('on',rec);
  bRec.textContent=rec?'■ STOP':'● REC';};
 // hooks die midi.js aanroept bij elke noot-aan / noot-uit
 window.onMidiNote=function(n,i,v){if(!G2BKB.on)return;
  if(sel>=0){try{speel(sel,n-60)}catch(e){}}
  if(rec&&sel>=0)take.push({n:n,t:performance.now()-t0,v:v||100});};
 window.onMidiOff=function(n){if(rec)take.push({n:n,t:performance.now()-t0,v:0});};
 function vlq(x){const b=[x&127];while(x>>=7)b.unshift((x&127)|128);return b}
 bDl.onclick=()=>{
  const open={},ev=[];
  take.sort((a,b)=>a.t-b.t).forEach(e=>{
   if(e.v>0){(open[e.n]=open[e.n]||[]).push(e)}
   else if(open[e.n]&&open[e.n].length){const on=open[e.n].shift();
    ev.push([on.t|0,0x90,on.n,on.v],[e.t|0,0x80,e.n,0]);}});
  const now=performance.now()-t0;
  for(const n in open)open[n].forEach(on=>ev.push([on.t|0,0x90,on.n,on.v],[now|0,0x80,+n,0]));
  ev.sort((a,b)=>a[0]-b[0]);
  const trk=[0,0xFF,0x51,3,0x07,0xA1,0x20]; // tempo 500000 us -> 1 tick(1/500ppq) = 1 ms
  let last=0;for(const e of ev){trk.push(...vlq(e[0]-last),e[1],e[2],e[3]);last=e[0]}
  trk.push(0,0xFF,0x2F,0);
  const hdr=[0x4D,0x54,0x68,0x64,0,0,0,6,0,0,0,1,0x01,0xF4,0x4D,0x54,0x72,0x6B,
   (trk.length>>24)&255,(trk.length>>16)&255,(trk.length>>8)&255,trk.length&255];
  const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([new Uint8Array(hdr),new Uint8Array(trk)],{type:'audio/midi'}));
  a.download='g2b-opname.mid';a.click();};
})();
