// Read-only Internet verification. No tablet/board command, APK installation,
// credentials, filesystem cleanup or presentation publication.
import {createHash,randomUUID} from 'node:crypto';
import {WebSocket} from '../services/client-management/node_modules/ws/wrapper.mjs';
import {toolHeaders,assertToolPeerCompatible} from './peer-identity.mjs';
const base='https://office.digitalogic.ir/pakar';
async function read(url,maximum){
  const response=await fetch(url,{headers:toolHeaders(),redirect:'error',signal:AbortSignal.timeout(10000)});
  if(!response.ok||Number(response.headers.get('content-length')??0)>maximum)throw Error('Presentation HTTP rejected');
  const chunks=[];let bytes=0;for await(const chunk of response.body){bytes+=chunk.length;if(bytes>maximum)throw Error('Presentation limit');chunks.push(chunk);}
  return Buffer.concat(chunks);
}
const peer=assertToolPeerCompatible(JSON.parse(await read(base+'/api/peer/info',16384)));
if(peer.application!=='pakarcontroller-client-management')throw Error('Unexpected management peer');
const manifest=JSON.parse(await read(base+'/api/presentation',32768));
if(!Number.isSafeInteger(manifest.revision)||manifest.revision<1||!Number.isSafeInteger(manifest.bytes)||manifest.bytes<1||manifest.bytes>24576||
  !/^[a-f0-9]{64}$/.test(manifest.sha256??'')||new URL(manifest.url).origin!==new URL(base).origin)throw Error('Invalid presentation manifest');
const bytes=await read(manifest.url,24576),digest=createHash('sha256').update(bytes).digest('hex');
if(bytes.length!==manifest.bytes||digest!==manifest.sha256||JSON.parse(bytes).revision!==manifest.revision)throw Error('Presentation digest/bytes/revision mismatch');
const session=randomUUID();
const notice=await new Promise((resolve,reject)=>{
  const ws=new WebSocket(base.replace('https:','wss:')+'/api/events',{headers:toolHeaders()});
  const timeout=setTimeout(()=>{ws.terminate();reject(Error('Presentation WSS notice deadline'));},10000);
  ws.on('error',error=>{clearTimeout(timeout);reject(error);});
  ws.on('open',()=>ws.send(JSON.stringify({type:'subscribe',package:'ir.digitalogic.pakar.pakarcontroller',session,presentation:true})));
  ws.on('message',raw=>{try{
    const value=JSON.parse(raw.toString());if(value.kind!=='presentation')return;
    if(value.type!=='app-update-offer'||value.package!=='ir.digitalogic.pakar.pakarcontroller'||value.session!==session||value.manifest_url!==base+'/api/presentation'||
      value.revision!==manifest.revision||value.sha256!==digest||value.expires_at_ms<=Date.now()||value.expires_at_ms>Date.now()+60000)throw Error('Presentation WSS identity mismatch');
    clearTimeout(timeout);ws.close();resolve({session_bound:true,revision:value.revision,sha256:value.sha256});
  }catch(error){clearTimeout(timeout);ws.terminate();reject(error);}});
});
console.log(JSON.stringify({service:base,build:peer.build,revision:manifest.revision,bytes:bytes.length,sha256:digest,websocket:notice,device_contacted:false}));
