// Actual RayanLamp browser-console.test.ts scenarios against the shipped module.
// Node VM replaces Vitest; no donor build, browser dependency or SDK cache.
import assert from 'node:assert/strict';
import {webcrypto} from 'node:crypto';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import {spawnSync} from 'node:child_process';
const root=new URL('../',import.meta.url);
const read=p=>readFileSync(new URL(p,root),'utf8').replaceAll('\r\n','\n');
const module=read('assets/web/browser-console.js'),web=read('firmware/esp8266/src/WebUi.h');
const calls=[],sink=Object.fromEntries(['debug','info','log','warn','error','groupCollapsed','groupEnd'].map(method=>[method,(...args)=>calls.push({method,args})]));
let clock=100;
const context=vm.createContext({console:sink,clock:()=>clock,
  document:{documentElement:{dataset:{palette:'studio',theme:'dark'}}},
  PEALAYER_PALETTES:JSON.parse(read('assets/themes/palettes.json'))});
vm.runInContext(module+'\nglobalThis.output=new StyledBrowserConsole(console,clock);',context);
const run=code=>vm.runInContext(code,context);
const clear=()=>{calls.length=0};
const events=()=>calls.filter(c=>c.args[4]?.event);
clear();
assert.equal(run('output.start({source:"web",unexpected:"no"})'),true);
assert.equal(run('output.start()'),false);
for(const method of ['groupCollapsed','info','debug','log','groupEnd'])assert.equal(calls.filter(c=>c.method===method).length,1);
assert.match(calls[0].args[1],/color:#[a-f\d]{6};background:#[a-f\d]{6}/i);
assert.equal(events()[0].args[4].unexpected,undefined);
for(const [level,method]of [['debug','debug'],['info','info'],['success','info'],['warn','warn'],['error','error']]){
  clear();assert.equal(run(`output.event('${level}','device.event','Useful activity',{source:'fixture'})`),true);
  assert.equal(calls[0].method,method);
  assert.match(calls[0].args[0],/%c/);assert.match(calls[0].args[3],/background:transparent/);
  assert.equal(calls[0].args[4].level,level);assert.equal(calls[0].args[4].event,'device.event');
}
// Actual allowlist + recursive redaction, including donor's camelCase blind spot.
clear();run(`output.event('error','transport.error','Relay failed',{
  source:'relay',error_code:401,action:'Bearer abc.def.ghi https://david:password@lamp.local/?token=private',
  csrf_token:'must-not-appear',unexpected:'must-not-appear',
  state:{mqttPassword:'nested-secret',wsToken:'nested-secret',authorization:'nested-secret',safe:'ok'},
  timestamp:'forged',level:'forged',event:'forged',detail:'forged'
})`);
const safe=events()[0].args[4];
assert.equal(safe.action,'Bearer [REDACTED] https://[REDACTED]@lamp.local/?token=[REDACTED]');
assert.equal(safe.state.mqttPassword,'[REDACTED]');assert.equal(safe.state.safe,'ok');
assert.doesNotMatch(JSON.stringify(calls),/must-not-appear|nested-secret|private|password@|forged/);
clear();run("output.event('warn','device.event','first\\u001b[31m\\nsecond\\u202Ehidden 100% %c',{source:'lamp\\u009b',action:'refresh\\r\\nnow'})");
assert.match(calls[0].args[0],/first\[31m secondhidden 100%% %%c/);
assert.doesNotMatch(JSON.stringify(calls),/[\u001b\u202e\u009b]/);
assert.equal(calls[0].args[4].action,'refresh now');
const error=run("browserErrorMetadata(Object.assign(new Error('Bearer secret-value at /events?token=swordfish'),{code:'unauthorized',status:401}))");
assert.equal(error.error_code,'unauthorized');assert.deepEqual(Object.keys(error),['error_code']);
assert.equal(run('browserErrorMetadata(Object.assign(new Error("secret"),{status:401})).error_code'),'http_401');
assert.equal(run('browserErrorMetadata("secret").error_code'),'unknown_error');
assert.equal(run("redactBrowserText('eyJabcdefghijk.abcdefghijk.abcdefghijk')"),'[REDACTED JWT]');
assert.equal(run("redactBrowserText('a'.repeat(501)).length"),501);
// Bounded output, recursive cycles and prototype-key protection.
assert.equal(run('sanitizeBrowserValue(Array.from({length:30},(_,i)=>i)).length'),21);
assert.equal(run('sanitizeBrowserValue(Object.fromEntries(Array.from({length:40},(_,i)=>[i,i]))).omitted'),10);
assert.equal(run('sanitizeBrowserValue({a:{a:{a:{a:{a:1}}}}}).a.a.a.a'),'[Maximum depth]');
assert.equal(run('(()=>{const x={};x.self=x;return sanitizeBrowserValue(x).self})()'),'[Circular]');
assert.equal(run('sanitizeBrowserValue(new Uint8Array([1,2,3]))'),'[Typed buffer 3 bytes]');
assert.equal(run('sanitizeBrowserValue(new Date(NaN))'),'[Invalid date]');
assert.equal(run('sanitizeBrowserValue(NaN)'),'[Nonfinite number]');
assert.equal(run('Object.hasOwn(sanitizeBrowserValue(JSON.parse(\'{"__proto__":{"polluted":true}}\')), "__proto__")'),false);
// Actual quiet-window contract, plus bounded lifetime and backward-clock recovery.
clear();clock=100;
const keyed="output.event('info','device.event','on',{state:'on'},{dedupeKey:'same-state',dedupeWindowMs:1000})";
assert.equal(run(keyed),true);clock=500;assert.equal(run(keyed),false);clock=1101;assert.equal(run(keyed),true);
clock=1;assert.equal(run(keyed),true,'Backward clock must not suppress indefinitely');
run("for(let i=0;i<300;i++)output.event('debug','device.event','state',{}, {dedupeKey:'key'+i})");
assert.equal(run('output.recent.size'),256);
assert.equal(run("output.event('unknown','device.event','no')"),false);
assert.equal(run("output.event('info','constructor','no')"),false);
assert.equal(run("new StyledBrowserConsole({info(){throw Error('sink failure')}}).event('info','device.event','safe')"),false);
assert.equal(run("new StyledBrowserConsole(console,()=>NaN).event('info','device.event','safe')"),false);
// Explicit palette CSS follows current shared app theme, not lamp constants.
const dark=run("browserConsoleStyle('info')");run("document.documentElement.dataset.theme='light'");
assert.notEqual(run("browserConsoleStyle('info')"),dark);
// Real HTTP adapter: quiet polls/telemetry, accepted != readback, no secrets/replay.
let response={status:200,ok:true,text:async()=>'{"accepted":true}'},requestCount=0,loginCount=0;
const fixtureIdentity={version:'0.2.8',board_id:'esp8266-aabbccddeeff',boot_id:'a'.repeat(32),http_peer_fencing:true};
Object.assign(context,{Headers,AbortController,URL,setTimeout,clearTimeout,crypto:webcrypto});
context.fetch=async(path,options)=>{requestCount++;if(path==='/api/status'&&response.ok)return {status:200,ok:true,text:async()=>JSON.stringify(fixtureIdentity)};return response};context.showLogin=()=>loginCount++;
vm.runInContext(read('assets/web/peer-http.js')+'\nconst peerHttp=createPeerHttp();',context);
vm.runInContext(web.slice(web.indexOf('async function api('),web.indexOf('function showLogin()')),context);
clear();await run("api('/api/status')");
await run("api('/api/clients/report',{method:'POST',body:'private-client-data'})");
assert.equal(events().length,0);
await run("api('/api/cell?token=path-secret',{method:'POST',body:'body-secret',headers:{Authorization:'Bearer header-secret'}})");
assert.equal(events().length,1);assert.equal(events()[0].args[4].endpoint,'/api/cell');
assert.match(events()[0].args[4].detail,/readback may still be required/);
assert.doesNotMatch(JSON.stringify(calls),/poll-secret|private-client-data|path-secret|body-secret|header-secret/);
clear();response={status:500,ok:false,text:async()=>'Bearer server-secret ?token=raw-secret'};
const before=requestCount;
await assert.rejects(run("api('/api/cell',{method:'POST',body:'lost-secret'})"));
assert.equal(requestCount,before+1,'Uncertain write is never retried');
assert.equal(events()[0].args[4].error_code,'http_500');assert.doesNotMatch(JSON.stringify(calls),/server-secret|raw-secret|lost-secret/);
await assert.rejects(run("api('/api/cell',{method:'POST'})"));assert.equal(events().length,2,'Each explicit mutation outcome remains visible');
response={status:401,ok:false,text:async()=>'login-secret'};
await assert.rejects(run("api('/api/status')"));assert.equal(loginCount,1);
// Distinct failures survive interleaving; no timer reopens a continuing outage.
clear();
for(let i=0;i<20;i++){
  response={status:i%2?500:503,ok:false,text:async()=>'private'};
  await assert.rejects(run("api('/api/status')"));
  clock+=60000;
}
assert.equal(events().length,2);
assert.deepEqual(events().map(e=>e.args[4].error_code),['http_503','http_500']);
await assert.rejects(run("api('/api/cloud')"));assert.equal(events().length,3,'Another source stays independent');
response={status:200,ok:true,text:async()=>'{}'};
await run("api('/api/status')");
assert.equal(events().at(-1).args[4].count,21,'Recovery includes the preceding authentication failure too');
assert.equal(events().at(-1).args[4].event,'transport.ready');
await run("api('/api/status')");assert.equal(events().length,4,'Successful polls stay quiet');
response={status:503,ok:false,text:async()=>'private'};
await assert.rejects(run("api('/api/status')"));assert.equal(events().length,5,'A later outage is new');
assert.equal(run("browserConsole.incidents.size"),2,'Cloud was not falsely recovered');
run("for(let i=0;i<300;i++)output.event('error','transport.error','failure',{error_code:i},{incidentKey:'bounded'})");
assert.equal(run('output.incidents.size'),128);
// Actual WebSocket ownership: transitions only; frames and stale callbacks quiet.
const sockets=[],timers=[];let renderCount=0,stops=0;
Object.assign(context,{location:{protocol:'http:',host:'ledboard.local'},wsState:{},stopStream:()=>stops++,render:()=>renderCount++,
  clearTimeout(){},setTimeout:callback=>{timers.push(callback);return timers.length},
  WebSocket:class {constructor(url){this.url=url;this.sent=[];sockets.push(this)}send(x){this.sent.push(x)}close(){this.closed=true}}});
context.refreshAuthenticated=async()=>{};
vm.runInContext('let authenticated=true,webSocketPaused=false,socket=null;\n'+read('assets/web/realtime-retry.js')+'\n'+web.slice(web.indexOf('function connectWs()'),web.indexOf('async function post(')),context);
clear();run('connectWs()');const first=sockets[0];first.onopen();
assert.equal(new URL(first.url).origin,'ws://ledboard.local');assert.equal(new URL(first.url).pathname,'/ws');
assert.equal(new URL(first.url).searchParams.get('version'),'0.2.8');assert.equal(events()[0].args[4].event,'websocket.connected');
assert.deepEqual(first.sent,['{"type":"status"}']);
first.onmessage({data:new Uint8Array(480)});first.onmessage({data:'{"hostname":"fixture"}'});
assert.equal(renderCount,1);assert.equal(events().length,1,'No frame or unchanged state trace');
const staleOpen=first.onopen,staleClose=first.onclose;
run('connectWs()');staleOpen();staleClose({code:1006});first.onerror();
assert.equal(events().length,1);assert.equal(timers.length,0);
sockets[1].onclose({code:1006,reason:'raw-secret'});
assert.equal(events().at(-1).args[4].error_code,1006);assert.equal(timers.length,1);assert.equal(stops,1);
assert.doesNotMatch(JSON.stringify(calls),/raw-secret/);
run('webSocketPaused=true');sockets[1].onclose({code:1006});assert.equal(timers.length,1);
assert.equal(web.split('// BEGIN BROWSER DIAGNOSTICS').length,2);
assert.equal(web.split('// BEGIN BROWSER DIAGNOSTICS\n')[1].split('\n// END BROWSER DIAGNOSTICS')[0],module);
// Check is read-only. It must pass after ONE generation, including the dictionary.
const check=spawnSync(process.execPath,['tools/generate-font-assets.mjs','--check'],{cwd:root,encoding:'utf8'});
assert.equal(check.status,0,check.stderr||check.stdout);
console.log('Rayan browser trace: all donor scenarios, recursive secret protection, bounded dedupe, current palette, real HTTP/WS ownership and one-pass canonical embedding passed. VM proof, not live firmware.');
