// Adapted from PU850 Tool/Go/download.go: bounded Range/length validation,
// Content-MD5 + whole-image integrity, resumable .part and commit-after-verify.
// 431104072952f694a35b3a83c19f13fbb098ca5b; owner-directed reuse, see notices.
import {open,lstat,link,unlink} from 'node:fs/promises';
import {resolve,dirname} from 'node:path';
import {createHash} from 'node:crypto';
import {pathToFileURL} from 'node:url';
import {deviceOrigin,deviceRequest} from './check-ethernet-live.mjs';
import {boardPeer} from './update-firmware.mjs';
export const maxDownloadSize=64*1024*1024;
const md5Pattern=/^[a-f0-9]{32}$/i;
export function parseContentMD5(value) {
  if(typeof value!=='string')throw Error('Invalid Content-MD5');
  if(md5Pattern.test(value))return value.toLowerCase();
  const bytes=Buffer.from(value,'base64');
  if(bytes.length!==16||bytes.toString('base64')!==value)throw Error('Invalid Content-MD5');
  return bytes.toString('hex');
}
// Actual C++ parse_header_md5/collect_download_hashes: Digest describes the
// complete representation; Content-MD5 on a partial response describes its body.
export function parseDigestMD5(value) {
  const match=typeof value==='string'&&/^md5=(.+)$/i.exec(value);
  if(!match||!/^[A-Za-z0-9+/]{22}==$/.test(match[1]))throw Error('Invalid firmware Digest header');
  return parseContentMD5(match[1]);
}
export function validateDownloadResponse(response,start,end,total) {
  if(response.status!==206||response.headers['content-range']!==`bytes ${start}-${end}/${total}`||
    response.headers['content-length']!==String(end-start+1)||response.body.length!==end-start+1)
    throw Error('Download Range/Content-Length mismatch; partial file retained');
  if(response.headers['content-encoding']&&response.headers['content-encoding']!=='identity')throw Error('Encoded firmware response rejected');
  if(response.headers['content-md5']&&parseContentMD5(response.headers['content-md5'])!==createHash('md5').update(response.body).digest('hex'))
    throw Error('Download section MD5 mismatch; partial file retained');
}
function validateWholeHashes(headers,expected,{section=false}={}) {
  if(!md5Pattern.test(headers['x-firmware-md5']??'')||headers['x-firmware-md5'].toLowerCase()!==expected)throw Error('Firmware MD5 identity header changed');
  if(headers.etag!==undefined&&(!/^"[a-f0-9]{32}"$/i.test(headers.etag)||headers.etag.slice(1,-1).toLowerCase()!==expected))throw Error('Firmware ETag changed');
  if(!section&&headers['content-md5']!==undefined&&parseContentMD5(headers['content-md5'])!==expected)
    throw Error('Firmware Content-MD5 conflicts with metadata');
  if(headers.digest!==undefined&&parseDigestMD5(headers.digest)!==expected)throw Error('Firmware Digest conflicts with metadata');
}
async function regularFile(path) {
  try {const value=await lstat(path);if(!value.isFile()||value.isSymbolicLink())throw Error('Output/partial path must be a regular non-symlink file');return value;}
  catch(error){if(error.code==='ENOENT')return null;throw error;}
}
async function digest(file,size) {
  const md5=createHash('md5'),sha=createHash('sha256'),chunk=Buffer.alloc(65536);
  for(let at=0;at<size;){const {bytesRead}=await file.read(chunk,0,Math.min(chunk.length,size-at),at);if(!bytesRead)throw Error('Partial file changed during verification');md5.update(chunk.subarray(0,bytesRead));sha.update(chunk.subarray(0,bytesRead));at+=bytesRead;}
  const probe=Buffer.alloc(1);if((await file.read(probe,0,1,size)).bytesRead)throw Error('Partial file grew during verification');
  return {md5:md5.digest('hex'),sha256:sha.digest('hex')};
}
export async function downloadFirmware({origin='http://ledboard.local',output,resume=false,run=false,timeoutSeconds=180,
  environment=process.env,request=deviceRequest,now=Date.now,signal}={}) {
  const url=deviceOrigin(origin);
  if(typeof output!=='string'||!output.trim())throw Error('An explicit output path is required');
  if(!Number.isInteger(timeoutSeconds)||timeoutSeconds<10||timeoutSeconds>600)throw Error('Timeout must be 10..600 seconds');
  const destination=resolve(output),part=destination+'.part';
  if(!(await lstat(dirname(destination))).isDirectory())throw Error('Output parent is not a directory');
  if(await regularFile(destination))throw Error('Destination already exists; choose a new backup path');
  const prior=await regularFile(part);
  if(prior&&!resume)throw Error('Partial file already exists; explicitly resume or choose a new path');
  if(prior&&prior.size>maxDownloadSize)throw Error('Partial file exceeds the 64 MiB bound');
  if(!run)return {plan:true,origin:url.origin,output:destination,resume,device_contacted:false,firmware_written:false};
  const user=environment.PAKAR_DOWNLOAD_USERNAME,password=environment.PAKAR_DOWNLOAD_PASSWORD;
  if((user===undefined)!==(password===undefined))throw Error('Set both PAKAR_DOWNLOAD_USERNAME/PASSWORD or neither');
  if(user!==undefined&&(!/^[A-Za-z0-9._-]{1,32}$/.test(user)||!/^[\x20-\x7e]{8,64}$/.test(password)))throw Error('Invalid download credentials');
  const auth=user===undefined?{}:{Authorization:'Basic '+Buffer.from(user+':'+password).toString('base64')};
  const deadline=now()+timeoutSeconds*1000;let peer;
  const check=()=>{if(signal?.aborted)throw Error('Download cancelled; partial file retained');if(now()>=deadline)throw Error('Download deadline; partial file retained');};
  const read=async(path,options={})=>{
    check();const r=await request(url,path,{...options,expectedAddress:peer?.address,
      headers:{...auth,...(peer?{'X-Pakar-Board':peer.board,'X-Pakar-Boot':peer.boot}:{}),...options.headers}});check();
    // C++ single_header rejects duplicate authority/integrity values even when
    // a transport might otherwise discard or coalesce identical duplicates.
    for(const name of ['content-length','content-range','content-md5','digest','x-firmware-md5','etag','x-pakar-board','x-pakar-boot'])
      if(r.distinctHeaders?.[name]&&r.distinctHeaders[name].length!==1)throw Error('Duplicate critical firmware header');
    if(r.status<200||r.status>=300)throw Error('Download read rejected (HTTP '+r.status+'); no retry');
    if(peer&&(r.remote!==peer.address||r.headers['x-pakar-board']!==peer.board||r.headers['x-pakar-boot']!==peer.boot))throw Error('Download peer/boot changed; partial file retained');
    return r;
  };
  const json=async(path)=>{const r=await read(path);let value;try{value=JSON.parse(r.body)}catch{throw Error('Invalid download metadata JSON')};return {value,remote:r.remote};};
  const first=await json('/api/status');peer=boardPeer(first.value,first.remote);
  const info=(await json('/update/info')).value;
  if(info.product!=='PakarController'||!md5Pattern.test(info.hash??'')||!Number.isSafeInteger(info.size)||info.size<24||info.size>maxDownloadSize)
    throw Error('Invalid running firmware metadata');
  const expected=info.hash.toLowerCase(),total=info.size;
  if(prior&&prior.size>total)throw Error('Partial file is larger than running firmware');
  const head=await read('/firmware/download',{method:'HEAD'});
  if(head.status!==200||head.headers['content-length']!==String(total))throw Error('Firmware HEAD length/status conflicts with metadata');
  validateWholeHashes(head.headers,expected);
  let file;
  try {
    // Exclusive creation preserves unknown partials. Resume never truncates.
    file=await open(part,prior?'r+':'wx+',0o600);
    const opened=await file.stat();
    if(prior&&(opened.ino!==prior.ino||opened.dev!==prior.dev||opened.size!==prior.size))throw Error('Partial file changed before resume');
    for(let at=prior?.size??0;at<total;){
      const end=Math.min(at+65535,total-1),r=await read('/firmware/download',{headers:{Range:`bytes=${at}-${end}`,'Accept-Encoding':'identity'}});
      validateDownloadResponse(r,at,end,total);validateWholeHashes(r.headers,expected,{section:true});
      let written=0;while(written<r.body.length){const n=(await file.write(r.body,written,r.body.length-written,at+written)).bytesWritten;if(!n)throw Error('Partial file write made no progress');written+=n;}
      at=end+1;
    }
    await file.sync();check();const beforeDigest=await file.stat(),identity=await digest(file,total),verified=await file.stat();
    // PU850 watch.go digestUnchangedFile: content hash plus size/time/mode
    // stability, rather than metadata-only success or trusting a stale path.
    const unchanged=(a,b)=>a.ino===b.ino&&a.dev===b.dev&&a.size===b.size&&a.mode===b.mode&&a.mtimeMs===b.mtimeMs&&a.ctimeMs===b.ctimeMs;
    if(!unchanged(beforeDigest,verified))throw Error('Partial file changed during verification');
    if(identity.md5!==expected)throw Error('Full firmware MD5 mismatch; partial file retained');
    const last=await json('/api/status');boardPeer(last.value,last.remote,peer);
    const after=await file.stat(),named=await regularFile(part);
    if(!named||!unchanged(verified,after)||!unchanged(after,named)||after.size!==total)throw Error('Partial path changed before commit');
    check();await file.close();file=null;
    // A hard-link create is atomic and fails if the destination appeared. It
    // never deletes/replaces another backup; both names hold verified bytes.
    await link(part,destination);
    let partialRetained=false;try{await unlink(part)}catch{partialRetained=true;}
    return {ok:true,origin:url.origin,output:destination,board_id:peer.board,boot_id:peer.boot,
      size:total,...identity,resumed_bytes:prior?.size??0,partial_retained:partialRetained,
      firmware_written:false,simulated:request!==deviceRequest};
  } finally {if(file)await file.close();}
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
  const controller=new AbortController(),cancel=()=>controller.abort();process.once('SIGINT',cancel);
  try {const options={signal:controller.signal},args=process.argv.slice(2);
    for(let i=0;i<args.length;i++){
      if(args[i]==='--run'){options.run=true;continue;}if(args[i]==='--resume'){options.resume=true;continue;}
      const key={'--origin':'origin','--output':'output','--timeout':'timeoutSeconds'}[args[i]];
      if(!key||!args[i+1]||args[i+1].startsWith('--'))throw Error('Use --output ABSOLUTE_PATH [--origin HTTP(S)_HOSTNAME --resume --timeout 180 --run]');
      options[key]=key==='timeoutSeconds'?Number(args[++i]):args[++i];
    }
    process.stdout.write(JSON.stringify(await downloadFirmware(options),null,2)+'\n');
  }catch(error){process.stderr.write(error.message+'\n');process.exitCode=controller.signal.aborted?130:1;}
  finally{process.removeListener('SIGINT',cancel);}
}
