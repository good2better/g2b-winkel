/* GIO-BREIN v1 — commando-brein voor de Werkstation.
   Laag 1: regel-parser (werkt ALTIJD, offline, €0).
   Laag 2: als Ollama op deze pc draait (localhost:11434) → losse taal via lokaal model.
   Commando's sturen de echte tools aan via deep-links met ?g/&bpm/&go. */
(function(){
'use strict';
var GENRES={house:124,houce:124,trap:145,bubbling:152,soca:128,schuivel:82,kawina:142,afro:112,afrobeat:104,boombap:88,dancehall:100,dembow:95,drill:142,funk:98,lofi:75,phonk:132,zouk:100,surinaams:138,rock:120,dna:140};
var ALIAS={houce:'house',house:'house','hip-hop':'boombap',hiphop:'boombap',hip:'boombap','boom-bap':'boombap',urban:'trap',edm:'house',dance:'house',reggaeton:'dembow','drum and bass':'dna',dnb:'dna','drum&bass':'dna',latin:'dembow',suri:'surinaams',kaseko:'kawina'};
var TOOLS={
 beat:['../beatmaker-gio/app/index.html','🧬 Beat Maker GIO'],
 gaw:['../gaw/index.html','🎚️ GAW'],
 master:['../master-web/master-club.html','💎 G2B MASTER'],
 menger:['../menger-web/index.html','🎛 MENGER (mix-check)'],
 melodie:['../melodie/index.html','🎼 MELODIE'],
 refors:['../refors-web/refors.html','🔬 REFORS (ruimte-test)'],
 midibib:['../midi-bib/index.html','📚 MIDI-bibliotheek'],
 laagbaas:['../laagbaas/index.html','🧠 LAAGBAAS'],
 stapel:['../../stapeldice-pro.html','🎲 STAPEL-DICE PRO'],
 shorts:['../shorts-web/index.html','✂️ SHORTS-knipper'],
 smash:['../../smash.html','💥 G2B SMASH'],
 airdrums:['../../airdrums.html','🥁 AIRDRUMS'],
 werkstation:['index.html','🏠 Werkstation']};
var KEYS={c:0,'c#':1,des:1,d:2,'d#':3,es:3,e:4,f:5,'f#':6,g:7,'g#':8,as:8,a:9,'a#':10,bes:10,b:11,h:11};
var NOTEN=['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
var MAJ=[0,2,4,5,7,9,11],MIN=[0,2,3,5,7,8,10];
var MAJQ=['','m','m','','','m','°'],MINQ=['m','°','','m','m','',''];
var MOOD={donker:{k:'A mineur',g:'drill',b:142,u:'kleine toonladder, half-time drums'},
  vrolijk:{k:'C majeur',g:'funk',b:98,u:'grote intervallen, levendige perc'},
  film:{k:'D mineur',g:'lofi',b:75,u:'langzame spannende lijn, veel ruimte'},
  strijd:{k:'E mineur',g:'trap',b:145,u:'lage tonen, agressieve 808'},
  rustig:{k:'F majeur',g:'zouk',b:100,u:'zachte akkoorden, weinig hats'},
  energiek:{k:'G majeur',g:'soca',b:128,u:'dichte perc, snelle groove'},
  tropisch:{k:'A majeur',g:'afro',b:112,u:'warme percussie, zonnige akkoorden'},
  houce:{k:'A mineur',g:'house',b:124,u:'four-on-the-floor, simpele bas-lijn'}};
function akkoorden(key){var m=key.trim().toLowerCase().replace('mineur','m').replace('min','m').replace('majeur','').replace('maj','');
 var minor=/m$/.test(m),root=m.replace(/m$/,'').trim();
 var n=KEYS[root];if(n==null)return null;
 var st=minor?MIN:MAJ,qu=minor?MINQ:MAJ;
 return st.map(function(s,i){return NOTEN[(n+s)%12]+qu[i]});}
function say(h){var l=document.getElementById('gioLog');if(!l)return;var d=document.createElement('div');d.className='gioMsg';d.innerHTML=h;l.appendChild(d);l.scrollTop=l.scrollHeight;}
function open(url){say('→ <a href="'+url+'" style="color:#7CF5B0">open '+url+'</a>');window.open(url,'_blank');}
function intent(t){
 t=' '+t.toLowerCase().replace(/[?!.,]/g,' ')+' ';
 var bpm=(t.match(/\b(\d{2,3})\s*(bpm)?\b/)||[])[1];
 var key=(t.match(/\b(in|toon|key)\s+([a-g](#|b|is|es)?\s*(mineur|min|majeur|maj|m)?)/)||[])[2];
 var g=null;for(var k in GENRES){if(t.indexOf(' '+k)>=0||t.indexOf(k+' ')>-1){g=ALIAS[k]||k;break}}
 if(!g)for(var k2 in ALIAS){if(t.indexOf(k2)>=0){g=ALIAS[k2];break}}
 if(/help|overzicht|wat kan je|commando/.test(t))return{a:'help'};
 if(/akkoord/.test(t)){var kk=key||(t.match(/\b([a-g](#|b|is|es)?\s*(mineur|maj|m))\b/)||[])[1];if(kk)return{a:'akkoorden',key:kk}}
 if(/samen|begeleid|stap|help me bouwen|maak samen|copro|co-?pilot/.test(t))return{a:'samen',g:g,bpm:bpm};
 if(/referentie|net als mijn|zoals mijn|vergelijk mijn|verbeter mijn|kwaliteit/.test(t))return{a:'ref'};
 if(/maak|genereer|bouw|nieuw|beat|dobbel|rol|willekeur|song|nummer|track/.test(t))return{a:'beat',g:g||'house',bpm:bpm||null,song:/song|nummer/.test(t)};
 if(/master/.test(t))return{a:'tool',tool:'master'};
 if(/menger|mix(?![a-z])|mixcheck/.test(t))return{a:'tool',tool:'menger'};
 if(/melodie/.test(t))return{a:'tool',tool:'melodie'};
 if(/refors|ruimte|echo test/.test(t))return{a:'tool',tool:'refors'};
 if(/laag|laagbaas|structuur/.test(t))return{a:'tool',tool:'laagbaas'};
 if(/stapel|one.?shot/.test(t))return{a:'tool',tool:'stapel'};
 if(/airdrums|drumpad/.test(t))return{a:'tool',tool:'airdrums'};
 if(/midi.?bib|bibliotheek/.test(t))return{a:'tool',tool:'midibib'};
 if(/short|knip|clip|promo.?video|smash/.test(t))return{a:'tool',tool:/smash/.test(t)?'smash':'shorts'};
 if(/gaw|werkplek|pianorol/.test(t))return{a:'tool',tool:'gaw'};
 for(var m2 in MOOD){if(t.indexOf(m2)>=0)return{a:'mood',mood:m2}}
 return g||bpm?{a:'beat',g:g||'house',bpm:bpm||null}:null;
}
function run(t){
 var i=intent(t);if(!i){say('❓ niet begrepen — typ <b>help</b> voor alle commando\'s');return}
 switch(i.a){
  case 'help':say('📋 <b>voorbeelden:</b><br>• maak houce beat 128 bpm<br>• nieuwe dembow<br>• trap op 145<br>• iets donkers voor een film<br>• akkoorden bij a mineur<br>• master / menger / melodie / refors / stapel / gaw<br>• welke key bij strijd?<br>• referentie — gebruik je eigen beat als leidraad<br>• <b>samen bouwen</b> — stap-voor-stap: jij keurt elke stap goed');break;
  case 'akkoorden':var acc=akkoorden(i.key);say(acc?('🎹 '+i.key.toUpperCase()+': <b>'+acc.join(' · ')+'</b>'):'❓ key niet gevonden — probeer bv. "akkoorden bij a mineur"');break;
  case 'mood':var M=MOOD[i.mood];say('💡 bij <b>'+i.mood+'</b> past: <b>'+M.k+'</b>, '+M.g+' ±'+M.b+'bpm — '+M.u+'. <a href="#" onclick="GIO.run(\'maak '+M.g+' '+M.b+' bpm\');return false" style="color:#7CF5B0">maak hem →</a>');break;
  case 'ref':say('🎯 <b>eigen beat als referentie</b> — alleen voor BOUWEN: hij leest je patroon/noten/structuur en herbouwt het met G2B-sounds. Je mix/master neemt hij NIET over — MENGER en MASTER doen dat op jouw eigen track, dus een slechte referentie-mix is geen probleem.');open(TOOLS.beat[0]+'?ref=1');break;
  case 'tool':var T=TOOLS[i.tool];say('🔧 '+T[1]);open(T[0]);break;
  case 'beat':var u=TOOLS.beat[0]+'?g='+i.g+(i.bpm?'&bpm='+i.bpm:'')+(i.song?'&song=1':'&go=1');say('🎲 '+i.g.toUpperCase()+(i.bpm?' @'+i.bpm:'')+' — open Beat Maker');open(u);break;
  case 'samen':startBuild(i);break;
 }}

/* ---- SAMEN BOUWEN: stap-voor-stap modus — jij keurt elke stap goed ---- */
var CTX=null;
var STAP=[
 {naam:'genre',vraag:function(){return 'STAP 1/6 — welk genre? Kies of typ zelf.'},knoppen:function(){return Object.keys(GENRES).filter(function(g){return g===ALIAS[g]||!ALIAS[g]}).slice(0,10).map(function(g){return {t:g,v:g}})}},
 {naam:'sfeer',vraag:function(){return 'STAP 2/6 — welke sfeer? (typ ook eigen mood)'},knoppen:function(){return Object.keys(MOOD).map(function(m){return {t:m,v:m}})}},
 {naam:'drums',vraag:function(){var M=CTX.sfeer&&MOOD[CTX.sfeer];var g=CTX.genre||(M?M.g:'house');var b=CTX.bpm||(M?M.b:GENRES[g]);return 'STAP 3/6 — drums per-laag: '+g+' @'+b+'bpm'+(M?' ('+M.u+')':'')},knoppen:function(){return [{t:'🧱 bouw per laag',v:'_drums'},{t:'⏭ overslaan',v:'_skip'}]}},
 {naam:'melodie',vraag:function(){return 'STAP 4/6 — melodie/akkoorden erbij? (MELODIE geeft varianten, jij kiest)'},knoppen:function(){return [{t:'🎼 open MELODIE',v:'_melodie'},{t:'🎹 toon akkoorden',v:'_akk'},{t:'⏭ overslaan',v:'_skip'}]}},
 {naam:'structuur',vraag:function(){return 'STAP 5/6 — song-structuur (intro/refrein/drop) met LAAGBAAS?'},knoppen:function(){return [{t:'🧠 open LAAGBAAS',v:'_laag'},{t:'⏭ overslaan',v:'_skip'}]}},
 {naam:'mix-master',vraag:function(){return 'STAP 6/6 — afwerken: MENGER checkt de mix, MASTER maakt hem klaar'},knoppen:function(){return [{t:'🎛 MENGER',v:'_menger'},{t:'💎 MASTER',v:'_master'},{t:'✅ klaar',v:'_done'}]}}];
function startBuild(i){CTX={stap:0,genre:i.g,bpm:i.bpm,sfeer:null};say('🤝 <b>SAMEN BOUWEN</b> — ik stel voor, jij keurt goed of past aan. <a href="#" onclick="GIO._stop();return false" style="color:#B77">stoppen</a>');stap()}
function stap(){var S=STAP[CTX.stap];if(!S){einde();return}var kb=S.knoppen().map(function(k){return '<a href="#" onclick="GIO._kies(\''+k.v+'\');return false" style="display:inline-block;margin:2px 5px 0 0;padding:4px 10px;border:1px solid #2C4A38;border-radius:14px;color:#7CF5B0;font-size:12px">'+k.t+'</a>'}).join('');
 say('🪜 '+S.vraag()+'<br>'+kb)}
function kies(v){var S=STAP[CTX.stap];
 if(v==='_skip'){CTX.stap++;stap();return}
 if(v==='_drums'){var M=CTX.sfeer&&MOOD[CTX.sfeer];var g=CTX.genre||(M?M.g:'house');var b=CTX.bpm||(M?M.b:GENRES[g]);say('🧱 <b>per-laag bouwen</b>: rol eerst KICK apart 🎲 → dan SNARE → dan HATS → dan PERC. 🔒 goede laag vastzetten, 🎲 rest opnieuw.');open(TOOLS.beat[0]+'?g='+g+'&bpm='+b+'&go=1&lagen=1');CTX.stap++;stap();return}
 if(v==='_melodie'){open(TOOLS.melodie[0]);CTX.stap++;stap();return}
 if(v==='_akk'){var M2=CTX.sfeer&&MOOD[CTX.sfeer];run('akkoorden bij '+(M2?M2.k:'a mineur'));return}
 if(v==='_laag'){open(TOOLS.laagbaas[0]);CTX.stap++;stap();return}
 if(v==='_menger'){open(TOOLS.menger[0]);return}
 if(v==='_master'){open(TOOLS.master[0]);return}
 if(v==='_done'){einde();return}
 if(STAP[CTX.stap].naam==='genre'){CTX.genre=ALIAS[v]||v;CTX.stap++;stap();return}
 if(STAP[CTX.stap].naam==='sfeer'){CTX.sfeer=v;var M3=MOOD[v];if(M3){if(!CTX.genre)CTX.genre=M3.g;if(!CTX.bpm)CTX.bpm=M3.b}CTX.stap++;stap();return}}
function einde(){var M=CTX&&CTX.sfeer&&MOOD[CTX.sfeer];say('✅ <b>samen gebouwd!</b> '+(CTX.genre||'geen genre')+(CTX.bpm?' @'+CTX.bpm+'bpm':'')+(M?', sfeer '+CTX.sfeer:'')+' — typ <b>samen bouwen</b> voor een nieuwe.');CTX=null}
function stopBuild(){CTX=null;say('🛑 samen-bouw gestopt.')}

var AI=null;
function probe(){fetch('http://localhost:11434/api/tags',{method:'GET'}).then(function(r){return r.json()}).then(function(d){
 AI=(d.models||[]).map(function(m){return m.name})[0]||null;
 var b=document.getElementById('gioAi');if(b)b.textContent=AI?('🧠 lokaal model: '+AI+' (gratis)'):'🧠 regel-parser (Ollama niet gevonden — alles werkt al)';
}).catch(function(){var b=document.getElementById('gioAi');if(b)b.textContent='🧠 regel-parser (Ollama niet gevonden — alles werkt al)'});}
function aiParse(t){return fetch('http://localhost:11434/api/generate',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({model:AI,stream:false,prompt:'Vertaal deze muziek-opdracht naar JSON {actie:beat|tool|akkoorden|mood|help, genre, bpm, key, tool, mood}. Genres: '+Object.keys(GENRES).join(',')+'. Tools: master,menger,melodie,refors,laagbaas,stapel,airdrums,midibib,gaw,shorts,smash. Alleen JSON.\n\nOpdracht: '+t})}).then(function(r){return r.json()}).then(function(d){try{return JSON.parse(d.response.match(/\{[\s\S]*\}/)[0])}catch(e){return null}}).catch(function(){return null});}
function go(){var inp=document.getElementById('gioIn');var t=(inp.value||'').trim();if(!t)return;inp.value='';say('<b style="color:#8AA">› '+t+'</b>');
 if(CTX){var low=t.toLowerCase();var S=STAP[CTX.stap];if(S.naam==='genre'){for(var gg in GENRES){if(low.indexOf(gg)>=0){kies(gg);return}}say('❓ typ een genre (bv. house, trap, soca) of klik');return}if(S.naam==='sfeer'){for(var mm in MOOD){if(low.indexOf(mm)>=0){kies(mm);return}}CTX.sfeer=t;CTX.stap++;stap();return}}
 if(AI){aiParse(t).then(function(j){if(j&&j.actie){var s=j.actie==='beat'?('maak '+(j.genre||'house')+' '+(j.bpm||'')+'bpm'):(j.actie==='tool'?j.tool:(j.actie==='akkoorden'?('akkoorden bij '+(j.key||'c')):(j.actie==='mood'?j.mood:'help')));run(s)}else run(t)})}else run(t);}
function boot(){
 var h=document.createElement('div');h.id='gioPanel';
 h.innerHTML='<style>#gioPanel{margin:26px 0;border:1px solid #23352A;border-radius:16px;background:#0C120E;padding:18px}#gioPanel h2{margin:0 0 4px;color:#3DDC84;font-size:17px;letter-spacing:1px}#gioLog{max-height:230px;overflow:auto;font-size:13.5px;line-height:1.55;padding:6px 0}#gioKn{margin:8px 0 6px}#gioKn a{display:inline-block;margin:0 6px 0 0;padding:5px 12px;border:1px solid #2C4A38;border-radius:14px;color:#7CF5B0;font-size:12.5px;text-decoration:none}#gioKn a:hover{border-color:#3DDC84}#gioIn{width:100%;box-sizing:border-box;padding:13px;border-radius:10px;border:1px solid #2C4A38;background:#101A13;color:#E8F0EA;font-size:14px}#gioAi{font-size:11px;color:#5B7B66;margin-top:8px}.gioMsg{margin:5px 0}</style>'
 +'<h2>🤖 GIO — jouw commando-brein</h2><div style="font-size:12px;color:#7B9B85">typ een opdracht, ik stuur je tools aan — offline, gratis</div><div id="gioLog"></div><div id="gioKn"><a href="#" data-run="samen bouwen">🤝 samen bouwen</a><a href="#" data-run="help">📋 help</a><a href="#" data-run="referentie">🎯 referentie</a></div><input id="gioIn" placeholder="typ een opdracht — of klik een knop hierboven" autocomplete="off"><div id="gioAi">🧠 zoeken naar lokaal model…</div>';
 var an=document.querySelector('footer,.foot')||document.body;an.parentNode.insertBefore(h,an);
 document.getElementById('gioIn').addEventListener('keydown',function(e){if(e.key==='Enter')go()});
 document.getElementById('gioKn').addEventListener('click',function(e){var a=e.target.closest('a[data-run]');if(!a)return;e.preventDefault();window.GIO.run(a.dataset.run)});
 window.GIO={run:function(t){say('<b style="color:#8AA">› '+t+'</b>');run(t)},_kies:kies,_stop:stopBuild};
 probe();
 say('👋 GIO klaar. Typ <b>help</b> voor voorbeelden — bv. <i>maak houce beat 128 bpm</i>.');
}
if(document.body)boot();else document.addEventListener('DOMContentLoaded',boot);
})();
