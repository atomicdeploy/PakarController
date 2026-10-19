// PCController artifacts store/download discipline: bounded streamed bytes,
// immutable content identity, durable publication and explicit auth/ownership.
// This temporary service never executes client code or arbitrary remote commands.
import http from 'node:http';
import {DatabaseSync} from 'node:sqlite';
import {createHash,randomBytes,randomUUID,timingSafeEqual} from 'node:crypto';
import {constants,createReadStream,createWriteStream} from 'node:fs';
import {mkdir,lstat,open,rename,unlink,readFile,readdir} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Transform} from 'node:stream';
import {pipeline} from 'node:stream/promises';
import {WebSocketServer} from 'ws';
import {isIP} from 'node:net';
import {toolRelease,toolPolicy,toolVersionEnforced,toolSemver,peerVersionAllowed} from '../../tools/peer-identity.mjs';

export const sha256=value=>createHash('sha256').update(value).digest('hex');
const maximum=128*1024*1024, platforms=new Set(['android','windows-x64']);
const identity=v=>typeof v==='string'&&/^[a-zA-Z0-9_-]{16,80}$/.test(v);
const hash=v=>typeof v==='string'&&/^[a-f0-9]{64}$/.test(v);
const clean=(v,n=128)=>typeof v==='string'&&!/[\x00-\x1f\x7f]/.test(v)&&v.length<=n;
const fail=(code,message)=>Object.assign(Error(message),{status:code});
const json=(res,status,value)=>{res.writeHead(status,{'Content-Type':'application/json; charset=utf-8','Cache-Control':'no-store','X-Content-Type-Options':'nosniff'});res.end(JSON.stringify(value));};
const compatibility=()=>({enforced:toolVersionEnforced(),minimum_app:toolPolicy.minimum_app,minimum_board:toolPolicy.minimum_board,maximum_app:toolPolicy.maximum_app,maximum_board:toolPolicy.maximum_board});
export const serviceIdentity=()=>({product:toolRelease.product,application:'pakarcontroller-client-management',service:'client-management',version:toolRelease.version,build:toolRelease.build,compatibility:compatibility()});
const serviceHeaders=()=>({Server:toolRelease.product+'/'+toolRelease.version,'X-Pakar-Application':'pakarcontroller-client-management','X-Pakar-Version':toolRelease.version,'X-Pakar-Build':String(toolRelease.build),'X-Pakar-Min-App-Version':toolPolicy.minimum_app,'X-Pakar-Version-Check':toolVersionEnforced()?'true':'false'});
function rejectHttpSocket(socket,code) {
  if(socket.destroyed||!socket.writable)return;
  socket.once('finish',()=>socket.destroy());
  socket.end('HTTP/1.1 '+code+' Rejected\r\n'+Object.entries(serviceHeaders()).map(([k,v])=>k+': '+v+'\r\n').join('')+'Connection: close\r\nCache-Control: no-store\r\nContent-Length: 0\r\n\r\n');
}
function compatible(req) {
  const version=req.headers['x-pakar-version'];
  if(toolVersionEnforced()&&!peerVersionAllowed(version,toolPolicy.minimum_app,toolPolicy.maximum_app))throw fail(426,'Unsupported application version; download a supported app from the public release feed');
  return version;
}
function operational(raw,keys) {
  if(raw==null)return undefined;
  if(!raw||Array.isArray(raw)||typeof raw!=='object')throw fail(400,'Invalid operational summary');
  const out={};for(const key of keys)if(raw[key]!==undefined){const v=raw[key];
    if(typeof v==='boolean'||(Number.isSafeInteger(v)&&v>=0&&v<=0xffffffff)||clean(v,96))out[key]=v;
    else throw fail(400,'Invalid operational value');
  }return out;
}
async function body(req,limit=16384){
  let size=0;const chunks=[];
  for await(const b of req){size+=b.length;if(size>limit)throw fail(413,'Request too large');chunks.push(b);}
  let value;try{value=JSON.parse(Buffer.concat(chunks).toString('utf8'));}catch{throw fail(400,'Expected JSON object');}
  if(!value||Array.isArray(value)||typeof value!=='object')throw fail(400,'Expected JSON object');return value;
}
function bearer(req){const h=req.headers.authorization;return typeof h==='string'&&/^Bearer [a-zA-Z0-9_-]{43}$/.test(h)?h.slice(7):'';}
function sameHash(a,b){return hash(a)&&hash(b)&&timingSafeEqual(Buffer.from(a,'hex'),Buffer.from(b,'hex'));}
function validateRelease(raw){
  if(!raw||raw.product!=='PakarController'||!platforms.has(raw.platform)||
    !toolSemver(raw.version)||!Number.isSafeInteger(raw.build)||raw.build<1||raw.build>2147483647||!Number.isSafeInteger(raw.bytes)||raw.bytes<64||raw.bytes>maximum||!hash(raw.sha256))throw fail(400,'Invalid release identity');
  const out={product:'PakarController',platform:raw.platform,version:raw.version,build:raw.build,bytes:raw.bytes,sha256:raw.sha256};
  if(raw.platform==='android'){
    if(raw.package!=='ir.digitalogic.pakar.pakarcontroller'||!Array.isArray(raw.signers)||!raw.signers.length||raw.signers.length>4||raw.signers.some(v=>!hash(v)))throw fail(400,'Invalid Android package/signer identity');
    out.package=raw.package;out.version_code=raw.build;out.version_name=raw.version;out.signers=raw.signers;
  }
  if(raw.source_sha!==undefined){if(!/^[a-f0-9]{40}$/.test(raw.source_sha))throw fail(400,'Invalid source identity');out.source_sha=raw.source_sha;}
  return out;
}
export function telemetry(raw,client,observation={}){
  if(raw.client_id!==client.id||raw.application!=='PakarController'||raw.platform!==client.platform)throw fail(409,'Client identity changed');
  const out={client_id:client.id,application:'PakarController',platform:client.platform};
  for(const key of ['version','update_state','connection_state','board_id','hostname','architecture','os','message'])if(raw[key]!==undefined){
    if(!clean(raw[key],key==='message'?256:128))throw fail(400,'Invalid telemetry text');out[key]=raw[key];
  }
  for(const key of ['build','uptime_seconds','memory_mb','memory_available_mb','cpu_count','received_bytes','expected_bytes'])if(raw[key]!==undefined){
    if(!Number.isSafeInteger(raw[key])||raw[key]<0||raw[key]>Number.MAX_SAFE_INTEGER)throw fail(400,'Invalid telemetry counter');out[key]=raw[key];
  }
  for(const key of ['foreground','auto_check','auto_update','auto_cleanup','allow_remote_uninstall','managed_installation'])if(typeof raw[key]==='boolean')out[key]=raw[key];
  for(const key of ['session_id','device_model'])if(raw[key]!=null){if(!clean(raw[key],96))throw fail(400,'Invalid peer detail');out[key]=raw[key];}
  if(raw.reported_ip!=null){if(typeof raw.reported_ip!=='string'||!isIP(raw.reported_ip))throw fail(400,'Invalid reported IP');out.reported_ip=raw.reported_ip;}
  if(raw.capabilities!=null){if(!Array.isArray(raw.capabilities)||raw.capabilities.length>12||raw.capabilities.some(v=>!clean(v,32)||!/^[a-z][a-z0-9_.-]*$/.test(v)))throw fail(400,'Invalid capability summary');out.capabilities=[...new Set(raw.capabilities)];}
  const hardware=operational(raw.hardware,['model','manufacturer','cpu','os_version','sdk','memory_mb','cpu_count','screen_width','screen_height']);
  const settings=operational(raw.settings,['transport','theme','language','direction','palette','sound','silent_mode','telemetry_enabled','version_check','auto_check','auto_update','auto_cleanup','allow_remote_uninstall']);
  if(hardware)out.hardware=hardware;if(settings)out.settings=settings;
  out.self_reported=true;
  if(observation.address){out.observed_ip=observation.address;out.observed_address_source=observation.source;}
  if(typeof observation.agent==='string')out.user_agent=observation.agent.replace(/[^\x20-\x7e]/g,'').slice(0,256);
  if(clean(observation.application,64))out.client_application=observation.application;
  if(clean(observation.session,80))out.client_session=observation.session;
  // No passwords, enrollment tokens, board credentials, file/process inventory,
  // screen/audio content, arbitrary nested values or user-authored documents.
  return out;
}

export async function createService({directory,publicBase,adminTokenHash,now=Date.now,test=false,iconPath}={}){
  const base=new URL(publicBase);
  if(base.username||base.password||base.search||base.hash||base.pathname!=='/pakar'||(base.protocol!=='https:'&&!(test&&base.hostname==='localhost')))throw Error('Use an HTTPS /pakar service base');
  if(!hash(adminTokenHash)||adminTokenHash===sha256(''))throw Error('Admin token hash is required');
  const root=resolve(directory);if(root===resolve('/')||root===resolve('.'))throw Error('Dedicated service data directory required');
  await mkdir(root,{recursive:true,mode:0o700});
  if((await lstat(root)).isSymbolicLink())throw Error('Data root must not be a symlink');
  const blobs=join(root,'packages');await mkdir(blobs,{recursive:true,mode:0o700});
  if((await lstat(blobs)).isSymbolicLink())throw Error('Package root must not be a symlink');
  const db=new DatabaseSync(join(root,'management.sqlite'));
  db.exec(`PRAGMA journal_mode=WAL;PRAGMA synchronous=FULL;PRAGMA wal_autocheckpoint=128;
    CREATE TABLE IF NOT EXISTS releases(sha TEXT PRIMARY KEY,platform TEXT,build INTEGER,metadata TEXT,published INTEGER,UNIQUE(platform,build));
    CREATE TABLE IF NOT EXISTS enrollments(hash TEXT PRIMARY KEY,platform TEXT,expires INTEGER,used INTEGER DEFAULT 0);
    CREATE TABLE IF NOT EXISTS clients(id TEXT PRIMARY KEY,platform TEXT,token_hash TEXT UNIQUE,created INTEGER,last_seen INTEGER,telemetry TEXT DEFAULT '{}',revoked INTEGER DEFAULT 0);
    CREATE TABLE IF NOT EXISTS commands(id TEXT PRIMARY KEY,client TEXT,action TEXT,created INTEGER,expires INTEGER,claimed INTEGER DEFAULT 0,result TEXT);`);
  const sql=(s,...args)=>db.prepare(s).get(...args);
  const run=(s,...args)=>db.prepare(s).run(...args);
  const all=(s,...args)=>db.prepare(s).all(...args);
  const sockets=new WebSocketServer({noServer:true,maxPayload:8192,perMessageDeflate:false});
  let publishing=false,closed=false,cleaning=false;const activeReads=new Map(),rates=new Map();
  function admin(req){if(!sameHash(sha256(bearer(req)),adminTokenHash))throw fail(401,'Administrator authentication required');}
  function client(req){const v=sql('SELECT * FROM clients WHERE token_hash=? AND revoked=0',sha256(bearer(req)));if(!v)throw fail(401,'Enrolled client authentication required');return v;}
  function descriptor(row){const v=JSON.parse(row.metadata);return {...v,url:`${publicBase}/packages/${v.sha256}/${v.platform==='android'?'PakarController.apk':'PakarController.exe'}`};}
  function releases(platform){return all('SELECT * FROM releases WHERE platform=? ORDER BY build DESC LIMIT 3',platform).map(descriptor);}
  function offers(session,pkg){
    if(!identity(session)||pkg!=='ir.digitalogic.pakar.pakarcontroller')throw fail(400,'Valid app session and self package required');
    return {offers:releases('android').slice(0,1).map(v=>({...v,session,expires_at_ms:now()+55000,service:serviceIdentity()}))};
  }
  function announce(){for(const socket of sockets.clients){if(socket.readyState===1&&socket.subscription)for(const offer of offers(socket.subscription.session,socket.subscription.package).offers)socket.send(JSON.stringify(offer));}}
  async function publish(req){
    if(publishing)throw fail(409,'Publication already active');
    let raw;try{raw=JSON.parse(Buffer.from(req.headers['x-pakar-release']??'','base64').toString('utf8'));}catch{throw fail(400,'Missing release metadata');}
    const meta=validateRelease(raw);
    if(Number(req.headers['content-length'])!==meta.bytes)throw fail(400,'Exact artifact length required');
    const existing=sql('SELECT * FROM releases WHERE platform=? AND build=?',meta.platform,meta.build);
    if(existing)throw fail(409,existing.sha===meta.sha256?'Release already published':'Build identity already consumed');
    if(sql('SELECT COUNT(*) AS n FROM releases').n>=256)throw fail(507,'Release registry limit; inspect retention');
    publishing=true;const temporary=join(blobs,randomUUID()+'.pending'),dest=join(blobs,meta.sha256);
    try{
      let bytes=0;const digest=createHash('sha256');let prefix=Buffer.alloc(0);
      const count=new Transform({transform(chunk,_,done){bytes+=chunk.length;if(bytes>meta.bytes){done(fail(413,'Artifact exceeds declared length'));return;}
        if(prefix.length<64)prefix=Buffer.concat([prefix,chunk.subarray(0,64-prefix.length)]);digest.update(chunk);done(null,chunk);}});
      await pipeline(req,count,createWriteStream(temporary,{flags:'wx',mode:0o600}));
      if(bytes!==meta.bytes||digest.digest('hex')!==meta.sha256)throw fail(400,'Artifact length or SHA256 mismatch');
      if(meta.platform==='android'?prefix.readUInt32LE(0)!==0x04034b50:prefix.toString('ascii',0,2)!=='MZ')throw fail(400,'Artifact header does not match platform');
      const f=await open(temporary,'r+');try{await f.sync();}finally{await f.close();}
      // A privileged mutation of this exclusive trusted-writer data root is
      // outside the boundary. Open files with NOFOLLOW; never serve paths.
      try{await lstat(dest);throw fail(409,'Digest already stored; inspect publication');}catch(e){if(e.code!=='ENOENT')throw e;}
      await rename(temporary,dest);
      if(process.platform!=='win32'){const dir=await open(blobs,'r');try{await dir.sync();}finally{await dir.close();}}
      run('INSERT INTO releases VALUES(?,?,?,?,?)',meta.sha256,meta.platform,meta.build,JSON.stringify(meta),now());
      announce();return descriptor({metadata:JSON.stringify(meta)});
    }finally{publishing=false;try{await unlink(temporary);}catch(e){if(e.code!=='ENOENT')throw e;}}
  }
  async function packageRead(req,res,sha,name){
    const row=sql('SELECT * FROM releases WHERE sha=?',sha);if(!row)throw fail(404,'Package not found');
    const meta=JSON.parse(row.metadata),expected=meta.platform==='android'?'PakarController.apk':'PakarController.exe';
    if(name!==expected)throw fail(404,'Package not found');
    const f=await open(join(blobs,sha),constants.O_RDONLY|(constants.O_NOFOLLOW??0));
    try{
      const stat=await f.stat();if(!stat.isFile()||stat.size!==meta.bytes)throw fail(503,'Published artifact is unavailable');
      let first=0,last=stat.size-1,status=200;
      if(req.headers.range){const m=/^bytes=(\d+)-(\d*)$/.exec(req.headers.range);if(!m)throw fail(416,'Invalid byte range');
        first=Number(m[1]);last=m[2]?Number(m[2]):last;status=206;
        if(!Number.isSafeInteger(first)||!Number.isSafeInteger(last)||first>last||last>=stat.size)throw fail(416,'Invalid byte range');}
      const headers={'Content-Type':meta.platform==='android'?'application/vnd.android.package-archive':'application/octet-stream','Content-Length':last-first+1,
        ETag:'"'+sha+'"','Accept-Ranges':'bytes','X-Checksum-SHA256':sha,'Cache-Control':'public, max-age=31536000, immutable','X-Content-Type-Options':'nosniff'};
      if(status===206)headers['Content-Range']=`bytes ${first}-${last}/${stat.size}`;
      res.writeHead(status,headers);if(req.method==='HEAD'){res.end();return;}
      activeReads.set(sha,(activeReads.get(sha)??0)+1);
      try{await pipeline(f.createReadStream({start:first,end:last,autoClose:false}),res);}finally{
        const remaining=activeReads.get(sha)-1;if(remaining)activeReads.set(sha,remaining);else activeReads.delete(sha);
      }
    }finally{await f.close();}
  }
  async function cleanup(){
    if(cleaning)return {removed:0,interrupted_uploads:0,busy:true};cleaning=true;
    try{let removed=0,interrupted_uploads=0;
    for(const platform of platforms){const rows=all('SELECT * FROM releases WHERE platform=? ORDER BY build DESC',platform);
      for(const row of rows.slice(3))if(now()-row.published>7*86400000&&!activeReads.has(row.sha)){
        const path=join(blobs,row.sha),stat=await lstat(path);if(!stat.isFile()||stat.isSymbolicLink())continue;
        const digest=createHash('sha256');let size=0;for await(const bytes of createReadStream(path)){size+=bytes.length;digest.update(bytes);}
        if(size!==JSON.parse(row.metadata).bytes||digest.digest('hex')!==row.sha)continue;
        await unlink(path);run('DELETE FROM releases WHERE sha=?',row.sha);removed++;
      }
    }
    run('DELETE FROM commands WHERE expires<? AND created<?',now(),now()-30*86400000);
    run('DELETE FROM enrollments WHERE expires<?',now());
    // Only this service's old create-new upload names in its exclusive data
    // root. Never a registered package, active upload, link or arbitrary file.
    if(!publishing)for(const entry of (await readdir(blobs,{withFileTypes:true})).slice(0,256)){
      if(!entry.isFile()||!/^[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89ab][a-f0-9]{3}-[a-f0-9]{12}\.pending$/.test(entry.name))continue;
      const path=join(blobs,entry.name),stat=await lstat(path);
      if(stat.isFile()&&!stat.isSymbolicLink()&&stat.nlink===1&&stat.size<=maximum&&now()-stat.mtimeMs>86400000){await unlink(path);interrupted_uploads++;}
    }
    return {removed,interrupted_uploads,retained_per_platform:3};
    }finally{cleaning=false;}
  }
  const server=http.createServer(async(req,res)=>{
    for(const [key,value]of Object.entries(serviceHeaders()))res.setHeader(key,value);
    try{
      const path=new URL(req.url,'http://localhost').pathname.replace(/^\/pakar(?=\/|$)/,'')||'/';
      const origin=req.headers.origin;if(origin&&origin!==base.origin)throw fail(403,'Cross-origin request rejected');
      const key=(req.headers['x-forwarded-for']??req.socket.remoteAddress??'').toString().split(',').at(-1).trim();
      const rate=rates.get(key)||{at:now(),n:0};if(now()-rate.at>60000){rate.at=now();rate.n=0;}if(++rate.n>1200)throw fail(429,'Request rate limit');
      if(rates.size>256)rates.clear();rates.set(key,rate);
      if(req.method==='GET'&&(path==='/api/health'||path==='/api/peer/info'))return json(res,200,{...serviceIdentity(),ready:true,platforms:[...platforms],capabilities:['app-updates','client-management','peer-version-policy']});
      if(req.method==='GET'&&path==='/icon.png'&&iconPath){const bytes=Buffer.from((await readFile(iconPath,'utf8')).trim(),'base64');res.writeHead(200,{'Content-Type':'image/png','Content-Length':bytes.length,'Cache-Control':'public, max-age=3600'});return res.end(bytes);}
      if(req.method==='GET'&&path==='/'){
        const page=(await readFile(new URL('./dashboard.html',import.meta.url),'utf8')).replace('{{PEER_IDENTITY}}',JSON.stringify(serviceIdentity()));res.writeHead(200,{'Content-Type':'text/html; charset=utf-8','Cache-Control':'no-store',
          'Content-Security-Policy':"default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'",'Referrer-Policy':'no-referrer'});return res.end(page);}
      if(req.method==='GET'&&path==='/api/releases'){
        const p=new URL(req.url,'http://localhost').searchParams.get('platform');if(!platforms.has(p))throw fail(400,'Unsupported platform');return json(res,200,{releases:releases(p)});}
      if(req.method==='GET'&&path==='/api/app-updates')return json(res,200,offers(req.headers['x-pakar-app-session'],req.headers['x-pakar-app-package']));
      const artifact=/^\/packages\/([a-f0-9]{64})\/(PakarController\.(?:exe|apk))$/.exec(path);
      if(artifact&&['GET','HEAD'].includes(req.method))return await packageRead(req,res,artifact[1],artifact[2]);
      if(req.method==='POST'&&path==='/api/clients/enroll'){
        const b=await body(req),e=sql('SELECT * FROM enrollments WHERE hash=?',sha256(typeof b.enrollment_token==='string'?b.enrollment_token:''));
        if(!e||e.used||e.expires<=now()||!identity(b.client_id)||b.application!=='PakarController'||b.platform!==e.platform)throw fail(403,'Enrollment rejected');
        compatible(req);
        if(sql('SELECT id FROM clients WHERE id=?',b.client_id))throw fail(409,'Client identity already enrolled');
        if(sql('SELECT COUNT(*) AS n FROM clients').n>=100)throw fail(507,'Client registry full');
        const token=randomBytes(32).toString('base64url');db.exec('BEGIN IMMEDIATE');
        try{run('UPDATE enrollments SET used=1 WHERE hash=?',e.hash);run('INSERT INTO clients(id,platform,token_hash,created,last_seen) VALUES(?,?,?,?,?)',b.client_id,b.platform,sha256(token),now(),now());db.exec('COMMIT');}catch(e){db.exec('ROLLBACK');throw e;}
        return json(res,201,{client_id:b.client_id,token});
      }
      if(path.startsWith('/api/admin/')){
        admin(req);
        if(req.method==='POST'&&path==='/api/admin/enrollments'){
          const b=await body(req);if(!platforms.has(b.platform))throw fail(400,'Unsupported platform');
          const ttl=b.ttl_seconds??600;if(!Number.isInteger(ttl)||ttl<60||ttl>86400)throw fail(400,'Invalid enrollment lifetime');
          const token=randomBytes(32).toString('base64url');run('INSERT INTO enrollments(hash,platform,expires) VALUES(?,?,?)',sha256(token),b.platform,now()+ttl*1000);
          return json(res,201,{enrollment_token:token,platform:b.platform,expires_at_ms:now()+ttl*1000});}
        if(req.method==='GET'&&path==='/api/admin/clients')return json(res,200,{clients:all('SELECT id,platform,created,last_seen,telemetry,revoked FROM clients').map(c=>({...c,telemetry:JSON.parse(c.telemetry),online:!c.revoked&&now()-c.last_seen<120000}))});
        if(req.method==='GET'&&path==='/api/admin/commands')return json(res,200,{commands:all('SELECT id,client,action,created,expires,claimed,result FROM commands ORDER BY created DESC LIMIT 128').map(c=>({...c,result:c.result?JSON.parse(c.result):null}))});
        const match=/^\/api\/admin\/clients\/([a-zA-Z0-9_-]{16,80})\/(commands|revoke)$/.exec(path);
        if(req.method==='POST'&&match){
          const b=await body(req),c=sql('SELECT * FROM clients WHERE id=? AND revoked=0',match[1]);if(!c)throw fail(404,'Enrolled client not found');
          if(match[2]==='revoke'){run('UPDATE clients SET revoked=1 WHERE id=?',c.id);return json(res,200,{revoked:true});}
          if(!['check-update','cleanup','uninstall'].includes(b.action))throw fail(400,'Unsupported management action');
          if(b.action==='uninstall'&&(b.confirmation!=='uninstall PakarController'||JSON.parse(c.telemetry).allow_remote_uninstall!==true))throw fail(409,'Client has not enabled self-uninstall');
          if(sql('SELECT COUNT(*) AS n FROM commands WHERE client=? AND expires>? AND claimed=0',c.id,now()).n>=4)throw fail(409,'Client command queue full');
          const id=randomUUID().replaceAll('-','');run('INSERT INTO commands(id,client,action,created,expires) VALUES(?,?,?,?,?)',id,c.id,b.action,now(),now()+55000);
          return json(res,202,{id,action:b.action,expires_at_ms:now()+55000,state:'queued'});}
        if(req.method==='POST'&&path==='/api/admin/releases')return json(res,201,await publish(req));
        if(req.method==='POST'&&path==='/api/admin/cleanup'){await body(req);return json(res,200,await cleanup());}
      }else if(path==='/api/clients/report'||path.startsWith('/api/client/')){
        const c=client(req);
        const version=compatible(req);
        if(req.method==='POST'&&path==='/api/clients/report'){
          const raw=await body(req);if(version&&raw.version!==version)throw fail(409,'Reported client version differs from request identity');
          const local=req.socket.remoteAddress?.replace(/^::ffff:/,'');
          const proxied=(local==='127.0.0.1'||local==='::1')&&req.headers['x-forwarded-for']&&isIP(key);
          const data=telemetry(raw,c,{address:proxied?key:local,source:proxied?'loopback_reverse_proxy':'tcp_peer',agent:req.headers['user-agent'],application:req.headers['x-pakar-client'],session:req.headers['x-pakar-client-id']});
          run('UPDATE clients SET last_seen=?,telemetry=? WHERE id=?',now(),JSON.stringify(data),c.id);return json(res,200,{accepted:true,client_id:c.id,server_time_ms:now(),service:serviceIdentity()});}
        if(req.method==='GET'&&path==='/api/client/commands'){run('UPDATE clients SET last_seen=? WHERE id=?',now(),c.id);return json(res,200,{commands:all('SELECT id,action,expires AS expires_at_ms FROM commands WHERE client=? AND expires>? AND claimed=0 ORDER BY created LIMIT 4',c.id,now())});}
        const m=/^\/api\/client\/commands\/([a-f0-9]{32})\/(claim|result)$/.exec(path);
        if(req.method==='POST'&&m){
          const b=await body(req),command=sql('SELECT * FROM commands WHERE id=? AND client=?',m[1],c.id);if(!command)throw fail(404,'Command not found');
          if(m[2]==='claim'){
            const changed=run('UPDATE commands SET claimed=1 WHERE id=? AND client=? AND expires>? AND claimed=0',m[1],c.id,now()).changes;
            if(changed!==1)throw fail(409,'Command expired or already consumed');return json(res,200,{claimed:true,id:m[1]});}
          if(!command.claimed||!clean(b.state,48)||!clean(b.detail??'',256))throw fail(400,'Invalid claimed command result');
          const value=JSON.stringify({state:b.state,detail:b.detail??''});
          if(command.result&&command.result!==value)throw fail(409,'Result identity already consumed');
          if(!command.result)run('UPDATE commands SET result=? WHERE id=?',value,m[1]);return json(res,200,{accepted:true,id:m[1]});}
      }
      throw fail(404,'Not found');
    }catch(e){if(!res.headersSent)json(res,e.status??500,{error:e.status?e.message:'Service operation failed'});else res.destroy();}
  });
  server.requestTimeout=120000;server.headersTimeout=10000;server.keepAliveTimeout=5000;server.maxHeadersCount=32;
  server.on('upgrade',(req,socket,head)=>{
    const reject=code=>rejectHttpSocket(socket,code);
    let path;try{path=new URL(req.url,'http://localhost').pathname;}catch{reject(400);return;}
    if(!['/api/events','/pakar/api/events'].includes(path)){reject(404);return;}
    if(req.headers.origin&&req.headers.origin!==base.origin){reject(403);return;}
    if(sockets.clients.size>=64){reject(503);return;}
    // Public immutable update offers are recovery/bootstrap, not enrollment,
    // commands or private telemetry. Unsupported clients can fetch their update.
    sockets.handleUpgrade(req,socket,head,ws=>sockets.emit('connection',ws,req));
  });
  server.on('clientError',(_error,socket)=>rejectHttpSocket(socket,400));
  sockets.on('wsClientError',(_error,socket)=>rejectHttpSocket(socket,400));
  sockets.on('headers',headers=>{for(const [key,value]of Object.entries(serviceHeaders()))headers.push(key+': '+value);});
  sockets.on('connection',ws=>{
    ws.alive=true;ws.on('pong',()=>{ws.alive=true;});
    const timer=setTimeout(()=>{if(!ws.subscription)ws.close(1008);},8000);timer.unref();
    ws.on('message',wire=>{try{const s=JSON.parse(wire.toString());if(ws.subscription||s.type!=='subscribe'||!identity(s.session)||s.package!=='ir.digitalogic.pakar.pakarcontroller')throw Error();
      ws.subscription={session:s.session,package:s.package};clearTimeout(timer);for(const offer of offers(s.session,s.package).offers)ws.send(JSON.stringify(offer));}catch{ws.close(1008);}});
    ws.on('close',()=>clearTimeout(timer));ws.on('error',()=>{});
  });
  const heartbeat=setInterval(()=>{for(const ws of sockets.clients)if(ws.readyState===1){if(!ws.alive){ws.terminate();continue;}ws.alive=false;ws.ping();}},25000);heartbeat.unref();
  const maintenance=setInterval(()=>cleanup().catch(()=>{}),6*60*60*1000);maintenance.unref();
  async function close(){if(closed)return;closed=true;clearInterval(heartbeat);clearInterval(maintenance);for(const ws of sockets.clients)ws.terminate();await new Promise(r=>sockets.close(r));await new Promise(r=>server.close(r));db.close();}
  return {server,close,cleanup,releases};
}
if(process.argv[1]&&resolve(process.argv[1])===fileURLToPath(import.meta.url)){
  const app=await createService({directory:process.env.PAKAR_DATA,publicBase:process.env.PAKAR_PUBLIC_BASE,adminTokenHash:process.env.PAKAR_ADMIN_TOKEN_HASH,iconPath:process.env.PAKAR_ICON_FILE});
  const port=Number(process.env.PAKAR_PORT??18743);if(!Number.isInteger(port)||port<1024||port>65535)throw Error('Invalid listener port');
  app.server.listen(port,'127.0.0.1',()=>console.log('PakarController client management listening on loopback'));
  for(const signal of ['SIGTERM','SIGINT'])process.once(signal,async()=>{await app.close();process.exit(0);});
}
