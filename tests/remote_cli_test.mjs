import assert from 'node:assert/strict';
import http from 'node:http';
import {execFile} from 'node:child_process';
import {promisify} from 'node:util';
import {fileURLToPath} from 'node:url';
const run=promisify(execFile);
const exe=process.argv[2]||fileURLToPath(new URL('../desktop/target/release/pakarcontroller.exe',import.meta.url));
let posts=0,gets=0,wrongAck=false;
let job={schema:1,job_id:1,boot_id:'fixture-boot',phase:'ready',targets:3,manifest_sha256:'a'.repeat(64),busy:false,cancellable:false,received_bytes:32768,expected_bytes:32768,error:''};
const server=http.createServer(async(req,res)=>{
 assert.equal(req.headers.authorization,'Basic '+Buffer.from('fixture:private-fixture-password').toString('base64'));
 let body='';for await(const c of req)body+=c;
 let value=job;
 if(req.method==='POST'){
  posts++;const form=new URLSearchParams(body);
  if(req.url==='/api/v1/firmware/fetch'){
   assert.equal(form.get('apply'),'false');assert.equal(form.get('bearer'),'private-fixture-token');
   assert.equal(form.get('url'),'https://updates.example.test/PakarController/manifest.json');
   assert.equal(form.get('sha256'),'a'.repeat(64));job={...job,phase:'manifest',busy:true,cancellable:true};
  }else{
   assert(['/api/v1/firmware/apply','/api/v1/firmware/cancel'].includes(req.url));
   assert.equal(form.get('job_id'),'1');assert.equal(form.get('boot_id'),'fixture-boot');assert.equal(form.get('sha256'),'a'.repeat(64));
   job={...job,phase:req.url.endsWith('apply')?'prepare':'cancelled',busy:req.url.endsWith('apply'),cancellable:false};
  }
  value=wrongAck?{...job,boot_id:'wrong-peer-boot'}:job;
 }else{
  gets++;
  if(req.url==='/api/v1/telemetry')value={schema:1,type:'telemetry',product:'PakarController',boot_id:'fixture-boot',esp:{model:'fixture only'},avr:{online:false},clients:[]};
  else assert.equal(req.url,'/api/v1/firmware/remote');
 }
 res.writeHead(req.url.endsWith('fetch')?202:200,{'Content-Type':'application/json'});res.end(JSON.stringify(value));
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const env={...process.env,PAKAR_ENDPOINT:'http://127.0.0.1:'+server.address().port,PAKAR_USER:'fixture',PAKAR_PASSWORD:'private-fixture-password',PAKAR_FIRMWARE_TOKEN:'private-fixture-token'};
try{
 const call=(...args)=>run(exe,['--command',...args],{env});
 assert.equal(JSON.parse((await call('info')).stdout).type,'telemetry');
 const before=posts;await assert.rejects(call('firmware','fetch','http://insecure.test/manifest.json','a'.repeat(64)));assert.equal(posts,before);
 const staged=JSON.parse((await call('firmware','fetch','https://updates.example.test/PakarController/manifest.json','a'.repeat(64),'all')).stdout);assert.equal(staged.phase,'manifest');
 assert(!JSON.stringify(staged).includes('private-fixture-token'));
 job={...job,phase:'ready',busy:false,cancellable:false};
 await assert.rejects(call('firmware','apply','1','stale-boot','a'.repeat(64)));assert.equal(posts,before+1);
 assert.equal(JSON.parse((await call('firmware','apply','1','fixture-boot','a'.repeat(64))).stdout).phase,'prepare');
 job={...job,phase:'ready',busy:false};wrongAck=true;const lostBefore=posts;
 await assert.rejects(call('firmware','apply','1','fixture-boot','a'.repeat(64)));assert.equal(posts,lostBefore+1,'Uncertain mutation must never replay');
 wrongAck=false;job={...job,phase:'ready',busy:false};
 assert.equal(JSON.parse((await call('firmware','cancel','1','fixture-boot','a'.repeat(64))).stdout).phase,'cancelled');
 assert(gets>=5);
 console.log('Built CLI: full telemetry, stage-only pinned HTTPS, token in form only, stale identity rejection, exact apply/cancel and wrong-ACK no-replay passed (loopback, not hardware).');
}finally{await new Promise(resolve=>server.close(resolve));}
