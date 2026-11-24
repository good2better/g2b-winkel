// G2B export-slot: demo open, export/download/print/copy achter licentiecode.
// Gebruik: if(!G2B_EXPORT('LEADFABRIEK'))return; bovenin de export-functie.
(function(){
  var CODES={
    FACTUURMAKER:'G2B-FACTUUR-D3K91P', LEADFABRIEK:'G2B-LEAD-L7Q2MW', CRMFABRIEK:'G2B-CRM-C8X4RT',
    LIJSTFABRIEK:'G2B-LIJST-V5N9KD', MAILFABRIEK:'G2B-MAIL-M2F7HB', MARKETINGFABRIEK:'G2B-MARK-P6T3ZN',
    SEQUENTIEFABRIEK:'G2B-SEQ-S9W4JC', SHEETFABRIEK:'G2B-SHEET-H4R8VG', LEADRADAR:'G2B-RADAR-R6D2KX',
    GAMEFABRIEK:'G2B-GAME-G3J7QF', SEOFABRIEK:'G2B-SEO-E8K5TD', WATCHERFABRIEK:'G2B-WATCH-W9C4NB',
    FLOWFABRIEK:'G2B-FLOW-F7M6PL', AGENCY:'G2B-AGENCY-A2V9SR', KANAALFABRIEK:'G2B-KANAAL-K4T8DW',
    SONICAUDIT:'G2B-SONIC-A7K3V9'
  };
  var MASTER=['G2B-CLUB-KVJF1N'];
  function modal(tool,cb){
    if(document.getElementById('g2bxslot'))return;
    var d=document.createElement('div');d.id='g2bxslot';
    d.innerHTML='<div style="position:fixed;inset:0;background:rgba(0,0,0,.82);z-index:99999;display:flex;align-items:center;justify-content:center;font-family:sans-serif">'+
    '<div style="background:#111a14;border:1px solid #2f5b41;border-radius:14px;padding:22px;max-width:340px;width:90%">'+
    '<b style="color:#35d07f">🔒 Export zit op slot</b>'+
    '<p style="color:#bcd;font-size:13px;line-height:1.5;margin:10px 0">Je kunt deze demo volledig gebruiken, maar om te downloaden/exporteren heb je de licentiecode van <b>'+tool+'</b> nodig. Die krijg je na aankoop op je bedankt-pagina.</p>'+
    '<input id="g2bxcode" placeholder="G2B-....-......" style="width:100%;box-sizing:border-box;padding:10px;border-radius:8px;border:1px solid #2f5b41;background:#0a0f0c;color:#fff;font-size:14px;letter-spacing:1px">'+
    '<div id="g2bxerr" style="color:#e66;font-size:12px;min-height:16px;margin-top:6px"></div>'+
    '<button id="g2bxok" style="width:100%;margin-top:8px;padding:11px;border:0;border-radius:8px;background:#35d07f;color:#04150c;font-weight:700;cursor:pointer">ONTGRENDEL</button>'+
    '<button id="g2bxno" style="width:100%;margin-top:6px;padding:9px;border:0;border-radius:8px;background:transparent;color:#789;cursor:pointer">terug naar demo</button>'+
    '<a href="https://good2better.com" target="_blank" style="display:block;text-align:center;color:#35d07f;font-size:12px;margin-top:10px">nog geen code? → good2better.com</a>'+
    '</div></div>';
    document.body.appendChild(d);
    function close(){d.remove()}
    d.querySelector('#g2bxno').onclick=close;
    d.querySelector('#g2bxok').onclick=function(){
      var c=d.querySelector('#g2bxcode').value.trim().toUpperCase();
      if(c===CODES[tool]||MASTER.indexOf(c)>-1){sessionStorage.setItem('g2b_export_'+tool,'1');close();cb(true)}
      else d.querySelector('#g2bxerr').textContent='code klopt niet — check je bedankt-pagina';
    };
    d.querySelector('#g2bxcode').addEventListener('keydown',function(e){if(e.key==='Enter')d.querySelector('#g2bxok').click()});
    setTimeout(function(){var i=d.querySelector('#g2bxcode');i&&i.focus()},50);
  }
  window.G2B_EXPORT_CODES=CODES;
  window.G2B_EXPORT=function(tool){
    tool=String(tool||'').toUpperCase();
    if(sessionStorage.getItem('g2b_export_'+tool)==='1')return true;
    try{if(localStorage.getItem('g2b_aura_licentie'))return true;}catch(e){}
    modal(tool,function(){});return false;
  };
  // async variant voor gebruik binnen handlers die mogen wachten
  window.G2B_EXPORT_THEN=function(tool,cb){
    tool=String(tool||'').toUpperCase();
    if(sessionStorage.getItem('g2b_export_'+tool)==='1'){cb();return}
    try{if(localStorage.getItem('g2b_aura_licentie')){cb();return}}catch(e){}
    modal(tool,function(ok){if(ok)cb()});
  };
})();
