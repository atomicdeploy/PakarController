// Serve the actual embedded HTML against a visibly labeled, read-only local peer.
// No board connection, copied package, authentication secret or disk log.
import http from 'node:http';
import {readFile} from 'node:fs/promises';
const webSourceUrl=new URL('../firmware/esp8266/src/WebUi.h',import.meta.url);
const status={hostname:'SIMULATED WEB FIXTURE — NOT A BOARD',canonical_url:'loopback visual test only',version:'FIXTURE ONLY',
  mode:'cell',brightness:255,effect:'none',pixels:480,avr_online:false,tm1637_ack:false,tm1637_segments:[6,91,79,102],
  tm1637_host_active:false,link_mode:'busy-wire',websocket_clients:0,station_enabled:false,station_connected:false,
  ap_enabled:false,ap_started:false,mdns_started:false,ssdp_started:false,netbios_started:false,ntp_synced:false,
  esp_uart_errors:0,avr_uart_errors:0,usb_serial_errors:0,mqtt_enabled:false,cloud_websocket_enabled:false,
  active_outputs:0,audio_cues:[],cells:Array.from({length:80},(_,i)=>i===11?'red':i===25?'off':'white')};
const values={
  '/api/v1/auth/info':{ready:true,username:'fixture',default_credentials:false},
  '/api/v1/status':status,
  '/api/v1/cells/mapping':{physical:Array.from({length:80},(_,i)=>i+1),names:Array(80).fill('')},
  '/api/v1/device/name':{hostname:'simulated-web-fixture'},
  '/api/v1/cloud':{mqtt_enabled:false,mqtt_tls:true,mqtt_host:'',mqtt_port:8883,mqtt_user:'',mqtt_base_topic:'',websocket_enabled:false,websocket_tls:true,websocket_host:'',websocket_port:443,websocket_path:'/',telemetry_seconds:30},
  '/api/v1/firmware/remote':{schema:1,job_id:0,boot_id:'SIMULATED',targets:3,manifest_sha256:'',phase:'idle',busy:false,cancellable:false},
};
const server=http.createServer(async(req,res)=>{
  res.setHeader('Cache-Control','no-store');
  if(req.method!=='GET'){res.writeHead(405,{'Content-Type':'application/json'});res.end(JSON.stringify({error:'Read-only SIMULATED fixture; no device mutation'}));return;}
  if(req.url==='/'){const source=await readFile(webSourceUrl,'utf8');res.writeHead(200,{'Content-Type':'text/html;charset=utf-8'});res.end(source.match(/R"HTML\(([\s\S]*)\)HTML";/)[1]);return;}
  const value=values[req.url];res.writeHead(value?200:404,{'Content-Type':'application/json'});res.end(JSON.stringify(value||{error:'Fixture has no such endpoint'}));
});
server.on('upgrade',(_req,socket)=>socket.destroy());
server.listen(0,'127.0.0.1',()=>console.log(JSON.stringify({scope:'SIMULATED read-only actual Web UI, not hardware acceptance',url:'http://localhost:'+server.address().port+'/',pid:process.pid})));
process.on('SIGINT',()=>server.close(()=>process.exit(0)));
