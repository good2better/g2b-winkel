// G2B cookie-melding (AVG) — simpel, geen trackers aan boord.
(function(){
try{if(localStorage.getItem('g2b_consent'))return}catch(e){return}
var bar=document.createElement('div');
bar.id='g2b-consent';
bar.style.cssText='position:fixed;left:0;right:0;bottom:0;z-index:99999;background:#101512;color:#eef4f0;border-top:2px solid #2fbf71;padding:12px 16px;font:13px/1.5 system-ui,sans-serif;display:flex;gap:12px;align-items:center;flex-wrap:wrap;justify-content:center';
bar.innerHTML='<span style="max-width:640px">🍪 Deze site gebruikt alleen <b>functionele cookies</b> (winkelmandje, voorkeuren) — geen trackers, geen advertenties. Jouw gegevens blijven van jou.</span>';
var ok=document.createElement('button');ok.textContent='Oké, begrepen';
ok.style.cssText='background:#2fbf71;color:#04120a;border:0;border-radius:8px;padding:8px 16px;font-weight:700;cursor:pointer';
var no=document.createElement('button');no.textContent='Liever niet';
no.style.cssText='background:transparent;color:#93a69b;border:1px solid #263028;border-radius:8px;padding:8px 16px;cursor:pointer';
ok.onclick=function(){try{localStorage.setItem('g2b_consent','ok')}catch(e){};bar.remove()};
no.onclick=function(){try{localStorage.setItem('g2b_consent','no')}catch(e){};bar.remove()};
bar.appendChild(ok);bar.appendChild(no);
function show(){document.body.appendChild(bar)}
if(document.body)show();else document.addEventListener('DOMContentLoaded',show);
})();
