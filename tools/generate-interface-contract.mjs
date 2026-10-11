// One page/field catalog for native, embedded Web and mobile consumers.
// Mechanical generated outputs skip unchanged writes; --check never writes.
import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
const root=new URL('../',import.meta.url),read=p=>readFile(new URL(p,root),'utf8');
const c=JSON.parse(await read('assets/contracts/device-settings.json'));
assert.equal(c.schema,1);assert.equal(c.pages.length,10);assert.equal(new Set(c.pages.map(p=>p.id)).size,10);
for(const p of c.pages){assert.match(p.id,/^[a-z-]+$/);assert.match(p.icon,/^[A-Z_]+$/);assert(Array.isArray(p.features));}
async function output(path,text){let old='';try{old=await read(path)}catch{}if(old.replace(/\r\n/g,'\n')!==text){assert(!process.argv.includes('--check'),'Stale interface contract: '+path);await writeFile(new URL(path,root),text);console.log('Generated '+path)}}
const quoted=JSON.stringify;
await output('desktop/src/ui/settings_catalog.rs','// Generated from assets/contracts/device-settings.json; do not hand-edit.\n'+
 'const PAGES: [(&str, &str, &str); 10] = [\n'+c.pages.map(p=>`    (${quoted(p.label)}, icons::${p.icon}, ${quoted(p.keywords)}),`).join('\n')+'\n];\n'+
 'const PAGE_IDS: [&str; 10] = ['+c.pages.map(p=>quoted(p.id)).join(', ')+'];\n'+
 'const PAGE_FEATURES: [&[&str]; 10] = ['+c.pages.map(p=>'&['+p.features.map(quoted).join(', ')+']').join(', ')+'];\n');
await output('mobile/lib/settings_catalog.dart','// Generated shared device page catalog; mobile native editors remain tracked.\nconst deviceSettingsCatalog = '+JSON.stringify(c,null,2)+';\n');
const module=await read('assets/web/device-settings.js');assert(!module.includes('</script>')&&!module.includes(')HTML"'));
const path='firmware/esp8266/src/WebUi.h';let web=await read(path),begin='// BEGIN DEVICE SETTINGS CONTRACT',end='// END DEVICE SETTINGS CONTRACT';
assert.equal(web.split(begin).length,2);assert.equal(web.split(end).length,2);
web=web.slice(0,web.indexOf(begin)+begin.length)+'\nconst DEVICE_SETTINGS='+JSON.stringify(c)+';\n'+module.trim()+'\n'+web.slice(web.indexOf(end));
await output(path,web.replace(/\r\n/g,'\n'));
console.log('Interface catalog and settings embedding are current.');
