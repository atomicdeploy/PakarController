import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const source=readFileSync(new URL('../firmware/esp8266/src/WebUi.h',import.meta.url),'utf8');
const persian=JSON.parse(source.match(/const PAKAR_FA=(\{[^\n]+\});/)[1]);
const contextCode=source.split('// BEGIN PEER CONTEXT')[1].split('// END PEER CONTEXT')[0];
const nodes=new Map();
function element(){const node={hidden:false,dataset:{},children:[],classList:{toggle(){}},replaceChildren(){this.children=[]},
  appendChild(n){n.parentElement=this;this.children.push(n);return n},append(...items){for(const item of items)this.appendChild(item)},
  insertBefore(n,anchor){const at=this.children.indexOf(anchor);assert(at>=0);n.parentElement=this;this.children.splice(at,0,n)}};
  Object.defineProperty(node,'id',{get(){return this.nodeId},set(value){this.nodeId=value;nodes.set(value,this)}});return node}
const cards=[...source.matchAll(/data-capability="([^"]+)"/g)].map(m=>Object.assign(element(),{dataset:{capability:m[1]}}));
for(const match of source.matchAll(/<[^>]+data-capability="[^" ]+"[^>]*>/g))assert(match[0].includes(' hidden'), 'No unacknowledged flash of controls at startup');
const metrics=Object.fromEntries(['tm','avr','link','uart','winner'].map(id=>[id,element()]));
const boardModes=element(),snakeControls=element(),contextMessage=element();
const ledCard=cards.find(n=>n.dataset.capability==='leds'),tableCard=cards.find(n=>n.dataset.capability==='table');
tableCard.appendChild(boardModes);const patternRow=element(),pattern=element();pattern.id='pattern';patternRow.appendChild(pattern);ledCard.appendChild(patternRow);
let now=1000,stops=0,closed=0,cancelledOrders=0;
const document={title:'PakarController',documentElement:{dataset:{}},createElement:element,getElementById:id=>nodes.get(id),querySelector:s=>metrics[s.slice(1)]??null,querySelectorAll:s=>s==='[data-capability]'?cards:[]};
let writable=true;
const context=vm.createContext({Date:{now:()=>now},state:{cells:[]},document,boardModes,snakeControls,contextMessage,
  cellManager:{open:false,close(){closed++;this.open=false}},cellBatchCancelled:false,
  ownedCopy:[],ownedAttributes:[],ownedDynamicCopy:new Map(),uiLanguagePreference:'en',uiDirectionPreference:'auto',
  resolveUiLanguage:value=>value,resolveUiDirection:(_preference,language)=>language==='fa'?'rtl':'ltr',uiNumber:String,
  uiTr:text=>context.uiLanguage==='fa'?(persian[text]??text):text,
  peerHttp:{writable:()=>writable},
  TextEncoder,DataView,Uint8Array,atob,
  stopStream(){stops++},closeCellMenu(){},setEditingOrder(v){if(!v)cancelledOrders++}});
for(const name of ['uiCopyValues','setUiText'])vm.runInContext(source.match(new RegExp('function '+name+'[^\\n]+'))[0],context);
vm.runInContext(source.slice(source.indexOf('function applyLocalization(){'),source.indexOf('function saveLocalization(){')),context);
vm.runInContext(source.split('// BEGIN APPLICATION IDENTITY')[1].split('// END APPLICATION IDENTITY')[0],context);
vm.runInContext(contextCode,context);
const run=s=>vm.runInContext(s,context);
const set=s=>{context.state=s;run('contextReceivedAt=Date.now();renderContext()')};
const visible=f=>cards.filter(n=>n.dataset.capability===f).some(n=>!n.hidden);
const manifest=(features,kind='controller')=>({name:'Fixture board',board_kind:kind,acknowledged_features:features});
set({hostname:'ledboard',pixels:480,cells:Array(80).fill('white'),avr_online:true,tm1637_ack:true,audio_cues:['welcome']});
assert(cards.every(n=>n.hidden),'No inferred board kind or controls from generic status/ACK/audio metadata');
assert(!run("contextAllows('/api/buzzer')"));
set({capabilities:manifest(['telemetry','buzzer'])});
assert(visible('buzzer'));assert(!visible('display'));assert(!visible('text'));assert(!visible('table'));
assert.equal(boardModes.children.length,0);assert(snakeControls.hidden);
set({capabilities:manifest(['telemetry','display'])});
assert(visible('display'));assert(!visible('text'));assert(!visible('buzzer'));
assert(!run("contextAllows('/api/display',{text:'HELLO'})"));
assert(run("contextAllows('/api/display',{value:1})"));
set({capabilities:manifest(['text'])});assert(visible('text'));assert(!visible('display'));assert(!visible('buzzer'));
const modes=[{id:'idle',code:0,label:'Idle',kind:'presentation'},{id:'cell',code:1,label:'Cell selection',kind:'presentation'},{id:'snake',code:3,label:'Snake',kind:'presentation'},{id:'first-wins',code:2,label:'First wins',kind:'game'}];
const table={...manifest(['table','leds','streaming'],'table'),table:{columns:10,rows:8,pixels_per_cell:6},modes};
set({capabilities:table,mode:'snake'});assert(visible('table'));assert(snakeControls.hidden,'Snake stays deferred even if an older manifest advertises it');
assert.equal(run('currentPeer().modes.length'),1);assert(!run("contextAllows('/api/mode',{value:'first-wins'})"));
assert(!run("contextAllows('/api/snake/direction',{value:'up'})"));assert(!run("contextAllows('/api/cells',{ids:'1,2'})"),'A Table ACK alone cannot enable bulk writes');
table.acknowledged_features.push('cell-batch');set({capabilities:table,mode:'cell'});assert(run("contextAllows('/api/cells',{ids:'1,2'})"));
assert(!run("contextAllows('/api/mode',{value:'idle'})"));assert(run("contextAllows('/api/led/output',{enabled:'true'})"));
table.acknowledged_features.push('inputs','outputs');set({capabilities:table,mode:'cell'});
assert.equal(run('currentPeer().modes.length'),2);assert(snakeControls.hidden);
assert(!run("contextAllows('/api/mode',{value:'snake'})"));
assert(run("contextAllows('/api/cell',{id:1})"));
assert.match(document.title,/Fixture board/);
const originalModes=boardModes.children;run('renderContext()');assert.equal(boardModes.children,originalModes,'Stable discovery does not replace focused mode widgets');
context.cellManager.open=true;context.cellBatchCancelled=false;now+=8001;run('renderContext()');
assert(cards.every(n=>n.hidden));assert(context.cellBatchCancelled);assert(closed>0&&stops>0&&cancelledOrders>0);
assert(!run("contextAllows('/api/mode',{value:'snake'})"));
set({capabilities:{...table,board_kind:'controller'}});assert(!visible('table'));assert.equal(boardModes.children.length,0);
set({capabilities:{...table,table:{columns:9,rows:8,pixels_per_cell:6}}});assert(!visible('table'),'Unsupported geometry is not forced into an 80-cell table');
set({capabilities:{...table,acknowledged_features:null}});assert(cards.every(n=>n.hidden));
set({capabilities:{...table,extra_optional:{future:true}}});assert(visible('table'),'Additive metadata does not select another generation');
// Exercise the actual dynamically created switch, not the old early return.
set({capabilities:manifest(['leds']),led_output:{enabled:true}});
const output=nodes.get('ledOutputControl'),toggle=nodes.get('ledOutputEnabled');
assert(output&&toggle);assert.equal(output.parentElement,ledCard,'Generic LED service is not owned by the Table modes card');
assert(!ledCard.hidden&&tableCard.hidden&&!output.hidden);assert.equal(toggle.checked,true);
assert.equal(output.children[1].textContent,'Output');context.uiLanguagePreference='fa';run('applyLocalization()');
assert.equal(output.children[1].textContent,'خروجی','Dynamic caption participates in actual live-localization ownership');
context.uiLanguagePreference='en';run('applyLocalization()');assert.equal(output.children[1].textContent,'Output');
for(const led_output of [undefined,null,{},true,{enabled:'true'},{enabled:1}]){set({capabilities:manifest(['leds']),led_output});assert(output.hidden)}
set({capabilities:manifest([]),led_output:{enabled:true}});assert(output.hidden,'Output metadata alone does not authorize the service');
set({capabilities:manifest(['leds']),led_output:{enabled:true}});writable=false;run('renderContext()');assert(toggle.disabled);writable=true;run('renderContext()');assert(!toggle.disabled);
const sent=[];let finishPost;
context.post=(path,values)=>{sent.push({path,values});return new Promise(resolve=>finishPost=resolve)};
toggle.checked=false;const pending=toggle.onchange();assert.equal(toggle.checked,true,'Request does not optimistically change observed On');assert(toggle.disabled);assert.equal(sent.length,1);assert.equal(sent[0].path,'/api/led/output');assert.equal(sent[0].values.enabled,'false');
await toggle.onchange();assert.equal(sent.length,1,'Pending output requests cannot duplicate');finishPost();await pending;assert.equal(toggle.checked,true,'Completion alone is not output readback');
set({capabilities:manifest(['leds']),led_output:{enabled:false}});assert.equal(toggle.checked,false);toggle.checked=true;const resume=toggle.onchange();assert.equal(sent[1].values.enabled,'true');assert.equal(toggle.checked,false);
set({capabilities:manifest([])});assert(output.hidden);finishPost();await resume;assert(output.hidden,'Late completion cannot revive a revoked peer control');
set({capabilities:manifest(['leds']),led_output:{enabled:false}});now+=8001;run('renderContext()');assert(output.hidden,'Expired output state is never presented as a current Off');
set({capabilities:manifest(['leds']),led_output:{enabled:true,mode:'static'}});
const modeControl=nodes.get('ledOutputModeControl'),modeSelect=nodes.get('ledOutputMode'),rainbowButton=nodes.get('rainbowTest');
assert(modeControl&&!modeControl.hidden);assert.equal(modeControl.parentElement,ledCard);
assert.equal(modeSelect.value,'static');assert.match(nodes.get('ledOutputModeHint').textContent,/six matching pixels.*Rainbow test animates explicitly/);
modeSelect.value='effects';const modePending=modeSelect.onchange();
assert.equal(modeSelect.value,'static','Draft mode is not observed output');assert(modeSelect.disabled&&rainbowButton.disabled);
assert.equal(sent.at(-1).path,'/api/led/output');assert.deepEqual({...sent.at(-1).values},{mode:'effects'});
const beforeDuplicate=sent.length;await modeSelect.onchange();await rainbowButton.onclick();assert.equal(sent.length,beforeDuplicate);
finishPost();await modePending;assert.equal(modeSelect.value,'static','ACK alone cannot enable observed animation');
set({capabilities:manifest(['leds']),led_output:{enabled:true,mode:'effects'}});assert.equal(modeSelect.value,'effects');
set({capabilities:manifest(['leds']),led_output:{enabled:false,mode:'static'}});
const rainbowPending=rainbowButton.onclick();assert.equal(sent.at(-1).path,'/api/effect');assert.deepEqual({...sent.at(-1).values},{value:'rainbow'});
assert.equal(modeSelect.value,'static');assert.equal(toggle.checked,false,'Rainbow test neither enables output nor switches mode');
finishPost();await rainbowPending;
for(const mode of [undefined,null,'future',1]){set({capabilities:manifest(['leds']),led_output:{enabled:true,mode}});assert(modeControl.hidden,'Unknown modes are not fabricated as static')}
set({capabilities:manifest([]),led_output:{enabled:true,mode:'static'}});assert(modeControl.hidden);
const native=readFileSync(new URL('../desktop/src/ui/shell.rs',import.meta.url),'utf8');
const nativeStatus=readFileSync(new URL('../desktop/src/ui/status_bar.rs',import.meta.url),'utf8');
for(const gate of ['status.avr_available','status.display_available','status.network_available','status.telemetry_available'])assert(nativeStatus.includes(gate));
assert.match(source, /data-capability="display" hidden><h2>Live display<\/h2>[^\n]+id="displayValue"/,'Numeric display controls require Display, not Text');
for(const feature of ['display','text','buzzer'])assert(native.includes(`if self.has_feature("${feature}")`));
const nativeLogic=native.slice(native.indexOf('pub fn native_logic('),native.indexOf('pub fn draw_shell('));
assert(nativeLogic.includes('self.synchronize_context(ctx)'), 'Capability authority must update without painting');
assert(!native.slice(native.indexOf('fn draw_shell_contents(')).includes('self.synchronize_context('), 'No second authority update during painting');
assert(readFileSync(new URL('../desktop/src/main.rs',import.meta.url),'utf8').includes('self.native_logic(ctx)'));
assert(native.includes('panel.available(self.peer_context())'));
assert(!native.includes('("Snake", "snake", 3)'),'Native mode UI is not a fixed table list');
console.log('Actual Web peer context and output control: no-ACK/offline, independent ACKs, generic LED placement, pending/no-optimism/readback, live localization, mode filtering, stale revocation and cross-board rebind passed. Native wiring is source-checked only.');
