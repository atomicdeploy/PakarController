// Explicit maintenance bootstrap, not a version-specific API adapter. All
// normal controls and the new boot retain supported-release admission.
import assert from 'node:assert/strict';
import {boardPeer,toolRelease} from '../tools/peer-identity.mjs';
import {updateFirmware,imageIdentity,uploadImage} from '../tools/update-firmware.mjs';
import {createServer} from 'node:http';
import {once} from 'node:events';
const board='esp32s3-recovery-fixture',boot='a'.repeat(32),address='192.0.2.20',sha='1'.repeat(64);
const old={version:'0.2.7',board_id:board,boot_id:boot,hostname:'fixture',http_peer_fencing:true,programming_latch:false,
  capabilities:{hostname:'fixture',acknowledged_features:['network']}};
assert.throws(()=>boardPeer(old,address),/Unsupported/);
assert.equal(boardPeer(old,address,null,{maintenanceBoard:board}).board,board);
for(const status of [{...old,board_id:'other'},{...old,version:'unknown'},{...old,http_peer_fencing:false}])
  assert.throws(()=>boardPeer(status,address,null,{maintenanceBoard:board}));
const image=Buffer.alloc(96);image[0]=0xe9;image[1]=1;image.writeUInt16LE(9,12);image[23]=1;
const identity=imageIdentity(image);
for(const scenario of ['success','no-recovery','wrong-board','unsupported-new-boot','wrong-manifest','missing-pin','partial-transfer']){
  let rebooted=false,writes=0,sockets=0,clock=0;
  const value=x=>({status:200,remote:address,headers:{},body:Buffer.from(JSON.stringify(x))});
  const request=async(_origin,path)=>{
    if(path==='/api/status')return value({...old,boot_id:rebooted?'b'.repeat(32):boot,
      version:rebooted&&scenario!=='unsupported-new-boot'?toolRelease.version:old.version});
    if(path==='/api/auth')return value({enabled:false});
    if(path==='/')return {...value(null),body:Buffer.from('<html>actual owner fixture</html>')};
    if(path==='/update/info')return value({product:'PakarController',framework:'ESPAsyncWebServer',model:'esp32s3',ota_capacity_bytes:0x330000,upload:'/update'});
    if(path==='/api/firmware/remote')return value({job_id:21,boot_id:boot,targets:1,phase:'failed',busy:false,cancellable:false,
      received_bytes:scenario==='partial-transfer'?1:0,expected_bytes:0,manifest_sha256:scenario==='wrong-manifest'?'2'.repeat(64):sha,
      error:'Download TLS allocation failed',esp:{size:0,sha256:'',version:''}});
    if(path==='/api/telemetry')return value({board_id:board,boot_id:'b'.repeat(32),esp:{sketch_bytes:image.length,sketch_md5:identity.md5,
      boot_integrity_verified:true,ota:{running_offset:0x340000,boot_offset:0x340000,running_state_code:2}}});
    if(path==='/firmware/download')return {status:206,remote:address,headers:{'content-range':`bytes 0-${image.length-1}/${image.length}`},body:image};
    throw Error('Unexpected maintenance path '+path);
  };
  const options={image,mode:'local',run:true,ackFailedJobId:21,
    ...(scenario==='missing-pin'?{}:{manifest:'https://publisher.invalid/manifest.json',manifestSha:sha}),
    recoveryBoard:scenario==='no-recovery'?null:scenario==='wrong-board'?'other':board,
    request,upload:async()=>{writes++;rebooted=true},websocket:async(_url,_peer,options)=>{
      assert.equal(options?.maintenanceBoard,sockets++===0?board:undefined);
    },wait:async ms=>{clock+=ms},now:()=>clock,timeoutSeconds:10};
  if(scenario==='success'){
    const result=await updateFirmware(options);assert.equal(result.ok,true);assert.equal(result.recovery_board,board);
    assert.equal(result.acknowledged_failed_job.esp.size,0);assert.equal(result.acknowledged_failed_job.esp.sha256,'');
    assert.equal(writes,1);assert.equal(sockets,2);
  }else{await assert.rejects(updateFirmware(options));assert.equal(writes,scenario==='unsupported-new-boot'?1:0)}
}
// Actual multipart wire, >four producer chunks, with explicit bounded pacing.
const large=Buffer.alloc(70000,42);large[0]=0xe9;large[1]=1;large.writeUInt16LE(9,12);large[23]=1;
const server=createServer(async(req,res)=>{
  const chunks=[];for await(const chunk of req)chunks.push(chunk);const body=Buffer.concat(chunks);
  assert.equal(body.length,Number(req.headers['content-length']));assert(body.includes(large));
  assert.equal(req.headers['x-pakar-firmware-md5'],imageIdentity(large).md5);
  res.writeHead(200,{'Content-Type':'application/json'});res.end('{"updated":true,"rebooting":true}');
});
server.listen(0,'127.0.0.1');await once(server,'listening');
try{
  const origin=new URL('http://localhost:'+server.address().port),peer={board,boot,address:'127.0.0.1'};
  assert.throws(()=>uploadImage(origin,peer,large,imageIdentity(large),{uploadRate:-1}),/rate/);
  assert.equal((await uploadImage(origin,peer,large,imageIdentity(large),{timeoutSeconds:10,uploadRate:16777216})).updated,true);
}finally{server.close();await once(server,'close')}
console.log('Explicit exact-board maintenance, default release rejection, zero-byte manifest failure and mandatory supported new boot passed; no device contacted.');
