// Read-only actual HTTPS + public WebSocket delivery. No installation or client
// command. Hash in memory; never duplicate release artifacts onto disk.
import {createHash} from 'node:crypto';
import {WebSocket} from '../services/client-management/node_modules/ws/wrapper.mjs';
const base='https://office.digitalogic.ir/pakar',proof={service:base,packages:[],websocket:null};
for(const platform of ['windows-x64','android']){
 const response=await fetch(base+'/api/releases?platform='+platform,{redirect:'error',signal:AbortSignal.timeout(10000)});
 if(!response.ok)throw Error('Catalog unavailable');const release=(await response.json()).releases[0];
 if(!release||release.product!=='PakarController'||release.platform!==platform||!Number.isSafeInteger(release.bytes)||release.bytes<1||release.bytes>128*1024*1024||!/^[a-f0-9]{64}$/.test(release.sha256)||new URL(release.url).origin!==new URL(base).origin)throw Error('Catalog identity invalid');
 const data=await fetch(release.url,{redirect:'error',signal:AbortSignal.timeout(60000)});if(!data.ok)throw Error('Artifact unavailable');
 const digest=createHash('sha256');let bytes=0;for await(const b of data.body){bytes+=b.length;if(bytes>release.bytes)throw Error('Artifact exceeds identity');digest.update(b);}
 const actual=digest.digest('hex');if(bytes!==release.bytes||actual!==release.sha256)throw Error('Artifact identity differs');
 proof.packages.push({platform,build:release.build,bytes,sha256:actual});
}
proof.websocket=await new Promise((done,reject)=>{
 const socket=new WebSocket(base.replace('https:','wss:')+'/api/events');const timeout=setTimeout(()=>{socket.terminate();reject(Error('Public WSS deadline'));},10000);
 socket.on('error',e=>{clearTimeout(timeout);reject(e);});socket.on('open',()=>socket.send(JSON.stringify({type:'subscribe',session:'verification-session-1234567890',package:'ir.digitalogic.pakar.pakarcontroller'})));
 socket.once('message',bytes=>{try{const v=JSON.parse(bytes.toString());if(v.session!=='verification-session-1234567890'||v.sha256!==proof.packages[1].sha256||v.version_code!==proof.packages[1].build||v.expires_at_ms<Date.now())throw Error('Public WSS identity differs');clearTimeout(timeout);socket.close();done({version_code:v.version_code,sha256:v.sha256,session_bound:true});}catch(e){clearTimeout(timeout);socket.terminate();reject(e);}});
});
console.log(JSON.stringify(proof));
