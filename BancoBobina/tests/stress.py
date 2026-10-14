"""USB-powered test only: disconnect coil 12 V before --pulses."""
import argparse, concurrent.futures, json, time, urllib.request, urllib.parse, urllib.error
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--pulses',action='store_true');args=p.parse_args()
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
base='http://192.168.0.240'
latencies=[];count=0

def call(path,data=None,expected=200):
 global count
 encoded=None if data is None else urllib.parse.urlencode(data).encode()
 start=time.monotonic()
 req=urllib.request.Request(base+path,data=encoded,headers={'Connection':'close'})
 try:
  with opener.open(req,timeout=5) as r: status=r.status;body=r.read()
 except urllib.error.HTTPError as e: status=e.code;body=e.read()
 latencies.append(time.monotonic()-start);count+=1
 assert status==expected,(path,status,body)
 return json.loads(body) if status==200 else None

def config(state,period=10000,ton=3000,mask=0):
 return dict(periodUs=period,tonUs=ton,mask=mask,revision=state['revision'],bootId=state['bootId'])

report={};start=time.monotonic()
try:
 initial=call('/stop',{});boot=initial['bootId'];assert initial['ready'] and not initial['running']
 report['initial']=initial
 # Invalid, overflowing, incomplete and stale requests cannot change outputs.
 bad=[dict(periodUs=0),dict(tonUs=6000),dict(mask=1),dict(mask=2),dict(mask=4),dict(periodUs='4294967296'),dict(tonUs='-1'),dict(periodUs='1.2')]
 for changes in bad:
  data=config(initial);data.update(changes);call('/configure',data,400)
 call('/run',{},400)
 old=config(initial,mask=3);state=call('/stop',{});call('/run',old,409)
 current=call('/status');assert current['revision']==state['revision'] and not current['running']
 # Rapid valid edits while stopped, plus parallel status clients.
 for i in range(80):
  state=call('/configure',config(state,4000 if i%2 else 10000,100 if i%2 else 3000,3 if i%2 else 0))
  assert not state['running'] and state['bootId']==boot
 with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
  results=list(pool.map(lambda _:call('/status'),range(30)))
 assert all(s['bootId']==boot and not s['running'] for s in results)
 print('PASS: validation, stale starts, 80 rapid edits, 30 concurrent status reads.',flush=True)
 if args.pulses:
  timings=[(4000,1800),(4000,100),(10000,3000),(1000000,5000),(4001,1800),(100000,5000)]
  for i in range(60):
   period,ton=timings[i%len(timings)]
   state=call('/run',config(state,period,ton,3))
   assert state['running'] and state['bootId']==boot and state['timerFaults']==0,state
   # Keep running during status reads; repeated runs replace configuration atomically.
   if i%3==0:
    state=call('/status');assert state['running'] and state['timerFaults']==0,state
   if i%2:
    state=call('/stop',{});assert not state['running']
  print('PASS: 60 rapid starts/reconfigurations and 30 stops with both GPIO22/23.',flush=True)
  state=call('/run',config(state,4000,1800,3))
  for i in range(60):
   time.sleep(1)
   state=call('/status')
   assert state['bootId']==boot and state['running'] and state['timerFaults']==0,state
   if (i+1)%20==0:print(f'250 Hz / 45%: {i+1} s, heap={state["freeHeap"]}, faults=0',flush=True)
 state=call('/stop',{});assert not state['running']
 # Restore the usual settings, without selecting any GPIO.
 state=call('/configure',config(state))
 time.sleep(2)
 final=call('/status')
 assert final['bootId']==boot and not final['running'] and final['mask']==0
 assert final['timerFaults']==0 and final['freeHeap']>80000
 assert final['freeHeap']>=initial['freeHeap']-15000,(initial['freeHeap'],final['freeHeap'])
 report.update(result='PASS',final=final,requests=count,elapsedSeconds=round(time.monotonic()-start,2),maxLatencySeconds=round(max(latencies),3),meanLatencySeconds=round(sum(latencies)/len(latencies),3),pulseTest=args.pulses)
 print(json.dumps(report,indent=2),flush=True)
finally:
 try:call('/stop',{})
 except Exception as e:print('STOP NOT CONFIRMED:',e,flush=True)
 if report.get('result')=='PASS':Path(__file__).with_name('hardware-result.json').write_text(json.dumps(report,indent=2))
