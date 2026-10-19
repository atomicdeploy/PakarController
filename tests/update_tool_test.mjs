// HTTP/WS doubles and a loopback wire server, never a physical flash test.
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {createHash} from 'node:crypto';
import {once} from 'node:events';
import {readFileSync} from 'node:fs';
import {boardPeer,imageIdentity,selectPath,updateFirmware,uploadImage,checkWebSocket,boundedReadRequest} from '../tools/update-firmware.mjs';
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
for(const ackFailedJobId of [0,-1,1.5,0x100000000,NaN])await assert.rejects(updateFirmware({image,ackFailedJobId}));
await assert.rejects(updateFirmware({image,mode:'remote',manifest:'https://office.example/manifest.json',manifestSha,resumeJobId:18,ackFailedJobId:20}));
const failedPlan=await updateFirmware({image,mode:'local',ackFailedJobId:20});
assert.equal(failedPlan.device_contacted,false);assert.equal(failedPlan.ack_failed_job,20);
const priorImage=Buffer.from(image);priorImage[25]=42;const priorIdentity=imageIdentity(priorImage);
await assert.rejects(updateFirmware({image,ackFailedImage:priorImage}));
await assert.rejects(updateFirmware({image,ackFailedJobId:20,ackFailedImage:Buffer.alloc(96)}));
const priorPlan=await updateFirmware({image,ackFailedJobId:20,ackFailedImage:priorImage});
assert.equal(priorPlan.ack_failed_image.sha256,priorIdentity.sha256);assert.equal(priorPlan.image.sha256,identity.sha256);
for(const resumeJobId of [0,-1,1.5,0x100000000,NaN])await assert.rejects(updateFirmware({image,mode:'remote',manifest:'https://office.example/manifest.json',manifestSha,resumeJobId}));
await assert.rejects(updateFirmware({image,resumeJobId:18}));
await assert.rejects(updateFirmware({image,mode:'auto',manifest:'https://office.example/manifest.json',manifestSha,resumeJobId:18}));
for(const scenario of ['local','remote','auth','unknown-auth','bad-product','busy','latch','lost-upload','lost-fetch','wrong-stage','reboot-peer','bad-range','bad-sha','bad-ws','wrong-boot-headers','existing-capacity','dhcp',
  'resume','resume-wrong-job','resume-wrong-boot','resume-wrong-sha','resume-wrong-targets','resume-wrong-size','resume-not-ready','resume-busy','resume-changed-before-apply',
  'ack','ack-local','ack-remote','ack-no-explicit','ack-wrong-job','ack-wrong-boot','ack-wrong-sha','ack-wrong-size','ack-avr','ack-pair','ack-busy','ack-cancellable',
  'ack-ready','ack-applying','ack-no-error','ack-bad-manifest','ack-latch','ack-latch-late','ack-peer-change','ack-boot-change','ack-changed-before-write','ack-error-changed',
  'ack-new-image','ack-new-image-wrong-prior','ack-new-image-changed-prior']) {
  const resume=scenario.startsWith('resume'),ack=scenario.startsWith('ack');let jobReads=0,statusReads=0;
  const newImage=scenario.startsWith('ack-new-image');
  let clock=0,rebooted=false,writes=0,fetches=0,applies=0,ws=0;const calls=[],events=[];
  const request=async(_url,path,options={})=>{
    calls.push({path,options});if(rebooted&&options.headers?.['X-Pakar-Boot']===base.boot_id)throw Error('old boot fenced');
    if(rebooted&&scenario==='dhcp'&&options.expectedAddress===address)throw Error('cached address unreachable');
    const remoteAddress=rebooted&&scenario==='dhcp'?'192.168.50.21':address;
    const value=v=>({status:200,remote:remoteAddress,headers:{},body:Buffer.from(JSON.stringify(v))});
    if(path==='/api/status'){statusReads++;return value({...base,
      board_id:(rebooted&&scenario==='reboot-peer')||(scenario==='ack-peer-change'&&statusReads>1)?'other':base.board_id,
      boot_id:rebooted?'b'.repeat(32):scenario==='ack-boot-change'&&statusReads>1?'c'.repeat(32):base.boot_id,
      programming_latch:scenario==='ack-latch'||(scenario==='ack-latch-late'&&statusReads>1)});}
    if(path==='/api/auth')return value(scenario==='unknown-auth'?{}:{enabled:scenario==='auth'});
    if(path==='/')return {status:200,remote:address,headers:{},body:Buffer.from('<!doctype html><html>fixture</html>')};
    if(path==='/update/info')return value({product:scenario==='bad-product'?'other':'PakarController',framework:'ESPAsyncWebServer',model:'esp32s3 + ATmega328P',...(scenario==='existing-capacity'?{free:0x330000}:{ota_capacity_bytes:0x330000}),
      upload:'/update',remote_fetch:'/api/firmware/fetch',remote_apply:'/api/firmware/apply'});
    if(path==='/api/telemetry')return value({board_id:base.board_id,boot_id:'b'.repeat(32),esp:{sketch_bytes:image.length,sketch_md5:identity.md5,boot_integrity_verified:true,ota:{running_offset:65536,boot_offset:65536,running_state_code:2}}});
    if(path==='/firmware/download')return {status:scenario==='bad-range'?200:206,remote:address,headers:{'content-range':`bytes 0-${image.length-1}/${image.length}`},body:scenario==='bad-sha'?Buffer.alloc(image.length):image};
    if(path==='/api/firmware/fetch'){fetches++;writes++;assert.equal(options.form.apply,'false');assert.equal(options.form.targets,'1');if(scenario==='lost-fetch')throw Error('lost fetch');return value({job_id:7,boot_id:base.boot_id});}
    if(path==='/api/firmware/apply'){applies++;writes++;assert.equal(options.form.job_id,String(resume?18:7));rebooted=true;return value({job_id:resume?18:7,boot_id:base.boot_id});}
    if(path==='/api/firmware/remote'){jobReads++;
      if(ack&&!fetches&&!rebooted)return value({
        job_id:scenario==='ack-wrong-job'||(scenario==='ack-changed-before-write'&&jobReads>1)?21:20,
        boot_id:scenario==='ack-wrong-boot'?'c'.repeat(32):base.boot_id,
        manifest_sha256:scenario==='ack-bad-manifest'?'':manifestSha,
        targets:scenario==='ack-avr'?2:scenario==='ack-pair'?3:1,
        busy:scenario==='ack-busy',cancellable:scenario==='ack-cancellable',
        phase:scenario==='ack-ready'?'ready':scenario==='ack-applying'?'program_esp':'failed',
        error:scenario==='ack-no-error'?'':scenario==='ack-error-changed'&&jobReads>1?'Different failure':'Download HTTP status -1; TLS error -32512',
        esp:{sha256:scenario==='ack-wrong-sha'||scenario==='ack-new-image-wrong-prior'||(scenario==='ack-new-image-changed-prior'&&jobReads>1)?'4'.repeat(64):newImage?priorIdentity.sha256:identity.sha256,size:scenario==='ack-wrong-size'?image.length+1:image.length},
        received_bytes:0,expected_bytes:0});
      return value(fetches||resume?{
      job_id:scenario==='resume-wrong-job'?19:resume?18:7,
      boot_id:scenario==='resume-wrong-boot'?'c'.repeat(32):rebooted?'b'.repeat(32):base.boot_id,
      manifest_sha256:scenario==='resume-wrong-sha'?'3'.repeat(64):manifestSha,
      targets:scenario==='resume-wrong-targets'?3:1,busy:scenario==='resume-busy',
      phase:scenario==='resume-not-ready'?'complete':rebooted?'complete':'ready',
      esp:{size:scenario==='resume-wrong-size'?image.length+1:image.length,
        sha256:scenario==='wrong-stage'||(scenario==='resume-changed-before-apply'&&jobReads>1)?'2'.repeat(64):identity.sha256}}:
      {busy:scenario==='busy',phase:'idle'});
    }
    throw Error('unexpected path '+path);
  };
  const upload=async(_origin,peer,bytes,hash,settings)=>{writes++;assert.equal(settings.timeoutSeconds,10);assert.equal(peer.boot,base.boot_id);assert.deepEqual(bytes,image);assert.equal(hash.sha256,identity.sha256);
    if(scenario==='lost-upload')throw Error('outcome unknown');rebooted=true;};
  const websocket=async()=>{ws++;if(scenario==='bad-ws')throw Error('bad socket');};
  const remote=resume||['remote','lost-fetch','wrong-stage','ack-remote'].includes(scenario);
  const options={image,run:true,mode:remote?'remote':scenario==='ack-local'?'local':'auto',...(remote?{manifest:'https://office.example/manifest.json',manifestSha}:{}),
    ...(resume?{resumeJobId:18}:{}),...(ack&&scenario!=='ack-no-explicit'?{ackFailedJobId:20}:{}),...(newImage?{ackFailedImage:priorImage}:{}),request:async(url,path,options)=>{
      const response=await request(url,path,options);
      if(scenario==='latch'&&path==='/api/status')return {...response,body:Buffer.from(JSON.stringify({...base,programming_latch:true}))};
      return response;
    },upload,websocket,wait:async ms=>{clock+=ms;},now:()=>clock,timeoutSeconds:10,log:event=>events.push(event)};
  if(['local','remote','wrong-boot-headers','existing-capacity','dhcp','resume','ack','ack-local','ack-remote','ack-new-image'].includes(scenario)){
    const result=await updateFirmware(options);assert.equal(result.ok,true);assert.equal(result.authentication,false);
    assert.equal(result.simulated,true);assert.equal(result.usb_disconnect_ready,false);assert.equal(ws,2);
    assert.equal(result.boot_after,'b'.repeat(32));assert.equal(writes,resume?1:remote?2:1);
    if(resume){assert.equal(fetches,0);assert.equal(applies,1);assert.equal(result.resume_job,18);assert.equal(result.transfer_ms,null);assert.equal(result.download_timing,'previous invocation; not measured here');}
    if(ack){
      const receipt=result.acknowledged_failed_job;assert.equal(receipt.job_id,20);assert.equal(receipt.boot_id,base.boot_id);
      assert.equal(receipt.error,'Download HTTP status -1; TLS error -32512');assert.equal(receipt.esp.sha256,newImage?priorIdentity.sha256:identity.sha256);
      assert.equal(receipt.received_bytes,0);assert.equal(receipt.expected_bytes,0);
      assert.deepEqual(events.find(e=>e.stage==='ack-failed-job')?.acknowledged_failed_job,receipt);
      assert.equal(fetches,remote?1:0);assert.equal(applies,remote?1:0,'Explicit local recovery never applies or cancels the old remote job');
    }
    const firstPostBootStatus=calls.find(c=>rebooted&&c.path==='/api/status'&&!c.options.headers['X-Pakar-Boot']&&c.options.expectedAddress);
    assert.ok(firstPostBootStatus,'Reboot observation must pin TCP but not send stale boot headers');
  }else await assert.rejects(updateFirmware(options));
  if(['auth','unknown-auth','bad-product','busy','latch','bad-ws'].includes(scenario))assert.equal(writes,0);
  if(['lost-upload','bad-range','bad-sha','reboot-peer'].includes(scenario))assert.equal(writes,1,'No retry/fallback after mutation');
  if(['lost-fetch','wrong-stage'].includes(scenario)){assert.equal(fetches,1);assert.equal(applies,0);}
  if(resume&&scenario!=='resume'){assert.equal(writes,0);assert.equal(fetches,0);assert.equal(applies,0);}
  if(ack&&!['ack','ack-local','ack-remote','ack-new-image'].includes(scenario)){assert.equal(writes,0);assert.equal(fetches,0);assert.equal(applies,0);assert(!events.some(e=>e.stage==='ack-failed-job'));}
  assert(!calls.some(c=>c.path==='/api/firmware/cancel'),'Failed receipt acknowledgement never clears or cancels a board job');
}

// Only one additional read attempt; exact address and headers stay pinned.
for(const method of ['GET','HEAD','POST']) {
  let calls=0;const observed=[];
  const request=async(_origin,_path,options)=>{calls++;observed.push(options);if(calls===1)throw Object.assign(Error('reset'),{code:'ECONNRESET'});return {status:200};};
  const options={method,expectedAddress:address,headers:{'X-Pakar-Board':base.board_id,'X-Pakar-Boot':base.boot_id},...(method==='POST'?{form:{job_id:'18'}}:{})};
  const operation=boundedReadRequest(request,new URL('http://ledboard.local'),'/api/firmware/remote',options);
  if(method==='POST'){await assert.rejects(operation);assert.equal(calls,1);}
  else {assert.equal((await operation).status,200);assert.equal(calls,2);assert.equal(observed[0],observed[1]);assert.deepEqual(observed[1].headers,options.headers);}
}
{
  let calls=0;await assert.rejects(boundedReadRequest(async()=>{calls++;throw Object.assign(Error('reset'),{code:'ECONNRESET'});},new URL('http://ledboard.local'),'/api/status',{expectedAddress:address}));assert.equal(calls,2);
  calls=0;await assert.rejects(boundedReadRequest(async()=>{calls++;throw Error('TCP peer changed; no retry');},new URL('http://ledboard.local'),'/api/status',{expectedAddress:address}));assert.equal(calls,1);
  calls=0;assert.equal((await boundedReadRequest(async()=>{calls++;return {status:401};},new URL('http://ledboard.local'),'/api/auth',{expectedAddress:address})).status,401);assert.equal(calls,1);
  calls=0;await assert.rejects(boundedReadRequest(async()=>{calls++;throw Object.assign(Error('reset'),{code:'ECONNRESET'});},new URL('http://ledboard.local'),'/api/status',{}));assert.equal(calls,1,'Unpinned hostname read is not retried');
}

let received,wsMode='valid',statusRequests=0,pongs=0;const wireStatus={...base};
const server=createServer(async(req,res)=>{const chunks=[];for await(const chunk of req)chunks.push(chunk);
  received={path:req.url,headers:req.headers,body:Buffer.concat(chunks)};res.setHeader('Content-Type','application/json');res.end('{"updated":true,"rebooting":true}');});
const sockets=new Set();server.on('connection',socket=>{sockets.add(socket);socket.on('close',()=>sockets.delete(socket));});
server.on('upgrade',(req,socket)=>{
  if(wsMode==='no-upgrade')return;
  if(wsMode==='auth'){socket.end('HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\n\r\n');return;}
  const accept=createHash('sha1').update(req.headers['sec-websocket-key']+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest('base64');
  socket.write('HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: '+(wsMode==='bad-upgrade'?'invalid':accept)+'\r\n\r\n');
  const frame=(first,payload)=>{const header=Buffer.alloc(4);header[0]=first;header[1]=126;header.writeUInt16BE(payload.length,2);return Buffer.concat([header,payload]);};
  const send=value=>socket.write(frame(0x81,Buffer.from(JSON.stringify(value))));
  let ingress=Buffer.alloc(0),finalFrame=null;
  socket.on('data',chunk=>{
    ingress=Buffer.concat([ingress,chunk]);
    while(ingress.length>=2){
      const opcode=ingress[0]&15,length=ingress[1]&127;
      assert.equal(ingress[0]&128,128);assert.equal(ingress[1]&128,128,'Client requests/control frames must be masked');assert(length<126);
      if(ingress.length<6+length)return;
      const mask=ingress.subarray(2,6),payload=Buffer.from(ingress.subarray(6,6+length));
      for(let i=0;i<length;i++)payload[i]^=mask[i%4];ingress=ingress.subarray(6+length);
      if(opcode===10){assert.equal(payload.toString(),'p');pongs++;if(finalFrame){socket.write(finalFrame);finalFrame=null;}continue;}
      assert.equal(opcode,1);assert.equal(payload.toString(),'{"type":"status"}');statusRequests++;
      if(wsMode==='compact'||wsMode==='only-compact'||wsMode==='foreign-compact') {
        send({type:'status-rgb',board_id:wsMode==='foreign-compact'?'other':base.board_id,boot_id:base.boot_id,
          revision:2,status_rgb:{available:true,have_output:true,rgb:[1,2,3]}});
        if(wsMode==='foreign-compact')return;
        send({type:'update-progress',board_id:base.board_id,boot_id:base.boot_id,revision:3,
          event:'update.phase',phase:'download',progress_known:false});
        if(wsMode==='only-compact')return;
      }
      if(wsMode.startsWith('panel-')||wsMode.startsWith('avr-')||wsMode==='all-compact'||wsMode==='only-panel') {
        const panel={type:'panel-event',board_id:base.board_id,boot_id:base.boot_id,event_id:1,uptime_ms:100,
          kind:'progress',value:42,text:'P042',hardware_confirmed:false,
          presentation:{text:'P042',lifetime_ms:2000,priority:90,blink:'steady',marquee:'none'}};
        const avr={type:'pc-contract-event',board_id:base.board_id,boot_id:base.boot_id,event_id:2,
          avr_session:7,response_opcode:0x9c,payload_hex:'393f3f06c5'};
        const event=wsMode.startsWith('avr-')?avr:panel;
        if(wsMode.endsWith('-foreign'))event.board_id='other';
        if(wsMode.endsWith('-wrong-boot'))event.boot_id='c'.repeat(32);
        if(wsMode==='panel-malformed')event.presentation.lifetime_ms=-1;
        if(wsMode==='panel-confirmed')event.hardware_confirmed=true;
        if(wsMode==='panel-oversized')event.text='x'.repeat(33);
        if(wsMode==='avr-malformed')event.payload_hex='0x';
        if(wsMode==='avr-oversized')event.payload_hex='00'.repeat(48);
        if(wsMode==='avr-request-opcode')event.response_opcode=0x38;
        send(event);
        if(wsMode==='all-compact')send(avr);
        if(!['panel-valid','avr-valid','all-compact'].includes(wsMode))return;
      }
      if(wsMode==='unknown-event'){send({type:'pong',board_id:base.board_id,boot_id:base.boot_id});return;}
      if(wsMode==='bad-fragment'){socket.write(frame(0x80,Buffer.from('{}')));return;}
      if(wsMode==='compact-wrong-boot'){send({type:'status-rgb',board_id:base.board_id,boot_id:'c'.repeat(32),revision:2,status_rgb:{available:true,have_output:true,rgb:[1,2,3]}});return;}
      const text=Buffer.from(JSON.stringify({...wireStatus,board_id:wsMode==='other-peer'?'other':base.board_id}));
      // Full status is request-driven, fragmented across TCP chunks, with a
      // legal ping interleaved in the fragmented text message.
      const split=Math.floor(text.length/2);socket.write(frame(1,text.subarray(0,split)).subarray(0,3));
      finalFrame=frame(0x80,text.subarray(split));
      socket.write(frame(1,text.subarray(0,split)).subarray(3));socket.write(Buffer.from([0x89,1,0x70]));
    }
  });
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
try{
  const origin=new URL('http://localhost:'+server.address().port),peer={board:base.board_id,boot:base.boot_id,address:'127.0.0.1'};
  for(const timeoutSeconds of [0,9,601,NaN])assert.throws(()=>uploadImage(origin,peer,image,identity,{timeoutSeconds}));
  await uploadImage(origin,peer,image,identity);assert.equal(received.headers.authorization,undefined);
  assert.equal(received.headers['x-pakar-firmware-size'],String(image.length));assert.equal(received.headers['x-pakar-firmware-md5'],identity.md5);
  assert.equal(Number(received.headers['content-length']),received.body.length);assert.ok(received.body.includes(image));
  assert.ok(received.body.indexOf(Buffer.from('name="MD5"'))<received.body.indexOf(Buffer.from('name="firmware"')));
  for(wsMode of ['valid','compact','panel-valid','avr-valid','all-compact']){
    const before=statusRequests,result=await checkWebSocket(origin,peer);
    assert.equal(statusRequests,before+1,'Exactly one read-only request, no replay');assert.equal(result.status_requests,1);
    assert.equal(result.ignored_events,['compact','all-compact'].includes(wsMode)?2:['panel-valid','avr-valid'].includes(wsMode)?1:0);assert(result.connect_ms>=0&&result.status_ms>=0&&result.total_ms>=0);
  }
  for(wsMode of ['bad-upgrade','auth','other-peer','foreign-compact','compact-wrong-boot','unknown-event','bad-fragment',
    'panel-foreign','panel-wrong-boot','panel-malformed','panel-confirmed','panel-oversized',
    'avr-foreign','avr-wrong-boot','avr-malformed','avr-oversized','avr-request-opcode']){
    const before=statusRequests;await assert.rejects(checkWebSocket(origin,peer));
    assert.equal(statusRequests,before+(['bad-upgrade','auth'].includes(wsMode)?0:1));
  }
  wsMode='only-compact';const before=statusRequests;
  await assert.rejects(checkWebSocket(origin,peer,{statusTimeoutMs:50}),/full-status deadline.*compact events 2/);
  assert.equal(statusRequests,before+1,'Partial events never satisfy readiness or trigger request replay');
  wsMode='only-panel';const panelBefore=statusRequests;
  await assert.rejects(checkWebSocket(origin,peer,{statusTimeoutMs:50}),/full-status deadline.*compact events 1/);
  assert.equal(statusRequests,panelBefore+1,'A valid panel event is not full status and never triggers replay');
  wsMode='no-upgrade';await assert.rejects(checkWebSocket(origin,peer,{connectTimeoutMs:50}),/connect\/upgrade deadline/);
  assert(pongs>=1,'Interleaved ping receives a masked pong');
}finally{for(const socket of sockets)socket.destroy();server.close();await once(server,'close');}

const sketch=readFileSync(new URL('../firmware/esp8266/esp8266.ino',import.meta.url),'utf8');
const info=sketch.slice(sketch.indexOf('void registerAsyncOta() {'),sketch.indexOf('void registerApi() {'));
assert.doesNotMatch(info,/FirmwarePlatform::sketch(?:Size|Md5)\(/,'Progress info cannot hash the whole image on each poll');
for(const token of ['X-Pakar-Firmware-Size','X-Pakar-Firmware-MD5','otaUploadedBytes!=otaExpectedBytes','Update.end(true)','otaProgress.advance'])assert.ok(info.includes(token));
const panelService=readFileSync(new URL('../firmware/esp8266/src/SemanticPanelService.h',import.meta.url),'utf8');
const panel=readFileSync(new URL('../firmware/esp8266/src/SemanticPanel.h',import.meta.url),'utf8');
const loop=sketch.slice(sketch.indexOf('void loop()'));
const background=sketch.slice(sketch.indexOf('void upgradeBackground() {'),sketch.indexOf('bool upgradeRebootReady() {'));
assert.match(loop,/serviceSemanticPanel\(millis\(\)\)/,'Main loop owns presentation');
assert.match(background,/serviceUpgrade\(\);[\s\S]*serviceSemanticPanel\(millis\(\)\)/,'Cooperative maintenance also samples the same presenter');
assert.doesNotMatch(info,/serviceSemanticPanel\(|sendToAvr\(/,'Async upload callbacks may publish progress, never perform UART presentation');
assert.match(panelService,/updating&&upgradeLifecycle\.known\(\)\?upgradeLifecycle\.percent\(\):-1/,'Unknown stages stay indeterminate');
assert.match(panelService,/semanticPanel\.persistent\(updating,text\)/,'Measured update state has priority over ordinary notices');
assert.match(panelService,/urbootRawOwned\|\|programmingLatched\|\|avrUploadOwner\|\|avrCaptured\|\|[\s\S]*semanticPanel\.manualActive\(now\)/,'Raw UART, Prog, uploads and manual owners inhibit automatic leases');
assert.match(panelService,/semanticPanel\.frame\(now,tm1637DetectionKnown\(\)&&tm1637Ack,inhibited,payload\)/,'Fresh observed TM1637 ACK gates admission');
assert.match(panelService,/if\(updating&&avrMaintenancePanel\)payload\[4\]\|=0x04U/,'Menu override requires the actual AVR acknowledgement');
assert.match(panelService,/if\(sendToAvr\(kDisplayText,sequence\+\+,payload,sizeof\(payload\)\)\)semanticPanel\.admitted\(now,payload\)/,'Failed UART writes cannot advance the presentation lease');
assert.doesNotMatch(panelService,/requestAndWaitForAck\(|\bdelay\(/,'Presenter cannot block the maintenance loop');
assert.match(panelService,/d\["hardware_confirmed"\]=false/,'Requested presentation never fabricates physical confirmation');
const frame=panel.slice(panel.indexOf('bool frame('),panel.indexOf('void admitted('));
assert.match(frame,/if\(!available\|\|inhibited\)/);
assert.match(frame,/prefix\[8\]=\{5,4,1,4,0,0xb0,4,0\}/,'Actual producer uses a once-only 1200ms scheduled lease, not capture or a looping marquee');
assert.doesNotMatch(frame,/sent_\s*=\s*true/,'Only acknowledged transport admission advances the send clock');
const wrapper=readFileSync(new URL('../tools/update-firmware.ps1',import.meta.url),'utf8');
assert.ok(wrapper.includes('@(Get-Command node -CommandType Application -ErrorAction Stop)[0].Source'),'Multiple installed Node runtimes must resolve to one PATH-preferred executable');
assert.ok(wrapper.includes('[long]$ResumeJobId')&&wrapper.includes("'--resume-job'"));
assert.ok(wrapper.includes('[long]$AckFailedJobId')&&wrapper.includes("'--ack-failed-job'"));
assert.ok(wrapper.includes('[string]$AckFailedImage')&&wrapper.includes("'--ack-failed-image'"));
console.log('Automatic updater plan/preflight/admission/no-replay/new-boot/SHA/auth-off and real loopback multipart/fragmented WebSocket fixtures passed. Not physical OTA.');
