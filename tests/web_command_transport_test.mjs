import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const context=vm.createContext({setTimeout,clearTimeout,URLSearchParams,Uint8Array,AbortController});
vm.runInContext(readFileSync(new URL('../assets/web/command-transport.js',import.meta.url),'utf8'),context);
const state={board_id:'board',boot_id:'boot',command_clock_ms:100};
const reply=p=>({type:'command-result',request_id:p.request_id,board_id:p.board_id,boot_id:p.boot_id,accepted:true,status:{...state},count:2});
let posts=0,sends=0,transport;
transport=context.createCommandTransport({snapshot:()=>state,valid:()=>true,random:()=>new Uint8Array(16).fill(1),
 socket:()=>({readyState:1,bufferedAmount:0,send:packet=>{sends++;queueMicrotask(()=>transport.receive(reply(JSON.parse(packet))))}}),
 http:()=>{posts++;throw Error('Unexpected HTTP');}});
assert.equal((await transport.send('/api/cells',{ids:'1,2',state:'lost'})).count,2);assert.equal(posts,0);assert.equal(sends,1);
let packet;
transport=context.createCommandTransport({snapshot:()=>state,valid:()=>true,random:()=>new Uint8Array(16).fill(2),
 socket:()=>({readyState:1,bufferedAmount:0,send:p=>{packet=p}}),
 http:async(path,options)=>{assert.equal(path,'/api/command');assert.equal(options.body.get('packet'),packet);return reply(JSON.parse(packet));}});
await transport.send('/api/effect',{value:'rainbow'});
assert.equal(await transport.send('/api/audio',{cue:'welcome'}),null,'Unenveloped commands retain HTTP and are never retried over legacy WS');
assert.equal(transport.receive({type:'command-result',request_id:'foreign'}),false);
// Freshness authorizes dispatch, not rejection of a delayed matching receipt.
let fresh=true;
transport=context.createCommandTransport({snapshot:()=>state,valid:()=>fresh,random:()=>new Uint8Array(16).fill(3),
 socket:()=>null,http:async(_path,options)=>{fresh=false;return reply(JSON.parse(options.body.get('packet')))}});
assert.equal((await transport.send('/api/cells',{ids:'1,2',state:'lost'})).accepted,true);
// A different boot must never accept the pending old-board result.
fresh=true;
transport=context.createCommandTransport({snapshot:()=>state,valid:()=>fresh,random:()=>new Uint8Array(16).fill(4),
 socket:()=>null,http:async(_path,options)=>{const result=reply(JSON.parse(options.body.get('packet')));state.boot_id='new-boot';return result}});
await assert.rejects(transport.send('/api/cells',{ids:'1,2',state:'lost'}),/Uncorrelated/);

// Actual living command shapes from handleCloudCommand and mobile command_wire.
let captured;
transport=context.createCommandTransport({snapshot:()=>state,valid:()=>true,random:()=>new Uint8Array(16).fill(5),
 socket:()=>null,http:async(_path,options)=>{captured=JSON.parse(options.body.get('packet'));return reply(captured)}});
await transport.send('/api/led/presentation',{settings:JSON.stringify({enabled:true,states:{lost:{rgb:0xaa0000,motion:2}}})});
assert.equal(captured.command,'state-presentation');assert.equal(captured.settings.enabled,true);assert.equal(captured.settings.states.lost.rgb,0xaa0000);
await transport.send('/api/led/preview',{ids:'1,2',rgb:'255',duration_ms:'1000'});
assert.equal(captured.command,'color-preview');assert.equal(captured.rgb,255);assert.equal(captured.duration_ms,1000);
await transport.send('/api/led/preview',{cancel:'true'});assert.equal(captured.cancel,true);
await transport.send('/api/cells/assignments',{assignments:JSON.stringify([{cell:1,state:'won'},{cell:80,state:'lost'}])});
assert.equal(captured.command,'cell-assignments');assert(Array.isArray(captured.assignments));assert.equal(captured.assignments[1].cell,80);
await transport.send('/api/game/settings',{enabled_mask:'15',idle_rgb:'16777215',win_rgb:65280,lose_rgb:'16711680',auto_reset_ms:'0',extended_effects:'true',chase_neighbors:'false',chase_invert:false,chase_period_ms:'900',winner_cue:'winner',presentation:JSON.stringify({flashes:3,groups:[{id:2,lose_motion:3}]})});
assert.equal(captured.command,'game-settings');assert.equal(captured.enabled_mask,15);assert.equal(captured.extended_effects,true);assert.equal(captured.chase_neighbors,false);assert.equal(captured.presentation.groups[0].lose_motion,3);
await transport.send('/api/led/diagnostic',{index:'12',count:'6',rgb:'65280',off:'false',effect:'breathe',composite:'overlay',period_ms:'900',duty_percent:'35',min_brightness:'10',max_brightness:'240',enabled:'true'});
assert.equal(captured.command,'pixel-diagnostic');assert.equal(captured.period_ms,900);assert.equal(captured.duty_percent,35);assert.equal(captured.min_brightness,10);assert.equal(captured.max_brightness,240);assert.equal(captured.enabled,true);assert.equal(captured.off,false);assert.equal(captured.composite,'overlay');
for(const[path,values]of [
 ['/api/led/diagnostic',{period_ms:'99'}],['/api/led/diagnostic',{duty_percent:'100'}],['/api/led/diagnostic',{max_brightness:'256'}],['/api/led/diagnostic',{min_brightness:200,max_brightness:10}],['/api/led/diagnostic',{effect:'fire'}],['/api/led/diagnostic',{composite:'unknown'}],
 ['/api/led/presentation',{settings:'[]'}],['/api/led/presentation',{settings:'null'}],
 ['/api/led/preview',{rgb:'bad',duration_ms:'1000'}],['/api/led/preview',{rgb:'0',duration_ms:'10001'}],
 ['/api/game/settings',{enabled_mask:'0'}],['/api/game/settings',{chase_period_ms:'399'}],
 ['/api/game/settings',{extended_effects:'false-ish'}],['/api/game/settings',{presentation:'[]'}],
 ['/api/cells/assignments',{assignments:'[{"cell":1,"state":"won"},{"cell":1,"state":"off"}]'}],
 ['/api/cells/assignments',{assignments:'[{"cell":81,"state":"off"}]'}],
 ['/api/cells/assignments',{assignments:'[{"cell":1,"state":"invalid"}]'}],
])await assert.rejects(transport.send(path,values),/Invalid|Provide/);

// send() can throw after browser buffering changes. Recover only the exact same
// immutable intent; reserved routing fields cannot be overwritten by form data.
let original,failedSends=0,recoveryPosts=0;
transport=context.createCommandTransport({snapshot:()=>state,valid:()=>true,random:()=>new Uint8Array(16).fill(6),
 socket:()=>({readyState:1,bufferedAmount:0,send:encoded=>{original=encoded;failedSends++;throw Error('socket closed')}}),
 http:async(_path,options)=>{recoveryPosts++;assert.equal(options.body.get('packet'),original);return reply(JSON.parse(original))}});
await transport.send('/api/brightness',{value:'128',board_id:'foreign',boot_id:'foreign',request_id:'foreign',command:'reset'});
assert.equal(failedSends,1);assert.equal(recoveryPosts,1);assert.equal(JSON.parse(original).command,'brightness');assert.equal(JSON.parse(original).board_id,state.board_id);
assert.equal(transport.receive(reply(JSON.parse(original))),false,'Completed request releases the pending receipt');

// A selected session changing during a throwing send cannot fall through to HTTP.
transport=context.createCommandTransport({snapshot:()=>state,valid:()=>true,random:()=>new Uint8Array(16).fill(7),
 socket:()=>({readyState:1,bufferedAmount:0,send:()=>{state.boot_id='another-boot';throw Error('closed')}}),
 http:()=>{throw Error('Must not replay on changed peer')}});
await assert.rejects(transport.send('/api/effect',{value:'rainbow'}),/Board connection changed/);

// Preference is evaluated for each new intent, never as a change of intent ID.
let permitFallback=false,fallbackCalls=0;
transport=context.createCommandTransport({snapshot:()=>state,valid:()=>true,fallback:()=>permitFallback,random:()=>new Uint8Array(16).fill(8),
 socket:()=>null,http:async(_path,options)=>{fallbackCalls++;return reply(JSON.parse(options.body.get('packet')))}});
await assert.rejects(transport.send('/api/effect',{value:'rainbow'}),/fallback is disabled/);assert.equal(fallbackCalls,0);
permitFallback=true;await transport.send('/api/effect',{value:'rainbow'});assert.equal(fallbackCalls,1);

// A late WebSocket receipt wins recovery and cancels the now-redundant HTTP read.
let recoverySignal,latePacket;
transport=context.createCommandTransport({snapshot:()=>state,valid:()=>true,random:()=>new Uint8Array(16).fill(9),
 socket:()=>({readyState:1,bufferedAmount:0,send:encoded=>{latePacket=JSON.parse(encoded)}}),
 http:(_path,options)=>{recoverySignal=options.signal;queueMicrotask(()=>transport.receive(reply(latePacket)));return new Promise((resolve,reject)=>options.signal.addEventListener('abort',()=>reject(Error('aborted')),{once:true}))}});
await transport.send('/api/effect',{value:'rainbow'});assert.equal(recoverySignal.aborted,true);
assert.equal(transport.receive(reply(latePacket)),false);

console.log('Web WS fast path, living typed commands, exact-ID throw/timeout recovery, configurable fallback, cancellation and peer/receipt isolation passed');
