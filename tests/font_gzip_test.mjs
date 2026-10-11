import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {gzipSync,gunzipSync} from 'node:zlib';
import {normalizeGzipPlatform,portableGzip} from '../tools/portable-gzip.mjs';
const read=p=>readFileSync(new URL('../'+p,import.meta.url));
const font=read('assets/fonts/Vazirmatn-Regular.ttf');
const generated=read('firmware/esp8266/src/FontAssets.h').toString();
const embedded=Buffer.from(generated.match(/kVazirmatnGzip\[\] PROGMEM = \{([\s\S]*?)\};/)[1].match(/\d+/g).map(Number));
const canonical=portableGzip(font),raw=gzipSync(font,{level:9});
assert.equal(canonical[9],255);
assert.deepEqual(canonical,embedded,'Committed asset still uses strict equality');
assert.deepEqual(gunzipSync(embedded,{maxOutputLength:font.length}),font,'Exact licensed font bytes');
for(const os of [3,10,11,255]){
  const fixture=Buffer.from(raw);fixture[9]=os;
  const normalized=normalizeGzipPlatform(fixture);
  assert.equal(fixture[9],os,'Normalization does not mutate caller data');
  assert.deepEqual(normalized,canonical,'Unix/Windows metadata must not stale source');
  assert.deepEqual(normalized.subarray(10),raw.subarray(10),'Payload and trailer unchanged');
  assert.deepEqual(gunzipSync(normalized),font);
}
for(const flag of [1,2,4,8,16,32]){
  const fixture=Buffer.from(raw);fixture[3]=flag;
  assert.throws(()=>normalizeGzipPlatform(fixture),/optional fields/);
}
const dated=Buffer.from(raw);dated[4]=1;
assert.throws(()=>normalizeGzipPlatform(dated),/timestamp/);
assert.throws(()=>normalizeGzipPlatform(Buffer.from('not gzip')),/gzip\/deflate/);
assert.match(read('tools/generate-font-assets.mjs').toString(),/portableGzip\(font\)/);
console.log('Portable gzip OS metadata, unchanged deflate/CRC, exact font round-trip and unsafe-header rejection pass; no firmware/SDK build.');
