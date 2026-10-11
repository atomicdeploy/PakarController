import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import {webcrypto} from 'node:crypto';
const source=readFileSync(new URL('../firmware/esp8266/src/WebUi.h',import.meta.url),'utf8');
const script=source.match(/<script>([\s\S]*?)<\/script>/)[1];
// Execute the actual embedded Web application with a DOM/HTTP test double.
// These fixtures are not hardware observations or live firmware proof.
function element(){return {value:'',checked:false,hidden:false,textContent:'',className:'',children:[],get options(){return this.children},replaceChildren(...e){this.children=[...e]},dataset:{},listeners:{},classList:{toggle(){}},appendChild(e){this.children.push(e)},append(...e){this.children.push(...e)},setAttribute(){},addEventListener(type,handler){this.listeners[type]=handler},querySelector(){return null},querySelectorAll(){return []}}}
const nodes=Object.fromEntries([...source.matchAll(/id="([^"]+)"/g)].map(m=>[m[1],element()]));
nodes.workspace.hidden=true;
let signedIn=false,user='admin',password='pakarcontroller',socketCount=0,statusRequests=0,hostname='test-fixture',displayName='Fixture table';
const calls=[];
const context=vm.createContext({...nodes,URLSearchParams,FormData,Uint8Array,TextEncoder,DataView,atob,console,
  performance,crypto:webcrypto,navigator:{platform:'fixture',hardwareConcurrency:4},screen:{width:1280,height:720},
  document:{documentElement:{dataset:{},style:{setProperty(){}}},querySelector:s=>nodes[s.slice(1)],querySelectorAll:()=>[],createElement:element,createTextNode:text=>({textContent:text}),createElementNS:()=>({...element(),namespaceURI:'svg'})},
  location:{protocol:'http:',host:'pakarcontroller'},setInterval:()=>1,clearInterval(){},setTimeout:()=>1,
  WebSocket:class {constructor(){socketCount++;this.readyState=1}close(){this.readyState=3}send(){}},
  fetch:async(path,options={})=>{
    calls.push({path,options});let code=200,body={};
    if(path==='/api/auth/info')body={ready:true,username:user,default_credentials:password==='pakarcontroller'};
    else if(path==='/login'){
      signedIn=options.body.get('username')===user&&options.body.get('password')===password;
      if(!signedIn){code=401;body='Incorrect username or password'}
    }else if(!signedIn){code=401;body={error:'login_required'}}
    else if(path==='/logout'){signedIn=false}
    else if(path==='/api/auth'){
      assert.equal(options.method,'POST');assert.equal(options.body.get('current_password'),password);
      user=options.body.get('username');password=options.body.get('password');signedIn=false;
      body={updated:true,reauthenticate:true};
    }else if(path==='/api/device/name'){
      let changed=false;
      if(options.method==='POST'){changed=hostname!==options.body.get('hostname');hostname=options.body.get('hostname');displayName=options.body.get('display_name');}
      body={hostname,display_name:displayName,persisted:true,accepted:options.method==='POST',apply_pending:changed};
    }else if(path.split('?')[0]==='/api/status'){
      statusRequests++;body={hostname:'test-fixture',cells:[],tm1637_segments:[]};
    }else if(path==='/api/cells/mapping')body={physical:[],names:[]};
    return {ok:code<400,status:code,statusText:String(code),text:async()=>typeof body==='string'?body:JSON.stringify(body)};
  }});
vm.runInContext(source.match(/<script data-pakar-presentation>([\s\S]*?)<\/script>/)[1],context);
vm.runInContext(script,context);
const settle=async()=>{for(let i=0;i<15;i++)await Promise.resolve()};
await settle();
assert.equal(nodes.workspace.hidden,true);assert.equal(socketCount,0);
assert.match(nodes.loginHint.textContent,/admin \/ pakarcontroller/);
nodes.loginUser.value='admin';nodes.loginPassword.value='wrong';
await nodes.loginForm.listeners.submit({preventDefault(){}});await settle();
assert.match(nodes.loginError.textContent,/Incorrect/);assert.equal(socketCount,0);
nodes.loginPassword.value='pakarcontroller';
await nodes.loginForm.listeners.submit({preventDefault(){}});await settle();
assert.equal(nodes.workspace.hidden,false,nodes.loginError.textContent);assert.equal(socketCount,1);assert.equal(nodes.loginPassword.value,'');
assert.equal(statusRequests,1);assert.match(nodes.accessWarning.textContent,/Default password/);
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
await vm.runInContext('signOut()',context);assert.equal(nodes.workspace.hidden,true);
console.log('Embedded Web sign-in, rejection, rotation, secret clearing, validated persistent rename, reconnect link and sign-out passed.');
