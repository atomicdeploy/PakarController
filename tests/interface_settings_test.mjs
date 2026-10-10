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
for(const group of ['network','cloud']){const name=group==='network'?'Network':'Cloud',body=read('desktop/src/device_settings.rs').split('pub struct '+name+' {')[1].split('}')[0];assert.deepEqual([...body.matchAll(/pub (\w+):/g)].map(m=>m[1]).sort(),Object.keys(catalog[group]).sort(),'Native and Web field contract cannot drift');}
const nodes={};const element=()=>({value:'',checked:false,disabled:false,textContent:'',className:'',open:false,close(){this.open=false},querySelectorAll(){return[]}});
for(const id of ['settingsMessage','networkSave','cloudSave','settingsReload','wifiSsid','stationEnabled','apEnabled','wifiPassword','wifiOpen','mqttEnabled','mqttTls','mqttHost','mqttPort','mqttUser','mqttTopic','cloudWsEnabled','cloudWsTls','cloudWsHost','cloudWsPort','cloudWsPath','telemetrySeconds','mqttSecretMode','websocketSecretMode','mqttPassword','cloudWsToken','deviceSettingsDialog','wsState'])nodes[id]=element();
nodes.mqttSecretMode.value=nodes.websocketSecretMode.value='keep';
const network={station_enabled:true,ap_enabled:true,station_ssid:'DRS Tether'};
const cloud={mqtt_enabled:false,mqtt_tls:true,mqtt_host:'mqtt.digitalogic.ir',mqtt_port:8883,mqtt_user:'pakar',mqtt_base_topic:'pakar',mqtt_password_set:true,websocket_enabled:false,websocket_tls:true,websocket_host:'',websocket_port:443,websocket_path:'/',websocket_token_set:false,telemetry_seconds:30};
let saved={network:{...network},cloud:{...cloud}},calls=[],behavior='success';
const context=vm.createContext({DEVICE_SETTINGS:catalog,TextEncoder,URLSearchParams,console,localStorage:{getItem(){return null}},document:{querySelector:s=>nodes[s.slice(1)]},currentPeer:()=>({known:true,has:f=>f==='network'}),confirm:()=>true,
 api:async(path,options={})=>{calls.push({path,options});const group=path.split('/').at(-1);if(options.method==='POST'){assert(nodes.mqttHost.disabled,'Inputs must freeze during a write');if(behavior==='uncertain')throw Error('timeout');if(behavior==='negative')return{updated:false};const next={...saved[group]};for(const[k,type]of Object.entries(catalog[group])){if(k.endsWith('_set'))continue;const formKey=k==='station_ssid'?'ssid':k;if(options.body.has(formKey)){const value=options.body.get(formKey);next[k]=type==='boolean'?value==='true':type==='port'||type==='interval'?Number(value):value}}for(const[k,p]of [['mqtt_password','mqtt_password_set'],['websocket_token','websocket_token_set']])if(options.body.has(k))next[p]=!!options.body.get(k);if(behavior!=='mismatch')saved[group]=next;return group==='network'?{accepted:true}:{updated:true}}return {...saved[group]};}
});
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
nodes.wifiPassword.value='';assert.throws(()=>run("settingsWrite('network')"),/8–63/);nodes.wifiOpen.checked=true;assert.equal(run("settingsWrite('network').form.password"),'');
run('settingsInvalidate()');assert.equal(nodes.cloudWsToken.value,'');assert.equal(run('webSettings.network'),null);
let finishOldRead;context.api=()=>new Promise(resolve=>{finishOldRead=resolve});
const oldRead=run("settingsLoad('cloud')");run('settingsInvalidate()');finishOldRead(cloud);await oldRead;assert.equal(run('webSettings.cloud'),null,'Revoked sessions must ignore late read replies');
assert(source.includes('const epoch=++webSettings.epoch'),'Starting a write revokes older reads as well');
assert(web.includes('.settings-field[hidden]{display:none}'),'Hidden secret rows cannot be made visible by grid styling');
let collapsed=false,panelStorage=null;context.panel={dataset:{panelId:'live-device',panelLabel:'Live device',capability:'network'},hidden:false,classList:{toggle(_name,value){collapsed=value}},querySelector(){return{setAttribute(){}}}};
context.localStorage={getItem(){return panelStorage},setItem(_key,value){panelStorage=value}};
run("workspacePanels.push(panel);panelToggle('live-device','hidden')");assert(context.panel.hidden);assert(panelStorage.includes('live-device'));
run("panelToggle('live-device','hidden');panelToggle('live-device','collapsed')");assert(!context.panel.hidden);assert(collapsed);
context.currentPeer=()=>({known:false,has:()=>false});run('applyPanelVisibility()');assert(context.panel.hidden,'Capability loss must override stored panel visibility');
console.log('Shared settings catalog, strict required fields, secrets, dirty/conflict guards, ACK/readback, no replay, and session revocation passed (actual module / HTTP doubles, not hardware).');
