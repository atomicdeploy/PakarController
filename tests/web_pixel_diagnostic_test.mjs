import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const web=readFileSync(new URL('../firmware/esp/src/WebUi.h',import.meta.url),'utf8');
const source=web.split('// BEGIN PIXEL DIAGNOSTIC')[1].split('// END PIXEL DIAGNOSTIC')[0];
function fixture(){
  const nodes=new Map(),sent=[],calls=[],errors=[];
  function element(){const n={children:[],hidden:false,value:'',disabled:false,setAttribute(){},appendChild(child){child.parentElement=this;this.children.push(child);return child},append(...items){items.forEach(v=>this.appendChild(v))}};
    Object.defineProperty(n,'id',{get(){return this.key},set(value){this.key=value;nodes.set(value,this)}});return n}
  const card=element(),row=element(),pattern=element();pattern.id='pattern';card.appendChild(row);row.appendChild(pattern);
  const owner={generation:1},features=new Set(['leds','streaming','table']);
  const ws={readyState:1,bufferedAmount:0,send(bytes){sent.push(new Uint8Array(bytes));context.onSent?.(sent.length)}};
  const status={pixels:480,led_output:{enabled:true,mode:'static'}};
  const context=vm.createContext({Uint8Array,URLSearchParams,Number,Error,authenticated:true,state:status,socket:ws,
    pixelDiagnosticPending:false,ledOutputPending:false,streamStarting:false,streamRequest:0,owner,features,
    streamStart:{disabled:false},streamStop:{disabled:true},streaming:false,
    frameStream:{running:()=>context.streaming},currentPeer:()=>({has:key=>features.has(key)}),
    peerHttp:{writable:()=>true,pin:()=>owner,assertOwner:peer=>{if(peer!==context.owner)throw Error('peer changed')}},
    document:{getElementById:id=>nodes.get(id),createElement:element},
    setUiText:(node,text,values={})=>{node.textContent=text.replace(/\{(\w+)\}/g,(_,key)=>values[key])},
    setUiAttribute:(node,name,value)=>node.setAttribute(name,value),
    renderLedOutputControl(){},render:actual=>{context.state=actual},report:error=>errors.push(error),
    api:async(path,options={})=>{calls.push({path,options});if(context.overrideApi)return context.overrideApi(path,options);
      return path==='/api/led/output'?{enabled:true,mode:'effects'}:{pixels:480,led_output:{enabled:true,mode:'effects'}}},
  });
  vm.runInContext(source,context);
  vm.runInContext(web.split('\n').find(line=>line.startsWith('function syncStreamControls()')),context);
  vm.runInContext(web.split('\n').find(line=>line.startsWith('async function startStream()')),context);
  const run=code=>vm.runInContext(code,context);run('renderPixelDiagnostic()');
  return {context,nodes,sent,calls,errors,run,ws};
}
function verifyDot(chunks,id){
  assert.equal(chunks.length,2);
  assert.deepEqual([...chunks[0].slice(0,10)],[80,75,65,82,0,0,0,0,240,0]);
  assert.deepEqual([...chunks[1].slice(0,10)],[80,75,65,82,0,0,240,0,240,1]);
  const rgb=Buffer.concat(chunks.map(v=>Buffer.from(v.slice(10))));assert.equal(rgb.length,1440);
  for(let pixel=1;pixel<=480;pixel++)assert.deepEqual([...rgb.slice((pixel-1)*3,pixel*3)],pixel===id?[255,255,255]:[0,0,0]);
}
for(const id of [1,6,7,475,480]){const f=fixture();verifyDot(f.run(`pixelDiagnosticPackets(${id})`),id)}
for(const id of [0,481,1.5,NaN])assert.throws(()=>fixture().context.pixelDiagnosticPackets(id));
{
  const f=fixture();f.nodes.get('physicalPixelIndex').value='480';await f.run('showPhysicalPixel()');
  assert.deepEqual(f.calls.map(v=>v.path),['/api/led/output','/api/status']);
  assert.equal(f.calls[0].options.body.toString(),'mode=effects');verifyDot(f.sent,480);
  assert.match(f.nodes.get('physicalPixelResult').textContent,/480 once.*unconfirmed/);
  assert(f.nodes.get('physicalPixelNext').disabled);await f.run('showPhysicalPixel(1)');assert.equal(f.sent.length,2);
  await f.run('showPhysicalPixel(-1)');verifyDot(f.sent.slice(2),479);assert.equal(f.nodes.get('physicalPixelIndex').value,'479');
}
for(const denial of ['off','streaming','starting','geometry','capability']){
  const f=fixture();if(denial==='off')f.context.state.led_output.enabled=false;
  if(denial==='streaming')f.context.streaming=true;if(denial==='starting')f.context.streamStarting=true;
  if(denial==='geometry')f.context.state.pixels=479;if(denial==='capability')f.context.features.delete('streaming');
  f.run('renderPixelDiagnostic()');assert(f.nodes.get('physicalPixelShow').disabled);
  await f.run('showPhysicalPixel()');assert.equal(f.calls.length,0);assert.equal(f.sent.length,0);
}
for(const failure of ['ack','readback','queued','partial']){
  const f=fixture();
  if(failure==='ack')f.context.overrideApi=async()=>({enabled:true,mode:'static'});
  if(failure==='readback')f.context.overrideApi=async path=>({pixels:480,led_output:{enabled:true,mode:'static'},enabled:true,mode:path==='/api/led/output'?'effects':'static'});
  if(failure==='queued')f.ws.bufferedAmount=1;
  if(failure==='partial')f.context.onSent=()=>{f.ws.readyState=3};
  await f.run('showPhysicalPixel()');assert.equal(f.sent.length,failure==='partial'?1:0);assert.equal(f.errors.length,1);
  assert.match(f.nodes.get('physicalPixelResult').textContent,/unconfirmed.*Not replayed/);
}
{
  const f=fixture();f.context.features.add('pixel-diagnostic');f.run('renderPixelDiagnostic()');
  assert(f.nodes.get('pixelDiagnostic').hidden,'Typed diagnostics replaces the legacy frame sender instead of duplicating it');
  assert.equal(f.calls.length,0);assert.equal(f.sent.length,0);
}
for(const cancellation of ['peer','stop']){
  const f=fixture();let finish;f.context.overrideApi=()=>new Promise(resolve=>finish=resolve);
  const pending=f.run('showPhysicalPixel()');assert(f.context.pixelDiagnosticPending);assert(f.context.streamStart.disabled);
  await f.run('showPhysicalPixel()');await f.run('startStream()');assert.equal(f.calls.length,1,'No overlapping diagnostic or stream startup');
  if(cancellation==='peer')f.context.owner={generation:2};else f.context.streamRequest++;
  finish({enabled:true,mode:'effects'});await pending;assert.equal(f.sent.length,0);assert(!f.context.pixelDiagnosticPending);
}
console.log('Actual Web physical-pixel diagnostic: exact480-pixel single frame, dot1/480 andcell80 boundaries, mode ACK/readback, Off/stream/peer gates, bounded buffering, partial-send cancellation and no replay passed; no optical proof.');
