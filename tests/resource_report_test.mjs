import assert from 'node:assert/strict';
import {avrStaticRam,otaCapacity,metric,compareBudgets,liveResources,observationStatus,fetchResources,buildMarkdown,collectBuild} from '../tools/resource-report.mjs';
import {mkdirSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {resolve} from 'node:path';
const elf=Buffer.alloc(172);elf.write('7f454c46',0,'hex');elf[4]=1;elf[5]=1;elf.writeUInt16LE(83,18);elf.writeUInt32LE(52,32);elf.writeUInt16LE(40,46);elf.writeUInt16LE(3,48);
for(const [i,address,size] of [[0,0x800100,100],[1,0x800164,200],[2,0x810000,50]]){const at=52+40*i;elf.writeUInt32LE(2,at+8);elf.writeUInt32LE(address,at+12);elf.writeUInt32LE(size,at+20);}
assert.equal(avrStaticRam(elf),300);assert.throws(()=>avrStaticRam(elf.subarray(0,60)));elf[5]=2;assert.throws(()=>avrStaticRam(elf));
const pt=Buffer.alloc(96,255);for(let i=0;i<2;i++){const at=i*32;pt.writeUInt16LE(0x50aa,at);pt[at+2]=0;pt[at+3]=0x10+i;pt.writeUInt32LE(3342336-i*4096,at+8);}
assert.equal(otaCapacity(pt),3338240);assert.throws(()=>otaCapacity(pt.subarray(0,32)));
assert.equal(metric(0,2048).used_percent,0);assert.throws(()=>metric(null,2048));
const rows=[{target:'avr',profile:'same',metrics:{ram:metric(1800,2048)}}],policy={ram:{max_used_percent:90,max_growth_bytes:128}};
const baseline={source_sha:'baseline',source_dirty:false,boards:[{target:'avr',profile:'same',metrics:{ram:metric(1600,2048)}}]};
assert.match(compareBudgets(rows,baseline,policy).violations[0],/grew 200/);
assert.equal(compareBudgets(rows,{...baseline,source_dirty:true},policy).comparisons.length,0);
assert.throws(()=>compareBudgets(rows,null,{ram:{max_used_percent:null}}));
baseline.boards[0].profile='different';assert.equal(compareBudgets(rows,baseline,policy).comparisons.length,0);
assert.match(compareBudgets([{...rows[0],metrics:{ram:metric(2050,2048)}}],null,policy).violations[0],/physical/);
assert.match(buildMarkdown({source_sha:'abc',boards:rows,...compareBudgets(rows,null,policy),scope:'static only'}),/static only/);
const telemetry={type:'telemetry',product:'PakarController',board_id:'board',boot_id:'boot',esp:{internal_heap_free_bytes:74000,psram_bytes:8388608,free_psram_bytes:8300000,filesystem:{known:true,total_bytes:100,used_bytes:10,age_ms:90000}},avr:{online:true,telemetry_recent:false,free_sram_bytes:293}};
const live=liveResources(telemetry,{observedAt:'2026-10-27T00:00:00Z'});
assert.equal(live.esp.internal_heap_free_bytes,74000);assert.equal(live.avr.free_sram_bytes,null);assert.equal(live.avr.status,'stale');assert.equal(live.esp.filesystem.age_ms,90000);
assert.equal(observationStatus(live,Date.parse(live.observed_at)+15001),'stale');assert.equal(observationStatus(live,Date.parse(live.observed_at)+1),'observed');
assert.throws(()=>liveResources({...telemetry,board_id:null}));
const result=await fetchResources('http://ledcontroller.local',async(url,path,options)=>{assert.equal(path,'/api/telemetry');assert.deepEqual(options.headers,{});return {status:200,body:Buffer.from(JSON.stringify(telemetry))};},{});
assert.equal(result.board_id,'board');await assert.rejects(()=>fetchResources('http://ledcontroller.local',async()=>({status:302}),{}),/HTTP 302/);
const dir=fileURLToPath(new URL('../build/tests/resource-report',import.meta.url));
const artifacts=[];elf[5]=1;
for(const target of ['avr','avr-busy-wire','avr-buffered','esp32s3-buffered']){
  const folder=resolve(dir,target);mkdirSync(folder,{recursive:true});
  const avr=target.startsWith('avr'),name=avr?'avr.ino.with_bootloader.bin':'esp32s3.ino.bin',bytes=Buffer.alloc(avr?32768:2048);
  const sha=createHash('sha256').update(bytes).digest('hex');writeFileSync(resolve(folder,name),bytes);
  artifacts.push({path:target+'/'+name,format:'raw',sha256:sha});
  writeFileSync(resolve(folder,'resource-build.json'),JSON.stringify({artifact_sha256:sha,fqbn:'fixture',version:'0.2.8',numeric_feature_flags:{PAKAR_NETWORK_ENABLED:1}}));
  if(avr){writeFileSync(resolve(folder,'avr-image.json'),JSON.stringify({binary_sha256:sha,structural_validation:true,application_bytes:1000,boot_start:32384,profile:'fixture'}));writeFileSync(resolve(folder,'avr.ino.elf'),elf);}
  else writeFileSync(resolve(folder,'esp32s3.ino.partitions.bin'),pt);
}
writeFileSync(resolve(dir,'firmware-report.json'),JSON.stringify({source_sha:'fixture',source_dirty:false,artifacts}));
const budgets={avr_flash:{max_used_percent:98,max_growth_bytes:2048},avr_static_ram:{max_used_percent:90,max_growth_bytes:128},esp_ota:{max_used_percent:95,max_growth_bytes:262144}};
const collected=collectBuild(dir,budgets);assert.equal(collected.boards.length,4);assert.equal(collected.boards[0].build.version,'0.2.8');assert.equal(collected.violations.length,0);
writeFileSync(resolve(dir,'avr/resource-build.json'),JSON.stringify({artifact_sha256:'incorrect'}));assert.throws(()=>collectBuild(dir,budgets),/metadata/);
console.log('Resource reports: ELF/partition bounds, physical and growth budgets, missing values, stale AVR/filesystem and read-only fetching passed.');
