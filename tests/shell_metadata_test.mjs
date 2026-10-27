// Execute the actual simple getter prefix, not a second implementation.
// No board, network or settings writes; stable fixture files use existing cache.
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {resolve,dirname} from 'node:path';
const source=readFileSync('firmware/esp/src/ShellService.h','utf8');
const start=source.indexOf('String shellGet(const ShellProperty &p){');
const end=source.indexOf('  if(key=="hostname")',start);
if(start<0||end<start)throw Error('Actual metadata getter extraction markers changed');
const fixture=`#include <cassert>
#include <string>
#include "ShellProperties.h"
#define F(text) text
using String=std::string;
struct Platform {unsigned reads=0;String getSketchMD5(){++reads;return "0123456789abcdef0123456789abcdef";}} ESP;
${source.slice(start,end)}
return "unknown";
}
int main(){
ShellProperty p{};
assert(shellProperty("firmware.hash",p)&&!p.writable&&!p.secret);
assert(shellGet(p)=="0123456789abcdef0123456789abcdef"&&ESP.reads==1);
assert(shellProperty("build.date",p)&&!p.writable&&!p.secret);
assert(shellGet(p)==String(__DATE__ " " __TIME__)&&ESP.reads==1);
p.secret=true;p.key="firmware.hash";
assert(shellGet(p)=="<write-only>"&&ESP.reads==1);
}
`;
const generated=resolve('build/native/shell_metadata_fixture.cpp'),output=resolve('build/native/shell_metadata_test.exe');
mkdirSync(dirname(generated),{recursive:true});
let prior;try{prior=readFileSync(generated,'utf8')}catch(error){if(error.code!=='ENOENT')throw error;}
if(prior!==fixture)writeFileSync(generated,fixture);
for(const platform of ['ESP8266','ESP32']){
  const args=['-std=c++17','-Wall','-Wextra','-Werror','-DARDUINO_ARCH_'+platform,
    '-I',resolve('firmware/esp/src'),'-I',resolve('tests/shell_properties_stubs'),generated,'-o',output];
  const compile=spawnSync(process.env.CXX||'g++',args,{stdio:'inherit',timeout:60000});
  if(compile.error||compile.status!==0)throw Error('Actual metadata getter fixture compilation failed');
  const run=spawnSync(output,[],{stdio:'inherit',timeout:10000});
  if(run.error||run.status!==0)throw Error('Actual metadata getter fixture failed');
}
console.log('Actual read-only firmware hash/build date getters and secret-first guard pass for both platform profiles. Not SDK/hardware acceptance.');
