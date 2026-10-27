// Tiny generated fixtures exercise the actual packager; no build, publish or flash.
import assert from 'node:assert/strict';
import {mkdtempSync,mkdirSync,writeFileSync,readFileSync,readdirSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join,resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {sha256} from '../tools/prepare-release.mjs';
const fixture=mkdtempSync(join(tmpdir(),'PakarController-release-test-'));
const incoming=join(fixture,'incoming'),output=join(fixture,'output');
const put=(relative,data)=>{const path=join(incoming,relative);mkdirSync(resolve(path,'..'),{recursive:true});writeFileSync(path,data);};
const esp=Buffer.alloc(64);esp[0]=0xe9;esp[1]=1;
const s3=Buffer.from(esp);s3.writeUInt16LE(9,12);s3[23]=1;
const avr=Buffer.alloc(32768,0xff);avr[0]=0x0c;avr[1]=0x94;avr[32767]=0;
for(const mode of ['','-busy-wire']){put(`firmware/esp8266${mode}/esp8266.ino.bin`,esp);put(`firmware/avr${mode}/avr.ino.with_bootloader.bin`,avr);}
put('firmware/esp32s3-buffered/esp32s3.ino.bin',s3);
put('firmware/avr-buffered/avr.ino.with_bootloader.bin',avr);
const native=Buffer.alloc(64);native.write('MZ');put('windows/pakarcontroller.exe',native);
const apk=Buffer.alloc(64);apk.writeUInt32BE(0x504b0304);put('android/app-release.apk',apk);
const version=/kVersion\[\]\s*=\s*"(\d+\.\d+\.\d+)"/.exec(readFileSync('libraries/BoardCore/src/BuildInfo.h','utf8'))[1];
const args=['tools/prepare-release.mjs',incoming,output,`v${version}-preview`,'a'.repeat(40),'atomicdeploy/PakarController','https://github.com/atomicdeploy/PakarController/actions/runs/1'];
const run=spawnSync(process.execPath,args,{encoding:'utf8'});
assert.equal(run.status,0,run.stdout+'\n'+run.stderr);
const files=readdirSync(output).filter(name=>!name.startsWith('.'));
assert.equal(files.length,9);
assert(files.includes('PakarController-Windows.zip'));
assert(!files.some(name=>/polling|busy-wire/.test(name)),'Legacy artifacts must not leak into the default release');
for(const line of readFileSync(join(output,'SHA256SUMS.txt'),'utf8').trim().split('\n')){
  const [hash,name]=line.split('  ');assert.equal(sha256(readFileSync(join(output,name))),hash,name);
}
const info=JSON.parse(readFileSync(join(output,'release-manifest.json'),'utf8'));
assert.equal(info.commit,'a'.repeat(40));assert.equal(info.physicalAcceptance,false);assert(info.draft&&info.prerelease);
assert.deepEqual(info.releaseTargets,['esp32s3','atmega328p']);
for(const mode of ['buffered']){
  const value=JSON.parse(readFileSync(join(output,`manifest-${mode}.json`),'utf8'));
  assert.equal(value.esp.model,'esp32s3');assert.equal(value.profile,'pakar-esp32s3-atmega328p');
  for(const component of ['esp','avr'])assert.equal(value[component].sha256,sha256(readFileSync(join(output,`${component}-${mode}.bin`))));
}
const before=sha256(readFileSync(join(output,'SHA256SUMS.txt')));
const repeat=spawnSync(process.execPath,args,{encoding:'utf8'});
assert.notEqual(repeat.status,0);assert.match(repeat.stderr,/Refuse to overwrite/);
assert.equal(sha256(readFileSync(join(output,'SHA256SUMS.txt'))),before);
const legacyOutput=join(fixture,'legacy-output');
const legacyArgs=[...args];legacyArgs[2]=legacyOutput;
const legacyRun=spawnSync(process.execPath,[...legacyArgs,'--legacy'],{encoding:'utf8'});
assert.equal(legacyRun.status,0,legacyRun.stdout+'\n'+legacyRun.stderr);
const legacyInfo=JSON.parse(readFileSync(join(legacyOutput,'release-manifest.json'),'utf8'));
assert.match(legacyInfo.legacyEsp8266,/unfinished/);
for(const mode of ['polling','busy-wire']){
  const value=JSON.parse(readFileSync(join(legacyOutput,`manifest-${mode}.json`),'utf8'));
  assert.equal(value.esp.model,'nodemcuv2');assert.equal(value.esp.sha256,sha256(esp));
}
console.log('Actual release packager: primary S3/AVR pair, two tested ZIPs, nine default assets, separately opted-in legacy pairs, complete checksum/source manifest and overwrite rejection passed. Dummy fixtures, not installable devices. Small fixture retained: '+fixture);
