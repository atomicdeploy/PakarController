import assert from 'node:assert/strict';
import {readFileSync,existsSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
const root=fileURLToPath(new URL('..',import.meta.url));
const read=p=>readFileSync(resolve(root,p),'utf8').replaceAll('\r\n','\n');
const hash=text=>createHash('sha256').update(text).digest('hex');
const mapping=JSON.parse(read('docs/pccontroller/SOURCE-MAPPING.json'));
assert.equal(mapping.revision,'20c6115bb462fe5111b66cd7c21f7328a1f78892');
assert.equal(mapping.files.length,22);
const sources=new Set();
for(const entry of mapping.files){
  assert(!sources.has(entry.source));sources.add(entry.source);
  assert.equal(entry.source_read,'complete');
  assert.match(entry.source_sha256,/^[0-9a-f]{64}$/);
  assert(!entry.target.includes('..') && !entry.target.startsWith('/'));
  const text=read(entry.target);
  assert.equal(hash(text),entry.target_sha256_utf8_lf,entry.target+' changed; rereview mapping');
  assert(text.split('\n')[entry.line-1].includes(entry.anchor),entry.target+':'+entry.line+' stale anchor');
  assert(entry.retained.length>20 && entry.omitted_code_and_reason.length>4);
  if(process.env.PAKAR_DONOR_PATH){
    const source=resolve(process.env.PAKAR_DONOR_PATH,entry.source);
    assert(existsSync(source));
    assert.equal(createHash('sha256').update(readFileSync(source)).digest('hex'),entry.source_sha256);
  }
}
const registry=JSON.parse(read('docs/pccontroller/ProtocolContract.json'));
const header=read('libraries/BoardCore/src/pccontroller/ProtocolContract.h');
assert.equal(registry.envelope.maximumPayload,48);
for(const op of registry.opcodes){
  assert.match(header,new RegExp('\\b'+op.cpp+'\\s*=\\s*0x'+op.value.toString(16).padStart(2,'0'),'i'),op.cpp);
}
console.log('22 exact donor mappings bound to current target hashes/lines; actual donor registry/header opcode values checked. No whole-repository or physical certification.');
