// Serialize the EXISTING canonical PNGs; no rasterizer, image changes or heap
// base64 decoding on the ESP HTTP path. Same low-write/check pattern as branding.
import {readFile,writeFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import assert from 'node:assert/strict';
const root=new URL('../',import.meta.url),check=process.argv.includes('--check');
const variants=[];
for(const role of ['keys','table','app-icon']){
 const base64=(await readFile(new URL(`assets/branding/${role}.png.base64`,root),'utf8')).trim();
 const png=Buffer.from(base64,'base64');assert.equal(png.subarray(0,8).toString('hex'),'89504e470d0a1a0a');
 assert.equal(png[28],0,'UPnP PNG must not be interlaced');
 const width=png.readUInt32BE(16),height=png.readUInt32BE(20),channels={0:1,2:3,3:1,4:2,6:4}[png[25]];
 assert(width>0&&height>0&&channels&&png.length<=16384,'Invalid bounded icon');
 const depth=png[24]*channels,etag='"'+createHash('sha256').update(png).digest('hex')+'"';
 const lines=[];for(let i=0;i<png.length;i+=24)lines.push('  '+[...png.subarray(i,i+24)].map(v=>'0x'+v.toString(16).padStart(2,'0')).join(','));
 variants.push(`static const uint8_t kBoardIconPng[] PROGMEM = {\n${lines.join(',\n')}\n};\nstatic constexpr unsigned kBoardIconWidth=${width},kBoardIconHeight=${height},kBoardIconDepth=${depth};\nstatic const char kBoardIconEtag[] = ${JSON.stringify(etag)};`);
}
const text='// Generated from canonical board/application PNG masters; do not edit.\n#pragma once\n#include <Arduino.h>\n#include <DeviceContext.h>\n#if PAKAR_FIRST_WINS_ENABLED\n'+variants[0]+'\n#elif PAKAR_TABLE_ENABLED\n'+variants[1]+'\n#else\n'+variants[2]+'\n#endif\n';
const path=new URL('firmware/esp/src/BoardIconAsset.h',root);let old;try{old=await readFile(path,'utf8')}catch(e){if(e.code!=='ENOENT')throw e;}
if(old!==text){assert(!check,'Board HTTP icon asset is stale; regenerate branding');await writeFile(path,text);}
console.log('Role-specific HTTP PNG/size/depth/ETag assets current.');
