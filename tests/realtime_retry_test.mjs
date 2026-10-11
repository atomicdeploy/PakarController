// Actual Rayan sync reconnect-before-open/stop scenarios; shipped JS in Node VM.
// No donor build, physical observation, fabricated event sequence or write replay.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const read=p=>readFileSync(new URL('../'+p,import.meta.url),'utf8').replaceAll('\r\n','\n');
const module=read('assets/web/realtime-retry.js'),web=read('firmware/esp8266/src/WebUi.h');
const context=vm.createContext({AbortController});
vm.runInContext(module,context);
const create=vm.runInContext('createRealtimeRetry',context);
function harness(extra={}){
  let serial=0,available=true,connections=0;
  const timers=new Map(),errors=[],order=[];
  const prepare=async()=>{order.push('readback')};
  const controller=create({enabled:()=>available,prepare,connect:()=>{order.push('socket');connections++},
    onError:error=>errors.push(error),random:()=>0.5,
    setTimer:(run,delay)=>{const id=++serial;timers.set(id,{run,delay});return id},clearTimer:id=>timers.delete(id),...extra});
  const fire=async()=>{const [id,timer]=timers.entries().next().value;timers.delete(id);await timer.run();return timer.delay};
  return {controller,timers,errors,order,fire,get connections(){return connections},disable(){available=false}};
}
// Donor prepareReconnect is completed before any replacement socket is opened.
const first=harness();assert.equal(first.controller.schedule(),true);
assert.equal(first.controller.schedule(),false,'one pending retry owner');
assert.equal(await first.fire(),500);assert.deepEqual(first.order,['readback','socket']);
assert.equal(first.timers.size,0,'readback deadline released');
assert.equal(await (first.controller.schedule(),first.fire()),1000);
first.controller.cancel();assert.equal(await (first.controller.schedule(),first.fire()),500,'open/manual reset restores first attempt');
// Exact donor 500ms exponential base, 10-second cap, 0.8..1.2 jitter.
const backoff=harness();
for(const delay of [500,1000,2000,4000,8000,10000,10000,10000]){
  backoff.controller.schedule();assert.equal(await backoff.fire(),delay);
}
for(const [random,delay]of [[0,400],[0.5,500],[1,600]]){
  const sample=harness({random:()=>random});sample.controller.schedule();
  assert(Math.abs(await sample.fire()-delay)<1e-9,'floating-point jitter retains donor bounds');
}
// Failure schedules one retry; callback errors cannot suppress recovery.
let failures=0;
const failing=harness({prepare:async()=>{failures++;throw Error('readback failed')}});
failing.controller.schedule();await failing.fire();assert.equal(failures,1);
assert.equal(failing.errors.length,1);assert.equal(failing.timers.size,1);
assert.equal([...failing.timers.values()][0].delay,1000);failing.controller.cancel();
assert.equal(failing.timers.size,0);
// Stop prevents late prepare/old queued callbacks from reopening a socket.
let complete,receivedSignal;
const pending=harness({prepare:signal=>{receivedSignal=signal;return new Promise(resolve=>complete=resolve)}});
pending.controller.schedule();const inFlight=pending.fire();await Promise.resolve();
assert.equal([...pending.timers.values()][0].delay,12000);
pending.controller.cancel();assert(receivedSignal.aborted);assert.equal(pending.timers.size,0);complete();await inFlight;
assert.equal(pending.connections,0);assert.equal(pending.timers.size,0);
pending.controller.schedule();const stale=[...pending.timers.values()][0].run;
pending.controller.cancel();pending.controller.schedule();await stale();
assert.equal(pending.timers.size,1,'stale timer must not disarm the newer generation');pending.controller.cancel();
// Actual api.ts AbortController deadline/finally cleanup, only reconnect status GET.
const timed=harness({prepare:signal=>new Promise((resolve,reject)=>signal.addEventListener('abort',()=>reject(Object.assign(Error('timed out'),{name:'AbortError'}))))});
timed.controller.schedule();const timedFlight=timed.fire();await Promise.resolve();
const timeout=[...timed.timers.values()][0];assert.equal(timeout.delay,12000);timeout.run();await timedFlight;
assert.equal(timed.connections,0);assert.equal(timed.errors[0].name,'AbortError');assert.equal(timed.timers.size,1);timed.controller.cancel();
const disabled=harness();disabled.disable();assert.equal(disabled.controller.schedule(),false);
// Shipped integration passes the cancellation signal through actual API GET.
assert(web.includes("async function refreshAuthenticated(options={})"));
assert(web.includes("'?pixels=1':''),options)"));
assert(web.includes('if(!options.signal?.aborted)render(next)'));
assert(module.includes('prepare:signal=>refreshAuthenticated({signal})'));
assert(!module.includes("method:'POST'")&&!module.includes('establishSession(')&&!module.includes('localStorage'));
assert(web.includes('function showLogin(){webSocketReconnect.cancel();'));
assert(web.includes("'pagehide',()=>{webSocketReconnect.cancel();"));
assert(web.includes('function pauseWebSocket(){webSocketPaused=true;webSocketReconnect.cancel();'));
assert(!web.includes('webSocketRetry='));
assert.equal(web.split('// BEGIN RAYANLAMP REALTIME RETRY\n')[1].split('\n// END RAYANLAMP REALTIME RETRY')[0],module);

// Execute actual Web connectWs + refreshAuthenticated + api, not just a helper.
const sockets=[],requests=[],liveTimers=new Map(),activity=[];
let timerID=0,status=200,constructorFails=false;
const live=vm.createContext({AbortController,Math:Object.assign(Object.create(Math),{random:()=>0.5}),location:{protocol:'https:',host:'ledboard.local'},
  cellShape:{value:'rounded'},wsState:{},render:value=>activity.push('render:'+value.hostname),
  stopStream(){},traceBrowserEvent(){},browserErrorMetadata:()=>({}),
  setTimeout:(run,delay)=>{const id=++timerID;liveTimers.set(id,{run,delay});return id},
  clearTimeout:id=>liveTimers.delete(id),
  fetch:async(path,options)=>{requests.push({path,options});activity.push('GET');return {status,ok:status===200,text:async()=>JSON.stringify({hostname:'verified-peer'})}},
  WebSocket:class {constructor(url){if(constructorFails)throw Error('socket unavailable');this.url=url;this.sent=[];sockets.push(this);activity.push('socket')}close(){}send(value){this.sent.push(value)}}
});
vm.runInContext('let authenticated=true,webSocketPaused=false,socket=null;\n'+module+'\n'+
  web.slice(web.indexOf('async function api('),web.indexOf('function showLogin()'))+'\n'+
  web.split('\n').find(line=>line.startsWith('async function refreshAuthenticated('))+'\n'+
  web.slice(web.indexOf('function connectWs()'),web.indexOf('async function post('))+
  '\nfunction showLogin(){authenticated=false;webSocketReconnect.cancel()}',live);
const execute=code=>vm.runInContext(code,live);
const fireLive=async()=>{const[id,timer]=liveTimers.entries().next().value;liveTimers.delete(id);await timer.run();return timer.delay};
execute('connectWs()');sockets[0].onopen();assert.equal(sockets[0].url,'wss://ledboard.local/ws');
sockets[0].onclose({code:1006});assert.equal(await fireLive(),500);
assert.equal(sockets.length,2);assert.deepEqual(activity.slice(-3),['GET','render:verified-peer','socket']);
assert.equal(requests[0].path,'/api/status');assert.equal(requests[0].options.credentials,'same-origin');
assert.equal(requests[0].options.method,undefined,'session preparation is read-only GET');
assert(requests[0].options.signal instanceof AbortSignal);
assert.equal(liveTimers.size,0);
// Expired session stops recovery; no donor-style automatic mutating renewal.
status=401;sockets[1].onclose({code:1006});await fireLive();
assert.equal(sockets.length,2);assert.equal(liveTimers.size,0);assert.equal(execute('authenticated'),false);
// Constructor failures get the same bounded recovery, not an unhandled throw.
constructorFails=true;execute('authenticated=true;connectWs()');assert.equal(liveTimers.size,1);
assert.equal(execute('socket'),null,'failed replacement relinquishes old socket ownership');
assert.equal(live.wsState.textContent,'WebSocket retrying');
constructorFails=false;status=200;await fireLive();assert.equal(sockets.length,3);
execute('webSocketPaused=true;webSocketReconnect.cancel()');sockets[2].onclose({code:1006});
assert.equal(liveTimers.size,0);
// Even an injected fetch ignoring abort cannot repaint capabilities after logout.
let finishRead;
live.fetch=()=>new Promise(resolve=>finishRead=resolve);
execute('webSocketPaused=false;authenticated=true');sockets[2].onclose({code:1006});
const lateRead=fireLive();await Promise.resolve();const paintBefore=activity.length;
execute('showLogin()');assert.equal(liveTimers.size,0);
finishRead({status:200,ok:true,text:async()=>'{"hostname":"obsolete-peer"}'});await lateRead;
assert.equal(activity.length,paintBefore);assert.equal(sockets.length,3);
const rename=web.split('\n').find(line=>line.startsWith("nameForm.addEventListener('submit'"));
assert(rename.includes('return}webSocketReconnect.cancel();stopStream();'));
console.log('Rayan realtime retry: readback-before-open, exact capped jitter, timer/generation ownership, cancellation, 12s read deadline and no write replay passed; VM proof, not LAN.');
