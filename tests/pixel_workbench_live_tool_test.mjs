// Execute the actual commissioning script with bounded, in-memory adapters.
// No production helper is imported and no socket, board, or file is written.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {Script} from 'node:vm';

const toolUrl=new URL('../tools/check-pixel-workbench-live.mjs',import.meta.url);
let source=readFileSync(toolUrl,'utf8');
const imports=[
  "import {randomUUID} from 'node:crypto';",
  "import {setTimeout as delay} from 'node:timers/promises';",
  "import {deviceOrigin,deviceRequest} from './check-ethernet-live.mjs';",
  "import {boardPeer} from './peer-identity.mjs';",
];
assert.deepEqual(source.match(/^import .+;$/gm),imports,'Review any newly imported production adapter before running it in this fixture');
for(const line of imports)source=source.replace(line,'');
const script=new Script(`(async()=>{${source}\n})()`,{filename:toolUrl.pathname});
const board='esp32s3-001122334455',boot='a'.repeat(32),address='192.0.2.10';
const featureIds=['pixel-corrections-preview','pixel-correction-rule-toggle','pixel-diagnostic-effects','pixel-diagnostic-overlay'];
const clone=value=>JSON.parse(JSON.stringify(value));

async function runFixture(scenario='success',{run=true}={}){
  let clock=1000,uuidCounter=0,statusReads=0,correctionReads=0,delayCount=0,pendingReads=0;
  const calls=[],packets=[],outputs=[],requestIds=new Set();
  const scene={board_id:board,boot_id:boot,hostname:'fixture',
    capabilities:{acknowledged_features:featureIds},
    mode:'cell',effect:'none',winner:0,brightness:255,
    cells:Array(80).fill('white'),cell_states:Array(80).fill('idle'),red_lock:false,
    state_presentation:{idle:{rgb:0xffffff},lost:{rgb:0xff0000},won:{rgb:0x00ff00}},
    led_output:{enabled:true,mode:'effects',driver:{backend:'rmt-dma'},
      diagnostic:{revision:7,enabled:false,active:false,index:0,count:1,rgb:0,off:false,effect:'solid',composite:'clear',period_ms:500,duty_percent:50,min_brightness:0,max_brightness:255}},
    color_preview:{active:false},pixel_frame:{revision:10,pending:false}};
  const initialScene=clone(scene);
  const saved={revision:31,enabled:true,rules:[],max_rules:24,sources:Array.from({length:480},(_,i)=>i)};
  const initialSaved=clone(saved);
  let preview=null;
  const response=value=>({status:200,remote:address,body:Buffer.from(JSON.stringify(value))});
  function correctionState(){
    if(preview&&clock>=preview.expiry)preview=null;
    const active_sources=[...saved.sources];
    if(preview)for(const rule of preview.rules)if(rule.enabled!==false&&rule.kind==='black')active_sources[rule.pixel]=-1;
    return {...clone(saved),active_sources,preview:preview?{active:true,lease_id:preview.lease}: {active:false}};
  }
  function snapshot(){
    return {...clone(scene),command_clock_ms:clock,
      pixel_frame:{...scene.pixel_frame,pending:pendingReads>0}};
  }
  async function deviceRequest(origin,path,options={}){
    assert.equal(origin.origin,'http://fixture.local');
    assert(calls.length<100,'Bound every route/read loop without touching a real device');
    if(calls.length){
      assert.equal(options.expectedAddress,address);
      assert.equal(options.headers?.['X-Pakar-Board'],board);
      assert.equal(options.headers?.['X-Pakar-Boot'],boot);
    }
    calls.push({path,form:options.form?clone(options.form):null});
    if(!options.form&&path==='/api/status?pixels=1'){
      statusReads++;
      if(statusReads===2){
        if(scenario==='scene-state-changed')scene.cell_states[0]='lost';
        if(scenario==='scene-presentation-changed')scene.state_presentation.lost.rgb=0x0000ff;
        if(scenario==='scene-lock-changed')scene.red_lock=true;
        if(scenario==='missing-clock')clock=Number.MAX_SAFE_INTEGER;
      }
      const value=snapshot();
      if(pendingReads>0)pendingReads--;
      return response(value);
    }
    if(!options.form&&path==='/api/pixels/corrections'){
      correctionReads++;
      if(scenario==='competing-preview'&&correctionReads===3)preview={lease:'foreign-lease',expiry:clock+10000,rules:[]};
      return response(correctionState());
    }
    if(options.form&&path==='/api/pixels/corrections/preview'){
      assert.deepEqual(Object.keys(options.form),['configuration']);
      const config=JSON.parse(options.form.configuration);
      assert.equal(config.revision,saved.revision);
      if(config.clear){
        assert.equal(config.lease_id,preview?.lease,'Never clear another editor lease');
        preview=null;
      }else{
        assert.equal(config.enabled,true);
        assert(Number.isInteger(config.ttl_ms)&&config.ttl_ms>=250&&config.ttl_ms<=10000);
        assert.deepEqual(config.rules.slice(0,-1),saved.rules);
        const rule=config.rules.at(-1);
        assert.deepEqual(rule,{kind:'black',pixel:0,offset:0,enabled:!!preview});
        if(preview)assert.equal(config.lease_id,preview.lease);
        preview={lease:preview?.lease??'owned-lease',rules:clone(config.rules),expiry:clock+config.ttl_ms};
      }
      return response(correctionState());
    }
    if(options.form&&path==='/api/command'){
      assert.deepEqual(Object.keys(options.form),['packet']);
      const packet=JSON.parse(options.form.packet);
      packets.push(packet);
      assert.match(packet.request_id,/^[a-f0-9]{32}$/,'Real CommandReceipts rejects hyphenated UUIDs');
      assert(!requestIds.has(packet.request_id),'Each distinct mutation needs its own correlation id');
      requestIds.add(packet.request_id);
      assert.equal(packet.board_id,board);assert.equal(packet.boot_id,boot);
      assert.equal(packet.type,'command');assert.equal(packet.command,'pixel-diagnostic');
      assert.equal(packet.expires_at_ms,clock+8000);
      assert.deepEqual(Object.fromEntries(['index','count','rgb','off','effect','composite','period_ms','duty_percent','min_brightness','max_brightness'].map(key=>[key,packet[key]])),
        {index:0,count:1,rgb:0x00ff00,off:false,effect:'breathe',composite:'overlay',period_ms:500,duty_percent:50,min_brightness:20,max_brightness:255});
      const receipt={type:'command-result',request_id:packet.request_id,board_id:board,boot_id:boot,accepted:true};
      if(scenario==='rejected')return response({...receipt,accepted:false,error:'Board is busy'});
      const {revision}=scene.led_output.diagnostic;
      const {request_id,board_id,boot_id,expires_at_ms,type,command,...fields}=packet;
      scene.led_output.diagnostic={...fields,active:fields.enabled,revision:revision+1};
      scene.pixel_frame.revision++;
      if(scenario==='pending-frame')pendingReads=2;
      if(scenario==='lost-receipt')throw Error('Fixture transport closed after admission');
      if(scenario==='unknown-receipt')delete receipt.accepted;
      if(scenario==='foreign-rejection')Object.assign(receipt,{request_id:'f'.repeat(32),accepted:false,error:'Not ours'});
      if(scenario==='foreign-status')receipt.status={...snapshot(),boot_id:'b'.repeat(32)};
      else receipt.status=snapshot();
      return response(receipt);
    }
    assert.fail(`Unexpected route/mutation ${path}; no firmware, settings, or saved correction writes are allowed`);
  }
  const context={Buffer,URL,
    process:{argv:['node','fixture','--origin','http://fixture.local','--board',board,...(run?['--run']:[])],stdout:{write:chunk=>outputs.push(chunk)}},
    randomUUID:()=>`00000000-0000-4000-8000-${String(++uuidCounter).padStart(12,'0')}`,
    Date:{now:()=>clock},
    delay:async ms=>{
      assert(++delayCount<=35,'Read-only settling must have a finite budget');
      assert([100,500,850].includes(ms));clock+=ms;
      if(scenario==='superseded-diagnostic'&&ms===500)scene.led_output.diagnostic.revision++;
    },
    deviceOrigin:value=>new URL(value),deviceRequest,
    boardPeer:(value,remote)=>({board:value.board_id,boot:value.boot_id,address:remote}),
  };
  let error;
  try{await script.runInNewContext(context,{timeout:1000,contextCodeGeneration:{strings:false,wasm:false}});}catch(caught){error=caught;}
  assert.equal(outputs.length,1,'Evidence is emitted even if both the request and guarded cleanup fail');
  const evidence=JSON.parse(outputs[0]);
  assert.equal(evidence.flash_writes,false);assert.equal(evidence.optical_verified,false);
  assert.equal(evidence.board,board);assert.equal(evidence.boot,boot);
  assert.deepEqual(saved,initialSaved,'Transient correction checks never modify the durable map');
  assert(calls.filter(call=>call.form).every(call=>['/api/pixels/corrections/preview','/api/command'].includes(call.path)));
  return {error,evidence,calls,packets,scene,initialScene,saved,preview,delayCount};
}

for(const scenario of ['success','pending-frame']){
  const result=await runFixture(scenario);
  assert.equal(result.error,undefined);
  for(const field of ['disabled_row_verified','enabled_row_verified','preview_expiry_verified','diagnostic_overlay_verified','scene_restored','saved_corrections_preserved'])assert.equal(result.evidence[field],true,field);
  assert.deepEqual(result.packets.map(packet=>packet.enabled),[true,false]);
  assert.equal(result.calls.filter(call=>call.path==='/api/pixels/corrections/preview').length,2,'Successful expiry requires no clear or save command');
  assert.equal(result.scene.led_output.diagnostic.active,false);
  for(const field of ['cells','cell_states','state_presentation','red_lock','mode','effect','winner'])assert.deepEqual(result.scene[field],result.initialScene[field]);
  if(scenario==='pending-frame')assert(result.delayCount>2,'Publication is settled by reads, never by replaying a command');
}
{
  const result=await runFixture('success',{run:false});
  assert.equal(result.error,undefined);assert.equal(result.calls.filter(call=>call.form).length,0);
  assert.equal(result.evidence.scene_restored,undefined,'Read-only preflight is not a commissioning result');
}
{
  const result=await runFixture('rejected');
  assert.match(result.error?.message,/Diagnostic rejected: Board is busy; no replay/);
  assert.equal(result.packets.length,1);
  assert.equal(result.calls.at(-1).path,'/api/command','A correlated rejection disarms cleanup instead of issuing a new mutation');
  assert.equal(result.scene.led_output.diagnostic.active,false);
  assert.match(result.evidence.error,/Board is busy/);
}
for(const [scenario,reason] of [['unknown-receipt',/acceptance unknown/],['lost-receipt',/transport closed/],['foreign-rejection',/Uncorrelated/],['foreign-status',/Same-peer diagnostic receipt unavailable/]]){
  const result=await runFixture(scenario);
  assert.match(result.error?.message,/ownership uncertain; no blind cleanup command/);
  assert.match(result.evidence.error,reason,'Keep the original failure in the evidence even when cleanup cannot establish ownership');
  assert.equal(result.packets.length,1,'Unknown outcomes must not be retried or cleared');
  assert.equal(result.scene.led_output.diagnostic.active,true);
  assert.equal(result.evidence.scene_restored,undefined);
}
for(const scenario of ['scene-state-changed','scene-presentation-changed','scene-lock-changed','competing-preview']){
  const result=await runFixture(scenario);
  assert.match(result.error?.message,/Scene or editor changed before diagnostic; no mutation/);
  assert.equal(result.packets.length,0,'An authoritative change after preview expiry prevents diagnostic admission');
  assert.equal(result.calls.filter(call=>call.form).length,2,'Only the preceding, expiring preview checks ran');
  assert.equal(result.evidence.diagnostic_overlay_verified,undefined);
  if(scenario==='competing-preview')assert.equal(result.preview.lease,'foreign-lease','Another editor is never cleared');
}
{
  const result=await runFixture('missing-clock');
  assert.match(result.error?.message,/Fresh command clock unavailable; no mutation/);
  assert.equal(result.packets.length,0);
}
{
  const result=await runFixture('superseded-diagnostic');
  assert.match(result.error?.message,/ownership uncertain; no blind cleanup command/);
  assert.equal(result.packets.length,1,'Do not clear another instance\'s newer diagnostic');
  assert.equal(result.scene.led_output.diagnostic.active,true);
  assert.equal(result.evidence.scene_restored,undefined);
}
console.log('Pixel workbench tool: 14 bounded in-memory cases passed; no device contact or physical acceptance.');
