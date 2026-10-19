// Serve the actual embedded HTML against a visibly labeled, read-only local peer.
// No board connection, copied package, authentication secret or disk log.
import http from 'node:http';
import {createHash} from 'node:crypto';
import {readFile} from 'node:fs/promises';
import {toolRelease,toolPolicy} from '../tools/peer-identity.mjs';
const webSourceUrl=new URL('../firmware/esp8266/src/WebUi.h',import.meta.url);
const status={hostname:'SIMULATED WEB FIXTURE — NOT A BOARD',canonical_url:'loopback visual test only',version:'0.2.8',
  mode:'cell',brightness:255,effect:'none',pixels:480,avr_online:false,tm1637_ack:false,tm1637_segments:[6,91,79,102],
  tm1637_host_active:false,link_mode:'busy-wire',websocket_clients:0,station_enabled:false,station_connected:false,
  ap_enabled:false,ap_started:false,mdns_started:false,ssdp_started:false,netbios_started:false,ntp_synced:false,
  esp_uart_errors:0,avr_uart_errors:0,usb_serial_errors:0,mqtt_enabled:false,cloud_websocket_enabled:false,
  active_outputs:0,audio_cues:[],cells:Array.from({length:80},(_,i)=>i===11?'red':i===25?'off':'white')};
const values={
  '/api/auth/info':{ready:true,enabled:false,username:'fixture',default_credentials:false},
  '/api/status':status,
  '/api/cells/mapping':{physical:Array.from({length:80},(_,i)=>i+1),names:Array(80).fill('')},
  '/api/device/name':{hostname:'simulated-web-fixture'},
  '/api/network':{station_enabled:false,ap_enabled:true,station_ssid:'Fixture only'},
  '/api/cloud':{mqtt_enabled:false,mqtt_tls:true,mqtt_host:'mqtt.digitalogic.ir',mqtt_port:8883,mqtt_user:'fixture',mqtt_base_topic:'fixture',mqtt_password_set:false,websocket_enabled:false,websocket_tls:true,websocket_host:'',websocket_port:443,websocket_path:'/',websocket_token_set:false,telemetry_seconds:30},
  '/api/firmware/remote':{job_id:0,boot_id:'SIMULATED',targets:3,manifest_sha256:'',phase:'idle',busy:false,cancellable:false},
};
const contextProfile=process.argv.includes('--controller')?'controller':process.argv.includes('--unacknowledged')?'unknown':'table';
const icon=(await readFile(new URL('../assets/branding/app-icon.png.base64',import.meta.url),'utf8')).trim();
const application={id:'simulated.'+contextProfile,name:contextProfile==='table'?'Pakar Table':'Pakar Keys',title:contextProfile==='table'?'Table controller':'Keypad controller',icon_png_base64:icon};
values['/api/device/name']={hostname:'simulated-web-fixture',display_name:'SIMULATED '+contextProfile.toUpperCase()+' — NOT A BOARD',application,persisted:true,schema:1};
if(contextProfile!=='unknown')status.capabilities={name:'SIMULATED '+contextProfile.toUpperCase()+' — NOT A BOARD',board_kind:contextProfile,
  hostname:'simulated-web-fixture',application,
  acknowledged_features:contextProfile==='table'?['network','telemetry','identity','table','leds','streaming']:['network','telemetry','identity','buzzer'],
  ...(contextProfile==='table'?{table:{columns:10,rows:8,pixels_per_cell:6},modes:[{id:'cell',code:1,label:'Cell selection',kind:'presentation'},{id:'snake',code:3,label:'Snake',kind:'presentation'}]}:{})};
if(contextProfile==='controller'){status.avr_online=true;status.audio_cues=['welcome'];}
// Extend the existing no-write visual harness, rather than a second package or
// inferred live-board snapshot. Physical cell 80 is deliberately remapped to 1.
const pixelFixture=process.argv.includes('--pixels');
const diagnosticFixture=process.argv.includes('--diagnostic');
if(diagnosticFixture){
  const {enforce,...range}=toolPolicy;
  Object.assign(status,{board_id:'simulated-pixel-diagnostics',boot_id:'a'.repeat(32),http_peer_fencing:true,
    command_clock_ms:1000,compatibility:{...range,enforced:enforce},version:toolRelease.version,
    led_output:{enabled:true,mode:'effects',diagnostic:{revision:1,active:true,index:474,count:6,rgb:0x1234ab,off:false}}});
  status.capabilities.acknowledged_features.push('command-ack','pixel-diagnostic');
}
const toastFixture=process.argv.includes('--toasts');
if(pixelFixture){values['/api/cells/mapping'].physical[0]=80;values['/api/cells/mapping'].physical[79]=1;}
const server=http.createServer(async(req,res)=>{
  res.setHeader('Cache-Control','no-store');
  if(req.method==='GET'&&req.url==='/assets/fonts/Vazirmatn-Regular.ttf'){res.writeHead(200,{'Content-Type':'font/ttf'});res.end(await readFile(new URL('../assets/fonts/Vazirmatn-Regular.ttf',import.meta.url)));return;}
  if(req.method!=='GET'){res.writeHead(405,{'Content-Type':'application/json'});res.end(JSON.stringify({error:'Read-only SIMULATED fixture; no device mutation'}));return;}
  if(req.url==='/'){const source=await readFile(webSourceUrl,'utf8');let html=source.match(/R"HTML\(([\s\S]*)\)HTML";/)[1];if(toastFixture)html=html.replace('</body>','<button type="button" id="fixtureToast" style="position:fixed;top:4px;left:4px;z-index:101" onclick="webToasts??=createToastController({root:notice,translate:uiTr});webToasts.showToast(\'info\',\'SIMULATED notification\',\'Read-only fixture — not a device event\',60000);webToasts.showToast(\'success\',\'SIMULATED success\',\'No board command was sent\',60000);webToasts.showToast(\'error\',\'SIMULATED error\',\'Keyboard Escape dismisses the focused notification\',60000)">SIMULATED · Test notifications</button></body>');res.writeHead(200,{'Content-Type':'text/html;charset=utf-8'});res.end(html);return;}
  const url=new URL(req.url,'http://localhost'),value=values[url.pathname];
  const response=pixelFixture&&url.pathname==='/api/status'&&url.searchParams.get('pixels')==='1'?{...status,pixel_preview_requested:true,pixel_frame:{revision:42,brightness:255,physical:values['/api/cells/mapping'].physical,rgb:['1234ab','ff0000','00ff00','0000ff','ffffff','000000'].join('').repeat(80)}}:value;
  res.writeHead(response?200:404,{'Content-Type':'application/json'});res.end(JSON.stringify(response||{error:'Fixture has no such endpoint'}));
});
server.on('upgrade',(req,socket)=>{
  if(!diagnosticFixture || req.url!=='/ws' || !req.headers['sec-websocket-key']){socket.destroy();return;}
  const accept=createHash('sha1').update(req.headers['sec-websocket-key']+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest('base64');
  socket.write('HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: '+accept+'\r\nSec-WebSocket-Protocol: arduino\r\n\r\n');
  // Read-only simulated pushes. Never execute any received command or forward
  // client data to hardware; HTTP mutations remain405 above.
  const push=()=>{const payload=Buffer.from(JSON.stringify(status)),header=Buffer.alloc(4);
    header[0]=0x81;header[1]=126;header.writeUInt16BE(payload.length,2);socket.write(Buffer.concat([header,payload]));};
  socket.on('error',()=>socket.destroy());
  socket.on('data',bytes=>{if((bytes[0]&15)===8)socket.end();});
  const timer=setInterval(()=>{if(!socket.destroyed)push();},1000);
  socket.once('close',()=>clearInterval(timer));push();
});
server.listen(0,'127.0.0.1',()=>console.log(JSON.stringify({scope:'SIMULATED read-only actual Web UI, not hardware acceptance',url:'http://localhost:'+server.address().port+'/',pid:process.pid})));
process.on('SIGINT',()=>server.close(()=>process.exit(0)));
