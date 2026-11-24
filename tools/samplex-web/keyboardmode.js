// G2B KEYBOARD-MODUS v2 — gedeeld door de browser-instrumenten.
// 🎹 KEYBOARD aan = pad-klik SELECTEERT de sound (blauw licht) — speel hem op je
// midi-keyboard OF laptop-toetsen (A W S E D F T G Y H U J K = piano, Z..M octaaf lager,
// [ ] = octaaf schuiven). ● REC telt 3-2-1 af en toont LIVE wat je speelt op een
// mini-grit (noot-hoogte × tijd). 🔁 LOOP speelt je take rond. ⬇ .mid = kopers.
(function(){
 window.G2BKB={on:false};
 const sel=new Set();let rec=false,take=[],t0=0,counting=false;
 let mode='norm'; // norm | akkoord | octaaf | arp
 const st=document.createElement('style');st.textContent=
 '.pad.sel{outline:3px solid #4cc9f0;outline-offset:-3px;box-shadow:0 0 22px #4cc9f088}'+
 '#kbbar{position:fixed;right:10px;bottom:8px;display:flex;gap:6px;z-index:99;align-items:center}'+
 '#kbbar button{background:#131c2e;color:#e8eef8;border:1px solid #22314d;border-radius:8px;padding:7px 11px;font-size:.72rem;font-weight:700;cursor:pointer}'+
 '#kbbar button.on{background:#16a34a;color:#04120a;border-color:#16a34a}'+
 '#kbbar button.rec{background:#7a1d1d;color:#ffd6d6;border-color:#a04040}'+
 '#kbbar button.mode.on{background:#4cc9f0;color:#04120a;border-color:#4cc9f0}'+
 '#kbcnt{position:fixed;inset:0;display:none;align-items:center;justify-content:center;z-index:98;pointer-events:none}'+
 '#kbcnt span{font:900 130px/1 system-ui;color:#19c37d;text-shadow:0 0 40px #19c37d88}'+
 '#kbgrid{position:fixed;left:50%;transform:translateX(-50%);bottom:8px;display:none;gap:4px;z-index:97}'+
 '#kbgrid div{width:26px;height:14px;border-radius:4px;background:#131c2e;border:1px solid #22314d;display:flex;align-items:center;justify-content:center;font:700 9px/1 system-ui;color:#4cc9f0}'+
 '#kbgrid div.on{background:#16a34a;border-color:#16a34a;color:#04120a}'+
 '#kbgrid div.maat{border-color:#4cc9f0}'+
 '#kbstrip{position:fixed;left:8px;bottom:8px;display:none;z-index:97;border:1px solid #22314d;border-radius:8px;background:#0b1220dd}';
 document.head.appendChild(st);
 const bar=document.createElement('div');bar.id='kbbar';
 const mk=(t,cls)=>{const b=document.createElement('button');b.textContent=t;if(cls)b.className=cls;return b};
 const bKb=mk('🎹 KEYBOARD');bKb.title='aan = klik een pad (blauw) om de sound te kiezen, speel op midi- of laptop-toetsen';
 const bNorm=mk('NORMAAL','mode on');
 const bAkk=mk('AKKOORD','mode');bAkk.title='1 toets = majeur-akkoord (3 noten)';
 const bOct=mk('OCTAAF','mode');bOct.title='elke noot klinkt ook een octaaf hoger+lager';
 const bArp=mk('ARP','mode');bArp.title='houd een noot vast → hij rolt omhoog (arpeggio)';
 const bRec=mk('● REC','rec');bRec.style.display='none';bRec.title='neem je spel op — telt 3-2-1 af en toont het LIVE op het strip-raster';
 const bLoop=mk('🔁 LOOP');bLoop.style.display='none';bLoop.title='speelt je opname in een rondje af';
 const bDl=mk('⬇ .mid');bDl.style.display='none';bDl.title='download je opname — kopers (licentie)';
 bar.append(bKb,bNorm,bAkk,bOct,bArp,bRec,bLoop,bDl);document.body.appendChild(bar);
 const modeBtns=[bNorm,bAkk,bOct,bArp];
 const setMode=m=>{mode=m;modeBtns.forEach(b=>b.classList.toggle('on',
   (m==='norm'&&b===bNorm)||(m==='akkoord'&&b===bAkk)||(m==='octaaf'&&b===bOct)||(m==='arp'&&b===bArp)));
  if(m!=='arp'&&arpT){clearInterval(arpT);arpT=null;}};
 bNorm.onclick=()=>setMode('norm');bAkk.onclick=()=>setMode('akkoord');
 bOct.onclick=()=>setMode('octaaf');bArp.onclick=()=>setMode('arp');
 // --- live note-strip: toont wat je speelt tijdens REC (pitch × tijd) ---
 const strip=document.createElement('canvas');strip.id='kbstrip';strip.width=340;strip.height=64;document.body.appendChild(strip);
 const sg=strip.getContext('2d');let stripRaf=null;
 const stripDraw=()=>{
  const w=strip.width,h=strip.height,SPAN=8000; // 8 sec zicht
  sg.fillStyle='#0b1220';sg.fillRect(0,0,w,h);
  sg.strokeStyle='#22314d';for(let i=1;i<8;i++){sg.beginPath();sg.moveTo(i*w/8,0);sg.lineTo(i*w/8,h);sg.stroke();}
  const now=rec?performance.now()-t0:(take.length?take[take.length-1].t:0);
  const x1=Math.max(0,now-SPAN);
  take.forEach(e=>{if(e.t<x1||e.t>now)return;
   const x=(e.t-x1)/SPAN*w,y=h-((e.n-48)/48)*h;
   sg.fillStyle=e.v>0?'#19c37d':'#4cc9f0';sg.fillRect(x,Math.max(2,y-3),Math.max(4,w/60),5);});
  if(rec){sg.fillStyle='#19c37d';sg.fillRect(w-2,0,2,h);}
 };
 const stripStart=()=>{strip.style.display='block';const l=()=>{stripDraw();stripRaf=requestAnimationFrame(l)};l();};
 const stripStop=()=>{if(stripRaf)cancelAnimationFrame(stripRaf);stripRaf=null;stripDraw();};
 let looping=false,loopT=null;
 function loopStop(){looping=false;if(loopT)clearTimeout(loopT);loopT=null;bLoop.classList.remove('on');}
 bLoop.onclick=()=>{
  if(looping){loopStop();return;}
  if(!take.length){bLoop.title='neem eerst iets op met ● REC';setTimeout(()=>bLoop.title='speelt je opname in een rondje af',1800);return;}
  looping=true;bLoop.classList.add('on');
  const dur=take[take.length-1].t||1;
  const speelTake=()=>{if(!looping)return;
   take.forEach(e=>{if(e.v>0)setTimeout(()=>{if(looping)sel.forEach(s=>{try{speel(s,e.n-60)}catch(x){}})},e.t)});
   loopT=setTimeout(speelTake,dur+500);};
  speelTake();};
 const cnt=document.createElement('div');cnt.id='kbcnt';cnt.innerHTML='<span></span>';document.body.appendChild(cnt);
 // maat-raster: 4 cellen per maat × 4 maten, licht loopt mee op 120bpm
 const grid=document.createElement('div');grid.id='kbgrid';
 for(let i=0;i<16;i++){const d=document.createElement('div');d.textContent=(i%4)+1;if(i%4===0)d.classList.add('maat');grid.appendChild(d)}
 document.body.appendChild(grid);
 let gridTimer=null,gridTick=0;
 function gridStart(){gridTick=0;grid.style.display='flex';gridTickGo();}
 function gridTickGo(){gridTimer=setTimeout(()=>{if(grid.style.display==='none')return;
   grid.querySelectorAll('div').forEach((d,i)=>d.classList.toggle('on',i===gridTick%16));gridTick++;
   gridTickGo();},500)}
 function gridStop(){grid.style.display='none';if(gridTimer)clearTimeout(gridTimer);gridTimer=null;}
 function padIdx(el){if(el.id&&el.id.startsWith('p'))return +el.id.slice(1);const a=[...document.querySelectorAll('.pad')];return a.indexOf(el)}
 document.addEventListener('pointerdown',e=>{
  if(!G2BKB.on)return;
  const p=e.target.closest&&e.target.closest('.pad');if(!p)return;
  e.stopPropagation();e.preventDefault();
  const i=padIdx(p);if(i<0)return;
  if(sel.has(i)){sel.delete(i);p.classList.remove('sel');return;} // wegklikken = uit
  sel.add(i);p.classList.add('sel');p.classList.add('hit');setTimeout(()=>p.classList.remove('hit'),140);
 },true);
 bKb.onclick=()=>{G2BKB.on=!G2BKB.on;bKb.classList.toggle('on',G2BKB.on);
  bRec.style.display=bDl.style.display=G2BKB.on?'':'none';
  bNorm.style.display=bAkk.style.display=bOct.style.display=bArp.style.display=G2BKB.on?'':'none';
  bLoop.style.display=(G2BKB.on&&take.length)?'':'none';
  if(!G2BKB.on){document.querySelectorAll('.pad.sel').forEach(x=>x.classList.remove('sel'));sel.clear();stopCount();loopStop();
   if(arpT){clearInterval(arpT);arpT=null;}}
  bKb.textContent=G2BKB.on?'🎹 KIES EEN PAD':'🎹 KEYBOARD';};
 // --- aftel/count-in: 3-2-1 tikjes, daarna start de opname ---
 let cntTimer=null;
 function tik(hoog){
  try{const A=(window.G2BAC||(window.AudioContext&&new AudioContext())),g=A.createGain(),o=A.createOscillator();
   o.frequency.value=hoog?1200:750;o.type='square';g.gain.setValueAtTime(hoog?0.16:0.09,A.currentTime);
   g.gain.exponentialRampToValueAtTime(0.001,A.currentTime+0.07);o.connect(g);g.connect(A.destination);
   o.start();o.stop(A.currentTime+0.08);}catch(e){}}
 function stopCount(){counting=false;if(cntTimer){clearTimeout(cntTimer);cntTimer=null}cnt.style.display='none';gridStop();}
 function countIn(){
  counting=true;let n=3;
  const stap=()=>{if(!counting)return;
   if(n>0){cnt.firstChild.textContent=n;cnt.style.display='flex';tik(false);n--;cntTimer=setTimeout(stap,500);}
   else{cnt.style.display='none';tik(true);rec=true;take=[];t0=performance.now();gridStart();stripStart();
    bRec.classList.add('on');bRec.textContent='■ STOP';counting=false;}};
  stap();}
 bRec.onclick=()=>{
  if(counting){stopCount();bRec.textContent='● REC';return;}
  if(!rec){countIn();}
  else{rec=false;bRec.classList.remove('on');bRec.textContent='● REC';gridStop();stripStop();
   if(take.length)bLoop.style.display='';}};
 // --- ARP: vastgehouden noot rolt omhoog op 1/8 van 120bpm ---
 let arpT=null,arpHeld=new Set(),arpIdx=0;
 function arpGo(){if(arpT)return;
  arpT=setInterval(()=>{const h=[...arpHeld].sort((a,b)=>a-b);if(!h.length){clearInterval(arpT);arpT=null;return;}
   const n=h[arpIdx%h.length];arpIdx++;
   sel.forEach(s=>{try{speel(s,n-60)}catch(e){}});
   if(rec&&sel.size)take.push({n:n,t:performance.now()-t0,v:100});},125);}
 // hooks die midi.js aanroept bij elke noot-aan / noot-uit
 window.onMidiNote=function(n,i,v){if(!G2BKB.on)return;
  let notes=[n];
  if(mode==='akkoord')notes=[n,n+4,n+7];            // majeur-trieklank
  if(mode==='octaaf')notes=[n-12,n,n+12];          // octaaf-stapel
  if(mode==='arp'){arpHeld.add(n);notes=[n];arpGo();}
  notes.forEach(nn=>sel.forEach(s=>{try{speel(s,nn-60)}catch(e){}}));
  if(rec&&sel.size)notes.forEach(nn=>take.push({n:nn,t:performance.now()-t0,v:v||100}));};
 window.onMidiOff=function(n){if(mode==='arp'){arpHeld.delete(n);if(!arpHeld.size&&arpT){clearInterval(arpT);arpT=null;}}
  let notes=[n];
  if(mode==='akkoord')notes=[n,n+4,n+7];
  if(mode==='octaaf')notes=[n-12,n,n+12];
  if(rec)notes.forEach(nn=>take.push({n:nn,t:performance.now()-t0,v:0}));};
 // neem ook toetsenbord-noten mee in de opname
 window.G2BKB.log=function(n,semi,v){if(rec&&sel.size)take.push({n:n,t:performance.now()-t0,v:v||100});};
 // --- laptop-toetsenbord als piano (geen midi-keyboard nodig) ---
 const kmap={a:0,w:1,s:2,e:3,d:4,f:5,t:6,g:7,y:8,h:9,u:10,j:11,k:12,o:13,l:14,p:15,';':16,z:-12,x:-11,c:-10,v:-9,b:-8,n:-7,m:-6};
 const kbOct={v:0},kbDown={};
 document.addEventListener('keydown',e=>{
  if(!G2BKB.on||e.repeat||e.target.tagName==='INPUT'||e.target.tagName==='TEXTAREA')return;
  const k=e.key.toLowerCase();
  if(k==='['){kbOct.v=Math.max(-3,kbOct.v-1);return;}
  if(k===']'){kbOct.v=Math.min(3,kbOct.v+1);return;}
  if(!(k in kmap)||kbDown[k])return;kbDown[k]=true;
  window.onMidiNote&&window.onMidiNote(60+kmap[k]+kbOct.v*12,0,100);});
 document.addEventListener('keyup',e=>{
  const k=e.key.toLowerCase();
  if(!(k in kmap))return;delete kbDown[k];
  window.onMidiOff&&window.onMidiOff(60+kmap[k]+kbOct.v*12);});
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
  const trk=[0,0,0xFF,0x51,3,0x07,0xA1,0x20];
  let last=0;for(const e of ev){trk.push(...vlq(e[0]-last),e[1],e[2],e[3]);last=e[0]}
  trk.push(0,0,0xFF,0x2F,0);
  const hdr=[0x4D,0x54,0x68,0x64,0,0,0,6,0,0,0,1,0x01,0xF4,0x4D,0x54,0x72,0x6B,
   (trk.length>>24)&255,(trk.length>>16)&255,(trk.length>>8)&255,trk.length&255];
  const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([new Uint8Array(hdr),new Uint8Array(trk)],{type:'audio/midi'}));
  a.download='g2b-opname.mid';if(window.G2BExport&&!G2BExport.ok(window.G2B_KB_SLUG||'club'))return;a.click();};
})();
