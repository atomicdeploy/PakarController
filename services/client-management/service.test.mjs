import test from 'node:test';
import assert from 'node:assert/strict';
import {randomBytes} from 'node:crypto';
import {mkdir,mkdtemp,rm,lstat,readFile,writeFile,utimes} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {WebSocket} from 'ws';
import {createService,sha256} from './service.mjs';

test('actual management HTTP, durable enrollment, immutable packages and one-use actions',async t=>{
  const parent=resolve('build/native/client-management');await mkdir(parent,{recursive:true});
  const directory=await mkdtemp(join(parent,'owned-test-'));
  assert.ok(directory.startsWith(parent+requireSeparator()));
  const admin=randomBytes(32).toString('base64url');let clock=1800000000000;
  let app=await createService({directory,publicBase:'http://localhost/pakar',adminTokenHash:sha256(admin),now:()=>clock,test:true});
  await new Promise(r=>app.server.listen(0,'127.0.0.1',r));
  const base=`http://127.0.0.1:${app.server.address().port}`;
  t.after(async()=>{await app.close();await rm(directory,{recursive:true});});
  async function request(path,{method='GET',token,body,headers={}}={}){
    const r=await fetch(base+'/pakar'+path,{method,headers:{...(token?{Authorization:'Bearer '+token}:{}),...(body?{'Content-Type':'application/json'}:{}),...headers},body:body?JSON.stringify(body):undefined});
    return {status:r.status,body:await r.json()};
  }
  const administrate=(path,body)=>request('/api/admin'+path,{method:'POST',token:admin,body});
  await t.test('private inventory, cross-origin and unsafe commands fail closed',async()=>{
    assert.equal((await request('/api/admin/clients')).status,401);
    assert.equal((await request('/api/health',{headers:{Origin:'https://foreign.invalid'}})).status,403);
    assert.equal((await request('/api/health')).body.ready,true);
    assert.equal((await request('/api/app-updates')).status,400);
    assert.equal((await request('/packages/../../management.sqlite')).status,404);
  });
  let token,client='android-test-client-0001';
  await t.test('enrollment is one-use, platform-bound, and stores only hashes',async()=>{
    const e=await administrate('/enrollments',{platform:'android'});assert.equal(e.status,201);
    const enrollment={enrollment_token:e.body.enrollment_token,client_id:client,platform:'android',application:'PakarController'};
    assert.equal((await request('/api/clients/enroll',{method:'POST',body:{...enrollment,platform:'windows-x64'}})).status,403);
    const enrolled=await request('/api/clients/enroll',{method:'POST',body:enrollment});assert.equal(enrolled.status,201);token=enrolled.body.token;
    assert.equal((await request('/api/clients/enroll',{method:'POST',body:enrollment})).status,403);
    const inventory=await request('/api/admin/clients',{token:admin});assert.equal(inventory.body.clients.length,1);
    assert.ok(!JSON.stringify(inventory).includes(token));
    assert.ok(!JSON.stringify(inventory).includes('token_hash'));
    const bytes=Buffer.concat(await Promise.all(['management.sqlite','management.sqlite-wal'].map(p=>readFile(join(directory,p)))));
    assert.ok(!bytes.includes(Buffer.from(token)));assert.ok(!bytes.includes(Buffer.from(e.body.enrollment_token)));
  });
  await t.test('telemetry is identity-bound and strips unknown/private fields',async()=>{
    const report={client_id:client,application:'PakarController',platform:'android',version:'0.2.7',build:9,foreground:true,update_state:'idle',password:'not-sent',files:['private']};
    assert.equal((await request('/api/clients/report',{method:'POST',token,body:{...report,client_id:'another-client-000'}})).status,409);
    assert.equal((await request('/api/clients/report',{method:'POST',token,body:report})).status,200);
    const v=(await request('/api/admin/clients',{token:admin})).body.clients[0];assert.equal(v.telemetry.build,9);assert.equal(v.telemetry.password,undefined);assert.equal(v.telemetry.files,undefined);
    assert.equal((await administrate('/clients/'+client+'/commands',{action:'shell'})).status,400);
    assert.equal((await administrate('/clients/'+client+'/commands',{action:'uninstall',confirmation:'uninstall PakarController'})).status,409);
  });
  await t.test('durable command claims cannot replay and expired commands do not act',async()=>{
    const queued=await administrate('/clients/'+client+'/commands',{action:'check-update'});assert.equal(queued.status,202);
    const id=queued.body.id,path='/api/client/commands/'+id;
    assert.equal((await request('/api/client/commands',{token})).body.commands.length,1);
    assert.equal((await request(path+'/result',{method:'POST',token,body:{state:'done'}})).status,400);
    assert.equal((await request(path+'/claim',{method:'POST',token,body:{}})).status,200);
    assert.equal((await request(path+'/claim',{method:'POST',token,body:{}})).status,409);
    assert.equal((await request('/api/client/commands',{token})).body.commands.length,0);
    for(let i=0;i<2;i++)assert.equal((await request(path+'/result',{method:'POST',token,body:{state:'requested'}})).status,200);
    assert.equal((await request(path+'/result',{method:'POST',token,body:{state:'different'}})).status,409);
    const late=await administrate('/clients/'+client+'/commands',{action:'cleanup'});clock+=56000;
    assert.equal((await request('/api/client/commands/'+late.body.id+'/claim',{method:'POST',token,body:{}})).status,409);
  });
  let latest;
  async function publish(build,{badDigest=false}={}){
    const data=Buffer.alloc(512,build);data.writeUInt32LE(0x04034b50);
    const meta={product:'PakarController',platform:'android',version:'0.2.7',build,bytes:data.length,sha256:badDigest?'0'.repeat(64):sha256(data),package:'ir.digitalogic.pakar.pakarcontroller',signers:['a'.repeat(64)]};
    const r=await fetch(base+'/pakar/api/admin/releases',{method:'POST',headers:{Authorization:'Bearer '+admin,'X-Pakar-Release':Buffer.from(JSON.stringify(meta)).toString('base64'),'Content-Length':String(data.length)},body:data});
    return {status:r.status,body:await r.json(),data};
  }
  await t.test('publication verifies bounded artifact SHA and serving ranges use exact immutable identity',async()=>{
    assert.equal((await publish(1,{badDigest:true})).status,400);
    latest=await publish(1);assert.equal(latest.status,201);
    assert.equal((await publish(1)).status,409);
    const path='/packages/'+latest.body.sha256+'/PakarController.apk';
    const r=await fetch(base+'/pakar'+path);assert.deepEqual(Buffer.from(await r.arrayBuffer()),latest.data);
    assert.equal(r.headers.get('x-checksum-sha256'),latest.body.sha256);
    const range=await fetch(base+'/pakar'+path,{headers:{Range:'bytes=4-9'}});assert.equal(range.status,206);assert.equal((await range.arrayBuffer()).byteLength,6);
    assert.equal((await fetch(base+'/pakar'+path,{headers:{Range:'bytes=512-513'}})).status,416);
    assert.equal((await fetch(base+'/pakar'+path.replace('.apk','.exe'))).status,404);
    const catalog=await request('/api/app-updates',{headers:{'X-Pakar-App-Session':'session-1234567890123456','X-Pakar-App-Package':'ir.digitalogic.pakar.pakarcontroller'}});
    assert.equal(catalog.body.offers[0].version_code,1);assert.equal(catalog.body.offers[0].expires_at_ms,clock+55000);
  });
  await t.test('WebSocket feed is session-bound and contains public releases only',async()=>{
    const ws=new WebSocket(base.replace('http:','ws:')+'/pakar/api/events');t.after(()=>ws.terminate());
    await new Promise((r,reject)=>{ws.once('open',r);ws.once('error',reject);});
    const received=new Promise((r,reject)=>{ws.once('message',b=>r(JSON.parse(b.toString())));ws.once('error',reject);});
    ws.send(JSON.stringify({type:'subscribe',session:'session-1234567890123456',package:'ir.digitalogic.pakar.pakarcontroller'}));
    const v=await received;assert.equal(v.session,'session-1234567890123456');assert.equal(v.sha256,latest.body.sha256);assert.equal(v.token,undefined);ws.terminate();
  });
  await t.test('retention preserves newest three and current packages; revocation is immediate',async()=>{
    const interrupted=join(directory,'packages','0f5b3f55-ab44-4a27-a000-57087eb668ce.pending');
    const unrelated=join(directory,'packages','user-file.pending');
    await writeFile(interrupted,'aborted transfer',{flag:'wx'});await utimes(interrupted,new Date(clock-2*86400000),new Date(clock-2*86400000));
    await writeFile(unrelated,'preserve',{flag:'wx'});
    assert.equal((await app.cleanup()).interrupted_uploads,1);assert.equal(await readFile(unrelated,'utf8'),'preserve');
    for(let build=2;build<=4;build++)assert.equal((await publish(build)).status,201);
    assert.equal((await app.cleanup()).removed,0);clock+=8*86400000;
    assert.equal((await app.cleanup()).removed,1);
    await assert.rejects(lstat(join(directory,'packages',latest.body.sha256)),{code:'ENOENT'});
    assert.deepEqual(app.releases('android').map(v=>v.build),[4,3,2]);
    assert.equal((await administrate('/clients/'+client+'/revoke',{})).status,200);
    assert.equal((await request('/api/client/commands',{token})).status,401);
  });
  await t.test('service restart retains releases, enrollment revocation and command receipts',async()=>{
    await app.close();
    app=await createService({directory,publicBase:'http://localhost/pakar',adminTokenHash:sha256(admin),now:()=>clock,test:true});
    assert.deepEqual(app.releases('android').map(v=>v.build),[4,3,2]);
    await new Promise(r=>app.server.listen(0,'127.0.0.1',r));
    const port=app.server.address().port;
    const response=await fetch(`http://127.0.0.1:${port}/pakar/api/client/commands`,{headers:{Authorization:'Bearer '+token}});assert.equal(response.status,401);
  });
});
function requireSeparator(){return process.platform==='win32'?'\\':'/';}
