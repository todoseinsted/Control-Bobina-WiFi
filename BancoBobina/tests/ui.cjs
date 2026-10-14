const fs=require('fs'),vm=require('vm'),assert=require('assert/strict');
const html=fs.readFileSync(__dirname+'/../web_page.h','utf8');
const source=html.match(/<script>([\s\S]*?)<\/script>/)[1];
const gpioPins=[13,14,26,33,22,19,5,16,2,27,12,25,32,23,18,17,4,15];
const fields=new Map(),checks=gpioPins.map(value=>({value:String(value),checked:false,addEventListener(){}}));
function element(id){if(!fields.has(id))fields.set(id,{value:'',textContent:'',style:{},classList:{toggle(){},remove(){}},addEventListener(){}});return fields.get(id);}
const jobs=[],timers=new Map();let timerId=0,active=0,maxActive=0;
const state={periodUs:10000,tonUs:3000,mask:0,running:false,revision:0,bootId:17,ready:true,reason:'detenido',pins:gpioPins};
const context=vm.createContext({console,URLSearchParams,AbortController,document:{hidden:false,getElementById:element,querySelectorAll(sel){if(sel==='.gpio:checked')return checks.filter(c=>c.checked);return checks;}},
 setTimeout(fn,ms){timers.set(++timerId,{fn,ms});return timerId;},clearTimeout(id){timers.delete(id);},
 fetch(path,options){active++;maxActive=Math.max(active,maxActive);return new Promise((resolve,reject)=>{
  let done=false;const end=fn=>{if(done)return;done=true;active--;fn();};
  jobs.push({path,options,reply(s=state,status=200){end(()=>resolve({ok:status===200,json:async()=>({...s}),text:async()=>'rechazado'}));}});
  options.signal.addEventListener('abort',()=>end(()=>reject(Object.assign(new Error('aborted'),{name:'AbortError'}))));
 });}
});
const run=s=>vm.runInContext(s,context),flush=async()=>{for(let i=0;i<5;i++)await new Promise(setImmediate);};
(async()=>{
 run(source);assert.equal(jobs.length,1);assert.equal(jobs[0].path,'/status');
 for(let i=0;i<100;i++)run('refresh()');assert.equal(jobs.length,1,'polls must not overlap');
 jobs[0].reply();await flush();checks[0].checked=true;
 run('startSelected()');assert.equal(jobs.at(-1).path,'/run');
 const payload=jobs.at(-1).options.body;assert.equal(payload.get('revision'),'0');assert.equal(payload.get('bootId'),'17');
 // Stop while start is pending; cancel its response and send exactly one stop.
 for(let i=0;i<100;i++)run("command('/stop',{})");await flush();
 assert.equal(jobs.at(-1).path,'/stop');assert.equal(jobs.filter(j=>j.path==='/stop').length,1);
 for(let i=0;i<100;i++)run("command('/stop',{})");await flush();
 assert.equal(jobs.filter(j=>j.path==='/stop').length,1,'active stop must be coalesced');
 jobs.at(-1).reply({...state,revision:2});await flush();assert.equal(run('busy'),false);
 assert.equal(element('runState').textContent,'○ APAGADOS');
 // A lost start reply must trigger stop, never repeat start.
 run('startSelected()');const startCount=jobs.filter(j=>j.path==='/run').length;
 const timeout=[...timers.values()].find(t=>t.ms===3000);assert.ok(timeout);timeout.fn();await flush();
 assert.equal(jobs.at(-1).path,'/stop');assert.equal(jobs.filter(j=>j.path==='/run').length,startCount);
 jobs.at(-1).reply({...state,revision:4});await flush();
 // User commands interrupt a status poll, without parallel fetches.
 run('refresh()');run('startSelected()');await flush();assert.equal(jobs.at(-1).path,'/run');
 jobs.at(-1).reply({...state,mask:1,running:true,revision:5});await flush();
 assert.equal(element('runState').textContent,'● DISPARANDO');
 // Invalid input never reaches the server.
 const before=jobs.length;run('periodUs=100;startSelected()');assert.equal(jobs.length,before);
 // A failed poll marks the state unknown; a following successful poll recovers it.
 run('refresh()');jobs.at(-1).reply(state,500);await flush();assert.equal(run('online'),false);
 run('refresh()');jobs.at(-1).reply({...state,revision:6});await flush();assert.equal(run('online'),true);
 assert.equal(maxActive,1,'at most one fetch may be active');
 assert.equal(run('busy'),false);assert.equal(run('inFlight'),false);
 console.log('PASS: serialization, 200 repeated stops, stale response cancellation, start timeout -> stop, polling, validation and recovery.');
})().catch(e=>{console.error(e);process.exitCode=1;});
