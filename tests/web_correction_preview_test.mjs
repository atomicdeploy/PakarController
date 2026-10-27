// Production controller/projection executed with pinned transport and clock
// doubles. No browser, board, optical output or real lease acceptance proof.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const read=name=>readFileSync(new URL('../'+name,import.meta.url),'utf8');
const context=vm.createContext({});vm.runInContext(read('assets/web/feature-workspaces.js'),context);vm.runInContext(read('assets/web/correction-preview.js'),context);
const project=(...args)=>Array.from(context.WebFeatureContracts.correctionSources(...args));
const drain=()=>new Promise(setImmediate);
function harness(){
 let time=0,key='board:boot:route',permitted=true,nextTimer=0,active=false,lease=7,expected=[],sendHook,readHook;
 let config={revision:0,enabled:true,rules:[{kind:'black',pixel:0,offset:0,enabled:true}]};
 const timers=new Map(),sent=[],reads=[],notices=[];
 const observation=()=>({revision:config.revision,active_sources:[...expected],preview:{active,lease_id:active?lease:0,base_revision:config.revision,remaining_ms:active?10000:0}});
 async function send(data,identity){sent.push({data:structuredClone(data),key:identity.key});if(sendHook)return sendHook(data,identity);
  if(data.clear){assert.equal(data.lease_id,lease);active=false;return observation();}
  assert.equal(data.revision,config.revision);if(active)assert.equal(data.lease_id,lease);else assert.equal(data.lease_id,undefined);
  active=true;expected=project(6,data.enabled,data.rules);return observation();
 }
 const preview=context.createPixelCorrectionPreview({owner:()=>({key,pin:'opaque-'+key}),allowed:()=>permitted,configuration:()=>config,pixels:()=>6,project,send,read:async identity=>{reads.push(identity.key);return readHook?readHook():observation();},onChange:value=>notices.push(value),now:()=>time,setTimer:(fn,ms)=>{const id=++nextTimer;timers.set(id,{fn,at:time+ms});return id;},clearTimer:id=>timers.delete(id)});
 return {preview,sent,reads,notices,timers,observation,set sendHook(value){sendHook=value;},set readHook(value){readHook=value;},set config(value){config=value;},get config(){return config;},set key(value){key=value;},set allowed(value){permitted=value;},set time(value){time=value;},get time(){return time;},
  async advance(ms){const end=time+ms;for(let n=0;n<100;n++){const due=[...timers].filter(([,v])=>v.at<=end).sort((a,b)=>a[1].at-b[1].at)[0];if(!due)break;time=due[1].at;timers.delete(due[0]);due[1].fn();await drain();}time=end;await drain();}};
}
const base=harness();assert.equal(base.sent.length,0);assert.equal(base.timers.size,0,'No automatic preview or timers before explicit start');
assert.equal(await base.preview.start(),true);assert.equal(base.reads.length,1);assert.equal(base.preview.snapshot().lease,7);assert.equal(base.sent[0].data.ttl_ms,10000);
assert.equal(await base.preview.start(),false,'Duplicate start cannot acquire another lease');
base.config={...base.config,rules:[{kind:'remove',pixel:1,offset:0,enabled:false}]};base.preview.edited();base.preview.edited();await base.advance(279);assert.equal(base.sent.length,1);await base.advance(1);
assert.equal(base.sent.length,2);assert.equal(base.sent[1].data.lease_id,7);assert.equal(base.sent[1].data.rules[0].enabled,false,'Debounced edits preserve disabled flags');
const beforeRenew=base.sent.length;await base.advance(5000);assert.equal(base.sent.length,beforeRenew+1,'Only a confirmed owned lease gets one-shot renewal');
assert.equal(await base.preview.stop(),true);assert.equal(base.sent.at(-1).data.clear,true);assert.equal(base.preview.snapshot().enabled,false);assert.equal(base.timers.size,0);await base.advance(30000);assert.equal(base.sent.length,beforeRenew+2,'Stop cancels all renew/debounce/expiry work');

const changed=harness();await changed.preview.start();changed.key='other-board:boot:route';changed.preview.observe();await drain();assert.equal(changed.preview.snapshot().enabled,false);assert.equal(changed.sent.length,1,'Cleanup never redirects an old lease to another board');assert.equal(changed.timers.size,0);
const route=harness();await route.preview.start();route.key='board:boot:reconnected-route';await route.preview.stop();assert.equal(route.sent.length,1,'Same board/boot on a new route is a different connection owner');
const denied=harness();denied.allowed=false;assert.equal(await denied.preview.start(),false);assert.equal(denied.sent.length,0);

const cancelled=harness();let late;cancelled.sendHook=()=>new Promise(resolve=>late=resolve);const pendingStart=cancelled.preview.start();await cancelled.preview.stop();assert.equal(await cancelled.preview.start(),false,'A stopped in-flight acquisition cannot overlap a new start');
cancelled.sendHook=async data=>{assert.equal(data.clear,true);return {revision:0,preview:{active:false,lease_id:0,base_revision:0,remaining_ms:0}};};late({preview:{active:true,lease_id:7}});assert.equal(await pendingStart,false);assert.equal(cancelled.reads.length,0);assert.equal(cancelled.sent.length,2,'Late acquisition is released once, not revived');assert.equal(cancelled.preview.snapshot().lease,null);
const latePeer=harness();let foreignReply;latePeer.sendHook=()=>new Promise(resolve=>foreignReply=resolve);const foreignStart=latePeer.preview.start();latePeer.key='replacement';foreignReply({preview:{active:true,lease_id:7}});assert.equal(await foreignStart,false);assert.equal(latePeer.sent.length,1,'No delayed release to a replacement owner');assert.equal(latePeer.reads.length,0);

const mismatch=harness();mismatch.readHook=()=>({...mismatch.observation(),active_sources:[0,1,2,3,4,5]});assert.equal(await mismatch.preview.start(),false);assert.match(mismatch.preview.snapshot().error,/readback differs/);assert.equal(mismatch.sent.length,2);assert.equal(mismatch.sent[1].data.lease_id,7);
for(const patch of [{remaining_ms:0},{remaining_ms:10001},{base_revision:1},{lease_id:8},{active:false}]){const bad=harness();bad.readHook=()=>({...bad.observation(),preview:{...bad.observation().preview,...patch}});assert.equal(await bad.preview.start(),false);assert.equal(bad.sent.length,2);assert.equal(bad.sent.at(-1).data.lease_id,7,'Malformed readback releases only the acquired owner ID');}
const foreignLease=harness();await foreignLease.preview.start();foreignLease.sendHook=data=>data.clear?{revision:0,preview:{active:false,lease_id:0,base_revision:0,remaining_ms:0}}:{preview:{active:true,lease_id:8}};foreignLease.preview.edited();await foreignLease.advance(280);assert.equal(foreignLease.preview.snapshot().enabled,false);assert.equal(foreignLease.sent.at(-1).data.lease_id,7,'Foreign renewal never clears the other editor lease');
const invalidConfig=harness();invalidConfig.config={revision:0,enabled:true,rules:[{kind:'black',pixel:6}]};assert.equal(await invalidConfig.preview.start(),false);assert.equal(invalidConfig.sent.length,0,'Canonical bounds fail before acquisition');
const cancelledRead=harness();let settleRead;cancelledRead.readHook=()=>new Promise(resolve=>settleRead=resolve);const reading=cancelledRead.preview.start();await drain();await cancelledRead.preview.stop();settleRead(cancelledRead.observation());assert.equal(await reading,false);assert.equal(cancelledRead.sent.length,2,'Late readback cannot revive a cancelled acquisition');
const lateConfirmation=harness();lateConfirmation.readHook=()=>{lateConfirmation.time=9800;return lateConfirmation.observation();};assert.equal(await lateConfirmation.preview.start(),false);assert.match(lateConfirmation.preview.snapshot().error,/too late/);assert.equal(lateConfirmation.sent.length,2);
const uncertain=harness();uncertain.sendHook=()=>{throw Error('uncertain transport outcome');};assert.equal(await uncertain.preview.start(),false);assert.equal(uncertain.sent.length,1,'Unknown acquisition outcome is never replayed or cleared without an owned ID');assert.equal(uncertain.timers.size,0);

const revision=harness();await revision.preview.start();revision.config={...revision.config,revision:1};revision.preview.edited();await revision.advance(280);assert.equal(revision.sent.length,2);assert.equal(revision.sent[1].data.clear,true);assert.equal(revision.sent[1].data.revision,0,'Saved-revision change releases only the original base');assert.match(revision.preview.snapshot().error,/Saved corrections changed/);
const committed=harness();await committed.preview.start();committed.preview.committed();await committed.advance(30000);assert.equal(committed.sent.length,1,'Only a verified external save may retire the lease without another command');assert.equal(committed.preview.snapshot().enabled,false);

const queued=harness();await queued.preview.start();let settleRenew;queued.sendHook=data=>data.clear?{revision:0,preview:{active:false,lease_id:0,base_revision:0,remaining_ms:0}}:new Promise(resolve=>settleRenew=resolve);
await queued.advance(5000);assert(queued.preview.snapshot().pending);queued.preview.edited();await queued.advance(280);queued.config={...queued.config,rules:[]};settleRenew(queued.observation());await drain();queued.sendHook=undefined;await queued.advance(280);assert.equal(queued.sent.at(-1).data.rules.length,0,'Pending edits coalesce to the latest configuration');await queued.preview.stop();
const expires=harness();await expires.preview.start();let expiredRenew;expires.sendHook=data=>data.clear?{revision:0,preview:{active:false,lease_id:0,base_revision:0,remaining_ms:0}}:new Promise(resolve=>expiredRenew=resolve);
await expires.advance(10000);assert.equal(expires.preview.snapshot().enabled,false);assert.equal(expires.sent.filter(r=>r.data.clear).length,1);expiredRenew({preview:{active:true,lease_id:7}});await drain();assert.equal(expires.sent.filter(r=>r.data.clear).length,1,'Late expiry cleanup reuses the same release result, never resends it');
const cleanup=harness();await cleanup.preview.start();cleanup.sendHook=()=>{throw Error('offline');};assert.equal(await cleanup.preview.stop(),false);assert.match(cleanup.preview.snapshot().error,/release unconfirmed/);assert.equal(cleanup.sent.length,2);assert.equal(cleanup.timers.size,0);
const disposed=harness();await disposed.preview.start();await disposed.preview.dispose();await disposed.advance(30000);assert.equal(disposed.sent.length,2);assert.equal(await disposed.preview.start(),false);
console.log('Web correction preview controller: explicit-only lease, canonical projection, debounce/coalescing, owner/route/revision guards, exact readback, bounded renewal/expiry, no replay and cleanup races passed. Staged helper; no UI/board acceptance.');
