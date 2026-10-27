// Compile actual production functions with bounded clock/socket doubles. Never
// contact a board or use a rewritten lease algorithm in place of the source.
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {resolve,dirname} from 'node:path';
import {createHash} from 'node:crypto';
const root=fileURLToPath(new URL('../',import.meta.url));
const path='firmware/esp/src/ShellService.h';
const current=readFileSync(resolve(root,path),'utf8');
let source=current,baseline=false;
const args=process.argv.slice(2);
if(args.length){
  if(args.length!==2||args[0]!=='--baseline-ref'||!/^[a-zA-Z0-9._/-]+$/.test(args[1]))throw Error('Use --baseline-ref REVIEWED_GIT_REF');
  const read=spawnSync('git',['show',args[1]+':'+path],{cwd:root,encoding:'utf8',timeout:10000});
  if(read.status!==0)throw Error('Cannot read requested baseline');source=read.stdout;baseline=true;
}
const part=(start,end)=>{const at=source.indexOf(start),until=source.indexOf(end,at+start.length);if(at<0||until<=at)throw Error('Actual shell fixture extraction markers changed');return source.slice(at,until);};
let fixture=readFileSync(new URL('./shell_service_test.cpp',import.meta.url),'utf8');
fixture=fixture.replace('/*__PRODUCTION_SESSION__*/',part('struct DeviceShellSession : public Print {','std::unique_ptr<DeviceShellSession> telnetSessions'))
  .replace('/*__PRODUCTION_INITIALIZER__*/',part('void initializeDeviceShell(','void inputDeviceShell('))
  .replace('/*__PRODUCTION_SERVICE__*/',part('void serviceDeviceShells(){','bool updateShellConfiguration('));
const output=resolve(root,'build/native/shell_service_test.exe'),generated=resolve(root,'build/native/shell_service_fixture.cpp');
mkdirSync(dirname(output),{recursive:true});
let prior;try{prior=readFileSync(generated,'utf8')}catch(error){if(error.code!=='ENOENT')throw error;}
if(prior!==fixture)writeFileSync(generated,fixture);
for(const platform of ['ESP32','ESP8266']){
  const compile=spawnSync(process.env.CXX||'g++',['-std=c++17','-O2','-g0','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
    '-DARDUINO_ARCH_'+platform,'-I',resolve(root,'firmware/esp/src'),generated,'-o',output],{cwd:root,stdio:'inherit',timeout:60000});
  if(compile.error||compile.status!==0)throw Error('Actual shell service fixture compilation failed');
  const result=spawnSync(output,[],{cwd:root,encoding:'utf8',timeout:10000});
  if(result.error)throw result.error;
  if(baseline){
    if(result.status===0||!result.stderr.includes('telnetSessions[0]&&first.connected()'))throw Error('Baseline did not reproduce the expected first-session rejection');
    console.log(platform+' baseline reproduced first-session rejection; not runtime acceptance');
  }
  else {process.stdout.write(result.stdout);if(result.status!==0){process.stderr.write(result.stderr);throw Error('Actual shell service fixture failed');}}
}
console.log('Production shell source SHA256 '+createHash('sha256').update(source).digest('hex'));
