// Executes the actual shipped browser transport. No board or installed app proof.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {webcrypto} from 'node:crypto';
import vm from 'node:vm';
import {webPeerSource} from '../tools/web-peer-build.mjs';
import {assertToolPeerCompatible,toolHeaders,toolRelease,toolVersionCompare,toolReport,toolVersionEnforced} from '../tools/peer-identity.mjs';
const source=await webPeerSource(readFileSync(new URL('../assets/web/peer-http.js',import.meta.url),'utf8'));
const storage=new Map(),context=()=>vm.createContext({Headers,AbortController,setTimeout,clearTimeout,URL,crypto:webcrypto,
  sessionStorage:{getItem:key=>storage.get(key),setItem:(key,value)=>storage.set(key,value)},
  navigator:{platform:'Win32',userAgent:'Actual browser UA',hardwareConcurrency:8,deviceMemory:4},screen:{width:1920,height:1080}});
const c=context();vm.runInContext(source+'\nglobalThis.factory=createPeerHttp;globalThis.compare=peerVersionCompare;',c);
const status={version:'0.2.8',application:'PakarController',board_id:'esp32s3-fixture',boot_id:'a'.repeat(32),http_peer_fencing:true,
  compatibility:{enforced:true,minimum_app:'0.2.8',maximum_app:null}};
for(const [a,b,result]of [['0.2.7','0.2.7',0],['0.2.8','0.2.7',1],['0.2.7+local','0.2.7',0],['0.2.7-rc.1','0.2.7',-1],
  ['1.0.0-alpha.9','1.0.0-alpha.10',-1],['1.0.0-alpha','1.0.0-alpha.1',-1],['1.0.0-alpha.1','1.0.0-alpha.x',-1],['01.2.7','0.2.7',null],
  ['0.2.7-01','0.2.7',null],['0.2.7\r\nX: injected','0.2.7',null],['development','0.2.7',null],
  ['4294967295.0.0','0.2.8',1],['4294967296.0.0','0.2.8',null],['0.2.8+'+'x'.repeat(58),'0.2.8',null]]){
  assert.equal(c.compare(a,b),result);assert.equal(toolVersionCompare(a,b),result,'Node/browser bounds must agree');
}
let current=status,calls=[];
const http=c.factory({fetcher:async(path,init)=>{calls.push({path,init});return Response.json(path==='/api/status'?current:{accepted:true},
  {headers:{Server:'PakarController/0.2.8','X-Pakar-Version':'0.2.8','X-Pakar-Build':'15'}})}});
await http.request('/api/status');assert(http.writable());
await http.request('/api/status',{headers:{'User-Agent':'Forged browser'}});
const identity=http.clientIdentity();assert.equal(identity.version,toolRelease.version);assert.equal(identity.build,toolRelease.build);
assert.match(identity.id,/^[a-f0-9-]{36}$/);
const headers=calls[0].init.headers;
assert.equal(headers.get('User-Agent'),null,'Browser UA must remain genuine');
assert.equal(headers.get('X-Pakar-Client'),'pakarcontroller-web');assert.equal(headers.get('X-Pakar-Version'),toolRelease.version);
assert.equal(headers.get('X-Pakar-Client-Id'),identity.id);assert.equal(headers.get('X-Pakar-Build'),String(toolRelease.build));
assert.equal(http.peerIdentity().headers.Server,'PakarController/0.2.8');
const socket=new URL(http.webSocketUrl('ws://ledboard.local/ws'));
assert.equal(socket.pathname,'/ws');assert.equal(socket.searchParams.get('client'),'pakarcontroller-web');
assert.equal(socket.searchParams.get('version'),identity.version);assert.equal(socket.searchParams.get('client_id'),identity.id);
const c2=context();vm.runInContext(source+'\nglobalThis.factory=createPeerHttp;',c2);
assert.equal(c2.factory().clientIdentity().id,identity.id,'Reload preserves the tab session, not a fresh report peer each refresh');
const report=http.report({settings:{theme:'dark',password:'secret',mqtt_token:'secret',language:'en'}});
assert.equal(report.version,identity.version);assert.equal(report.user_agent,'Actual browser UA');assert.equal(report.hardware.cpu_count,8);
assert.equal(report.session_id,identity.id);assert.equal(report.hardware.memory_mb,4096);
assert.equal(report.settings.theme,'dark');assert(!JSON.stringify(report).includes('secret'));assert(Buffer.byteLength(JSON.stringify(report))<=1536);
for(const bad of [{...status,version:'0.2.7'},{...status,version:'0.2.8-rc.1'},{...status,version:'nonsense'},
  {...status,compatibility:{enforced:true,minimum_app:'0.2.9'}},{...status,compatibility:{enforced:true,minimum_app:'0.2.0',maximum_app:'0.2.8'}}]){
  current=bad;await assert.rejects(http.request('/api/status'),e=>e.code==='unsupported_peer_version');
  assert(!http.writable());const count=calls.length;await assert.rejects(http.request('/api/cells',{method:'POST'}));assert.equal(calls.length,count,'Blocked version sends no mutation');
  assert.throws(()=>assertToolPeerCompatible(bad),/Unsupported/);
}
current={...status,version:'9.0.0',compatibility:{enforced:true,minimum_app:'0.2.8'}};
await http.request('/api/status');assert(http.writable(),'Minimum floor is not a stale equality adapter');assertToolPeerCompatible(current);
current={...status,version:'0.2.9-alpha',compatibility:{enforced:true,minimum_app:'0.2.8'}};
await http.request('/api/status');assert(http.writable(),'Future prerelease above the stable floor remains additive');assertToolPeerCompatible(current);
const permissive=c.factory({enforceVersions:false});permissive.observe({...status,version:'old-unsupported'});assert(permissive.writable());
permissive.observe({...status,version:'old-unsupported',boot_id:'0'.repeat(32)});assert(!permissive.writable(),'Version override never relaxes peer identity');
assertToolPeerCompatible({...status,version:'old-unsupported'},{enforce:false});
const tool=toolHeaders({'User-Agent':'wrong','X-Pakar-Version':'wrong','user-agent':'wrong','x-pakar-version':'wrong'});
assert(!Object.hasOwn(tool,'user-agent')&&!Object.hasOwn(tool,'x-pakar-version'),'Case variants cannot duplicate product identity');
assert.match(tool['User-Agent'],new RegExp('^PakarController/'+toolRelease.version.replaceAll('.','\\.')+' '));
assert.equal(tool['X-Pakar-Version'],toolRelease.version);assert.equal(tool['X-Pakar-Client-Id'],toolHeaders()['X-Pakar-Client-Id']);
assert.equal(toolReport().id,tool['X-Pakar-Client-Id']);assert.equal(toolReport().version,toolRelease.version);
assert(toolVersionEnforced({}));assert(!toolVersionEnforced({PAKAR_ENFORCE_PEER_VERSIONS:'0'}));assert(!toolVersionEnforced({PAKAR_ENFORCE_PEER_VERSIONS:'false'}));
console.log('Web/tool peer identification: genuine browser UA, own build identity, stable session, HTTP/WS metadata, strict inclusive floor/exclusive ceiling, disable flag and secret-free report passed.');
