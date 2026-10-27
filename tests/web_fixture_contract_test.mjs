// HTTP-level fixture safety and real contract-shape tests. These deliberately
// prove no hardware behavior: every child binds loopback and owns memory only.
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {once} from 'node:events';
import {test} from 'node:test';
import {fileURLToPath} from 'node:url';

async function fixture(flags,run){
  const child=spawn(process.execPath,[fileURLToPath(new URL('./web_appearance_fixture.mjs',import.meta.url)), '--app',...flags],{env:{...process.env,PAKAR_FIXTURE_PORT:'0'},stdio:['ignore','pipe','pipe'],windowsHide:true});
  let errorOutput='';child.stderr.on('data',chunk=>errorOutput+=chunk);
  const origin=await new Promise((resolve,reject)=>{
    let output='';const timer=setTimeout(()=>{child.kill();reject(Error('Fixture startup deadline: '+errorOutput));},8000);
    child.stdout.on('data',chunk=>{output+=chunk;for(const line of output.split('\n')){try{const ready=JSON.parse(line);if(ready.url){clearTimeout(timer);resolve(ready.url);return;}}catch{}}});
    child.once('exit',code=>{clearTimeout(timer);reject(Error('Fixture exited '+code+': '+errorOutput));});
  });
  try{await run(origin);}finally{if(child.exitCode===null){const ended=once(child,'exit');child.kill();await ended;}}
}
async function read(origin){const response=await fetch(new URL('api/status',origin));assert.equal(response.headers.get('x-pakar-simulated'),'true');assert.equal(response.status,200);return response.json();}
function packet(status,command,extra={}){return {type:'command',request_id:'1'.repeat(32),board_id:status.board_id,boot_id:status.boot_id,expires_at_ms:status.command_clock_ms+8000,command,...extra};}
async function send(origin,status,value,headers={}){return fetch(new URL('api/command',origin),{method:'POST',headers:{'X-Pakar-Board':status.board_id,'X-Pakar-Boot':status.boot_id,...headers},body:new URLSearchParams({packet:JSON.stringify(value)})});}

test('default application fixture is explicitly simulated and rejects writes',()=>fixture([],async origin=>{
  const status=await read(origin);assert.equal(status.simulated,true);assert.equal(status.fixture_read_only,true);
  assert.equal(status.capabilities.role,'table-selection');assert(status.capabilities.acknowledged_features.includes('table'));
  assert(!status.capabilities.acknowledged_features.includes('game-press'));
  const response=await send(origin,status,packet(status,'cells',{ids:'1',state:'lost'}));assert.equal(response.status,405);
  assert.deepEqual((await read(origin)).cells,status.cells);
}));

test('unacknowledged preview does not invent table/game capability or crash',()=>fixture(['--unacknowledged'],async origin=>{
  const status=await read(origin);assert.equal(status.capabilities,undefined);assert.equal(status.simulated,true);
}));

test('unknown capabilities cannot actuate even in explicit interactive fixture',()=>fixture(['--unacknowledged','--interactive'],async origin=>{
  const status=await read(origin);
  const response=await send(origin,status,packet(status,'effect',{value:'rainbow'}));assert.equal(response.status,400);
  assert.equal((await read(origin)).effect,'none');
}));

test('interactive preview uses semantic states, count receipt, peer pin and idempotency',()=>fixture(['--interactive'],async origin=>{
  const status=await read(origin);assert.equal(status.fixture_read_only,false);
  const value=packet(status,'cells',{ids:'1-3, 3',state:'lost'});
  assert.equal((await send(origin,status,value,{'X-Pakar-Board':'different-board'})).status,409);
  const first=await send(origin,status,value);assert.equal(first.status,200);const receipt=await first.json();
  assert.equal(receipt.type,'command-result');assert.equal(receipt.request_id,value.request_id);assert.equal(receipt.count,3);assert.equal(receipt.simulated,true);
  assert.deepEqual(receipt.status.cell_states.slice(0,4),['lost','lost','lost','idle']);
  assert.deepEqual(await (await send(origin,status,value)).json(),receipt,'Same request returns original receipt');
  assert.equal((await send(origin,status,{...value,state:'won'})).status,400,'Same identity cannot mean a new action');
  assert.equal((await send(origin,status,{...value,request_id:'2'.repeat(32),boot_id:'b'.repeat(32)})).status,400);
  assert.equal((await send(origin,status,{...value,request_id:'3'.repeat(32),command:'firmware-upload'})).status,400,'No fixture firmware/remote forwarding');
  assert.equal((await fetch(new URL('api/peers/request',origin),{method:'POST',body:'host=ledcontroller.local'})).status,405);
}));

test('key preview uses runtime mask and real game-press contract without table leakage',()=>fixture(['--controller','--interactive'],async origin=>{
  const status=await read(origin);assert.equal(status.capabilities.role,'first-wins');assert.equal(status.pixels,216);assert.equal(status.key_game.enabled_mask,15);
  assert(!status.capabilities.acknowledged_features.includes('table'));assert(status.capabilities.acknowledged_features.includes('game-press'));
  assert(status.capabilities.acknowledged_features.includes('reset-ack'));assert(status.capabilities.acknowledged_features.includes('key-settings'));
  let response=await send(origin,status,packet(status,'game-press',{key:2}));assert.equal(response.status,200);assert.equal((await response.json()).status.winner,2);
  response=await send(origin,status,packet(status,'game-press',{key:5,request_id:'2'.repeat(32)}));assert.equal(response.status,400,'Disabled key cannot win');
  response=await send(origin,status,packet(status,'reset',{request_id:'3'.repeat(32)}));assert.equal(response.status,200);const receipt=await response.json();assert.equal(receipt.status.winner,0);assert.equal(receipt.avr_acknowledged,true);assert.equal(receipt.simulated,true);
}));

test('pixel diagnostic probe has bounded physical indices and revisioned state',()=>fixture(['--interactive'],async origin=>{
  const status=await read(origin),value=packet(status,'pixel-diagnostic',{index:474,count:6,rgb:0x00ff00,off:false});
  const response=await send(origin,status,value);assert.equal(response.status,200);const receipt=await response.json();
  assert.equal(receipt.status.led_output.diagnostic.index,474);assert.equal(receipt.status.led_output.diagnostic.count,6);assert.equal(receipt.status.led_output.diagnostic.revision,2);
  const invalid={...value,request_id:'2'.repeat(32),index:479};assert.equal((await send(origin,status,invalid)).status,400);
  assert.equal((await read(origin)).led_output.diagnostic.revision,2,'Rejected probe does not change state');
}));

test('one application and its packaged assets are served without an alternate app route',()=>fixture([],async origin=>{
  const html=await(await fetch(origin)).text();assert(html.includes('<!doctype html>'));assert(html.includes('PakarApplicationUtilities'));
  for(const name of ['manifest.webmanifest','sw.js','app-icon.png','assets/fonts/Vazirmatn-Regular.ttf']){
    const response=await fetch(new URL(name,origin));assert.equal(response.status,200,name+' is a required packaged asset');
  }
  for(const name of ['ng','app.js','feature-pages.js','workspace.js','package.json'])assert.equal((await fetch(new URL(name,origin))).status,404,name+' is not an alternate application route');
  assert.equal((await fetch(new URL('package.json',origin))).status,404);
  assert.equal((await fetch(new URL('api/proxy?url=http://ledcontroller.local',origin))).status,404);
  assert.equal((await read(origin)).simulated,true);
}));
