import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import {webcrypto} from 'node:crypto';
const js=readFileSync(new URL('../assets/web/basic-cells.js',import.meta.url),'utf8');
const peer=readFileSync(new URL('../assets/web/peer-http.js',import.meta.url),'utf8');
const html=readFileSync(new URL('../assets/web/basic.html',import.meta.url),'utf8');
const firmware=readFileSync(new URL('../firmware/esp8266/esp8266.ino',import.meta.url),'utf8');
assert.match(firmware,/server\.native\(\)\.on\(AsyncURIMatcher::exact\("\/basic"\)/,'Immutable page bypasses the hardware queue');
assert.match(firmware,/request->send\(200, "text\/html; charset=utf-8",\s*reinterpret_cast<const uint8_t \*>\(kBasicUi\), sizeof\(kBasicUi\) - 1\)/,'Use bounded flash response, not ESP32 char* send_P heap overload');
const {createBasicCells,createPeerHttp}=vm.runInNewContext(peer+'\n'+js+'\n({createBasicCells,createPeerHttp})',{URLSearchParams,Headers,AbortController,setTimeout,clearTimeout,crypto:webcrypto});
function fixture(){
  const status={version:'0.2.8',board_id:'esp32s3-aabbccddeeff',boot_id:'1'.repeat(32),http_peer_fencing:true,hostname:'ledboard',pixels:480,
    capabilities:{board_kind:'table',acknowledged_features:['table','leds','cell-batch'],table:{columns:10,rows:8,pixels_per_cell:6}},
    cells:Array(80).fill('white'),led_output:{enabled:true,mode:'static',driver:{backend:'gpio-static',dma:false,ready:true}}};
  const f={status,calls:[],updates:[],time:100,authenticated:true,authEnabled:false,defaultCredentials:false,failRead:false,hold:false,pending:null};
  const http=createPeerHttp({now:()=>f.time,fetcher:async(path,init)=>{
    const url=new URL(path,'http://ledboard.local'),method=init.method||'GET';f.calls.push({path,method,init});
    if(url.pathname==='/api/auth/info')return Response.json({enabled:f.authEnabled,username:'admin',default_credentials:f.defaultCredentials});
    if(url.pathname==='/login'){f.authenticated=true;return Response.json({authenticated:true})}
    if(!f.authenticated)return Response.json({error:'login_required'},{status:401});
    if(url.pathname==='/api/status'){
      if(f.failRead)throw Error('Connection lost');
      if(f.hold)return new Promise(resolve=>f.pending=()=>{f.hold=false;resolve(Response.json(status))});
      return Response.json(status);
    }
    if(url.pathname==='/api/cells'){
      assert.equal(method,'POST');assert(init.body instanceof URLSearchParams);assert.equal(init.body.get('effect'),'immediate');
      assert.equal(init.headers.get('X-Pakar-Board'),status.board_id);assert.equal(init.headers.get('X-Pakar-Boot'),status.boot_id);
      assert.deepEqual([...init.body.keys()],['ids','state','effect']);
      status.cells[Number(init.body.get('ids'))-1]=init.body.get('state');
      if(f.afterPost)f.afterPost();return Response.json({accepted:true,count:f.ackCount??1});
    }
    if(url.pathname==='/api/led/output'){
      assert.equal(method,'POST');assert.equal(init.body,'');assert.equal(url.searchParams.get('enabled'),'true');assert.equal(url.searchParams.get('mode'),'static');
      status.led_output={...status.led_output,enabled:true,mode:'static'};return Response.json(status.led_output);
    }
    throw Error('Unexpected endpoint '+path);
  }});
  f.app=createBasicCells({http,changed:s=>f.updates.push(s)});return f;
}
const posts=f=>f.calls.filter(c=>c.method==='POST');
{
  const f=fixture();assert(!f.app.snapshot().canWrite);await f.app.refresh();assert(f.app.snapshot().canWrite);
  f.app.select(80);await f.app.setColor('red');assert.equal(posts(f).length,1);assert.equal(posts(f)[0].path,'/api/cells');assert.equal(posts(f)[0].init.body.toString(),'ids=80&state=red&effect=immediate');assert.equal(f.app.snapshot().cells[79],'red');
  assert(f.app.snapshot().cells.slice(0,79).every(v=>v==='white'));await f.app.setColor('white');assert.equal(posts(f).length,2);assert.equal(f.app.snapshot().cells[79],'white');
  f.time+=60000;assert(f.app.snapshot().canWrite,'Inactivity is not a forced manual refresh');await f.app.setColor('red');assert.equal(posts(f).length,3,'Same peer is freshly read before exactly one POST');
}
{
  const f=fixture();await f.app.refresh();f.time+=60000;f.status.boot_id='2'.repeat(32);assert(!await f.app.setColor('red'));assert.equal(posts(f).length,0);assert(!f.app.snapshot().known);assert.match(f.app.snapshot().message,/boot changed/);
}
{
  const f=fixture();await f.app.refresh();f.afterPost=()=>{f.hold=true};const pending=f.app.setColor('red');
  for(let i=0;i<20&&!f.pending;i++)await new Promise(resolve=>setImmediate(resolve));assert(f.pending);
  assert.equal(f.app.snapshot().cells[0],'white','No optimistic paint after POST/ACK');assert(f.app.snapshot().busy);
  await f.app.setColor('white');assert.equal(posts(f).length,1,'Double clicks do not duplicate writes');f.pending();await pending;assert.equal(f.app.snapshot().cells[0],'red');
}
{
  const f=fixture();await f.app.refresh();f.afterPost=()=>{f.failRead=true};assert(!await f.app.setColor('red'));
  assert.equal(posts(f).length,1);assert(!f.app.snapshot().known);assert(f.app.snapshot().cells.every(v=>v===null));
  assert(!await f.app.setColor('white'));assert.equal(posts(f).length,1,'Uncertain mutation is not replayed');f.failRead=false;await f.app.refresh();assert(f.app.snapshot().known);
}
for(const mode of ['effects','disabled']){
  const f=fixture();f.status.led_output.mode='effects';f.status.led_output.driver={backend:'rmt-dma',dma:true,ready:true};if(mode==='disabled')f.status.led_output.enabled=false;
  await f.app.refresh();assert(!f.app.snapshot().canWrite);assert(f.app.snapshot().canEnable);assert(!await f.app.setColor('red'));assert.equal(posts(f).length,0);
  assert(await f.app.enableStatic());assert.equal(posts(f).length,1);assert(f.app.snapshot().canWrite);
}
for(const invalid of ['capabilities','geometry','driver']){
  const f=fixture();if(invalid==='capabilities')f.status.capabilities.acknowledged_features=[];if(invalid==='geometry')f.status.cells.pop();if(invalid==='driver')f.status.led_output.driver.ready=false;
  await f.app.refresh();assert(!f.app.snapshot().canWrite);assert(!f.app.snapshot().canEnable);
}
for(const driver of [{backend:'gpio-static',dma:false,ready:true},{backend:'rmt-dma',dma:true,ready:true}]){
  const f=fixture();f.status.led_output.driver=driver;await f.app.refresh();assert(f.app.snapshot().canWrite);
  assert.match(f.app.snapshot().reason,driver.dma?/RMT DMA/:/GPIO/);assert.doesNotMatch(f.app.snapshot().reason,/non-DMA/);
  assert(await f.app.setColor('red'));assert.equal(posts(f).length,1);assert.equal(f.app.snapshot().cells[0],'red');
}
for(const driver of [{backend:'rmt-dma',dma:false,ready:true},{backend:'gpio-static',dma:true,ready:true},{backend:'unknown',dma:true,ready:true}]){
  const f=fixture();f.status.led_output.driver=driver;await f.app.refresh();assert(!f.app.snapshot().canWrite);assert(!f.app.snapshot().canEnable);
  assert(!await f.app.setColor('red'));assert.equal(posts(f).length,0);
}
{
  const f=fixture();f.status.capabilities.acknowledged_features=['table','leds'];await f.app.refresh();assert(!f.app.snapshot().canWrite);assert(!f.app.snapshot().canEnable);
  assert(!await f.app.setColor('red'));assert.equal(posts(f).length,0,'No legacy per-cell fallback for old firmware');assert.match(f.app.snapshot().reason,/atomic cell batches/);
}
{
  const f=fixture();await f.app.refresh();f.ackCount=80;assert(!await f.app.setColor('red'));assert.equal(posts(f).length,1);assert(!f.app.snapshot().known);assert.match(f.app.snapshot().message,/not acknowledged/);
}
{
  const f=fixture();await f.app.refresh();f.afterPost=()=>{f.status.cells.pop()};assert(!await f.app.setColor('red'));assert.equal(posts(f).length,1);assert(!f.app.snapshot().known,'Reject incomplete all-cell readback');
}
{
  const f=fixture();f.authenticated=false;f.authEnabled=true;f.defaultCredentials=true;assert(!await f.app.refresh());assert.equal(f.app.snapshot().authInfo.default_credentials,true);
  assert(await f.app.login('owner','private-fixture'));assert.equal(posts(f).length,1);assert.equal(posts(f)[0].path,'/login');assert(!JSON.stringify(f.app.snapshot()).includes('private-fixture'));
}
assert(html.includes('{{APP_ICON}}')&&html.includes('{{PALETTE}}')&&html.includes('{{PEER_HTTP}}')&&html.includes('{{BASIC_CELLS}}'));
for(const token of ['canvas','surface-0','surface-1','surface-2','surface-3','text','muted','line','blue','red'])assert(html.includes(`var(--${token})`));
assert(!/#[0-9a-f]{3,8}\b/i.test(html),'No second hardcoded palette');
assert.match(html,/grid-template-columns:repeat\(10/);assert.match(html,/nth-child\(odd\).*translateY\(calc\(50%/);
assert.match(js,/byId\('basicLogin'\)\.hidden=s\.known/);assert.match(js,/default_credentials!==true/);
assert.doesNotMatch(js,/setInterval|setTimeout|WebSocket|requestAnimationFrame/,'No background traffic or animation');
console.log('Actual basic controller + shared peer HTTP: logical1..80 readback, singlePOST/nooptimism, stale samepeer refresh, changedboot refusal, unknown/no replay, explicit static enable, login and palette/row-major wiring passed; no browser/physical acceptance.');
