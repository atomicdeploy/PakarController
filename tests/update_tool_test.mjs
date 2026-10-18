// HTTP/WS doubles and a loopback wire server, never a physical flash test.
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {createHash} from 'node:crypto';
import {once} from 'node:events';
import {readFileSync} from 'node:fs';
import {boardPeer,imageIdentity,selectPath,updateFirmware,uploadImage,checkWebSocket} from '../tools/update-firmware.mjs';
const image=Buffer.alloc(96);image[0]=0xe9;image[1]=1;image.writeUInt16LE(9,12);image[23]=1;
const identity=imageIdentity(image),manifestSha='1'.repeat(64),address='192.168.50.20';
const base={board_id:'esp32s3-fixture',boot_id:'a'.repeat(32),http_peer_fencing:true,hostname:'ledboard',programming_latch:false,
  capabilities:{hostname:'ledboard',acknowledged_features:['network']}};
assert.equal(selectPath('auto',{upload:'/update'},true),'local');
assert.equal(selectPath('auto',{remote_fetch:'/api/firmware/fetch',remote_apply:'/api/firmware/apply'},true),'remote');
assert.throws(()=>selectPath('auto',{},false));
assert.throws(()=>boardPeer({...base,boot_id:'0'.repeat(32)},address));
assert.throws(()=>boardPeer(base,address,{board:base.board_id,boot:base.boot_id,address:'192.168.4.1'}));
assert.throws(()=>imageIdentity(Buffer.alloc(96)));
const plan=await updateFirmware({image});assert.equal(plan.device_contacted,false);assert.equal(plan.mode,'auto');
for(const scenario of ['local','remote','auth','unknown-auth','bad-product','busy','latch','lost-upload','lost-fetch','wrong-stage','reboot-peer','bad-range','bad-sha','bad-ws','wrong-boot-headers','existing-capacity','dhcp']) {
  let clock=0,rebooted=false,writes=0,fetches=0,applies=0,ws=0;const calls=[];
  const request=async(_url,path,options={})=>{
    calls.push({path,options});if(rebooted&&options.headers?.['X-Pakar-Boot']===base.boot_id)throw Error('old boot fenced');
    if(rebooted&&scenario==='dhcp'&&options.expectedAddress===address)throw Error('cached address unreachable');
    const remoteAddress=rebooted&&scenario==='dhcp'?'192.168.50.21':address;
    const value=v=>({status:200,remote:remoteAddress,headers:{},body:Buffer.from(JSON.stringify(v))});
    if(path==='/api/status')return value({...base,board_id:rebooted&&scenario==='reboot-peer'?'other':base.board_id,boot_id:rebooted?'b'.repeat(32):base.boot_id});
    if(path==='/api/auth')return value(scenario==='unknown-auth'?{}:{enabled:scenario==='auth'});
    if(path==='/')return {status:200,remote:address,headers:{},body:Buffer.from('<!doctype html><html>fixture</html>')};
    if(path==='/update/info')return value({product:scenario==='bad-product'?'other':'PakarController',framework:'ESPAsyncWebServer',model:'esp32s3 + ATmega328P',...(scenario==='existing-capacity'?{free:0x330000}:{ota_capacity_bytes:0x330000}),
      upload:'/update',remote_fetch:'/api/firmware/fetch',remote_apply:'/api/firmware/apply'});
    if(path==='/api/telemetry')return value({board_id:base.board_id,boot_id:'b'.repeat(32),esp:{sketch_bytes:image.length,sketch_md5:identity.md5,boot_integrity_verified:true,ota:{running_offset:65536,boot_offset:65536,running_state_code:2}}});
    if(path==='/firmware/download')return {status:scenario==='bad-range'?200:206,remote:address,headers:{'content-range':`bytes 0-${image.length-1}/${image.length}`},body:scenario==='bad-sha'?Buffer.alloc(image.length):image};
    if(path==='/api/firmware/fetch'){fetches++;writes++;assert.equal(options.form.apply,'false');assert.equal(options.form.targets,'1');if(scenario==='lost-fetch')throw Error('lost fetch');return value({job_id:7,boot_id:base.boot_id});}
    if(path==='/api/firmware/apply'){applies++;writes++;rebooted=true;return value({job_id:7,boot_id:base.boot_id});}
    if(path==='/api/firmware/remote')return value(fetches?{job_id:7,boot_id:rebooted?'b'.repeat(32):base.boot_id,manifest_sha256:manifestSha,targets:1,busy:false,
      phase:rebooted?'complete':'ready',esp:{size:image.length,sha256:scenario==='wrong-stage'?'2'.repeat(64):identity.sha256}}:
      {busy:scenario==='busy',phase:'idle'});
    throw Error('unexpected path '+path);
  };
  const upload=async(_origin,peer,bytes,hash)=>{writes++;assert.equal(peer.boot,base.boot_id);assert.deepEqual(bytes,image);assert.equal(hash.sha256,identity.sha256);
    if(scenario==='lost-upload')throw Error('outcome unknown');rebooted=true;};
  const websocket=async()=>{ws++;if(scenario==='bad-ws')throw Error('bad socket');};
  const remote=['remote','lost-fetch','wrong-stage'].includes(scenario);
  const options={image,run:true,mode:remote?'remote':'auto',...(remote?{manifest:'https://office.example/manifest.json',manifestSha}:{}),
    request:async(url,path,options)=>{
      const response=await request(url,path,options);
      if(scenario==='latch'&&path==='/api/status')return {...response,body:Buffer.from(JSON.stringify({...base,programming_latch:true}))};
      return response;
    },upload,websocket,wait:async ms=>{clock+=ms;},now:()=>clock,timeoutSeconds:10};
  if(['local','remote','wrong-boot-headers','existing-capacity','dhcp'].includes(scenario)){
    const result=await updateFirmware(options);assert.equal(result.ok,true);assert.equal(result.authentication,false);
    assert.equal(result.simulated,true);assert.equal(result.usb_disconnect_ready,false);assert.equal(ws,2);
    assert.equal(result.boot_after,'b'.repeat(32));assert.equal(writes,remote?2:1);
    const firstPostBootStatus=calls.find(c=>rebooted&&c.path==='/api/status'&&!c.options.headers['X-Pakar-Boot']&&c.options.expectedAddress);
    assert.ok(firstPostBootStatus,'Reboot observation must pin TCP but not send stale boot headers');
  }else await assert.rejects(updateFirmware(options));
  if(['auth','unknown-auth','bad-product','busy','latch','bad-ws'].includes(scenario))assert.equal(writes,0);
  if(['lost-upload','bad-range','bad-sha','reboot-peer'].includes(scenario))assert.equal(writes,1,'No retry/fallback after mutation');
  if(['lost-fetch','wrong-stage'].includes(scenario)){assert.equal(fetches,1);assert.equal(applies,0);}
}

let received,wsMode='valid';const wireStatus={...base};
const server=createServer(async(req,res)=>{const chunks=[];for await(const chunk of req)chunks.push(chunk);
  received={path:req.url,headers:req.headers,body:Buffer.concat(chunks)};res.setHeader('Content-Type','application/json');res.end('{"updated":true,"rebooting":true}');});
const sockets=new Set();server.on('connection',socket=>{sockets.add(socket);socket.on('close',()=>sockets.delete(socket));});
server.on('upgrade',(req,socket)=>{
  const accept=createHash('sha1').update(req.headers['sec-websocket-key']+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest('base64');
  socket.write('HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: '+(wsMode==='bad-upgrade'?'invalid':accept)+'\r\n\r\n');
  const text=Buffer.from(JSON.stringify({...wireStatus,board_id:wsMode==='other-peer'?'other':base.board_id}));
  const frame=(first,payload)=>{const header=Buffer.alloc(4);header[0]=first;header[1]=126;header.writeUInt16BE(payload.length,2);return Buffer.concat([header,payload]);};
  // Fragmented text spanning TCP chunks is accepted, not assumed atomic.
  const split=Math.floor(text.length/2);socket.write(frame(1,text.subarray(0,split)).subarray(0,3));
  socket.write(frame(1,text.subarray(0,split)).subarray(3));socket.write(frame(0x80,text.subarray(split)));
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
try{
  const origin=new URL('http://localhost:'+server.address().port),peer={board:base.board_id,boot:base.boot_id,address:'127.0.0.1'};
  await uploadImage(origin,peer,image,identity);assert.equal(received.headers.authorization,undefined);
  assert.equal(received.headers['x-pakar-firmware-size'],String(image.length));assert.equal(received.headers['x-pakar-firmware-md5'],identity.md5);
  assert.equal(Number(received.headers['content-length']),received.body.length);assert.ok(received.body.includes(image));
  assert.ok(received.body.indexOf(Buffer.from('name="MD5"'))<received.body.indexOf(Buffer.from('name="firmware"')));
  await checkWebSocket(origin,peer);
  for(wsMode of ['bad-upgrade','other-peer'])await assert.rejects(checkWebSocket(origin,peer));
}finally{for(const socket of sockets)socket.destroy();server.close();await once(server,'close');}

const sketch=readFileSync(new URL('../firmware/esp8266/esp8266.ino',import.meta.url),'utf8');
const info=sketch.slice(sketch.indexOf('void registerAsyncOta() {'),sketch.indexOf('void registerApi() {'));
assert.doesNotMatch(info,/FirmwarePlatform::sketch(?:Size|Md5)\(/,'Progress info cannot hash the whole image on each poll');
for(const token of ['X-Pakar-Firmware-Size','X-Pakar-Firmware-MD5','otaUploadedBytes!=otaExpectedBytes','Update.end(true)','otaProgress.advance'])assert.ok(info.includes(token));
assert.ok(sketch.indexOf('updatePanel.next')>sketch.indexOf('void loop()'));
assert.ok(sketch.includes('!programmingLatched && !avrUploadOwner,updateText'));
console.log('Automatic updater plan/preflight/admission/no-replay/new-boot/SHA/auth-off and real loopback multipart/fragmented WebSocket fixtures passed. Not physical OTA.');
