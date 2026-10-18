import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import {webcrypto} from 'node:crypto';
const source=readFileSync(new URL('../firmware/esp8266/src/WebUi.h',import.meta.url),'utf8');
const script=source.match(/<script>([\s\S]*?)<\/script>/)[1];
// Execute the actual embedded Web application with a DOM/HTTP test double.
// These fixtures are not hardware observations or live firmware proof.
function element(){return {showModal(){this.open=true},close(){this.open=false},value:'',checked:false,hidden:false,textContent:'',className:'',children:[],get options(){return this.children},replaceChildren(...e){this.children=[...e]},dataset:{},style:{},listeners:{},classList:{toggle(){}},appendChild(e){this.children.push(e)},append(...e){this.children.push(...e)},setAttribute(){},addEventListener(type,handler){const previous=this.listeners[type];this.listeners[type]=(...args)=>{previous?.(...args);return handler(...args)}},querySelector(){return null},querySelectorAll(){return []}}}
const nodes=Object.fromEntries([...source.matchAll(/id="([^"]+)"/g)].map(m=>[m[1],element()]));
nodes.workspace.hidden=true;
let authEnabled=true,failStatus=false;
let signedIn=false,user='admin',password='pakarcontroller',socketCount=0,statusRequests=0,hostname='test-fixture',displayName='Fixture table';
const calls=[];
const context=vm.createContext({...nodes,Headers,AbortController,URLSearchParams,FormData,Uint8Array,TextEncoder,DataView,atob,console,
  performance,crypto:webcrypto,navigator:{platform:'fixture',hardwareConcurrency:4},screen:{width:1280,height:720},
  document:{getElementById:id=>nodes[id]||null,addEventListener(){},documentElement:{dataset:{},style:{setProperty(){}}},querySelector:s=>nodes[s.slice(1)],querySelectorAll:()=>[],createElement:element,createTextNode:text=>({textContent:text}),createElementNS:()=>({...element(),namespaceURI:'svg'})},
  location:{protocol:'http:',host:'pakarcontroller'},setInterval:()=>1,clearInterval(){},setTimeout:()=>1,clearTimeout(){},
  WebSocket:class {constructor(){socketCount++;this.readyState=1;this.bufferedAmount=0}close(){this.readyState=3}send(){}},
  fetch:async(path,options={})=>{
    calls.push({path,options});let code=200,body={};
    if(path==='/api/auth/info')body={ready:true,enabled:authEnabled,username:user,default_credentials:authEnabled&&password==='pakarcontroller'};
    else if(path==='/login'){
      signedIn=options.body.get('username')===user&&options.body.get('password')===password;
      if(!signedIn){code=401;body='Incorrect username or password'}
    }else if(authEnabled&&!signedIn){code=401;body={error:'login_required'}}
    else if(path==='/logout'){signedIn=false}
    else if(path==='/api/auth'){
      assert.equal(options.method,'POST');assert.equal(options.body.get('current_password'),password);
      authEnabled=options.body.get('enabled')==='true';
      if(authEnabled){user=options.body.get('username');password=options.body.get('password');}signedIn=false;
      body={updated:true,enabled:authEnabled,reauthenticate:authEnabled};
    }else if(path==='/api/device/name'){
      let changed=false;
      if(options.method==='POST'){changed=hostname!==options.body.get('hostname');hostname=options.body.get('hostname');displayName=options.body.get('display_name');}
      body={hostname,display_name:displayName,persisted:true,accepted:options.method==='POST',apply_pending:changed};
    }else if(path.split('?')[0]==='/api/status'){
      if(failStatus)throw Error('Network unreachable');statusRequests++;body={hostname:'test-fixture',board_id:'esp8266-aabbccddeeff',boot_id:'a'.repeat(32),http_peer_fencing:true,cells:[],tm1637_segments:[],capabilities:{name:'Fixture table',board_kind:'table',acknowledged_features:['table','leds','streaming'],table:{columns:10,rows:8,pixels_per_cell:6}}};
    }else if(path==='/api/cells/mapping')body={physical:[],names:[]};
    return {ok:code<400,status:code,statusText:String(code),text:async()=>typeof body==='string'?body:JSON.stringify(body)};
  }});
vm.runInContext(source.match(/<script data-pakar-presentation>([\s\S]*?)<\/script>/)[1],context);
vm.runInContext(script,context);
const settle=async()=>{for(let i=0;i<100;i++)await Promise.resolve()};
await settle();
assert.equal(nodes.workspace.hidden,true);assert.equal(socketCount,0);
assert.match(nodes.loginHint.textContent,/admin \/ pakarcontroller/);
nodes.loginUser.value='admin';nodes.loginPassword.value='wrong';
await nodes.loginForm.listeners.submit({preventDefault(){}});await settle();
assert.match(nodes.loginError.textContent,/Incorrect/);assert.equal(socketCount,0);
nodes.loginPassword.value='pakarcontroller';
await nodes.loginForm.listeners.submit({preventDefault(){}});await settle();
assert.equal(nodes.workspace.hidden,false,nodes.loginError.textContent);assert.equal(socketCount,1);assert.equal(nodes.loginPassword.value,'');
assert.equal(statusRequests,2,'Initial read and browser-telemetry write preflight');assert.match(nodes.accessWarning.textContent,/Default password/);
nodes.accessCurrent.value=password;nodes.accessUser.value='David';nodes.accessPassword.value='new-password';nodes.accessConfirm.value='different';
await nodes.accessForm.listeners.submit({preventDefault(){}});
assert.equal(calls.filter(c=>c.path==='/api/auth').length,0);
nodes.accessConfirm.value='new-password';await nodes.accessForm.listeners.submit({preventDefault(){}});await settle();
assert.equal(nodes.workspace.hidden,true);assert.equal(nodes.accessCurrent.value,'');assert.equal(nodes.accessPassword.value,'');
assert.equal(nodes.accessConfirm.value,'');assert.equal(nodes.loginUser.value,'David');
assert.doesNotMatch(nodes.loginHint.textContent,/pakarcontroller/);
assert.ok(calls.every(c=>!c.path.includes('password=')),'Secrets must not be put into URLs');
nodes.loginUser.value='David';nodes.loginPassword.value='new-password';
await nodes.loginForm.listeners.submit({preventDefault(){}});await settle();
assert.equal(nodes.workspace.hidden,false);assert.equal(nodes.accessWarning.textContent,'');
assert.equal(nodes.deviceName.value,'test-fixture.local');assert.equal(nodes.nameSave.disabled,false);
assert.equal(nodes.boardDisplayName.value,'Fixture table');
nodes.boardDisplayName.value='David’s table';await nodes.nameForm.listeners.submit({preventDefault(){}});await settle();
assert.equal(displayName,'David’s table');assert.equal(nodes.nameSave.disabled,false);assert.equal(nodes.workspace.hidden,false);
assert.match(vm.runInContext('webToasts.snapshot().at(-1).message',context),/verified/);
nodes.deviceName.value='https://bad.local';
await nodes.nameForm.listeners.submit({preventDefault(){}});
assert.equal(calls.filter(c=>c.path==='/api/device/name'&&c.options.method==='POST').length,1);
nodes.deviceName.value='LEDBOARD.Local';await nodes.nameForm.listeners.submit({preventDefault(){}});
assert.equal(hostname,'ledboard');assert.equal(nodes.nameSave.disabled,true);
assert.equal(nodes.nameResult.href,'http://ledboard.local/');assert.equal(nodes.nameResult.hidden,false);
assert.match(vm.runInContext('webToasts.snapshot().at(-1).message',context),/Identity saved/);
vm.runInContext('authenticated=true;syncStreamControls()',context);
// The rename retired its socket; never enable streaming from stale UI identity.
assert.equal(nodes.streamStart.disabled,true);
await vm.runInContext('refresh();connectWs()',context);await settle();
vm.runInContext('syncStreamControls()',context);assert.equal(nodes.streamStart.disabled,false);
await vm.runInContext('startStream()',context);assert.equal(vm.runInContext('frameStream.running()',context),true);
assert.equal(nodes.streamStart.disabled,true);assert.equal(nodes.streamStop.disabled,false);
assert.equal(nodes.streamFps.value,'60');assert.equal(nodes.streamQueue.value,'2');
vm.runInContext('uiLanguage="fa";renderStreamStatus()',context);
assert.match(nodes.streamStats.textContent,/ارسال‌های مرورگر/);assert.match(nodes.streamStats.textContent,/۰ فریم/);
vm.runInContext('uiLanguage="en";renderStreamStatus()',context);assert.match(nodes.streamStats.textContent,/Browser sends/);
vm.runInContext('selectedCells=new Set([1,2]);cellSearch.value="1–2";remoteBearer.value="private";render({...state,boot_id:"b".repeat(32)})',context);
assert.equal(vm.runInContext('frameStream.running()',context),false,'Reboot revokes the actual running browser stream');
assert.equal(vm.runInContext('selectedCells.size',context),0);assert.equal(nodes.cellSearch.value,'');assert.equal(nodes.remoteBearer.value,'');
assert.equal(vm.runInContext('webSettings.cloud',context),null);assert.equal(nodes.boardDisplayName.value,'');
await vm.runInContext('signOut()',context);assert.equal(nodes.workspace.hidden,true);
authEnabled=false;signedIn=false;await vm.runInContext('bootstrap()',context);await settle();
assert.equal(nodes.workspace.hidden,false);assert.equal(nodes.loginPanel.hidden,true);assert.equal(nodes.signOutButton.hidden,true);assert.equal(nodes.accessCredentials.hidden,true);assert.equal(nodes.accessCurrentRow.hidden,true);
const loginCount=calls.filter(c=>c.path==='/login').length;await vm.runInContext('signOut()',context);assert.equal(calls.filter(c=>c.path==='/login').length,loginCount);
failStatus=true;await vm.runInContext('bootstrap()',context);await settle();assert.equal(nodes.loginPanel.hidden,true);assert.equal(nodes.connectionRetry.hidden,false);assert.match(nodes.connectionState.textContent,/unreachable/);
nodes.appearanceOpen.listeners.click();assert.equal(nodes.appearanceDialog.open,true);assert.equal(nodes.uiHapticsRow.hidden,true);assert.equal(nodes.uiLanguage.value,'system');nodes.appearanceCancel.listeners.click();assert.equal(nodes.appearanceDialog.open,false);
const prefs=source.slice(source.indexOf('<dialog id="appearanceDialog"'),source.indexOf('</dialog>',source.indexOf('<dialog id="appearanceDialog"')));
for(const id of ['uiLanguage','uiDirection','uiHaptics'])assert(prefs.includes('id="'+id+'"'));
console.log('Embedded Web auth-off login suppression, contextual Preferences, no fake login after connection failure, sign-in, rotation, validated rename, acknowledged stream start, live Persian/English stream status, reboot cancellation and sign-out passed.');
