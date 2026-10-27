// Host-owned demonstrations. Nothing runs on import or without explicit --run.
// Uses the repository's bounded hostname-first HTTP/peer policy, not a second
// device protocol. Browser apps should use the shared WS-first dispatcher.
import {randomBytes} from 'node:crypto';
import {pathToFileURL} from 'node:url';
import {setTimeout as delay} from 'node:timers/promises';
import {deviceOrigin,deviceRequest} from '../tools/check-ethernet-live.mjs';
import {boardPeer} from '../tools/peer-identity.mjs';

// Same logical table geometry as web/modules/table-layout.js:20 staggered
// columns by4 rows. Only IDs are sent; firmware owns map, variable spans and
// corrections. No guessed physical pixel multiplication exists here.
export function cellAt(x,y){return 1+Math.floor(x/2)+(x%2)*10+y*20;}
export function snakeFrame(tick){
  const path=Array.from({length:80},(_,i)=>{const row=Math.floor(i/20);return cellAt(row%2?19-i%20:i%20,row)});
  return Array.from({length:5},(_,tail)=>({id:path[(tick-tail+80)%80],rgb:tail?0x008000:0x00ff00,effect:'solid'}));
}
export const memoryPattern=Object.freeze([1,14,27,43,68]);
export function memoryFrame(hidden=false){return hidden?[]:memoryPattern.map(id=>({id,rgb:0x00ff00,effect:'solid'}));}

export async function runMiniApp({origin='http://ledcontroller.local',mode='snake',run=false,
  steps=40,request=deviceRequest,wait=delay,now=()=>performance.now(),log=console.log,signal,environment=process.env}={}){
  if(!run)throw Error('Explicit --run required; this example temporarily changes visible LEDs.');
  if(!['snake','memory'].includes(mode)||!Number.isInteger(steps)||steps<1||steps>80)throw Error('Use snake or memory, steps1..80.');
  const url=deviceOrigin(origin);let peer=null,lease=null,uncertain=false,lastCommandMs=0;
  const user=environment.PAKAR_USERNAME,password=environment.PAKAR_PASSWORD;
  if((user===undefined)!==(password===undefined))throw Error('Set both PAKAR_USERNAME and PAKAR_PASSWORD, or neither.');
  const authorization=user===undefined?{}:{Authorization:'Basic '+Buffer.from(user+':'+password).toString('base64')};
  const read=async(path,options={})=>{
    const response=await request(url,path,{...options,expectedAddress:peer?.address,
      headers:{...authorization,...(peer?{'X-Pakar-Board':peer.board,'X-Pakar-Boot':peer.boot}:{}),...options.headers}});
    if(response.status!==200)throw Error(`HTTP${response.status}; nothing replayed.`);
    const value=JSON.parse(response.body.toString('utf8'));
    if(peer&&(value.board_id!==peer.board||value.boot_id!==peer.boot))throw Error('Peer changed; nothing replayed.');
    return {value,remote:response.remote};
  };
  const first=await read('/api/status');peer=boardPeer(first.value,first.remote);
  if(!first.value.capabilities?.acknowledged_features?.includes('host-presentation'))throw Error('Board does not advertise host-presentation.');
  const groups=(await read('/api/pixels/groups')).value;
  if(groups.group_count!==80||!Number.isInteger(groups.revision))throw Error('These examples require an80-cell table.');
  const initial=(await read('/api/presentation')).value;
  if(initial.active||initial.temporary||initial.persistent)throw Error('Existing presentation present; no ownership or saved content replaced.');
  const owner=`example-${mode}-${randomBytes(4).toString('hex')}`;
  const send=async(fields)=>{
    const commandStarted=now();
    if(signal?.aborted&&fields.action!=='clear')throw Error('Stopped by user.');
    const status=(await read('/api/status')).value;boardPeer(status,peer.address,peer);
    const current=(await read('/api/presentation')).value;
    if(!Number.isSafeInteger(current.revision)||!Number.isSafeInteger(status.command_clock_ms))throw Error('No authoritative revision/clock.');
    if(lease?(current.lease_id!==lease.id||current.owner!==owner||current.revision!==lease.revision):current.active)throw Error('Presentation ownership changed; no mutation sent.');
    const packet={type:'command',command:'presentation',board_id:peer.board,boot_id:peer.boot,
      request_id:randomBytes(16).toString('hex'),expires_at_ms:status.command_clock_ms+8000,
      revision:current.revision,group_layout_revision:groups.revision,owner,
      lifetime:'temporary',ttl_ms:3000,space:'cells',composite:'replace',replace:true,
      ...(lease?{lease_id:lease.id}:{}),...fields};
    uncertain=true;
    const receipt=(await read('/api/command',{form:{packet:JSON.stringify(packet)}})).value;
    if(receipt.request_id!==packet.request_id||receipt.accepted!==true||!receipt.presentation||
      !Number.isInteger(receipt.presentation.revision)||receipt.presentation.revision<=current.revision||
      (fields.action!=='clear'&&(!Number.isInteger(receipt.presentation.lease_id)||receipt.presentation.lease_id<1)))throw Error('Command outcome unknown/rejected; no replay. TTL restores the scene.');
    uncertain=false;lease=fields.action==='clear'?null:{id:receipt.presentation.lease_id,revision:receipt.presentation.revision};
    lastCommandMs=Math.max(0,now()-commandStarted);
    return receipt;
  };
  const sleep=ms=>wait(ms,undefined,signal?{signal}:{});
  const holdFor=async milliseconds=>{
    const deadline=now()+milliseconds;
    while(now()<deadline){
      const remaining=deadline-now();
      // Do not begin a renewal whose observed round-trip budget would cross
      // the phase deadline; the existing3s lease covers this final interval.
      if(remaining<=1000+lastCommandMs){await sleep(remaining);break;}
      await sleep(1000);
      if(now()>=deadline)break;
      // Monotonic deadline includes renewal/readback network time. It does not
      // turn ten one-second waits plus ten round trips into a longer interval.
      await send({entries:[],replace:false});
    }
  };
  try{
    if(mode==='snake'){
      log('Host Snake trail: temporary green five-cell trail. Ctrl+C stops; no saved state writes.');
      for(let tick=0;tick<steps;++tick){await send({entries:snakeFrame(tick)});await sleep(250);}
    }else{
      log('Memory: remember the five green logical cells. Ten-second host deadline; final command latency remains observable.');
      await send({entries:memoryFrame()});
      // Each renewal retains the board-local phase and contents, not a redraw.
      await holdFor(10000);
      await send({entries:memoryFrame(true)});
      log('Pattern hidden. Recall the five IDs; the answer prints after five seconds.');
      await holdFor(5000);
      log('Answer: '+memoryPattern.join(', '));
    }
  }finally{
    // Do not clear an unknown mutation or somebody else's newer state. A lost
    // receipt/connection expires naturally in<=3s; never retry blind cleanup.
    if(lease&&!uncertain)await send({action:'clear'});
  }
  return {mode,board_id:peer.board,temporary_only:true,restored:true,physical_verified:false};
}

if(process.argv[1]&&import.meta.url===pathToFileURL(process.argv[1]).href){
  const controller=new AbortController();process.once('SIGINT',()=>controller.abort());
  try{
    const options={signal:controller.signal};const args=process.argv.slice(2);
    for(let i=0;i<args.length;++i){const flag=args[i];if(flag==='--run'){options.run=true;continue;}
      const key={'--origin':'origin','--mode':'mode','--steps':'steps'}[flag];
      if(!key||!args[i+1])throw Error('Use --run --origin http://ledcontroller.local --mode snake|memory [--steps40].');
      options[key]=key==='steps'?Number(args[++i]):args[++i];
    }
    console.log(JSON.stringify(await runMiniApp(options)));
  }catch(error){console.error(error.message);process.exitCode=1;}
}
