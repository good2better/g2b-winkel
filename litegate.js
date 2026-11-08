/* G2B LITE-GATE — gratis downloads vragen eerst naam/land/email/rol.
   Werkt op index.html (productkaarten) en p.html (detailpagina):
   - kaart: <a class="koop" data-buy="id"> van een product met prijs GRATIS
   - detail: <a class="koop" data-gate="1"> (p.html zet dit bij gratis producten)
   Gegevens gaan naar de E-mailvanger (Google Sheet) met bron lite-<product>;
   daarna start de download gewoon door. */
(function(){
  var EV_URL='https://script.google.com/macros/s/AKfycbwbGZ-zxrQgxEFJbUhv2-HJ7yNP1yXb66W2wo4tEWtXyx7PcDsFiGdh619QT-6LlTXC/exec';
  var ROLLEN=['producer','engineer','artist','supervisor','dj','iets anders'];
  var modal=null,pending=null;

  function css(){
    if(document.getElementById('lgCss'))return;
    var s=document.createElement('style');s.id='lgCss';
    s.textContent=''
      +'.lgBg{position:fixed;inset:0;background:rgba(4,8,12,.78);z-index:9999;display:flex;align-items:center;justify-content:center;padding:18px}'
      +'.lgBox{background:#0E141C;border:1px solid #2A3D52;border-radius:18px;padding:24px;width:min(430px,96vw);box-shadow:0 18px 60px rgba(0,0,0,.6)}'
      +'.lgBox h2{color:#3DDC84;font-size:20px;margin:0 0 4px;letter-spacing:1px}'
      +'.lgBox .sub{color:#8AA0B5;font-size:13px;margin-bottom:16px}'
      +'.lgBox label{display:block;color:#7CF5B0;font-size:12px;font-weight:700;margin:10px 0 4px;letter-spacing:.5px}'
      +'.lgBox input,.lgBox select{width:100%;padding:11px 12px;border-radius:10px;border:1px solid #2A3D52;background:#111A24;color:#E8F0EA;font-size:14px;box-sizing:border-box}'
      +'.lgBox .go{width:100%;margin-top:18px;padding:13px;border:none;border-radius:12px;background:linear-gradient(135deg,#3DDC84,#25C46E);color:#06120B;font-size:15px;font-weight:800;letter-spacing:1px;cursor:pointer}'
      +'.lgBox .annuleer{display:block;text-align:center;margin-top:10px;color:#5B7086;font-size:12.5px;cursor:pointer}'
      +'.lgErr{color:#FF7B7B;font-size:12.5px;margin-top:8px;display:none}';
    document.head.appendChild(s);
  }

  function toon(href,productId,titel){
    css();
    if(modal)modal.remove();
    pending={href:href,id:productId};
    modal=document.createElement('div');modal.className='lgBg';
    modal.innerHTML=''
      +'<div class="lgBox">'
      +'  <h2>GRATIS DOWNLOAD</h2>'
      +'  <div class="sub">'+(titel?('<b style="color:#E8F0EA">'+titel+'</b> — '):'')
      +'  Vul je gegevens in en de download start direct. Geen account, geen kosten.</div>'
      +'  <label>Naam</label><input id="lgNaam" type="text" autocomplete="name">'
      +'  <label>Land</label><input id="lgLand" type="text" autocomplete="country-name">'
      +'  <label>E-mail</label><input id="lgMail" type="email" autocomplete="email" placeholder="jij@voorbeeld.com">'
      +'  <label>Ik ben een…</label><select id="lgRol">'+ROLLEN.map(function(r){return '<option>'+r+'</option>';}).join('')+'</select>'
      +'  <div class="lgErr" id="lgErr">Vul je e-mailadres in (met een @ erin).</div>'
      +'  <button class="go" id="lgGo">⬇ START DOWNLOAD</button>'
      +'  <span class="annuleer" id="lgX">annuleren</span>'
      +'</div>';
    document.body.appendChild(modal);
    modal.addEventListener('click',function(e){if(e.target===modal)sluit();});
    modal.querySelector('#lgX').onclick=sluit;
    modal.querySelector('#lgGo').onclick=go;
    setTimeout(function(){modal.querySelector('#lgNaam').focus();},50);
  }
  function sluit(){if(modal){modal.remove();modal=null;}}
  // g2bGate(cb,bron): dezelfde modal voor in-tool exports — na gegevens roept cb() het echte download
  window.g2bGate=function(cb,bron){toon('',bron||'export','');pending.cb=cb;};
  function go(){
    var em=(modal.querySelector('#lgMail').value||'').trim().toLowerCase();
    if(!em||em.indexOf('@')<1){modal.querySelector('#lgErr').style.display='block';return;}
    var naam=modal.querySelector('#lgNaam').value.trim();
    var land=modal.querySelector('#lgLand').value.trim();
    var rol=modal.querySelector('#lgRol').value;
    var href=pending.href,pid=pending.id;
    try{
      fetch(EV_URL,{method:'POST',mode:'no-cors',headers:{'Content-Type':'text/plain'},
        body:JSON.stringify({email:em,bron:'lite-'+pid,tijd:new Date().toISOString(),
          extra:{naam:naam,land:land,rol:rol,product:pid}})});
    }catch(e){}
    sluit();
    var cb=pending&&pending.cb;
    if(cb){cb();return;}
    // echte download starten na de gate
    var a=document.createElement('a');a.href=href;a.target='_blank';a.rel='noopener';
    document.body.appendChild(a);a.click();a.remove();
  }

  document.addEventListener('click',function(e){
    var a=e.target.closest&&e.target.closest('a.koop');
    if(!a)return;
    var pid=a.getAttribute('data-buy')||'';
    var gratis=false;
    if(a.hasAttribute('data-gate')){gratis=true;pid=pid||((location.search.match(/id=([^&]+)/)||[])[1]||'');}
    else if(pid){
      var p=(window._prods||[]).find(function(x){return x.id===pid;});
      gratis=p&&(p.prijs==='GRATIS');
    }
    if(!gratis)return;
    // alleen echte bestands-downloads gaten — een link naar een .html pagina is geen download
    var href=a.href||'';
    if(/\.html?(\?|#|$)/i.test(href))return;
    e.preventDefault();e.stopPropagation();
    var t=a.textContent||'';
    toon(a.href,pid,t);
  },true);
})();
