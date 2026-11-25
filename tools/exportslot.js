// G2B export-slot: demo open, export/download/print/copy achter licentiecode.
// v2: codes staan als SHA-256-hashes in de bron — niet meer plaintext leesbaar op GitHub.
// Nieuwe tool-code toevoegen: sha256(code in HOOFDLETTERS) hier plakken.
(function(){
  var THASH={
    FACTUURMAKER:'e4b52b892b120e0ffd6594e59f61f3657e210baab82d0d04f92d1646ad3c8a9b',
    LEADFABRIEK:'22a1400bae7f22a005505638effa64f72332bac914179db68711749adc66cf3b',
    CRMFABRIEK:'26e4c1219822298b59774b2eb47d9ab186f502d0906caea3c77a3eef3fec4c3a',
    LIJSTFABRIEK:'3dd71d9fa0061c1eaf1617506facdb23f3af9ce34bab6cdb6ec993aecec9810a',
    MAILFABRIEK:'7aa2b446ad9e72a2b167c9205b1f04c1b7b2fd8f2cdd6a3f3de26fb0aa093386',
    MARKETINGFABRIEK:'d6f3865f71d079eb512ee228ea6e4b4cdac9321334d60463b76b951d3b39f568',
    SEQUENTIEFABRIEK:'838efd3edffba8fcc92bf2d6adcc42f67283c0fcb4904bd3674628818871ee77',
    SHEETFABRIEK:'39da276bb4a573667f7d50adba2c70274bac2273f2bab8164531b7be1cfb017f',
    LEADRADAR:'3b1a79c7793ceef37da276994657a02e7d573e22b066c10d1a4d924859dd1e25',
    GAMEFABRIEK:'1ebcf672d396a273834ac4fc88050e3c24586740684f87f2256f0531efd2dee2',
    SEOFABRIEK:'64dc6eab8b0c6ad7edcbc82a6d95436780bb4cff0b70fc43f7f064edcf0b55de',
    WATCHERFABRIEK:'756ffba70aa36d8052f360c91478e004bb665530261bdf3910155c01f280983e',
    FLOWFABRIEK:'0adb5696e21db85ccfc90762a8571c0d23c2801b173013c58a2dfa575de24010',
    AGENCY:'ea601aa5b57e53b0085065045b1d21f9f81ea439505bd6a2f1b87936df7a0c40',
    KANAALFABRIEK:'6592c966a5a9135cd4712c10ae978af86d64b67258ff0f1021832aa68ad685ec',
    SONICAUDIT:'97c014705cbad523dfee993b79aaf2de38bea93025865d6eebbb08172e139cee',
    MELODIE:'d1601949c9d2117e077728a283e046e28d7b735051770c8fcae420ff506cd4e9',
    BASFABRIEK:'926a43dbaf2a0447edd1b6d86da48702b233f70430f1439d2740e82327220427',
    BIBLIOTHEEK:'ab2b92b50e1472096a3339531b6faab877faa8a76625dbb60ce88f8bc5fdd002',
    MIDIBIB:'49535746066e3319bde5d4b807c1610eb3f4ac3ec33895d59c21c62f95c929a0',
    LAAGBAAS:'52a51c76168884acd6804b22ebd18789b127a1d0e21b8e5edf6882f01671cf7b',
    MENGERWEB:'5b1eafa122cecc67b90ac079d61a495d479df4566dfd7a0885b8600eda19a607',
    GAW:'8594293a12de60194e052b30538d2d0f226ad77c31716ac861d9aca3023b0209',
    GAWMINI:'bd726ba978dfda3947b6e69b3f230f851ffdcdafec28ce223c1f5dad9623407b'
  };
  var MHASH=['e7cca66310a7920e3504d054a377e64fa39423e5ddc283a8afc7fa091b881972'];
  // lidmaatschap-licenties (mijn-apps + club + vrienden) — lowercase genormaliseerd
  var LIDHASH=['a6027a131916280eab4a9f091cf28b36eb1dc59285023ce8c2403a76f24ff982','9d61007b8a32bcce2ae1c1d8b35a5d90457ec99875c1fd33122c72419a1940e7','812a199d4a4024b7a5449886a886c464e3113868277798accb8aa8e6426be87d','57907b5fa536b2a1a5c8b836b59244e2c0c49bc7e789711599d1bb9fa24530bf','70f0dc433307319601a029cee9e9e03d98a6d0678bc70a704f810c0303f36842','a7e537756133cb285d3fa7c170b680623a851a203ea520d052f6c514c2e0c306','1c138173c0ea24af960f15f7846bc88df2c29d7c902f22f98afc6b9a82fdd7cd','342a7d7e48f648e393bbbc465025bde05e72d00a5c1925f3cc2e40d6fbe2a2de','1925ea999ef01bb59df7936944d5d6e2dfe67c464a62601c8f22f6c3aac54937','582489f8427e52f22e274c1c9a8533ea65028b359adcf776c1b918296f8ab77a','fbd8692638e8aeeaf2e7850126837a9de0e52d2e807ae9208cecd88840498263','cb536bed2b20215cd6555c8964ae6e39f6a9465d51ca1027f27b93e986650003','97664e3b00a0c393902216bcea93eca56392a3ab8fb7572db877b968dff8d0cf','24bc9637971213048fffedfd12ba0b24581d1abb2c22632d31a1dd534dfab5e3','200c9937843f1cf79760af4b8dce705e34ee2290928664167989a055a9fbd165','8d39a937e5e7af3a2e8df7286b9bebc1eeb9231f121d7d11dab0d38da6186af7','7438707c7c2f633aaed3697ad315dc8d17ae608ce7bf09d092e5ccf90f18b119','c48d74f6bba57ede80d4d758ce7053b862218eb68ecccf3f9f26f251a0ebef84','dc86a4f55e723dd0709293bee7318c3c75624743d5c367a42528deec5f98ae03','01a13a8a9a784233e42aefa2f32ec447325a4a15c418b14011d056af6fbe3ced','0d8b3b19ac6ac775d0814049f688461e1ee7513f69a384e598432b7358455255','5b7106a0e4531689d6364d391e803b3e1a93c6a92a7428c46644d9303ae02ebe'];
  function rr(v,a){return (v>>>a)|(v<<(32-a))}
  function sha(ascii){
   var mp=Math.pow,mx=mp(2,32),ln=ascii.length*8,result='',words=[];
   var hash=[0x6A09E667,0xBB67AE85,0x3C6EF372,0xA54FF53A,0x510E527F,0x9B05688C,0x1F83D9AB,0x5BE0CD19];
   var k=[0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2];
   ascii=unescape(encodeURIComponent(ascii));
   ascii+='\x80';
   while(ascii.length%64-56)ascii+='\x00';
   for(var i=0;i<ascii.length;i++){var j=ascii.charCodeAt(i);words[i>>2]|=j<<((3-i)%4)*8}
   words[words.length]=((ln/mx)|0);words[words.length]=(ln);
   for(var j=0;j<words.length;){
    var w=words.slice(j,j+=16),o=hash;hash=hash.slice(0,8);
    for(var i=0;i<64;i++){
     var w15=w[i-15],w2=w[i-2];
     var a=hash[0],e=hash[4];
     var t1=hash[7]+(rr(e,6)^rr(e,11)^rr(e,25))+((e&hash[5])^(~e&hash[6]))+k[i]
      +(w[i]=i<16?w[i]:(w[i-16]+(rr(w15,7)^rr(w15,18)^(w15>>>3))+w[i-7]+(rr(w2,17)^rr(w2,19)^(w2>>>10)))|0);
     var t2=(rr(a,2)^rr(a,13)^rr(a,22))+((a&hash[1])^(a&hash[2])^(hash[1]&hash[2]));
     hash=[(t1+t2)|0].concat(hash);hash[4]=(hash[4]+t1)|0;
    }
    for(var i=0;i<8;i++)hash[i]=(hash[i]+o[i])|0;
   }
   for(var i=0;i<8;i++)for(var j=3;j+1;j--){var b=(hash[i]>>(j*8))&255;result+=((b<16)?0:'')+b.toString(16)}
   return result;
  }
  function licOk(){
   try{
    var v=(localStorage.getItem('g2b_aura_licentie')||'').trim().toLowerCase();
    if(v&&v!=='ok'&&v.length>=4)return LIDHASH.indexOf(sha(v))>=0;
   }catch(e){}
   return false;
  }
  function check(tool,code){
   tool=String(tool||'').toUpperCase();
   var c=String(code||'').trim().toUpperCase();
   if(!c)return false;
   var h=sha(c);
   return h===THASH[tool]||MHASH.indexOf(h)>=0;
  }
  function which(code){
   var c=String(code||'').trim().toUpperCase();
   if(!c)return '';
   var h=sha(c);
   if(MHASH.indexOf(h)>=0)return 'MASTER';
   for(var k in THASH)if(THASH[k]===h)return k;
   return '';
  }
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
      if(check(tool,c)){sessionStorage.setItem('g2b_export_'+tool,'1');close();cb(true)}
      else d.querySelector('#g2bxerr').textContent='code klopt niet — check je bedankt-pagina';
    };
    d.querySelector('#g2bxcode').addEventListener('keydown',function(e){if(e.key==='Enter')d.querySelector('#g2bxok').click()});
    setTimeout(function(){var i=d.querySelector('#g2bxcode');i&&i.focus()},50);
  }
  window.G2B_EXPORT_CHECK=check;
  window.G2B_EXPORT_WHICH=which;
  window.G2B_EXPORT=function(tool){
    tool=String(tool||'').toUpperCase();
    if(sessionStorage.getItem('g2b_export_'+tool)==='1')return true;
    if(licOk())return true;
    modal(tool,function(){});return false;
  };
  window.G2B_EXPORT_THEN=function(tool,cb){
    tool=String(tool||'').toUpperCase();
    if(sessionStorage.getItem('g2b_export_'+tool)==='1'){cb();return}
    if(licOk()){cb();return}
    modal(tool,function(ok){if(ok)cb()});
  };
})();
