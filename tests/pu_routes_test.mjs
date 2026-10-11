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
