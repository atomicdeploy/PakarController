import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

const source=readFileSync(new URL('../web/index.html',import.meta.url),'utf8');
// Parse trusted repository script bodies for syntax, never sanitize user HTML.
// Same forgiving tag extraction as web_app_contract_test.mjs.
const scriptBodies = html => [...html.matchAll(/<script\b[^>]*>([\s\S]*?)<\/script[^>]*>/gi)].map(match=>match[1]);
for(const body of scriptBodies(source))new vm.Script(body);
assert.deepEqual(scriptBodies('<SCRIPT>const upper=1;</SCRIPT >'), ['const upper=1;']);
assert.deepEqual(scriptBodies('<script>const checked=1;</script\t\n ignored>'), ['const checked=1;']);
const controls=source.split('// BEGIN KEY GAME PRESS CONTROLS')[1].split('// END KEY GAME PRESS CONTROLS')[0];
function fixture(){
  const calls=[],peer={known:true,role:'first-wins',has:()=>true};
  const game={available_modes:[{id:'press-to-win',label:'First press wins'},{id:'toggle',label:'Toggle'}],mode:'press-to-win',phase:'ready',revision:4,ready:true,participating_mask:3,settings:{mode:'press-to-win',toggle_kind:'toggle'}};
  const context=vm.createContext({state:{board_id:'board',boot_id:'boot',minigame:game,key_game:{enabled_mask:3}},currentPeer:()=>peer,peerHttp:{writable:()=>true},renderKeyGame(){},post:async(path,data)=>{calls.push({path,...data});return true}});
  vm.runInContext(controls,context);const run=code=>vm.runInContext(code,context);
  run('syncKeyGameOwner()');return {context,game,peer,calls,run,arm:()=>run('keyGameControl.readOnly=false')};
}
{
  const f=fixture();assert.equal(f.calls.length,0,'Observation does not start a round');
  await f.run("changeMiniGame('control',{action:'start'})");assert.equal(f.calls.length,0);
  f.arm();await f.run("changeMiniGame('configure',{settings:{mode:'toggle'}})");
  assert.equal(f.calls[0].path,'/api/minigame/configure');assert.equal(f.game.mode,'press-to-win','No optimistic mode change');
  await f.run('pressGameKey(1)');assert.equal(f.calls[1].path,'/api/minigame/input');assert.equal(f.calls[1].action,'tap');
  await f.run('pressGameKey(3)');assert.equal(f.calls.length,2,'Only participating boxes accept input');
}
for(const mutate of [f=>f.game.ready=false,f=>f.peer.known=false,f=>f.peer.role='table',f=>f.peer.has=()=>false,f=>f.context.state.programming_latch=true,f=>f.context.peerHttp.writable=()=>false,f=>f.game.available_modes=[]]){
  const f=fixture();f.arm();mutate(f);await f.run("changeMiniGame('control',{action:'start'})");await f.run('pressGameKey(1)');assert.equal(f.calls.length,0,'Unavailable minigames cannot fall through to legacy presses');
}
{
  const f=fixture();f.arm();let finish;f.context.post=async(path,data)=>{f.calls.push({path,...data});return new Promise(resolve=>finish=resolve)};
  const first=f.run("changeMiniGame('control',{action:'start'})"),finishFirst=finish;
  await f.run("changeMiniGame('control',{action:'start'})");assert.equal(f.calls.length,1,'Pending control serializes repeated presses');
  f.context.state.boot_id='next';f.run('syncKeyGameOwner()');assert.equal(f.run('keyGameControl.readOnly'),true);
  f.arm();const second=f.run("changeMiniGame('control',{action:'reset'})"),finishSecond=finish;
  finishFirst(true);await first;assert.notEqual(f.run('keyGameControl.pending'),null,'Old completion cannot release new owner');
  finishSecond(true);await second;
}
{
  const f=fixture();f.arm();f.game.mode='toggle';f.game.settings.toggle_kind='press';let finish;
  f.context.post=async(path,data)=>{f.calls.push({path,...data});if(data.action==='down')return new Promise(resolve=>finish=resolve);return true};
  f.run('holdMiniGameKey(1)');const release=f.run('releaseMiniGameKey(1)');assert.equal(f.calls.length,1);
  f.run('keyGameControl.readOnly=true');finish(true);await release;
  assert.deepEqual(f.calls.map(call=>call.action),['down','up'],'Release waits for down and remains allowed after read-only selection');
}
{
  const f=fixture();f.arm();f.game.mode='toggle';f.game.settings.toggle_kind='press';let finish;
  f.context.post=async(path,data)=>{f.calls.push({path,...data});return new Promise(resolve=>finish=resolve)};
  f.run('holdMiniGameKey(1)');const release=f.run('releaseMiniGameKey(1)');f.context.state.boot_id='other';finish(true);await release;
  assert.equal(f.calls.length,1,'Release never targets a replacement peer');
}

const context=vm.createContext({setTimeout,clearTimeout,URLSearchParams,Uint8Array,AbortController});
vm.runInContext(readFileSync(new URL('../assets/web/command-transport.js',import.meta.url),'utf8'),context);
const f=fixture(),state=f.context.state;state.command_clock_ms=100;
let transport,packet,requests=0,transform=status=>status;
transport=context.createCommandTransport({snapshot:()=>state,valid:()=>true,random:()=>new Uint8Array(16).fill(4),
  socket:()=>({readyState:1,bufferedAmount:0,send:bytes=>{
    requests++;packet=JSON.parse(bytes);const status=JSON.parse(JSON.stringify(state));status.minigame.revision++;
    if(packet.settings)Object.assign(status.minigame.settings,packet.settings);
    queueMicrotask(()=>transport.receive({type:'command-result',request_id:packet.request_id,board_id:packet.board_id,boot_id:packet.boot_id,accepted:true,status:transform(status)}));
  }}),http:()=>{throw Error('Immediate WS receipt must not use HTTP')}});
await transport.send('/api/minigame/configure',{settings:{mode:'toggle',limit_ms:'60000',hard:'true'}});
assert.equal(packet.command,'minigame-configure');assert.equal(packet.settings.limit_ms,60000);assert.equal(packet.settings.hard,true);
await transport.send('/api/minigame/control',{action:'pause'});assert.equal(packet.command,'minigame-control');
await transport.send('/api/minigame/configure',{settings:{intro_enabled:true,color_arpeggio:true,intro_ms:1000,preview_ms:1500,mismatch_ms:1200}});
assert.equal(packet.settings.intro_enabled,true);assert.equal(packet.settings.mismatch_ms,1200);
await transport.send('/api/minigame/input',{key:'2',action:'down'});assert.equal(packet.key,2);
const before=requests;
for(const [path,data] of [
 ['/api/minigame/configure',{settings:{mode:'not-advertised'}}],['/api/minigame/configure',{settings:{limit_ms:-1}}],
 ['/api/minigame/configure',{settings:{toggle_kind:'unknown'}}],['/api/minigame/configure',{settings:{outputs:[{source:'held',key:0}]}}],
 ['/api/minigame/control',{action:'autoplay'}],['/api/minigame/input',{key:10,action:'tap'}],['/api/minigame/input',{key:1,action:'unknown'}],
])await assert.rejects(transport.send(path,data));
assert.equal(requests,before,'Malformed settings cannot dispatch');
state.minigame.ready=false;assert.equal(await transport.send('/api/minigame/control',{action:'start'}),null);assert.equal(requests,before);state.minigame.ready=true;
transform=status=>{status.minigame.settings.limit_ms=1;return status};await assert.rejects(transport.send('/api/minigame/configure',{settings:{limit_ms:60000}}),/readback differs/);
transform=status=>{status.minigame.revision=0;return status};await assert.rejects(transport.send('/api/minigame/control',{action:'start'}),/stale/);
console.log('Web minigame admission, peer ownership, momentary release, typed WS commands and readback guards passed');
