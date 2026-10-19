// PCController netpolicy/http_url.go authority checks adapted to a hostname-
// first Ethernet acceptance probe. No redirect, proxy or automatic write retry.
import http from 'node:http';
import https from 'node:https';
import {isIP} from 'node:net';
import {pathToFileURL} from 'node:url';
import {setTimeout as delay} from 'node:timers/promises';
import {toolHeaders,assertToolPeerCompatible} from './peer-identity.mjs';

export function deviceOrigin(value) {
  const url=new URL(value);
  if(!['http:','https:'].includes(url.protocol)||url.username||url.password||url.search||url.hash||
     url.pathname!=='/'||/[\s\\#]/.test(value)||isIP(url.hostname)||url.port==='0'||
     url.hostname.length>253||!url.hostname.split('.').every(s=>/^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$/i.test(s))) {
    throw Error('Use a hostname-only HTTP(S) origin without credentials, path, query or fragment');
  }
  return url;
}
const ipv4=address=>address?.replace(/^::ffff:/,'');
export function ethernetReady(status,remote,peer=null) {
  assertToolPeerCompatible(status);
  const e=status?.ethernet;
  if(status?.http_peer_fencing!==true||!/^[a-z0-9_-]{1,64}$/.test(status.board_id??'')||
     !/^[a-f0-9]{32}$/.test(status.boot_id??'')||/^0{32}$/.test(status.boot_id)||
     typeof status.capabilities?.board_kind!=='string'||!status.capabilities.board_kind||
     status.capabilities.hostname!==status.hostname||!status.capabilities?.acknowledged_features?.includes('network'))throw Error('Compatible board/boot identity is unavailable');
  if(peer&&(status.board_id!==peer.board||status.boot_id!==peer.boot))throw Error('Board/boot changed; nothing replayed');
  if(status.uplink!=='ethernet'||status.uplink_ipv4_ready!==true||e?.configured!==true||
     e.initialized!==true||e.link_up!==true||e.ipv4_ready!==true||isIP(e.ip)!==4||
     e.ip==='0.0.0.0'||e.ip.startsWith('127.')||Number(e.ip.split('.')[0])>=224||
     ipv4(remote)!==e.ip)throw Error('Ethernet is not ready or this TCP connection reached a different interface');
  return {board:status.board_id,boot:status.boot_id,address:e.ip};
}
export function deviceRequest(origin,path,{form,headers={},expectedAddress,method}={}) {
  if(!path.startsWith('/')||path.startsWith('//')||/[\s\\]/.test(path))throw Error('Invalid device path');
  const body=form?new URLSearchParams(form).toString():null;
  return new Promise((resolve,reject)=>{
    const client=origin.protocol==='https:'?https:http;
    const request=client.request(new URL(path,origin),{
      method:method||(body?'POST':'GET'),agent:false,family:4,
      ...(expectedAddress?{lookup:(_host,options,callback)=>callback(null,options.all?[{address:expectedAddress,family:4}]:expectedAddress,4)}:{}),
      headers:toolHeaders({Accept:'application/json',...headers,...(body?{'Content-Type':'application/x-www-form-urlencoded','Content-Length':Buffer.byteLength(body)}:{})}),
    },response=>{
      const remote=ipv4(response.socket.remoteAddress),chunks=[];let bytes=0;
      if(expectedAddress&&remote!==expectedAddress){response.destroy();reject(Error('TCP peer changed; no retry'));return;}
      response.on('data',chunk=>{bytes+=chunk.length;if(bytes>512*1024){request.destroy(Error('Response exceeds bounded probe size'));return;}chunks.push(chunk)});
      response.on('error',()=>reject(Error('Device response interrupted; no retry')));
      response.on('end',()=>resolve({status:response.statusCode,headers:response.headers,body:Buffer.concat(chunks),remote}));
    });
    const timer=setTimeout(()=>request.destroy(Error('Device request timed out; no retry')),8000);
    request.on('close',()=>clearTimeout(timer));
    request.on('error',error=>{
      const code=typeof error.code==='string'&&/^[A-Z0-9_]{1,32}$/.test(error.code)?error.code:
        error.message==='Device request timed out; no retry'?'deadline':'transport';
      reject(Error(`Device request ${method||(body?'POST':'GET')} ${path.split('?')[0]} failed (${code}); no retry`,{cause:error}));
    });
    if(body)request.write(body);request.end();
  });
}

export async function checkEthernet({origin,run=false,manifest,manifestSha,imageSha,timeoutSeconds=180,
  environment=process.env,request=deviceRequest,wait=delay}={}) {
  const url=deviceOrigin(origin);
  if(!run)throw Error('Explicit --run required. Optional --manifest starts ESP-only staging, never apply');
  if(!Number.isInteger(timeoutSeconds)||timeoutSeconds<5||timeoutSeconds>600)throw Error('Timeout must be5..600 seconds');
  const user=environment.PAKAR_ETH_USERNAME,password=environment.PAKAR_ETH_PASSWORD;
  if((user===undefined)!==(password===undefined))throw Error('Set both PAKAR_ETH_USERNAME/PASSWORD, or neither');
  const headers=user===undefined?{}:{Authorization:'Basic '+Buffer.from(user+':'+password).toString('base64')};
  if(manifest){const source=new URL(manifest);
    if(source.protocol!=='https:'||source.username||source.password||source.hash||/[\s\\]/.test(manifest)||
       !/^[a-f0-9]{64}$/.test(manifestSha??'')||!/^[a-f0-9]{64}$/.test(imageSha??'')) {
      throw Error('Remote staging requires HTTPS manifest and pinned manifest/image SHA256 values');
    }
  } else if(manifestSha||imageSha)throw Error('Manifest URL is required with hashes');
  let peer=null;
  const read=async(path,options={})=>{
    const response=await request(url,path,{...options,expectedAddress:peer?.address,
      headers:{...headers,...(peer?{'X-Pakar-Board':peer.board,'X-Pakar-Boot':peer.boot}:{}),...options.headers}});
    if(response.status<200||response.status>=300)throw Error('Device rejected '+path.split('?')[0]+' (HTTP '+response.status+'); no retry');
    return response;
  };
  const json=async(path,options)=>{const r=await read(path,options);let value;
    try{value=JSON.parse(r.body.toString('utf8'))}catch{throw Error('Malformed device JSON response')}
    return {value,remote:r.remote};
  };
  const first=await json('/api/status');peer=ethernetReady(first.value,first.remote);
  const fresh=async()=>{const next=await json('/api/status');ethernetReady(next.value,next.remote,peer);return next.value};
  const html=await read('/');if(!/<html|<!doctype html/i.test(html.body.toString('utf8')))throw Error('Device Web UI not served');
  const xml=await read('/description.xml');if(!/<UDN>uuid:/.test(xml.body.toString('utf8')))throw Error('SSDP description not served');
  const config=(await json('/api/network/ethernet')).value;if(config.configured!==true||typeof config.dhcp!=='boolean')throw Error('Ethernet configuration unavailable');
  const head=await read('/firmware/download',{method:'HEAD'});
  const size=Number(head.headers['content-length']);if(!Number.isSafeInteger(size)||size<24)throw Error('Firmware HEAD size unavailable');
  const end=Math.min(size-1,255),range=await read('/firmware/download',{headers:{Range:'bytes=0-'+end}});
  if(range.status!==206||range.headers['content-range']!=='bytes 0-'+end+'/'+size||range.body.length!==end+1||range.body[0]!==0xe9)throw Error('Firmware byte-range serving failed');
  let remoteFetch='not_requested';
  if(manifest){
    const status=await fresh();
    if(status.station_connected!==false)throw Error('Disable station association for the Ethernet-only outbound test; recovery AP may stay on');
    if(status.programming_latch)throw Error('Programming safety is active; no staging request sent');
    const previous=(await json('/api/firmware/remote')).value;
    if(previous.busy||!['idle','complete','cancelled'].includes(previous.phase))throw Error('Existing firmware job needs inspection; not overwritten');
    // One mutation only. Preserve any unknown result, never auto-apply/cancel.
    const stage=(await json('/api/firmware/fetch',{form:{url:manifest,sha256:manifestSha,targets:'1',apply:'false',
      ...(environment.PAKAR_ETH_FIRMWARE_BEARER?{bearer:environment.PAKAR_ETH_FIRMWARE_BEARER}:{})}})).value;
    if(!Number.isSafeInteger(stage.job_id)||stage.job_id<1||stage.boot_id!==peer.boot)throw Error('Unknown staging outcome; inspect original job');
    const deadline=Date.now()+timeoutSeconds*1000;
    for(;;){
      const status=await fresh();if(status.station_connected!==false)throw Error('Station became connected; Ethernet-only egress is no longer established');
      const job=(await json('/api/firmware/remote')).value;
      if(job.job_id!==stage.job_id||job.boot_id!==peer.boot||job.manifest_sha256!==manifestSha)throw Error('Staging job identity changed');
      if(job.phase==='ready'){
        if(job.targets!==1||job.esp?.sha256!==imageSha||!(job.esp.size>0)||job.busy!==false)throw Error('Staged image readback differs from the expected ESP image');
        remoteFetch={phase:'ready',job_id:job.job_id,image_sha256:imageSha,applied:false};break;
      }
      if(!job.busy||Date.now()>=deadline)throw Error('Staging failed or timed out; inspect original job, no retry or automatic cancel');
      await wait(1000);
    }
  }
  await fresh();
  return {ok:true,origin:url.origin,board_id:peer.board,boot_id:peer.boot,ethernet_address:peer.address,
    local_http:'Web UI, JSON, SSDP XML, Ethernet settings, firmware HEAD/Range',firmware_size:size,remote_fetch:remoteFetch,
    websocket:'requires separate app handshake/control test',multicast:'requires separate on-LAN discovery test',
    simulated:request!==deviceRequest,firmware_applied:false,avr_touched:false};
}

if(process.argv[1]&&import.meta.url===pathToFileURL(process.argv[1]).href){
  try{const options={};const args=process.argv.slice(2);
    for(let i=0;i<args.length;i++){if(args[i]==='--run'){options.run=true;continue;}
      const key={'--origin':'origin','--manifest':'manifest','--manifest-sha256':'manifestSha','--image-sha256':'imageSha','--timeout':'timeoutSeconds'}[args[i]];
      if(!key||!args[i+1])throw Error('Use --run --origin http://ledboard.local [--manifest HTTPS_URL --manifest-sha256 SHA --image-sha256 SHA --timeout 180]');
      options[key]=key==='timeoutSeconds'?Number(args[++i]):args[++i];
    }
    process.stdout.write(JSON.stringify(await checkEthernet(options),null,2)+'\n');
  }catch(error){process.stderr.write(error.message+'\n');process.exitCode=1;}
}
