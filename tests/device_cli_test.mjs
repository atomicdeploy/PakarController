import assert from 'node:assert/strict';
import http from 'node:http';
import {execFile} from 'node:child_process';
import {promisify} from 'node:util';
import {fileURLToPath} from 'node:url';
import {cliStatus,cliHeaders,assertCliIdentity} from './cli_peer_fixture.mjs';
const run=promisify(execFile);
const exe=process.argv[2]||fileURLToPath(new URL('../desktop/target/release/pakarcontroller.exe',import.meta.url));
// Loopback HTTP fixture, not an actual device; exercises the built CLI request path.
let requests=0,preflights=0,persisted=true,hostname='old-board',display_name='Fixture board';
const server=http.createServer(async(req,res)=>{
  requests++;cliHeaders(res);assertCliIdentity(req,req.url!=='/api/status');
  assert.equal(req.headers.authorization,'Basic '+Buffer.from('fixture:private-fixture-password').toString('base64'));
  if(req.method==='GET'&&req.url==='/api/status'){preflights++;res.end(JSON.stringify(cliStatus({hostname})));return;}
  assert.equal(req.url,'/api/device/name');
  let body='';for await(const chunk of req)body+=chunk;
  let apply_pending=false;
  if(req.method==='POST') { const form=new URLSearchParams(body);assert.equal(form.get('hostname'),'ledboard');apply_pending=hostname!==form.get('hostname');hostname='ledboard';display_name=form.get('display_name')??display_name; }
  res.writeHead(req.method==='POST'?202:200,{'Content-Type':'application/json'});
  res.end(JSON.stringify({hostname,display_name,apply_pending,persisted,accepted:req.method==='POST',fqdn:hostname+'.local'}));
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const env={...process.env,PAKAR_ENDPOINT:'http://127.0.0.1:'+server.address().port,PAKAR_USER:'fixture',PAKAR_PASSWORD:'private-fixture-password'};
try {
  const info=await run(exe,['--command','device','name'],{env});assert.equal(JSON.parse(info.stdout).hostname,'old-board');
  const before=requests;
  await assert.rejects(run(exe,['--command','device','rename','bad/url'],{env}));assert.equal(requests,before+1,'Only the read-only release preflight; invalid name sends no device action');
  const renamed=await run(exe,['--command','device','rename','LEDBOARD.Local'],{env});assert.match(renamed.stdout,/http:\/\/ledboard.local/);
  const labelled=await run(exe,['--command','device','label','Living','room','table'],{env});
  assert.equal(display_name,'Living room table');assert.match(labelled.stdout,/verified/);
  const invalidBefore=requests;await assert.rejects(run(exe,['--command','device','label','bad\nname'],{env}));assert.equal(requests,invalidBefore+1,'Invalid label sends only the read-only release preflight');
  persisted=false;
  await assert.rejects(run(exe,['--command','device','rename','ledboard'],{env}),e=>e.code===2&&/did not confirm/.test(e.stderr));
  assert.equal(preflights,6,'One release/identity preflight per explicit command');
  assert.equal(requests,12,'Six preflights plus explicit GET/POST and required readback; no discovery or mutation retries');
  console.log('Built CLI: supported pinned preflight, authenticated GET/POST, normalization, invalid-name rejection before mutation, persistence confirmation and no mutation retries passed (loopback fixture).');
} finally {await new Promise(resolve=>server.close(resolve));}
