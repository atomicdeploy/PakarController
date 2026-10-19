// Explicit simulated, read-only visual peer. Does not quit/forward to the
// user's existing application. Select only the spawned executable's window.
import assert from 'node:assert/strict';
import http from 'node:http';
import {spawn} from 'node:child_process';
import {writeFile,mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
import {toolRelease,toolPolicy} from '../tools/peer-identity.mjs';
const [exe,profile,view='firmware',screenshot]=process.argv.slice(2);assert(exe&&profile);assert(['firmware','table','cells','highlight'].includes(view));
await mkdir(profile,{recursive:true});
const simulatedIdentity={board_id:'esp32s3-simulated-visual-fixture',boot_id:'b'.repeat(32),http_peer_fencing:true};
const remote={job_id:0,boot_id:simulatedIdentity.boot_id,phase:'idle',targets:3,manifest_sha256:'',busy:false,cancellable:false,received_bytes:0,expected_bytes:0,error:''};
const server=http.createServer(async(req,res)=>{
 res.setHeader('Server','PakarController/'+toolRelease.version);res.setHeader('X-Pakar-Version',toolRelease.version);
 if(req.method==='POST'&&req.url==='/api/clients/report'){
  let raw='';for await(const c of req)raw+=c;const report=JSON.parse(new URLSearchParams(raw).get('report'));
  assert.equal(report.application,'PakarController');assert.equal(report.version,toolRelease.version);
  res.writeHead(200,{'Content-Type':'application/json'});res.end('{"accepted":true}');return;
 }
 if(req.method!=='GET'){res.writeHead(405);res.end('Fixture refuses hardware writes');return;}
 const values={
  '/api/status':{...simulatedIdentity,hostname:'SIMULATED remote fixture',version:toolRelease.version,compatibility:{...toolPolicy,enforced:toolPolicy.enforce},mode:'table',brightness:200,effect:'rainbow',pixels:480,avr_online:true,station_connected:true,programming_latch:false,cells:Array(80).fill('white'),tm1637_segments:[6,91,79,102]},
  '/api/network':{station_enabled:true,ap_enabled:true,station_ssid:'SIMULATED'},
  '/api/cloud':{mqtt_enabled:false,mqtt_tls:true,mqtt_host:'mqtt.example.invalid',mqtt_port:8883,mqtt_user:'fixture',mqtt_base_topic:'pakar/fixture',mqtt_password_set:false,websocket_enabled:false,websocket_tls:true,websocket_host:'ws.example.invalid',websocket_port:443,websocket_path:'/controller',websocket_token_set:false,telemetry_seconds:30},
  '/api/device/name':{hostname:'SIMULATED-visual-fixture'},
  '/api/auth/info':{ready:true,default_credentials:false,username:'fixture'},
  '/api/cells/mapping':{physical:Array.from({length:80},(_,i)=>i+1),names:Array(80).fill('')},
  '/api/firmware/remote':remote,
  '/api/telemetry':{type:'telemetry',product:'PakarController',boot_id:'SIMULATED-visual-fixture',esp:{model:'SIMULATED ONLY'},avr:{model:'SIMULATED ONLY'},clients:[]}
 };
 const value=values[req.url];res.writeHead(value?200:404,{'Content-Type':'application/json'});res.end(JSON.stringify(value||{}));
});
server.on('upgrade',(_req,socket)=>socket.destroy());
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const endpoint='http://127.0.0.1:'+server.address().port;
const config=resolve(profile,'preferences.json');await writeFile(config,JSON.stringify({endpoint,user:'fixture',single_instance:false,native_dialogs:true,theme:'Dark',close_to_tray:false,notifications:{system:'Off',in_app:false}}));
const args=view==='highlight'?['--find-cell','1,3-8']:view==='table'?['--panel','Table']:view==='cells'?['--manage-cells']:['--settings-page','firmware'];
const child=spawn(exe,args,{env:{...process.env,PAKAR_CONFIG_FILE:config,PAKAR_ENDPOINT:endpoint,PAKAR_USER:'fixture',PAKAR_PASSWORD:'fixture-only',...(screenshot?{PAKAR_SCREENSHOT_PATH:resolve(screenshot)}:{})},windowsHide:true,stdio:['ignore','pipe','pipe']});
child.stdout.resume();child.stderr.on('data',data=>process.stderr.write(data));
console.log(JSON.stringify({scope:'SIMULATED peer, no physical firmware evidence',process_id:child.pid,executable:resolve(exe),endpoint,isolated_profile:config}));
child.once('exit',()=>server.close());
