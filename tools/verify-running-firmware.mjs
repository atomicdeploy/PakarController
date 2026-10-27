// Read-only post-deployment verification, without another firmware copy/flash.
// Reuses PU850 upload.js verifiedDownload/verifyAfterReboot adaptations already
// owned by update-firmware, download-firmware and the shared peer transport.
import {open, lstat, writeFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {dirname, resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {performance} from 'node:perf_hooks';
import {deviceOrigin, deviceRequest} from './check-ethernet-live.mjs';
import {imageIdentity, checkWebSocket, boundedReadRequest} from './update-firmware.mjs';
import {validateDownloadResponse, parseContentMD5, parseDigestMD5, maxDownloadSize, verificationRangeBytes} from './download-firmware.mjs';
export {verificationRangeBytes} from './download-firmware.mjs';
import {boardPeer} from './peer-identity.mjs';

const criticalHeaders=['content-length','content-range','content-md5','digest','x-firmware-md5','etag','x-pakar-board','x-pakar-boot'];

function runningMetadata(value,peer,image) {
  const esp=value?.esp,ota=esp?.ota;
  if(value?.board_id!==peer.board||value.boot_id!==peer.boot||esp?.boot_integrity_verified!==true||
    esp.sketch_bytes!==image.size||esp.sketch_md5!==image.md5||
    !Number.isSafeInteger(ota?.running_offset)||ota.running_offset<=0||ota.running_offset%65536!==0||
    ota.running_offset!==ota.boot_offset||ota.running_state_code!==2)
    throw Error('Running image size/MD5/integrity/VALID selected slot differs; no firmware write attempted');
  return {running_offset:ota.running_offset,boot_offset:ota.boot_offset,running_state_code:ota.running_state_code};
}

function wholeHash(headers,image,{head=false}={}) {
  if(headers['x-firmware-md5']?.toLowerCase()!==image.md5||
    (headers.etag!==undefined&&headers.etag.toLowerCase()!==`"${image.md5}"`)||
    (headers.digest!==undefined&&parseDigestMD5(headers.digest)!==image.md5)||
    (head&&headers['content-md5']!==undefined&&parseContentMD5(headers['content-md5'])!==image.md5))
    throw Error('Running firmware hash header differs; no retry');
}

export async function verifyRunningFirmware({origin,board,image,receipt,
  timeoutMs=120000,request=deviceRequest,websocket=checkWebSocket,now=()=>performance.now(),log=()=>{}}={}) {
  const url=deviceOrigin(origin);
  if(!/^[a-z0-9_-]{1,64}$/.test(board??''))throw Error('An explicit exact --board ID is required');
  if(!Number.isSafeInteger(timeoutMs)||timeoutMs<20||timeoutMs>120000)throw Error('Verification budget must be20..120000ms');
  if(typeof receipt!=='string'||!receipt.trim())throw Error('An explicit new JSON receipt path is required');
  const destination=resolve(receipt);
  if(!(await lstat(dirname(destination))).isDirectory())throw Error('Receipt parent must already exist');
  try{await lstat(destination);throw Error('Receipt already exists; preserve it and choose a new path');}
  catch(error){if(error.code!=='ENOENT')throw error;}
  let bytes;
  if(Buffer.isBuffer(image))bytes=image;
  else {
    // Read the validated open descriptor, never reopen a checked pathname.
    const handle=await open(image,'r');
    try{
      const file=await handle.stat(),path=await lstat(image);
      if(!file.isFile()||!path.isFile()||path.isSymbolicLink()||file.dev!==path.dev||file.ino!==path.ino||file.size>maxDownloadSize)
        throw Error('Candidate must be a bounded regular application image');
      bytes=await handle.readFile();
      const after=await handle.stat();
      if(bytes.length!==file.size||after.size!==file.size||after.mtimeMs!==file.mtimeMs)
        throw Error('Candidate changed while reading');
    }finally{await handle.close();}
  }
  if(bytes.length>maxDownloadSize)throw Error('Candidate exceeds firmware size bound');
  const expected=imageIdentity(bytes);
  bytes=null; // Only bounded response chunks are retained during readback.
  const started=now(),deadline=started+timeoutMs;
  let peer=null,retries=0,ranges=0;
  const check=()=>{if(now()>=deadline)throw Error('Read-only verification deadline; no upload, reset or replay attempted');};
  // Existing transport caps each attempt at8s. Every initial attempt AND retry
  // checks the overall budget; worst final-read overrun is8s, never unbounded.
  const guardedRequest=async(...args)=>{check();const response=await request(...args);check();return response;};
  const read=async(path,options={})=>{
    const response=await boundedReadRequest(guardedRequest,url,path,{...options,method:options.method??'GET',
      expectedAddress:peer?.address,headers:{'Accept-Encoding':'identity',
        ...(peer?{'X-Pakar-Board':peer.board,'X-Pakar-Boot':peer.boot}:{}),...options.headers}},event=>{retries++;log(event);});
    for(const name of criticalHeaders)if(response.distinctHeaders?.[name]&&response.distinctHeaders[name].length!==1)
      throw Error('Duplicate critical response header; no retry');
    if(response.status<200||response.status>=300)throw Error(`Read rejected (HTTP ${response.status}); no retry`);
    if(response.headers['content-encoding']&&response.headers['content-encoding']!=='identity')throw Error('Encoded verification response rejected');
    if(peer&&(response.remote!==peer.address||response.headers['x-pakar-board']!==peer.board||response.headers['x-pakar-boot']!==peer.boot))
      throw Error('Readback peer/boot changed; no retry');
    return response;
  };
  const json=async path=>{const response=await read(path);let value;
    try{value=JSON.parse(response.body)}catch{throw Error('Malformed verification JSON; no retry');}
    return {value,response};
  };
  const first=await json('/api/status');peer=boardPeer(first.value,first.response.remote);
  if(peer.board!==board)throw Error('Connected board does not match exact requested ID; no further requests');
  if(first.response.headers['x-pakar-board']!==peer.board||first.response.headers['x-pakar-boot']!==peer.boot)
    throw Error('Initial response board/boot headers do not match admitted peer');
  const metadata=async()=>{
    const info=(await json('/update/info')).value;
    if(info?.product!=='PakarController'||!info.model?.startsWith('esp32s3')||info.size!==expected.size||info.hash!==expected.md5)
      throw Error('Running firmware metadata differs from candidate; no firmware write attempted');
    return runningMetadata((await json('/api/telemetry')).value,peer,expected);
  };
  const slot=await metadata();
  check();const remaining=Math.floor(deadline-now());
  if(remaining<20)throw Error('Verification deadline before WebSocket observation');
  const connectTimeoutMs=Math.min(5000,Math.floor(remaining/2));
  const statusTimeoutMs=Math.min(12000,remaining-connectTimeoutMs);
  const ws=await websocket(url,peer,{connectTimeoutMs,statusTimeoutMs});check();
  if(ws?.board_id!==peer.board||ws.boot_id!==peer.boot||ws.status_requests!==1)throw Error('WebSocket same-peer readback unverified');
  const head=await read('/firmware/download',{method:'HEAD'});
  if(head.status!==200||head.headers['content-length']!==String(expected.size))throw Error('Running firmware HEAD length/status differs');
  wholeHash(head.headers,expected,{head:true});
  const sha=createHash('sha256'),md5=createHash('md5');
  for(let start=0;start<expected.size;start+=verificationRangeBytes){
    const end=Math.min(start+verificationRangeBytes-1,expected.size-1);
    const response=await read('/firmware/download',{headers:{Range:`bytes=${start}-${end}`}});
    validateDownloadResponse(response,start,end,expected.size);wholeHash(response.headers,expected);
    sha.update(response.body);md5.update(response.body);ranges++;
    log({stage:'readback',verified_bytes:end+1,expected_bytes:expected.size});
  }
  if(sha.digest('hex')!==expected.sha256||md5.digest('hex')!==expected.md5)throw Error('Running-image SHA256/MD5 mismatch; no firmware write attempted');
  const last=await json('/api/status');boardPeer(last.value,last.response.remote,peer);
  const finalSlot=await metadata();
  if(finalSlot.running_offset!==slot.running_offset)throw Error('Running firmware slot changed during readback');
  check();
  const result={ok:true,read_only:true,origin:url.origin,board_id:peer.board,boot_id:peer.boot,address:peer.address,
    image:expected,slot,websocket:ws,range_bytes:verificationRangeBytes,range_count:ranges,transport_retries:retries,
    elapsed_ms:Math.round(now()-started),request_budget_ms:timeoutMs,max_inflight_read_overrun_ms:8000,
    firmware_written:false,avr_touched:false,simulated:request!==deviceRequest||websocket!==checkWebSocket,
    remaining:'Physical LED/AVR output and USB-disconnection acceptance are not established by this read-only receipt'};
  // Exclusive creation: another process's receipt can never be overwritten.
  await writeFile(destination,JSON.stringify(result,null,2)+'\n',{flag:'wx',mode:0o600});
  return result;
}

export function parseVerificationArguments(args) {
  const options={},keys={'--origin':'origin','--board':'board','--image':'image','--receipt':'receipt'};
  for(let i=0;i<args.length;i++){
    const key=keys[args[i]],value=args[++i];
    if(!key||typeof value!=='string'||!value||value.startsWith('--')||options[key]!==undefined)
      throw Error('Use --origin HTTP(S)_HOSTNAME --board EXACT_ID --image CANDIDATE_BIN --receipt NEW_JSON_PATH (read-only)');
    options[key]=value;
  }
  if(Object.keys(options).length!==4)throw Error('All of --origin, --board, --image and --receipt are required');
  return options;
}

if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
  try{
    const options=parseVerificationArguments(process.argv.slice(2));
    const result=await verifyRunningFirmware({...options,log:event=>process.stderr.write(JSON.stringify(event)+'\n')});
    process.stdout.write(JSON.stringify(result,null,2)+'\n');
  }catch(error){process.stderr.write(error.message+'\n');process.exitCode=1;}
}
