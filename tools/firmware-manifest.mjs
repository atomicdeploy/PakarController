// Actual PU850 C# FirmwareManifest LoadAsync/SelectFor/PortableFileName and
// bounded ComputeFirmwareHashesAsync, adapted to the existing Node owner.
// Donor 431104072952f694a35b3a83c19f13fbb098ca5b; see third-party notices.
import {createHash} from 'node:crypto';
import {gunzipSync} from 'node:zlib';
export const maximumFirmwareBytes=16*1024*1024;
const md5=bytes=>createHash('md5').update(bytes).digest('hex');
export function portableFileName(value) {
  if(typeof value!=='string'||/[\x00-\x1f\x7f]/.test(value))throw Error('Invalid manifest file name');
  const name=value.trim().replace(/^"|"$/g,'').replaceAll('\\','/').split('/').at(-1);
  if(!name||name==='.'||name==='..')throw Error('Manifest file name is empty or relative');
  return name;
}
export function parseFirmwareManifest(content,fileName) {
  if(typeof content!=='string'||Buffer.byteLength(content)>65536)throw Error('Manifest exceeds the 64 KiB bound');
  const lines=content.replaceAll('\r\n','\n').replaceAll('\r','\n').replace(/\n+$/,'').split('\n');
  if(lines.length!==2||lines.some(line=>!line.trim()))throw Error('Manifest requires exactly two non-empty MD5 records');
  const selected=portableFileName(fileName).toLowerCase(),result={};
  for(const line of lines){
    const match=/^([a-f0-9]{32})\s+\*?(.+?)\s*$/i.exec(line.trim());
    if(!match)throw Error('Invalid MD5 manifest record');
    const compressed=/\s+\(compressed\)\s*$/i.test(match[2]);
    const name=portableFileName(match[2].replace(/\s+\(compressed\)\s*$/i,'')).toLowerCase();
    if(name!==selected)throw Error('Manifest names a different firmware file');
    const key=compressed?'compressed_md5':'raw_md5';
    if(Object.hasOwn(result,key))throw Error('Duplicate raw/compressed manifest entry');
    result[key]=match[1].toLowerCase();
  }
  if(!result.raw_md5||!result.compressed_md5)throw Error('Manifest requires raw and compressed hashes');
  return result;
}
export function firmwareHashes(compressed,{maximum=maximumFirmwareBytes}={}) {
  if(!Number.isSafeInteger(maximum)||maximum<1||maximum>maximumFirmwareBytes)throw Error('Invalid firmware safety bound');
  if(!Buffer.isBuffer(compressed)||!compressed.length||compressed.length>maximum)throw Error('Compressed firmware exceeds the safety bound');
  let raw;try{raw=gunzipSync(compressed,{maxOutputLength:maximum});}catch{throw Error('Invalid or oversized gzip firmware');}
  if(!raw.length)throw Error('Raw firmware is empty');
  return {compressed_md5:md5(compressed),raw_md5:md5(raw),compressed_size:compressed.length,raw_size:raw.length};
}
export function validateFirmwareManifest(compressed,content,fileName,options) {
  const expected=parseFirmwareManifest(content,fileName),actual=firmwareHashes(compressed,options);
  if(actual.compressed_md5!==expected.compressed_md5)throw Error('Compressed firmware MD5 mismatch');
  if(actual.raw_md5!==expected.raw_md5)throw Error('Raw firmware MD5 mismatch');
  return actual;
}
export function createFirmwareManifest(raw,compressed,fileName) {
  const name=portableFileName(fileName);
  const content=`${md5(raw)} *${name}\n${md5(compressed)} *${name} (compressed)\n`;
  // Validate the real compressed bytes, not just the two advertised hashes.
  return {content,identity:validateFirmwareManifest(compressed,content,name)};
}
