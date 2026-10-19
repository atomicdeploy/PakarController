// PU850 AsyncOTA/FirmwareTransfer + PCController UPDATE-PROGRESS contracts.
// Reuses the reviewed hostname/TCP pin policy from check-ethernet-live; no
// credentials, proxy, redirects, automatic write retry or duplicate image copy.
import http from 'node:http';
import https from 'node:https';
import {readFileSync} from 'node:fs';
import {createHash, randomBytes} from 'node:crypto';
import {isIP} from 'node:net';
import {pathToFileURL} from 'node:url';
import {setTimeout as delay} from 'node:timers/promises';
import {deviceOrigin,deviceRequest} from './check-ethernet-live.mjs';
import {manifest as validateImage} from './create-remote-manifest.mjs';
import {toolHeaders,boardPeer} from './peer-identity.mjs';
export {boardPeer} from './peer-identity.mjs';

export function imageIdentity(bytes) {
  // Existing publisher validates raw S3 application, not bootloader/merged flash.
  validateImage(bytes,null,'https://firmware.invalid/','0.0.0','buffered','esp32s3');
  return {size:bytes.length,sha256:createHash('sha256').update(bytes).digest('hex'),
    md5:createHash('md5').update(bytes).digest('hex')};
}
export function selectPath(mode,info,hasManifest) {
  if(!['auto','local','remote'].includes(mode))throw Error('Mode must be auto, local or remote');
  if(mode!=='remote' && info?.upload==='/update')return 'local';
  if(mode!=='local' && hasManifest && info?.remote_fetch==='/api/firmware/fetch' && info?.remote_apply==='/api/firmware/apply')return 'remote';
  throw Error('No verified requested update path; no write attempted. USB requires explicit device inventory, not guessed fallback');
}
function requestOptions(origin,peer,headers={}) {
  return {agent:false,family:4,
    lookup:(_host,options,done)=>done(null,options.all?[{address:peer.address,family:4}]:peer.address,4),
    headers:toolHeaders({'X-Pakar-Board':peer.board,'X-Pakar-Boot':peer.boot,...headers})};
}
function transientReadError(error) {
  for(let current=error,depth=0;current&&depth<3;current=current.cause,depth++) {
    if(['ECONNRESET','EPIPE','ETIMEDOUT','ECONNABORTED','ENETUNREACH','EHOSTUNREACH','ECONNREFUSED'].includes(current.code))return true;
    if(current.message==='Device response interrupted; no retry'||current.message==='Device request timed out; no retry')return true;
  }
  return false;
}
// One extra transport READ only, with the same pinned address and immutable
// headers. HTTP rejection, identity/auth/JSON failures and ALL writes bypass it.
export async function boundedReadRequest(request,origin,path,options={},log=()=>{}) {
  const method=options.form?'POST':options.method||'GET';
  const pinned=Object.freeze({...options,headers:Object.freeze({...options.headers})});
  try{return await request(origin,path,pinned)}catch(error){
    if(!['GET','HEAD'].includes(method)||options.form||isIP(pinned.expectedAddress)!==4||!transientReadError(error))throw error;
    log({stage:'read-retry',method,path:path.split('?')[0],attempt:2});
    return request(origin,path,pinned);
  }
}
function readyJob(job,id,peer,manifestSha,identity) {
  if(job?.job_id!==id||job.boot_id!==peer.boot||job.manifest_sha256!==manifestSha||job.targets!==1||
    job.phase!=='ready'||job.busy!==false||job.esp?.sha256!==identity.sha256||job.esp?.size!==identity.size)
    throw Error('Existing ready job/boot/manifest/target/image does not match; no apply or fetch');
}
function failedJobReceipt(job,id,peer,identity) {
  if(job?.job_id!==id||job.boot_id!==peer.boot||job.targets!==1||job.phase!=='failed'||
    job.busy!==false||job.cancellable!==false||job.esp?.sha256!==identity.sha256||job.esp?.size!==identity.size||
    !/^[a-f0-9]{64}$/.test(job.manifest_sha256??'')||typeof job.error!=='string'||!job.error.length||job.error.length>4096)
    throw Error('Failed job acknowledgement does not match terminal ESP-only job/boot/image; no write');
  // Acknowledgement is local operator intent, not a cancel/clear request. Keep
  // the diagnostic in the result/log even if a later explicit fetch replaces it.
  return {job_id:job.job_id,boot_id:job.boot_id,phase:job.phase,targets:job.targets,
    busy:job.busy,cancellable:job.cancellable,manifest_sha256:job.manifest_sha256,error:job.error,
    esp:{size:job.esp.size,sha256:job.esp.sha256},
    received_bytes:job.received_bytes??null,expected_bytes:job.expected_bytes??null};
}
export function uploadImage(origin,peer,bytes,identity,{timeoutSeconds=180}={}) {
  if(!Number.isInteger(timeoutSeconds)||timeoutSeconds<10||timeoutSeconds>600)throw Error('Upload timeout must be10..600 seconds');
  const boundary='Pakar'+randomBytes(16).toString('hex');
  const start=Buffer.from(`--${boundary}\r\nContent-Disposition: form-data; name="MD5"\r\n\r\n${identity.md5}\r\n--${boundary}\r\nContent-Disposition: form-data; name="firmware"; filename="esp.bin"\r\nContent-Type: application/octet-stream\r\n\r\n`);
  const end=Buffer.from(`\r\n--${boundary}--\r\n`);
  return new Promise((resolve,reject)=>{
    const client=origin.protocol==='https:'?https:http;
    const req=client.request(new URL('/update',origin),{method:'POST',...requestOptions(origin,peer,{
      'Content-Type':'multipart/form-data; boundary='+boundary,'Content-Length':start.length+bytes.length+end.length,
      'X-Pakar-Firmware-Size':String(bytes.length),'X-Pakar-Firmware-MD5':identity.md5})},res=>{
      let result=Buffer.alloc(0);
      if(res.socket.remoteAddress?.replace(/^::ffff:/,'')!==peer.address){req.destroy(Error('TCP peer changed'));return;}
      res.on('data',chunk=>{if(result.length+chunk.length>16384){req.destroy(Error('Oversized update reply'));return;}result=Buffer.concat([result,chunk]);});
      res.on('error',()=>reject(Error('Upload response interrupted; outcome unknown, no retry')));
      res.on('end',()=>{let value;try{value=JSON.parse(result)}catch{}
        if(res.statusCode!==200 || value?.updated!==true || value?.rebooting!==true)reject(Error('Update rejected or outcome unknown (HTTP '+res.statusCode+'); inspect before retry'));
        else resolve(value);
      });
    });
    const started=Date.now();
    const timer=setTimeout(()=>req.destroy(Error('Upload deadline; outcome unknown, no retry')),timeoutSeconds*1000);
    req.on('close',()=>clearTimeout(timer));req.on('error',error=>reject(Error('Upload failed after '+(Date.now()-started)+'ms: '+(error.code??error.message)+'; outcome unknown, no fallback or retry')));
    req.write(start);req.write(bytes);req.end(end);
  });
}
// Native ws_worker requests this same read-only snapshot on connection. Never
// replay it or treat optional compact presentation events as a full status.
function maskedFrame(opcode,payload) {
  const mask=randomBytes(4),frame=Buffer.alloc(6+payload.length);
  frame[0]=0x80|opcode;frame[1]=0x80|payload.length;mask.copy(frame,2);
  for(let i=0;i<payload.length;i++)frame[6+i]=payload[i]^mask[i%4];
  return frame;
}
function compactWebSocketEvent(value,peer) {
  if(!['status-rgb','update-progress','panel-event','pc-contract-event'].includes(value?.type))return false;
  if(value.board_id!==peer.board || value.boot_id!==peer.boot)
    throw Error('Compact WebSocket event identity changed');
  const unsigned=(number,maximum=0xffffffff)=>Number.isSafeInteger(number)&&number>=0&&number<=maximum;
  const ascii=(text,maximum)=>typeof text==='string'&&text.length<=maximum&&/^[\x20-\x7e]*$/.test(text);
  // Optional requested presentation and observed AVR events carry no full
  // status/capability authority. Validate them, then keep waiting for the ONE
  // requested same-peer full status; they never renew its deadline or replay IO.
  if(value.type==='panel-event') {
    const p=value.presentation;
    if(!unsigned(value.event_id)||!unsigned(value.uptime_ms)||!unsigned(value.value,65535)||
      !['rest','cell','mode','effect','pixel','network','progress','fault','complete','output','menu','scan','state'].includes(value.kind)||
      !ascii(value.text,32)||value.hardware_confirmed!==false||!p||
      !ascii(p.text,4)||p.text.length!==4||!unsigned(p.lifetime_ms,65535)||!unsigned(p.priority,255)||
      !['steady','slow','double-fast'].includes(p.blink)||!['once','none'].includes(p.marquee))
      throw Error('Invalid compact WebSocket panel event');
    return true;
  }
  if(value.type==='pc-contract-event') {
    if(!unsigned(value.event_id)||!unsigned(value.avr_session)||!unsigned(value.response_opcode,255)||
      value.response_opcode<128||typeof value.payload_hex!=='string'||
      !/^(?:[a-f0-9]{2}){0,47}$/.test(value.payload_hex))
      throw Error('Invalid compact WebSocket AVR event');
    return true;
  }
  if(!Number.isSafeInteger(value.revision)||value.revision<1)throw Error('Invalid compact WebSocket revision');
  if(value.type==='status-rgb') {
    const rgb=value.status_rgb;
    if(typeof rgb?.available!=='boolean'||typeof rgb.have_output!=='boolean'||
      !Array.isArray(rgb.rgb)||rgb.rgb.length!==3||rgb.rgb.some(v=>!Number.isInteger(v)||v<0||v>255))
      throw Error('Invalid compact WebSocket RGB');
  } else if(!['update.phase','update.progress'].includes(value.event)||typeof value.phase!=='string'||
    !value.phase.length||value.phase.length>64||typeof value.progress_known!=='boolean')
      throw Error('Invalid compact WebSocket update progress');
  return true;
}
// Read-only WS proof pinned to the same TCP address/board/boot as HTTP. Supports
// fragmented text and ping/pong; no actuator or firmware mutation is sent.
export function checkWebSocket(origin,peer,{connectTimeoutMs=5000,statusTimeoutMs=12000}={}) {
  if(!Number.isInteger(connectTimeoutMs)||connectTimeoutMs<10||connectTimeoutMs>10000||
     !Number.isInteger(statusTimeoutMs)||statusTimeoutMs<10||statusTimeoutMs>20000)
    throw Error('WebSocket connect/status deadlines outside bounded limits');
  return new Promise((resolve,reject)=>{
    const key=randomBytes(16).toString('base64'),client=origin.protocol==='https:'?https:http;
    let socket,buffer=Buffer.alloc(0),message=Buffer.alloc(0),fragmented=false,done=false;
    const started=Date.now();let upgradedAt=0,ignoredEvents=0,timer;
    const finish=(error)=>{if(done)return;done=true;clearTimeout(timer);socket?.destroy();req.destroy();error?reject(error):resolve({board_id:peer.board,boot_id:peer.boot,
      connect_ms:upgradedAt-started,status_ms:Date.now()-upgradedAt,total_ms:Date.now()-started,ignored_events:ignoredEvents,status_requests:1});};
    const req=client.request(new URL('/ws',origin),{...requestOptions(origin,peer,{
      Connection:'Upgrade',Upgrade:'websocket','Sec-WebSocket-Key':key,'Sec-WebSocket-Version':'13'})});
    timer=setTimeout(()=>finish(Error('WebSocket connect/upgrade deadline after '+(Date.now()-started)+'ms; no status or write replay')),connectTimeoutMs);
    req.on('response',res=>{res.resume();finish(Error('WebSocket handshake rejected (HTTP '+res.statusCode+')'));});
    req.on('error',()=>finish(Error('WebSocket connection failed')));
    req.on('upgrade',(res,connection,head)=>{
      socket=connection;
      const expected=createHash('sha1').update(key+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest('base64');
      if(res.statusCode!==101 || res.headers['sec-websocket-accept']!==expected ||
        res.headers.upgrade?.toLowerCase()!=='websocket' ||
        !res.headers.connection?.toLowerCase().split(',').map(v=>v.trim()).includes('upgrade') ||
        connection.remoteAddress?.replace(/^::ffff:/,'')!==peer.address) {
        finish(Error('Invalid WebSocket upgrade/TCP peer'));return;
      }
      upgradedAt=Date.now();clearTimeout(timer);
      timer=setTimeout(()=>finish(Error('WebSocket same-peer full-status deadline after '+(Date.now()-upgradedAt)+
        'ms (upgrade '+(upgradedAt-started)+'ms, compact events '+ignoredEvents+'); no mutation attempted')),statusTimeoutMs);
      const receive=chunk=>{try{
        buffer=Buffer.concat([buffer,chunk]);if(buffer.length>131072)throw Error('WebSocket frame exceeds bound');
        while(buffer.length>=2){
          const first=buffer[0],opcode=first&15,final=!!(first&128);let size=buffer[1]&127,offset=2;
          if((first&112)||(buffer[1]&128))throw Error('Invalid server WebSocket frame');
          if(size===126){if(buffer.length<4)return;size=buffer.readUInt16BE(2);offset=4;}
          else if(size===127){if(buffer.length<10)return;if(buffer.readUInt32BE(2)!==0)throw Error('Oversized WebSocket frame');size=buffer.readUInt32BE(6);offset=10;}
          if(size>65536 || (opcode>=8 && (!final || size>125)))throw Error('Invalid WebSocket length');
          if(buffer.length<offset+size)return;
          const payload=buffer.subarray(offset,offset+size);buffer=buffer.subarray(offset+size);
          if(opcode===8)throw Error('WebSocket closed before peer status');
          if(opcode===9){socket.write(maskedFrame(10,payload));continue;}
          if(opcode===10)continue;
          if(opcode!==1 && opcode!==0)throw Error('Expected WebSocket text status');
          if((opcode===0)!==fragmented)throw Error('Invalid WebSocket fragments');
          message=Buffer.concat([message,payload]);if(message.length>65536)throw Error('WebSocket message exceeds bound');
          fragmented=!final;if(!final)continue;
          let status;try{status=JSON.parse(message.toString('utf8'))}catch{throw Error('Malformed WebSocket JSON')}
          message=Buffer.alloc(0);
          if(compactWebSocketEvent(status,peer)){ignoredEvents++;continue;}
          boardPeer(status,peer.address,peer);finish();return;
        }
      }catch(error){finish(error);}};
      connection.on('data',receive);connection.on('error',()=>finish(Error('WebSocket interrupted')));
      connection.on('close',()=>{if(!done)finish(Error('WebSocket closed before status'));});
      // ONE masked read-only request, only after the HTTP upgrade and actual
      // TCP peer have been validated. Never automatically resend any message.
      try{connection.write(maskedFrame(1,Buffer.from('{"type":"status"}')))}
      catch{finish(Error('WebSocket status request write failed; no retry'));return;}
      if(head.length)receive(head);
    });req.end();
  });
}

export async function updateFirmware({origin='http://ledboard.local',image,mode='auto',run=false,manifest,manifestSha,resumeJobId=null,ackFailedJobId=null,ackFailedImage=null,
  backup=null,resumeBackup=false,timeoutSeconds=180,request=deviceRequest,upload=uploadImage,websocket=checkWebSocket,wait=delay,now=Date.now,log=()=>{}}={}) {
  const url=deviceOrigin(origin),bytes=Buffer.isBuffer(image)?image:readFileSync(image),identity=imageIdentity(bytes);
  if(!['auto','local','remote'].includes(mode))throw Error('Mode must be auto, local or remote');
  if(!Number.isInteger(timeoutSeconds)||timeoutSeconds<10||timeoutSeconds>600)throw Error('Timeout must be10..600 seconds');
  if(manifest){const source=new URL(manifest);
    if(source.protocol!=='https:'||source.username||source.password||source.hash||/[\s\\]/.test(manifest)||!/^[a-f0-9]{64}$/.test(manifestSha??''))throw Error('Remote path needs direct HTTPS URL and pinned manifest SHA256');
  } else if(manifestSha)throw Error('Manifest URL is missing');
  if(resumeJobId!==null && (!Number.isSafeInteger(resumeJobId)||resumeJobId<1||resumeJobId>0xffffffff||mode!=='remote'||!manifest||!manifestSha))
    throw Error('Resume requires a positive job ID, explicit remote mode, HTTPS manifest and pinned SHA256');
  if(ackFailedJobId!==null && (!Number.isSafeInteger(ackFailedJobId)||ackFailedJobId<1||ackFailedJobId>0xffffffff||resumeJobId!==null))
    throw Error('Failed-job acknowledgement requires a positive job ID and cannot be combined with resume');
  if(ackFailedImage!==null&&ackFailedJobId===null)throw Error('Prior failed image requires an explicit failed-job acknowledgement');
  // A terminal failure can outlive a successful local recovery. A subsequent
  // candidate must acknowledge the ORIGINAL failed image, never forge its
  // receipt to match the new upload. Both application images are validated.
  const failedIdentity=ackFailedImage===null?identity:imageIdentity(Buffer.isBuffer(ackFailedImage)?ackFailedImage:readFileSync(ackFailedImage));
  let backupPlan=null,download;
  if(backup!==null){
    if(typeof backup!=='string'||!backup.trim())throw Error('Backup requires an explicit new output path');
    // Actual PU performUpload/BackupInstalledFirmwareAsync sequencing, using
    // our existing verified downloader, not a second transfer implementation.
    ({downloadFirmware:download}=await import('./download-firmware.mjs'));
    backupPlan=await download({origin:url.origin,output:backup,resume:resumeBackup,timeoutSeconds,run:false});
  }else if(resumeBackup)throw Error('Resume backup requires an explicit backup path');
  if(!run)return {plan:true,mode,resume_job:resumeJobId,ack_failed_job:ackFailedJobId,ack_failed_image:ackFailedJobId===null?null:failedIdentity,origin:url.origin,image:identity,authentication:'disabled required',avr_touched:false,
    backup:backupPlan,policy:'auto prefers local Async HTTP; remote requires pinned manifest; no write fallback/replay',device_contacted:false};
  let peer=null;
  const read=async(path,{unfenced=false,resolveHostname=false,...options}={})=>{
    log({stage:'request',method:options.form?'POST':options.method||'GET',path:path.split('?')[0]});
    const response=await boundedReadRequest(request,url,path,{...options,expectedAddress:resolveHostname?undefined:peer?.address,
      headers:{...(peer&&!unfenced?{'X-Pakar-Board':peer.board,'X-Pakar-Boot':peer.boot}:{}),...options.headers}},log);
    if(response.status<200||response.status>=300)throw Error('Device rejected '+path.split('?')[0]+' (HTTP '+response.status+'); no retry');
    return response;
  };
  const json=async(path,options)=>{const r=await read(path,options);let value;
    try{value=JSON.parse(r.body)}catch{throw Error('Malformed device JSON')};return {value,remote:r.remote};};
  const first=await json('/api/status');peer=boardPeer(first.value,first.remote);
  const auth=(await json('/api/auth')).value;if(auth.enabled!==false)throw Error('Device authentication is enabled/unknown; disable through the existing owner settings or physical recovery first');
  const page=await read('/');if(!/<html|<!doctype html/i.test(page.body.toString('utf8')))throw Error('Device Web UI not served');
  const info=(await json('/update/info')).value;
  // The installed S3's existing `free` field is ESP.getFreeSketchSpace(): the
  // next OTA partition size. The explicit field is additive, not a generation.
  const capacity=info.ota_capacity_bytes??info.free;
  if(info.product!=='PakarController'||info.framework!=='ESPAsyncWebServer'||!info.model?.startsWith('esp32s3')||
    !Number.isSafeInteger(capacity)||!(identity.size<capacity))throw Error('S3 Async HTTP product/capacity not verified; use inventoried USB bootstrap if metadata is unavailable');
  if(first.value.programming_latch!==false)throw Error('AVR programming latch is active/unknown; no ESP-only update');
  const previous=(await json('/api/firmware/remote')).value;
  let acknowledgedFailure=null;
  if(resumeJobId!==null)readyJob(previous,resumeJobId,peer,manifestSha,identity);
  else if(ackFailedJobId!==null)acknowledgedFailure=failedJobReceipt(previous,ackFailedJobId,peer,failedIdentity);
  else if(previous.busy!==false||!['idle','complete','cancelled'].includes(previous.phase))throw Error('Existing firmware job requires inspection; no overwrite');
  await websocket(url,peer);
  const route=selectPath(mode,info,!!manifest),before={...peer};
  let backupReceipt=null,backupMs=null;
  if(backupPlan){
    if(!/^[a-f0-9]{32}$/i.test(info.hash??'')||!Number.isSafeInteger(info.size)||info.size<24||info.size>64*1024*1024)
      throw Error('Verified running image metadata required before backup/update; no write');
    log({stage:'backup',image_bytes:info.size});const backupStarted=now();
    backupReceipt=await download({origin:url.origin,output:backupPlan.output,resume:resumeBackup,
      timeoutSeconds,run:true,expectedPeer:{...peer},environment:{},request,now});
    if(backupReceipt.ok!==true||backupReceipt.board_id!==peer.board||backupReceipt.boot_id!==peer.boot||
       backupReceipt.origin!==url.origin||backupReceipt.size!==info.size||backupReceipt.md5!==info.hash.toLowerCase())
      throw Error('Backup does not match admitted running image; no update');
    backupMs=now()-backupStarted;
    // A slow backup never carries an earlier auth decision into a write.
    if((await json('/api/auth')).value.enabled!==false)throw Error('Authentication changed during backup; no update');
    if(resumeJobId===null&&ackFailedJobId===null){
      const current=(await json('/api/firmware/remote')).value;
      if(current.busy!==false||current.phase!==previous.phase||current.job_id!==previous.job_id||
         current.boot_id!==previous.boot_id||current.manifest_sha256!==previous.manifest_sha256)
        throw Error('Firmware job changed during backup; no overwrite or update');
    }
    log({stage:'backup-verified',image_bytes:backupReceipt.size,sha256:backupReceipt.sha256});
  }
  // Refresh the same boot immediately before the single selected write path.
  const fresh=await json('/api/status');boardPeer(fresh.value,fresh.remote,peer);
  if(fresh.value.programming_latch!==false)throw Error('Programming state changed; no update');
  if(acknowledgedFailure){
    const observed=failedJobReceipt((await json('/api/firmware/remote')).value,ackFailedJobId,peer,failedIdentity);
    if(JSON.stringify(observed)!==JSON.stringify(acknowledgedFailure))throw Error('Failed job changed before acknowledgement; no write');
    log({stage:'ack-failed-job',acknowledged_failed_job:acknowledgedFailure});
  }
  log({stage:resumeJobId===null?'upload':'resume-apply',path:route,image_bytes:identity.size,resume_job:resumeJobId});
  const started=now();let jobId=null,applyRequestMs=null;
  if(route==='local')await upload(url,peer,bytes,identity,{timeoutSeconds});
  else {
    if(resumeJobId!==null) {
      jobId=resumeJobId;
      // Re-observe after WS/status preflight, immediately before the ONE apply.
      readyJob((await json('/api/firmware/remote')).value,jobId,peer,manifestSha,identity);
    } else {
      const staged=(await json('/api/firmware/fetch',{form:{url:manifest,sha256:manifestSha,targets:'1',apply:'false'}})).value;
      if(!Number.isSafeInteger(staged.job_id)||staged.job_id<1||staged.boot_id!==peer.boot)throw Error('Unknown fetch admission; inspect original job, no retry');
      jobId=staged.job_id;const deadline=now()+timeoutSeconds*1000;
    for(;;){
      const job=(await json('/api/firmware/remote')).value;
      if(job.job_id!==jobId||job.boot_id!==peer.boot||job.manifest_sha256!==manifestSha||job.targets!==1)throw Error('Remote job identity changed; no apply');
      log({stage:job.phase,received_bytes:job.received_bytes,expected_bytes:job.expected_bytes});
      if(job.phase==='ready'){
        readyJob(job,jobId,peer,manifestSha,identity);break;
      }
      if(!job.busy||now()>=deadline)throw Error('Remote stage failed/deadline; no retry or cancel');await wait(1000);
    }
    }
    const applyStarted=now();
    const result=(await json('/api/firmware/apply',{form:{job_id:String(jobId),boot_id:peer.boot,sha256:manifestSha}})).value;
    applyRequestMs=now()-applyStarted;
    if(result.job_id!==jobId||result.boot_id!==peer.boot)throw Error('Apply outcome unknown; no retry');
  }
  const transferMs=resumeJobId===null?now()-started:null,deadline=now()+timeoutSeconds*1000;let after;
  log({stage:'reconnect',transfer_ms:transferMs});
  for(;;){
    if(now()>=deadline)throw Error('Reboot verification deadline; no write retry');await wait(1500);
    // Old boot headers must NOT fence the new boot's read-only observation.
    // TCP address remains pinned, then board identity is independently checked.
    let response,resolved=false;
    try{response=await json('/api/status',{unfenced:true})}catch{
      // DHCP may change after reboot. Refresh hostname only after the cached
      // route fails; this is read-only, then independently rebind the same board.
      try{response=await json('/api/status',{unfenced:true,resolveHostname:true});resolved=true;}catch{continue;}
    }
    after=boardPeer(response.value,response.remote,before,{allowReboot:true,allowAddressChange:resolved});
    if(after.boot===before.boot)continue;peer=after;break;
  }
  if((await json('/api/auth')).value.enabled!==false)throw Error('Authentication changed after reboot');
  const telemetry=(await json('/api/telemetry')).value,e=telemetry.esp;
  if(telemetry.board_id!==peer.board||telemetry.boot_id!==peer.boot||e?.boot_integrity_verified!==true||
    e.sketch_bytes!==identity.size||e.sketch_md5!==identity.md5||e.ota?.running_offset!==e.ota?.boot_offset ||
    e.ota?.running_state_code!==2)throw Error('Running image identity/integrity/selected slot not verified');
  // Full running-image SHA, not just a POST reply or a repeated version string.
  const hash=createHash('sha256');
  for(let start=0;start<identity.size;start+=65536){const end=Math.min(start+65535,identity.size-1);
    const chunk=await read('/firmware/download',{headers:{Range:`bytes=${start}-${end}`}});
    if(chunk.status!==206||chunk.headers['content-range']!==`bytes ${start}-${end}/${identity.size}`||chunk.body.length!==end-start+1)throw Error('Running-image export incomplete; no flash retry');
    hash.update(chunk.body);
  }
  if(hash.digest('hex')!==identity.sha256)throw Error('Running-image SHA256 mismatch');
  await websocket(url,peer);
  if(route==='remote'){
    const result=(await json('/api/firmware/remote')).value;
    if(result.job_id!==jobId||result.phase!=='complete'||result.manifest_sha256!==manifestSha)throw Error('Remote completion not verified');
  }
  return {ok:true,path:route,origin:url.origin,board_id:peer.board,boot_before:before.boot,boot_after:peer.boot,
    image:identity,resume_job:resumeJobId,acknowledged_failed_job:acknowledgedFailure,transfer_ms:transferMs,
    ...(backupReceipt?{backup:backupReceipt,backup_ms:backupMs}:{}),
    ...(resumeJobId===null?{}:{apply_request_ms:applyRequestMs,download_timing:'previous invocation; not measured here'}),
    total_ms:now()-started,authentication:false,websocket:'same-peer status before/after',
    running_image:'full SHA256 readback and VALID selected/running slot',simulated:request!==deviceRequest||upload!==uploadImage||websocket!==checkWebSocket,
    avr_touched:false,usb_disconnect_ready:false,remaining:'External-power, Wi-Fi reconnect/AP recovery and physical matrix acceptance'};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(process.argv[1]).href){
  try{const options={};const args=process.argv.slice(2);
    for(let i=0;i<args.length;i++){
      if(args[i]==='--run'){options.run=true;continue;}
      if(args[i]==='--resume-backup'){options.resumeBackup=true;continue;}
      const key={'--origin':'origin','--image':'image','--mode':'mode','--manifest':'manifest','--manifest-sha':'manifestSha','--timeout':'timeoutSeconds','--resume-job':'resumeJobId','--ack-failed-job':'ackFailedJobId','--ack-failed-image':'ackFailedImage','--backup':'backup'}[args[i]];
      if(!key||!args[i+1]||args[i+1].startsWith('--'))throw Error('Usage: node tools/update-firmware.mjs --image ESP_BIN [--origin http://ledboard.local] [--mode auto|local|remote] [--manifest HTTPS_URL --manifest-sha SHA256] [--backup NEW_PATH [--resume-backup]] [--resume-job POSITIVE_ID (remote only) | --ack-failed-job POSITIVE_ID [--ack-failed-image ORIGINAL_FAILED_BIN]] [--run]');
      options[key]=['timeoutSeconds','resumeJobId','ackFailedJobId'].includes(key)?Number(args[++i]):args[++i];
    }
    options.log=value=>console.error(JSON.stringify(value));console.log(JSON.stringify(await updateFirmware(options),null,2));
  }catch(error){console.error(error.message);process.exitCode=1;}
}
