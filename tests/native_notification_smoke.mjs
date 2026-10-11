// Opt-in real local Windows smoke. Never launch or quit an application, touch
// device parameters, alter Windows settings, or treat API acceptance as shown.
import assert from 'node:assert/strict';
import {execFile} from 'node:child_process';
import {promisify} from 'node:util';
import {readFile, mkdir, writeFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import path from 'node:path';
const run=promisify(execFile);
const [exe,pidText,output,sourceCommit,capture]=process.argv.slice(2);
assert(exe&&pidText&&output&&/^[0-9a-f]{40}$/.test(sourceCommit||''),
  'usage: node native_notification_smoke.mjs <running.exe> <expected-pid> <output-directory> <source-commit> [capture.exe]');
const owner=Number(pidText);
assert(Number.isSafeInteger(owner)&&owner>0,'Expected a positive owned process ID');
async function status(){
  const state=JSON.parse((await run(exe,['--app-status'])).stdout.trim());
  assert.equal(state.application,'PakarController');
  assert.equal(state.process_id,owner,'Never operate a different GUI owner');
  assert.equal(state.single_instance,true);
  return state;
}
const before=await status();
assert.equal(before.tray_available,true,'Native tray must be registered');
assert.notEqual(before.notifications.policy,'Off','Enable notifications yourself before this opt-in smoke');
assert.equal(typeof before.notifications.shell_events.shown,'number');
await run(exe,['--preferences-page','notifications']);
await status();
await run(exe,['--notification-test']);
let after;
for(let i=0;i<30;i++){
  await new Promise(r=>setTimeout(r,250));
  after=await status();
  if(after.notifications.accepted>before.notifications.accepted && after.notifications.shell_events.shown>before.notifications.shell_events.shown)break;
}
assert(after.notifications.accepted>before.notifications.accepted,'Shell did not accept a new request');
assert(after.notifications.shell_events.shown>before.notifications.shell_events.shown,
  'Windows did not report a shown banner. Do not change OS privacy/notification settings automatically.');
assert.equal(after.notifications.failed,before.notifications.failed);
await mkdir(output,{recursive:true});
const screenshot=capture?path.join(output,'notification-preferences.png'):null;
if(capture){ await status(); await run(capture,[screenshot,'Preferences — PakarController']); }
const receipt={scope:'Actual local Windows Shell notification, not device/ASUS acceptance',
  source_commit:sourceCommit,process_id:owner,
  executable_sha256:createHash('sha256').update(await readFile(exe)).digest('hex'),
  before,after,screenshot,limits:'No banner click, hidden-window, Jump List, taskbar restart or physical device test'};
await writeFile(path.join(output,'native-notification-receipt.json'),JSON.stringify(receipt,null,2));
console.log(JSON.stringify({passed:true,source_commit:sourceCommit,process_id:owner,
  sha256:receipt.executable_sha256,accepted:after.notifications.accepted,shown:after.notifications.shell_events.shown,
  failed:after.notifications.failed,screenshot}));
