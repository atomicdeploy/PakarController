// Extract actual command/session/SSH-loop functions; do not replace dispatch.
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {resolve} from 'node:path';
import {homedir} from 'node:os';
const shell=readFileSync('firmware/esp/src/ShellService.h','utf8');
const ssh=readFileSync('firmware/esp/src/SshShellBinding.h','utf8');
const tui=readFileSync('firmware/esp/src/ShellTuiBinding.h','utf8');
function part(text,start,end){const a=text.indexOf(start),b=text.indexOf(end,a+start.length);if(a<0||b<=a)throw Error('SSH exec production markers changed');return text.slice(a,b);}
let fixture=readFileSync('tests/ssh_exec_binding_test.cpp','utf8')
 .replace('/*__SESSION__*/',part(shell,'struct DeviceShellSession : public Print {','std::unique_ptr<DeviceShellSession> telnetSessions'))
 .replace('/*__COMMANDS__*/',part(shell,'void shellPrompt(','void inputDeviceShell('))
 .replace('/*__TUI_INPUT__*/',tui.slice(tui.indexOf('void shellTuiInput(')))
 .replace('/*__SSH_LOOP__*/',part(ssh,'void serviceSshShell(){','void registerSshApi(){'));
mkdirSync('build/native',{recursive:true});
const generated=resolve('build/native/ssh_exec_binding_fixture.cpp'),output=resolve('build/native/ssh_exec_binding_test.exe');
let prior;try{prior=readFileSync(generated,'utf8')}catch(e){if(e.code!=='ENOENT')throw e;}
if(prior!==fixture)writeFileSync(generated,fixture);
const args=process.argv.slice(2);
if(args.length&&(args.length!==2||args[0]!=='--arduino-json'))throw Error('Use --arduino-json INCLUDE_PATH');
const json=resolve(args.length?args[1]:resolve(homedir(),'Documents/Arduino/libraries/ArduinoJson/src'));
for(const role of [0,1]){
 for(const [command,args] of [[process.env.CXX||'g++',['-std=c++17','-O2','-g0','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',`-DPAKAR_FIRST_WINS_ENABLED=${role}`,'-I',resolve('firmware/esp/src'),'-I',json,generated,'-o',output]],[output,[]]]){
  const result=spawnSync(command,args,{stdio:'inherit',timeout:60000});if(result.error||result.status!==0)throw Error('Actual SSH exec binding fixture failed');
 }
}
