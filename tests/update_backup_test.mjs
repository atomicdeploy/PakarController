// Actual donor backup-before-upload orchestration + actual Pakar downloader.
// Fixture requests only; never contact a board or run a real updater.
import assert from 'node:assert/strict';
import {mkdir,writeFile,readFile,lstat,unlink} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {updateFirmware,imageIdentity} from '../tools/update-firmware.mjs';
import {toolRelease} from '../tools/peer-identity.mjs';
const directory=resolve('build/native/update-backup-fixture');await mkdir(directory,{recursive:true});
const absent=async p=>{try{await lstat(p);return false}catch(error){if(error.code==='ENOENT')return true;throw error;}};
const owned=new Set();
const image=Buffer.alloc(96);image[0]=0xe9;image[1]=1;image.writeUInt16LE(9,12);image[23]=1;
const next=imageIdentity(image),old=Buffer.alloc(140000,42),md5=createHash('md5').update(old).digest('hex'),manifestSha='1'.repeat(64);
const peer={board:'esp32s3-fixture',boot:'a'.repeat(32),address:'192.0.2.8'};
const base={version:toolRelease.version,board_id:peer.board,boot_id:peer.boot,hostname:'fixture',
  http_peer_fencing:true,programming_latch:false,capabilities:{hostname:'fixture',acknowledged_features:['network']}};
try{
 for(const scenario of ['success','recovery-noheaders','resume','complete','remote','remote-resume','bad-section','bad-section-remote','bad-peer','bad-final-boot','metadata-changed',
   'auth-changed','job-changed','peer-after-backup','old-release','deadline','destination-race','existing-output','existing-part','plan']){
  const output=resolve(directory,scenario+'.bin'),part=output+'.part';
  for(const p of [output,part]){assert(await absent(p),'Unknown retained fixture; inspect '+p);owned.add(p);}
  if(['resume','complete','existing-part'].includes(scenario))await writeFile(part,old.subarray(0,scenario==='complete'?old.length:13),{flag:'wx'});
  if(scenario==='existing-output')await writeFile(output,'preserved',{flag:'wx'});
  const remote=['remote','remote-resume','bad-section-remote'].includes(scenario),resumeJob=scenario==='remote-resume';
  let requests=0,statusReads=0,infoReads=0,authReads=0,jobReads=0,ranges=0,mutations=0,fetches=0,sockets=0,clock=0,rebooted=false;
  const events=[];
  const request=async(_origin,path,options={})=>{
   requests++;const boot=rebooted?'b'.repeat(32):peer.boot;
   const headers=scenario==='recovery-noheaders'?{'x-firmware-md5':md5}:{'x-pakar-board':peer.board,'x-pakar-boot':boot,'x-firmware-md5':md5};
   const response=(value,extra={})=>({status:200,remote:peer.address,headers,body:Buffer.from(JSON.stringify(value)),...extra});
   if(path==='/api/status'){
    statusReads++;
    if(statusReads===2){assert.equal(options.expectedAddress,peer.address);assert.equal(options.headers['X-Pakar-Board'],peer.board);assert.equal(options.headers['X-Pakar-Boot'],peer.boot);}
    if(scenario==='destination-race'&&statusReads===3)await writeFile(output,'occupied fixture',{flag:'wx'});
    return response({...base,version:!rebooted&&scenario==='recovery-noheaders'||scenario==='old-release'&&statusReads===2?'0.2.7':base.version,
      board_id:(scenario==='bad-peer'&&statusReads===2)||(scenario==='peer-after-backup'&&statusReads===4)?'another-board':peer.board,
      boot_id:scenario==='bad-final-boot'&&statusReads===3?'c'.repeat(32):boot});
   }
   if(path==='/api/auth'){authReads++;return response({enabled:scenario==='auth-changed'&&authReads===2});}
   if(path==='/')return response(null,{body:Buffer.from('<!doctype html><html>fixture</html>')});
   if(path==='/update/info'){
    infoReads++;if(infoReads===2){assert.equal(options.expectedAddress,peer.address);assert.equal(options.headers['X-Pakar-Board'],peer.board);assert.equal(options.headers['X-Pakar-Boot'],peer.boot);assert.equal(options.headers.Authorization,undefined);}
    return response({product:'PakarController',framework:'ESPAsyncWebServer',model:'esp32s3',ota_capacity_bytes:1000000,
      upload:'/update',remote_fetch:'/api/firmware/fetch',remote_apply:'/api/firmware/apply',size:old.length,hash:scenario==='metadata-changed'&&infoReads===1?'0'.repeat(32):md5});
   }
   if(path==='/api/firmware/remote'){
    jobReads++;return response(fetches||resumeJob?{job_id:18,boot_id:boot,phase:rebooted?'complete':'ready',busy:false,
      manifest_sha256:manifestSha,targets:1,esp:{size:image.length,sha256:next.sha256}}:
      {phase:scenario==='job-changed'&&jobReads>1?'ready':'idle',busy:false});
   }
   if(path==='/api/firmware/fetch'||path==='/api/firmware/apply'){
    assert.deepEqual(await readFile(output),old,'Backup must precede even remote staging');assert(events.some(e=>e.stage==='backup-verified'));
    if(path.endsWith('/fetch'))fetches++;else{mutations++;rebooted=true;}
    return response({job_id:18,boot_id:peer.boot});
   }
   if(path==='/firmware/download'){
    if(rebooted)return response(null,{status:206,headers:{...headers,'content-range':`bytes 0-${image.length-1}/${image.length}`},body:image});
    if(options.method==='HEAD'){if(scenario==='deadline')clock=10001;return response(null,{headers:{...headers,'content-length':String(old.length)},body:Buffer.alloc(0)});}
    ranges++;const match=/^bytes=(\d+)-(\d+)$/.exec(options.headers.Range);assert(match);
    const [start,end]=match.slice(1).map(Number),bytes=Buffer.from(old.subarray(start,end+1));
    const digest=createHash('md5').update(bytes).digest('hex');if(scenario.startsWith('bad-section'))bytes[0]^=1;
    return response(null,{status:206,headers:{...headers,'content-range':`bytes ${start}-${end}/${old.length}`,
      'content-length':String(bytes.length),'content-md5':digest},body:bytes});
   }
   if(path==='/api/telemetry')return response({board_id:peer.board,boot_id:boot,esp:{sketch_bytes:image.length,
    sketch_md5:next.md5,boot_integrity_verified:true,ota:{running_offset:65536,boot_offset:65536,running_state_code:2}}});
   throw Error('Unexpected fixture request '+path);
  };
  const options={image,origin:'http://fixture.local',backup:output,resumeBackup:['resume','complete'].includes(scenario),
    ...(scenario==='recovery-noheaders'?{recoveryBoard:peer.board}:{}),
   ...(remote?{mode:'remote',manifest:'https://fixture.invalid/manifest.json',manifestSha}:{}),...(resumeJob?{resumeJobId:18}:{}),now:()=>clock,
   run:scenario!=='plan',request,websocket:async()=>{sockets++;},wait:async()=>{},timeoutSeconds:10,log:event=>events.push(event),
   upload:async(_origin,admitted)=>{mutations++;assert.equal(admitted.boot,peer.boot);
    assert.deepEqual(await readFile(output),old,'Verified backup must exist BEFORE the one write');
    assert(events.some(e=>e.stage==='backup-verified'));rebooted=true;}};
  if(['success','recovery-noheaders','resume','complete','remote','remote-resume'].includes(scenario)){
   const result=await updateFirmware(options);assert.equal(result.ok,true);assert.equal(mutations,1);assert.equal(sockets,2);
   assert.equal(result.backup.output,output);assert.equal(result.backup.md5,md5);assert.equal(result.backup.boot_id,peer.boot);
   assert.equal(result.backup.firmware_written,false);assert.equal(result.backup.resumed_bytes,scenario==='resume'?13:scenario==='complete'?old.length:0);
   assert.equal(ranges,scenario==='complete'?0:3);assert.equal(authReads,3);assert.equal(await absent(part),true);
   assert.equal(fetches,scenario==='remote'?1:0);assert.equal(result.path,remote?'remote':'local');
  }else if(scenario==='plan'){
   const result=await updateFirmware(options);assert.equal(result.device_contacted,false);assert.equal(result.backup.device_contacted,false);
   assert.equal(result.backup.output,output);assert.equal(requests,0);assert.equal(sockets,0);assert.equal(await absent(part),true);
  }else{
   await assert.rejects(updateFirmware(options));assert.equal(mutations,0,'No failed backup may permit a firmware write');assert.equal(fetches,0);
   if(['existing-output','existing-part'].includes(scenario)){assert.equal(requests,0);assert.equal(sockets,0);}
   if(scenario==='existing-output')assert.equal((await readFile(output)).toString(),'preserved');
   if(scenario==='destination-race'){assert.equal((await readFile(output)).toString(),'occupied fixture');assert.deepEqual(await readFile(part),old);}
   if(['auth-changed','job-changed','peer-after-backup','metadata-changed'].includes(scenario))assert.deepEqual(await readFile(output),old,'Verified backup retained when later admission fails');
  }
 }
 await assert.rejects(updateFirmware({image,resumeBackup:true}),/explicit backup path/);
 await assert.rejects(updateFirmware({image,backup:''}),/explicit new output/);
 const input=resolve(directory,'cli-input.bin'),cliOutput=resolve(directory,'cli-backup.bin');
 for(const p of [input,cliOutput]){assert(await absent(p),'Unknown retained CLI fixture; inspect '+p);owned.add(p);}
 await writeFile(input,image,{flag:'wx'});
 const cli=spawnSync(process.execPath,['tools/update-firmware.mjs','--image',input,'--origin','http://localhost:1','--backup',cliOutput],
  {encoding:'utf8',timeout:10000,windowsHide:true});
 assert.equal(cli.status,0,cli.stderr);assert.equal(JSON.parse(cli.stdout).device_contacted,false);
 assert.equal(JSON.parse(cli.stdout).backup.output,cliOutput);assert.equal(await absent(cliOutput),true);
 const wrapper=spawnSync('pwsh',['-NoProfile','-File','tools/update-firmware.ps1','-Image',input,
  '-Origin','http://localhost:1','-Backup',cliOutput],{encoding:'utf8',timeout:45000,windowsHide:true});
 assert.equal(wrapper.status,0,wrapper.stderr);assert.equal(JSON.parse(wrapper.stdout).device_contacted,false);
 assert.equal(JSON.parse(wrapper.stdout).backup.output,cliOutput);assert.equal(await absent(cliOutput),true);
}finally{for(const p of owned)if(!(await absent(p)))await unlink(p);}
console.log('Actual verified backup precedes one upload; plan/resume/peer/boot/hash/auth/destination failures preserve data and permit zero writes. Fixture-only, not OTA acceptance.');
