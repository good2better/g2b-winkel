/* G2B LiveWereld — eigen vanilla browserengine voor levende achtergronden.
   Geen externe dependencies; canvasdeeltjes, parallaxsfeer en klikmagie. */
(function(){
"use strict";
const TYPES={
 vuurvliegen:{n:34,kind:'glow',speed:.16,size:[2,5],alpha:.85},
 sterren:{n:70,kind:'star',speed:.02,size:[1,2.6],alpha:.9},
 regen:{n:90,kind:'rain',speed:1.8,size:[7,15],alpha:.45},
 sneeuw:{n:70,kind:'snow',speed:.42,size:[1.8,4.8],alpha:.9},
 bladeren:{n:34,kind:'leaf',speed:.34,size:[4,9],alpha:.85},
 confetti:{n:65,kind:'confetti',speed:.7,size:[3,7],alpha:.9},
 bubbels:{n:42,kind:'bubble',speed:.38,size:[3,9],alpha:.55},
 meteoren:{n:24,kind:'meteor',speed:1.15,size:[10,24],alpha:.9},
 vonken:{n:58,kind:'ember',speed:.72,size:[1.8,4.8],alpha:.85},
 mist:{n:24,kind:'mist',speed:.08,size:[30,82],alpha:.18},
 aurora:{n:55,kind:'star',speed:.02,size:[1,2.4],alpha:.75}
};
function mulberry(seed){let s=seed>>>0;return()=>{s|=0;s=s+0x6D2B79F5|0;let t=Math.imul(s^s>>>15,1|s);t=t+Math.imul(t^t>>>7,61|t)^t;return((t^t>>>14)>>>0)/4294967296}}
function hash(str){let h=2166136261;for(const c of String(str))h=Math.imul(h^c.charCodeAt(0),16777619);return h>>>0}
function create(host,opt){
 opt=opt||{};const cv=document.createElement('canvas');cv.className='livewereld-canvas';
 cv.style.cssText='position:absolute;inset:0;width:100%;height:100%;pointer-events:none;z-index:0';
 host.insertBefore(cv,host.firstChild);const x=cv.getContext('2d');
 let W=0,H=0,parts=[],effect=opt.effect||'vuurvliegen',pal=opt.palette||['#12355c','#07111f'],seed=hash(effect),running=true,last=performance.now(),bursts=[],pmx=0,pmy=0,tx=0,ty=0;
 function resize(){const r=host.getBoundingClientRect(),dpr=Math.min(2,devicePixelRatio||1);W=Math.max(2,Math.round(r.width*dpr));H=Math.max(2,Math.round(r.height*dpr));cv.width=W;cv.height=H;seedParts()}
 host.addEventListener('pointermove',e=>{const r=host.getBoundingClientRect();tx=((e.clientX-r.left)/Math.max(1,r.width)-.5)*2;ty=((e.clientY-r.top)/Math.max(1,r.height)-.5)*2});
 host.addEventListener('pointerleave',()=>{tx=0;ty=0});
 function col(i){const cols=['#f7c948','#ffffff','#7dd3fc','#34d399','#fb7185','#c084fc'];return cols[i%cols.length]}
 function seedParts(){parts=[];const conf=TYPES[effect]||TYPES.vuurvliegen,r=mulberry(seed+Math.floor(W*.7)+H);
  for(let i=0;i<conf.n;i++)parts.push({x:r(),y:r(),z:.35+r()*.65,ph:r()*9,rot:r()*7,c:col(i),s:conf.size[0]+r()*(conf.size[1]-conf.size[0]),v:.65+r()*.7})}
 function rr(cx,cy,rw,rh,c){x.fillStyle=c;x.beginPath();x.ellipse(cx,cy,rw,rh,0,0,7);x.fill()}
 function paintBackdrop(t){
  const u=W/100,v=H/100,tt=t*.00018;
  const g=x.createLinearGradient(0,0,0,H);g.addColorStop(0,pal[0]+'55');g.addColorStop(.55,pal[1]+'22');g.addColorStop(1,'rgba(0,0,0,.18)');x.fillStyle=g;x.fillRect(0,0,W,H);
  /* hemellichaam met zachte gloed */
  const mx=W*.78+pmx*.012*W,my=H*.17+pmy*.01*H,mg=x.createRadialGradient(mx,my,0,mx,my,H*.18);mg.addColorStop(0,'rgba(255,244,190,.52)');mg.addColorStop(.22,'rgba(255,244,190,.16)');mg.addColorStop(1,'rgba(255,244,190,0)');x.fillStyle=mg;x.beginPath();x.arc(mx,my,H*.18,0,7);x.fill();
  x.fillStyle='rgba(255,238,176,.75)';x.beginPath();x.arc(mx,my,H*.045,0,7);x.fill();
  /* verre heuvels in meerdere dieptelagen */
  for(let laag=0;laag<3;laag++){x.fillStyle=['rgba(4,18,24,.26)','rgba(3,14,18,.38)','rgba(2,10,13,.54)'][laag];x.beginPath();x.moveTo(0,H);
   const base=H*(.58+laag*.1);x.lineTo(0,base);for(let px=0;px<=W+20;px+=W/18){const y=base-Math.sin(px/W*5.4+laag*1.7)*H*(.045+laag*.012)-Math.cos(px/W*11+laag)*H*.018;x.lineTo(px,y)}x.lineTo(W,H);x.closePath();x.fill()}
  if(effect==='aurora'){
   /* noorderlicht: gelaagde bewegende linten in meerdere kleuren */
   for(let band=0;band<4;band++){x.save();x.globalCompositeOperation='lighter';const yy=H*(.14+band*.065),amp=H*(.035+band*.012),grad=x.createLinearGradient(0,yy-H*.08,0,yy+H*.12);grad.addColorStop(0,'rgba(32,230,139,0)');grad.addColorStop(.45,['rgba(32,230,139,.22)','rgba(115,199,255,.18)','rgba(192,132,252,.16)','rgba(32,230,139,.12)'][band]);grad.addColorStop(1,'rgba(32,230,139,0)');x.fillStyle=grad;x.beginPath();x.moveTo(0,yy);for(let px2=0;px2<=W;px2+=W/36){const y=yy+Math.sin(px2/W*(5.5+band)+tt*(9+band)+band)*amp+Math.sin(px2/W*13-band)*H*.012;x.lineTo(px2,y)}for(let px2=W;px2>=0;px2-=W/36){const y=yy+H*(.07+band*.012)+Math.sin(px2/W*(5.5+band)+tt*(9+band)+band+1.4)*amp;x.lineTo(px2,y)}x.closePath();x.fill();x.restore()}
  }
  if(effect==='meteoren'){
   /* diepe nacht met zachte sterrennevel */
   const ng=x.createRadialGradient(W*.24,H*.18,0,W*.24,H*.18,H*.55);ng.addColorStop(0,'rgba(115,199,255,.14)');ng.addColorStop(.45,'rgba(192,132,252,.06)');ng.addColorStop(1,'rgba(0,0,0,0)');x.fillStyle=ng;x.fillRect(0,0,W,H*.75);
  }
  if(effect==='bubbels'||effect==='regen'){
   /* water met gelaagde golven en maanreflectie */
   x.fillStyle=effect==='bubbels'?'rgba(13,91,112,.42)':'rgba(18,61,82,.35)';x.fillRect(0,H*.66,W,H*.34);
   for(let r=0;r<6;r++){x.strokeStyle='rgba(145,220,255,'+(.13+r*.025)+')';x.lineWidth=Math.max(1,H*.006);x.beginPath();const yy=H*(.69+r*.045);for(let px=0;px<=W;px+=W/34){const y=yy+Math.sin(px/W*12+r+tt*(8+r))*H*.009;x.lineTo(px,y)}x.stroke()}
   x.fillStyle='rgba(255,238,176,.09)';for(let r=0;r<9;r++)x.fillRect(mx-W*.018+(r%2)*8,H*(.68+r*.026),W*.036,H*.006)}
  if(effect==='vuurvliegen'||effect==='bladeren'||effect==='sneeuw'){
   /* bos-silhouetten: stammen, kronen en voorgrondtakken */
   for(let k=0;k<9;k++){const bx=(k*13+4)*u,by=H*.62,bh=H*(.22+(k%3)*.045);x.fillStyle='rgba(5,18,12,.58)';x.fillRect(bx,by-bh,W*.012,bh);x.fillStyle=k%2?'rgba(19,72,42,.42)':'rgba(16,61,38,.48)';rr(bx+W*.006,by-bh,W*.055,H*.07,x.fillStyle)}
   x.strokeStyle='rgba(5,14,10,.42)';x.lineWidth=H*.014;for(let k=0;k<4;k++){x.beginPath();const sx=W*(.08+k*.25);x.moveTo(sx,H*.86);x.quadraticCurveTo(sx+W*.05,H*.7,sx+W*.14,H*.64+Math.sin(tt+k)*H*.015);x.stroke()}}
  if(effect==='confetti'){
   /* feest: spotlights en slingers */
   for(let k=0;k<3;k++){const sx=W*(.2+k*.3);const gr=x.createLinearGradient(sx,0,W/2,H);gr.addColorStop(0,['rgba(247,201,72,.22)','rgba(125,211,252,.18)','rgba(192,132,252,.18)'][k]);gr.addColorStop(1,'rgba(255,255,255,0)');x.fillStyle=gr;x.beginPath();x.moveTo(sx,0);x.lineTo(W*.5+(k-1)*W*.17,H*.82);x.lineTo(W*.5+(k-1)*W*.17+W*.12,H*.82);x.closePath();x.fill()}
   x.strokeStyle='rgba(255,255,255,.32)';x.lineWidth=2;x.beginPath();x.moveTo(0,H*.16);x.quadraticCurveTo(W/2,H*.25,W,H*.16);x.stroke();for(let k=0;k<14;k++){const px=k/13*W,py=H*.16+Math.sin(k/13*Math.PI)*H*.07;x.fillStyle=['#f7c948','#fb7185','#34d399','#7dd3fc'][k%4];x.beginPath();x.moveTo(px,py);x.lineTo(px+W*.012,py+H*.035);x.lineTo(px-W*.012,py+H*.035);x.closePath();x.fill()}}
  if(effect==='sterren'||effect==='sneeuw'){
   /* bergen / winterdiepte */
   x.fillStyle='rgba(18,28,48,.48)';for(let k=0;k<5;k++){const bx=k*W*.23-W*.08;x.beginPath();x.moveTo(bx,H*.65);x.lineTo(bx+W*.14,H*.35+(k%2)*H*.07);x.lineTo(bx+W*.28,H*.65);x.closePath();x.fill();x.fillStyle='rgba(240,248,255,.12)';x.beginPath();x.moveTo(bx+W*.14,H*.35+(k%2)*H*.07);x.lineTo(bx+W*.1,H*.46+(k%2)*H*.05);x.lineTo(bx+W*.18,H*.46+(k%2)*H*.05);x.closePath();x.fill();x.fillStyle='rgba(18,28,48,.48)'}}
  if(effect==='vonken'){
   /* warme gloedlaag alsof er beneden vuur/kamplicht is */
   const fg=x.createRadialGradient(W*.5,H*1.05,0,W*.5,H*1.05,H*.75);fg.addColorStop(0,'rgba(255,119,48,.28)');fg.addColorStop(.45,'rgba(247,201,72,.09)');fg.addColorStop(1,'rgba(0,0,0,0)');x.fillStyle=fg;x.fillRect(0,H*.35,W,H*.65);
  }
  if(effect==='mist'){
   /* zachte grondmistbanken als extra dieptelaag */
   for(let bank=0;bank<4;bank++){const yy=H*(.68+bank*.07),gr=x.createLinearGradient(0,yy-H*.04,0,yy+H*.05);gr.addColorStop(0,'rgba(210,235,255,0)');gr.addColorStop(.5,'rgba(210,235,255,'+(.06+bank*.025)+')');gr.addColorStop(1,'rgba(210,235,255,0)');x.fillStyle=gr;x.beginPath();x.ellipse(W*(.18+bank*.22)+Math.sin(tt*8+bank)*W*.025,yy,W*.32,H*.055,0,0,7);x.fill()}
  }
  /* zachte vignette voor filmische diepte */
  const vg=x.createRadialGradient(W/2,H*.42,H*.18,W/2,H*.5,H*.95);vg.addColorStop(0,'rgba(0,0,0,0)');vg.addColorStop(1,'rgba(0,0,0,.34)');x.fillStyle=vg;x.fillRect(0,0,W,H);
 }
 function draw(dt,t){x.clearRect(0,0,W,H);pmx+=(tx-pmx)*.055;pmy+=(ty-pmy)*.055;const conf=TYPES[effect]||TYPES.vuurvliegen,scale=Math.min(W,H)/720;paintBackdrop(t);
  for(const p of parts){let px=p.x*W+Math.sin(t*.0007+p.ph)*12*p.z+pmx*18*p.z,py=p.y*H+pmy*9*p.z;
   if(conf.kind==='rain'){py=((p.y+t*.00009*conf.speed*p.v)%1.15)*H;px=(p.x*W+py*.055)%W;x.strokeStyle='rgba(170,215,255,'+conf.alpha*p.z+')';x.lineWidth=Math.max(1,p.z*1.5);x.beginPath();x.moveTo(px,py);x.lineTo(px-p.s*scale*.25,py+p.s*scale);x.stroke();continue}
   if(conf.kind==='snow'||conf.kind==='leaf'||conf.kind==='confetti'||conf.kind==='bubble')py=((p.y+t*.000055*conf.speed*p.v)%1.12)*H;
   if(conf.kind==='bubble')py=(1-((p.y+t*.000055*conf.speed*p.v)%1.12))*H;
   if(px<0)px+=W;if(px>W)px-=W;
   if(conf.kind==='glow'){const tw=.45+.55*Math.sin(t*.003+p.ph)**2,r=p.s*scale*p.z*(1+tw);const g=x.createRadialGradient(px,py,0,px,py,r*4);g.addColorStop(0,'rgba(255,238,150,'+conf.alpha*tw+')');g.addColorStop(.35,'rgba(247,201,72,'+(.28*tw)+')');g.addColorStop(1,'rgba(247,201,72,0)');x.fillStyle=g;x.beginPath();x.arc(px,py,r*4,0,7);x.fill()}
   else if(conf.kind==='star'){const tw=.25+.75*Math.sin(t*.002+p.ph)**2;x.fillStyle='rgba(255,255,255,'+conf.alpha*tw+')';x.beginPath();x.arc(px,py,p.s*scale*p.z,0,7);x.fill()}
   else if(conf.kind==='snow'){x.fillStyle='rgba(255,255,255,'+conf.alpha*p.z+')';x.beginPath();x.arc(px,py,p.s*scale,0,7);x.fill()}
   else if(conf.kind==='leaf'){x.save();x.translate(px,py);x.rotate(p.rot+t*.001);x.fillStyle='rgba(91,160,83,'+conf.alpha*p.z+')';x.beginPath();x.ellipse(0,0,p.s*scale,p.s*scale*.45,0,0,7);x.fill();x.restore()}
   else if(conf.kind==='confetti'){x.save();x.translate(px,py);x.rotate(p.rot+t*.002);x.fillStyle=p.c;x.globalAlpha=conf.alpha*p.z;x.fillRect(-p.s*scale/2,-p.s*scale/4,p.s*scale,p.s*scale/2);x.globalAlpha=1;x.restore()}
   else if(conf.kind==='bubble'){x.strokeStyle='rgba(185,230,255,'+conf.alpha*p.z+')';x.lineWidth=1.4;x.beginPath();x.arc(px,py,p.s*scale,0,7);x.stroke();x.fillStyle='rgba(255,255,255,.16)';x.beginPath();x.arc(px-p.s*scale*.3,py-p.s*scale*.3,p.s*scale*.24,0,7);x.fill()}
   else if(conf.kind==='meteor'){const cyc=(t*.000055*conf.speed*p.v+p.y)%1.35,xx=(p.x*W+cyc*W*.42+pmx*18*p.z)%W,yy=cyc*H*.72;x.save();x.globalCompositeOperation='lighter';const lg=x.createLinearGradient(xx,yy,xx-W*.045*p.z,yy-H*.025*p.z);lg.addColorStop(0,'rgba(255,255,255,'+conf.alpha*p.z+')');lg.addColorStop(.35,'rgba(125,211,252,'+(.45*conf.alpha*p.z)+')');lg.addColorStop(1,'rgba(125,211,252,0)');x.strokeStyle=lg;x.lineWidth=Math.max(1.2,p.s*scale*.16);x.beginPath();x.moveTo(xx,yy);x.lineTo(xx-W*.045*p.z,yy-H*.025*p.z);x.stroke();x.restore()}
   else if(conf.kind==='ember'){const yy=(1-((p.y+t*.000055*conf.speed*p.v)%1.08))*H,xx=p.x*W+Math.sin(t*.001+p.ph)*18*p.z+pmx*15*p.z,tw=.35+.65*Math.sin(t*.006+p.ph)**2,r=p.s*scale*(1+tw);const eg=x.createRadialGradient(xx,yy,0,xx,yy,r*4);eg.addColorStop(0,'rgba(255,238,150,'+conf.alpha*tw+')');eg.addColorStop(.35,'rgba(255,120,48,'+(.35*tw)+')');eg.addColorStop(1,'rgba(255,80,20,0)');x.fillStyle=eg;x.beginPath();x.arc(xx,yy,r*4,0,7);x.fill()}
   else if(conf.kind==='mist'){const xx=((p.x+t*.000018*conf.speed*p.v)%1.2)*W-W*.1+pmx*28*p.z,yy=H*(.62+p.y*.34)+Math.sin(t*.0006+p.ph)*H*.018;const mg=x.createRadialGradient(xx,yy,0,xx,yy,p.s*scale);mg.addColorStop(0,'rgba(210,235,255,'+conf.alpha*p.z+')');mg.addColorStop(1,'rgba(210,235,255,0)');x.fillStyle=mg;x.beginPath();x.ellipse(xx,yy,p.s*scale*1.8,p.s*scale*.55,0,0,7);x.fill()}}
  for(let i=bursts.length-1;i>=0;i--){const b=bursts[i],age=(t-b.t)/750;if(age>=1){bursts.splice(i,1);continue}
   x.save();x.globalAlpha=1-age;for(let k=0;k<b.n;k++){const a=k/b.n*Math.PI*2+b.rot,d=(14+age*72)*(b.big?1.35:1),px=b.x+Math.cos(a)*d,py=b.y+Math.sin(a)*d;x.fillStyle=k%2?'#f7c948':'#fff';x.beginPath();x.arc(px,py,(4.5*(1-age)+1)*((W+H)/1500),0,7);x.fill()}x.restore()}
 }
 function loop(t){if(!running)return;const dt=Math.min(50,t-last);last=t;draw(dt,t);requestAnimationFrame(loop)}
 resize();addEventListener('resize',resize);draw(0,performance.now());requestAnimationFrame(loop);
 return{
  setScene(o){o=o||{};if(o.effect&&TYPES[o.effect])effect=o.effect;if(o.palette)pal=o.palette;seed=hash(effect+JSON.stringify(pal));seedParts()},
  burst(cx,cy,big){const r=host.getBoundingClientRect(),dpr=cv.width/Math.max(1,r.width);bursts.push({x:cx*dpr,y:cy*dpr,t:performance.now(),rot:Math.random()*7,n:big?16:9,big:!!big})},
  effect(){return effect},particles(){return parts.length},destroy(){running=false;cv.remove()}
 }}
window.LiveWereld={create,TYPES};
})();
