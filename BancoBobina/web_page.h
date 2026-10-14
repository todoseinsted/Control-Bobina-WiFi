#pragma once
#include <Arduino.h>
const char htmlPage[] PROGMEM = R"HTML(
<!doctype html><html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Metatron · Control PWM</title>
<style>
:root{color-scheme:dark;font-family:system-ui,sans-serif;background:#101722;color:#edf2f7}*{box-sizing:border-box}body{margin:0}main{max-width:1040px;margin:auto;padding:26px 18px 50px}h1{margin:0;font-size:30px}h2{font-size:19px;margin-top:0}p{line-height:1.5}.muted,small{color:#b3c0d0}.top,.actions{display:flex;gap:12px;align-items:center;justify-content:space-between;flex-wrap:wrap}.card{background:#192433;border:1px solid #334155;border-radius:14px;padding:20px;margin-top:18px}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:18px}label{display:block}input[type=number]{display:block;width:100%;background:#0f1722;border:1px solid #607189;border-radius:7px;padding:11px;color:white;font-size:19px;margin:7px 0}button{border:0;border-radius:8px;padding:12px 16px;background:#33465e;color:white;font-size:15px;cursor:pointer}button:disabled{opacity:.45;cursor:not-allowed}.primary{background:#157767}.stop{background:#bb303f;font-weight:700}.badge{border-radius:30px;padding:9px 15px;background:#334155}.on{background:#157767}.message{min-height:25px;margin:14px 0 0}.error{color:#ffb4b4}.channels{width:100%;border-collapse:collapse;margin-top:16px}.channels td,.channels th{padding:12px 8px;border-bottom:1px solid #334155;text-align:left}.channels label{display:flex;align-items:center;gap:9px}input[type=checkbox]{width:22px;height:22px;accent-color:#30c9ac}.wave{display:flex;height:40px;overflow:hidden;border-radius:8px;margin:15px 0}.wave span{display:flex;align-items:center;justify-content:center;white-space:nowrap;min-width:0;font-size:13px}.a{background:#157767}.b{background:#365db4}.gap{background:#344153}#applied{font-variant-numeric:tabular-nums}@media(max-width:620px){.grid{grid-template-columns:1fr}.card{padding:15px}.channels td,.channels th{padding:10px 3px}h1{font-size:25px}.wave span{font-size:10px}}
.control-panel{position:sticky;top:0;z-index:5;box-shadow:0 8px 25px #0005;border:2px solid #526781}.main-buttons{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin:16px 0}.main-buttons button{min-height:66px;font-size:21px;font-weight:750}.guide{margin:10px 0 0;font-size:16px}.badge{font-weight:800}.on{background:#157767}summary{cursor:pointer;font-size:19px;font-weight:650;padding:3px 0 15px}.small-actions{margin-top:20px}#reason{margin:8px 0}.message:empty{display:none}@media(max-width:620px){.main-buttons button{font-size:17px;padding:12px 8px}.control-panel{padding:12px}main{padding-top:12px}}
</style></head><body><main>
<div class="top"><div><h1>Metatron · Control PWM</h1><p class="muted">GPIO22 + GPIO23 · Disparo simultáneo · IP 192.168.0.240</p></div><span id="runState" class="badge">Conectando…</span></div>
<section class="card control-panel" aria-label="Encendido y apagado">
<strong id="applied">Leyendo la placa…</strong><p id="reason" class="muted"></p>
<div class="main-buttons"><button class="primary" id="start" disabled>▶ Iniciar disparos</button><button class="stop" id="stop">■ Apagar disparos</button></div>
<p id="guide" class="guide">Habilitá el par GPIO22 + GPIO23.</p>
<p id="message" class="message" role="status" aria-live="polite"></p>
<small>Iniciar aplica tu selección y los tiempos elegidos. Apagar detiene todas las salidas.</small>
</section>
<div class="card"><h2>Habilitá las dos salidas simultáneas</h2><p class="muted">GPIO22 y GPIO23 suben y bajan juntos. Una única selección habilita el par; los demás GPIO no se utilizan.</p>
<div class="actions"><button id="all">Habilitar el par</button><button id="none">Deshabilitar el par</button><span id="count">0 / 2 seleccionados</span></div>
<table class="channels"><thead><tr><th>Salidas</th><th>Habilitación conjunta</th></tr></thead><tbody id="channels"></tbody></table></div>
<details class="card"><summary>Ajustar frecuencia y tiempos</summary><p class="muted">Ambos GPIO reciben el mismo pulso por período, sin alternancia.</p>
<div class="grid">
<label>Frecuencia por salida (Hz)<input id="frequency" type="number" min="1" max="250" step="0.1" value="100"><small>Mínima: 1 Hz · Máxima: 250 Hz</small></label>
<label>Período completo (ms)<input id="period" type="number" min="4" max="1000" step="0.001" value="10"><small>Período = 1000 / frecuencia</small></label>
<label>Ciclo de trabajo (%)<input id="duty" type="number" min="0.01" max="45" step="0.1" value="30"><small id="dutyLimits">Máximo: 45 %; limitado además por el tiempo de pulso</small></label>
<label>Tiempo encendido · Ton (ms)<input id="ton" type="number" min="0.1" max="5" step="0.001" value="3"><small id="tonLimits">De 0,1 a 5 ms, sujeto al período</small></label>
<label>Tiempo apagado · Toff (ms)<input id="gap" type="number" min="0.1" step="0.001" value="7"><small>Toff = período − Ton. Cambiarlo modifica Ton y el porcentaje.</small></label>
<div><p>Secuencia de un período</p><div class="wave"><span id="wa" class="a">22 + 23</span><span id="wg1" class="gap">Apagados</span></div><small id="sequence"></small></div>
</div>
<p class="muted">Límites del generador, no especificaciones de la Hellux HE11421. El archivo original usa 100 Hz, 30 % y pulsos de 3 ms. Se conserva el límite de 45 % de la versión inicial.</p>
<button id="defaults">Restablecer 100 Hz / 30 %</button><div class="actions small-actions"><button id="apply" disabled>Guardar cambios sin iniciar</button><button id="reload">Recuperar valores de la placa</button></div></details>
<p class="muted">Al encender: 100 Hz · 30 % · pulso de 3 ms · pausa de 7 ms. Los disparos arrancan apagados y todos los GPIO deshabilitados.</p>
<p class="muted">Cerrar esta página no apaga los disparos. Si se desconecta el Wi-Fi de la placa, se detienen y requieren un nuevo inicio.</p>
<small>Apagar lleva los GPIO a LOW; el estado eléctrico de la bobina depende del circuito PC817. Los tiempos mostrados son consignas, no mediciones.</small>
</main><script>
const $=id=>document.getElementById(id);

let pins=[],last=null,periodUs=10000,tonUs=3000,dirty=true,busy=false,online=false,invalidEdit=false,operation=0;
function message(text,error=false){$('message').textContent=text;$('message').classList.toggle('error',error);}
function chosenMask(){return document.querySelectorAll('.gpio:checked').length?3:0;}
function validation(){
 if(invalidEdit||!Number.isFinite(periodUs)||!Number.isFinite(tonUs))return 'Completá los campos con números válidos.';
 if(periodUs<4000||periodUs>1000000)return 'Usá una frecuencia de 1 a 250 Hz (4 a 1000 ms).';
 if(tonUs<100||tonUs>5000)return 'El pulso debe estar entre 0,1 y 5 ms.';
 if(tonUs*100>periodUs*45)return 'El ciclo de trabajo no puede superar 45 %.';
 if(periodUs-tonUs<100)return 'La pausa debe ser al menos 0,1 ms.';
 return '';
}
function controls(){
 $('apply').disabled=busy||!online||!!validation();
 $('start').disabled=busy||!online||!last||!last.ready||!!validation()||chosenMask()===0||(last.running&&!dirty);
 $('start').textContent=last&&last.running&&dirty?'Aplicar y reiniciar disparos':'▶ Iniciar disparos';
 $('guide').textContent=!online?'Sin conexión: no se puede confirmar el estado.':busy?'Enviando orden a la placa…':validation()||(!chosenMask()?'1. Habilitá el par GPIO22 + GPIO23 para empezar.':dirty?'2. Pulsá Iniciar para aplicar tu selección.':last&&last.running?'Disparos activos. Pulsá Apagar para detenerlos.':'Listo para iniciar los GPIO seleccionados.');
 document.querySelectorAll('input').forEach(el=>el.disabled=busy);
 for(const id of ['all','none','defaults','reload'])$(id).disabled=busy;
 $('count').textContent=(chosenMask()?2:0)+' / 2 seleccionados';
}
function fields(except=''){
 const values={frequency:1000000/periodUs,period:periodUs/1000,duty:tonUs*100/periodUs,ton:tonUs/1000,gap:(periodUs-tonUs)/1000};
 for(const [id,val] of Object.entries(values))if(id!==except)$(id).value=Number(val.toFixed(4));
 const maxTon=Math.min(5000,Math.floor(periodUs*.45),periodUs-100);
 $('tonLimits').textContent='Con este período: 0,1 a '+(maxTon/1000).toFixed(3)+' ms';
 $('dutyLimits').textContent='Con este período: '+(10000/periodUs).toFixed(2)+' a '+(maxTon*100/periodUs).toFixed(2)+' %';
 $('sequence').textContent='GPIO22 + GPIO23: '+(tonUs/1000).toFixed(3)+' ms encendidos → '+values.gap.toFixed(3)+' ms apagados';
 const duty=Math.max(0,Math.min(50,values.duty));
 $('wa').style.width=duty+'%';
 $('wg1').style.width=(100-duty)+'%';
 controls();
}
function edit(id){
 const value=Number($(id).value);dirty=true;
 invalidEdit=$(id).value.trim()===''||!Number.isFinite(value)||value<0||(id==='frequency'&&value===0);
 if(invalidEdit){message('Ingresá un número válido.',true);controls();return;}
 const oldDuty=tonUs/periodUs;
 if(id==='frequency'||id==='period'){
   periodUs=Math.round(id==='frequency'?1000000/value:value*1000);
   tonUs=Math.round(periodUs*oldDuty);
 } else if(id==='duty')tonUs=Math.round(periodUs*value/100);
 else if(id==='ton')tonUs=Math.round(value*1000);
 else tonUs=Math.round(periodUs-value*1000);
 fields(id);message(validation()||'Cambios pendientes de aplicar.',!!validation());
}
function buildChannels(){
 $('channels').innerHTML='<tr><td>GPIO22 + GPIO23</td><td><label><input class="gpio" type="checkbox" value="22">Habilitar ambos</label></td></tr>';
 document.querySelectorAll('.gpio').forEach(c=>c.addEventListener('change',()=>{dirty=true;controls();message('Selección preparada. Pulsá Iniciar disparos.');}));
}
function showStatus(s){
 if(last&&(s.revision!==last.revision||s.bootId!==last.bootId))dirty=true;
 last=s;pins=s.pins;online=true;
 $('runState').textContent=s.running?'● DISPARANDO':'○ APAGADOS';$('runState').classList.toggle('on',s.running);
 $('applied').textContent='Aplicado: '+(1000000/s.periodUs).toFixed(2)+' Hz · '+(100*s.tonUs/s.periodUs).toFixed(2)+' % · '+(s.tonUs/1000).toFixed(3)+' ms · '+pins.filter((p,i)=>s.mask&(1<<i)).length+' GPIO habilitados';
 $('reason').textContent=s.reason;controls();
}
function loadForm(s){
 periodUs=s.periodUs;tonUs=s.tonUs;invalidEdit=false;
 document.querySelectorAll('.gpio').forEach(c=>c.checked=!!(s.mask&(1<<pins.indexOf(Number(c.value)))));
 dirty=false;fields();
}
// A single request in flight. Stop replaces queued work and cancels its response.
let pending=null,inFlight=false,controller=null,pollTimer=null,activePath=null;
async function request(path,data){
 controller=new AbortController();
 const timeout=setTimeout(()=>controller.abort(),3000);
 try{
  const options=data===undefined?{cache:'no-store'}:{method:'POST',body:new URLSearchParams(data)};
  options.signal=controller.signal;
  const r=await fetch(path,options);
  if(!r.ok)throw new Error(await r.text());
  return await r.json();
 }finally{clearTimeout(timeout);controller=null;}
}
function schedulePoll(){
 clearTimeout(pollTimer);
 pollTimer=setTimeout(()=>{if(!document.hidden)refresh(false);else schedulePoll();},2000);
}
function enqueue(path,data,load=false){
 const poll=path==='/status';
 if(poll&&inFlight)return;
 if(path!=='/stop'&&busy)return;
 pending={path,data,load,id:++operation};
 if(!poll){busy=true;controls();}
 clearTimeout(pollTimer);
 if(controller)controller.abort();
 if(!inFlight)pump();
}
async function pump(){
 inFlight=true;
 while(pending){
  const job=pending;pending=null;activePath=job.path;
  try{
   const s=await request(job.path,job.data);
   if(job.id!==operation)continue;
   showStatus(s);
   if(job.load||job.path==='/configure'||job.path==='/run')loadForm(s);
   if(job.path!=='/status')message(job.path==='/stop'?'Disparos apagados.':s.reason);
  }catch(e){
   if(job.id!==operation)continue;
   online=false;$('runState').textContent='SIN CONFIRMACIÓN';$('runState').classList.remove('on');
   message('No se pudo confirmar el estado. '+(e.name==='AbortError'?'Tiempo de espera agotado.':e.message),true);
   // Never retry a start. If its reply is lost, attempt a stop instead.
   if(job.path==='/run')pending={path:'/stop',data:{},load:false,id:++operation};
  }
 }
 activePath=null;inFlight=false;busy=false;controls();schedulePoll();
}
function command(path,data){
 if(path!=='/stop'){
  if(!last||!online||busy)return;
  data={...data,revision:last.revision,bootId:last.bootId};
 }else{
  if(activePath==='/stop'||(pending&&pending.path==='/stop'))return;
  message('Apagando: esperando confirmación de la placa…');
 }
 enqueue(path,data);
}
function startSelected(){
 const error=validation();if(error||!chosenMask()){message(error||'Habilitá el par GPIO22 + GPIO23.',true);return;}
 command('/run',{periodUs,tonUs,mask:chosenMask()});
}
function refresh(load=false){enqueue('/status',undefined,load||!last);}
buildChannels();fields();
for(const id of ['frequency','period','duty','ton','gap'])$(id).addEventListener('input',()=>edit(id));
$('defaults').onclick=()=>{periodUs=10000;tonUs=3000;invalidEdit=false;dirty=true;fields();message('100 Hz y 30 % listos para aplicar.');};
for(const [id,checked] of [['all',true],['none',false]])$(id).onclick=()=>{document.querySelectorAll('.gpio').forEach(c=>c.checked=checked);dirty=true;controls();message('Selección preparada. Pulsá Iniciar disparos.');};
$('apply').onclick=()=>{const error=validation();if(error){message(error,true);return;}command('/configure',{periodUs,tonUs,mask:chosenMask()});};
$('start').onclick=startSelected;
$('stop').onclick=()=>command('/stop',{});
$('reload').onclick=()=>refresh(true);
refresh(true);
</script></body></html>
)HTML";
