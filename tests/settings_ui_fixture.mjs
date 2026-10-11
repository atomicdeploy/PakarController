// Visible loopback fixture, NOT a real board. Own profile; CLI/IPC navigation only.
import assert from 'node:assert/strict';
import http from 'node:http';
import {spawn,execFile} from 'node:child_process';
import {promisify} from 'node:util';
import {mkdtemp,writeFile,readFile,mkdir} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {createHash} from 'node:crypto';
const run=promisify(execFile);
const [exe,capture,output]=process.argv.slice(2);
assert(exe&&capture&&output,'usage: node settings_ui_fixture.mjs <exe> <capture.exe> <evidence-directory>');
const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
const profile=await mkdtemp(path.join(tmpdir(),'PakarController-settings-fixture-'));
const requests=[];
const network={station_enabled:true,ap_enabled:true,station_ssid:'SIMULATED network'};
const cloud={mqtt_enabled:false,mqtt_tls:true,mqtt_host:'mqtt.example.invalid',mqtt_port:8883,mqtt_user:'fixture',mqtt_base_topic:'pakar/fixture',mqtt_password_set:true,websocket_enabled:false,websocket_tls:true,websocket_host:'ws.example.invalid',websocket_port:443,websocket_path:'/controller',websocket_token_set:false,telemetry_seconds:30};
const server=http.createServer((req,res)=>{
  requests.push({method:req.method,path:req.url});
  if(req.method!=='GET'){res.writeHead(405);res.end('This visual fixture is read-only');return;}
  const values={
    '/api/v1/status':{hostname:'SIMULATED-settings-fixture',version:'FIXTURE ONLY',mode:'idle',brightness:200,effect:'rainbow',pixels:480,station_enabled:true,ap_enabled:true,station_connected:true,ap_started:true,ntp_synced:true,link_mode:'busy-wire',avr_online:true,tm1637_ack:true,tm1637_segments:[6,91,79,102],tm1637_brightness:4,tm1637_host_active:true,cells:Array(80).fill('white')},
    '/api/v1/network':network,'/api/v1/cloud':cloud,
    '/api/v1/device/name':{hostname:'SIMULATED-settings-fixture'},
    '/api/v1/auth/info':{ready:true,default_credentials:false,username:'fixture'},
    '/api/v1/cells/mapping':{physical:Array.from({length:80},(_,i)=>i+1),names:Array(80).fill('')},
  };
  const value=values[req.url];res.writeHead(value?200:404,{'Content-Type':'application/json'});res.end(JSON.stringify(value||{}));
});
server.on('upgrade',(_req,socket)=>socket.destroy());
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const endpoint='http://127.0.0.1:'+server.address().port;
const env={...process.env,PAKAR_CONFIG_FILE:path.join(profile,'preferences.json'),PAKAR_ENDPOINT:endpoint,PAKAR_USER:'fixture',PAKAR_PASSWORD:'fixture-secret'};
await writeFile(env.PAKAR_CONFIG_FILE,JSON.stringify({endpoint,user:'fixture',theme:'Dark',native_dialogs:true,single_instance:true}));
let child;
try{
  await assert.rejects(run(exe,['--app-status'],{env}), 'Close the existing app gracefully before running this owned fixture');
  child=spawn(exe,['--device-settings'],{env,windowsHide:true,stdio:['ignore','pipe','pipe']});
  child.stdout.resume();child.stderr.resume();
  const status=async()=>JSON.parse((await run(exe,['--app-status'],{env})).stdout.trim());
  const awaitState=async predicate=>{
    for(let attempt=0;attempt<50;attempt++){
      let value;try{value=await status();}catch{}
      if(value){assert.equal(value.process_id,child.pid,'Never control another GUI owner');if(predicate(value))return value;}
      await pause(200);
    }
    throw new Error('GUI fixture did not reach the requested state');
  };
  await awaitState(s=>s.device_settings?.network_loaded&&s.device_settings?.cloud_loaded&&s.device_online);
  await mkdir(output,{recursive:true});
  const captures=[];
  for(const [page,index] of [['overview',0],['wifi',2],['mqtt',3],['firmware',9]]){
    await run(exe,['--settings-page',page],{env});
    const value=await awaitState(s=>s.device_settings?.page===index&&s.device_settings?.open);
    await pause(500);
    const file=path.join(output,`settings-${page}-fixture.png`);
    await run(capture,[file,'Device settings — PakarController']);
    captures.push({page,file,snapshot:value});
  }
  assert(requests.every(request=>request.method==='GET'),'Visual proof must not write to any peer');
  const receipt={scope:'SIMULATED loopback visual proof, not board acceptance',endpoint,process_id:child.pid,exe_sha256:createHash('sha256').update(await readFile(exe)).digest('hex'),captures,requests,isolated_profile:profile};
  await writeFile(path.join(output,'settings-fixture-receipt.json'),JSON.stringify(receipt,null,2));
  console.log(JSON.stringify({passed:true,scope:receipt.scope,pages:captures.map(c=>c.page),process_id:child.pid,sha256:receipt.exe_sha256}));
}finally{
  if(child){
    const reply=await run(exe,['--app-status'],{env}).then(r=>JSON.parse(r.stdout.trim())).catch(()=>null);
    if(reply?.process_id===child.pid)await run(exe,['--quit'],{env});
    await Promise.race([new Promise(resolve=>child.once('exit',resolve)),pause(3000)]);
  }
  await new Promise(resolve=>server.close(resolve));
}
