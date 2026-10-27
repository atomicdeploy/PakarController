// No device contact. Exercise the real read-only verifier with bounded fixtures.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {mkdir,lstat,readFile,writeFile,unlink,readdir} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {verifyRunningFirmware,parseVerificationArguments,verificationRangeBytes} from '../tools/verify-running-firmware.mjs';
import {imageIdentity} from '../tools/update-firmware.mjs';
import {toolRelease} from '../tools/peer-identity.mjs';

const directory=resolve('build/native/verify-running-firmware-fixture');
await mkdir(directory,{recursive:true});
const before=new Set(await readdir(directory)),owned=[];
const absent=async path=>{try{await lstat(path);return false;}catch(error){if(error.code==='ENOENT')return true;throw error;}};
const image=Buffer.alloc(verificationRangeBytes*2+101,42);
image[0]=0xe9;image[1]=1;image.writeUInt16LE(9,12);image[23]=1;
const expected=imageIdentity(image),board='esp32s3-fixture',boot='a'.repeat(32),address='192.0.2.8';
const status={version:toolRelease.version,board_id:board,boot_id:boot,hostname:'fixture',http_peer_fencing:true,
  capabilities:{hostname:'fixture',acknowledged_features:['network']}};
let completed=0;
try{
  for(const scenario of ['success','retry','wrong-peer','wrong-header','wrong-boot','wrong-address','metadata','slot','hash',
    'range','length','section-hash','duplicate','encoding','http-reject','transport-twice','protocol-no-retry',
    'deadline','retry-deadline','ws-peer','final-slot','existing-receipt','receipt-race','file-success','file-directory']){
    const receipt=resolve(directory,scenario+'.json');assert(await absent(receipt),'Unknown fixture must be preserved: '+receipt);owned.push(receipt);
    if(scenario==='existing-receipt')await writeFile(receipt,'preserve',{flag:'wx'});
    let candidate=image;
    if(scenario==='file-success'){
      candidate=resolve(directory,'candidate.bin');assert(await absent(candidate));owned.push(candidate);
      await writeFile(candidate,image,{flag:'wx'});
    }else if(scenario==='file-directory')candidate=directory;
    let clock=0,requests=0,ranges=0,sockets=0,statusReads=0,telemetryReads=0,firstRangeAttempts=0;
    const events=[],calls=[];
    const request=async(_origin,path,options={})=>{
      requests++;calls.push({path,options});
      assert(['GET','HEAD'].includes(options.method),'No board write is permitted');assert.equal(options.form,undefined);
      assert.equal(options.headers['Accept-Encoding'],'identity');
      if(requests>1){assert.equal(options.expectedAddress,address);assert.equal(options.headers['X-Pakar-Board'],board);assert.equal(options.headers['X-Pakar-Boot'],boot);}
      const headers={'x-pakar-board':board,'x-pakar-boot':boot,'x-firmware-md5':expected.md5};
      const response=(value,extra={})=>({status:200,remote:address,headers,body:Buffer.from(JSON.stringify(value)),...extra});
      if(path==='/api/status'){
        statusReads++;
        return response({...status,board_id:scenario==='wrong-peer'?'another-board':board,
          boot_id:scenario==='wrong-boot'&&statusReads===2?'b'.repeat(32):boot});
      }
      if(path==='/update/info')return response({product:'PakarController',model:'esp32s3',size:expected.size,hash:scenario==='metadata'?'0'.repeat(32):expected.md5});
      if(path==='/api/telemetry'){
        telemetryReads++;
        const offset=scenario==='final-slot'&&telemetryReads===2?131072:65536;
        return response({board_id:board,boot_id:boot,esp:{sketch_bytes:expected.size,sketch_md5:expected.md5,boot_integrity_verified:true,
          ota:{running_offset:offset,boot_offset:offset,running_state_code:scenario==='slot'?1:2}}});
      }
      assert.equal(path,'/firmware/download','Only status/info/telemetry/firmware read routes are allowed');
      if(options.method==='HEAD')return response(null,{headers:{...headers,'content-length':String(expected.size)},body:Buffer.alloc(0)});
      ranges++;
      const match=/^bytes=(\d+)-(\d+)$/.exec(options.headers.Range);assert(match);
      const [start,end]=match.slice(1).map(Number);assert(end-start+1<=16384,'Readback chunks must not regress to65KiB');
      if(start===0){
        firstRangeAttempts++;
        if(['retry','transport-twice','retry-deadline'].includes(scenario)&&(firstRangeAttempts===1||scenario==='transport-twice')){
          if(scenario==='retry-deadline')clock=120001;
          throw Object.assign(Error('fixture network reset'),{code:'ECONNRESET'});
        }
        if(scenario==='protocol-no-retry')throw Error('Malformed fixture response');
      }
      if(scenario==='deadline')clock=120001;
      const chunk=Buffer.from(image.subarray(start,end+1));
      const digest=createHash('md5').update(chunk).digest('hex');
      if(scenario==='hash'||scenario==='section-hash')chunk[0]^=1;
      if(scenario==='receipt-race'&&end===image.length-1)await writeFile(receipt,'another owner',{flag:'wx'});
      return response(null,{status:scenario==='http-reject'?503:206,remote:scenario==='wrong-address'?'192.0.2.9':address,
        headers:{...headers,'x-pakar-boot':scenario==='wrong-header'?'b'.repeat(32):boot,
          'content-range':`bytes ${scenario==='range'?start+1:start}-${end}/${image.length}`,
          'content-length':String(chunk.length+(scenario==='length'?1:0)),
          ...(scenario==='hash'?{}:{'content-md5':digest}),...(scenario==='encoding'?{'content-encoding':'gzip'}:{})},
        ...(scenario==='duplicate'?{distinctHeaders:{'x-pakar-board':[board,board]}}:{}),body:chunk});
    };
    const websocket=async(_origin,peer,options)=>{
      sockets++;assert.equal(peer.board,board);assert.equal(peer.boot,boot);assert.equal(peer.address,address);
      assert(options.connectTimeoutMs+options.statusTimeoutMs<=120000-clock);
      return {board_id:scenario==='ws-peer'?'other':board,boot_id:boot,status_requests:1};
    };
    const invocation=()=>verifyRunningFirmware({origin:'http://fixture.local',board,image:candidate,receipt,request,websocket,now:()=>clock,log:event=>events.push(event)});
    if(['success','retry','file-success'].includes(scenario)){
      const result=await invocation();assert.equal(result.ok,true);assert.equal(result.read_only,true);assert.equal(result.firmware_written,false);
      assert.equal(result.avr_touched,false);assert.equal(result.simulated,true);assert.equal(result.image.sha256,expected.sha256);
      assert.equal(result.range_count,3);assert.equal(result.range_bytes,16384);assert.equal(result.transport_retries,scenario==='retry'?1:0);
      assert.equal(sockets,1);assert.equal(statusReads,2);assert.equal(telemetryReads,2);assert.deepEqual(JSON.parse(await readFile(receipt)),result);
      if(scenario==='retry')assert.equal(events.filter(e=>e.stage==='read-retry').length,1);
    }else{
      await assert.rejects(invocation());
      if(scenario==='existing-receipt'){assert.equal(requests,0);assert.equal((await readFile(receipt)).toString(),'preserve');}
      else if(scenario==='file-directory'){assert.equal(requests,0);assert(await absent(receipt));}
      else if(scenario==='receipt-race')assert.equal((await readFile(receipt)).toString(),'another owner');
      else assert(await absent(receipt),'Failure must not publish a successful receipt');
      if(scenario==='wrong-peer'){assert.equal(requests,1);assert.equal(sockets,0);}
      if(['hash','range','length','section-hash','duplicate','encoding','http-reject','protocol-no-retry','wrong-header','wrong-address'].includes(scenario))
        assert.equal(firstRangeAttempts,1,'Integrity/protocol rejection must never retry');
      if(scenario==='transport-twice')assert.equal(firstRangeAttempts,2,'At mostone transport retry');
      if(scenario==='retry-deadline')assert.equal(firstRangeAttempts,1,'No retry starts after the overall deadline');
      if(scenario==='deadline')assert.equal(ranges,1);
    }
    assert(calls.every(({path,options})=>!['/update','/api/firmware/apply','/api/firmware/fetch','/api/reset'].includes(path)&&['GET','HEAD'].includes(options.method)));
    completed++;
  }
  const valid=['--origin','http://fixture.local','--board',board,'--image','candidate.bin','--receipt','receipt.json'];
  assert.deepEqual(parseVerificationArguments(valid),{origin:'http://fixture.local',board,image:'candidate.bin',receipt:'receipt.json'});
  for(const args of [[],[...valid,'--run'],[...valid,'--mode','local'],[...valid,'--board',board],valid.slice(0,-1)])assert.throws(()=>parseVerificationArguments(args));
  const cli=spawnSync(process.execPath,['tools/verify-running-firmware.mjs',...valid,'--run'],{encoding:'utf8',timeout:10000,windowsHide:true});
  assert.equal(cli.status,1);assert.match(cli.stderr,/read-only/);assert.equal(cli.stdout,'');
  assert.deepEqual(await readFile(resolve(directory,'candidate.bin')),image,'The explicit input fixture is not rewritten');
  const created=(await readdir(directory)).filter(file=>!before.has(file)&&file!=='candidate.bin');
  assert(created.every(file=>file.endsWith('.json')),'Verifier must not create binary readback files');
}finally{for(const path of owned)if(!(await absent(path)))await unlink(path);}
console.log(`Read-only running firmware verifier: ${completed} peer/hash/range/deadline/receipt scenarios plus strict CLI passed; no board contact or flash.`);
