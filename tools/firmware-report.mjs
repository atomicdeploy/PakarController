// PU build.sh binary-size/MD5/compression and build.yml download summaries,
// adapted without renaming gzip to .bin, sourcing shell metadata or rewriting
// timestamps. S3 application bytes always stay raw; compression is explicit.
import {readFileSync,writeFileSync,existsSync,realpathSync,lstatSync} from 'node:fs';
import {resolve,relative,sep} from 'node:path';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {fileURLToPath,pathToFileURL} from 'node:url';
import {portableGzip} from './portable-gzip.mjs';
export const expectedArtifacts=[
  ...['','-busy-wire','-buffered'].map(mode=>`avr${mode}/avr.ino.with_bootloader.bin`),
  ...['','-busy-wire'].map(mode=>`esp8266${mode}/esp8266.ino.bin`),
  'esp32s3-buffered/esp32s3.ino.bin',
];
const digest=(algorithm,bytes)=>createHash(algorithm).update(bytes).digest('hex');
export function unchangedWrite(path,bytes) {
  if(existsSync(path)&&readFileSync(path).equals(Buffer.from(bytes)))return false;
  // Reports are derived metadata, not original firmware/settings/recovery data.
  writeFileSync(path,bytes);return true;
}
export function collectArtifacts(directory,{required=false,compress=false}={}) {
  const root=realpathSync(directory),artifacts=[];
  for(const name of expectedArtifacts){
    const path=resolve(root,name);
    if(!existsSync(path)){if(required)throw Error('Missing expected firmware artifact: '+name);continue;}
    if(lstatSync(path).isSymbolicLink()||!realpathSync(path).startsWith(root+sep))throw Error('Escaping firmware artifact');
    const bytes=readFileSync(path);
    if(!bytes.length||bytes.length>4*1024*1024)throw Error('Invalid firmware artifact size');
    if(name.startsWith('avr')&&bytes.length!==32768)throw Error('AVR image must include full flash/Urboot');
    if(name.startsWith('esp')&&bytes[0]!==0xe9)throw Error('ESP application must remain raw; do not rename gzip to .bin');
    artifacts.push({path:name,size:bytes.length,sha256:digest('sha256',bytes),md5:digest('md5',bytes),format:'raw'});
    if(compress&&name.startsWith('esp8266')){
      const gz=portableGzip(bytes);unchangedWrite(path+'.gz',gz);
      artifacts.push({path:name+'.gz',size:gz.length,sha256:digest('sha256',gz),md5:digest('md5',gz),format:'gzip',raw_sha256:digest('sha256',bytes),raw_size:bytes.length});
    }
  }return artifacts;
}
export function artifactMarkdown(report,url='') {
  const link=/^https:\/\/github\.com\/[\w.-]+\/[\w.-]+\/actions\/runs\/\d+(?:\/artifacts\/\d+)?$/.test(url)?`\n[Download build artifacts](${url})\n`:'';
  return '# PakarController firmware artifacts\n\n'+`Source: \`${report.source_sha}\` · source tree ${report.source_dirty?'dirty / not a release':'clean'}\n`+link+
    '\n| Artifact | Bytes | SHA-256 |\n|---|---:|---|\n'+report.artifacts.map(row=>`| ${row.path} | ${row.size.toLocaleString('en-US')} | \`${row.sha256}\` |`).join('\n')+
    '\n\nMD5 and format/size metadata are in firmware-report.json. Build artifacts do not prove installation, electrical/optical operation or physical acceptance.\n';
}
export function main(args=process.argv.slice(2)) {
  const options=new Set(args);for(const arg of options)if(!['--write','--require','--compress','--help'].includes(arg))throw Error('Unknown report option');
  if(options.has('--help')){console.log('PakarController artifacts: read-only by default. --write emits stable metadata; --require needs all six images; --compress adds separate ESP8266 .gz only.');return;}
  const root=fileURLToPath(new URL('../',import.meta.url)),directory=resolve(root,'build');
  const source_sha=execFileSync('git',['rev-parse','HEAD'],{cwd:root,encoding:'utf8'}).trim();
  const source_dirty=!!execFileSync('git',['status','--porcelain'],{cwd:root,encoding:'utf8'}).trim();
  const report={product:'PakarController',source_sha,source_dirty,scope:'existing artifact bytes, not embedded-source or hardware proof',artifacts:collectArtifacts(directory,{required:options.has('--require'),compress:options.has('--compress')})};
  if(options.has('--write')){
    unchangedWrite(resolve(directory,'firmware-report.json'),JSON.stringify(report,null,2)+'\n');
    unchangedWrite(resolve(directory,'SHA256SUMS'),report.artifacts.map(row=>`${row.sha256}  ${row.path}`).join('\n')+'\n');
  }
  if(process.env.GITHUB_STEP_SUMMARY)writeFileSync(process.env.GITHUB_STEP_SUMMARY,artifactMarkdown(report,process.env.PAKAR_ARTIFACT_URL??''),{flag:'a'});
  console.log(JSON.stringify(report,null,2));return report;
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{main();}catch(error){console.error(error.message);process.exitCode=1;}}
