// Scoped commissioning check. Reuses the existing hostname/TCP and identity
// fences; never retries a mutation, saves corrections, or changes AVR state.
import {randomUUID} from 'node:crypto';
import {setTimeout as delay} from 'node:timers/promises';
import {deviceOrigin,deviceRequest} from './check-ethernet-live.mjs';
import {boardPeer} from './peer-identity.mjs';

const args=process.argv.slice(2), origin=deviceOrigin(args[args.indexOf('--origin')+1]);
const expected=args[args.indexOf('--board')+1];
if(!args.includes('--origin')||!args.includes('--board')||!/^esp32s3-[a-f0-9]+$/.test(expected))throw Error('Require --origin HOSTNAME --board EXACT_ID [--run]');
let owner,lease=null,diagnosticRevision=null,initial,base,diagnosticTouched=false;
const evidence={origin:origin.origin,board:expected,optical_verified:false,flash_writes:false};
async function json(path,form){
  const response=await deviceRequest(origin,path,{form,expectedAddress:owner?.address,
    headers:owner?{'X-Pakar-Board':owner.board,'X-Pakar-Boot':owner.boot}:{}});
  if(response.status!==200)throw Error(`${path}: HTTP ${response.status}; no retry`);
  const value=JSON.parse(response.body.toString('utf8'));
  return {value,remote:response.remote};
}
async function status(){
  const result=await json('/api/status?pixels=1');
  const peer=boardPeer(result.value,result.remote);
  if(peer.board!==expected||(owner&&(peer.board!==owner.board||peer.boot!==owner.boot)))throw Error('Peer changed; no replay');
  owner=peer;return result.value;
}
async function command(fields,{ownedRevision=null}={}){
  const current=await status();
  if(!Number.isSafeInteger(current.command_clock_ms)||current.command_clock_ms<0||current.command_clock_ms>Number.MAX_SAFE_INTEGER-8000)throw Error('Fresh command clock unavailable; no mutation');
  if(ownedRevision===null){
    const corrections=(await json('/api/pixels/corrections')).value;
    if(!sameScene(current)||current.led_output?.diagnostic?.active||current.color_preview?.active||corrections.preview?.active||!sameSaved(corrections))throw Error('Scene or editor changed before diagnostic; no mutation');
  }
  if(ownedRevision!==null&&current.led_output?.diagnostic?.revision!==ownedRevision)throw Error('Diagnostic changed before cleanup; no mutation');
  const packet={type:'command',command:'pixel-diagnostic',request_id:randomUUID().replaceAll('-',''),board_id:owner.board,boot_id:owner.boot,
    expires_at_ms:current.command_clock_ms+8000,...fields};
  if(ownedRevision===null)diagnosticTouched=true;
  const reply=(await json('/api/command',{packet:JSON.stringify(packet)})).value;
  if(reply.type!=='command-result'||reply.request_id!==packet.request_id||reply.board_id!==owner.board||reply.boot_id!==owner.boot)throw Error('Uncorrelated diagnostic receipt; no replay');
  if(reply.accepted===false){
    if(ownedRevision===null)diagnosticTouched=false;
    throw Error('Diagnostic rejected: '+String(reply.error)+'; no replay');
  }
  if(reply.accepted!==true)throw Error('Diagnostic acceptance unknown; no replay');
  if(reply.status?.board_id!==owner.board||reply.status?.boot_id!==owner.boot)throw Error('Same-peer diagnostic receipt unavailable; no replay');
  const revision=reply.status?.led_output?.diagnostic?.revision;
  if(!Number.isSafeInteger(revision)||revision>0xffffffff||!Number.isSafeInteger(current.led_output?.diagnostic?.revision)||revision<=current.led_output.diagnostic.revision)throw Error('New diagnostic revision unavailable; no replay');
  return reply;
}
function sameSaved(value){
  return value.revision===base.revision&&value.enabled===base.enabled&&JSON.stringify(value.rules)===JSON.stringify(base.rules);
}
function sameScene(value){
  return value.mode===initial.mode&&value.effect===initial.effect&&value.winner===initial.winner&&value.brightness===initial.brightness&&JSON.stringify(value.cells)===JSON.stringify(initial.cells)&&
    JSON.stringify(value.cell_states)===JSON.stringify(initial.cell_states)&&
    JSON.stringify(value.state_presentation)===JSON.stringify(initial.state_presentation)&&value.red_lock===initial.red_lock&&
    value.led_output.enabled===initial.led_output.enabled&&value.led_output.mode===initial.led_output.mode&&
    value.led_output.driver.backend===initial.led_output.driver.backend;
}
async function settledDiagnostic(fields,revision){
  const deadline=Date.now()+3000;
  for(;;){
    const current=await status(),diag=current.led_output?.diagnostic;
    if(!diag||diag.revision!==revision)throw Error('Diagnostic superseded; no replay');
    const exact=diag.active===fields.enabled&&diag.enabled===fields.enabled&&
      ['index','count','rgb','off','effect','composite','period_ms','duty_percent','min_brightness','max_brightness'].every(key=>diag[key]===fields[key]);
    if(!exact||!sameScene(current))throw Error('Diagnostic observation differs from request; no replay');
    if(Number.isSafeInteger(current.pixel_frame?.revision)&&current.pixel_frame.revision>0&&current.pixel_frame.pending===false)return current;
    if(Date.now()>=deadline)throw Error('Diagnostic publication still pending; read-only deadline reached');
    await delay(100);
  }
}
const diagnosticFields={index:0,count:1,rgb:0x00ff00,off:false,effect:'breathe',composite:'overlay',period_ms:500,duty_percent:50,min_brightness:20,max_brightness:255,enabled:true};
try{
  initial=await status();base=(await json('/api/pixels/corrections')).value;
  const features=initial.capabilities.acknowledged_features;
  for(const feature of ['pixel-corrections-preview','pixel-correction-rule-toggle','pixel-diagnostic-effects','pixel-diagnostic-overlay']){
    if(!features.includes(feature))throw Error('Missing capability: '+feature);
  }
  if(base.preview?.active||initial.color_preview?.active||initial.led_output.diagnostic?.active||!initial.led_output.enabled)throw Error('Existing diagnostic/preview or disabled output; no mutation');
  evidence.boot=owner.boot;evidence.saved_revision=base.revision;
  if(args.includes('--run')){
    if(base.rules.length>=base.max_rules||base.enabled!==true)throw Error('No safe temporary rule slot; saved corrections untouched');
    const testPixel=base.sources.findIndex(source=>source>=0);
    if(testPixel<0)throw Error('No unmasked source slot available; no mutation');
    const candidate={revision:base.revision,enabled:base.enabled,rules:[...base.rules,{kind:'black',pixel:testPixel,offset:0,enabled:false}],ttl_ms:3000};
    let preview=(await json('/api/pixels/corrections/preview',{configuration:JSON.stringify(candidate)})).value;
    lease=preview.preview?.lease_id;
    if(!lease||!sameSaved(preview)||JSON.stringify(preview.active_sources)!==JSON.stringify(base.sources))throw Error('Disabled temporary row changed output or saved values');
    evidence.disabled_row_verified=true;
    candidate.rules.at(-1).enabled=true;candidate.lease_id=lease;candidate.ttl_ms=700;
    preview=(await json('/api/pixels/corrections/preview',{configuration:JSON.stringify(candidate)})).value;
    if(preview.preview?.active!==true||preview.preview.lease_id!==lease||!sameSaved(preview)||preview.active_sources[testPixel]!==-1)throw Error('Enabled temporary row did not black its slot');
    evidence.enabled_row_verified=true;
    await delay(850);
    preview=(await json('/api/pixels/corrections')).value;
    if(preview.preview?.active||!sameSaved(preview)||JSON.stringify(preview.active_sources)!==JSON.stringify(base.sources))throw Error('Preview did not expire to saved mapping');
    lease=null;evidence.preview_expiry_verified=true;
    const receipt=await command(diagnosticFields);
    diagnosticRevision=receipt.status?.led_output?.diagnostic?.revision;
    await settledDiagnostic(diagnosticFields,diagnosticRevision);
    evidence.diagnostic_overlay_verified=true;
    await delay(500);
  }
}catch(error){evidence.error=error.message;throw error;}finally{
 try{
  // Cleanup cannot act on a changed peer or erase another editor's newer test.
  if(lease){
    await status();const value=(await json('/api/pixels/corrections')).value;
    if(value.preview?.active&&value.preview.lease_id===lease&&sameSaved(value))await json('/api/pixels/corrections/preview',{configuration:JSON.stringify({clear:true,lease_id:lease,revision:base.revision})});
  }
  if(diagnosticTouched){
    const current=await status();
    if(diagnosticRevision&&current.led_output.diagnostic.revision===diagnosticRevision){
      const fields={...diagnosticFields,enabled:false};
      const receipt=await command(fields,{ownedRevision:diagnosticRevision});
      const after=await settledDiagnostic(fields,receipt.status.led_output.diagnostic.revision);
      evidence.scene_restored=sameScene(after)&&after.led_output.diagnostic.active===false;
      if(!evidence.scene_restored)throw Error('Scene restoration not verified');
      const corrections=(await json('/api/pixels/corrections')).value;
      evidence.saved_corrections_preserved=sameSaved(corrections)&&!corrections.preview?.active&&JSON.stringify(corrections.sources)===JSON.stringify(base.sources);
      if(!evidence.saved_corrections_preserved)throw Error('Saved corrections changed during diagnostics');
    }else throw Error('Diagnostic ownership uncertain; no blind cleanup command');
  }
 }finally{process.stdout.write(JSON.stringify(evidence,null,2)+'\n');}
}
