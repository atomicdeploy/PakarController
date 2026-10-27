import test from 'node:test';
import assert from 'node:assert/strict';
import {Snake,logicalCell,position} from '../mini-apps/snake/engine.mjs';
import {Presenter} from '../mini-apps/snake/presenter.mjs';
import {createSnakeServer} from '../mini-apps/snake/server.mjs';
import {toolRelease} from '../tools/peer-identity.mjs';
import {WebSocket,WebSocketServer} from '../services/client-management/node_modules/ws/wrapper.mjs';
import {readFile} from 'node:fs/promises';

test('80 table addresses are bijective and match staggered columns',()=>{
  const ids=new Set();for(let y=0;y<4;y++)for(let x=0;x<20;x++){const id=logicalCell(x,y);ids.add(id);assert.deepEqual(position(id),{x,y});}
  assert.equal(ids.size,80);assert.deepEqual([0,1,2,3].map(y=>logicalCell(0,y)),[1,21,41,61]);assert.equal(logicalCell(1,0),11);
});
test('opening is inert; all four edges wrap',()=>{
  const s=new Snake();const before=s.snapshot();s.step();assert.deepEqual(s.snapshot(),before);
  assert.equal(s.next(logicalCell(19,2),'right'),logicalCell(0,2));
  assert.equal(s.next(logicalCell(0,2),'left'),logicalCell(19,2));
  assert.equal(s.next(logicalCell(7,0),'up'),logicalCell(7,3));
  assert.equal(s.next(logicalCell(7,3),'down'),logicalCell(7,0));
});
test('controls, growth, one queued turn, tail departure and collisions',()=>{
  const s=new Snake({random:()=>0});s.command('start');assert.equal(s.turn('left'),false);
  assert.equal(s.turn('up'),true);assert.equal(s.turn('left'),false);s.step();assert.equal(s.direction,'up');
  s.command('pause');const t=s.tick;s.step();assert.equal(s.tick,t);s.command('resume');s.step();assert.equal(s.tick,t+1);
  s.command('restart');s.food=[s.next(s.body[0],'right')];s.step();assert.equal(s.score,1);assert.equal(s.body.length,4);
  s.command('stop');s.setMap({[s.next(logicalCell(4,1),'right')]:'block'});s.command('restart');s.step();assert.equal(s.phase,'over');
});
test('map validation and bounded free food search',()=>{
  const s=new Snake();assert.throws(()=>s.setMap({81:'block'}));assert.throws(()=>s.setMap({1:'teleport'}));assert.throws(()=>s.configure({interval:0}));
  s.body=Array.from({length:80},(_,i)=>i+1);s.food=[];s.fillFood();assert.equal(s.phase,'won');
});
test('portals, switch and mirror mechanics are not executable map code',()=>{
  const s=new Snake({random:()=>0});const next=s.next(s.body[0],'right');s.setMap({[next]:'teleport',80:'teleport'});s.command('start');s.step();assert.equal(s.body[0],s.next(80,'right'));
  s.command('stop');s.reset();s.setMap({[next]:'switch',80:'block'});s.command('start');s.step();assert.equal(s.switchOpen,true);
  s.command('stop');s.reset();s.setMap({[next]:'detour1'});s.command('start');s.step();assert.equal(s.direction,'down');
});
function fixture({reject=false,address='192.0.2.2'}={}){
  const calls=[],board='table-test',boot='a'.repeat(32),identity={board_id:board,boot_id:boot};
  const status={...identity,version:toolRelease.version,http_peer_fencing:true,hostname:'ledcontroller',command_clock_ms:1000,
    capabilities:{hostname:'ledcontroller',acknowledged_features:['network','host-presentation','table-selection']}};
  let revision=0;
  const request=async(_url,path,options={})=>{
    calls.push({path,options});let value;
    if(path==='/api/status')value=status;
    else if(path==='/api/pixels/groups')value={...identity,revision:4,group_count:80};
    else if(path==='/api/presentation')value={...identity,revision,active:false};
    else{const packet=JSON.parse(options.form.packet);if(reject)throw Error('Lost receipt');value={...identity,request_id:packet.request_id,accepted:true,presentation:{revision:++revision,lease_id:9}};}
    return {status:200,body:Buffer.from(JSON.stringify(value)),remote:address};
  };return {calls,request,status};
}
test('temporary presentation uses logical cells, revision, release and no saved writes',async()=>{
  const f=fixture(),p=new Presenter({origin:'http://ledcontroller.local',request:f.request,environment:{}});await p.connect();
  assert.equal(f.calls.filter(c=>c.options.form).length,0);await p.frame([{id:80,rgb:0x00ff00}]);await p.renew();await p.close();
  const packets=f.calls.filter(c=>c.options.form).map(c=>JSON.parse(c.options.form.packet));
  assert.equal(packets.length,3);assert.equal(packets[0].space,'cells');assert.equal(packets[0].lifetime,'temporary');assert.equal(packets[1].lease_id,9);assert.equal(packets[1].replace,false);assert.equal(packets[2].action,'clear');
});

test('actual loopback WebSocket carries correlated frames; no HTTP mutations',async()=>{
  const f=fixture({address:'127.0.0.1'}),server=new WebSocketServer({port:0,host:'127.0.0.1'});
  await new Promise(resolve=>server.once('listening',resolve));let revision=0;const packets=[];
  server.on('connection',ws=>ws.on('message',raw=>{
    const v=JSON.parse(raw);
    if(v.type==='status')ws.send(JSON.stringify(f.status));
    else if(v.type==='command'){packets.push(v);ws.send(JSON.stringify({type:'command-result',board_id:f.status.board_id,boot_id:f.status.boot_id,
      request_id:v.request_id,accepted:true,presentation:{revision:++revision,lease_id:7}}));}
  }));
  const p=new Presenter({origin:`http://ledcontroller.local:${server.address().port}`,request:f.request,Socket:WebSocket,environment:{}});
  try{await p.connect();assert.equal(p.info().transport,'WebSocket');await p.frame([{id:1,rgb:0xff0000}]);await p.renew();await p.close();
    assert.equal(packets.length,3);assert.equal(packets[2].action,'clear');assert.equal(f.calls.filter(c=>c.options.form).length,0);
  }finally{await p.close();for(const ws of server.clients)ws.terminate();await new Promise(resolve=>server.close(resolve));}
});

test('runtime disabled rejects starting; invalid map leaves options untouched',async()=>{
  const app=await createSnakeServer({enabled:false});
  try{const r=await fetch(new URL('/api/snake',app.url),{method:'POST',headers:{'X-Snake-Token':app.token},body:'{"action":"start"}'});
    assert.equal(r.status,400);assert.equal(app.snapshot().phase,'ready');
  }finally{await app.close();}
  const other=await createSnakeServer();
  try{const before=other.snapshot().options;
    const r=await fetch(new URL('/api/snake',other.url),{method:'POST',headers:{'X-Snake-Token':other.token},body:JSON.stringify({action:'configure',options:{interval:80},map:{81:'block'}})});
    assert.equal(r.status,400);assert.deepEqual(other.snapshot().options,before);
  }finally{await other.close();}
});

test('hidden native and Android launchers only open external UI; board build defaults off',async()=>{
  const read=p=>readFile(new URL('../'+p,import.meta.url),'utf8');
  assert.match(await read('desktop/src/ui/shell.rs'),/Command::Snake => ctx.open_url/);
  assert.match(await read('mobile/android/app/src/main/AndroidManifest.xml'),/android:path="\/snake"/);
  const activity=await read('mobile/android/app/src/main/kotlin/ir/digitalogic/pakar/pakarcontroller/SnakeLauncherActivity.kt');
  assert.match(activity,/http:\/\/127.0.0.1:18490\/snake/);assert.doesNotMatch(activity,/api\/command|action.*start/);
  const role=await read('libraries/BoardCore/src/GameRole.h');assert.match(role,/#define PAKAR_BOARD_SNAKE 0/);
  assert.match(role,/#if PAKAR_BOARD_SNAKE && !PAKAR_TABLE_SELECTION/);
});
test('unknown receipt blocks replay and cleanup mutations',async()=>{
  const f=fixture({reject:true}),p=new Presenter({origin:'http://ledcontroller.local',request:f.request,environment:{}});await p.connect();
  await assert.rejects(p.frame([{id:1,rgb:0}]),/Lost receipt/);await assert.rejects(p.frame([]),/unknown/);await p.close();
  assert.equal(f.calls.filter(c=>c.options.form).length,1);
});
test('real loopback launcher serves inert UI, gates POST and controls host game',async()=>{
  let connected=0;class NeverConnect{constructor(){connected++;throw Error('Unexpected board contact');}}
  const app=await createSnakeServer({PresenterClass:NeverConnect});
  try{
    const root=new URL(app.url).origin;
    assert.match(await fetch(app.url).then(r=>r.text()),/DRS Snake/);assert.equal(app.snapshot().phase,'ready');assert.equal(connected,0);
    assert.equal((await fetch(root+'/api/snake',{method:'POST',body:'{}'})).status,403);
    const cmd=async body=>fetch(root+'/api/snake',{method:'POST',headers:{'X-Snake-Token':app.token},body:JSON.stringify(body)}).then(r=>r.json());
    assert.equal((await cmd({action:'start',board:false})).phase,'running');assert.equal((await cmd({action:'pause'})).phase,'paused');
    assert.equal((await cmd({action:'resume'})).phase,'running');assert.equal((await cmd({action:'stop'})).phase,'stopped');assert.equal(connected,0);
  }finally{await app.close();}
});
