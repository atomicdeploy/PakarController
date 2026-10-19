import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const read=p=>readFileSync(new URL('../'+p,import.meta.url),'utf8');
const catalog=JSON.parse(read('assets/contracts/device-settings.json'));
const source=read('assets/web/device-settings.js'),web=read('firmware/esp8266/src/WebUi.h');
assert(web.includes(source.trim()),'Production uses the actual shared settings module');
assert(!web.includes('<span>UART mode</span>'));assert(!web.includes("s.link_mode||'polling'"));
const native=read('desktop/src/ui/device_settings.rs');assert(native.includes('include!("settings_catalog.rs")'));
const rust=read('desktop/src/ui/settings_catalog.rs'),dart=read('mobile/lib/settings_catalog.dart');
for(const page of catalog.pages){assert(rust.includes(JSON.stringify(page.id)));assert(rust.includes(JSON.stringify(page.label)));assert(dart.includes(JSON.stringify(page.id)));}
for(const group of ['network','cloud']){const name=group==='network'?'Network':'Cloud',source=read('desktop/src/device_settings.rs'),body=source.split('pub struct '+name+' {')[1].split('}')[0];let fields=[...body.matchAll(/pub (\w+):/g)].map(m=>m[1]);if(group==='network'){assert(body.includes('#[serde(flatten)]'));assert(fields.includes('addressing'));const addressing=source.split('pub struct Addressing {')[1].split('}')[0];fields=fields.filter(v=>v!=='addressing').concat([...addressing.matchAll(/pub (\w+):/g)].map(m=>m[1]));}assert.deepEqual(fields.sort(),Object.keys(catalog[group]).sort(),'Native and Web field contract cannot drift');}
const nodes={};const element=()=>({value:'',checked:false,disabled:false,textContent:'',className:'',open:false,close(){this.open=false},querySelectorAll(){return[]}});
for(const id of ['settingsMessage','networkSave','cloudSave','settingsReload','wifiSsid','stationEnabled','apEnabled','wifiPassword','wifiOpen','mqttEnabled','mqttTls','mqttHost','mqttPort','mqttUser','mqttTopic','cloudWsEnabled','cloudWsTls','cloudWsHost','cloudWsPort','cloudWsPath','telemetrySeconds','mqttSecretMode','websocketSecretMode','mqttPassword','cloudWsToken','deviceSettingsDialog','wsState'])nodes[id]=element();
for(const id of ['statusRgbSettings','rgbEnabled','rgbPin','rgbConfirmed','rgbBrightness','rgbRead','rgbSave'])nodes[id]=element();
nodes.mqttSecretMode.value=nodes.websocketSecretMode.value='keep';
const network={station_enabled:true,ap_enabled:true,station_ssid:'DRS Tether'};
const cloud={mqtt_enabled:false,mqtt_tls:true,mqtt_host:'mqtt.digitalogic.ir',mqtt_port:8883,mqtt_user:'pakar',mqtt_base_topic:'pakar',mqtt_password_set:true,websocket_enabled:false,websocket_tls:true,websocket_host:'',websocket_port:443,websocket_path:'/',websocket_token_set:false,telemetry_seconds:30};
let saved={network:{...network},cloud:{...cloud}},calls=[],behavior='success';
const fixturePeer=Object.freeze({board:'fixture',boot:'a'.repeat(32),generation:1});
const context=vm.createContext({peerHttp:{pin:()=>fixturePeer,writable:()=>true},DEVICE_SETTINGS:catalog,TextEncoder,URLSearchParams,console,localStorage:{getItem(){return null}},document:{querySelector:s=>nodes[s.slice(1)]},currentPeer:()=>({known:true,has:f=>f==='network'}),confirm:()=>true,
 api:async(path,options={})=>{calls.push({path,options});const group=path.split('/').at(-1);if(options.method==='POST'){assert(nodes.mqttHost.disabled,'Inputs must freeze during a write');if(behavior==='uncertain')throw Error('timeout');if(behavior==='negative')return{updated:false};const next={...saved[group]};for(const[k,type]of Object.entries(catalog[group])){if(k.endsWith('_set'))continue;const formKey=k==='station_ssid'?'ssid':k;if(options.body.has(formKey)){const value=options.body.get(formKey);next[k]=type==='boolean'?value==='true':type==='port'||type==='interval'?Number(value):value}}for(const[k,p]of [['mqtt_password','mqtt_password_set'],['websocket_token','websocket_token_set']])if(options.body.has(k))next[p]=!!options.body.get(k);if(behavior!=='mismatch')saved[group]=next;return group==='network'?{accepted:true}:{updated:true}}return {...saved[group]};}
});
vm.runInContext(read('assets/web/rayanlamp.js')+'\n'+read('assets/web/localization.js'),context);
vm.runInContext(source,context);const run=s=>vm.runInContext(s,context);context.initialCloud=cloud;context.initialNetwork=network;
run("settingsObserve('network',initialNetwork);settingsObserve('cloud',initialCloud)");
assert.throws(()=>run("settingsSnapshot('cloud',{})"),/Missing/);
nodes.mqttHost.value='changed.digitalogic.ir';context.changed={...cloud,mqtt_user:'other'};run("settingsObserve('cloud',changed)");assert.equal(nodes.mqttHost.value,'changed.digitalogic.ir');assert(run('webSettings.cloud.conflict'));
run('settingsInvalidate()');run("settingsObserve('network',initialNetwork);settingsObserve('cloud',initialCloud)");
let write=run("settingsWrite('cloud')");assert(!Object.hasOwn(write.form,'mqtt_password'));assert(!Object.hasOwn(write.form,'websocket_token'));assert(!Object.hasOwn(write.form,'mqtt_password_set'));
nodes.mqttSecretMode.value='clear';nodes.websocketSecretMode.value='replace';nodes.cloudWsToken.value='new-secret';write=run("settingsWrite('cloud')");assert.equal(write.form.mqtt_password,'');assert.equal(write.expected.mqtt_password_set,false);assert.equal(write.form.websocket_token,'new-secret');assert.equal(write.expected.websocket_token_set,true);
await run("settingsSave('cloud')");assert.equal(calls.filter(c=>c.options.method==='POST').length,1);assert.match(nodes.settingsMessage.textContent,/verified/);assert.equal(nodes.cloudWsToken.value,'');assert.equal(nodes.mqttSecretMode.value,'keep');
for(const failure of ['negative','mismatch','uncertain']){behavior=failure;calls=[];nodes.mqttUser.value='edit-'+failure;await run("settingsSave('cloud')");assert.equal(calls.filter(c=>c.options.method==='POST').length,1);assert.match(nodes.settingsMessage.textContent,/not retried/);assert.equal(nodes.mqttUser.value,'edit-'+failure,'Failed saves preserve the draft');}
behavior='success';calls=[];saved.cloud={...saved.cloud,mqtt_user:'external-client'};await run("settingsSave('cloud')");assert.equal(calls.filter(c=>c.options.method==='POST').length,0);assert(run('webSettings.cloud.conflict'));
nodes.wifiPassword.value='';assert(!Object.hasOwn(run("settingsWrite('network').form"),'password'),'Blank preserves the stored secret for the unchanged SSID');assert(!Object.hasOwn(run("settingsWrite('network').form"),'dhcp'),'Older firmware addressing stays unknown, not a fabricated DHCP default');nodes.wifiSsid.value='Other WiFi';assert.throws(()=>run("settingsWrite('network')"),/password or explicitly select Open/);nodes.wifiOpen.checked=true;assert.equal(run("settingsWrite('network').form.password"),'');
for(const id of ['wifiDhcp','wifiAddress','wifiGateway','wifiSubnet','wifiDns1','wifiDns2'])nodes[id]=element();
context.addressedNetwork={...network,dhcp:false,address:'192.168.7.8',gateway:'192.168.7.1',subnet:'255.255.255.0',dns1:'0.0.0.0',dns2:'0.0.0.0'};
run("settingsInvalidate();settingsObserve('network',addressedNetwork)");
nodes.wifiAddress.value='192.168.007.009';nodes.wifiDns1.value='';
write=run("settingsWrite('network')");assert.equal(write.form.address,'192.168.7.9');assert.equal(write.form.dns1,'0.0.0.0');assert.equal(write.form.dhcp,false);
nodes.wifiDhcp.checked=true;write=run("settingsWrite('network')");assert.equal(write.form.address,'192.168.7.8','DHCP preserves the actual saved static profile, not unsent static drafts');
context.incompleteNetwork={...network,dhcp:true};assert.throws(()=>run("settingsSnapshot('network',incompleteNetwork)"),/Incomplete/);
nodes.wifiDhcp.checked=false;nodes.wifiSubnet.value='255.0.255.0';assert.throws(()=>run("settingsWrite('network')"),/Invalid static/);
vm.runInContext(web.split(/\r?\n/).find(line=>line.startsWith('function statusRgbOutput(')),context);
context.rgbStatus={status_rgb:{available:true,have_output:true,rgb:[0,255,0],brightness:32,state:'ready'}};
assert.deepEqual([...run('statusRgbOutput(rgbStatus,true).rgb')],[0,255,0],'Actual emitted RGB is not brightness-scaled again');
assert.equal(run('statusRgbOutput(rgbStatus,false)'),null);
context.rgbStatus.status_rgb.have_output=false;assert.equal(run('statusRgbOutput(rgbStatus,true)'),null);
context.rgbStatus.status_rgb.have_output=true;context.rgbStatus.status_rgb.rgb=[0,256,0];assert.equal(run('statusRgbOutput(rgbStatus,true)'),null);
// Exercise the actual shared RGB editor, including the same pending/draft fence
// as network/cloud, rather than a duplicate production implementation.
const originalApi=context.api;
context.currentPeer=()=>({known:true,has:f=>['network','status-rgb'].includes(f)});
context.peerHttp.assertOwner=peer=>assert.equal(peer,fixturePeer);
const rgbConfig={enabled:false,pin:48,pin_confirmed:true,brightness:32};
let rgbSaved={...rgbConfig},rgbCalls=[],rgbBehavior='success';
context.api=async(path,options={})=>{
 assert.equal(path,'/api/status-rgb');assert.equal(options.peer,fixturePeer);rgbCalls.push(options);
 if(options.method==='POST'){
  assert(nodes.rgbPin.disabled&&nodes.mqttHost.disabled,'All device settings freeze during RGB writes');
  if(rgbBehavior==='uncertain')throw Error('timeout');
  if(rgbBehavior==='negative')return{saved:false};
  if(rgbBehavior!=='mismatch')rgbSaved={enabled:options.body.get('enabled')==='true',pin:Number(options.body.get('pin')),pin_confirmed:options.body.get('pin_confirmed')==='true',brightness:Number(options.body.get('brightness'))};
  return{saved:true};
 }
 return{...rgbSaved,persisted:rgbBehavior!=='not-persisted'};
};
assert.throws(()=>run('statusRgbConfig({})'),/Complete valid/);
assert.throws(()=>run('statusRgbConfig({enabled:true,pin:48,pin_confirmed:false,brightness:32})'),/confirmed/);
assert.throws(()=>run('statusRgbConfig({enabled:false,pin:4,pin_confirmed:true,brightness:32})'),/confirmed/);
run("settingsInvalidate();settingsObserve('network',initialNetwork);settingsObserve('cloud',initialCloud)");
await run('statusRgbSettingsLoad()');assert.equal(nodes.rgbBrightness.value,'32');assert.equal(nodes.rgbSave.disabled,true);
nodes.rgbBrightness.value='64';run('settingsState()');assert.match(nodes.settingsMessage.textContent,/Unsaved/);
context.confirm=()=>false;assert.equal(run('settingsCanClose()'),false,'RGB draft participates in dialog-close protection');
rgbCalls=[];await run('settingsReload()');assert.equal(rgbCalls.length,0,'Reload cannot discard an RGB draft without confirmation');assert.equal(nodes.rgbBrightness.value,'64');
context.confirm=()=>true;rgbCalls=[];await run('statusRgbSettingsLoad(true)');
assert.equal(rgbCalls.filter(c=>c.method==='POST').length,1);assert.equal(rgbCalls.length,3);assert.equal(rgbSaved.brightness,64);assert.match(nodes.settingsMessage.textContent,/persisted and read back/);
assert(!nodes.mqttHost.disabled,'Completing RGB work releases the shared pending lock');assert.equal(run('statusRgbDirty()'),false);
for(const failure of ['negative','mismatch','uncertain','not-persisted']){
 rgbBehavior=failure;rgbCalls=[];nodes.rgbBrightness.value=String(Number(nodes.rgbBrightness.value)+1);
 await run('statusRgbSettingsLoad(true)');assert.equal(rgbCalls.filter(c=>c.method==='POST').length,1);assert.match(nodes.settingsMessage.textContent,/not retried/);assert(run('statusRgbDirty()'),'Failed RGB writes preserve the draft');
 rgbBehavior='success';await run('statusRgbSettingsLoad()');
}
nodes.rgbBrightness.value='99';rgbSaved={...rgbSaved,brightness:100};rgbCalls=[];await run('statusRgbSettingsLoad(true)');
assert.equal(rgbCalls.filter(c=>c.method==='POST').length,0);assert(run('webSettings.rgb.conflict'));assert.equal(nodes.rgbBrightness.value,'99');assert(nodes.rgbSave.disabled);
await run('statusRgbSettingsLoad()');assert.equal(nodes.rgbBrightness.value,'100');
let finishRgbRead;context.api=()=>new Promise(resolve=>{finishRgbRead=resolve});
const oldRgbRead=run('statusRgbSettingsLoad()');run('settingsInvalidate()');finishRgbRead({...rgbSaved,persisted:true});await oldRgbRead;
assert.equal(run('webSettings.rgb'),null,'An obsolete RGB read cannot repopulate a revoked session');
context.api=originalApi;
// Compact events merge only RGB and cannot renew whole-peer freshness or
// replace unrelated cells/settings/workspace state.
const rgbEventStart=web.indexOf('function acceptStatusRgb('),rgbEventEnd=web.indexOf('// The observed pixel',rgbEventStart);
assert(rgbEventStart>=0&&rgbEventEnd>rgbEventStart);vm.runInContext(web.slice(rgbEventStart,rgbEventEnd),context);
context.state={board_id:'fixture',boot_id:'a'.repeat(32),status_rgb:{revision:1},cells:[{state:'white'}]};
context.contextReceivedAt=42;let rgbRenders=0;context.renderStatusRgb=()=>rgbRenders++;
context.rgbEvent={type:'status-rgb',board_id:'fixture',boot_id:'a'.repeat(32),revision:2,status_rgb:{revision:2,available:true,have_output:true,rgb:[1,2,3]}};
const cellsBefore=context.state.cells;assert(run('acceptStatusRgb(rgbEvent)'));assert.equal(rgbRenders,1);assert.equal(context.state.cells,cellsBefore);assert.equal(context.contextReceivedAt,42);
assert(!run('acceptStatusRgb(rgbEvent)'),'Duplicate revisions are ignored');
context.rgbEvent={...context.rgbEvent,revision:3,status_rgb:{...context.rgbEvent.status_rgb,revision:3},boot_id:'b'.repeat(32)};assert(!run('acceptStatusRgb(rgbEvent)'));
context.rgbEvent={...context.rgbEvent,boot_id:'a'.repeat(32),status_rgb:{revision:3,rgb:[1,2,3]}};assert(!run('acceptStatusRgb(rgbEvent)'),'Missing critical output flags are ignored');
context.nextRgbStatus={board_id:'fixture',boot_id:'a'.repeat(32),capabilities:{acknowledged_features:['status-rgb']},status_rgb:{revision:1}};
assert.equal(run('retainStatusRgb(nextRgbStatus,state).status_rgb.revision'),2);
context.nextRgbStatus.boot_id='b'.repeat(32);assert.equal(run('retainStatusRgb(nextRgbStatus,state).status_rgb.revision'),1,'RGB never survives a peer boot change');
context.currentPeer=()=>({known:true,has:f=>f==='network'});
context.ethernetFixture={configured:true,enabled:true,lldp_enabled:true,lldp_receive:false,dhcp:false,address:'192.168.9.2',gateway:'192.168.9.1',subnet:'255.255.255.0',dns1:'0.0.0.0',dns2:'0.0.0.0'};
assert.equal(run('ethernetConfig(ethernetFixture).address'),'192.168.9.2');
assert(!Object.hasOwn(run('ethernetConfig(ethernetFixture)'),'configured'));
assert.throws(()=>run('ethernetConfig({...ethernetFixture,configured:false})'),/not supported/);
assert.throws(()=>run("ethernetConfig({...ethernetFixture,subnet:'255.0.255.0'})"),/Invalid static/);
assert.throws(()=>run("settingsSnapshot('network',{station_enabled:false,ap_enabled:false,station_ssid:'Fixture'})"),/Ethernet/);
assert.equal(run("settingsSnapshot('network',{station_enabled:false,ap_enabled:false,station_ssid:'Fixture',ethernet_ready:true}).station_enabled"),false);
run('settingsInvalidate()');assert.equal(nodes.cloudWsToken.value,'');assert.equal(run('webSettings.network'),null);
let finishOldRead;context.api=()=>new Promise(resolve=>{finishOldRead=resolve});
const oldRead=run("settingsLoad('cloud')");run('settingsInvalidate()');finishOldRead(cloud);await oldRead;assert.equal(run('webSettings.cloud'),null,'Revoked sessions must ignore late read replies');
assert(source.includes('const epoch=++webSettings.epoch'),'Starting a write revokes older reads as well');
assert(source.includes("webSettings.pending||['network','cloud'].some(settingsDirty)||statusRgbDirty()"),'RGB drafts participate in browser-unload protection');
assert(web.includes('.settings-field[hidden]{display:none}'),'Hidden secret rows cannot be made visible by grid styling');
let collapsed=false,panelStorage=null;context.panel={dataset:{panelId:'live-device',panelLabel:'Live device',capability:'network'},hidden:false,classList:{toggle(_name,value){collapsed=value}},querySelector(){return{setAttribute(){}}}};
context.localStorage={getItem(){return panelStorage},setItem(_key,value){panelStorage=value}};
run("workspacePanels.push(panel);panelToggle('live-device','hidden')");assert(context.panel.hidden);assert(panelStorage.includes('live-device'));
run("panelToggle('live-device','hidden');panelToggle('live-device','collapsed')");assert(!context.panel.hidden);assert(collapsed);
context.currentPeer=()=>({known:false,has:()=>false});run('applyPanelVisibility()');assert(context.panel.hidden,'Capability loss must override stored panel visibility');
console.log('Shared settings catalog, strict required fields, secrets, dirty/conflict guards, ACK/readback, no replay, and session revocation passed (actual module / HTTP doubles, not hardware).');
for(const id of ['ntfyEnabled','ntfyHost','ntfyTopic','ntfyToken','ntfySecretMode'])nodes[id]=element();
nodes.ntfySecretMode.value='keep';context.currentPeer=()=>({has:()=>true});
context.ntfyCloud={...cloud,ntfy_enabled:false,ntfy_host:'ntfy.sh',ntfy_topic:'fixture-private',ntfy_token_set:false};
run("settingsInvalidate();settingsObserve('cloud',ntfyCloud)");
assert.equal(run("settingsWrite('cloud').form.ntfy_enabled"),false);
assert(!Object.hasOwn(run("settingsWrite('cloud').form"),'ntfy_token'));
nodes.ntfyEnabled.checked=true;nodes.ntfySecretMode.value='replace';nodes.ntfyToken.value='tk_fixture';
write=run("settingsWrite('cloud')");assert.equal(write.form.ntfy_token,'tk_fixture');assert.equal(write.expected.ntfy_token_set,true);
assert(!Object.hasOwn(write.form,'ntfy_token_set'));
nodes.ntfyTopic.value='bad/path';assert.throws(()=>run("settingsWrite('cloud')"),/single topic/);
nodes.ntfyTopic.value='';assert.throws(()=>run("settingsWrite('cloud')"),/single topic/);
nodes.ntfyTopic.value='fixture-private';nodes.ntfySecretMode.value='clear';
assert.equal(run("settingsWrite('cloud').form.ntfy_token"),'');
run('settingsClearSecrets()');assert.equal(nodes.ntfyToken.value,'');assert.equal(nodes.ntfySecretMode.value,'keep');
console.log('ntfy settings: opt-in, optional support, topic validation and write-only token keep/replace/clear passed. No real publish.');
