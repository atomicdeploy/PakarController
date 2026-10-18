// PU850 upload.yml artifact-to-device utility, adapted to the SAME verified
// updater. No latest-success/first-bin guessing, untrusted PR run or blind retry.
import {readFileSync,realpathSync,lstatSync} from 'node:fs';
import {resolve,sep} from 'node:path';
import {pathToFileURL} from 'node:url';
import {spawnSync} from 'node:child_process';
import {imageIdentity,updateFirmware} from './update-firmware.mjs';
export function trustedBuild(run,{repository,expectedSha,runId}) {
  if(repository!=='atomicdeploy/PakarController'||!/^\d+$/.test(String(runId))||!/^[a-f0-9]{40}$/.test(expectedSha)||
    run.id!==Number(runId)||run.repository?.full_name!==repository||run.head_repository?.full_name!==repository||
    run.head_sha!==expectedSha||run.head_branch!=='main'||run.path!=='.github/workflows/build.yml'||
    !['push','workflow_dispatch'].includes(run.event)||run.status!=='completed'||run.conclusion!=='success')throw Error('Only an exact successful trusted main build may deploy');
  return run.head_sha;
}
export function verifiedArtifact(directory,expectedSha) {
  const root=realpathSync(directory),reportPath=resolve(root,'firmware-report.json'),path=resolve(root,'esp32s3-buffered/esp32s3.ino.bin');
  for(const file of [reportPath,path])if(lstatSync(file).isSymbolicLink()||!realpathSync(file).startsWith(root+sep))throw Error('Escaping deployment artifact');
  const report=JSON.parse(readFileSync(reportPath)),bytes=readFileSync(path),identity=imageIdentity(bytes);
  const row=report.artifacts?.filter(row=>row.path==='esp32s3-buffered/esp32s3.ino.bin');
  if(report.product!=='PakarController'||report.source_sha!==expectedSha||report.source_dirty!==false||row?.length!==1||
    row[0].format!=='raw'||row[0].size!==identity.size||row[0].sha256!==identity.sha256||row[0].md5!==identity.md5)throw Error('Artifact provenance/hash/size not verified');
  return {bytes,identity};
}
export async function deployArtifact({directory,expectedSha,origin,runId,repository='atomicdeploy/PakarController',run,
  updater=updateFirmware}={}) {
  trustedBuild(run,{repository,expectedSha,runId});const {bytes}=verifiedArtifact(directory,expectedSha);
  return await updater({image:bytes,origin,mode:'auto',run:true});
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
  try{
    const {PAKAR_BUILD_RUN:runId,PAKAR_EXPECTED_SHA:expectedSha,GITHUB_REPOSITORY:repository,PAKAR_DEVICE_ORIGIN:origin,PAKAR_INCOMING:directory}=process.env;
    if(!/^\d+$/.test(runId??''))throw Error('Explicit build run required');
    const result=spawnSync('gh',['api',`repos/${repository}/actions/runs/${runId}`],{encoding:'utf8',timeout:15000,windowsHide:true});
    if(result.status!==0)throw Error('Build metadata lookup failed; no deployment');
    const run=JSON.parse(result.stdout);trustedBuild(run,{repository,expectedSha,runId});
    if(process.argv.includes('--preflight'))console.log(JSON.stringify({trusted:true,source_sha:expectedSha,run_id:runId,device_contacted:false}));
    else if(process.argv.includes('--run'))console.log(JSON.stringify(await deployArtifact({directory,expectedSha,origin,runId,repository,run}),null,2));
    else throw Error('Use --preflight or explicit --run');
  }catch(error){console.error(error.message);process.exitCode=1;}
}
