import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
const root=fileURLToPath(new URL('..',import.meta.url));
const read=p=>readFileSync(resolve(root,p),'utf8').replaceAll('\r\n','\n');
const hash=text=>createHash('sha256').update(text).digest('hex');
const value=JSON.parse(read('docs/PU850-ROUTE-PARITY.json'));
assert.equal(value.revision,'431104072952f694a35b3a83c19f13fbb098ca5b');
assert.deepEqual(value.migration.platforms,['ESP8266','ESP32-S3']);
assert.match(value.migration.review,/registration slice 560-1554/);
assert.match(value.migration.diagnostic_build,/USB-only passive UART2 RX18 capture/);
assert.match(value.migration.diagnostic_build,/not a compatible AVR protocol/);
for(const gap of [/LAN GET\/HEAD\/Range/,/ISP.*disabled/,/WINS registration.*LLMNR/,/TUI addressing editing/,/translated=false/,/SDK default blocking deadline/])
 assert(value.migration.remaining.some(text=>gap.test(text)),'Migration omission needs a concrete remaining acceptance step: '+gap);
assert.match(read('firmware/esp32s3/src/Discovery.h'),/bool netbiosReady\(\) const \{ return nbns_\.running\(\); \}/);
assert.match(value.migration.remaining.join('\n'),/S3 NBNS name\/node-status queries are implemented/);
assert.equal(value.routes.length,50);const identities=new Set();
for(const row of value.routes){
 const id=row.source+':'+row.line;assert(!identities.has(id));identities.add(id);
 assert.match(value.sources[row.source].sha256,/^[0-9a-f]{64}$/);
 assert(row.reason.length>30);
 assert(['adapted','imported','not-applicable','commented-business','commented-replaced'].includes(row.status));
 if(row.target){const text=read(row.target);assert.equal(hash(text),row.target_sha256_utf8_lf,row.target+' needs re-review');assert(text.split('\n')[row.target_line-1].includes(row.anchor));}
 else assert(['not-applicable','commented-business'].includes(row.status));
}
if(process.env.PAKAR_PU_DONOR_PATH){
 for(const [file,meta]of Object.entries(value.sources))assert.equal(hash(readFileSync(resolve(process.env.PAKAR_PU_DONOR_PATH,file))),meta.sha256);
 const src=readFileSync(resolve(process.env.PAKAR_PU_DONOR_PATH,'pu850-esp.ino'),'utf8');
 const registrations=[...src.matchAll(/server->on\("(\/[^"\n]*)"/g)];
 assert.equal(registrations.length,value.routes.filter(r=>r.source==='pu850-esp.ino').length,'No route registration may be silently skipped');
 for(const match of registrations){const line=src.slice(0,match.index).split('\n').length;assert(identities.has('pu850-esp.ino:'+line));}
}
console.log('50 donor route registrations classified; exact source hashes, target hashes/line anchors, concrete exclusions and zero unclassified routes in reviewed slice checked.');
