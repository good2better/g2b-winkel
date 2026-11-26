/* G2B hub-layout v1 — compacte, consistente browserhubs zonder framework. */
(function(){
'use strict';
if(window.__G2B_HUB_V1__)return;window.__G2B_HUB_V1__=true;

function ready(fn){if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',fn);else fn()}
ready(function(){
 function gridOf(g){
  var n=g.nextElementSibling;
  if(n&&n.classList&&n.classList.contains('groep-uitleg'))n=n.nextElementSibling;
  return (n&&n.classList&&n.classList.contains('grid'))?n:null;
 }
 var groups=[].slice.call(document.querySelectorAll('.groep')).filter(function(g){return !!gridOf(g)});
 if(!groups.length)return;

 var css='\
.hubnav{position:sticky;top:0;z-index:80;margin:10px 0 15px;padding:8px;border:1px solid var(--rand,#22314d);border-radius:14px;background:rgba(9,15,26,.94);backdrop-filter:blur(8px);box-shadow:0 8px 24px rgba(0,0,0,.25)}\
.hubnav-row{display:flex;align-items:center;gap:7px;flex-wrap:wrap}\
.hubtotal{font-size:.68rem;color:#7ee2a8;font-weight:800;letter-spacing:.6px;white-space:nowrap}\
.hubchips{display:flex;gap:6px;overflow-x:auto;scrollbar-width:thin;flex:1;min-width:130px;padding:2px}\
.hubchip{white-space:nowrap;text-decoration:none;color:var(--zacht,#9fb0c9);border:1px solid var(--rand,#22314d);border-radius:999px;padding:4px 9px;font-size:.66rem;background:rgba(255,255,255,.02)}\
.hubchip:hover{color:#7ee2a8;border-color:var(--groen,#16a34a)}\
.hubbtn{border:1px solid var(--rand,#22314d);background:var(--kaart,#131c2e);color:var(--tekst,#e8eef8);border-radius:9px;padding:5px 8px;font-size:.65rem;font-weight:800;cursor:pointer;white-space:nowrap}\
.hubbtn.on{background:var(--groen,#16a34a);color:#04120a;border-color:var(--groen,#16a34a)}\
.hubsearch{min-width:130px;flex:1;max-width:250px;background:rgba(0,0,0,.22);border:1px solid var(--rand,#22314d);color:var(--tekst,#e8eef8);border-radius:9px;padding:6px 9px;font-size:.75rem}\
.groep.hubgroup{cursor:pointer;user-select:none;display:flex;align-items:center;gap:7px}\
.groep.hubgroup:before{content:"▾";color:var(--groen,#16a34a);font-size:.8rem;width:12px}\
.groep.hubgroup.hubfolded:before{content:"▸"}\
.groep .hubcount{margin-left:auto;font-size:.62rem;color:var(--zacht,#9fb0c9);border:1px solid var(--rand,#22314d);border-radius:999px;padding:1px 7px;font-weight:700;letter-spacing:0;text-transform:none}\
.grid.hubfolded,.groep-uitleg.hubfolded{display:none!important}\
body.hubcompact .grid{grid-template-columns:repeat(auto-fill,minmax(176px,1fr));gap:9px}\
body.hubcompact a.app{padding:11px 12px;border-radius:11px}\
body.hubcompact a.app img{display:none}\
body.hubcompact a.app .ic{display:inline-block;font-size:1.18rem;margin:0 5px 0 0;vertical-align:baseline}\
body.hubcompact a.app .nm{display:inline;font-size:.86rem;line-height:1.25}\
body.hubcompact a.app .in{font-size:.72rem;line-height:1.34;margin-top:5px}\
body.hubcompact a.app .tag{font-size:.57rem;padding:1px 5px}\
@media(max-width:620px){body.hubcompact .grid{grid-template-columns:repeat(2,minmax(0,1fr));gap:7px}body.hubcompact a.app{padding:10px}.hubnav{margin-left:-4px;margin-right:-4px;border-radius:10px}.hubbtn{font-size:.6rem;padding:4px 7px}.hubchip{font-size:.62rem}}\
@media(max-width:390px){body.hubcompact .grid{grid-template-columns:1fr}}';
 var st=document.createElement('style');st.id='g2b-hub-css';st.textContent=css;document.head.appendChild(st);

 var keyBase='g2bhub:'+location.pathname;
 function load(k,def){try{var v=localStorage.getItem(keyBase+':'+k);return v==null?def:v}catch(e){return def}}
 function save(k,v){try{localStorage.setItem(keyBase+':'+k,v)}catch(e){}}
 function txt(g){return g.textContent.replace(/\s+/g,' ').trim()}
 function appsOf(g){return [].slice.call(gridOf(g).querySelectorAll('a.app'))}
 function foldedKey(g){return 'fold:'+txt(g).toLowerCase().replace(/[^a-z0-9]+/g,'-').slice(0,80)}
 function setFold(g,fold,remember){
  var gr=gridOf(g);g.classList.toggle('hubfolded',fold);gr.classList.toggle('hubfolded',fold);
  var uitleg=g.nextElementSibling;if(uitleg&&uitleg.classList&&uitleg.classList.contains('groep-uitleg'))uitleg.classList.toggle('hubfolded',fold);
  if(remember)save(foldedKey(g),fold?'1':'0');
 }

 var total=groups.reduce(function(n,g){return n+appsOf(g).length},0);
 groups.forEach(function(g,i){
  g.classList.add('hubgroup');g.setAttribute('role','button');g.setAttribute('tabindex','0');
  if(!g.id)g.id='hubgroep-'+i;
  var c=document.createElement('span');c.className='hubcount';c.textContent=appsOf(g).length+'×';g.appendChild(c);
  var fold=load(foldedKey(g),'0')==='1';setFold(g,fold,false);
  function toggle(){setFold(g,!g.classList.contains('hubfolded'),true)}
  g.addEventListener('click',function(e){if(e.target.closest('a'))return;toggle()});
  g.addEventListener('keydown',function(e){if(e.key==='Enter'||e.key===' '){e.preventDefault();toggle()}});
 });

 var nav=document.createElement('div');nav.className='hubnav';
 var row=document.createElement('div');row.className='hubnav-row';
 var tot=document.createElement('span');tot.className='hubtotal';tot.textContent=total+' tools';row.appendChild(tot);
 var chips=document.createElement('div');chips.className='hubchips';
 groups.forEach(function(g){
  var a=document.createElement('a');a.className='hubchip';a.href='#'+g.id;a.textContent=txt(g).replace(/\s+/g,' ').replace(/\d+×$/,'').trim();
  a.onclick=function(e){e.preventDefault();setFold(g,false,false);document.getElementById(g.id).scrollIntoView({behavior:'smooth',block:'start'})};
  chips.appendChild(a);
 });
 row.appendChild(chips);
 var hasSearch=!!document.getElementById('zoek');
 var search=null;
 if(!hasSearch){search=document.createElement('input');search.className='hubsearch';search.type='search';search.placeholder='🔍 zoek…';row.appendChild(search)}
 var bView=document.createElement('button');bView.className='hubbtn';bView.type='button';
 var bFold=document.createElement('button');bFold.className='hubbtn';bFold.type='button';bFold.textContent='INEEN';
 row.appendChild(bView);row.appendChild(bFold);nav.appendChild(row);
 groups[0].parentNode.insertBefore(nav,groups[0]);

 function applyView(){
  var compact=load('view','compact')!=='groot';
  document.body.classList.toggle('hubcompact',compact);
  bView.textContent=compact?'COMPACT':'PLAATJES';bView.classList.toggle('on',compact);
 }
 bView.onclick=function(){save('view',load('view','compact')==='groot'?'compact':'groot');applyView()};
 applyView();

 function allFolded(){return groups.every(function(g){return g.classList.contains('hubfolded')})}
 function refreshFoldBtn(){bFold.textContent=allFolded()?'OPEN ALLES':'INEEN'}
 groups.forEach(function(g){g.addEventListener('click',function(){setTimeout(refreshFoldBtn,0)})});
 bFold.onclick=function(){var open=allFolded();groups.forEach(function(g){setFold(g,!open,true)});refreshFoldBtn()};
 refreshFoldBtn();

 function applySearch(q){
  q=(q||'').trim().toLowerCase();
  document.querySelectorAll('a.app').forEach(function(a){a.style.display=(!q||a.textContent.toLowerCase().indexOf(q)>=0)?'':'none'});
  groups.forEach(function(g){
   if(q)setFold(g,false,false);
   var any=appsOf(g).some(function(a){return a.style.display!=='none'});
   g.style.display=(!q||any)?'':'none';
  });
 }
 if(search)search.addEventListener('input',function(){applySearch(search.value)});
 var old=document.getElementById('zoek');
 if(old)old.addEventListener('input',function(){applySearch(old.value)});
});
})();
