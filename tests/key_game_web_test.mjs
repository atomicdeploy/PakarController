import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

const source=readFileSync(new URL('../web/index.html',import.meta.url),'utf8');
const code=source.split('// BEGIN KEY GAME PRESS CONTROLS')[1].split('// END KEY GAME PRESS CONTROLS')[0];
function fixture(){
  const calls=[];
  const peer={known:true,role:'first-wins',has:()=>true};
  const context=vm.createContext({state:{board_id:'board',boot_id:'boot',winner:0,key_mask_applied:true,key_game:{enabled_mask:3}},
    currentPeer:()=>peer,peerHttp:{writable:()=>true},renderKeyGame(){},post:async(path,data)=>{calls.push({path,...data});return true}});
  vm.runInContext(code,context);
  const run=s=>vm.runInContext(s,context);
  run('syncKeyGameOwner()');
  return {context,peer,calls,run,arm:()=>run('keyGameControl.readOnly=false')};
}
{
  const f=fixture();
  await f.run('pressGameKey(1)');assert.equal(f.calls.length,0,'Read-only by default');
  f.arm();await f.run('pressGameKey(1)');
  assert.deepEqual(f.calls,[{path:'/api/game/press',key:1}]);
  assert.equal(f.context.state.winner,0,'No optimistic winner or color changes');
}
for(const mutate of [f=>f.context.state.winner=1,f=>f.context.state.programming_latch=true,
  f=>f.context.state.key_mask_applied=false,f=>f.peer.known=false,f=>f.peer.role='table',
  f=>f.peer.has=()=>false,f=>f.context.peerHttp.writable=()=>false]){
  const f=fixture();f.arm();mutate(f);await f.run('pressGameKey(1)');assert.equal(f.calls.length,0);
}
for(const key of [0,3,9,10,1.5,'1',null]){
  const f=fixture();f.arm();await f.run(`pressGameKey(${JSON.stringify(key)})`);assert.equal(f.calls.length,0);
}
{
  const f=fixture();let finish;
  f.context.post=()=>{f.calls.push('sent');return new Promise(resolve=>finish=resolve)};
  f.arm();const first=f.run('pressGameKey(1)');const finishFirst=finish;await f.run('pressGameKey(2)');assert.equal(f.calls.length,1,'No overlapping presses');
  f.context.state.boot_id='next';f.run('syncKeyGameOwner()');assert.equal(f.run('keyGameControl.readOnly'),true);
  f.arm();const second=f.run('pressGameKey(2)');const finishSecond=finish;
  // Retain both completions to prove an old response cannot release a newer command.
  assert.equal(f.calls.length,2);finishFirst(true);await first;
  assert.notEqual(f.run('keyGameControl.pending'),null,'Old response cannot release newer pending command');
  finishSecond(true);await second;
  assert.equal(f.run('keyGameControl.pending'),null);
}
for(const reject of [false,true]){
  const f=fixture();f.arm();f.context.post=async()=>{f.calls.push('sent');if(reject)throw Error('transport failed');return false};
  if(reject)await assert.rejects(f.run('pressGameKey(1)'));else await f.run('pressGameKey(1)');
  assert.equal(f.calls.length,1,'Never replay a failed command');assert.equal(f.run('keyGameControl.pending'),null);
}
{
  const f=fixture();f.arm();f.context.peerHttp.writable=()=>false;f.run('syncKeyGameOwner()');
  assert.equal(f.run('keyGameControl.readOnly'),true,'Disconnect disarms');
}
assert(source.includes('if(!miniGameMomentary())pressGameKey(key)'));
assert(!source.includes('data-test-press')&&!source.includes('data-test-key'),'No separate trigger controls');
assert(source.includes('button:disabled{opacity:1;cursor:default}'),'Read-only retains live colors');
console.log('Key game main-box press, read-only, peer and pending guards passed');
