// Read-only acceptance probe: the request POST schedules only a remote GET.
// Reuses the canonical hostname/identity/HTTP owners; never retries a POST.
import http from 'node:http';
import https from 'node:https';
import {randomBytes} from 'node:crypto';
import {pathToFileURL} from 'node:url';
import {deviceOrigin,deviceRequest} from './check-ethernet-live.mjs';
import {boardPeer,toolHeaders} from './peer-identity.mjs';
import {checkWebSocket} from './update-firmware.mjs';
import {setTimeout as delay} from 'node:timers/promises';

export function relayReadEnvelope({id,hostname,board,boot='',path,address}) {
  deviceOrigin('http://'+hostname);
  if(!/^[a-z0-9_-]{1,64}$/.test(board??'')||!(/^[a-f0-9]{32}$/.test(id??''))||/^0{32}$/.test(id))throw Error('Explicit target board and request ID required');
  if(!['/api/peer/info','/api/status'].includes(path)||
    (boot&&(!/^[a-f0-9]{32}$/.test(boot)||/^0{32}$/.test(boot)))||
    (path!=='/api/peer/info'&&!boot))throw Error('Verified target boot and read path required');
  if(address!==undefined){
    const parts=address.split('.');
    if(parts.length!==4||parts.some(p=>! /^(0|[1-9][0-9]{0,2})$/.test(p)||Number(p)>255))throw Error('A private IPv4 address hint is required');
    const [a,b,,d]=parts.map(Number);
    if(d===0||d===255||!(a===10||(a===172&&b>=16&&b<=31)||(a===192&&b===168)))throw Error('A private IPv4 address hint is required');
  }
  return {request_id:id,hostname,board_id:board,boot_id:boot,method:'GET',path,...(address?{address}:{})};
}
export async function checkPeerRelay({origin,targetHostname,targetBoard,targetAddress,samples=3}) {
  const url=deviceOrigin(origin);deviceOrigin('http://'+targetHostname);
  if(!/^[a-z0-9_-]{1,64}$/.test(targetBoard??''))throw Error('Explicit target board ID required');
  if(!Number.isInteger(samples)||samples<1||samples>6)throw Error('Use 1..6 status samples (bounded receipt history)');
  // Validate hints before opening a network connection. They are dial candidates,
  // never replacements for the destination's board/boot identity.
  relayReadEnvelope({id:'10000000000000000000000000000000',hostname:targetHostname,board:targetBoard,path:'/api/peer/info',address:targetAddress});
  const initial=await deviceRequest(url,'/api/status');
  const status=JSON.parse(initial.body),peer=boardPeer(status,initial.remote);
  await checkWebSocket(url,peer);
  if(!status.capabilities?.acknowledged_features?.includes('peer-relay-http'))throw Error('Relay capability not acknowledged');
  const headers=toolHeaders({'X-Pakar-Board':peer.board,'X-Pakar-Boot':peer.boot});
  const result=[];
  let targetBoot='';
  for(const path of ['/api/peer/info',...Array(samples).fill('/api/status')]) {
    const id=randomBytes(16).toString('hex');
    const body=Buffer.from(JSON.stringify(relayReadEnvelope({id,hostname:targetHostname,board:targetBoard,boot:targetBoot,path,address:targetAddress})));
    const started=performance.now();
    const admission=await new Promise((resolve,reject)=>{
      const req=(url.protocol==='https:'?https:http).request(new URL('/api/peers/request',url),{
        method:'POST',agent:false,family:4,
        lookup:(_host,options,cb)=>cb(null,options.all?[{address:peer.address,family:4}]:peer.address,4),
        headers:{...headers,'Content-Type':'application/json','Content-Length':body.length},
      },res=>{let bytes=0,chunks=[];
        res.on('data',c=>{bytes+=c.length;if(bytes>65536)req.destroy(Error('Oversized admission'));else chunks.push(c)});
        res.on('error',reject);res.on('end',()=>{try{
          if(res.statusCode!==200||res.headers['x-pakar-board']!==peer.board||res.headers['x-pakar-boot']!==peer.boot)throw Error('Relay identity/status changed');
          resolve(JSON.parse(Buffer.concat(chunks)));
        }catch(e){reject(e)}});
      });
      const timer=setTimeout(()=>req.destroy(Error('Relay admission outcome unknown; no retry')),8000);
      req.on('close',()=>clearTimeout(timer));req.on('error',reject);req.end(body);
    });
    if(admission.accepted!==true||admission.request_id!==id)throw Error('Relay rejected: '+(admission.error??'uncorrelated admission'));
    let receipt=admission;
    const until=Date.now()+15000;
    while(receipt.busy===true&&Date.now()<until){
      await delay(200);
      const r=await deviceRequest(url,'/api/peers/result?request_id='+id,{headers,expectedAddress:peer.address});
      if(r.status!==200||r.headers['x-pakar-board']!==peer.board||r.headers['x-pakar-boot']!==peer.boot)throw Error('Relay changed while reading result');
      receipt=JSON.parse(r.body);
    }
    if(receipt.request_id!==id||receipt.target_board_id!==targetBoard||receipt.response_verified!==true||receipt.phase!=='complete')throw Error('Peer read failed: '+(receipt.error||receipt.phase));
    const remote=JSON.parse(receipt.response);
    if(remote.board_id!==targetBoard||!/^[a-f0-9]{32}$/.test(remote.boot_id??'')||(targetBoot&&remote.boot_id!==targetBoot))throw Error('Downstream identity mismatch');
    targetBoot=remote.boot_id;
    result.push({path,board_id:remote.board_id,boot_id:remote.boot_id,hostname:remote.hostname,avr_online:remote.avr_online,role:remote.capabilities?.role,
      observer_elapsed_ms:Math.round(performance.now()-started),relay_elapsed_ms:receipt.elapsed_ms??null,
      resolution_ms:receipt.resolution_ms??null,http_ms:receipt.http_ms??null,route_source:receipt.route_source??null,
      verified_address:receipt.address??null});
  }
  return {ok:true,via_board_id:peer.board,origin:url.origin,targetHostname,results:result,commands_sent:0,firmware_written:false};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(process.argv[1]).href){
  try {
    const args=process.argv.slice(2),options={};
    for(let i=0;i<args.length;i+=2){const key={'--origin':'origin','--target-hostname':'targetHostname','--target-board':'targetBoard','--target-address':'targetAddress','--samples':'samples'}[args[i]];if(!key||!args[i+1])throw Error('Use --origin HOST_ORIGIN --target-hostname HOST --target-board BOARD_ID [--target-address PRIVATE_IPV4] [--samples 1..6]');options[key]=key==='samples'?Number(args[i+1]):args[i+1]}
    console.log(JSON.stringify(await checkPeerRelay(options),null,2));
  }catch(e){console.error(e.message);process.exitCode=1;}
}
