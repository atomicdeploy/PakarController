// Probe/HTTP fixtures only, not an ESP, W5500, multicast or Internet test.
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {deviceOrigin,ethernetReady,deviceRequest,checkEthernet} from '../tools/check-ethernet-live.mjs';
const status={hostname:'ledboard',board_id:'esp32s3-fixture',boot_id:'a'.repeat(32),http_peer_fencing:true,
  uplink:'ethernet',uplink_ipv4_ready:true,station_connected:false,programming_latch:false,
  capabilities:{board_kind:'controller',hostname:'ledboard',acknowledged_features:['network']},ethernet:{configured:true,initialized:true,link_up:true,ipv4_ready:true,ip:'192.168.50.20'}};
assert.equal(deviceOrigin('http://ledboard.local').hostname,'ledboard.local');
for(const origin of ['http://192.168.50.20','http://user:secret@ledboard.local','http://ledboard.local/api','http://ledboard.local:0','ftp://ledboard.local'])assert.throws(()=>deviceOrigin(origin));
const peer=ethernetReady(status,'::ffff:192.168.50.20');
assert.throws(()=>ethernetReady(status,'192.168.4.1'));
assert.throws(()=>ethernetReady({...status,boot_id:'b'.repeat(32)},status.ethernet.ip,peer));
for(const change of [{uplink:'wifi'},{uplink_ipv4_ready:false},{ethernet:{...status.ethernet,link_up:false}},
  {ethernet:{...status.ethernet,ip:'127.0.0.1'}}])assert.throws(()=>ethernetReady({...status,...change},status.ethernet.ip));
await assert.rejects(checkEthernet({origin:'http://ledboard.local'}),/--run/);

for(const scenario of ['success','local-only','wrong-interface','wifi-egress','busy-job','bad-range','lost-fetch','wrong-image','reboot']){
  const observed=structuredClone(status),calls=[];let dispatched=false;
  if(scenario==='wifi-egress')observed.station_connected=true;
  const hashes={manifest:'1'.repeat(64),image:'2'.repeat(64)};
  const request=async(_origin,path,options={})=>{
    calls.push({path,options});
    if(options.form){assert.equal(path,'/api/firmware/fetch');assert.equal(options.form.targets,'1');assert.equal(options.form.apply,'false');
      assert.equal(options.headers['X-Pakar-Board'],status.board_id);assert.equal(options.headers['X-Pakar-Boot'],status.boot_id);
      dispatched=true;if(scenario==='lost-fetch')throw Error('lost reply');
    }
    const response={status:200,headers:{},remote:scenario==='wrong-interface'?'192.168.4.1':status.ethernet.ip,body:Buffer.alloc(0)};
    const json=value=>({...response,body:Buffer.from(JSON.stringify(value))});
    if(path==='/api/status')return json({...observed,boot_id:scenario==='reboot'&&dispatched?'b'.repeat(32):observed.boot_id});
    if(path==='/')return {...response,body:Buffer.from('<!doctype html><html>fixture</html>')};
    if(path==='/description.xml')return {...response,body:Buffer.from('<device><UDN>uuid:fixture</UDN></device>')};
    if(path==='/api/network/ethernet')return json({configured:true,dhcp:true});
    if(path==='/firmware/download'){
      if(options.method==='HEAD')return {...response,headers:{'content-length':'1024'}};
      const bytes=Buffer.alloc(256);bytes[0]=0xe9;
      return {...response,status:scenario==='bad-range'?200:206,headers:{'content-range':'bytes 0-255/1024'},body:bytes};
    }
    if(path==='/api/firmware/remote')return json(dispatched?{job_id:7,boot_id:status.boot_id,manifest_sha256:hashes.manifest,
      phase:'ready',targets:1,busy:false,esp:{size:1024,sha256:scenario==='wrong-image'?'3'.repeat(64):hashes.image}}:
      {phase:scenario==='busy-job'?'ready':'idle',busy:false});
    if(path==='/api/firmware/fetch')return json({job_id:7,boot_id:status.boot_id});
    throw Error('Unexpected path');
  };
  const promise=checkEthernet({origin:'http://ledboard.local',run:true,request,wait:async()=>{},
    ...(scenario==='local-only'?{}:{manifest:'https://updates.example.test/manifest.json',manifestSha:hashes.manifest,imageSha:hashes.image})});
  if(scenario==='success'||scenario==='local-only'){
    const result=await promise;assert(result.ok&&result.simulated&&!result.firmware_applied&&!result.avr_touched);
    assert.match(result.websocket,/separate/);assert.equal(typeof result.remote_fetch,scenario==='success'?'object':'string');
  }else await assert.rejects(promise);
  assert(calls.filter(c=>c.options.form).length<=1,'Never retry a staging mutation');
  assert(!calls.some(c=>/apply|cancel|avr\//.test(c.path)),'Never apply, cancel or touch AVR automatically');
  if(['wrong-interface','wifi-egress','busy-job','bad-range'].includes(scenario))assert(!dispatched);
}

// Real loopback HTTP demonstrates socket-peer capture and no redirect replay.
const paths=[];const server=createServer((req,res)=>{paths.push(req.url);if(req.url.startsWith('/drop?')){req.socket.destroy();return;}res.writeHead(req.url==='/redirect'?302:200,
  req.url==='/redirect'?{Location:'/unexpected'}:{'Content-Type':'application/json'});res.end('{}')});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
try{
  const origin=new URL('http://localhost:'+server.address().port);
  const result=await deviceRequest(origin,'/status');assert.equal(result.remote,'127.0.0.1');assert.equal(result.body.toString(),'{}');
  assert.equal((await deviceRequest(origin,'/redirect')).status,302);assert(!paths.includes('/unexpected'));
  await assert.rejects(deviceRequest(origin,'/drop?secret=private'),error=>
    error.message==='Device request GET /drop failed (ECONNRESET); no retry' &&
    error.cause?.code==='ECONNRESET' && !error.message.includes('private'));
}finally{await new Promise(resolve=>server.close(resolve))}
console.log('Ethernet live-probe safety/identity/interface/HEAD-Range/staging tests passed using fixtures and loopback; no board or WAN acceptance.');
