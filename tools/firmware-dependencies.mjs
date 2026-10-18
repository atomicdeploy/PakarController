// Adapted PU upgrade.sh exact-name indexing, check/apply and no-downgrade flow.
// One memory index, numeric SemVer (not lexical max), bounded HTTPS, one atomic
// pin-file update. This does not install dependencies or flash a device by default.
import {readFileSync,writeFileSync,renameSync,existsSync} from 'node:fs';
import {resolve,dirname} from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
import {gunzipSync} from 'node:zlib';
import {createLogger,runChild,ToolError} from './script-console.mjs';
const root=resolve(dirname(fileURLToPath(import.meta.url)),'..');
export const pinFile=resolve(root,'tools/firmware-dependencies.json');
const semver=/^(0|[1-9]\d*)\.(0|[1-9]\d*)(?:\.(0|[1-9]\d*))?$/;
export function compareVersions(a,b) {
  if(!semver.test(a)||!semver.test(b))throw new ToolError('Expected stable numeric major.minor.patch version');
  const parts=value=>[...value.split('.').map(Number),0,0].slice(0,3);
  const left=parts(a),right=parts(b);
  if([...left,...right].some(n=>!Number.isSafeInteger(n)))throw new ToolError('Version component exceeds bounds');
  for(let i=0;i<3;i++)if(left[i]!==right[i])return left[i]<right[i]?-1:1;return 0;
}
function indexUrl(raw) {
  const url=new URL(raw);
  if(url.protocol!=='https:'||url.username||url.password||url.hash||url.search||/[\s\\]/.test(raw))throw new ToolError('Dependency index must be direct HTTPS');
  return url;
}
export function validatePins(pins) {
  if(!pins||!Array.isArray(pins.platforms)||!pins.platforms.length||!Array.isArray(pins.libraries)||!pins.libraries.length)throw new ToolError('Invalid firmware dependency pins');
  for(const [kind,rows] of [['platform',pins.platforms],['library',pins.libraries]]){
    const names=new Set();
    for(const row of rows){
      if(typeof row.name!=='string'||!(/^[A-Za-z0-9][A-Za-z0-9 ._+-]{0,79}$/.test(row.name)||(kind==='platform'&&/^[A-Za-z0-9_-]+:[A-Za-z0-9_-]+$/.test(row.name)))||names.has(row.name))throw new ToolError('Invalid/duplicate dependency name');
      names.add(row.name);compareVersions(row.version,row.version);if(kind==='platform')indexUrl(row.index);
    }
  }indexUrl(pins.library_index);return pins;
}
export function installationPlan(pins) {
  validatePins(pins);
  return [...new Set(pins.platforms.map(row=>row.index))].map(url=>['config','add','board_manager.additional_urls',url])
    .concat([['core','update-index']],pins.platforms.map(row=>['core','install',row.name+'@'+row.version]),pins.libraries.map(row=>['lib','install',row.name+'@'+row.version]));
}
export async function fetchIndex(raw,{fetcher=fetch,env=process.env,curl=spawnSync}={}) {
  const url=indexUrl(raw);
  // PU's curl path honors the operator's HTTP(S)/SOCKS proxy configuration.
  // Node20 fetch does not, so keep curl for that case. Proxy credentials stay
  // only in the inherited environment, never argv, progress or error text.
  if(env.https_proxy||env.HTTPS_PROXY||env.all_proxy||env.ALL_PROXY){
    const response=curl('curl',['-q','--fail','--silent','--show-error','--proto','=https','--max-time','30','--max-filesize',String(32*1024*1024),url.href],
      {env,windowsHide:true,shell:false,timeout:35000,maxBuffer:32*1024*1024});
    if(response.error||response.status!==0)throw new ToolError('Dependency index proxy download failed; inspect proxy/certificate configuration. No pins changed.',4);
    return decodeIndex(Buffer.from(response.stdout));
  }
  const response=await fetcher(url,{redirect:'error',signal:AbortSignal.timeout(30000)});
  if(!response.ok)throw new ToolError('Index fetch rejected: HTTP '+response.status,4);
  const reader=response.body.getReader(),chunks=[];let size=0;
  try{for(;;){const {done,value}=await reader.read();if(done)break;size+=value.length;if(size>32*1024*1024)throw new ToolError('Compressed index exceeds32MiB',4);chunks.push(Buffer.from(value));}}
  finally{await reader.cancel();}
  return decodeIndex(Buffer.concat(chunks));
}
function decodeIndex(bytes) {
  if(bytes.length>32*1024*1024)throw new ToolError('Compressed index exceeds32MiB',4);
  if(bytes[0]===31&&bytes[1]===139)bytes=gunzipSync(bytes,{maxOutputLength:256*1024*1024});
  try{return JSON.parse(bytes)}catch{throw new ToolError('Malformed dependency index',5);}
}
export async function checkDependencies(pins,{readIndex=fetchIndex}={}) {
  validatePins(pins);const cache=new Map(),result=[];
  const read=async url=>{if(!cache.has(url))cache.set(url,await readIndex(url));return cache.get(url);};
  for(const [kind,rows] of [['platform',pins.platforms],['library',pins.libraries]])for(const row of rows){
    const index=await read(kind==='platform'?row.index:pins.library_index);
    const candidates=kind==='platform'?index.packages?.flatMap(pkg=>(pkg.platforms??[]).filter(p=>pkg.name+':'+p.architecture===row.name).map(p=>p.version)):
      index.libraries?.filter(lib=>lib.name===row.name).map(lib=>lib.version);
    const versions=(candidates??[]).filter(value=>typeof value==='string'&&semver.test(value));
    if(!versions.length)throw new ToolError('No stable registry version for '+row.name,5);
    const latest=versions.sort(compareVersions).at(-1),update=compareVersions(latest,row.version)>0;
    result.push({kind,name:row.name,current:row.version,latest,update});
  }return result;
}
// Update only reviewed version slots; detect concurrent edits and refuse stale
// pending output rather than truncating the old pins or overwriting another run.
export function applyUpdates(path,before,pins,updates) {
  const next=structuredClone(pins);let changes=0;
  for(const update of updates.filter(row=>row.update)){
    const row=next[update.kind==='platform'?'platforms':'libraries'].find(row=>row.name===update.name);
    if(!row||row.version!==update.current||compareVersions(update.latest,row.version)<=0)throw new ToolError('Dependency update identity changed',6);
    row.version=update.latest;changes++;
  }
  if(!changes)return false;
  if(readFileSync(path,'utf8')!==before)throw new ToolError('Pin file changed during check; nothing overwritten',6);
  const pending=path+'.pending';writeFileSync(pending,JSON.stringify(next,null,2)+'\n',{flag:'wx'});
  try{if(readFileSync(path,'utf8')!==before)throw new ToolError('Concurrent pin change; inspect pending output',6);renameSync(pending,path);}
  catch(error){throw error;}return true;
}
export function dependencyMarkdown(rows) {
  return '# Firmware dependency check\n\n| Dependency | Pinned | Registry stable | Result |\n|---|---|---|---|\n'+rows.map(row=>`| ${row.name} | ${row.current} | ${row.latest} | ${row.update?'Update available':compareVersions(row.current,row.latest)>0?'Pinned ahead; no downgrade':'Current'} |`).join('\n')+'\n\nVersion changes require full CI and hardware acceptance; no SDK installation or device update is implied.\n';
}
export async function main(args=process.argv.slice(2)) {
  const allowed=new Set(['--check','--apply','--install','--plan','--json','--help']);
  if(args.some(arg=>!allowed.has(arg)))throw new ToolError('Unknown dependency option');
  if(args.includes('--help')){console.log('PakarController dependencies: --plan (offline) | --check (default) | --apply (pin edits) | --install (locked SDK install). --json supported.');return;}
  if(['--apply','--install','--check'].filter(arg=>args.includes(arg)).length>1)throw new ToolError('Select one dependency operation');
  const json=args.includes('--json'),logger=createLogger({json}),before=readFileSync(pinFile,'utf8'),pins=validatePins(JSON.parse(before));
  if(args.includes('--plan')){console.log(JSON.stringify({pins,commands:installationPlan(pins),installed:false,device_contacted:false},null,2));return;}
  if(args.includes('--install')){
    const executable=existsSync(resolve(root,'tools/arduino-cli.exe'))?resolve(root,'tools/arduino-cli.exe'):'arduino-cli';
    // Reuse existing config; don't initialize/overwrite user directories or URLs.
    const config=spawnConfig(executable);const existing=new Set(config.board_manager?.additional_urls??[]);
    for(const command of installationPlan(pins)){
      if(command[0]==='config'&&existing.has(command.at(-1)))continue;
      logger.step('Arduino '+command.slice(0,2).join(' '));
      const result=await runChild(executable,command,{cwd:root,logger,json});
      if(result.exit_code)throw new ToolError('Locked dependency installation failed; no unpinned fallback',result.exit_code);
    }logger.result('Locked dependencies installed; no firmware build/device contact');return;
  }
  const rows=await checkDependencies(pins);let applied=false;
  if(args.includes('--apply'))applied=applyUpdates(pinFile,before,pins,rows);
  if(json)console.log(JSON.stringify({rows,applied,updates_available:rows.some(row=>row.update)}));
  else{for(const row of rows)(row.update?logger.warn:logger.success)(`${row.kind} ${row.name}: ${row.current} → ${row.latest}${row.update?' (available)':''}`);logger.result(applied?'Pin updates applied; review/CI required':'Check complete; pins unchanged');}
  if(process.env.GITHUB_STEP_SUMMARY)writeFileSync(process.env.GITHUB_STEP_SUMMARY,dependencyMarkdown(rows),{flag:'a'});
}
// Native argv, not a shell/PowerShell string; no token or password in arguments.
import {spawnSync} from 'node:child_process';
function spawnConfig(executable){const result=spawnSync(executable,['config','dump','--format','json'],{encoding:'utf8',windowsHide:true,timeout:10000});if(result.status!==0)throw new ToolError('Arduino CLI configuration unavailable',3);return JSON.parse(result.stdout);}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{await main();}catch(error){createLogger().error(error.message);process.exitCode=error.exitCode??1;}}
