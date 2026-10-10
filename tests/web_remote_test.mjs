import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const source=readFileSync(new URL('../firmware/esp8266/src/WebUi.h',import.meta.url),'utf8');
const script=source.match(/<script>([\s\S]*?)<\/script>/)[1];new vm.Script(script); // Parse the complete shipped script.
let posted;
const context=vm.createContext({URLSearchParams,Number,JSON,Error,confirm:()=>true,report:e=>{throw e;},
  remoteState:{textContent:''},remoteStage:{disabled:false},remoteApply:{disabled:false},remoteCancel:{disabled:false},
  api:async(path,options)=>{posted={path,body:options?.body};return job;}});
vm.runInContext('let remoteJob=null;',context);
for(const name of ['validRemoteJob','showRemoteJob','remoteAction'])vm.runInContext(source.match(new RegExp('^(?:async )?function '+name+'\\([^\\n]+$','m'))[0],context);
const job={schema:1,job_id:4,boot_id:'owner-boot',targets:3,manifest_sha256:'a'.repeat(64),phase:'ready',busy:false,cancellable:false};context.job=job;
vm.runInContext('showRemoteJob(job)',context);assert.equal(context.remoteApply.disabled,false);assert.equal(context.remoteCancel.disabled,false);
await vm.runInContext("remoteAction('apply')",context);assert.equal(posted.path,'/api/v1/firmware/apply');
assert.equal(posted.body.get('job_id'),'4');assert.equal(posted.body.get('boot_id'),'owner-boot');assert.equal(posted.body.get('sha256'),'a'.repeat(64));
context.job={...job,phase:'release_pair',busy:true};vm.runInContext('showRemoteJob(job)',context);assert.equal(context.remoteApply.disabled,true);assert.equal(context.remoteCancel.disabled,true);
context.job={...job,phase:'synthetic-success'};assert.throws(()=>vm.runInContext('showRemoteJob(job)',context));
assert.match(script,/apply:'false'/);assert.match(script,/remoteBearer\.value=''/);
console.log('Complete Web script parses; actual production remote controls pin job/boot/hash, stage-only default, clear bearer and reject invalid phases.');
