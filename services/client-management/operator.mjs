// Root-local operator for the dedicated office service. Credentials never appear
// in process arguments, telemetry or normal inventory. Enrollment output is secret.
import {readFile} from 'node:fs/promises';
const token=(await readFile('/etc/pakarcontroller-management/admin-token','utf8')).trim();
if(!/^[A-Za-z0-9_-]{43}$/.test(token))throw Error('Private operator credential unavailable');
const [action,client,command]=process.argv.slice(2);
let method='GET',path,body;
if(action==='clients')path='/api/admin/clients';
else if(action==='commands')path='/api/admin/commands';
else if(action==='enroll'&&['android','windows-x64'].includes(client)){method='POST';path='/api/admin/enrollments';body={platform:client};}
else if(action==='cleanup'){method='POST';path='/api/admin/cleanup';body={};}
else if(['command','revoke'].includes(action)&&/^[A-Za-z0-9_-]{16,80}$/.test(client??'')){
  method='POST';path='/api/admin/clients/'+client+(action==='command'?'/commands':'/revoke');
  if(action==='command'&&!['check-update','cleanup'].includes(command))throw Error('Only check-update and cleanup may be queued by this tool');body=action==='command'?{action:command}:{};
}else throw Error('Usage: operator.mjs clients|commands|enroll android|windows-x64|cleanup|command CLIENT check-update|cleanup|revoke CLIENT');
const r=await fetch('http://127.0.0.1:18743/pakar'+path,{method,headers:{Authorization:'Bearer '+token,...(body?{'Content-Type':'application/json'}:{})},body:body?JSON.stringify(body):undefined,redirect:'error',signal:AbortSignal.timeout(10000)});
const value=await r.json();if(!r.ok)throw Error('Management request rejected: HTTP '+r.status);
process.stdout.write(JSON.stringify(value)+'\n');
