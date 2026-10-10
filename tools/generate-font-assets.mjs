// Share actual Pealayer Vazirmatn bytes, not a CDN or an OS-dependent font.
// Like generate-branding: deterministic output and no identical-file writes.
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {gzipSync} from 'node:zlib';
const root=new URL('../',import.meta.url);
const font=await readFile(new URL('assets/fonts/Vazirmatn-Regular.ttf',root));
assert.equal(font.readUInt32BE(0),0x00010000,'Real TrueType font');
const compressed=gzipSync(font,{level:9});
const lines=[];
for(let i=0;i<compressed.length;i+=24)lines.push('  '+[...compressed.subarray(i,i+24)].join(',')+',');
const license=await readFile(new URL('desktop/licenses/Vazirmatn-OFL.txt',root),'utf8');
const header='// Generated from assets/fonts/Vazirmatn-Regular.ttf; OFL in desktop/licenses.\n#pragma once\n#include <Arduino.h>\nstatic const uint8_t kVazirmatnGzip[] PROGMEM = {\n'+lines.join('\n')+'\n};\nstatic const char kVazirmatnLicense[] PROGMEM = '+JSON.stringify(license.replaceAll('\r\n','\n'))+';\n';
async function sync(path,bytes){const url=new URL(path,root);let old;try{old=await readFile(url);}catch(e){if(e.code!=='ENOENT')throw e;}
if(old?.equals(Buffer.from(bytes)))return;
assert(!process.argv.includes('--check'),'Font assets stale: '+path);
await mkdir(new URL('.',url),{recursive:true});await writeFile(url,bytes);}
await sync('firmware/esp8266/src/FontAssets.h',header);
await sync('mobile/assets/fonts/Vazirmatn-Regular.ttf',font);
await sync('mobile/assets/fonts/Vazirmatn-OFL.txt',await readFile(new URL('desktop/licenses/Vazirmatn-OFL.txt',root)));
// Import the actual native translation table into the standalone Web surface.
// Only keys occurring in this application are retained: no media/timeline UI.
const native=await readFile(new URL('desktop/src/ui/i18n.rs',root),'utf8');
let web=await readFile(new URL('firmware/esp8266/src/WebUi.h',root),'utf8');
const withoutGenerated=web.replace(/\/\/ BEGIN SHARED LOCALIZATION[\s\S]*?\/\/ END SHARED LOCALIZATION/,'').replaceAll('&amp;','&').replaceAll('&lt;','<').replaceAll('&gt;','>');
let js=await readFile(new URL('assets/web/localization.js',root),'utf8');
const dictionary={};
for(const match of native.matchAll(/((?:"[^"\n]+"\s*\|\s*)*"[^"\n]+")\s*=>\s*(?:\{\s*)?"([^"\n]+)"/g)){
  for(const key of match[1].matchAll(/"([^"\n]+)"/g))if(withoutGenerated.includes(key[1])||['Language','Direction','System language','Automatic','Left to right','Right to left','English','Persian'].includes(key[1]))dictionary[key[1]]=match[2];
}
const dictionaryBegin='// BEGIN SHARED PERSIAN DICTIONARY',dictionaryEnd='// END SHARED PERSIAN DICTIONARY';
js=js.slice(0,js.indexOf(dictionaryBegin)+dictionaryBegin.length)+'\nconst PAKAR_FA='+JSON.stringify(dictionary)+';\n'+js.slice(js.indexOf(dictionaryEnd));
await sync('assets/web/localization.js',js);
const begin='// BEGIN SHARED LOCALIZATION',end='// END SHARED LOCALIZATION';
assert.equal(web.split(begin).length,2);assert.equal(web.split(end).length,2);
web=web.replace(/\/\/ BEGIN SHARED LOCALIZATION[\s\S]*?\/\/ END SHARED LOCALIZATION\r?\n/,'');
web=web.replace('<script>','<script>\n'+begin+'\n'+js+'\n'+end);
await sync('firmware/esp8266/src/WebUi.h',web);
console.log('Shared Vazirmatn assets verified:',font.length,'bytes; gzip',compressed.length);
