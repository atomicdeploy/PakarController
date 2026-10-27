import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

const source=readFileSync(new URL('../web/index.html',import.meta.url),'utf8');
const translations=JSON.parse(readFileSync(new URL('../assets/web/controller-copy.json',import.meta.url),'utf8'));
const code=source.split('// BEGIN LED BACKEND RUNTIME')[1].split('// END LED BACKEND RUNTIME')[0];
const fixture=({active='gpio-static',requested=active,pending=false,error=0,available=['gpio-static','rmt-dma']}={})=>({
  board_id:'fixture-board',boot_id:'fixture-boot',command_clock_ms:1000,
  capabilities:{acknowledged_features:['leds','command-ack'],led_output:{runtime_switch:true,available_backends:available}},
  led_output:{backend_saved:true,backend_settings_error:'',enabled:true,mode:'effects',driver:{
    backend:active,requested_backend:requested,switch_pending:pending,switch_error:error,error_code:0,
    available_backends:available,ready:true,
  }},
});
const nodes=new Map();
function element(){
  const node={children:[],hidden:false,dataset:{},attrs:{},
    appendChild(child){child.parentElement=this;this.children.push(child);return child;},
    append(...children){for(const child of children)this.appendChild(child);},
    replaceChildren(...children){this.children=[];this.append(...children);},
    insertBefore(child,anchor){const at=this.children.indexOf(anchor);assert(at>=0);child.parentElement=this;this.children.splice(at,0,child);},
    setAttribute(key,value){this.attrs[key]=value;},
  };
  Object.defineProperty(node,'id',{get(){return this.nodeId;},set(value){this.nodeId=value;nodes.set(value,this);}});
  return node;
}
const anchor=element(),parent=element();parent.append(anchor);
const requests=[];
let writable=true;
const context=vm.createContext({setTimeout,clearTimeout,URLSearchParams,Uint8Array,AbortController,
  state:fixture(),ledOutputPending:false,
  document:{createElement:element,getElementById:id=>nodes.get(id)},
  currentPeer:()=>({has:()=>true}),peerHttp:{writable:()=>writable},
  uiTr:(key,values={})=>key.replace(/\{(\w+)\}/g,(match,name)=>String(values[name]??match)),
  setUiText:(node,key,values={})=>{node.textContent=key.replace(/\{(\w+)\}/g,(match,name)=>String(typeof values[name]==='function'?values[name]():values[name]??match));},
  requestLedOutput:async values=>requests.push(values),
});
vm.runInContext(code,context);
const run=expression=>vm.runInContext(expression,context);
context.anchor=anchor;
const render=value=>{context.state=value;run('renderLedBackendControl(anchor)');};

for(const bad of [null,{}, {led_output:{driver:{backend:'rmt-dma'}}},
  {...fixture(),capabilities:{acknowledged_features:['leds'],led_output:{backend:'gpio-static'}}}]){
  context.input=bad;assert.equal(run('ledBackendObservation(input)'),null);
}
render({});assert.equal(nodes.has('ledBackendControl'),false,'Unknown/old firmware does not acquire the selector');
render(fixture());
const selector=nodes.get('ledBackend'),note=nodes.get('ledBackendState');
assert.equal(selector.value,'gpio-static');assert.equal(note.textContent,'Active: GPIO · simple');
assert.deepEqual(selector.children.map(item=>item.value),['gpio-static','rmt-dma']);
selector.value='rmt-dma';await selector.onchange();
assert.equal(selector.value,'gpio-static','Selection remains the actual driver until live readback');
assert.equal(requests.length,1);assert.equal(requests[0].backend,'rmt-dma');
render(fixture({requested:'rmt-dma',pending:true}));assert(selector.disabled);
selector.value='rmt-dma';await selector.onchange();assert.equal(requests.length,1,'Pending changes do not dispatch twice');
assert.equal(note.textContent,'Switching to RMT + DMA…');
render(fixture({active:'rmt-dma'}));assert.equal(selector.value,'rmt-dma');
assert.equal(note.textContent,'Active: RMT + DMA');
render(fixture({requested:'rmt-dma',error:261}));assert.equal(note.className,'field-error');assert.match(note.textContent,/261.*GPIO/);
render(fixture({available:['gpio-static']}));assert.deepEqual(selector.children.map(item=>item.value),['gpio-static']);
writable=false;render(fixture());assert(selector.disabled);writable=true;
render({});assert(nodes.get('ledBackendControl').hidden);

const pending=fixture({requested:'rmt-dma',pending:true});context.state=pending;
assert.equal(run('ledBackendApplied(ledBackendObservation(state),"rmt-dma")'),false);
context.state=fixture({active:'rmt-dma'});assert.equal(run('ledBackendApplied(ledBackendObservation(state),"rmt-dma")'),true);
delete context.state.led_output.backend_saved;assert.equal(run('ledBackendApplied(ledBackendObservation(state),"rmt-dma")'),false);

// Actual async observer: events can finish without any polling request, while
// only explicitly enabled HTTP fallback can fetch a missing observation.
for(const result of ['event','http','failure','owner','timeout']){
  context.state=fixture({requested:'rmt-dma',pending:true});
  context.expected={...context.state};
  const subscribers=new Set();let reads=0;
  context.options={snapshot:()=>context.state,subscribe:listener=>{subscribers.add(listener);return()=>subscribers.delete(listener);},
    read:async()=>{reads++;return fixture({active:'rmt-dma'});},present:value=>{context.state=value;},
    allowed:()=>true,fallback:()=>result==='http',timeoutMs:90,readDelayMs:10,
  };
  const waiting=run('waitForLedBackend("rmt-dma",expected,options)');
  const observed=fixture(result==='failure'?{requested:'rmt-dma',error:261}:{active:'rmt-dma'});
  if(result==='owner')observed.boot_id='foreign';
  if(['event','failure','owner'].includes(result))queueMicrotask(()=>{for(const notify of subscribers)notify(observed);});
  if(result==='failure')await assert.rejects(waiting,/change failed.*261/);
  else if(result==='owner')await assert.rejects(waiting,/session changed/);
  else if(result==='timeout')await assert.rejects(waiting,/unconfirmed.*No command was replayed/);
  else assert.equal((await waiting).led_output.driver.backend,'rmt-dma');
  assert.equal(subscribers.size,0,'Every completion releases its observer');
  assert.equal(reads,result==='http'?1:0);
}

// Exercise shipped WS-first command transport + post success gating together.
vm.runInContext(source.slice(source.indexOf('function createCommandTransport('),source.indexOf('/* Reusable DOM controls')),context);
vm.runInContext(source.match(/async function post\(path,data=\{\}\)\{[^\n]+/)[0],context);
let transport,mutations=0,http=0,success=0,failure=0;
context.state=fixture();
context.commandHaptic=kind=>{if(kind==='command.success')success++;else failure++;};
context.report=error=>{throw error;};
context.render=value=>{context.state=value;run('for(const observer of ledBackendObservers)observer(state)');};
context.refresh=async()=>{throw Error('Backend operation must use its own verified readback');};
context.peerHttp.pin=()=>({});
context.api=async()=>{http++;throw Error('No HTTP request expected');};
transport=context.createCommandTransport({snapshot:()=>context.state,valid:()=>true,fallback:false,
  random:()=>new Uint8Array(16).fill(3),
  socket:()=>({readyState:1,bufferedAmount:0,send:encoded=>{
    mutations++;const packet=JSON.parse(encoded),status=fixture({requested:'rmt-dma',pending:true});
    queueMicrotask(()=>transport.receive({type:'command-result',accepted:true,request_id:packet.request_id,
      board_id:packet.board_id,boot_id:packet.boot_id,status}));
  }}),http:async()=>{http++;throw Error('No HTTP write expected');},
});
context.controlRequest=async(path,data)=>{const receipt=await transport.send(path,data);context.render(receipt.status);return receipt;};
const posting=run('post("/api/led/output",{backend:"rmt-dma"})');
await new Promise(resolve=>setTimeout(resolve,10));
assert.equal(success,0,'Accepted does not emit successful application feedback');
context.render(fixture({active:'rmt-dma'}));
assert.equal(await posting,true);assert.equal(success,1);assert.equal(failure,0);assert.equal(mutations,1);assert.equal(http,0);
for(const key of ['LED transmitter','GPIO · simple','Switching to {backend}…','Active: {backend}',
  'Transmitter not ready · {backend}','LED transmitter change failed · {error} · active: {backend}',
  'LED transmitter change was accepted, but active state is unconfirmed. No command was replayed.']){
  assert.match(translations[key],/[\u0600-\u06ff]/,`${key} has Persian application-owned copy`);
}
console.log('Web LED backend capability/options, actual/pending/error UI, saved-state admission, observer cleanup, bounded read-only fallback and one WS intent passed');
