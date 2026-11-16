// G2B Videoboek-Speler — gedeelde engine voor ALLE interactieve videoboeken.
// Gebruik: G2B_BOEK.start(cfg, {pg, vond, punten}) — cfg = {titel, paginas:[{art,tekst,sfeer,vrienden:[{c,s,x,y}]}]}
// art = bestandsnaam in IMGPRE, vriend c = mem-<c>-sil-<s|0>.png in IMGPRE.
window.G2B_BOEK=(()=>{
const IMGPRE='../gsbooks/img/';
const JIG=c=>IMGPRE+'mem-'+c+'-sil-'+(c.includes('sunny')||c.includes('ziggy')?'0':'0')+'.png';

function start(cfg,hud){
 const pgEl=hud.pg||null,vEl=hud.vond||null,ptEl=hud.punten||null;
 let pg=0,gfound=0,punten=0;
 const A=new(window.AudioContext||window.webkitAudioContext)();
 let muziek=false,muzTimer=null;
 const note=(f,d=.35,v=.06)=>{const o=A.createOscillator(),g=A.createGain();o.type='sine';o.frequency.value=f;g.gain.setValueAtTime(v,A.currentTime);g.gain.exponentialRampToValueAtTime(.001,A.currentTime+d);o.connect(g);g.connect(A.destination);o.start();o.stop(A.currentTime+d)};
 const pling=()=>{note(660,.09,.12);setTimeout(()=>note(880,.11,.12),70)};
 const hooray=()=>{[523,659,784,1047].forEach((f,i)=>setTimeout(()=>note(f,.16,.1),i*90))};
 const ARP=[[523,659,784,988],[440,523,659,880],[349,440,523,698],[392,494,587,784]];let ap=0;
 const muz=()=>{if(muziek){muziek=false;clearInterval(muzTimer)}else{
  muziek=true;muzTimer=setInterval(()=>{const ch=ARP[ap%4];ch.forEach((f,i)=>setTimeout(()=>note(f,.5,.045),i*140));ap++},1600)}};
 muz();
 const PAGES=cfg.paginas,N=PAGES.length;

 class Boek extends Phaser.Scene{
  preload(){PAGES.forEach((p,i)=>this.load.image('art'+i,IMGPRE+p.art));
   const set=new Set();PAGES.forEach(p=>(p.vrienden||[]).forEach(v=>set.add(v.c+':'+(v.s||0))));
   [...set].forEach(k=>{const[c,s]=k.split(':');this.load.image('v-'+k,IMGPRE+'mem-'+c+'-sil-'+s+'.png')});}
  create(){this.showPage()}
  showPage(){
   const P=PAGES[pg],W=560,H=560;
   this.children.removeAll(true);this.pageT=0;
   if(pgEl)pgEl.textContent='Pagina '+(pg+1)+'/'+N;
   const bg=this.add.image(W/2,H/2,'art'+pg).setDisplaySize(W*1.06,H*1.06).setAlpha(0);
   this.tweens.add({targets:bg,alpha:1,duration:600});
   this.tweens.add({targets:bg,scaleX:1.10,scaleY:1.10,duration:9000,yoyo:true,repeat:-1,ease:'Sine.easeInOut'});
   const kleur=P.sfeer==='nacht'?0xffe9a8:(P.sfeer==='eiland'?0xbffff5:0xfff6d8);
   for(let i=0;i<26;i++){
    const d=this.add.circle(Math.random()*W,Math.random()*H,1+Math.random()*2.2,kleur,.25+Math.random()*.5);
    this.tweens.add({targets:d,y:d.y-(18+Math.random()*40),alpha:0,duration:2600+Math.random()*3000,repeat:-1,delay:Math.random()*2500});}
   const vr=P.vrienden||[];
   this.rest=vr.length;
   vr.forEach(v=>{
    const s=this.add.image(v.x,v.y,'v-'+v.c+':'+(v.s||0)).setScale(.24).setAlpha(.62).setInteractive({useHandCursor:true});
    // lichte silhouet-kleur (popt op donkere tekeningen) + donkere gloed-rand (popt op lichte tekeningen)
    s.setTintFill(0xfff3cf);
    if(s.preFX)s.preFX.addGlow(0x2a1a06,.15,16,6);
    s.hint=this.time.addEvent({delay:5200+Math.random()*2200,loop:true,
     callback:()=>{if(!s.found)this.tweens.add({targets:s,scale:.27,y:v.y-8,duration:260,yoyo:true})}});
    s.on('pointerdown',()=>{if(s.found)return;s.found=true;s.hint.remove();
     const bonus=this.pageT<15?15:(this.pageT<30?10:5);
     punten+=10+bonus;if(ptEl)ptEl.textContent='⭐ '+punten;
     this.tweens.add({targets:s,alpha:1,scale:.3,angle:{from:-6,to:6},duration:300,yoyo:true,repeat:1});
     pling();gfound++;if(vEl)vEl.textContent='🔍 '+gfound;
     const p=this.add.text(v.x,v.y-40,'+'+(10+bonus),{font:'bold 16px Georgia',color:'#fff8d6'});
     this.tweens.add({targets:p,y:v.y-75,alpha:0,duration:900,onComplete:()=>p.destroy()});
     if(--this.rest===0)this.time.delayedCall(700,()=>this.pageDone())});
   });
   this.add.rectangle(0,H-92,W,92,0x000000,.62).setOrigin(0);
   const tt=this.add.text(24,H-80,'',{font:'15px Georgia',color:'#fff',wordWrap:{width:W-48}});
   let i=0;this.time.addEvent({delay:24,callback:()=>{tt.text+=P.tekst[i++]||''},repeat:P.tekst.length});
   const m=this.add.text(W-16,10,'🎵',{font:'22px system-ui'}).setOrigin(1,0).setInteractive({useHandCursor:true});
   m.setAlpha(muziek?1:.4);
   m.on('pointerdown',()=>{muz();m.setAlpha(muziek?1:.4)});
  }
  update(t,dt){this.pageT+=dt/1000}
  confetti(){for(let i=0;i<36;i++){
   const c=this.add.rectangle(Math.random()*560,-20-Math.random()*120,7,7,[0xffe066,0x7ee2a8,0x8fd0ff,0xffa8c9][i%4]);
   this.tweens.add({targets:c,y:580,x:c.x+(Math.random()*120-60),angle:360,duration:1400+Math.random()*1200,onComplete:()=>c.destroy()});}}
  pageDone(){
   const W=560,H=560;this.confetti();hooray();
   if(pg<N-1){
    const b=this.add.text(W/2,H/2,'▶ volgende pagina',{font:'bold 26px Georgia',color:'#ffe9a8'})
     .setOrigin(.5).setInteractive({useHandCursor:true}).setPadding(16).setBackgroundColor('#1c5c34cc');
    b.on('pointerdown',()=>{pg++;this.showPage()});
    this.tweens.add({targets:b,scale:1.06,duration:500,yoyo:true,repeat:-1});
   }else{
    const st=punten<60?'⭐☆☆':punten<110?'⭐⭐☆':'⭐⭐⭐';
    const b=this.add.text(W/2,H/2-20,'🎉 Einde! '+st+'\n'+punten+' punten — nog een keer?',{font:'bold 24px Georgia',color:'#ffe9a8',align:'center'})
     .setOrigin(.5).setInteractive({useHandCursor:true}).setPadding(18).setBackgroundColor('#1c5c34cc');
    b.on('pointerdown',()=>{pg=0;gfound=0;punten=0;
     if(vEl)vEl.textContent='🔍 0';if(ptEl)ptEl.textContent='⭐ 0';this.showPage()});
   }
  }
 }
 return new Phaser.Game({type:Phaser.AUTO,width:560,height:560,parent:'game',scene:Boek,backgroundColor:'#0b1f14'});
}
return {start};
})();
