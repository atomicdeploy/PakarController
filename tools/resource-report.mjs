// Extend the PU850 size-report adaptation in firmware-report.mjs. Read artifacts,
// never compiler console prose; a padded AVR recovery image is NOT used flash.
import {readFileSync,writeFileSync,existsSync} from 'node:fs';
import {resolve,dirname} from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
import {createHash} from 'node:crypto';
import {unchangedWrite} from './firmware-report.mjs';
import {deviceOrigin,deviceRequest} from './check-ethernet-live.mjs';
const hash=b=>createHash('sha256').update(b).digest('hex');
const number=n=>Number.isSafeInteger(n)&&n>=0?n:null;
const json=p=>JSON.parse(readFileSync(p,'utf8'));
const root=resolve(dirname(fileURLToPath(import.meta.url)),'..');

export function avrStaticRam(bytes) {
  if(bytes.length<52||bytes.subarray(0,4).toString('hex')!=='7f454c46'||bytes[4]!==1||bytes[5]!==1||bytes.readUInt16LE(18)!==83)throw Error('Expected little-endian ELF32 AVR');
  const start=bytes.readUInt32LE(32),size=bytes.readUInt16LE(46),count=bytes.readUInt16LE(48);
  if(size!==40||!count||start+size*count>bytes.length)throw Error('Invalid AVR ELF section table');
  let ram=0;
  for(let i=0;i<count;i++){
    const at=start+i*size,flags=bytes.readUInt32LE(at+8),address=bytes.readUInt32LE(at+12),length=bytes.readUInt32LE(at+20);
    // AVR data-space sections (.data/.bss/.noinit); excludes EEPROM at0x810000.
    if((flags&2)&&address>=0x800100&&address<0x810000)ram+=length;
  }
  return ram;
}
export function otaCapacity(bytes) {
  if(bytes.length<32||bytes.length%32)throw Error('Invalid partition table length');
  const sizes=[];
  for(let at=0;at+32<=bytes.length;at+=32){
    const magic=bytes.readUInt16LE(at);if(magic===0xffff||magic===0xebeb)break;
    if(magic!==0x50aa)throw Error('Invalid partition entry');
    if(bytes[at+2]===0&&bytes[at+3]>=0x10&&bytes[at+3]<=0x1f){const n=bytes.readUInt32LE(at+8);if(!n)throw Error('Empty OTA slot');sizes.push(n);}
  }
  if(sizes.length<2)throw Error('Two OTA application slots required');
  return Math.min(...sizes);
}
export function metric(used,capacity){
  if(number(used)===null||number(capacity)===null||!capacity)throw Error('Invalid resource measurement');
  return {used_bytes:used,capacity_bytes:capacity,remaining_bytes:capacity-used,used_percent:Math.round(used/capacity*10000)/100};
}
export function compareBudgets(rows,baseline,policy){
  const violations=[],comparisons=[];
  for(const row of rows)for(const [name,m] of Object.entries(row.metrics)){
    const rule=policy[name];if(!rule||!Number.isFinite(rule.max_used_percent)||rule.max_used_percent<=0||rule.max_used_percent>100||number(rule.max_growth_bytes)===null)throw Error('Invalid budget policy: '+name);
    if(m.used_bytes>m.capacity_bytes)violations.push(`${row.target}/${name}: exceeds physical capacity`);
    if(m.used_percent>rule.max_used_percent)violations.push(`${row.target}/${name}: exceeds ${rule.max_used_percent}% build budget`);
    const old=baseline?.source_dirty===false?baseline?.boards?.find(b=>b.target===row.target&&b.profile===row.profile)?.metrics?.[name]:null;
    if(old&&number(old.used_bytes)!==null&&old.capacity_bytes===m.capacity_bytes){
      const delta=m.used_bytes-old.used_bytes;comparisons.push({target:row.target,metric:name,delta_bytes:delta,baseline_source:baseline.source_sha});
      if(delta>rule.max_growth_bytes)violations.push(`${row.target}/${name}: grew ${delta} bytes (limit ${rule.max_growth_bytes})`);
    }
  }
  return {violations,comparisons,baseline_status:comparisons.length?'compared':'no comparable baseline'};
}
export function collectBuild(directory,policy,baseline=null){
  const manifest=json(resolve(directory,'firmware-report.json')),boards=[];
  for(const a of manifest.artifacts){
    if(a.format!=='raw'||!/^avr(?:-busy-wire|-buffered)?\/avr\.ino\.with_bootloader\.bin$|^esp32s3-buffered\/esp32s3\.ino\.bin$/.test(a.path))continue;
    const path=resolve(directory,a.path),bytes=readFileSync(path);
    if(hash(bytes)!==a.sha256)throw Error('Artifact no longer matches firmware report: '+a.path);
    const target=a.path.split('/')[0],folder=dirname(path),metadataPath=resolve(folder,'resource-build.json');
    const build=existsSync(metadataPath)?json(metadataPath):null;
    if(build&&build.artifact_sha256!==a.sha256)throw Error('Resource build metadata no longer matches artifact');
    const details={build:build??{status:'unavailable; existing artifact predates build metadata'}};
    if(target.startsWith('avr')){
      const image=json(resolve(folder,'avr-image.json'));
      if(image.binary_sha256!==a.sha256||image.structural_validation!==true)throw Error('AVR packaging identity mismatch');
      boards.push({target,profile:image.profile,...details,artifact_sha256:a.sha256,metrics:{avr_flash:metric(image.application_bytes,image.boot_start),avr_static_ram:metric(avrStaticRam(readFileSync(resolve(folder,'avr.ino.elf'))),2048)}});
    }else{
      const partitions=readFileSync(resolve(folder,'esp32s3.ino.partitions.bin'));
      boards.push({target,profile:(build?.fqbn??'unknown-fqbn')+':partition-sha256:'+hash(partitions),...details,artifact_sha256:a.sha256,metrics:{esp_ota:metric(bytes.length,otaCapacity(partitions))}});
    }
  }
  if(boards.length!==4)throw Error('Expected ESP32-S3 plus three AVR link profiles');
  return {kind:'build-resources',source_sha:manifest.source_sha,source_dirty:manifest.source_dirty,scope:'Existing artifact bytes; AVR static RAM excludes stack and runtime heap. Not live board usage.',boards,...compareBudgets(boards,baseline,policy)};
}
export function liveResources(t,{origin,observedAt=new Date().toISOString(),elapsedMs=0}={}){
  if(t.type!=='telemetry'||t.product!=='PakarController'||typeof t.board_id!=='string'||!t.board_id||typeof t.boot_id!=='string'||!t.boot_id)throw Error('Missing telemetry board/boot identity');
  const e=t.esp??{},a=t.avr??{},f=e.filesystem??{};
  const recent=a.online===true&&a.telemetry_recent===true;
  return {kind:'live-resources',origin,observed_at:observedAt,request_duration_ms:elapsedMs,status:'observed',stale_after_ms:15000,
    board_id:t.board_id,boot_id:t.boot_id,hostname:t.hostname,version:t.version,
    esp:{model:e.model,sketch_bytes:number(e.sketch_bytes),sketch_md5:e.sketch_md5??null,image_identity_scope:e.image_identity_scope??null,
      // free-sketch-space is not promised to be the current slot's capacity.
      ota_free_bytes:number(e.ota_free_bytes),uptime_ms:number(e.uptime_ms),internal_heap_free_bytes:number(e.internal_heap_free_bytes),internal_heap_largest_block_bytes:number(e.internal_heap_largest_block_bytes),internal_heap_minimum_free_bytes:number(e.internal_heap_minimum_free_bytes),psram_bytes:number(e.psram_bytes),free_psram_bytes:number(e.free_psram_bytes),uart_errors:number(e.uart_errors),
      filesystem:{scope:'cached_observation',known:f.known===true,total_bytes:f.known===true?number(f.total_bytes):null,used_bytes:f.known===true?number(f.used_bytes):null,age_ms:number(f.age_ms),captured_uptime_ms:number(f.captured_uptime_ms)}},
    avr:{status:recent?'observed':a.online===true?'stale':'offline',telemetry_age_ms:number(a.telemetry_age_ms),version:a.version??null,uptime_ms:recent?number(a.uptime_ms):null,free_sram_bytes:recent?number(a.free_sram_bytes):null,sram_capacity_bytes:number(a.sram_capacity_bytes),uart_errors:recent?number(a.uart_errors):null,lost_events:recent?number(a.lost_events):null},
    unmeasured:['CPU utilization','flash-write wear']};
}
export function observationStatus(report,now=Date.now()){
  if(report.status!=='observed')return report.status;
  const at=Date.parse(report.observed_at);return !Number.isFinite(at)||now<at||now-at>report.stale_after_ms?'stale':'observed';
}
export async function fetchResources(origin,request=deviceRequest,environment=process.env){
  const url=deviceOrigin(origin),start=Date.now();
  const user=environment.PAKAR_RESOURCE_USERNAME,password=environment.PAKAR_RESOURCE_PASSWORD;
  if((user===undefined)!==(password===undefined))throw Error('Set both resource credentials or neither');
  const response=await request(url,'/api/telemetry',{headers:user===undefined?{}:{Authorization:'Basic '+Buffer.from(user+':'+password).toString('base64')}});
  if(response.status!==200)throw Error('Telemetry HTTP '+response.status);
  return liveResources(JSON.parse(response.body.toString('utf8')),{origin:url.origin,elapsedMs:Date.now()-start});
}
export function buildMarkdown(r){
  return '# Build resource budgets\n\nSource: `'+r.source_sha+'` · '+r.baseline_status+'\n\n| Target | Resource | Used / capacity (bytes) | Used |\n|---|---|---:|---:|\n'+r.boards.flatMap(b=>Object.entries(b.metrics).map(([k,v])=>`| ${b.target} | ${k} | ${v.used_bytes} / ${v.capacity_bytes} | ${v.used_percent}% |`)).join('\n')+'\n\n'+r.scope+'\n\n'+r.comparisons.map(c=>`${c.target}/${c.metric}: ${c.delta_bytes>=0?'+':''}${c.delta_bytes} bytes against ${c.baseline_source}`).join('\n')+'\n'+r.violations.map(v=>'- '+v).join('\n')+'\n';
}
export async function main(args=process.argv.slice(2)){
  const options={};for(let i=0;i<args.length;i++){
    const k=args[i];if(k==='--write'){options.write=true;continue;}
    if(!['--origin','--directory','--baseline','--output'].includes(k)||!args[i+1]||args[i+1].startsWith('--'))throw Error('Use --origin URL [--output PATH] or --directory PATH [--baseline FILE] [--write]');options[k.slice(2)]=args[++i];
  }
  let report;
  if(options.origin){
    try{report=await fetchResources(options.origin);}catch(error){report={kind:'live-resources',origin:deviceOrigin(options.origin).origin,status:'unavailable',attempted_at:new Date().toISOString(),error:error.message};process.exitCode=1;}
  }else{
    const directory=resolve(options.directory??resolve(root,'build'));
    const baseline=options.baseline?json(options.baseline):null;
    if(baseline&&(baseline.kind!=='build-resources'||!Array.isArray(baseline.boards)))throw Error('Invalid build resource baseline');
    report=collectBuild(directory,json(resolve(root,'interfaces/resource-budgets.json')),baseline);
    if(options.write)unchangedWrite(resolve(directory,'resource-report.json'),JSON.stringify(report,null,2)+'\n');
    if(process.env.GITHUB_STEP_SUMMARY)writeFileSync(process.env.GITHUB_STEP_SUMMARY,buildMarkdown(report),{flag:'a'});
    if(report.violations.length)process.exitCode=1;
  }
  if(options.output)unchangedWrite(resolve(options.output),JSON.stringify(report,null,2)+'\n');
  console.log(JSON.stringify(report,null,2));return report;
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href)main().catch(e=>{console.error(e.message);process.exitCode=1;});
