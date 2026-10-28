// Real S3 schema method + shared renderer/actual generated PNG for each role.
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {resolve} from 'node:path';
import {createHash} from 'node:crypto';
import assert from 'node:assert/strict';
const read=p=>readFileSync(p,'utf8');
const source=read('firmware/esp32s3/src/SsdpDevice.cpp');
const a=source.indexOf('void SsdpDevice::refreshConfigId()'),b=source.indexOf('void SsdpDevice::handleClient()',a);
assert(a>=0&&b>a,'Actual schema fixture markers changed');
const fixture=read('tests/upnp_description_test.cpp').replace('/*__PRODUCTION_SCHEMA__*/',source.slice(a,b));
const dir=resolve('build/native');mkdirSync(dir,{recursive:true});
const cpp=resolve(dir,'upnp_description_fixture.cpp'),exe=resolve(dir,'upnp_description_test.exe');
let old;try{old=read(cpp)}catch(e){if(e.code!=='ENOENT')throw e;}if(old!==fixture)writeFileSync(cpp,fixture);
for(const [role,flags] of [['table',['-DPAKAR_GAME_MODE=1']],['keys',['-DPAKAR_GAME_MODE=2']],['app-icon',['-DPAKAR_GAME_MODE=1','-DPAKAR_TABLE_ENABLED=0']]]){
 const compile=spawnSync(process.env.CXX||'g++',['-std=c++17','-O2','-g0','-Wall','-Wextra','-Werror',...flags,'-I',resolve('tests/upnp_stubs'),'-I',resolve('libraries/BoardCore/src'),'-I',resolve('firmware/esp/src'),cpp,'-o',exe],{stdio:'inherit',timeout:60000});
 assert.equal(compile.status,0,'UPnP fixture compile');
 const run=spawnSync(exe,[],{encoding:'utf8',timeout:10000});assert.equal(run.status,0,run.stderr);
 const [xml,hex]=run.stdout.split('ICON '),png=Buffer.from(hex.trim(),'hex');
 const config=xml.match(/<root xmlns="urn:schemas-upnp-org:device-1-0" configId="(\d+)">/);
 assert(config&&+config[1]>0&&+config[1]<=0xffffff,'Description must identify the advertised config');
 assert(xml.includes('<specVersion><major>1</major><minor>1</minor></specVersion>'));
 const tags=['deviceType','friendlyName','manufacturer','manufacturerURL','modelName','modelNumber','modelURL','serialNumber','UDN','iconList','presentationURL'];
 let previous=-1;for(const tag of tags){const at=xml.indexOf(`<${tag}>`);assert(at>previous,`UPnP description order: ${tag}`);previous=at;}
 assert(png.equals(Buffer.from(read(`assets/branding/${role}.png.base64`).trim(),'base64')),'HTTP icon bytes must match peer artwork');
 assert(xml.includes(`<width>${png.readUInt32BE(16)}</width><height>${png.readUInt32BE(20)}</height>`));
 const channels={0:1,2:3,3:1,4:2,6:4}[png[25]];assert(xml.includes(`<depth>${png[24]*channels}</depth>`));
 const hash=createHash('sha256').update(png).digest('hex');assert(read('firmware/esp/src/BoardIconAsset.h').includes(hash));
 for(const origin of ['http://192.168.4.1/description.xml','http://192.168.1.201:8080/description.xml','http://10.0.0.20/description.xml']){
  const icon=xml.match(/<url>([^<]+)<\/url>/)[1];assert.equal(new URL(icon,origin).origin,new URL(origin).origin);
 }
 writeFileSync(resolve(dir,`upnp-description-${role}.xml`),xml);
}
const app=read('firmware/esp/application.ino'),access=read('firmware/esp/src/WebAccess.h');
assert(app.includes('AsyncURIMatcher::exact("/board-icon.png"), HTTP_GET|HTTP_HEAD'));
assert(app.includes('serveStaticAsset(request,"image/png",kBoardIconPng,sizeof(kBoardIconPng),kBoardIconEtag)'));
assert(access.includes('path == F("/board-icon.png")'));
assert(app.includes('PakarDiscovery::description(output,'),'Retained ESP8266 also shares XML renderer');
console.log('Actual UPnP schema: role PNG bytes/dimensions/depth, escaping, cache ID, no phantom services and AP/STA/Ethernet-relative URLs passed. Not LAN/router acceptance.');
