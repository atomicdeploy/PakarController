// Ported actual PU850 Go download tests: verified bytes, exact resume and
// preserve destination/partial on mismatch. No donor build or device contact.
import assert from 'node:assert/strict';
import {mkdir,writeFile,readFile,lstat,unlink} from 'node:fs/promises';
import {resolve} from 'node:path';
import {createHash} from 'node:crypto';
import {createServer} from 'node:http';
import {downloadFirmware,parseContentMD5,parseDigestMD5,validateDownloadResponse} from '../tools/download-firmware.mjs';
import {toolRelease} from '../tools/peer-identity.mjs';
import {deviceRequest} from '../tools/check-ethernet-live.mjs';
const fixture=resolve('build/native/download-firmware-fixture');await mkdir(fixture,{recursive:true});
const firmware=Buffer.alloc(140000);for(let i=0;i<firmware.length;i++)firmware[i]=i%256;
const md5=createHash('md5').update(firmware).digest('hex'),sha=createHash('sha256').update(firmware).digest('hex');
const peer={board:'esp32s3-fixture',boot:'a'.repeat(32),address:'192.0.2.7'};
const status={version:toolRelease.version,board_id:peer.board,boot_id:peer.boot,hostname:'fixture',http_peer_fencing:true,
  capabilities:{hostname:'fixture',acknowledged_features:['network']}};
const identity={'x-pakar-board':peer.board,'x-pakar-boot':peer.boot,'x-firmware-md5':md5,etag:`"${md5}"`,digest:'md5='+Buffer.from(md5,'hex').toString('base64')};
const absent=async path=>{try{await lstat(path);return false}catch(e){if(e.code==='ENOENT')return true;throw e;}};
const owned=new Set();
async function reserve(name){const output=resolve(fixture,name+'.bin');for(const p of [output,output+'.part'])assert(await absent(p),'Unexpected retained fixture; inspect '+p);owned.add(output);owned.add(output+'.part');return output;}
assert.equal(parseContentMD5(Buffer.from(md5,'hex').toString('base64')),md5);
assert.equal(parseContentMD5(md5.toUpperCase()),md5);
assert.equal(parseDigestMD5(identity.digest),md5);
assert.throws(()=>parseDigestMD5('md5='+md5));
assert.throws(()=>parseDigestMD5('sha-256='+Buffer.from(md5,'hex').toString('base64')));
for(const bad of ['','x',Buffer.from(md5,'hex').toString('base64')+'x',' '.repeat(32)])assert.throws(()=>parseContentMD5(bad));
assert.throws(()=>validateDownloadResponse({status:200,headers:{},body:firmware},0,1,2));
// Actual transport must preserve duplicates that Node's convenience header map
// may discard; C++ single_header's check needs the original value cardinality.
const transport=createServer((_request,response)=>{response.setHeader('ETag',[`"${md5}"`,`"${md5}"`]);response.end('{}');});
await new Promise(resolve=>transport.listen(0,'127.0.0.1',resolve));
try {const observed=await deviceRequest(new URL('http://localhost:'+transport.address().port),'/fixture',{expectedAddress:'127.0.0.1'});
  assert.equal(observed.distinctHeaders.etag.length,2);
} finally {await new Promise(resolve=>transport.close(resolve));}
try {
  for(const scenario of ['success','uppercase','basic','resume','complete','bad-range','bad-section','bad-whole','bad-metadata','changed-peer','changed-version','changed-final','changed-part','bad-head','bad-head-hash','bad-digest','duplicate-digest','duplicate-boot','bad-etag','bad-whole-header','bad-encoding','deadline','cancelled','destination-race']) {
    const output=await reserve(scenario);let ranges=0,reads=0,statusReads=0;const controller=new AbortController();
    if(['resume','complete'].includes(scenario))await writeFile(output+'.part',firmware.subarray(0,scenario==='resume'?13:firmware.length),{flag:'wx'});
    const request=async(url,path,options)=>{
      reads++;assert.equal(url.hostname,'fixture.local');assert.ok(!options.form);assert(['GET','HEAD'].includes(options.method??'GET'));
      if(scenario==='basic')assert.equal(options.headers.Authorization,'Basic '+Buffer.from('fixture:fixture-password').toString('base64'));
      if(reads>1){assert.equal(options.expectedAddress,peer.address);assert.equal(options.headers['X-Pakar-Board'],peer.board);assert.equal(options.headers['X-Pakar-Boot'],peer.boot);}
      if(path==='/api/status'){
        if(scenario==='changed-part'&&statusReads===1)await writeFile(output+'.part',Buffer.alloc(firmware.length));
        statusReads++;const value={...status,...(scenario==='changed-version'?{version:'0.0.0'}:{}),...(scenario==='changed-final'&&statusReads>1?{boot_id:'b'.repeat(32)}:{})};
        return {status:200,headers:identity,remote:peer.address,body:Buffer.from(JSON.stringify(value))};
      }
      if(path==='/update/info')return {status:200,headers:identity,remote:peer.address,
        body:Buffer.from(JSON.stringify({product:'PakarController',hash:scenario==='bad-metadata'?'invalid':md5,size:firmware.length}))};
      if(options.method==='HEAD')return {status:200,headers:{...identity,...(scenario==='uppercase'?{'x-firmware-md5':md5.toUpperCase(),etag:`"${md5.toUpperCase()}"`}:{}),...(scenario==='bad-head-hash'?{'content-md5':'0'.repeat(32)}:{}),...(scenario==='bad-digest'?{digest:'md5='+Buffer.alloc(16).toString('base64')}:{}),'content-length':String(firmware.length+(scenario==='bad-head'?1:0))},distinctHeaders:scenario==='duplicate-digest'?{digest:[identity.digest,identity.digest]}:scenario==='duplicate-boot'?{'x-pakar-boot':[peer.boot,peer.boot]}:{},remote:peer.address,body:Buffer.alloc(0)};
      assert.equal(path,'/firmware/download');ranges++;
      const match=/^bytes=(\d+)-(\d+)$/.exec(options.headers.Range);assert(match);const [start,end]=match.slice(1).map(Number);
      assert.equal(options.headers['Accept-Encoding'],'identity');
      if(scenario==='resume'&&ranges===1)assert.equal(start,13);
      let body=Buffer.from(firmware.subarray(start,end+1));if(scenario==='bad-whole'&&ranges===1)body[0]^=1;
      const headers={...identity,'content-length':String(body.length),'content-range':`bytes ${start}-${end}/${firmware.length}`,
        'content-md5':createHash('md5').update(body).digest('base64')};
      if(scenario==='bad-range')headers['content-range']='bytes 0-0/1';
      if(scenario==='bad-section')headers['content-md5']='0'.repeat(32);
      if(scenario==='bad-etag')headers.etag='"'+'0'.repeat(32)+'"';
      if(scenario==='bad-whole-header')headers['x-firmware-md5']='0'.repeat(32);
      if(scenario==='bad-encoding')headers['content-encoding']='gzip';
      if(scenario==='changed-peer')headers['x-pakar-boot']='b'.repeat(32);
      if(scenario==='cancelled')controller.abort();
      if(scenario==='destination-race'&&ranges===1)await writeFile(output,Buffer.from('keep existing backup'),{flag:'wx'});
      return {status:206,headers,remote:peer.address,body};
    };
    const operation=downloadFirmware({origin:'http://fixture.local',output,resume:['resume','complete'].includes(scenario),run:true,
      request,environment:scenario==='basic'?{PAKAR_DOWNLOAD_USERNAME:'fixture',PAKAR_DOWNLOAD_PASSWORD:'fixture-password'}:{},signal:controller.signal,...(scenario==='deadline'?{now:()=>++reads*100000}: {})});
    if(['success','uppercase','basic','resume','complete'].includes(scenario)){
      const result=await operation;assert.equal(result.sha256,sha);assert.equal(result.md5,md5);assert.equal(result.firmware_written,false);
      assert.deepEqual(await readFile(output),firmware);assert(await absent(output+'.part'));
      assert.equal(ranges,scenario==='complete'?0:3);assert.equal(statusReads,2);
    }else{
      await assert.rejects(operation);assert(reads<12,'No replay loop');
      if(scenario==='destination-race')assert.equal((await readFile(output)).toString(),'keep existing backup');
      else assert(await absent(output),'Failed download cannot publish output');
      if(scenario==='bad-whole')assert.equal((await readFile(output+'.part')).length,firmware.length);
    }
  }
  const plan=await reserve('plan');let requests=0;
  assert.equal((await downloadFirmware({output:plan,request:async()=>{requests++;throw Error('No network in plan');}})).device_contacted,false);
  assert.equal(requests,0);assert(await absent(plan+'.part'));
  const keep=await reserve('preserve');await writeFile(keep,'keep');
  await assert.rejects(downloadFirmware({output:keep,run:true,request:async()=>{requests++;}}),/already exists/);assert.equal(requests,0);
  assert.equal((await readFile(keep)).toString(),'keep');
  const partial=await reserve('unowned-part');await writeFile(partial+'.part','keep partial');
  await assert.rejects(downloadFirmware({output:partial,run:true,request:async()=>{requests++;}}),/explicitly resume/);assert.equal(requests,0);
  assert.equal((await readFile(partial+'.part')).toString(),'keep partial');
  console.log('PU850 download source: exact multi-range/resume/full hash, all advertised integrity, same-peer/release/deadline/cancel, no replay, atomic no-overwrite and preserved failures passed with local fixtures. No device writes.');
} finally {
  // Only exact tiny paths reserved by this fixture; unknown files fail above.
  for(const path of owned)if(!await absent(path))await unlink(path);
}
