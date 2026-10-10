// Explicit simulated, read-only visual peer. Does not quit/forward to the
// user's existing application. Select only the spawned executable's window.
import assert from 'node:assert/strict';
import http from 'node:http';
import {spawn} from 'node:child_process';
import {writeFile,mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
const [exe,profile,view='firmware']=process.argv.slice(2);assert(exe&&profile);assert(['firmware','table','cells'].includes(view));
await mkdir(profile,{recursive:true});
const remote={schema:1,job_id:0,boot_id:'SIMULATED-visual-fixture',phase:'idle',targets:3,manifest_sha256:'',busy:false,cancellable:false,received_bytes:0,expected_bytes:0,error:''};
const server=http.createServer(async(req,res)=>{
 if(req.method==='POST'&&req.url==='/api/v1/clients/report'){
  let raw='';for await(const c of req)raw+=c;const report=JSON.parse(new URLSearchParams(raw).get('report'));
  assert.equal(report.application,'PakarController');assert.equal(report.version,'0.2.7');
  res.writeHead(200,{'Content-Type':'application/json'});res.end('{"accepted":true}');return;
 }
 if(req.method!=='GET'){res.writeHead(405);res.end('Fixture refuses hardware writes');return;}
 const values={
  '/api/v1/status':{hostname:'SIMULATED remote fixture',version:'0.2.7',mode:'idle',brightness:200,effect:'rainbow',pixels:480,avr_online:true,station_connected:true,programming_latch:false,cells:Array(80).fill('white'),tm1637_segments:[6,91,79,102]},
  '/api/v1/network':{station_enabled:true,ap_enabled:true,station_ssid:'SIMULATED'},
  '/api/v1/cloud':{mqtt_enabled:false,mqtt_tls:true,mqtt_host:'mqtt.example.invalid',mqtt_port:8883,mqtt_user:'fixture',mqtt_base_topic:'pakar/fixture',mqtt_password_set:false,websocket_enabled:false,websocket_tls:true,websocket_host:'ws.example.invalid',websocket_port:443,websocket_path:'/controller',websocket_token_set:false,telemetry_seconds:30},
  '/api/v1/device/name':{hostname:'SIMULATED-visual-fixture'},
  '/api/v1/auth/info':{ready:true,default_credentials:false,username:'fixture'},
  '/api/v1/cells/mapping':{physical:Array.from({length:80},(_,i)=>i+1),names:Array(80).fill('')},
  '/api/v1/firmware/remote':remote,
  '/api/v1/telemetry':{schema:1,type:'telemetry',product:'PakarController',boot_id:'SIMULATED-visual-fixture',esp:{model:'SIMULATED ONLY'},avr:{model:'SIMULATED ONLY'},clients:[]}
 };
 const value=values[req.url];res.writeHead(value?200:404,{'Content-Type':'application/json'});res.end(JSON.stringify(value||{}));
});
server.on('upgrade',(_req,socket)=>socket.destroy());
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const endpoint='http://127.0.0.1:'+server.address().port;
const config=resolve(profile,'preferences.json');await writeFile(config,JSON.stringify({endpoint,user:'fixture',single_instance:false,native_dialogs:true,theme:'Dark',close_to_tray:false}));
const args=view==='table'?['--panel','Table']:view==='cells'?['--manage-cells']:['--settings-page','firmware'];
const child=spawn(exe,args,{env:{...process.env,PAKAR_CONFIG_FILE:config,PAKAR_ENDPOINT:endpoint,PAKAR_USER:'fixture',PAKAR_PASSWORD:'fixture-only'},windowsHide:true,stdio:['ignore','pipe','pipe']});
child.stdout.resume();child.stderr.on('data',data=>process.stderr.write(data));
console.log(JSON.stringify({scope:'SIMULATED peer, no physical firmware evidence',process_id:child.pid,executable:resolve(exe),endpoint,isolated_profile:config}));
child.once('exit',()=>server.close());
