import test from 'node:test';
import assert from 'node:assert/strict';
import {EventEmitter,once} from 'node:events';
import {createHash} from 'node:crypto';
import {WebSocket} from 'ws';
import mqtt from 'mqtt';
import {Gateway} from './gateway.mjs';
import {createPanel} from './service.mjs';

const board='esp32s3-duplex-fixture',boot='123456789abcdef0123456789abcdef0';
const release={version:'0.2.8',minimum_board:'0.2.8'};
const snapshot=()=>({version:'0.2.8',board_id:board,boot_id:boot,command_clock_ms:1000,cells:Array(80).fill('white'),capabilities:{acknowledged_features:['table','cell-batch','command-ack']}});
class Client extends EventEmitter {
  connected=true;sent=[];
  subscribe(_topics,_options,cb){cb()}
  publish(topic,bytes,options,cb){const p={topic,value:JSON.parse(bytes),options};this.sent.push(p);cb?.();this.onPublish?.(p)}
  end(){}
}
const incoming=(client,leaf,value,retain=false)=>client.emit('message','fixture/'+leaf,Buffer.from(JSON.stringify(value)),{retain});
const ack=(request,extra={})=>({type:'command-result',board_id:board,boot_id:boot,request_id:request.request_id,accepted:true,code:'accepted',message:'applied',...extra});
const command=(extra={})=>({type:'command',command:'cells',request_id:'same-intent',board_id:board,boot_id:boot,expires_at_ms:9000,ids:'1',state:'red',effect:'immediate',...extra});
function fixture(){const client=new Client(),gateway=new Gateway({client,topic:'fixture',board,release,timeout:100});client.emit('connect');incoming(client,'status',snapshot());return {client,gateway}}
function responder(client){client.onPublish=p=>queueMicrotask(()=>{if(p.value.command==='telemetry')incoming(client,'status',{...snapshot(),cells:['red',...Array(79).fill('white')]});else incoming(client,'event',ack(p.value))})}

test('Same request across concurrent channels applies once and receives authoritative readback',async()=>{
  const {client,gateway}=fixture();responder(client);
  const [a,b]=await Promise.all([gateway.commandPacket(command()),gateway.commandPacket(command())]);
  assert.deepEqual(a,b);assert.equal(a.status.cells[0],'red');assert.equal(a.request_id,'same-intent');
  assert.equal(client.sent.filter(p=>p.value.command==='cells').length,1);
  assert.equal(client.sent.find(p=>p.value.command==='cells').value.request_id,'same-intent');
  await assert.rejects(gateway.commandPacket(command({state:'white'})),{code:'request_id_conflict'});
});

test('Nested payload conflict is not hidden by canonical field ordering',async()=>{
  const {client,gateway}=fixture();responder(client);
  const p=command({command:'cell-assignments',assignments:[{cell:1,state:'red'}]});
  await gateway.commandPacket(p);
  await assert.rejects(gateway.commandPacket({...p,assignments:[{cell:1,state:'white'}]}),{code:'request_id_conflict'});
});

test('Pixel commit waits for actual published frame through read-only refresh, never another mutation',async()=>{
  const client=new Client(),gateway=new Gateway({client,topic:'fixture',board,release,timeout:500});client.emit('connect');incoming(client,'status',snapshot());
  let reads=0;
  client.onPublish=p=>queueMicrotask(()=>{
    if(p.value.command==='telemetry')incoming(client,'status',{...snapshot(),pixel_frame:{pending:++reads<2,rgb:'00'.repeat(1440)}});
    else incoming(client,'event',ack(p.value));
  });
  const result=await gateway.commandPacket(command({command:'pixel-diagnostic',index:0,count:1,rgb:0,off:true}));
  assert.equal(result.status.pixel_frame.pending,false);assert.equal(reads,2);
  assert.equal(client.sent.filter(p=>p.value.command==='pixel-diagnostic').length,1);
});

test('Cloud ACK must match board, boot and result type; uncertain outcome is cached, never replayed',async()=>{
  for(const bad of [{board_id:'foreign'},{boot_id:'f'.repeat(32)},{type:'event'},{error:'ambiguous_success'}]){
    const {client,gateway}=fixture();client.onPublish=p=>{if(p.value.command==='cells')incoming(client,'event',ack(p.value,bad))};
    await assert.rejects(gateway.commandPacket(command()),{code:'invalid_board_receipt_outcome_unknown'});
    await assert.rejects(gateway.commandPacket(command()),{code:'invalid_board_receipt_outcome_unknown'});
    assert.equal(client.sent.filter(p=>p.value.command==='cells').length,1);
  }
});

test('Expired and foreign intents never publish; boot replacement retires in-flight jobs',async()=>{
  const {client,gateway}=fixture();const before=client.sent.length;
  await assert.rejects(gateway.commandPacket(command({expires_at_ms:1000})),{code:'command_expired'});
  await assert.rejects(gateway.commandPacket(command({boot_id:'f'.repeat(32)})),{code:'peer_changed'});
  assert.equal(client.sent.length,before);
  const pending=gateway.commandPacket(command());incoming(client,'status',{...snapshot(),boot_id:'f'.repeat(32)});
  await assert.rejects(pending,{code:'peer_changed_outcome_unknown'});
});

test('Event journal bounds, cursor gaps, retained and foreign events',()=>{
  const {client,gateway}=fixture();const events=[];gateway.on('event',e=>events.push(e));
  const event={type:'panel-event',board_id:board,boot_id:boot,text:'ready'};
  const start=gateway.eventPage().cursor;
  incoming(client,'event',event,true);incoming(client,'event',{...event,boot_id:'f'.repeat(32)});assert.equal(events.length,0);
  for(let i=0;i<65;i++)incoming(client,'event',{...event,index:i});
  assert.equal(events.length,65);assert.equal(gateway.eventPage().events.length,64);assert(gateway.eventPage(start).gap);
  assert.equal(gateway.eventPage(gateway.eventPage().cursor).events.length,0);
  assert(gateway.eventPage('previous-session:1').gap);
  incoming(client,'status',{...snapshot(),boot_id:'f'.repeat(32)});
  assert.equal(gateway.eventPage().events.length,0);
});

test('Configuration events reach remote WS immediately; saved hostname is not mistaken for applied identity',async()=>{
  const client=new Client(),origin='http://127.0.0.1',password='fixture';
  const panel=createPanel({client,html:'fixture',basic:'fixture',font:Buffer.alloc(0),release,
    config:{origin,board,topic:'fixture',username:'fixture',password_sha256:createHash('sha256').update(password).digest('hex')}});
  await new Promise(resolve=>panel.server.listen(0,'127.0.0.1',resolve));
  const base='http://127.0.0.1:'+panel.server.address().port,authorization='Basic '+Buffer.from('fixture:'+password).toString('base64');
  let ws;
  try{
    client.emit('connect');incoming(client,'status',{...snapshot(),hostname:'before'});
    ws=new WebSocket(base.replace('http:','ws:')+'/ws',{headers:{Origin:origin,Authorization:authorization}});
    await once(ws,'message');
    const notice=once(ws,'message');
    const saved={type:'configuration-changed',board_id:board,boot_id:boot,revision:1,phase:'saved',scopes:['identity'],hostname:'before',configured_hostname:'after',apply_pending:true,addresses:[{interface:'station',ip:'192.0.2.10'}]};
    incoming(client,'event',saved);
    assert.deepEqual(JSON.parse((await notice)[0]),saved);
    assert.equal((await (await fetch(base+'/api/device/name',{headers:{authorization}})).json()).hostname,'before');
    incoming(client,'status',{...snapshot(),hostname:'after',configuration_revision:2});
    const applied=await (await fetch(base+'/api/status',{headers:{authorization}})).json();
    assert.equal(applied.hostname,'after');assert.equal(applied.configuration_revision,2);
    const history=await (await fetch(base+'/api/events',{headers:{authorization}})).json();
    assert.equal(history.events[0].event.configured_hostname,'after');
    assert.equal(client.sent.filter(p=>p.value.command!=='telemetry').length,0);
  }finally{ws?.terminate();await panel.close()}
});

test('Real HTTP + two WebSocket clients exchange commands and board events concurrently',async()=>{
  const client=new Client(),origin='http://127.0.0.1',password='isolated-fixture';
  const panel=createPanel({client,html:'fixture',basic:'fixture',font:Buffer.alloc(0),release,timeout:300,
    config:{origin,board,topic:'fixture',username:'fixture',password_sha256:createHash('sha256').update(password).digest('hex')}});
  await new Promise(resolve=>panel.server.listen(0,'127.0.0.1',resolve));
  const base='http://127.0.0.1:'+panel.server.address().port,authorization='Basic '+Buffer.from('fixture:'+password).toString('base64');
  const sockets=[],received=[];
  try{
    client.emit('connect');incoming(client,'status',snapshot());
    for(let i=0;i<2;i++){
      const ws=new WebSocket(base.replace('http:','ws:')+'/ws',{headers:{Origin:origin,Authorization:authorization}});sockets.push(ws);received.push([]);
      ws.on('message',bytes=>received[i].push(JSON.parse(bytes)));await once(ws,'open');
    }
    // Do not let a pending mutation prevent a ping or a board event.
    const completed=new Promise(resolve=>sockets[0].on('message',bytes=>{const p=JSON.parse(bytes);if(p.type==='command-result')resolve(p)}));
    const request=command();sockets[0].send(JSON.stringify(request));
    while(!client.sent.some(p=>p.value.command==='cells'))await new Promise(resolve=>setTimeout(resolve,2));
    const event={type:'panel-event',board_id:board,boot_id:boot,text:'during-command'};incoming(client,'event',event);
    const pong=once(sockets[0],'message');sockets[0].send(JSON.stringify({type:'ping'}));await pong;
    const recovery=fetch(base+'/api/command',{method:'POST',headers:{authorization,'X-Pakar-Version':release.version,'Content-Type':'application/json'},body:JSON.stringify(request)});
    responder(client);incoming(client,'event',ack(request));
    const reply=await completed;assert.equal(reply.accepted,true);assert.equal(reply.status.cells[0],'red');
    const http=await recovery;assert.equal(http.status,200);assert.equal((await http.json()).request_id,request.request_id);
    assert.equal(client.sent.filter(p=>p.value.command==='cells').length,1);
    assert(received.every(list=>list.some(e=>e.type==='panel-event'&&e.text==='during-command')));
    assert(received[0].some(e=>e.type==='pong'));
    const journal=await (await fetch(base+'/api/events',{headers:{authorization}})).json();assert.equal(journal.events[0].event.text,'during-command');
  }finally{for(const ws of sockets)ws.terminate();await panel.close()}
});

test('Real isolated MQTT broker: HTTP/WS command-result and concurrent board events',
  {skip:!process.env.PAKAR_TEST_MQTT_URL,timeout:20000},async()=>{
  const endpoint=new URL(process.env.PAKAR_TEST_MQTT_URL);
  // Explicit loopback-only fixture: never use this test to actuate a real board.
  assert.equal(endpoint.protocol,'mqtt:');assert.equal(endpoint.hostname,'127.0.0.1');
  const topic='pakar-fixture/'+Date.now(),options={clean:true,queueQoSZero:false,reconnectPeriod:0,connectTimeout:5000};
  const boardClient=mqtt.connect(endpoint.href,options),relayClient=mqtt.connect(endpoint.href,options);
  const password='fixture',origin='http://127.0.0.1';
  const panel=createPanel({client:relayClient,html:'fixture',basic:'fixture',font:Buffer.alloc(0),release,timeout:5000,
    config:{origin,board,topic,username:'fixture',password_sha256:createHash('sha256').update(password).digest('hex')}});
  let ws;let mutations=0;
  const state=snapshot();
  const publish=(leaf,value)=>boardClient.publish(topic+'/'+leaf,JSON.stringify(value),{qos:0,retain:false});
  try{
    await Promise.all([once(boardClient,'connect'),once(relayClient,'connect')]);
    await new Promise((resolve,reject)=>boardClient.subscribe(topic+'/command',{qos:0},e=>e?reject(e):resolve()));
    boardClient.on('message',(_topic,bytes)=>{
      const p=JSON.parse(bytes);
      if(p.command==='telemetry'){publish('status',state);return}
      mutations++;state.cells[0]='red';
      publish('event',{type:'panel-event',board_id:board,boot_id:boot,text:'mqtt-wire-event'});
      publish('event',ack(p));
    });
    const fresh=once(panel.gateway,'status');panel.gateway.requestRead();await fresh;
    await new Promise(resolve=>panel.server.listen(0,'127.0.0.1',resolve));
    const base='http://127.0.0.1:'+panel.server.address().port,authorization='Basic '+Buffer.from('fixture:'+password).toString('base64');
    ws=new WebSocket(base.replace('http:','ws:')+'/ws',{headers:{Authorization:authorization,'X-Pakar-Version':release.version}});
    const messages=[];ws.on('message',bytes=>messages.push(JSON.parse(bytes)));await once(ws,'open');
    const result=new Promise(resolve=>ws.on('message',bytes=>{const p=JSON.parse(bytes);if(p.type==='command-result')resolve(p)}));
    const p=command({expires_at_ms:6000});ws.send(JSON.stringify(p));
    const receipt=await result;assert.equal(receipt.status.cells[0],'red');assert.equal(receipt.request_id,p.request_id);
    assert(messages.some(m=>m.type==='panel-event'&&m.text==='mqtt-wire-event'));
    const response=await fetch(base+'/api/command',{method:'POST',headers:{authorization,'X-Pakar-Version':release.version,'Content-Type':'application/json'},body:JSON.stringify(p)});
    assert.equal(response.status,200);assert.equal((await response.json()).status.cells[0],'red');assert.equal(mutations,1);
  }finally{ws?.terminate();await panel.close();await boardClient.endAsync(true)}
});
