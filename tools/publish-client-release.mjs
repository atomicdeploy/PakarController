// Actual PU850 shared logging and explicit plan/run; PCController content-hash
// publication discipline. Operator supplies a verified package, never a URL.
import {createReadStream} from 'node:fs';
import {open,readFile,lstat} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import https from 'node:https';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
import {createLogger} from './script-console.mjs';
const log=createLogger(),opts={};
const root=fileURLToPath(new URL('../',import.meta.url));
if(process.platform==='win32'){for(const concern of ['client-management','pu-tooling']){const gate=spawnSync('pwsh',['-NoProfile','-File','tools/consult-reuse.ps1','-Concern',concern,'-Gate','-Summary'],{cwd:root,stdio:'inherit',windowsHide:true});if(gate.error||gate.status!==0)throw Error('Required reuse gate failed: '+concern);}}
for(let i=2;i<process.argv.length;i++){const key=process.argv[i];if(['--run','--plan'].includes(key))opts[key.slice(2)]=true;else if(['--file','--platform','--token-file','--base','--signer','--source'].includes(key)&&process.argv[i+1])opts[key.slice(2)]=process.argv[++i];else throw Error('Usage: publish-client-release --file FILE --platform android|windows-x64 --token-file FILE [--signer SHA256] [--base HTTPS] [--source COMMIT] --run|--plan');}
const identity=JSON.parse(await readFile(new URL('../interfaces/client-release.json',import.meta.url),'utf8'));
const base=new URL(opts.base??'https://office.digitalogic.ir/pakar');
if(base.protocol!=='https:'||base.username||base.password||base.search||base.hash||base.pathname!=='/pakar')throw Error('Use a direct HTTPS /pakar base');
if(!['android','windows-x64'].includes(opts.platform)||!opts.file)throw Error('Choose one self application artifact/platform');
const path=resolve(opts.file),stat=await lstat(path);if(!stat.isFile()||stat.isSymbolicLink()||stat.size<64||stat.size>128*1024*1024)throw Error('Bounded regular artifact required');
const f=await open(path,'r');let sha256;
try{const prefix=Buffer.alloc(4);await f.read(prefix,0,4,0);if(opts.platform==='android'?prefix.readUInt32LE(0)!==0x04034b50:prefix.toString('ascii',0,2)!=='MZ')throw Error('Artifact header/platform differs');
 const digest=createHash('sha256');for await(const b of f.createReadStream({start:0,autoClose:false}))digest.update(b);sha256=digest.digest('hex');}finally{await f.close();}
const metadata={...identity,platform:opts.platform,bytes:stat.size,sha256};
if(opts.source){if(!/^[a-f0-9]{40}$/.test(opts.source))throw Error('Exact source commit required');metadata.source_sha=opts.source;}
if(opts.platform==='android'){if(!/^[a-f0-9]{64}$/.test(opts.signer??''))throw Error('Supply actual apksigner-verified signer SHA256');metadata.package='ir.digitalogic.pakar.pakarcontroller';metadata.signers=[opts.signer];}
log.info(`Publish ${metadata.platform} ${identity.version} build ${identity.build} · ${stat.size} bytes`);log.detail(`SHA256 ${sha256}`);
if(!opts.run){log.result('Plan only. Confirm installed identity/signature and use --run; no file uploaded.');process.exit(0);}
if(!opts['token-file'])throw Error('Root/operator token file required; never put credentials in command-line values');
const token=(await readFile(resolve(opts['token-file']),'utf8')).trim();if(!/^[A-Za-z0-9_-]{43}$/.test(token))throw Error('Invalid operator token file');
const result=await new Promise((done,reject)=>{
 const request=https.request(new URL(base.href+'/api/admin/releases'),{method:'POST',headers:{Authorization:'Bearer '+token,'Content-Type':'application/octet-stream','Content-Length':String(stat.size),'X-Pakar-Release':Buffer.from(JSON.stringify(metadata)).toString('base64')},timeout:120000},response=>{
   const chunks=[];let bytes=0;response.on('data',b=>{bytes+=b.length;if(bytes>16384){response.destroy();reject(Error('Publication receipt too large'));}else chunks.push(b);});response.on('error',reject);response.on('end',()=>{if(response.statusCode!==201)return reject(Error('Publication rejected: HTTP '+response.statusCode+'; not retried'));try{done(JSON.parse(Buffer.concat(chunks).toString('utf8')));}catch{reject(Error('Invalid publication receipt'));}});
 });request.on('timeout',()=>request.destroy(Error('Publication deadline; outcome unknown, inspect before retry')));request.on('error',reject);
 const stream=createReadStream(path);stream.on('error',e=>request.destroy(e));stream.pipe(request);
});
if(result.sha256!==sha256||result.bytes!==stat.size||result.build!==identity.build)throw Error('Publication receipt identity differs');
log.result(`Published ${result.url}`);
