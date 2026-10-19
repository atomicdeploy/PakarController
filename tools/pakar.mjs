#!/usr/bin/env node
// PU build.cmd/build.sh help, preflight, validation and Tool/build-all.ps1
// dispatch adapted to one shell-independent owner; existing guards still run.
import {existsSync,readdirSync,realpathSync} from 'node:fs';
import {resolve,dirname,sep} from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
import {spawnSync} from 'node:child_process';
import {createLogger,runChild,ToolError} from './script-console.mjs';
const root=resolve(dirname(fileURLToPath(import.meta.url)),'..');
export const commands=Object.freeze({
  all:['build-all.ps1','Plan selected components; --run executes, --release packages'],
  firmware:['build-firmware.ps1','Compile selected boards; never flash'],
  desktop:['build-desktop.ps1','Check/test/build native client in its warm cache'],
  mobile:['build-mobile.ps1','Analyze/test/build Android preview'],
  update:['update-firmware.ps1','Plan by default; --run authorizes one verified update'],
  download:['download-firmware.ps1','Plan or save one verified running-image backup; explicit --run'],
  discovery:['test-device-discovery.ps1','Explicit discovery checks'],
  network:['set-device-network.ps1','Configure an explicitly selected USB peer'],
  name:['set-device-name.ps1','Rename an explicitly selected USB peer'],
  access:['set-device-access.ps1','Physical USB credential recovery'],
  storage:['storage-hygiene.ps1','Inspect storage; no automatic cleanup'],
  versions:['verify-versions.ps1','Check source version consistency'],
});
export function portableArguments(args) {
  const output=[];let literal=false;
  for(const arg of args){
    if(arg==='--'){literal=true;continue;}
    if(!literal&&/^--[a-z][a-z0-9-]*(?:=|$)/i.test(arg)){
      const match=/^--([^=]+)(?:=(.*))?$/.exec(arg);
      output.push('-'+match[1].split('-').map(word=>word[0].toUpperCase()+word.slice(1)).join(''));
      if(match[2]!==undefined)output.push(match[2]);
    }else output.push(arg);
  }return output;
}
export function legacyBuildArguments(args) {
  if(!args.length||args[0].startsWith('-'))return portableArguments(args);
  const [mode,...rest]=args;
  const target=rest[0]&&!rest[0].startsWith('-')?rest.shift():'both';
  if(!['polling','busy-wire','buffered'].includes(mode)||!['both','avr','esp8266','esp32s3'].includes(target))throw new ToolError('Use build [polling|busy-wire|buffered] [both|avr|esp8266|esp32s3], or --target ...');
  return ['-LinkMode',mode,'-Target',target,...portableArguments(rest)];
}
export function scriptPath(name,repository=root) {
  if(!/^[a-z][a-z0-9-]*$/.test(name))throw new ToolError('Use a listed script name, without a path/extension');
  const directory=realpathSync(resolve(repository,'tools')),candidate=resolve(directory,name+'.ps1');
  if(!existsSync(candidate)||!realpathSync(candidate).startsWith(directory+sep))throw new ToolError('Unknown or escaping script: '+name);
  return candidate;
}
export function help(command) {
  if(command&&commands[command])return `PakarController · ${command}\n${commands[command][1]}\n\nGNU --kebab-case and native -PowerShell options reach the same reviewed wrapper.\nExample: pakar firmware --target esp32s3 --plan\nFull help: pwsh -NoProfile -Command "Get-Help '${resolve(root,'tools',commands[command][0])}' -Full"\n`;
  return `PakarController · tools\n\nUsage: pakar <command> [options]\n\n${Object.entries(commands).map(([name,entry])=>`  ${name.padEnd(14)}${entry[1]}`).join('\n')}\n  build         Firmware; also accepts build.sh positional arguments\n  quality       Required repository checks; no deployment\n  test NAME     Run a test-* wrapper with explicit scope\n  dependencies  Check/plan or explicitly update dependency pins\n  artifacts     Firmware sizes/checksums and CI summary\n  doctor        Offline tool/version inventory; no installation/device contact\n  scripts       List all PowerShell utilities\n  run NAME      Run a listed utility with its original safety guards\n\nGlobal flags before command: --json, --quiet, --no-color\nUse --help or COMMAND --help. No default clean, debug build, install or flash.\nExamples:\n  pakar firmware --target esp32s3 --plan\n  pakar desktop --mode Check --plan\n  pakar mobile --mode ApkPreview --build-only --plan\n  pakar update --origin http://ledboard.local\n  pakar dependencies --check\n`;
}
export function doctor({env=process.env,probe=spawnSync}={}) {
  const specs=[['node',['--version']],['pwsh',['-NoProfile','-Command','$PSVersionTable.PSVersion.ToString()']],['git',['--version']],['arduino-cli',['version']],['cargo',['--version']],['adb',['version']]];
  return {platform:process.platform,architecture:process.arch,device_contacted:false,installed:false,
    tools:specs.map(([name,args])=>{const executable=name==='arduino-cli'&&existsSync(resolve(root,'tools/arduino-cli.exe'))?resolve(root,'tools/arduino-cli.exe'):name;
      const result=probe(executable,args,{env,encoding:'utf8',timeout:5000,windowsHide:true,shell:false});
      return {name,available:result.status===0,detail:String(result.stdout||result.error?.message||result.stderr||'Unavailable').trim().split(/\r?\n/)[0].slice(0,200)};})};
}
export async function main(argv=process.argv.slice(2),{env=process.env,runner=runChild,out=process.stdout,err=process.stderr}={}) {
  const args=[...argv];let json=false,quiet=false;const childEnv={...env};
  while(['--json','--quiet','--no-color'].includes(args[0])){const flag=args.shift();if(flag==='--json')json=true;else if(flag==='--quiet')quiet=true;else childEnv.NO_COLOR='1';}
  const logger=createLogger({json,quiet,env:childEnv,out,err});
  try{
    const [major,minor]=process.versions.node.split('.').map(Number);
    if(major<20||(major===20&&minor<19))throw new ToolError('Node.js20.19 or newer required',3);
    const command=args.shift();
    if(!command||['--help','-h','help'].includes(command)||args.includes('--help')||args.includes('-h')){const text=help(command);out.write(json?JSON.stringify({tool:'PakarController',help:text})+'\n':text);return 0;}
    if(command==='scripts'){const scripts=readdirSync(resolve(root,'tools')).filter(name=>name.endsWith('.ps1')).map(name=>name.slice(0,-4)).sort();out.write(json?JSON.stringify({scripts})+'\n':scripts.join('\n')+'\n');return 0;}
    if(command==='doctor'){const result=doctor({env:childEnv});if(json)out.write(JSON.stringify(result)+'\n');else{logger.info('Offline tool inventory · no installation/device contact');for(const tool of result.tools)(tool.available?logger.success:logger.warn)(`${tool.name}: ${tool.detail}`);}return 0;}
    let file,childArgs;
    if(['quality','dependencies','artifacts'].includes(command)){
      file=process.execPath;childArgs=[resolve(root,'tools',{'quality':'ci-checks.mjs','dependencies':'firmware-dependencies.mjs','artifacts':'firmware-report.mjs'}[command]),...(command==='quality'?['quality']:[]),...args];
    }else{
      let name;
      if(command==='run'||command==='test'){name=args.shift();if(command==='test')name='test-'+(name??'');}
      else if(command==='build')name='build-firmware';else if(commands[command])name=commands[command][0].slice(0,-4);else throw new ToolError('Unknown command. Run pakar --help');
      if(process.platform!=='win32'&&['capture-window-windows','stage-desktop','package-probe-windows'].includes(name))throw new ToolError('This utility requires Windows; no cross-platform operation is claimed',3);
      file='pwsh';childArgs=['-NoProfile','-File',scriptPath(name),...(command==='build'?legacyBuildArguments(args):portableArguments(args))];
    }
    logger.step(`PakarController · ${command}`);
    const result=await runner(file,childArgs,{cwd:root,env:childEnv,logger,json});
    (result.exit_code===0?logger.result:logger.error)(`${command} ${result.exit_code===0?'completed':'failed'} in ${(result.elapsed_ms/1000).toFixed(2)}s`,result);return result.exit_code;
  }catch(error){logger.error(error.message,{exit_code:error.exitCode??1});return error.exitCode??1;}
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href)process.exitCode=await main();
