import test from 'node:test';
import assert from 'node:assert/strict';
import {EventEmitter} from 'node:events';
import {createHash} from 'node:crypto';
import {Gateway,commandFor} from './gateway.mjs';
import {createPanel} from './service.mjs';
import {WebSocket} from 'ws';
// Keep the established CI/npm entrypoint exhaustive without a second job/cache.
import './duplex.test.mjs';
const board='esp32s3-fixture',boot='123456789abcdef0123456789abcdef0';
const release={version:'0.2.8',minimum_board:'0.2.8',maximum_board:null};
const snapshot=()=>({version:'0.2.8',board_id:board,boot_id:boot,hostname:'fixture',http_peer_fencing:true,command_clock_ms:1000,
  cells:Array(80).fill('white'),capabilities:{board_kind:'table',acknowledged_features:['table','leds','cell-batch','network','streaming'],table:{columns:10,rows:8,pixels_per_cell:6}}});
class Client extends EventEmitter{
  connected=true;sent=[];
  subscribe(_topics,_options,cb){cb()}
  publish(topic,bytes,options,cb){this.sent.push({topic,value:JSON.parse(bytes),options});cb?.();this.onPublish?.(this.sent.at(-1))}
  end(){}
}
function fixture(options={}){const client=new Client(),g=new Gateway({client,topic:'test/board',board,release,timeout:60,...options});client.emit('connect');client.emit('message','test/board/status',Buffer.from(JSON.stringify(snapshot())),{retain:false});return {client,g}}
const packet=(client,leaf,value,retain=false)=>client.emit('message','test/board/'+leaf,Buffer.from(JSON.stringify(leaf==='event'?{type:'command-result',board_id:board,boot_id:boot,...value}:value)),{retain});
test('Freshness comes from live same-board status, not retained packets/online claims',()=>{
  let now=100;const {client,g}=fixture({now:()=>now});assert(g.fresh());now+=9000;
  packet(client,'status',snapshot(),true);assert.equal(g.fresh(),false);
  packet(client,'status',{...snapshot(),board_id:'other'});assert.equal(g.fresh(),false);
  packet(client,'status',snapshot());assert(g.fresh());
  assert(!g.project().capabilities.acknowledged_features.includes('streaming'));
  assert(!g.project().capabilities.acknowledged_features.includes('network'));
  assert.equal(g.project().relay.direct,false);
});
test('Wrong boot and unsupported releases are rejected before publish',async()=>{
  const {client,g}=fixture();const before=client.sent.length;
  await assert.rejects(g.command('cell',{}, {board,boot:'f'.repeat(32)}),{code:'peer_changed'});
  packet(client,'status',{...snapshot(),version:'0.2.7'});
  await assert.rejects(g.command('cell',{}, {board,boot}),{code:'unsupported_peer_version'});
  assert.equal(client.sent.length,before);
});
test('One non-retained command, matching ACK, then fresh same-boot readback',async()=>{
  const {client,g}=fixture();let mutation;
  client.onPublish=p=>{
    if(p.value.command==='cells'){mutation=p;queueMicrotask(()=>packet(client,'event',{request_id:p.value.request_id,accepted:true,code:'accepted',message:'cell set accepted atomically'}))}
    else if(p.value.command==='telemetry')queueMicrotask(()=>packet(client,'status',{...snapshot(),cells:['red',...Array(79).fill('white')]}));
  };
  const reply=await g.command('cells',{ids:'1',state:'red',effect:'immediate'},{board,boot});
  assert.equal(reply.accepted,true);assert.equal(g.project().cells[0],'red');
  assert.equal(client.sent.filter(p=>p.value.command==='cells').length,1);
  assert.deepEqual(mutation.options,{qos:0,retain:false});
  assert.equal(mutation.value.board_id,board);assert.equal(mutation.value.boot_id,boot);
  assert.equal(mutation.value.client_version,'0.2.8');assert(mutation.value.expires_at>Date.now()/1000);
});
test('Missing/retained/foreign ACKs never cause a mutation replay',async()=>{
  const {client,g}=fixture();const keep=setTimeout(()=>{},200);
  client.onPublish=p=>{if(p.value.command==='effect'){packet(client,'event',{request_id:p.value.request_id,accepted:true,code:'accepted'},true);packet(client,'event',{request_id:'foreign',accepted:true,code:'accepted'})}};
  try{await assert.rejects(g.command('effect',{value:'rainbow'},{board,boot}),{code:'command_outcome_unknown'});assert.equal(client.sent.filter(p=>p.value.command==='effect').length,1)}finally{clearTimeout(keep)}
});
test('Broker loss retires pending commands; no new offline publication',async()=>{
  const {client,g}=fixture();const pending=g.command('effect',{value:'rainbow'},{board,boot});client.connected=false;client.emit('close');
  await assert.rejects(pending,{code:'connection_lost_outcome_unknown'});assert.throws(()=>g.send({command:'effect'}),{code:'broker_unavailable'});
});
test('UI forms map to bounded board-side commands; unrelated operations fail closed',()=>{
  assert.deepEqual(commandFor('/api/cells',new URLSearchParams({ids:'1,80',state:'red',effect:'immediate'})),['cells',{ids:'1,80',state:'red',effect:'immediate'},2]);
  assert.throws(()=>commandFor('/api/cells',new URLSearchParams({ids:'81',state:'red'})));
  assert.deepEqual(commandFor('/api/led/output',new URLSearchParams({enabled:'false'})),['led-output',{enabled:false}]);
  assert.throws(()=>commandFor('/api/led/output',new URLSearchParams({enabled:'yes'})));
  assert.throws(()=>commandFor('/api/brightness',new URLSearchParams({value:'256'})));
  assert.throws(()=>commandFor('/api/avr/program',new URLSearchParams()),{code:'route_unavailable_over_mqtt'});
});
test('HTTP panel is private, never returns credentials, and rejects cross-origin writes',async()=>{
  const client=new Client(),password='fixture-not-a-real-secret';
  const panel=createPanel({client,html:'<h1>Actual UI fixture</h1>',basic:'basic fixture',font:Buffer.from('font'),release,
    config:{origin:'http://127.0.0.1',board,topic:'test/board',username:'fixture',password_sha256:createHash('sha256').update(password).digest('hex')}});
  await new Promise(resolve=>panel.server.listen(0,'127.0.0.1',resolve));
  const base='http://127.0.0.1:'+panel.server.address().port;
  const authorization='Basic '+Buffer.from('fixture:'+password).toString('base64');
  try{
    const denied=await fetch(base+'/');assert.equal(denied.status,401);assert(denied.headers.get('www-authenticate'));
    const root=await fetch(base+'/',{headers:{authorization}});assert.equal(root.status,200);assert((await root.text()).includes('Actual UI fixture'));assert(root.headers.get('set-cookie').includes('HttpOnly; Secure; SameSite=Strict'));
    client.emit('connect');packet(client,'status',snapshot());
    const status=await fetch(base+'/api/status',{headers:{authorization}});assert.equal(status.status,200);assert.equal((await status.json()).board_id,board);
    const before=client.sent.length;
    const bad=await fetch(base+'/api/cells',{method:'POST',headers:{authorization,Origin:'https://foreign.invalid'},body:'ids=1&state=red'});assert.equal(bad.status,403);assert.equal(client.sent.length,before);
    const missingPin=await fetch(base+'/api/cells',{method:'POST',headers:{authorization,'X-Pakar-Version':release.version},body:'ids=1&state=red'});assert.equal(missingPin.status,409);
    const map=await (await fetch(base+'/api/cells/mapping',{headers:{authorization}})).json();assert.deepEqual(map.physical,[]);assert.equal(map.available,false);
  }finally{await panel.close()}
});

test('Real HTTP mutation receives one board ACK/readback; WebSocket remains same-origin and private',async()=>{
  const client=new Client(),password='fixture-secret',origin='http://127.0.0.1';
  const panel=createPanel({client,html:'panel',basic:'basic',font:Buffer.alloc(0),release,
    config:{origin,board,topic:'test/board',username:'fixture',password_sha256:createHash('sha256').update(password).digest('hex')}});
  await new Promise(resolve=>panel.server.listen(0,'127.0.0.1',resolve));
  const base='http://127.0.0.1:'+panel.server.address().port,authorization='Basic '+Buffer.from('fixture:'+password).toString('base64');
  const sockets=[];
  try{
    client.emit('connect');packet(client,'status',snapshot());
    for(const headers of [{Origin:origin},{Origin:'https://foreign.invalid',Authorization:authorization}]){
      const ws=new WebSocket(base.replace('http:','ws:')+'/ws',{headers});sockets.push(ws);
      await new Promise((resolve,reject)=>{ws.on('unexpected-response',(_req,res)=>{assert.equal(res.statusCode,403);res.resume();ws.terminate();resolve()});ws.on('open',()=>reject(Error('Unexpected admitted socket')));ws.on('error',()=>{})});
    }
    const ws=new WebSocket(base.replace('http:','ws:')+'/ws',{headers:{Origin:origin,Authorization:authorization}});sockets.push(ws);
    await new Promise((resolve,reject)=>{ws.once('message',bytes=>{assert.equal(JSON.parse(bytes).board_id,board);resolve()});ws.once('error',reject)});
    const pushed=new Promise((resolve,reject)=>{ws.once('message',bytes=>{try{assert.equal(JSON.parse(bytes).cells[0],'red');resolve()}catch(e){reject(e)}})});
    client.onPublish=p=>{
      if(p.value.command==='cells')queueMicrotask(()=>packet(client,'event',{request_id:p.value.request_id,accepted:true,code:'accepted',message:'cell set accepted atomically'}));
      if(p.value.command==='telemetry')queueMicrotask(()=>packet(client,'status',{...snapshot(),cells:['red',...Array(79).fill('white')]}));
    };
    const response=await fetch(base+'/api/cells',{method:'POST',headers:{authorization,Origin:origin,'X-Pakar-Board':board,'X-Pakar-Boot':boot},body:'ids=1&state=red&effect=immediate'});
    assert.equal(response.status,200);assert.equal((await response.json()).count,1);await pushed;
    assert.equal(client.sent.filter(p=>p.value.command==='cells').length,1);
    assert.equal((await (await fetch(base+'/api/status',{headers:{authorization}})).json()).cells[0],'red');
  }finally{for(const ws of sockets)ws.terminate();await panel.close()}
});
