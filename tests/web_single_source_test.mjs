// Guard canonical convergence without replacing the richer existing runtime.
// The baseline records original controls/functions, not a parity certification.
import assert from 'node:assert/strict';
import {readFile,readdir} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {gunzipSync} from 'node:zlib';
import {readWebSource,webHeader,webHeaderUrl} from '../tools/web-source.mjs';
const root=new URL('../',import.meta.url),baseline=JSON.parse(await readFile(new URL('fixtures/web_contract_baseline.json',import.meta.url),'utf8'));
const html=await readWebSource();
for(const id of baseline.ids)assert(new RegExp('\\bid=["\']'+id+'["\']').test(html),'Existing control removed: '+id);
for(const name of baseline.functions)assert(new RegExp('\\bfunction\\s+'+name.replaceAll('$','\\$')+'\\s*\\(').test(html),'Existing function removed: '+name);
assert.equal((html.match(/<!doctype html/gi)||[]).length,1,'Exactly one application document');
assert(!html.includes("installShell();$('refresh')"),'Retired NG bootstrap must not enter the shared runtime');
assert(!html.includes("createPeerHttp(),http=(path,options)"),'No second peer owner from the retired NG application');
assert.equal(await readFile(webHeaderUrl,'utf8'),webHeader(html),'Firmware header is generated from canonical HTML');
async function entries(directory,prefix=''){
  const result=[];for(const entry of await readdir(directory,{withFileTypes:true})){
    const path=prefix+entry.name;if(entry.isDirectory())result.push(...await entries(new URL(entry.name+'/',directory),path+'/'));else result.push(path);
  }return result;
}
const htmlEntries=(await entries(new URL('web/',root))).filter(name=>name.endsWith('.html'));
assert.deepEqual(htmlEntries,['index.html'],'Only one Web application entry exists; profiles are build output, not independent sources');
const revision=createHash('sha256').update(html).digest('hex');
for(const profile of ['full','onboard']){
  const output=new URL('build/web/'+profile+'/',root),receipt=JSON.parse(await readFile(new URL('build-manifest.json',output),'utf8'));
  assert.equal(receipt.source,'web/index.html');assert.equal(receipt.profile,profile);assert.equal(receipt.revision,revision);
  assert.deepEqual(receipt.disabled_features,[],'Default profiles preserve all capabilities');
  assert.equal(await readFile(new URL('index.html',output),'utf8'),html,profile+' must ship the same application');
  assert.equal(gunzipSync(await readFile(new URL('index.html.gz',output))).toString(),html,profile+' gzip must preserve source');
  for(const file of receipt.files){const bytes=await readFile(new URL(file.name,output));assert.equal(bytes.length,file.bytes);assert.equal(createHash('sha256').update(bytes).digest('hex'),file.sha256);}
}
console.log('Single Web source preserves '+baseline.ids.length+' original control IDs and '+baseline.functions.length+' function declarations; full/onboard packages and generated firmware header match. Not hardware acceptance.');
