import {readFileSync,writeFileSync,mkdirSync,renameSync} from 'node:fs';
import {resolve,dirname} from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';

export function manifest(esp,avr,base,version,mode,platform='esp8266'){
  const url=new URL(base);
  if(url.protocol!=='https:'||url.username||url.password||url.search||url.hash||!url.pathname.endsWith('/'))throw Error('Use a direct HTTPS directory URL without credentials, query or fragment');
  if(!/^\d{1,3}\.\d{1,3}\.\d{1,3}$/.test(version)||version.split('.').some(n=>Number(n)>255))throw Error('Invalid firmware version');
  if(!['esp8266','esp32s3'].includes(platform))throw Error('Invalid ESP platform');
  const s3=platform==='esp32s3';
  if(!(s3?mode==='buffered':['busy-wire','polling'].includes(mode)))throw Error('Invalid link mode for platform');
  if(esp.length<24||esp.length>=(s3?0x330000:1048576)||esp[0]!==0xe9||esp[1]<1||esp[1]>16)throw Error('Invalid ESP raw image');
  if(s3&&(esp.readUInt16LE(12)!==9||esp[23]!==1))throw Error('Use a hashed ESP32-S3 application image, not a merged flash image');
  if(avr!==null){
  const vector=avr.length===32768&&((avr[1]&0xf0)===0xc0||(avr[0]===0x0c&&avr[1]===0x94));
  if(!vector||!avr.subarray(32768-384).some(b=>b!==0xff))throw Error('Use the complete 32768-byte AVR image including Urboot, not HEX or application-only');
  }
  const component=(data,file,model)=>({url:new URL(file,url).href,size:data.length,sha256:createHash('sha256').update(data).digest('hex'),version,model});
  return {product:'PakarController',profile:s3?'pakar-esp32s3-atmega328p':'pakar-nodemcu-atmega328p',link_mode:mode,
    esp:component(esp,'esp.bin',s3?'esp32s3':'nodemcuv2'),
    ...(avr===null?{}:{avr:{...component(avr,'avr.bin','atmega328p'),signature:'1E950F',bootloader_included:true}})};
}
// Publish the three files as one immutable server-side release directory; never
// overwrite a live endpoint during deployment. The trusted manifest hash is
// delivered separately to the owner. This tool neither deploys nor flashes.
if(process.argv[1]&&resolve(process.argv[1])===fileURLToPath(import.meta.url)){
  const [espPath,avrPath,base,version,mode,output,platform='esp8266']=process.argv.slice(2);
  if(!output)throw Error('Usage: node tools/create-remote-manifest.mjs ESP_BIN AVR_FULL_BIN_OR_- HTTPS_DIRECTORY VERSION busy-wire|polling|buffered OUTPUT_DIRECTORY [esp8266|esp32s3]');
  const esp=readFileSync(espPath),avr=avrPath==='-'?null:readFileSync(avrPath),value=manifest(esp,avr,base,version,mode,platform);
  const json=Buffer.from(JSON.stringify(value,null,2)+'\n');
  mkdirSync(output,{recursive:true});
  for(const [name,data] of [['esp.bin',esp],...(avr===null?[]:[['avr.bin',avr]]),['manifest.json',json]]){
    const path=resolve(output,name);writeFileSync(path+'.pending',data,{flag:'wx'});renameSync(path+'.pending',path);
  }
  const sha=createHash('sha256').update(json).digest('hex');
  writeFileSync(resolve(output,'manifest.sha256'),sha+'  manifest.json\n');
  console.log(JSON.stringify({manifest_url:new URL('manifest.json',base).href,manifest_sha256:sha,version,link_mode:mode,deployed:false},null,2));
}
