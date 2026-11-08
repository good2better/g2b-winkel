// G2B Web MIDI — keyboard inprikken = pads bespelen (MPC-layout: noot 36+ = pad 0+)
(function(){
 if(!navigator.requestMIDIAccess)return;
 const lbl=document.createElement('div');
 lbl.style.cssText='position:fixed;left:10px;bottom:8px;font-size:.62rem;opacity:.55;pointer-events:none;color:var(--dim,#8b98a5)';
 document.body.appendChild(lbl);
 function hit(i){const p=document.getElementById('p'+i)||document.querySelectorAll('.pad')[i];if(p){p.classList.add('hit');setTimeout(()=>p.classList.remove('hit'),140)}}
 function msg(e){const[s,n,v]=e.data;const t=s&0xf0;
  if(t===0x90&&v>0){const i=(n-36)%16;const idx=i<0?n%16:i;
   if(!(window.G2BKB&&window.G2BKB.on)){try{speel(idx)}catch(err){}}
   hit(idx);if(window.onMidiNote)try{onMidiNote(n,idx,v)}catch(err){}}
  else if(t===0x80||(t===0x90&&v===0)){if(window.onMidiOff)try{onMidiOff(n)}catch(err){}}}
 navigator.requestMIDIAccess().then(m=>{
  function scan(){let c=0;m.inputs.forEach(i=>{i.onmidimessage=msg;c++});
   lbl.textContent=c?('🎹 MIDI: '+[...m.inputs.values()][0].name):'🎹 geen keyboard';
   lbl.style.opacity=c?'.85':'.4'}
  scan();m.onstatechange=scan;
 }).catch(()=>{lbl.textContent='🎹 MIDI uit'});
})();
