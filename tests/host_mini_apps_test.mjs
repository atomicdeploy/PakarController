import assert from 'node:assert/strict';
import {runMiniApp,snakeFrame,memoryFrame,cellAt} from '../examples/host-mini-apps.mjs';
import {toolPolicy} from '../tools/peer-identity.mjs';
const board='esp32s3-test',boot='1234567890abcdef1234567890abcdef';
function fixture({unknown=false,existing=false,networkDelay=0}={}){
  let time=0,revision=0,owner='',lease=0;const writes=[],waits=[],writeTimes=[];
  const reply=value=>({status:200,remote:'192.168.4.1',body:Buffer.from(JSON.stringify(value))});
  const identity={board_id:board,boot_id:boot};
  const request=async(_origin,path,options)=>{
    time+=networkDelay;
    if(path==='/api/status')return reply({...identity,version:toolPolicy.minimum_board,
      http_peer_fencing:true,hostname:'ledcontroller',command_clock_ms:time,
      capabilities:{hostname:'ledcontroller',acknowledged_features:['network','host-presentation']}});
    if(path==='/api/pixels/groups')return reply({...identity,group_count:80,revision:7});
    if(path==='/api/presentation')return reply({...identity,revision,owner,lease_id:lease,
      active:existing||!!lease,persistent:false,temporary:!!lease});
    assert.equal(path,'/api/command');const packet=JSON.parse(options.form.packet);writes.push(packet);writeTimes.push(time);
    assert.match(packet.request_id,/^[0-9a-f]{32}$/);assert.equal(packet.board_id,board);assert.equal(packet.boot_id,boot);
    assert.equal(packet.lifetime,'temporary');assert.equal(packet.group_layout_revision,7);
    assert.equal(packet.revision,revision);assert.equal(packet.expires_at_ms,time+8000-networkDelay*2);
    ++revision;owner=packet.owner;lease=packet.action==='clear'?0:123;
    if(unknown)throw Error('Simulated lost receipt');
    return reply({...identity,request_id:packet.request_id,accepted:true,presentation:{revision,lease_id:lease}});
  };
  return {request,writes,waits,writeTimes,now:()=>time,wait:async ms=>{time+=ms;waits.push(ms)}};
}
const geometry=Array.from({length:80},(_,i)=>cellAt(i%20,Math.floor(i/20)));
assert.equal(new Set(geometry).size,80);assert.equal(Math.min(...geometry),1);assert.equal(Math.max(...geometry),80);
for(let i=0;i<80;++i){const rows=snakeFrame(i);assert.equal(rows.length,5);assert.equal(new Set(rows.map(r=>r.id)).size,5);}
assert.equal(memoryFrame().length,5);assert.equal(memoryFrame(true).length,0);
let f=fixture();await assert.rejects(runMiniApp({...f,log:()=>{}}),/Explicit --run/);assert.equal(f.writes.length,0);
f=fixture();await runMiniApp({...f,mode:'snake',steps:3,run:true,log:()=>{}});
assert.equal(f.writes.length,4);assert.equal(f.writes.at(-1).action,'clear');
f=fixture();await runMiniApp({...f,mode:'memory',run:true,log:()=>{}});
const blank=f.writes.findIndex(p=>p.replace&&p.entries?.length===0);
assert.equal(f.writeTimes[blank]-f.writeTimes[0],10000);
assert.equal(f.writes.at(-1).action,'clear');assert(f.writes.slice(1,blank).every(p=>p.replace===false&&p.entries.length===0));
f=fixture({networkDelay:400});await runMiniApp({...f,mode:'memory',run:true,log:()=>{}});
const delayedBlank=f.writes.findIndex(p=>p.replace&&p.entries?.length===0);
assert(f.writeTimes[delayedBlank]-f.writeTimes[0]>=10000);
assert(f.writeTimes[delayedBlank]-f.writeTimes[0]<=11200); // Only final3x400ms read/read/write remains.
f=fixture({unknown:true});await assert.rejects(runMiniApp({...f,run:true,log:()=>{}}),/lost receipt/);
assert.equal(f.writes.length,1); // No blind release/replay after uncertain acceptance.
f=fixture({existing:true});await assert.rejects(runMiniApp({...f,run:true,log:()=>{}}),/Existing presentation/);
assert.equal(f.writes.length,0);
console.log('Host mini-app examples:80-cell geometry, ten-second memory phase, temporary-only fenced commands, explicit-run gate and unknown-outcome no-replay cleanup passed (mocked I/O).');
