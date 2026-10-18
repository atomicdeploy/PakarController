// Executes the shipped workspace owner and public settings module. Storage/DOM
// doubles only; no device, browser layout or physical persistence proof.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

const read=p=>readFileSync(new URL('../'+p,import.meta.url),'utf8');
const web=read('firmware/esp8266/src/WebUi.h');
const workspace=web.split('// BEGIN WORKSPACE STORAGE')[1].split('// END WORKSPACE STORAGE')[0];
const catalog=JSON.parse(read('assets/contracts/device-settings.json'));
const settings=read('assets/web/device-settings.js');
assert(web.includes(settings.trim()),'Authoritative settings module must match the shipped copy');
assert(web.includes("if(typeof suspendWorkspace==='function')suspendWorkspace()"));
assert(web.includes('renderAudio(s);observeWorkspace(s);'));
assert(web.includes('if(document.getElementById)initializeWorkspaceStorage()'));

function fixture(storage=new Map()){
  const nodes={},events={},windowEvents={},timers=new Map();let nextTimer=0,writes=0,apiCalls=0;
  const element=()=>{let value='';return {get value(){return value},set value(next){value=String(next)},checked:false,type:'text',tagName:'INPUT',children:[],scrollTop:0,disabled:false,open:false,dataset:{},style:{},
    options:[],classList:{toggle(){}},setAttribute(){},close(){this.open=false},addEventListener(){},replaceChildren(){this.children=[]},
    appendChild(n){this.children.push(n)},querySelector(){return null},querySelectorAll(){return []}}};
  for(const match of web.matchAll(/id="([^"]+)"/g))nodes[match[1]]=Object.assign(element(),{id:match[1]});
  for(const id of ['wifiDhcp','wifiAddress','wifiGateway','wifiSubnet','wifiDns1','wifiDns2'])nodes[id]=Object.assign(element(),{id});
  for(const id of ['additiveSelection','animateCells','showCellIndicators','pixelDiagnostics','displayScroll','stationEnabled','apEnabled','wifiDhcp','wifiOpen','mqttEnabled','mqttTls','cloudWsEnabled','cloudWsTls'])nodes[id].type='checkbox';
  for(const[id,values]of Object.entries({cellShape:['rounded','circle','hexagon'],displayRepeat:['once','loop','interval'],effect:['fade','immediate','pulse'],bulkState:['red','white','off'],bulkEffect:['fade','immediate','pulse'],mqttSecretMode:['keep','replace','clear'],websocketSecretMode:['keep','replace','clear']})){
    nodes[id].tagName='SELECT';nodes[id].options=values.map(value=>({value}));nodes[id].value=values[0];
  }
  nodes.displayText.value='PAKAR CONTROLLER';nodes.displayValue.value='1';nodes.cellId.value='1';nodes.mapLogical.value='1';
  nodes.animateCells.checked=true;nodes.mqttSecretMode.value=nodes.websocketSecretMode.value='keep';
  const document={scrollingElement:{scrollTop:0},querySelector:s=>nodes[s.slice(1)]??null,querySelectorAll:()=>[],
    createElement:element,addEventListener:(name,fn)=>(events[name]??=[]).push(fn)};
  const context=vm.createContext({...nodes,document,DEVICE_SETTINGS:catalog,TextEncoder,URLSearchParams,console,uiLanguage:'en',uiTr:v=>v,
    setInterval(){},setTimeout:fn=>{const id=++nextTimer;timers.set(id,fn);return id},clearTimeout:id=>timers.delete(id),
    addEventListener:(name,fn)=>(windowEvents[name]??=[]).push(fn),
    localStorage:{getItem:key=>storage.get(key)??null,setItem:(key,value)=>{writes++;storage.set(key,value)}},
    api(){apiCalls++;throw Error('Restoring a workspace must not contact or mutate a device')},
    currentPeer:()=>({known:true,has:()=>true}),peerHttp:{writable:()=>false},confirm:()=>true,
    selectedCells:new Set(),selectionAnchor:1,cellFocus:1,cellOrder:Array.from({length:80},(_,i)=>i+1),
    refreshCellOrder(){},paintSelection(){},renderCellPixels(){},displayControls(){},report(){},
    webSocketReconnect:{cancel(){}},stopStream(){},socket:null,
  });
  const run=code=>vm.runInContext(code,context);
  run(settings);run(workspace);
  for(const name of ['normalizedCellOrder','selectionQuery','saveTablePreferences'])run(web.split(/\r?\n/).find(line=>line.startsWith('function '+name+'(')));
  run('initializeWorkspaceStorage()');
  return {nodes,storage,events,windowEvents,timers,context,run,get writes(){return writes},get apiCalls(){return apiCalls},
    input:id=>{for(const fn of events.input??[])fn({target:nodes[id]})},
    scroll:(id,top)=>{const target=id==='document'?document:nodes[id];(id==='document'?document.scrollingElement:target).scrollTop=top;for(const fn of events.scroll??[])fn({target})},
    flush:()=>run('workspaceStorage.flush()')};
}
const first={board_id:'esp32s3-aabbccddeeff',boot_id:'a'.repeat(32),http_peer_fencing:true,hostname:'ledboard',capabilities:{board_kind:'table',application:{id:'pakarcontroller.table',name:'Pakar Table',title:'Table controller'},acknowledged_features:['table','network']}};
const network={station_enabled:true,ap_enabled:true,station_ssid:'Office'};
const cloud={mqtt_enabled:false,mqtt_tls:true,mqtt_host:'mqtt.example.test',mqtt_port:8883,mqtt_user:'controller',mqtt_base_topic:'pakar',mqtt_password_set:true,websocket_enabled:false,websocket_tls:true,websocket_host:'',websocket_port:443,websocket_path:'/',websocket_token_set:false,telemetry_seconds:30};
const f=fixture();f.context.first=first;f.context.network=network;f.context.cloud=cloud;
f.run('observeWorkspace(first)');const key=f.run('activeWorkspaceKey');
assert.equal(f.writes,0,'Loading/default rendering never rewrites browser preferences');
f.run("settingsObserve('network',network);settingsObserve('cloud',cloud)");
f.run("selectedCells=new Set([3,4,7]);selectionAnchor=3;cellFocus=7;cellSearch.value='۳–۴, 7';webSettings.page='mqtt';panelPreferences={'live-display':{hidden:true,collapsed:false}}");
f.nodes.displayText.value='Remember this draft';f.input('displayText');
f.nodes.displayScroll.checked=true;f.input('displayScroll');
f.nodes.mqttHost.value='draft.example.test';f.input('mqttHost');
f.nodes.mqttPassword.value='DO-NOT-STORE-MQTT';f.nodes.wifiPassword.value='DO-NOT-STORE-WIFI';f.nodes.cloudWsToken.value='DO-NOT-STORE-TOKEN';
f.nodes.accessPassword.value='DO-NOT-STORE-ACCESS';f.nodes.remoteBearer.value='DO-NOT-STORE-BEARER';
f.input('mqttPassword');f.input('accessPassword');
f.nodes.settingsSearch.value='broker';f.input('settingsSearch');f.scroll('document',420);f.scroll('deviceSettingsDialog',95);
assert.equal(f.writes,0,'A burst of edits is debounced');assert.equal(f.timers.size,1);
f.flush();assert.equal(f.writes,1,'One pending snapshot covers a burst');
const saved=JSON.parse(f.storage.get(key));
assert.deepEqual(saved.selection,[3,4,7]);assert.equal(saved.query,'۳–۴, 7');assert.equal(saved.scroll.document,420);
assert.equal(saved.settings.cloud.draft.mqtt_host,'draft.example.test');assert.equal(saved.drafts.displayText,'Remember this draft');
assert(!f.storage.get(key).includes('DO-NOT-STORE'));assert(!Object.hasOwn(saved,'mode'));assert(!Object.hasOwn(saved,'state'));assert(!Object.hasOwn(saved,'boot_id'));
f.run('scheduleWorkspaceSave()');f.flush();assert.equal(f.writes,1,'Unchanged periodic rendering skips writes');

// A real session change invalidates authority separately; the same immutable
// board/application restores browser work only after its new status arrives.
f.run("suspendWorkspace();settingsInvalidate();selectedCells.clear();cellSearch.value='';displayText.value='';");
f.context.reboot={...first,boot_id:'b'.repeat(32),hostname:'renamed-board'};
f.run('observeWorkspace(reboot)');
assert.equal(f.run('activeWorkspaceKey'),key,'Boot and hostname are not workspace identity');
assert.deepEqual(Array.from(f.run('[...selectedCells]')),[3,4,7]);assert.equal(f.nodes.cellSearch.value,'۳–۴, 7');
assert.equal(f.nodes.displayText.value,'Remember this draft');assert.equal(f.nodes.displayScroll.checked,true);
assert.equal(f.run('webSettings.cloud'),null,'Cached settings cannot become observed authority');
assert.equal(f.run('webSettings.pending'),false);assert.equal(f.nodes.mqttPassword.value,'');
f.run("settingsObserve('network',network);settingsObserve('cloud',cloud)");
assert.equal(f.nodes.mqttHost.value,'draft.example.test');assert.equal(f.run('webSettings.cloud.saved.mqtt_host'),cloud.mqtt_host);
assert.equal(f.run('webSettings.cloud.conflict'),false);assert.equal(f.apiCalls,0);

// The same public draft is preserved with a conflict if another client changed
// its saved baseline while disconnected. It cannot silently become saveable.
f.run('suspendWorkspace();settingsInvalidate()');f.run('observeWorkspace(first)');
f.context.changedCloud={...cloud,mqtt_user:'another-client'};
f.run("settingsObserve('cloud',changedCloud)");assert.equal(f.nodes.mqttHost.value,'draft.example.test');assert.equal(f.run('webSettings.cloud.conflict'),true);
f.run('suspendWorkspace();settingsInvalidate();observeWorkspace(first)');f.run("settingsObserve('cloud',changedCloud)");
assert.equal(f.run('webSettings.cloud.conflict'),true,'Repeated reconnect cannot silently accept the changed baseline');

f.context.second={...first,board_id:'esp32s3-112233445566'};
f.run('suspendWorkspace();settingsInvalidate();observeWorkspace(second)');
assert.notEqual(f.run('activeWorkspaceKey'),key);assert.deepEqual(Array.from(f.run('[...selectedCells]')),[]);
assert.equal(f.nodes.cellSearch.value,'');assert.equal(f.nodes.displayText.value,'PAKAR CONTROLLER','A new peer gets template defaults, not another peer draft');
assert.equal(f.nodes.displayScroll.checked,false);assert.equal(f.run('webSettings.page'),'overview');
f.context.otherApp={...first,capabilities:{...first.capabilities,application:{...first.capabilities.application,id:'pakarcontroller.keys'}}};
assert.notEqual(f.run('workspaceIdentity(otherApp)'),key);assert.equal(f.run('workspaceIdentity({hostname:"ledboard"})'),'');
for(const bad of [{...first,board_id:'UPPER'},{...first,capabilities:{}},{...first,capabilities:{application:{id:'bad\nidentity'}}}]){f.context.bad=bad;assert.equal(f.run('workspaceIdentity(bad)'),'')}
assert.equal(f.apiCalls,0,'No cached mode, selection, draft or layout can trigger a device POST');

// Unknown or secret fields in local data do not survive normalization. Restored
// public drafts remain plain inputs and do not invoke onchange handlers.
f.storage.set(key,JSON.stringify({...saved,pending:true,mode:'first-wins',state:{cells:['red']},drafts:{...saved.drafts,accessPassword:'MALICIOUS-SECRET',remoteBearer:'MALICIOUS-SECRET'},settings:{...saved.settings,cloud:{base:{...cloud,mqtt_password:'MALICIOUS-SECRET'},draft:{...saved.settings.cloud.draft,mqtt_password:'MALICIOUS-SECRET'}}}}));
const reopened=fixture(new Map(f.storage));reopened.context.first=first;reopened.run('observeWorkspace(first);scheduleWorkspaceSave()');reopened.flush();
const clean=JSON.parse(reopened.storage.get(key));assert(!reopened.storage.get(key).includes('MALICIOUS-SECRET'));assert(!Object.hasOwn(clean,'pending'));assert(!Object.hasOwn(clean,'mode'));assert(!Object.hasOwn(clean,'state'));
assert.equal(reopened.apiCalls,0);

// The donor's storage-disabled behavior remains usable for the current session.
f.run("const blockedWorkspace=createWorkspaceStorage({storage:()=>{throw Error('blocked')}});blockedWorkspace.save('blocked',{query:'draft'});blockedWorkspace.flush();");
assert.equal(f.run("blockedWorkspace.read('blocked').query"),'draft','Disabled browser storage still retains this session workspace');
assert.equal(f.apiCalls,0);
console.log('Web workspace: immutable board/application scope, reboot restoration, public draft conflict guards, private-field exclusion, scroll/selection/find retention, debounced equal-write skips and zero device mutations passed (DOM/storage doubles).');
