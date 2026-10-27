// Execute canonical controls through their shared command-owner hook. This
// proves dispatch and readback contracts, not transport timing or LED output.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import vm from 'node:vm';
const source=await readFile(new URL('../web/index.html',import.meta.url),'utf8');
const code=source.slice(source.indexOf('let presentationBusy=false;'),source.indexOf('function renderStateAppearance(){'));
function fixture(){
  const sent=[],reads=[],renders=[],errors=[];let active={board:'board',boot:'a'},enabled=true,role='table-selection',receipt,readback={state_presentation:{enabled:true,states:{lost:{rgb:0x112233}}}},deferred;
  const context=vm.createContext({selectedCells:new Set([1,7]),document:{getElementById:()=>({value:'#112233'})},
    currentPeer:()=>({has:()=>enabled,role}),peerHttp:{pin:()=>active,assertOwner:owner=>{if(owner!==active)throw Error('peer changed')}},
    controlRequest:async(path,data)=>{sent.push({path,data});return deferred?deferred:receipt;},
    api:async(path,options)=>{reads.push({path,options});assert.equal(path,'/api/status');assert.equal(options.peer,active);return readback;},
    render:value=>renders.push(value),report:error=>errors.push(error),renderStateAppearance(){}});
  vm.runInContext(code,context);
  return {context,sent,reads,renders,errors,run:text=>vm.runInContext(text,context),
    receipt:value=>receipt=value,readback:value=>readback=value,allow:value=>enabled=value,role:value=>role=value,changePeer:()=>active={board:'other',boot:'b'},defer:value=>deferred=value};
}
{
  const f=fixture();f.receipt({status:{state_presentation:{enabled:true,states:{lost:{rgb:0x112233}},other:'additive'}}});
  await f.run('saveStateAppearance({enabled:true,states:{lost:{rgb:0x112233}}})');
  assert.equal(f.sent.length,1);assert.equal(f.sent[0].path,'/api/led/presentation');
  assert.deepEqual(JSON.parse(f.sent[0].data.settings),{enabled:true,states:{lost:{rgb:0x112233}}});
  assert.equal(f.reads.length,0,'Correlated receipt state avoids another HTTP read');assert.equal(f.renders.length,1);assert.equal(f.errors.length,0);
}
{
  const f=fixture();await f.run('saveStateAppearance({enabled:true})');
  assert.equal(f.reads.length,1,'Absent receipt snapshot uses pinned readback');assert.equal(f.renders.length,1);
  f.readback({state_presentation:{enabled:false}});await f.run('saveStateAppearance({enabled:true})');
  assert.equal(f.sent.length,2,'Mismatch is not replayed');assert.equal(f.renders.length,1);assert.match(f.errors[0].message,/readback differs/i);
}
{
  const f=fixture();let resolve;f.defer(new Promise(done=>resolve=done));
  const pending=f.run('saveStateAppearance({enabled:true})');await f.run('saveStateAppearance({enabled:false})');assert.equal(f.sent.length,1,'One appearance mutation owns the pending state');
  f.changePeer();resolve({status:{state_presentation:{enabled:true}}});await pending;
  assert.equal(f.renders.length,0);assert.equal(f.reads.length,0);assert.match(f.errors[0].message,/peer changed/);
}
{
  const f=fixture();await f.run('temporaryStateColor()');assert.equal(f.sent[0].path,'/api/led/preview');
  assert.deepEqual(JSON.parse(JSON.stringify(f.sent[0].data)),{ids:'1,7',rgb:0x112233,duration_ms:5000});
  assert.equal(f.renders.length,0,'A test color does not optimistically rewrite semantic state');
  assert.deepEqual([...f.context.selectedCells],[1,7]);
  f.context.selectedCells.clear();f.role('first-wins');await f.run('temporaryStateColor()');assert.equal(f.sent[1].data.ids,'1-18');
  f.role('table-selection');await f.run('temporaryStateColor()');assert.equal(f.sent[2].data.ids,'1-80');
  await f.run('temporaryStateColor(true)');assert.deepEqual(JSON.parse(JSON.stringify(f.sent[3].data)),{cancel:true});
  f.allow(false);await f.run('temporaryStateColor()');await f.run('saveStateAppearance({enabled:true})');assert.equal(f.sent.length,4,'Missing capability cannot dispatch');
}
console.log('Canonical state presentation: shared WS-first owner hook, nested ACK-state validation, pinned fallback readback, no uncertain replay, mutation exclusion, peer-change rejection and non-semantic preview passed.');
