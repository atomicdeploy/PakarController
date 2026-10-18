import assert from 'node:assert/strict';
import http from 'node:http';
import {execFile} from 'node:child_process';
import {promisify} from 'node:util';
import {fileURLToPath} from 'node:url';
const run=promisify(execFile);
const exe=process.argv[2]||fileURLToPath(new URL('../desktop/target/release/pakarcontroller.exe',import.meta.url));
// Loopback only: actual built CLI serialization and acknowledgement handling.
// This does not establish device persistence, DHCP or deployed firmware success.
let enabled=false,wrongAck=false,posts=0;
const server=http.createServer(async(req,res)=>{
  res.setHeader('Content-Type','application/json');
  if(req.method==='GET'&&req.url==='/api/auth/info'){
    res.end(JSON.stringify({ready:true,enabled,username:'owner',default_credentials:false}));return;
  }
  assert.equal(req.method,'POST');assert.equal(req.url,'/api/auth');++posts;
  assert.equal(req.headers.authorization,'Basic '+Buffer.from('owner:private-fixture').toString('base64'));
  let body='';for await(const chunk of req)body+=chunk;
  const form=new URLSearchParams(body);
  assert.equal(form.get('current_password'),'private-fixture');
  enabled=form.get('enabled')==='true';
  if(enabled){assert.equal(form.get('username'),'David');assert.equal(form.get('password'),'new-fixture-secret');}
  else{assert.equal(form.get('username'),'');assert.equal(form.get('password'),'');}
  res.end(JSON.stringify({updated:true,enabled:wrongAck?!enabled:enabled,reauthenticate:enabled}));
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const env={...process.env,PAKAR_ENDPOINT:'http://127.0.0.1:'+server.address().port,
  PAKAR_USER:'owner',PAKAR_PASSWORD:'private-fixture',PAKAR_NEW_PASSWORD:'new-fixture-secret'};
try{
  const info=await run(exe,['--command','auth','info'],{env,timeout:5000});
  assert.equal(JSON.parse(info.stdout).enabled,false);assert.equal(posts,0);
  await run(exe,['--command','auth','change','David'],{env,timeout:5000});assert(enabled);
  const disabled=await run(exe,['--command','auth','disable'],{env,timeout:5000});
  assert.match(disabled.stdout,/authentication disabled and saved/);assert(!enabled);assert.equal(posts,2);
  wrongAck=true;
  await assert.rejects(run(exe,['--command','auth','disable'],{env,timeout:5000}),
    error=>error.code===2&&/did not confirm the authentication setting/.test(error.stderr));
  assert.equal(posts,3,'No mutation retries after a mismatched acknowledgement');
  console.log('Built CLI: public auth-off info, explicit enable/disable forms, current credentials and mismatched acknowledgement rejection passed (loopback, not a board).');
}finally{await new Promise(resolve=>server.close(resolve));}
