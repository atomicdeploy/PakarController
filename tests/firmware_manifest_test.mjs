// Actual C# SelfTests.TestManifestAsync cases, ported to the shipped module.
import assert from 'node:assert/strict';
import {gzipSync} from 'node:zlib';
import {createHash} from 'node:crypto';
import {mkdirSync,writeFileSync,readFileSync,existsSync,unlinkSync} from 'node:fs';
import {resolve} from 'node:path';
import {collectArtifacts} from '../tools/firmware-report.mjs';
import {portableFileName,parseFirmwareManifest,firmwareHashes,validateFirmwareManifest,createFirmwareManifest} from '../tools/firmware-manifest.mjs';
const raw=Buffer.from('raw firmware fixture'),compressed=gzipSync(raw),hash=b=>createHash('md5').update(b).digest('hex');
const content=hash(raw).toUpperCase()+'  *folder\\fixture.bin\r\n'+hash(compressed)+' *fixture.bin (compressed)\r\n';
assert.deepEqual(parseFirmwareManifest(content,'fixture.bin'),{raw_md5:hash(raw),compressed_md5:hash(compressed)});
assert.deepEqual(validateFirmwareManifest(compressed,content,'fixture.bin'),{compressed_md5:hash(compressed),raw_md5:hash(raw),compressed_size:compressed.length,raw_size:raw.length});
assert.equal(portableFileName('"folder\\fixture.bin"'),'fixture.bin');
for(const name of ['', '.', '..', 'folder/', 'bad\nname'])assert.throws(()=>portableFileName(name));
for(const text of [content+'\n'+hash(raw)+' *fixture.bin',content.replace('(compressed)',''),content.replace('folder\\fixture.bin','other.bin'),content.replace(hash(compressed),'x'+hash(compressed)),content.replace('\r\n','\r\n\r\n'),'x'.repeat(65537)])assert.throws(()=>parseFirmwareManifest(text,'fixture.bin'));
assert.throws(()=>validateFirmwareManifest(compressed,content.replace(hash(raw).toUpperCase(),'0'.repeat(32)),'fixture.bin'),/Raw firmware MD5/);
assert.throws(()=>validateFirmwareManifest(compressed,content.replace(hash(compressed),'0'.repeat(32)),'fixture.bin'),/Compressed firmware MD5/);
assert.throws(()=>firmwareHashes(Buffer.from('not gzip')),/gzip/);
assert.throws(()=>firmwareHashes(gzipSync(Buffer.alloc(0))),/empty/);
assert.throws(()=>firmwareHashes(gzipSync(Buffer.alloc(1025)),{maximum:1024}),/oversized/);
assert.throws(()=>firmwareHashes(Buffer.alloc(1025),{maximum:1024}),/Compressed firmware/);
const corrupt=Buffer.from(compressed);corrupt[corrupt.length-8]^=1;assert.throws(()=>firmwareHashes(corrupt),/gzip/);
const generated=createFirmwareManifest(raw,compressed,'esp8266.ino.bin.gz');
assert.equal(generated.identity.raw_md5,hash(raw));assert.equal(generated.content.split('\n').length,3);
assert.equal(parseFirmwareManifest(generated.content,'esp8266.ino.bin.gz').compressed_md5,hash(compressed));
assert.throws(()=>createFirmwareManifest(Buffer.from('different raw'),compressed,'fixture.gz'),/Raw firmware/);
const fixture=resolve('build/native/firmware-manifest-fixture');
const esp=resolve(fixture,'esp8266/esp8266.ino.bin'),s3=resolve(fixture,'esp32s3-buffered/esp32s3.ino.bin');
const owned=[esp,esp+'.gz',esp+'.gz.md5',s3];
for(const path of [...owned,s3+'.gz',s3+'.gz.md5'])assert(!existsSync(path),'Unexpected retained fixture: '+path);
mkdirSync(resolve(fixture,'esp8266'),{recursive:true});mkdirSync(resolve(fixture,'esp32s3-buffered'),{recursive:true});
try {
  const image=Buffer.alloc(96);image[0]=0xe9;writeFileSync(esp,image,{flag:'wx'});writeFileSync(s3,image,{flag:'wx'});
  assert.equal(collectArtifacts(fixture,{compress:true}).filter(row=>row.format==='gzip').length,0,
    'Primary release collection must ignore the unfinished legacy image');
  const artifacts=collectArtifacts(fixture,{compress:true,includeLegacy:true});
  assert.equal(artifacts.filter(row=>row.format==='gzip').length,1);
  const packaged=artifacts.find(row=>row.format==='gzip');assert.equal(packaged.path,'esp8266/esp8266.ino.bin.gz');
  assert.equal(validateFirmwareManifest(readFileSync(esp+'.gz'),readFileSync(esp+'.gz.md5','utf8'),'esp8266.ino.bin.gz').raw_md5,hash(image));
  assert.equal(packaged.raw_md5,hash(image));assert.equal(packaged.manifest,'esp8266/esp8266.ino.bin.gz.md5');
  assert.deepEqual(readFileSync(esp),image);assert.deepEqual(readFileSync(s3),image);assert(!existsSync(s3+'.gz'));
} finally {for(const path of owned)if(existsSync(path))unlinkSync(path);}
console.log('Actual C# manifest source: exact two records, portable names, raw/compressed MD5, corrupt/empty/bounded gzip and generated ESP8266 sidecar passed. Pure source fixture; S3 remains raw.');
