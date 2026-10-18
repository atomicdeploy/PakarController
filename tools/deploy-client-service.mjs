// PU850 tooling adaptation: explicit plan/run, bounded child exits, one stable
// destination and no arbitrary shell payload from an endpoint or device.
import {readFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {resolve} from 'node:path';
import {createLogger} from './script-console.mjs';
const root=fileURLToPath(new URL('../',import.meta.url)),log=createLogger();
const args=process.argv.slice(2),run=args.includes('--run');
for(const concern of ['client-management','pu-tooling']){const gate=spawnSync('pwsh',['-NoProfile','-File','tools/consult-reuse.ps1','-Concern',concern,'-Gate','-Summary'],{cwd:root,stdio:'inherit',windowsHide:true});if(gate.error||gate.status!==0)throw Error('Required reuse gate failed: '+concern);}
if(args.some(a=>!['--run','--plan'].includes(a)))throw Error('Usage: node tools/deploy-client-service.mjs --plan|--run');
const host='root@office.digitalogic.ir';
const ssh=['-o','BatchMode=yes','-o','ConnectTimeout=8','-o','StrictHostKeyChecking=yes',host];
const files=['services/client-management/service.mjs','services/client-management/operator.mjs','services/client-management/dashboard.html','services/client-management/package.json','services/client-management/package-lock.json','services/client-management/pakarcontroller-management.service','services/client-management/install-office.sh','assets/branding/app-icon.png.base64'];
log.info('Temporary HTTPS client management · https://office.digitalogic.ir/pakar');
log.detail('Loopback service only; scoped Apache include; no PBX/MQTT changes. Admin token stays root-only on office.');
if(!run){log.result('Plan only: upload '+files.length+' files, validate Apache, enable dedicated service. Use --run to deploy.');process.exit(0);}
function command(file,args,input){const r=spawnSync(file,args,{cwd:root,input,encoding:'utf8',timeout:120000,windowsHide:true});
  if(r.stdout)log.detail(r.stdout);if(r.stderr)log.detail(r.stderr);if(r.error||r.status!==0)throw Error('Scoped service deployment stopped: '+file+' '+(r.error?.code??r.status));}
command('ssh',[...ssh,'install -d -m 0700 /root/.cache/pakarcontroller-management-deploy']);
command('scp',['-o','BatchMode=yes','-o','ConnectTimeout=8','-o','StrictHostKeyChecking=yes',...files.map(p=>resolve(root,p)),host+':/root/.cache/pakarcontroller-management-deploy/']);
command('ssh',[...ssh,'bash /root/.cache/pakarcontroller-management-deploy/install-office.sh']);
log.result('Office service deployed. Client installation/enrollment is separately verified.');
