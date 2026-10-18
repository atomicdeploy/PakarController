// Actual adapted Rayan API request lifecycle + PCController admission semantics.
// HTTP doubles only: no LAN, firmware deployment or physical acceptance implied.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const source=readFileSync(new URL('../assets/web/peer-http.js',import.meta.url),'utf8').trim();
const web=readFileSync(new URL('../firmware/esp8266/src/WebUi.h',import.meta.url),'utf8').replaceAll('\r\n','\n');
assert.equal(web.split('// BEGIN PEER HTTP\n')[1].split('\n// END PEER HTTP')[0],source);
const c=vm.createContext({Headers,AbortController,setTimeout,clearTimeout,fetch});
vm.runInContext(source+'\nglobalThis.factory=createPeerHttp;',c);
const original={board_id:'esp8266-aabbccddeeff',boot_id:'a'.repeat(32),http_peer_fencing:true};
let current={...original},calls=[],time=100,changes=0,reply=()=>Response.json({accepted:true});
let client=c.factory({now:()=>time,changed:()=>changes++,fetcher:async(path,init)=>{calls.push({path,init});return path==='/api/status'?Response.json(current):reply(path,init)}});
client.observe(current);const peer=client.pin();
await client.request('/api/cell',{method:'POST',peer,body:'id=1'});
assert.deepEqual(calls.map(c=>c.path),['/api/status','/api/cell']);
assert.equal(calls[1].init.headers.get('X-Pakar-Board'),original.board_id);
assert.equal(calls[1].init.headers.get('X-Pakar-Boot'),original.boot_id);
assert.equal(calls[1].init.credentials,'same-origin');assert.equal(calls[1].init.cache,'no-store');
assert.equal(calls[1].init.redirect,'error','307/308 must not automatically replay a write or transfer its body/headers');
assert.equal(calls[1].init.headers.get('Content-Type'),null,'FormData boundary is left to fetch');
// Same-host reboot between draft creation and preflight: zero mutation calls.
calls=[];current={...original,boot_id:'b'.repeat(32)};
await assert.rejects(client.request('/api/cell',{method:'POST',peer}),/changed/);
assert.equal(calls.length,1);assert.equal(changes,2);
await assert.rejects(client.request('/api/cell',{method:'POST',peer}),/changed/);assert.equal(calls.length,1);
const second=client.pin();reply=()=>Response.json({error:'peer_changed',accepted:false},{status:409});
await assert.rejects(client.request('/api/cell',{method:'POST',peer:second}),/409/);assert(!client.writable());
// Legacy/malformed identities stay readable, never writable.
for(const bad of [{}, {...original,http_peer_fencing:false}, {...original,boot_id:'0'.repeat(32)}, {...original,board_id:123}, {...original,board_id:'Upper'}, {...original,boot_id:'A'.repeat(32)}]){client.observe(bad);assert(!client.writable());}
current={...original};client.observe(current);time+=8000;assert(!client.writable());
time=100;client.observe(current);
reply=()=>Response.json({error:'login_required'},{status:401});calls=[];
await assert.rejects(client.request('/api/cell',{method:'POST'}),/sign in/);assert.equal(calls.length,2,'One preflight and one POST, no 401 replay');
reply=()=>new Response('{broken',{headers:{'Content-Type':'application/json'}});
await assert.rejects(client.request('/api/cloud'),/malformed JSON/);
// Typed donor error status/code retains distinct causes sharing one HTTP status.
for(const code of ['storage_full','prog_locked']){
  reply=()=>Response.json({error:{code,message:'private detail'}},{status:409});
  await assert.rejects(client.request('/api/cloud'),error=>error.name==='PeerAPIError'&&error.status===409&&error.code===code&&!error.message.includes('private'));
}
await assert.rejects(client.request('https://other.local/api/cell',{method:'POST'}),/same-origin/);
await assert.rejects(client.request('//other.local/api/cell',{method:'POST'}),/same-origin/);
// Late old GET cannot populate new peer drafts even if fetch ignores abort.
let finish;
client=c.factory({fetcher:()=>new Promise(resolve=>finish=resolve)});client.observe(original);
const late=client.request('/api/cloud');client.observe({...original,boot_id:'c'.repeat(32)});finish(Response.json({mqtt_host:'old'}));
await assert.rejects(late,/Late reply/);
// Deadline, timer release and external cancellation port api.ts's lifecycle.
const timers=new Map();let id=0;
client=c.factory({setTimer:fn=>{timers.set(++id,fn);return id},clearTimer:id=>timers.delete(id),fetcher:(_p,{signal})=>new Promise((_r,reject)=>signal.addEventListener('abort',()=>reject(Error('aborted'))))});
const timeout=client.request('/api/cloud');assert.equal(timers.size,1);[...timers.values()][0]();
await assert.rejects(timeout,/timed out/);assert.equal(timers.size,0);
const abort=new AbortController(),cancel=client.request('/api/cloud',{signal:abort.signal});abort.abort();await assert.rejects(cancel,/cancelled/);assert.equal(timers.size,0);
// Transaction pins are wired through actual batch and settings paths.
const batchStart=web.indexOf('async function applyCellBatch(');
const batch=web.slice(batchStart,web.indexOf('\n}',batchStart)+2);
assert(batch.includes('const peer=peerHttp.pin();'));
assert(batch.includes("api('/api/cells',{method:'POST',peer,"));
assert(!batch.includes("api('/api/cell',"),'A bulk action must never become per-cell requests');
assert(batch.includes("ack?.accepted!==true"));
assert(batch.includes("actual.cells"),'Device readback is mandatory, not app-side color inference');
assert(web.includes("boardSoundTransaction((path,options={})=>api(path,{...options,peer}),guard,desired,peer.boot)"));
assert(web.includes('peer=peerHttp.pin();write=settingsWrite(group)'));
assert(web.includes("await api('/api/status',{peer});if(epoch!==webSettings.epoch)return"));
assert(web.includes('selectedCells.clear();cellSearch.value='));
assert(web.includes('pixelSample=null;pixelSampleAt=0;lastPixelStatus=null;remoteJob=null'));
console.log('Web peer HTTP: exact pin headers, reboot rejection, no replay, read-only legacy, late replies, cancellation/deadline cleanup and production transaction wiring passed.');
