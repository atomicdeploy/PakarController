import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const source=readFileSync(new URL('../assets/web/frame-stream.js',import.meta.url),'utf8').replaceAll('\r\n','\n').trim();
const web=readFileSync(new URL('../firmware/esp/src/WebUi.h',import.meta.url),'utf8').replaceAll('\r\n','\n');
assert.equal(web.split('// BEGIN FRAME STREAM')[1].split('// END FRAME STREAM')[0].trim(),source,'One actual embedded streaming engine');
const {createFrameStream,streamPolicy}=vm.runInNewContext(source+'\n({createFrameStream,streamPolicy})');
// Ported actual donor FakeAnimationFrameClock: cancelled callbacks are retained
// so stale delivery really executes, instead of merely assuming cancellation.
class FakeFrameClock {
  next=1; time=0; callbacks=new Map(); cancelled=[];
  now=()=>this.time;
  request=(fn,delay)=>{const id=this.next++;this.callbacks.set(id,{fn,delay});return id};
  cancel=id=>{this.cancelled.push(id)};
  fire(id){const callback=this.callbacks.get(id);assert(callback,'Scheduled callback exists');this.callbacks.delete(id);callback.fn()}
  delay(id=this.next-1){return this.callbacks.get(id)?.delay}
}
function fixture(overrides={}) {
  const clock=new FakeFrameClock(),sent=[],reports=[];
  const socket={readyState:1,bufferedAmount:0,send:b=>sent.push(b)};
  const settings={fps:60,queueFrames:2};let permitted=true;
  const options={clock,policy:()=>settings,valid:s=>permitted&&s===socket,
    frame:()=>[new Uint8Array(730),new Uint8Array(730)],status:s=>reports.push(s),...overrides};
  return {clock,socket,settings,sent,reports,stream:createFrameStream(options),deny:()=>{permitted=false}};
}
assert.equal(streamPolicy(0,1).interval,0,'0 is uncapped: no 20/25 FPS or vsync clamp');
assert.equal(streamPolicy(240,16).maxBytes,23360);
for(const [fps,queue] of [[-1,2],[241,2],[NaN,2],[1.5,2],['60',2],[60,0],[60,17],[60,1.5]])assert.throws(()=>streamPolicy(fps,queue));
{
  const f=fixture();f.stream.start(f.socket);assert.equal(f.clock.delay(),0);f.clock.fire(1);
  assert.equal(f.sent.length,2);assert.equal(f.stream.snapshot().sentFrames,1);assert.equal(f.clock.delay(),1000/60);
  f.settings.fps=0;f.clock.time=1000;f.clock.fire(2);
  assert.equal(f.clock.delay(),0);assert.equal(f.sent.length,4,'Late callback sends one latest frame, never catch-up history');
  assert.equal(f.reports.at(-1).sentFps,2,'Measured browser sends only');
  f.settings.fps=120;f.clock.fire(3);assert.equal(f.clock.delay(),1000/120);
  f.settings.fps=240;f.clock.fire(4);assert.equal(f.clock.delay(),1000/240);
  f.stream.stop();const receipt=f.stream.snapshot();f.clock.time=90000;
  assert.equal(f.stream.snapshot().sentFps,receipt.sentFps,'Stopped receipt rate is frozen');
}
{
  const f=fixture();f.socket.send=b=>{f.sent.push(b);f.clock.time+=2};f.stream.start(f.socket);f.clock.fire(1);
  assert.equal(f.clock.delay(),1000/60-4,'Frame work counts toward pacing interval');
  f.socket.send=b=>{f.sent.push(b);f.clock.time+=50};f.clock.fire(2);
  assert.equal(f.clock.delay(),0,'Slow work does not add another interval or replay missed frames');
}
{
  const f=fixture();f.stream.start(f.socket);f.socket.bufferedAmount=1461;f.clock.fire(1);
  assert.equal(f.sent.length,0);assert.equal(f.stream.snapshot().skipped,1);
  f.socket.bufferedAmount=1460;f.clock.fire(2);assert.equal(f.sent.length,2,'Exactly two-frame window admitted');
  f.settings.queueFrames=1;f.socket.bufferedAmount=1;f.clock.fire(3);assert.equal(f.sent.length,2);
  f.socket.bufferedAmount=0;f.clock.fire(4);assert.equal(f.sent.length,4,'Latest frame resumes only after queue drains');
}
{
  // Actual donor generation-change/cancel/dispose cases, strengthened so a
  // stale callback cannot clear the replacement owner's timer handle.
  const f=fixture();f.stream.start(f.socket);f.stream.stop();f.stream.start(f.socket);
  assert.deepEqual(f.clock.cancelled,[1]);f.clock.fire(1);assert.equal(f.sent.length,0);
  f.stream.dispose();assert.deepEqual(f.clock.cancelled,[1,2]);f.clock.fire(2);assert.equal(f.sent.length,0);
  assert.throws(()=>f.stream.start(f.socket),/disposed/);f.stream.dispose();assert.deepEqual(f.clock.cancelled,[1,2]);
}
{
  const f=fixture();f.stream.start(f.socket);f.deny();f.clock.fire(1);
  assert.equal(f.sent.length,0);assert.equal(f.stream.running(),false);assert.match(f.stream.snapshot().reason,/peer, capability or connection/);
  assert.throws(()=>f.stream.start(f.socket),/acknowledged/);
}
{
  const f=fixture();f.socket.send=b=>{f.sent.push(b);f.deny()};f.stream.start(f.socket);f.clock.fire(1);
  assert.equal(f.sent.length,1);assert.equal(f.stream.snapshot().sentChunks,1);assert.equal(f.stream.snapshot().sentFrames,0);
  assert.match(f.stream.snapshot().reason,/partial frame outcome unknown; not replayed/);assert.equal(f.clock.next,2,'No retry after partial outcome');
}
{
  const f=fixture();f.socket.send=b=>{if(f.sent.length)throw Error('Private transport detail');f.sent.push(b)};
  f.stream.start(f.socket);f.clock.fire(1);assert.equal(f.stream.running(),false);
  assert.equal(f.stream.snapshot().sentFrames,0);assert.equal(f.stream.snapshot().sentChunks,1);
  assert.match(f.stream.snapshot().reason,/outcome unknown, not replayed/);assert.doesNotMatch(f.stream.snapshot().reason,/Private/);assert.equal(f.clock.next,2);
}
for(const amount of [undefined,NaN,-1,0.5]) {
  const f=fixture();f.socket.bufferedAmount=amount;f.stream.start(f.socket);f.clock.fire(1);
  assert.equal(f.sent.length,0);assert.match(f.stream.snapshot().reason,/buffering is unknown/);
}
{
  const f=fixture({frame:()=>[new Uint8Array(729),new Uint8Array(730)]});
  f.stream.start(f.socket);f.clock.fire(1);assert.equal(f.sent.length,0);assert.equal(f.stream.running(),false);
}
{
  const f=fixture();f.stream.start(f.socket);f.settings.fps=NaN;f.clock.fire(1);
  assert.equal(f.sent.length,0);assert.equal(f.stream.running(),false);
}
{
  let f;f=fixture({status:s=>{if(s.active)f.stream.stop()}});f.stream.start(f.socket);
  assert.equal(f.clock.next,1,'Status-driven stop cannot leave an orphan timer');
}
// Actual packet builder: two ordered chunks, 480 physical pixels and commit
// only on the second chunk. No physical receipt is inferred from this fixture.
const packetSource=web.slice(web.indexOf('function rgb(h)'),web.indexOf('// BEGIN FRAME STREAM'));
const chunks=vm.runInNewContext('let streamHue=0;'+packetSource+';[packet(0,240,false),packet(240,240,true)]',{Uint8Array});
assert.deepEqual([...chunks[0].slice(0,10)],[80,75,65,82,0,0,0,0,240,0]);
assert.deepEqual([...chunks[1].slice(0,10)],[80,75,65,82,0,0,240,0,240,1]);
assert.equal(chunks[0].byteLength+chunks[1].byteLength,1460);
let departure,stopped=0,disposed=0;
vm.runInNewContext(web.split('\n').find(line=>line.startsWith("globalThis.addEventListener?.('pagehide'")),{
  addEventListener:(_,fn)=>{departure=fn},webSocketReconnect:{cancel(){}},stopStream:()=>stopped++,frameStream:{dispose:()=>disposed++},webToasts:null
});
departure({persisted:true});assert.equal(stopped,1);assert.equal(disposed,0,'Back-cache departure pauses but preserves restartable stream owner');
departure({persisted:false});assert.equal(disposed,1,'Final page departure disposes owner');
assert.match(web,/frameStream\.dispose\(\)/);assert.match(web,/document\.hidden\)stopStream\(\)/);
assert.match(web,/peerHttp\.assertOwner\(streamPeer\)/);assert.doesNotMatch(web,/setInterval\(streamTick,40\)/);
console.log('Actual Web streaming: configurable/uncapped pacing, bounded latest-frame buffering, late-generation cancellation, disposal, peer loss and unknown partial sends passed. Browser fixtures, not physical LED refresh.');
