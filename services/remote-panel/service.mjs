import http from 'node:http';
import {readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash,randomBytes,timingSafeEqual} from 'node:crypto';
import mqtt from 'mqtt';
import {WebSocketServer} from 'ws';
import {Gateway,commandFor,failure} from './gateway.mjs';
import {toolRelease,toolPolicy,peerVersionAllowed} from '../../tools/peer-identity.mjs';

const digest=v=>createHash('sha256').update(v).digest();
const equal=(a,b)=>a.length===b.length&&timingSafeEqual(a,b);
export function createPanel({config,client,html,basic,font,release,now=Date.now,timeout}){
  const origin=new URL(config.origin);
  if(origin.protocol!=='https:'&&origin.hostname!=='127.0.0.1')throw Error('HTTPS origin required');
  if(!/^[a-zA-Z0-9_.-]+(?:\/[a-zA-Z0-9_.-]+)*$/.test(config.topic)||!/^[a-z0-9_-]{1,64}$/.test(config.board))throw Error('Concrete board/topic required');
  if(!/^[a-f0-9]{64}$/.test(config.password_sha256)||!config.username)throw Error('Private panel credentials required');
  const gateway=new Gateway({client,topic:config.topic,board:config.board,release,now,timeout});
  const sessions=new Map(),failures=new Map(),reports=new Map();
  const expected=Buffer.from(config.password_sha256,'hex');
  const headers=res=>{res.setHeader('Server','PakarController-RemotePanel/'+toolRelease.version);res.setHeader('X-Pakar-Application','pakarcontroller-remote-panel');res.setHeader('X-Pakar-Version',toolRelease.version);res.setHeader('X-Pakar-Web-Version',release.version);res.setHeader('X-Pakar-Transport','mqtt');res.setHeader('Cache-Control','no-store');res.setHeader('X-Content-Type-Options','nosniff');res.setHeader('Referrer-Policy','no-referrer');res.setHeader('X-Frame-Options','DENY');if(gateway.status){res.setHeader('X-Pakar-Board',gateway.status.board_id);res.setHeader('X-Pakar-Boot',gateway.status.boot_id);res.setHeader('X-Pakar-Board-Version',gateway.status.version)}};
  const json=(res,status,value)=>{headers(res);res.writeHead(status,{'Content-Type':'application/json;charset=utf-8'});res.end(JSON.stringify(value))};
  function auth(req,res){
    const at=now();for(const [key,expiry]of sessions)if(expiry<=at)sessions.delete(key);
    const token=req.headers.cookie?.match(/(?:^|;\s*)pakar_remote_session=([a-f0-9]{64})(?:;|$)/)?.[1];
    if(token&&sessions.get(token)>at)return true;
    const encoded=req.headers.authorization?.match(/^Basic ([A-Za-z0-9+/=]{1,512})$/)?.[1];
    if(!encoded)return false;
    const prior=failures.get(req.socket.remoteAddress);if(prior&&at-prior.at<60000&&prior.count>=12)return false;
    const text=Buffer.from(encoded,'base64').toString('utf8'),colon=text.indexOf(':');
    if(colon<0||!equal(digest(text.slice(0,colon)),digest(config.username))||!equal(digest(text.slice(colon+1)),expected)){
      if(failures.size>128)failures.clear();failures.set(req.socket.remoteAddress,{at,count:prior&&at-prior.at<60000?prior.count+1:1});return false;
    }
    if(res){if(sessions.size>=16)sessions.delete(sessions.keys().next().value);const next=randomBytes(32).toString('hex');sessions.set(next,at+8*3600000);res.setHeader('Set-Cookie','pakar_remote_session='+next+'; Path=/; HttpOnly; Secure; SameSite=Strict; Max-Age=28800')}
    return true;
  }
  const challenge=res=>{headers(res);res.writeHead(401,{'WWW-Authenticate':'Basic realm="PakarController remote panel", charset="UTF-8"','Content-Type':'text/plain;charset=utf-8'});res.end('Sign in to the private remote panel. Device/broker credentials are not used here.')};
  const clientAllowed=req=>{
    if(config.enforce_peer_versions===false||toolPolicy.enforce===false)return true;
    const policy=gateway.status?.compatibility;
    if(policy?.enforced===false)return true;
    // Same-origin browsers run the exact served Web package. Native callers
    // identify themselves; Origin absence must not be mistaken for a browser.
    const version=req.headers['x-pakar-version']??(req.headers.origin===origin.origin?release.version:undefined);
    return peerVersionAllowed(version,policy?.minimum_app??toolPolicy.minimum_app,policy?.maximum_app??toolPolicy.maximum_app);
  };
  async function body(req){const chunks=[];let size=0;for await(const chunk of req){size+=chunk.length;if(size>8192)throw failure(413,'request_too_large');chunks.push(chunk)}return Buffer.concat(chunks).toString('utf8')}
  const server=http.createServer(async(req,res)=>{
    try{
      if(!auth(req,res)){challenge(res);return}
      if(!req.url||req.url.length>2048)throw failure(400,'invalid_path');
      const url=new URL(req.url,config.origin),path=url.pathname;
      if(req.method==='GET'&&(path==='/'||path==='/basic')){headers(res);res.writeHead(200,{'Content-Type':'text/html;charset=utf-8'});res.end(path==='/basic'?basic:html);return}
      if(req.method==='GET'&&path==='/assets/fonts/Vazirmatn-Regular.ttf'){headers(res);res.writeHead(200,{'Content-Type':'font/ttf'});res.end(font);return}
      if(req.method==='GET'&&path==='/api/auth/info'){json(res,200,{ready:true,enabled:false,default_credentials:false,username:'',remote_panel_authenticated:true});return}
      if(req.method==='GET'&&path==='/api/status'){json(res,200,await gateway.read());return}
      if(req.method==='GET'&&path==='/api/events'){json(res,200,gateway.eventPage(url.searchParams.has('after')?url.searchParams.get('after'):undefined));return}
      if(req.method==='GET'&&path==='/api/cells/assignments'){const s=await gateway.read();if(s.cells?.length!==80||s.cells.some(v=>!['red','white','off'].includes(v)))throw failure(503,'cell_states_unavailable');json(res,200,{board_id:s.board_id,boot_id:s.boot_id,red_lock:s.red_lock,assignments:s.cells.map((state,i)=>({cell:i+1,state}))});return}
      if(req.method==='GET'&&path==='/api/device/name'){const s=await gateway.read();json(res,200,{hostname:s.hostname,display_name:s.capabilities.name,application:s.capabilities.application});return}
      if(req.method==='GET'&&path==='/api/cells/mapping'){
        // MQTT has no physical-map read operation. Never manufacture a default
        // physical mapping or claim a sampled RGB frame that was not received.
        const s=await gateway.read();json(res,200,{board_id:s.board_id,boot_id:s.boot_id,available:false,physical:[],names:[]});return;
      }
      if(req.method==='GET'&&path==='/api/firmware/remote'){const s=await gateway.read();json(res,200,s.remote_firmware??{});return}
      if(req.method==='GET'&&path==='/api/telemetry'){const s=await gateway.read();if(!gateway.telemetry||gateway.telemetry.boot_id!==s.boot_id||now()-gateway.telemetryAt<0||now()-gateway.telemetryAt>=8000)throw failure(503,'complete_telemetry_unavailable');json(res,200,gateway.telemetry);return}
      if(req.method==='GET'&&path==='/api/cloud'){throw failure(501,'cloud_settings_unavailable_over_this_relay')}
      if(req.method==='POST'){
        if(req.headers.origin!==undefined&&req.headers.origin!==origin.origin)throw failure(403,'foreign_origin');
        if(req.headers.origin===undefined&&!req.headers.authorization)throw failure(403,'explicit_origin_required');
        if(!clientAllowed(req))throw failure(426,'unsupported_app_version');
        const text=await body(req);
        if(path==='/api/command'){
          let packet;try{packet=JSON.parse(req.headers['content-type']?.includes('application/json')?text:new URLSearchParams(text).get('packet')??'null')}catch{throw failure(400,'invalid_command')}
          const reply=await gateway.commandPacket(packet);json(res,200,reply);return;
        }
        if(path==='/api/clients/report'){
          const p=JSON.parse(req.headers['content-type']?.includes('application/json')?text:new URLSearchParams(text).get('report')??'null');if(typeof p?.id!=='string'||p.id.length>80)throw failure(400,'invalid_report');
          // Indirect reports remain relay-local; never call them MCU-observed.
          if(reports.size>=8&&!reports.has(p.id))reports.delete(reports.keys().next().value);
          reports.set(p.id,{application:String(p.application??'').slice(0,64),version:String(p.version??'').slice(0,64),at:now(),indirect:true});json(res,200,{accepted:true,indirect:true,scope:'relay-only'});return;
        }
        let fields;
        if(req.headers['content-type']?.includes('application/json')){let p;try{p=JSON.parse(text)}catch{throw failure(400,'invalid_json')}if(!p||Array.isArray(p)||typeof p!=='object')throw failure(400,'invalid_json');fields=Object.entries(p).map(([k,v])=>[k,typeof v==='object'?JSON.stringify(v):String(v)]);}else fields=new URLSearchParams(text);
        const form=new URLSearchParams(url.search);for(const [key,value]of fields){if(form.has(key))throw failure(400,'duplicate_parameter');form.append(key,value)}
        const [command,values,count]=commandFor(path,form),pin={board:req.headers['x-pakar-board'],boot:req.headers['x-pakar-boot']};
        const reply=await gateway.command(command,values,pin);
        let result;try{result=JSON.parse(reply.message)}catch{}
        if(path==='/api/led/output'){if(!result||typeof result.enabled!=='boolean')throw failure(502,'invalid_board_ack');json(res,200,result);return}
        json(res,200,{accepted:true,request_id:reply.request_id,...(count===undefined?{}:{count}),receipt:reply.message});return;
      }
      throw failure(404,'route_unavailable_over_mqtt');
    }catch(error){if(!res.headersSent)json(res,error.status??500,{error:error.code??'remote_panel_failed',outcome_known:error.outcome_known??(Number.isInteger(error.status)&&error.status<500&&!String(error.code).includes('outcome_unknown'))});else res.destroy()}
  });
  server.headersTimeout=15000;server.requestTimeout=15000;server.maxRequestsPerSocket=500;
  const ws=new WebSocketServer({noServer:true,maxPayload:4096,perMessageDeflate:false});
  server.on('upgrade',(req,socket,head)=>{
    const allowedOrigin=req.headers.origin===origin.origin||(req.headers.origin===undefined&&Boolean(req.headers.authorization));
    if(new URL(req.url,config.origin).pathname!=='/ws'||!allowedOrigin||!clientAllowed(req)||!auth(req)||ws.clients.size>=8){socket.end('HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n');return}
    ws.handleUpgrade(req,socket,head,connection=>ws.emit('connection',connection,req));
  });
  ws.on('connection',connection=>{
    let inFlight=0;
    const send=value=>{if(connection.bufferedAmount>65536){connection.close(1013,'Slow client');return}if(connection.readyState===1)connection.send(JSON.stringify(value))};
    if(gateway.fresh())connection.send(JSON.stringify(gateway.project()));
    connection.on('error',()=>{});
    connection.on('message',async(bytes,binary)=>{
      if(binary){connection.close(1003,'Pixel streaming unavailable over this relay');return}
      let p;try{p=JSON.parse(bytes.toString())}catch{connection.close(1008,'Invalid message');return}
      if(!p||Array.isArray(p)||typeof p!=='object'){connection.close(1008,'Invalid message');return}
      if(inFlight>=4){send({type:'command-result',request_id:p.request_id,accepted:false,error:'command_capacity',outcome_known:true});return}
      ++inFlight;
      try{
        if(p.type==='status')send(await gateway.read());
        else if(p.type==='ping')send({type:'pong'});
        else if(p.type==='events')send({type:'events',...gateway.eventPage(p.after)});
        else if(p.type==='command')send(await gateway.commandPacket(p));
        else send({type:'error',error:'unsupported_message'});
      }catch(error){send({type:'command-result',request_id:p.request_id,board_id:p.board_id,boot_id:p.boot_id,accepted:false,error:error.code??'relay_failed',outcome_known:error.outcome_known??(Number.isInteger(error.status)&&error.status<500&&!String(error.code).includes('outcome_unknown'))})}
      finally{--inFlight}
    });
  });
  gateway.on('status',s=>{const bytes=JSON.stringify(s);for(const connection of ws.clients){if(connection.bufferedAmount>65536){connection.close(1013,'Slow client');continue}if(connection.readyState===1)connection.send(bytes)}});
  gateway.on('event',event=>{const bytes=JSON.stringify(event);for(const connection of ws.clients){if(connection.bufferedAmount>65536){connection.close(1013,'Slow client');continue}if(connection.readyState===1)connection.send(bytes)}});
  gateway.on('telemetry',event=>{const bytes=JSON.stringify(event);for(const connection of ws.clients){if(connection.bufferedAmount>65536){connection.close(1013,'Slow client');continue}if(connection.readyState===1)connection.send(bytes)}});
  gateway.on('offline',()=>{for(const c of ws.clients)c.close(1013,'Board connection lost')});
  const stale=setInterval(()=>{if(!gateway.fresh())for(const c of ws.clients)c.close(1013,'Fresh board status unavailable')},2000);stale.unref();
  async function close(){clearInterval(stale);for(const c of ws.clients)c.terminate();await new Promise(resolve=>ws.close(resolve));await new Promise(resolve=>server.close(resolve));gateway.disconnect();client.end(true)}
  return {server,gateway,close};
}

if(process.argv[1]&&resolve(process.argv[1])===fileURLToPath(import.meta.url)){
  const config=JSON.parse(readFileSync(process.env.PAKAR_REMOTE_CONFIG??'/etc/pakarcontroller-remote-panel/config.json','utf8'));
  const root=resolve(fileURLToPath(new URL('../../',import.meta.url))),ui=resolve(config.ui_directory??root+'/firmware/esp8266/src');
  const header=readFileSync(ui+'/WebUi.h','utf8'),html=header.match(/R"HTML\(([\s\S]*)\)HTML";/)?.[1];
  if(!html)throw Error('Actual project Web UI missing');
  const release=JSON.parse(html.match(/const PAKAR_WEB_BUILD = (\{[^\n]+\});/)?.[1]??'null');
  if(!release?.version||!release?.minimum_board||release.enforcePeerVersions!==true)throw Error('Explicit supported-release Web identity required');
  const control=JSON.parse(readFileSync(config.mqtt_file,'utf8'));
  const client=mqtt.connect('mqtts://'+control.host+':'+control.port,{username:control.username,password:control.password,
    clientId:'pakar-panel-'+randomBytes(8).toString('hex'),clean:true,queueQoSZero:false,reconnectPeriod:2000,resubscribe:false,connectTimeout:8000,rejectUnauthorized:true});
  const basicSource=readFileSync(ui+'/basic.html','utf8').replace('{{PEER_HTTP}}',readFileSync(ui+'/peer-http.js','utf8')).replace('{{BASIC_CELLS}}',readFileSync(ui+'/basic-cells.js','utf8'));
  const panel=createPanel({config,client,html,basic:basicSource,font:readFileSync(root+'/assets/fonts/Vazirmatn-Regular.ttf'),release});
  panel.server.listen(config.port??18744,'127.0.0.1',()=>console.log(JSON.stringify({service:'pakarcontroller-remote-panel',version:toolRelease.version,web_version:release.version,bind:'127.0.0.1',port:config.port??18744,board:config.board,transport:'mqtt',authenticated:true})));
  process.on('SIGTERM',()=>panel.close().then(()=>process.exit(0)));
}
