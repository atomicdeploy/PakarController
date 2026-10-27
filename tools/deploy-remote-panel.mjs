// Explicit, additive office deployment. Reuse deploy-client-service's bounded
// SSH/SCP, shared gates, pinned lock and dedicated installation discipline.
import {spawnSync} from 'node:child_process';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
const root=fileURLToPath(new URL('../',import.meta.url)),args=process.argv.slice(2);
if(args.some(a=>!['--run','--plan'].includes(a)))throw Error('Use --plan or --run');
function run(tool,args,input){const r=spawnSync(tool,args,{cwd:root,input,encoding:'utf8',timeout:120000,windowsHide:true});if(r.stdout)process.stdout.write(r.stdout);if(r.stderr)process.stderr.write(r.stderr);if(r.error||r.status!==0)throw Error('Remote panel stopped: '+tool);}
for(const concern of ['client-management','pu-tooling'])run('pwsh',['-NoProfile','-File','tools/consult-reuse.ps1','-Concern',concern,'-Gate','-Summary']);
run(process.execPath,['--test','services/remote-panel/gateway.test.mjs']);
if(!args.includes('--run')){console.log('Plan: private loopback MQTT relay, current actual Web UI, dedicated Apache vhost; existing Wi-Fi/MQTT/PBX untouched. DNS is a separate scoped operation.');process.exit(0)}
const host='root@office.digitalogic.ir',ssh=['-o','BatchMode=yes','-o','ConnectTimeout=8','-o','StrictHostKeyChecking=yes'];
const files=['services/remote-panel/service.mjs','services/remote-panel/gateway.mjs','services/remote-panel/package.json','services/remote-panel/package-lock.json','services/remote-panel/install-office.sh','services/remote-panel/apache.conf','services/remote-panel/pakarcontroller-remote-panel.service','tools/peer-identity.mjs','tools/telemetry-reassembly.mjs','interfaces/peer-policy.json','interfaces/client-release.json','assets/fonts/Vazirmatn-Regular.ttf','firmware/esp/src/WebUi.h','assets/web/peer-http.js','assets/web/basic-cells.js','assets/web/basic.html'];
run('ssh',[...ssh,host,'install -d -m 0700 /root/.cache/pakarcontroller-remote-panel']);
run('scp',[...ssh,...files.map(p=>resolve(root,p)),host+':/root/.cache/pakarcontroller-remote-panel/']);
run('ssh',[...ssh,host,'bash /root/.cache/pakarcontroller-remote-panel/install-office.sh']);
