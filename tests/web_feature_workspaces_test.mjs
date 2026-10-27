import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import vm from 'node:vm';
const source=await readFile(new URL('../assets/web/feature-workspaces.js',import.meta.url),'utf8');
class Element{
 constructor(tag){this.tagName=tag;this.children=[];this.dataset={};this.attributes={};this.style={};this.value='';this.textContent='';this.disabled=false;this.hidden=false;this.classList={toggle(){},add(){}};}
 get value(){return this._value;}set value(value){this._value=String(value);}
 get textContent(){return this._text||'';}set textContent(value){this._text=String(value);this.textWrites=(this.textWrites||0)+1;}
 append(...nodes){for(const n of nodes){n.parentElement=this;this.children.push(n);if(this.tagName==='select'&&this.children.length===1)this.value=n.value;}}
 replaceChildren(...nodes){this.children=[];this.append(...nodes);}
 setAttribute(key,value){this.attributes[key]=value;}getAttribute(key){return this.attributes[key];}
 querySelectorAll(selector){const choices=selector.split(',');return this.children.flatMap(n=>[n,...n.querySelectorAll('*')]).filter(n=>choices.some(choice=>choice==='*'||choice===n.tagName||choice==='[data-open-corrections]'&&n.dataset.openCorrections));}
 querySelector(selector){return this.querySelectorAll(selector)[0]||null;}
 checkValidity(){const n=Number(this.value);return this.type!=='number'||this.value!==''&&Number.isInteger(n)&&n>=Number(this.min)&&n<=Number(this.max);}
 reportValidity(){return this.checkValidity();}scrollIntoView(){this.scrolls=(this.scrolls||0)+1;}focus(){}remove(){if(this.parentElement)this.parentElement.children=this.parentElement.children.filter(n=>n!==this);}
}
const layout=new Element('div'),listeners=new Map();
const document={createElement:tag=>new Element(tag),querySelector:()=>layout,querySelectorAll:selector=>layout.querySelectorAll(selector),getElementById:id=>layout.querySelectorAll('*').find(n=>n.id===id)||null};
const modalRequests=[];
const context=vm.createContext({document,navigator:{},setTimeout,clearTimeout,confirm:()=>{throw Error('Blocking browser confirm must not be used');},addEventListener:(name,fn)=>listeners.set(name,fn),removeEventListener:name=>listeners.delete(name)});
context.WebUI={dialog(spec){let onClose,closed=false;const prompt={spec,element:{addEventListener(type,callback){assert.equal(type,'close');onClose=callback;}},close(){if(closed)return;closed=true;onClose?.();}};modalRequests.push(prompt);return prompt;}};
const approveReload=async prompt=>{await prompt.spec.actions[1].action();prompt.close();};
vm.runInContext(await readFile(new URL('../assets/web/pixel-groups.js',import.meta.url),'utf8'),context);
vm.runInContext(source,context);const contracts=context.WebFeatureContracts;
vm.runInContext(await readFile(new URL('../assets/web/correction-preview.js',import.meta.url),'utf8'),context);
const configuration={revision:0,pixels:6,index_base:0,enabled:true,rules:[],sources:[0,1,2,3,4,5]};
assert.equal(contracts.parseCorrections(configuration).pixels,6);
const disabledRules={...configuration,rules:[{kind:'black',pixel:0,enabled:false},{kind:'shift',pixel:1,offset:1}]};
const normalizedRules=contracts.parseCorrections(disabledRules);
assert.equal(normalizedRules.rules[0].enabled,false);
assert.equal(normalizedRules.rules[1].enabled,true,'Only absent rule flags default to enabled');
for(const enabled of [null,0,1,'false'])assert.throws(()=>contracts.parseCorrections({...configuration,rules:[{kind:'black',pixel:0,enabled}]}));
assert.equal(contracts.correctionsMatch(normalizedRules,{...normalizedRules,rules:normalizedRules.rules.map(rule=>({...rule,enabled:true}))}),false,'Readback must verify per-rule flags');
for(const bad of [{...configuration,revision:-1},{...configuration,index_base:1},{...configuration,pixels:481},{...configuration,rules:[{kind:'black',pixel:0,offset:1}]},{...configuration,sources:[6,1,2,3,4,5]}])assert.throws(()=>contracts.parseCorrections(bad));
assert.deepEqual(Array.from(contracts.correctionSources(6,true,[{kind:'remove',pixel:1}])),[0,2,3,4,5,-1]);
assert.deepEqual(Array.from(contracts.correctionSources(6,true,[{kind:'shift',pixel:1,offset:2}])),[0,-1,-1,1,2,3]);
assert.deepEqual(Array.from(contracts.correctionSources(6,true,[{kind:'shift',pixel:3,offset:-2}])),[0,3,4,5,-1,-1]);
assert.deepEqual(Array.from(contracts.correctionSources(6,false,[{kind:'black',pixel:0}])),[0,1,2,3,4,5]);
assert.deepEqual(Array.from(contracts.correctionSources(6,true,[{kind:'black',pixel:0,enabled:false}])),[0,1,2,3,4,5]);
const windowRules=[{kind:'black',pixel:0},{kind:'remove',pixel:1},{kind:'black',pixel:4}];
const ruleWindow=contracts.correctionWindow(6,true,windowRules,1);assert.equal(ruleWindow.start,0);assert.equal(ruleWindow.focus,1);assert.deepEqual(Array.from(ruleWindow.sources),[-1,2,3,4,5,-1],'Rule window includes its prefix, not later rules');
assert.deepEqual(Array.from(contracts.correctionWindow(6,false,windowRules,1).sources),[0,1,2,3,4,5]);
assert.deepEqual(Array.from(contracts.correctionWindow(3,true,[{kind:'black',pixel:1,enabled:false}],0).sources),[0,1,2]);
assert.equal(contracts.correctionWindow(480,true,[{kind:'shift',pixel:479,offset:-1}],0).start,474);assert.equal(contracts.correctionWindow(480,true,[{kind:'black',pixel:0}],0).start,0);
for(const index of [-1,3,1.5])assert.throws(()=>contracts.correctionWindow(6,true,windowRules,index));
const rgbFrame={revision:1,pending:false,brightness:255,rgb:'ff000000ff000000ffffffff000000102030'};
const rgbState={pixels:6,pixel_frame:rgbFrame};
assert.deepEqual(Array.from(contracts.correctionRgb(rgbState,6)),[0xff0000,0x00ff00,0x0000ff,0xffffff,0,0x102030]);
assert.equal(contracts.correctionRgb(rgbState,6,undefined,false),null,'Retired peer RGB cannot color a mapping');
for(const bad of [{pending:true},{pending:undefined},{revision:0},{revision:0x100000000},{brightness:256},{rgb:'ff00xx'},{rgb:'f'.repeat(35)}])assert.equal(contracts.correctionRgb({...rgbState,pixel_frame:{...rgbFrame,...bad}},6),null);
assert.equal(contracts.correctionRgb({...rgbState,pixels:480},6),null);assert.equal(contracts.correctionInk(0xffffff),'#000000');assert.equal(contracts.correctionInk(0),'#ffffff');assert.equal(contracts.correctionInk(0xff0000),'#ffffff');
const addressMap={physical:Array.from({length:80},(_,i)=>80-i),names:Array(80).fill('')};
const addressArgs={wire:0,pixels:480,enabled:true,rules:[],state:{pixels:480},role:'table-selection',mapping:addressMap};
assert.equal(contracts.correctionAddress(addressArgs).logical,80,'Rule output slots use the actual logical permutation');
assert.equal(contracts.correctionAddress({...addressArgs,current:false}).reason,'not-current');
assert.equal(contracts.correctionAddress({...addressArgs,mapping:{physical:Array(80).fill(1)}}).reason,'no-logical-map');
assert.equal(contracts.correctionAddress({...addressArgs,rules:[{kind:'black',pixel:0}]}).reason,'black');
assert.equal(contracts.correctionAddress({...addressArgs,wire:479,rules:[{kind:'remove',pixel:0}]}).reason,'black');
const movedAddress=contracts.correctionAddress({...addressArgs,wire:4,rules:[{kind:'remove',pixel:0},{kind:'remove',pixel:0}]});
assert.equal(movedAddress.source,6);assert.equal(movedAddress.group,2);assert.equal(movedAddress.logical,79);
const keyAddress=contracts.correctionAddress({wire:96,pixels:216,enabled:true,rules:[],state:{pixels:216},role:'first-wins'});
assert.equal(keyAddress.box,5);assert.equal(keyAddress.part,'button');assert.equal(keyAddress.count,12);
assert.equal(contracts.correctionAddress({wire:108,pixels:216,enabled:true,rules:[],state:{pixels:216},role:'first-wins'}).part,'indicator');
const sized=context.PixelGroups.parse({...context.PixelGroups.defaults('table-selection'),revision:4,overrides:[{group:1,count:5},{group:2,count:7}]},'table-selection');
const sizedState={pixels:480,pixel_groups:sized,capabilities:{acknowledged_features:['pixel-group-layout']}};
const capturedColors={...rgbFrame,rgb:'112233'.repeat(480),group_layout_revision:4,group_counts:Array.from(sized.counts),group_starts:Array.from(sized.starts)};
assert.equal(contracts.correctionRgb({...sizedState,pixel_frame:capturedColors},480,'table-selection').length,480);
assert.equal(contracts.correctionRgb({...sizedState,pixel_frame:{...capturedColors,group_layout_revision:3}},480,'table-selection'),null,'Stale captured topology cannot color a current source map');
assert.equal(contracts.correctionRgb({...sizedState,pixel_frame:{...capturedColors,group_counts:Array(80).fill(6),group_starts:Array.from({length:80},(_,i)=>i*6)}},480,'table-selection'),null,'Same revision with different group counts fails closed');
assert.equal(contracts.correctionAddress({...addressArgs,wire:5,state:sizedState,mapping:{...addressMap,pixel_groups:sized}}).logical,79);
assert.equal(contracts.correctionAddress({...addressArgs,wire:5,state:sizedState,mapping:{...addressMap,pixel_groups:{...sized,revision:3}}}).reason,'no-logical-map','Stale group layouts cannot label logical cells');
assert.equal(contracts.correctionAddress({...addressArgs,state:{pixels:480,capabilities:{acknowledged_features:['pixel-group-layout']}}}).reason,'unavailable');
assert.equal(JSON.stringify(contracts.redact({token:'secret',token_set:true,nested:{password:'secret'}})),JSON.stringify({token:'[redacted]',token_set:true,nested:{password:'[redacted]'}}));
const audio={avr_online:true,audio_cues:['winner','new-board-melody','winner','<script>',1,'x'.repeat(41)]};
assert.equal(JSON.stringify(contracts.audioCues(audio)),JSON.stringify(['winner','new-board-melody']),'Advertised valid new cues are not discarded by a hardcoded client list');
assert.equal(JSON.stringify(contracts.audioCues(audio,'New board')),JSON.stringify(['new-board-melody']));
assert.equal(contracts.audioCues({...audio,avr_online:false}).length,0);
assert.equal(contracts.audioCues({...audio,audio_cues:Array.from({length:100},(_,i)=>'cue-'+i)}).length,64);

let state={board_id:'board',boot_id:'first',pixel_corrections_revision:0},saved=structuredClone(configuration),reads=0,writes=0,lastPayload,active=true;
const features=new Set(['identity','pixel-corrections']),errors=[];
const routed=[],fallbackRoutes=[];context.studioReveal=target=>fallbackRoutes.push(target);
const options={layout,reveal:target=>routed.push(target),snapshot:()=>state,peer:()=>({known:active,has:feature=>active&&features.has(feature)}),pin:()=>({board:'board',boot:state.boot_id}),report:error=>errors.push(error),
 api:async(path)=>{assert.equal(path,'/api/pixels/corrections');reads++;return structuredClone(saved);},
 command:async(path,data)=>{assert.equal(path,'/api/pixels/corrections');writes++;lastPayload=JSON.parse(data.configuration);saved={...saved,...lastPayload,revision:saved.revision+1};}};
const workspaces=context.createFeatureWorkspaces(options);await new Promise(setImmediate);
assert.equal(context.createFeatureWorkspaces(options),workspaces,'Initialization is idempotent, not a second runtime');
assert.equal(layout.children.length,4,'Capability-gated group editor joins the shared pixel workspace');
assert.equal(reads,1);assert.equal(writes,0,'Opening and painting cannot alter LEDs or flash');
for(let i=0;i<10;i++)workspaces.update();await new Promise(setImmediate);assert.equal(reads,1,'Status pushes do not create a new poll loop');
const corrections=document.getElementById('pixelCorrectionsCard');
const buttons=()=>corrections.querySelectorAll('button'),find=label=>buttons().find(button=>button.textContent===label);
const ruleRegion=corrections.querySelectorAll('div').find(n=>n.getAttribute('role')==='region');
assert.equal(ruleRegion.getAttribute('aria-label'),'Correction rules');assert.equal(ruleRegion.tabIndex,0);
const actionToolbar=find('Add rule').parentElement.parentElement;
assert.equal(actionToolbar.className,'correction-toolbar');
assert.equal(find('Save to board').parentElement.parentElement,actionToolbar);
assert.equal(find('Reload').parentElement.parentElement,actionToolbar);
assert.equal(ruleRegion.parentElement,actionToolbar.parentElement,'Actions and scrolling rules are siblings, not nested');
assert.equal(ruleRegion.parentElement.children.indexOf(actionToolbar)+1,ruleRegion.parentElement.children.indexOf(ruleRegion));
features.add('pixel-diagnostic');workspaces.update();await find('Pixel diagnostics').onclick();
assert.equal(routed.length,1);assert.equal(routed[0].id,'pixelDiagnosticsCard');assert.equal(routed[0].scrolls,1);
assert.equal(fallbackRoutes.length,0,'Injected navigation must not be followed by a second targetless fallback that opens All panels');
await document.getElementById('pixelDiagnosticsCard').querySelectorAll('button').find(b=>b.textContent==='Pixel corrections').onclick();
assert.equal(routed.length,2);assert.equal(routed[1],corrections);assert.equal(corrections.scrolls,1);assert.equal(fallbackRoutes.length,0);
features.delete('pixel-diagnostic');workspaces.update();
const enable=corrections.querySelectorAll('input').find(n=>n.type==='checkbox');enable.checked=false;enable.onchange();
assert.equal(find('Save to board').disabled,false);await find('Save to board').onclick();
assert.equal(writes,1);assert.equal(reads,2);assert.equal(lastPayload.revision,0);assert.equal(lastPayload.enabled,false);assert.equal(find('Save to board').disabled,true);
await find('Add rule').onclick();state.pixel_corrections_revision=2;saved={...saved,revision:2};workspaces.update();
assert.equal(find('Save to board').disabled,true,'Remote revision conflicts preserve drafts and block stale saves');assert.equal(reads,2,'A dirty editor is not silently reloaded');
const cancelledReload=find('Reload').onclick();assert.equal(modalRequests.length,1);
assert.equal(modalRequests[0].spec.title,'Discard correction edits?');
assert.equal(modalRequests[0].spec.actions[0].label,'Cancel');
modalRequests[0].close();await cancelledReload;assert.equal(reads,2,'Dismissal preserves the draft without reading');
const approvedReload=find('Reload').onclick();await find('Reload').onclick();
assert.equal(modalRequests.length,2,'Only one confirmation may own the editor');
await approveReload(modalRequests[1]);await approvedReload;assert.equal(reads,3);assert.equal(find('Save to board').disabled,true);
active=false;workspaces.update();assert.equal(corrections.hidden,true);assert.equal(find('Add rule').disabled,true);
active=true;workspaces.update();assert.equal(corrections.hidden,false);
assert.equal(writes,1);assert.equal(errors.length,0);
workspaces.dispose();assert.equal(layout.children.length,0);assert.equal(listeners.has('beforeunload'),false);
// An in-flight old-boot read cannot overwrite the replacement session's editor.
const deferred=[];state={board_id:'board',boot_id:'old',pixel_corrections_revision:0};
const switched=context.createFeatureWorkspaces({...options,api:()=>new Promise(resolve=>deferred.push(resolve))});
assert.equal(deferred.length,1);state={board_id:'board',boot_id:'new',pixel_corrections_revision:4};switched.update();assert.equal(deferred.length,2);
deferred[0]({...configuration,revision:999});await new Promise(setImmediate);
deferred[1]({...configuration,revision:4});await new Promise(setImmediate);
assert(document.getElementById('pixelCorrectionsCard').querySelectorAll('span').some(n=>n.textContent==='Revision 4'));
assert(!document.getElementById('pixelCorrectionsCard').querySelectorAll('span').some(n=>n.textContent==='Revision 999'));
switched.dispose();assert.equal(writes,1);
// A dialog is retired on peer/boot change or disposal. Its old confirmation
// cannot reload the new peer, discard the old draft or perform a write.
state={board_id:'board',boot_id:'prompt-old',pixel_corrections_revision:0};saved=structuredClone(configuration);
const promptWorkspace=context.createFeatureWorkspaces(options);await new Promise(setImmediate);
const promptCard=document.getElementById('pixelCorrectionsCard'),promptButton=label=>promptCard.querySelectorAll('button').find(b=>b.textContent===label);
await promptButton('Add rule').onclick();const staleReload=promptButton('Reload').onclick(),stalePrompt=modalRequests.at(-1),beforeSwitchReads=reads;
state={...state,boot_id:'prompt-new'};promptWorkspace.update();await staleReload;await new Promise(setImmediate);
assert.equal(reads,beforeSwitchReads+1,'Only the new session initial read runs');
await approveReload(stalePrompt);assert.equal(writes,1);
state={...state,boot_id:'prompt-old'};promptWorkspace.update();assert.equal(promptButton('Save to board').disabled,false,'Original unsaved draft survives');
const disconnectedReload=promptButton('Reload').onclick();active=false;promptWorkspace.update();await disconnectedReload;
active=true;promptWorkspace.update();assert.equal(promptButton('Save to board').disabled,false,'Disconnect cancels only the prompt');
const sharedUi=context.WebUI;context.WebUI={};const noModalReads=reads;
await promptButton('Reload').onclick();assert.equal(reads,noModalReads);assert.equal(promptButton('Save to board').disabled,false,'Missing dialog fails closed and keeps edits');
assert(errors.at(-1).message.includes('confirmation is unavailable'));context.WebUI=sharedUi;
const disposedReload=promptButton('Reload').onclick();promptWorkspace.dispose();await disposedReload;
assert.equal(writes,1);assert.equal(layout.children.length,0);
// Disabling, reordering and saving preserve each row's flag and still require
// an explicit commit. A checkbox is not an empty numeric draft.
const rowErrorsBefore=errors.length;
state={board_id:'board',boot_id:'row-flags',pixel_corrections_revision:0};saved=structuredClone(disabledRules);
const rowWorkspace=context.createFeatureWorkspaces({...options,reveal:undefined});await new Promise(setImmediate);
const rowCard=document.getElementById('pixelCorrectionsCard');
const rowButton=label=>rowCard.querySelectorAll('button').find(button=>button.textContent===label);
const rowPreviews=()=>rowCard.querySelectorAll('details').filter(n=>n.className==='correction-rule-preview'),rowDiagrams=()=>rowPreviews().map(n=>n.querySelector('div'));
assert.equal(rowPreviews().length,2);assert(rowDiagrams().every(n=>n.children.length===0),'Closed per-rule diagrams are lazy');
const previewReads=reads,previewWrites=writes;for(const view of rowPreviews()){view.open=true;view.ontoggle();}
assert.deepEqual(rowDiagrams()[0].children.map(n=>Number(n.dataset.source)),[0,1,2,3,4,5],'Disabled first rule previews unchanged sources');
assert.deepEqual(rowDiagrams()[1].children.map(n=>Number(n.dataset.source)),[0,-1,1,2,3,4]);assert.equal(rowDiagrams()[1].children[1].dataset.focused,'true');
state={...state,...rgbState};rowWorkspace.update();const retainedPreviewCircle=rowDiagrams()[1].children[2];assert.equal(retainedPreviewCircle.children[1].style.backgroundColor,'#00ff00');
state={...state,pixel_frame:{...rgbFrame,revision:2,rgb:'112233'.repeat(6)}};rowWorkspace.update();assert.equal(rowDiagrams()[1].children[2],retainedPreviewCircle,'Per-rule live colors do not rebuild the six-slot window');assert.equal(retainedPreviewCircle.children[1].style.backgroundColor,'#112233');
assert.equal(reads,previewReads);assert.equal(writes,previewWrites,'Opening and refreshing a local rule diagram sends no requests');
assert.equal(rowButton('Save to board').disabled,true,'Expanding a local diagram does not dirty correction data');
features.add('pixel-diagnostic');rowWorkspace.update();await rowButton('Pixel diagnostics').onclick();
assert.equal(fallbackRoutes.length,1);assert.equal(fallbackRoutes[0].id,'pixelDiagnosticsCard','Fallback receives the exact target too');
features.delete('pixel-diagnostic');rowWorkspace.update();
const ruleToggles=()=>rowCard.querySelectorAll('input').filter(n=>n.getAttribute('aria-label')?.startsWith('Enable rule '));
assert.equal(ruleToggles().length,2);assert.equal(ruleToggles()[0].checked,false);assert.equal(ruleToggles()[1].checked,true);
const addressHints=rowCard.querySelectorAll('p').filter(n=>n.className==='correction-address'),hintWrites=addressHints.map(n=>n.textWrites);
for(let i=0;i<10;i++){state={...state,brightness:i};rowWorkspace.update();}
assert.deepEqual(addressHints.map(n=>n.textWrites),hintWrites,'Unrelated live status does not recompute or repaint correction labels');
const firstPixel=rowCard.querySelectorAll('input').find(n=>n.getAttribute('aria-label')==='Pixel · 1');
assert.equal(rowButton('Decrease pixel').disabled,true);await rowButton('Increase pixel').onclick();
assert.equal(firstPixel.value,'2');assert.equal(rowButton('Decrease pixel').disabled,false);
firstPixel.value='';firstPixel.oninput();assert.equal(rowCard.querySelectorAll('p').filter(n=>n.className==='correction-address')[0].textContent,'Complete the pixel address');assert(rowDiagrams().every(n=>n.children.length===0),'Invalid prior input clears its own and dependent row previews');
await rowButton('Decrease pixel').onclick();assert.equal(firstPixel.value,'1','Step recovers invalid local input from last valid draft');
const lastPixel=firstPixel;lastPixel.value='6';lastPixel.oninput();assert.equal(rowButton('Increase pixel').disabled,true);
lastPixel.value='1';lastPixel.oninput();
const offsetInput=rowCard.querySelectorAll('input').find(n=>n.getAttribute('aria-label')==='Offset · 2');
const offsetLess=rowCard.querySelectorAll('button').find(n=>n.getAttribute('aria-label')==='Decrease offset · 2');
await offsetLess.onclick();assert.equal(offsetInput.value,'0','Signed offset steps through zero');
offsetInput.value='-6';offsetInput.oninput();assert.equal(offsetLess.disabled,true);offsetInput.value='1';offsetInput.oninput();
assert.equal(writes,1,'Number stepping changes only local drafts');
ruleToggles()[0].checked=true;ruleToggles()[0].onchange();ruleToggles()[0].checked=false;ruleToggles()[0].onchange();
assert.equal(writes,1,'Toggling rows does not apply or save');
await rowButton('Move down').onclick();
assert.equal(ruleToggles()[1].checked,false,'Disabled state follows a reordered rule');
assert(rowPreviews().every(view=>view.open),'Expanded local views follow their rule through reorder');
await rowButton('Save to board').onclick();
assert.equal(writes,2);assert.equal(lastPayload.rules[0].enabled,true);assert.equal(lastPayload.rules[1].enabled,false);
assert(lastPayload.rules.every(rule=>!Object.hasOwn(rule,'previewOpen')),'Presentation-only disclosure state never enters the board payload');
assert.equal(rowButton('Save to board').disabled,true,'Exact row-flag readback confirms the save');
rowWorkspace.dispose();assert.equal(errors.length,rowErrorsBefore);
// The same typed diagnostic frame works for table and first-wins boards. No raw
// RGB stream, renderer guess or uncorrelated status is used as completion proof.
const observation=(revision,index=0,count=1,rgb=0xffffff,off=false,extra={})=>({revision,index,count,rgb,off,active:true,...extra});
const diagnosticState=(revision=1,extra={})=>({board_id:'pixels',boot_id:'one',pixels:480,led_output:{diagnostic:observation(revision)},...extra});
assert.equal(contracts.parseDiagnostic(diagnosticState(),'table-selection').group,6);
assert.equal(contracts.parseDiagnostic(diagnosticState(1,{pixels:216}),'first-wins').group,12);
for(const bad of [observation(0),observation(1,480),observation(1,1,6),observation(1,0,12),observation(1,0,1,0x1000000),observation(1,0,1,0,false,{active:1})])assert.equal(contracts.parseDiagnostic(diagnosticState(1,{led_output:{diagnostic:bad}}),'table-selection'),null);
assert.equal(contracts.parseDiagnostic(diagnosticState(),'first-wins'),null,'Board pixel count must match its acknowledged role');
// Cursor import copies the selected output slot only. It neither follows a
// projected raw source nor actuates diagnostics, applies rules or writes flash.
state=diagnosticState(1,{led_output:{diagnostic:observation(1,20)}});
saved={...configuration,pixels:480,sources:Array.from({length:480},(_,i)=>i),rules:[{kind:'remove',pixel:0,offset:0,enabled:true},{kind:'black',pixel:5,offset:0,enabled:true}]};
features.clear();for(const f of ['identity','pixel-corrections','pixel-diagnostic','command-ack','leds'])features.add(f);
const cursorWritesBefore=writes,cursorWorkspace=context.createFeatureWorkspaces({...options,peer:()=>({known:active,role:'table-selection',has:f=>active&&features.has(f)})});await new Promise(setImmediate);
const cursorCorrectionCard=()=>document.getElementById('pixelCorrectionsCard');
const cursorImports=()=>cursorCorrectionCard().querySelectorAll('button').filter(n=>n.textContent==='Use selected cursor');
const cursorRulePixel=()=>cursorCorrectionCard().querySelectorAll('input').find(n=>n.getAttribute('aria-label')==='Pixel · 2');
const selectedCursor=document.getElementById('pixelDiagnosticsCard').querySelectorAll('input').find(n=>n.type==='number');
assert.equal(cursorImports()[1].disabled,false);assert.equal(cursorImports()[1].getAttribute('aria-label'),'Use selected cursor (21) · 2');
await cursorImports()[1].onclick();assert.equal(cursorRulePixel().value,'21','Use wire cursor 21, not projected raw pixel 22');
selectedCursor.value='34';selectedCursor.oninput();assert.equal(cursorImports()[1].getAttribute('aria-label'),'Use selected cursor (34) · 2');
state={...state,led_output:{diagnostic:observation(2,10)}};cursorWorkspace.update();
await cursorImports()[1].onclick();assert.equal(cursorRulePixel().value,'34','An unsent selected cursor survives another client live update');
const cursorMap=cursorCorrectionCard().querySelectorAll('div').find(n=>n.className==='correction-map'),cursorDisclosure=cursorCorrectionCard().querySelectorAll('details').find(n=>n.className==='mapping-disclosure');cursorDisclosure.open=true;cursorDisclosure.ontoggle();
assert.equal(cursorMap.children[33].dataset.focused,'true','The map highlights the selected output cursor, not its raw source');
selectedCursor.value='';selectedCursor.oninput();assert.equal(cursorImports()[1].disabled,true);assert(cursorMap.children.every(n=>n.dataset.focused==='false'),'Clearing cursor text immediately retires its prior map focus');
await cursorImports()[1].onclick();assert.equal(cursorRulePixel().value,'34','Invalid selected input never copies the old cursor');
features.delete('pixel-diagnostic');cursorWorkspace.update();assert.equal(cursorImports()[1].hidden,true);features.add('pixel-diagnostic');
const lateCursorImport=cursorImports()[1];state={...state,boot_id:'cursor-replacement',led_output:{diagnostic:observation(1,40)}};cursorWorkspace.update();await new Promise(setImmediate);
await lateCursorImport.onclick();assert.equal(cursorRulePixel().value,'6','Retired row callback cannot edit the replacement peer draft');
assert.equal(cursorCorrectionCard().querySelectorAll('button').find(n=>n.textContent==='Save to board').disabled,true);
assert.equal(writes,cursorWritesBefore,'Cursor import sends zero diagnostic or configuration commands');cursorWorkspace.dispose();
state=diagnosticState();features.clear();for(const f of ['identity','pixel-diagnostic','command-ack','leds'])features.add(f);
let role='table-selection',diagnosticWrites=0,diagnosticReads=0,lastDiagnostic,delayCommand=null;
const diagnosticOptions={...options,peer:()=>({known:active,role,has:feature=>active&&features.has(feature)}),
 api:async(path)=>{assert.equal(path,'/api/status');diagnosticReads++;return structuredClone(state);},
 command:async(path,values)=>{diagnosticWrites++;lastDiagnostic={path,values:structuredClone(values)};if(delayCommand)return delayCommand();
   if(path==='/api/effect')state={...state,effect:'rainbow',led_output:{diagnostic:{...state.led_output.diagnostic,revision:state.led_output.diagnostic.revision+1,active:false}}};
   else{assert.equal(path,'/api/led/diagnostic');state={...state,led_output:{diagnostic:{...values,active:true,revision:state.led_output.diagnostic.revision+1}}};}
   diagnostics.update();return {status:structuredClone(state)};
 }};
const diagnostics=context.createFeatureWorkspaces(diagnosticOptions),diagnosticCard=document.getElementById('pixelDiagnosticsCard');
const diagnosticButton=label=>diagnosticCard.querySelectorAll('button').find(b=>b.textContent===label);
const cursor=diagnosticCard.querySelectorAll('input').find(n=>n.type==='number'),probeColor=diagnosticCard.querySelectorAll('input').find(n=>n.type==='color'),step=diagnosticCard.querySelector('select');
assert.equal(diagnosticCard.hidden,false);assert.equal(diagnosticWrites,0);assert.equal(diagnosticReads,0);
cursor.value='7';cursor.oninput();probeColor.value='#00ff00';probeColor.oninput();
state={...state,led_output:{diagnostic:observation(2,20,1,0xff0000)}};diagnostics.update();
assert.equal(cursor.value,'7','Remote events preserve an unsent local cursor');assert.equal(probeColor.value,'#00ff00');
await diagnosticButton('Use live cursor').onclick();assert.equal(cursor.value,'21');assert.equal(probeColor.value,'#ff0000');assert.equal(diagnosticWrites,0);
step.value='6';step.onchange();assert.equal(cursor.value,'19','Group selection aligns at the group boundary without sending');
await diagnosticButton('Next').onclick();assert.equal(lastDiagnostic.path,'/api/led/diagnostic');assert.deepEqual(lastDiagnostic.values,{index:24,count:6,rgb:0xff0000,off:false});assert.equal(cursor.value,'25');
await diagnosticButton('All off').onclick();assert.deepEqual(lastDiagnostic.values,{index:0,count:1,rgb:0,off:true});assert.equal(state.led_output.diagnostic.off,true);
await diagnosticButton('Rainbow').onclick();assert.equal(lastDiagnostic.path,'/api/effect');assert.equal(lastDiagnostic.values.value,'rainbow');assert.equal(state.effect,'rainbow');
assert.equal(diagnosticReads,0,'Correlated command status needs no follow-up polling');
role='first-wins';state=diagnosticState(1,{board_id:'keys',boot_id:'keyboot',pixels:216});diagnostics.update();
assert.equal(step.children[1].value,'12');step.value='12';step.onchange();await diagnosticButton('Next').onclick();assert.equal(lastDiagnostic.values.count,12);assert.equal(lastDiagnostic.values.index,12);
cursor.value='205';cursor.oninput();assert.equal(diagnosticButton('Next').disabled,true,'Last group is bounded to this board');
features.delete('command-ack');diagnostics.update();assert.equal(diagnosticButton('Show pixel').disabled,true);features.add('command-ack');
const beforeBad=diagnosticWrites;cursor.value='217';cursor.oninput();await diagnosticButton('Show pixel').onclick();assert.equal(diagnosticWrites,beforeBad,'Invalid index cannot write');
// Another client's newer revision stays authoritative even if our older receipt
// arrives afterward. The receipt confirms our intent, not ownership forever.
cursor.value='1';cursor.oninput();let delayedResolve;delayCommand=()=>new Promise(resolve=>delayedResolve=resolve);
const issued=diagnosticButton('Show pixel').onclick(),accepted={...structuredClone(state),led_output:{diagnostic:observation(10,0,12,0xffffff)}};
state={...state,led_output:{diagnostic:observation(11,24,12,0x0000ff)}};diagnostics.update();
delayedResolve({status:accepted});await issued;assert.equal(cursor.value,'25');assert.equal(probeColor.value,'#0000ff','Newer live revision wins over a late receipt');
// A late old-session reply must never alter the newly selected board or replay.
cursor.value='1';cursor.oninput();let resolveCommand;delayCommand=()=>new Promise(resolve=>resolveCommand=resolve);
const oldReply=structuredClone(state),sending=diagnosticButton('Show pixel').onclick();
state=diagnosticState(1,{board_id:'replacement',boot_id:'second',pixels:216});diagnostics.update();
resolveCommand({status:oldReply});await sending;assert.equal(cursor.value,'1');assert.equal(diagnosticWrites,beforeBad+2);
assert(errors.some(e=>e.message.includes('session changed')));
diagnostics.dispose();assert.equal(layout.children.length,0);
// Effects and overlay are rendered by the board, not animated/streamed here.
role='table-selection';state=diagnosticState();features.clear();for(const f of ['identity','pixel-diagnostic','command-ack','leds','pixel-diagnostic-effects','pixel-diagnostic-overlay'])features.add(f);
const optionDefaults={...contracts.diagnosticOptions()},extendedObservation={...observation(3,1),...optionDefaults,effect:'breathe',composite:'overlay',period_ms:700};
assert.equal(contracts.parseDiagnostic({...state,led_output:{diagnostic:extendedObservation}},role).effect,'breathe');
for(const bad of [{period_ms:99},{duty_percent:0},{min_brightness:200,max_brightness:10},{effect:'fire'},{composite:'unknown'},{enabled:'true'}])assert.equal(contracts.parseDiagnostic({...state,led_output:{diagnostic:{...extendedObservation,...bad}}},role),null);
const missingOption={...extendedObservation};delete missingOption.max_brightness;assert.equal(contracts.parseDiagnostic({...state,led_output:{diagnostic:missingOption}},role),null,'Partial extended observations fail closed');
let advancedWrites=0,advancedReads=0,advancedPacket,mismatchDiagnostic=false,pendingDiagnosticFrame=false,publishAfterRead=true,advancedDiagnostics;
advancedDiagnostics=context.createFeatureWorkspaces({...diagnosticOptions,api:async path=>{assert.equal(path,'/api/status?pixels=1');advancedReads++;return {...structuredClone(state),pixel_frame:{revision:advancedWrites,pending:!publishAfterRead}};},command:async(path,values)=>{assert.equal(path,'/api/led/diagnostic');advancedWrites++;advancedPacket=structuredClone(values);state={...state,pixel_frame:{revision:advancedWrites,pending:pendingDiagnosticFrame},led_output:{diagnostic:{...optionDefaults,...values,active:true,revision:state.led_output.diagnostic.revision+1,...(mismatchDiagnostic?{period_ms:1111}:{})}}};advancedDiagnostics.update();return {status:structuredClone(state)};}});
const advancedCard=document.getElementById('pixelDiagnosticsCard'),advancedButton=label=>advancedCard.querySelectorAll('button').find(b=>b.textContent===label),advancedDetails=advancedCard.querySelector('details');
const effectPicker=advancedCard.querySelectorAll('select').find(n=>n.children.some(o=>o.value==='breathe')),compositePicker=advancedCard.querySelectorAll('select').find(n=>n.children.some(o=>o.value==='overlay')),timing=label=>advancedCard.querySelectorAll('input').find(n=>n.getAttribute('aria-label')===label);
assert.equal(advancedDetails.hidden,false);effectPicker.value='blink';effectPicker.onchange();compositePicker.value='overlay';compositePicker.onchange();timing('Period · ms').value='800';timing('Period · ms').oninput();timing('Minimum brightness').value='20';timing('Minimum brightness').oninput();timing('Maximum brightness').value='220';timing('Maximum brightness').oninput();
assert.equal(advancedWrites,0,'Editing advanced probe options sends nothing');pendingDiagnosticFrame=true;await advancedButton('Show pixel').onclick();assert.equal(advancedReads,1,'Pending publication gets one pinned read, not another command');pendingDiagnosticFrame=false;assert.equal(advancedPacket.effect,'blink');assert.equal(advancedPacket.composite,'overlay');assert.equal(advancedPacket.period_ms,800);assert.equal(advancedPacket.min_brightness,20);assert.equal(advancedPacket.max_brightness,220);
state={...state,led_output:{diagnostic:{...state.led_output.diagnostic,revision:state.led_output.diagnostic.revision+1,effect:'rainbow',period_ms:600}}};advancedDiagnostics.update();assert.equal(effectPicker.value,'rainbow');assert.equal(timing('Period · ms').value,'600','New live options follow the board');
timing('Period · ms').value='';timing('Period · ms').oninput();assert(advancedButton('Show pixel').disabled);assert(!advancedButton('All off').disabled);const beforeInvalidOptions=advancedWrites;await advancedButton('Show pixel').onclick();assert.equal(advancedWrites,beforeInvalidOptions);
await advancedButton('All off').onclick();assert.equal(advancedPacket.off,true);assert.equal(advancedPacket.composite,'clear','All off never inherits overlay masking');assert.equal(advancedPacket.effect,'solid');assert.equal(advancedPacket.period_ms,1000,'All off bypasses invalid animation draft with safe defaults');
timing('Period · ms').value='600';timing('Period · ms').oninput();features.delete('pixel-diagnostic-overlay');advancedDiagnostics.update();const beforeNoOverlay=advancedWrites;await advancedButton('Show pixel').onclick();assert.equal(advancedWrites,beforeNoOverlay,'Unavailable overlay cannot silently become destructive solo');features.add('pixel-diagnostic-overlay');
mismatchDiagnostic=true;await advancedButton('Show pixel').onclick();assert(errors.at(-1).message.includes('Diagnostic result differs'),'ACK with changed effect timing is not success');
mismatchDiagnostic=false;pendingDiagnosticFrame=true;publishAfterRead=false;const beforeUnpublished=[advancedWrites,advancedReads];await advancedButton('Show pixel').onclick();assert.deepEqual([advancedWrites,advancedReads],[beforeUnpublished[0]+1,beforeUnpublished[1]+1]);assert(errors.at(-1).message.includes('frame publication is not confirmed'),'Unpublished frame never claims success or replays a mutation');
features.delete('pixel-diagnostic-effects');features.delete('pixel-diagnostic-overlay');advancedDiagnostics.update();assert.equal(advancedDetails.hidden,true,'No unacknowledged advanced surface is shown');advancedDiagnostics.dispose();assert.equal(layout.children.length,0);
// The existing workspace writer owns durable diagnostic drafts. A reload or
// reboot restores user intent only, not old observations or pending commands.
features.add('pixel-diagnostic-effects');features.add('pixel-diagnostic-overlay');
const durable=new Map(),scope=s=>s.board_id+':'+(s.capabilities?.application?.id||'table');let draftSaves=0,persistent;
const persistenceOptions={...diagnosticOptions,workspaceScope:scope,readWorkspace:key=>durable.get(key),workspaceChanged(){draftSaves++;durable.set(scope(state),persistent.workspaceSnapshot(scope(state)));}};
const existingDiagnosticWrites=diagnosticWrites,existingDiagnosticReads=diagnosticReads;
persistent=context.createFeatureWorkspaces(persistenceOptions);
let persistedCard=document.getElementById('pixelDiagnosticsCard');
const numberIn=(label)=>persistedCard.querySelectorAll('input').find(n=>n.getAttribute('aria-label')===label);
numberIn('Physical pixel').value='9';numberIn('Physical pixel').oninput();
const storedColor=persistedCard.querySelectorAll('input').find(n=>n.type==='color');storedColor.value='#102030';storedColor.oninput();
const storedEffect=persistedCard.querySelectorAll('select').find(n=>n.children.some(o=>o.value==='breathe'));storedEffect.value='breathe';storedEffect.onchange();
numberIn('Period · ms').value='';numberIn('Period · ms').oninput();
const durableKey=scope(state),savedProbe=JSON.parse(JSON.stringify(durable.get(durableKey)));
assert.equal(savedProbe.index,8);assert.equal(savedProbe.rgb,0x102030);assert.equal(savedProbe.effect,'breathe');assert.equal(savedProbe.timingDrafts.period_ms,'');assert(draftSaves>0);
assert.equal(persistent.workspaceSnapshot('another-board'),undefined,'A peer switch cannot snapshot the previous draft into the next board');
for(const key of ['observation','pending','revision','dirty','command','count'])assert(!Object.hasOwn(savedProbe,key));
const writesBeforeRemote=draftSaves;state={...state,led_output:{diagnostic:{...extendedObservation,revision:100,index:30}}};persistent.update();assert.equal(draftSaves,writesBeforeRemote,'Live observations do not trigger durable writes');
persistent.dispose();
durable.set(durableKey,{...savedProbe,pending:true,observation:{revision:99999},command:'REPLAY',password:'SECRET'});
state={...state,boot_id:'rebooted',led_output:{diagnostic:null}};persistent=context.createFeatureWorkspaces(persistenceOptions);persistedCard=document.getElementById('pixelDiagnosticsCard');
assert.equal(numberIn('Physical pixel').value,'9');assert.equal(numberIn('Period · ms').value,'','Incomplete input remains a visible draft after reload');
assert.equal(persistedCard.querySelectorAll('input').find(n=>n.type==='color').value,'#102030');
assert(persistedCard.querySelectorAll('p').some(n=>n.textContent==='No diagnostic observation from this device'));
assert(!JSON.stringify(persistent.workspaceSnapshot(durableKey)).includes('SECRET'));
state={...state,led_output:{diagnostic:{...extendedObservation,revision:1,index:45}}};persistent.update();assert.equal(numberIn('Physical pixel').value,'9','Fresh state does not erase the restored unsent draft');
state={...state,board_id:'different-board'};persistent.update();assert.equal(numberIn('Physical pixel').value,'46','A new board follows its own fresh observation, not another board draft');assert.equal(persistent.workspaceSnapshot(scope(state)),null);
state={...state,board_id:durableKey.split(':')[0],capabilities:{...state.capabilities,application:{id:'different-application'}}};persistent.update();assert.equal(persistent.workspaceSnapshot(scope(state)),null,'Application identity isolates drafts even on the same physical board');
assert.equal(diagnosticWrites,existingDiagnosticWrites);assert.equal(diagnosticReads,existingDiagnosticReads,'Restoration never contacts the board');persistent.dispose();
const sanitized=contracts.diagnosticWorkspace({index:9999,rgb:-1,effect:'unknown',position:'too-long-input',min_brightness:200,max_brightness:10,timingDrafts:{period_ms:'0'.repeat(20),password:'SECRET'},observation:{revision:1}});
assert.equal(sanitized.index,0);assert.equal(sanitized.rgb,0xffffff);assert.equal(sanitized.effect,'solid');assert.equal(sanitized.position,'1');assert.equal(sanitized.min_brightness,0);assert.equal(sanitized.max_brightness,255);assert.equal(Object.keys(sanitized.timingDrafts).length,0);assert.equal(contracts.diagnosticWorkspace([]),null);
// Pixel-tool presentation uses the same durable writer, without restoring
// rules or observations. Fingerprints only match verified fresh board rules.
features.clear();for(const f of ['identity','pixel-corrections','pixel-diagnostic','pixel-diagnostic-effects','command-ack','leds'])features.add(f);
state=diagnosticState(1,{capabilities:{application:{id:'table'}}});
saved={...configuration,pixels:480,sources:Array.from({length:480},(_,i)=>i),rules:[{kind:'black',pixel:0},{kind:'shift',pixel:2,offset:1},{kind:'black',pixel:0}]};
const toolStore=new Map();let toolSaves=0,toolCommands=0,toolOwner;
const toolOptions={...options,workspaceScope:scope,peer:()=>({known:active,role:'table-selection',has:f=>active&&features.has(f)}),readPresentation:key=>toolStore.get(key),command(){toolCommands++;throw Error('Restoring presentation must not issue commands');},workspaceChanged(){toolSaves++;toolStore.set(scope(state),toolOwner.presentationSnapshot(scope(state)));}};
toolOwner=context.createFeatureWorkspaces(toolOptions);await new Promise(setImmediate);
const toolCard=()=>document.getElementById('pixelCorrectionsCard'),toolMap=()=>toolCard().querySelectorAll('details').find(n=>n.className==='mapping-disclosure'),toolSearch=()=>toolCard().querySelectorAll('input').find(n=>n.type==='search'),toolAdvanced=()=>document.getElementById('pixelDiagnosticsCard').querySelector('details'),toolPreviews=()=>toolCard().querySelectorAll('details').filter(n=>n.className==='correction-rule-preview');
assert.equal(toolSaves,0);toolMap().open=true;toolMap().ontoggle();toolSearch().value='12';toolSearch().oninput();toolAdvanced().open=true;toolAdvanced().ontoggle();await toolCard().querySelectorAll('button').find(n=>n.textContent==='Draft map').onclick();toolPreviews()[2].open=true;toolPreviews()[2].ontoggle();
const toolKey=scope(state),savedTools=JSON.parse(JSON.stringify(toolStore.get(toolKey)));assert.equal(savedTools.mapView,'draft');assert.equal(savedTools.mapFilter,'12');assert.equal(savedTools.mapOpen,true);assert.equal(savedTools.advancedOpen,true);assert.deepEqual(savedTools.expandedRules,['black:0:0:1:1'],'Duplicate rules have distinct disclosure occurrences');
const savesBeforeObservation=toolSaves;state={...state,uptime_ms:123};toolOwner.update();assert.equal(toolSaves,savesBeforeObservation,'Live status never persists pixel-tool presentation');toolOwner.dispose();
toolStore.set(toolKey,{...savedTools,rules:[{kind:'remove',pixel:30}],pending:true,password:'DO-NOT-RESTORE'});state={...state,boot_id:'restored-tools'};
const beforeToolReload=reads;toolOwner=context.createFeatureWorkspaces(toolOptions);await new Promise(setImmediate);
assert.equal(reads,beforeToolReload+1,'Only the normal verified correction load occurs; no restore fetch or command');assert.equal(toolMap().open,true);assert.equal(toolSearch().value,'12');assert.equal(toolAdvanced().open,true);assert.equal(toolPreviews()[0].open,false);assert.equal(toolPreviews()[2].open,true);assert.equal(toolSaves,savesBeforeObservation,'Passive restore does not rewrite preferences');
toolMap().ontoggle();toolAdvanced().ontoggle();toolPreviews()[2].ontoggle();assert.equal(toolSaves,savesBeforeObservation,'Late programmatic toggle events skip unchanged preference writes');
assert(!JSON.stringify(toolOwner.presentationSnapshot(toolKey)).includes('DO-NOT-RESTORE'));assert.equal(toolCard().querySelector('select').value,'black','Stored preference data never becomes correction configuration');toolOwner.dispose();
saved={...saved,rules:[{kind:'black',pixel:0},{kind:'shift',pixel:2,offset:1},{kind:'black',pixel:1}]};state={...state,boot_id:'changed-tools'};
toolOwner=context.createFeatureWorkspaces(toolOptions);await new Promise(setImmediate);assert(toolPreviews().every(n=>!n.open),'Changed fresh rules do not inherit an unrelated disclosure');toolMap().ontoggle();assert.equal(toolSaves,savesBeforeObservation,'Pruning unmatched fingerprints remains passive');
state={...state,board_id:'other-tools'};toolOwner.update();await new Promise(setImmediate);assert.equal(toolMap().open,false);assert.equal(toolSearch().value,'');assert.equal(toolAdvanced().open,false);assert.equal(toolOwner.presentationSnapshot(toolKey),undefined);
state={...state,board_id:'pixels',capabilities:{application:{id:'another-app'}}};toolOwner.update();await new Promise(setImmediate);assert.equal(toolMap().open,false,'Application identity also isolates presentation');assert.equal(toolCommands,0);toolOwner.dispose();
const cleanTools=contracts.pixelToolsWorkspace({mapView:'unsafe',mapOpen:'true',mapFilter:'private query',advancedOpen:1,expandedRules:['black:0:0:1:0','black:0:0:1:0','shift:480:1:1:0','shift:1:481:1:0','black:1:1:1:0','remove:1:0:1:32','SECRET'],password:'SECRET'});
assert.deepEqual(Array.from(cleanTools.expandedRules),['black:0:0:1:0']);assert.equal(cleanTools.mapView,'applied');assert.equal(cleanTools.mapOpen,false);assert.equal(cleanTools.mapFilter,'');assert.equal(cleanTools.advancedOpen,false);assert.equal(contracts.pixelToolsWorkspace([]),null);
// Custom group counts are physical, not logical cell IDs. Preview and step use
// the same prefix sums; invalid advertised topology never falls back to six.
const groups=context.PixelGroups;
const custom=groups.parse({...groups.defaults('table-selection'),revision:3,overrides:[{group:1,count:5},{group:2,count:7}]},'table-selection');
assert.deepEqual(Array.from(custom.starts.slice(0,4)),[0,5,12,18]);
assert.deepEqual({...groups.probe(custom,4,true,1)},{index:5,count:7});
assert.deepEqual({...groups.probe(custom,11,true,-1)},{index:0,count:5});
assert.equal(groups.probe(custom,0,true,-1),null);
assert.equal(groups.parse({...custom,partial:true,overrides:undefined,starts:undefined},'table-selection').starts[2],12);
for(const invalid of [{...custom,overrides:[{group:1,count:7}]},{...custom,overrides:[{group:1,count:5},{group:1,count:5}]},{...custom,starts:[1,...custom.starts.slice(1)]},{...custom,counts:[0,...custom.counts.slice(1)]},{...custom,group_count:18}])assert.throws(()=>groups.parse(invalid,'table-selection'));
assert.equal(groups.fromState({capabilities:{acknowledged_features:['pixel-group-layout']}},'table-selection'),null);
assert.equal(groups.frameLayout({group_layout_revision:3,group_counts:custom.counts,group_starts:custom.starts},'table-selection').counts[1],7);
assert.throws(()=>groups.frameLayout({group_layout_revision:3,group_counts:custom.counts},'table-selection'));
state={...diagnosticState(1),pixel_groups:custom,capabilities:{acknowledged_features:['pixel-group-layout']}};
features.clear();for(const f of ['identity','pixel-group-layout','pixel-diagnostic','command-ack','leds'])features.add(f);role='table-selection';delayCommand=null;
let groupSaved=structuredClone(custom),groupWrites=0,groupReads=0;
const groupOptions={...diagnosticOptions,
 api:async path=>{assert.equal(path,groups.path);groupReads++;return structuredClone(groupSaved);},
 command:async(path,data)=>{if(path===groups.path){groupWrites++;const wanted=JSON.parse(data.configuration);assert.equal(wanted.revision,groupSaved.revision);assert(wanted.overrides.every(rule=>!Object.hasOwn(rule,'countDraft')),'Local typed strings never enter the board contract');groupSaved=groups.parse({...groups.defaults(role),revision:wanted.revision+1,overrides:wanted.overrides},role);state={...state,pixel_groups:groupSaved};return {};}
 lastDiagnostic=structuredClone(data);state={...state,led_output:{diagnostic:{...data,revision:state.led_output.diagnostic.revision+1,active:true}}};return {status:structuredClone(state)};}};
const groupWorkspace=context.createFeatureWorkspaces(groupOptions);
await new Promise(setImmediate);assert.equal(groupWrites,0);assert.equal(groupReads,1);
const groupNode=document.getElementById('pixelGroupsCard'),groupButton=label=>groupNode.querySelectorAll('button').find(b=>b.textContent===label),groupDirty=()=>groupNode.querySelectorAll('p').some(p=>p.textContent==='Unsaved changes');
await groupButton('Use default').onclick();assert.equal(groupButton('Save to board').disabled,true,'Restoring group 1 alone would exceed capacity');
await groupButton('Use default').onclick();await groupButton('Save to board').onclick();assert.equal(groupWrites,1);assert.equal(groupReads,2);assert.deepEqual(Array.from(groupSaved.counts.slice(0,2)),[6,6]);
const dg=document.getElementById('pixelDiagnosticsCard'),dgStep=dg.querySelector('select'),dgCursor=dg.querySelectorAll('input').find(n=>n.type==='number');
groupSaved=custom;state={...state,pixel_groups:custom};groupWorkspace.update();dgStep.value='6';dgStep.onchange();dgCursor.value='1';dgCursor.oninput();
await dg.querySelectorAll('button').find(b=>b.textContent==='Next').onclick();assert.equal(lastDiagnostic.index,5);assert.equal(lastDiagnostic.count,7);assert.equal(lastDiagnostic.group_layout_revision,3);
// Group reload shares the in-app modal and exact peer/boot ownership. Approval
// is a read/discard only; missing dialog support never silently discards edits.
await groupButton('Customize group').onclick();const groupReadsBefore=groupReads,modalsBefore=modalRequests.length;
const groupCancel=groupButton('Reload').onclick();await groupButton('Reload').onclick();
assert.equal(modalRequests.length,modalsBefore+1,'Only one group confirmation owns the draft');
assert.equal(modalRequests.at(-1).spec.title,'Discard pixel group edits?');assert.equal(modalRequests.at(-1).spec.actions[0].autofocus,true);
modalRequests.at(-1).close();await groupCancel;assert.equal(groupReads,groupReadsBefore);assert(groupDirty());
const groupApprove=groupButton('Reload').onclick();await approveReload(modalRequests.at(-1));await groupApprove;
assert.equal(groupReads,groupReadsBefore+1);assert(!groupDirty());assert.equal(groupWrites,1);
groupNode.querySelectorAll('input').find(n=>n.getAttribute('aria-label')==='Physical group').value='3';
const redrawnGroupReset=groupButton('Use default'),redrawnGroupMore=groupButton('Increase pixel count');
await groupButton('Customize group').onclick();
const beforeRetiredRender=groupNode.querySelectorAll('input').filter(n=>n.getAttribute('aria-label')?.startsWith('Pixel count for group')).map(n=>n.value);
await redrawnGroupMore.onclick();await redrawnGroupReset.onclick();
assert.deepEqual(groupNode.querySelectorAll('input').filter(n=>n.getAttribute('aria-label')?.startsWith('Pixel count for group')).map(n=>n.value),beforeRetiredRender,'Same-peer retired render callbacks cannot change current rows');
const retiredGroupReload=groupButton('Reload').onclick(),retiredGroupPrompt=modalRequests.at(-1);
const retiredGroupReset=groupButton('Use default'),retiredGroupMore=groupButton('Increase pixel count'),retiredGroupInput=groupNode.querySelectorAll('input').find(n=>n.getAttribute('aria-label')==='Pixel count for group 1');
const oldGroupState=state;state={...state,boot_id:'group-new'};groupWorkspace.update();await retiredGroupReload;await new Promise(setImmediate);
const newGroupInputs=()=>groupNode.querySelectorAll('input').filter(n=>n.getAttribute('aria-label')?.startsWith('Pixel count for group')).map(n=>n.value);
const newGroupValues=newGroupInputs();
retiredGroupInput.value='9';retiredGroupInput.oninput();await retiredGroupMore.onclick();await retiredGroupReset.onclick();
assert.deepEqual(newGroupInputs(),newGroupValues,'Retired group row callbacks cannot mutate replacement draft');assert(!groupDirty(),'Retired inputs cannot mark replacement draft dirty');
const afterGroupSwitch=groupReads;await approveReload(retiredGroupPrompt);assert.equal(groupReads,afterGroupSwitch,'Late group approval cannot read replacement peer');
state=oldGroupState;groupWorkspace.update();assert(groupDirty(),'Original group edits survive boot change');
const groupDisconnect=groupButton('Reload').onclick();features.delete('pixel-group-layout');groupWorkspace.update();await groupDisconnect;
features.add('pixel-group-layout');groupWorkspace.update();assert(groupDirty());
context.WebUI={};const missingGroupReads=groupReads;await groupButton('Reload').onclick();assert.equal(groupReads,missingGroupReads);assert(groupDirty());assert(errors.at(-1).message.includes('Group confirmation is unavailable'));context.WebUI=sharedUi;
const groupDispose=groupButton('Reload').onclick();groupWorkspace.dispose();await groupDispose;assert.equal(groupWrites,1);assert.equal(layout.children.length,0);
// Invalid typed counts are drafts too; structural redraw/peer return must not
// replace them with the last valid value and accidentally re-enable Save.
state={...oldGroupState,boot_id:'typed-counts',pixel_groups:custom};groupSaved=custom;
const typedGroups=context.createFeatureWorkspaces(groupOptions);await new Promise(setImmediate);
const typedCard=document.getElementById('pixelGroupsCard'),typedButton=label=>typedCard.querySelectorAll('button').find(b=>b.textContent===label),typedInput=()=>typedCard.querySelectorAll('input').find(n=>n.getAttribute('aria-label')==='Pixel count for group 1');
let nextCustomGroup=3;
for(const invalid of ['', '0', '256', '1.5']){
 typedInput().value=invalid;typedInput().oninput();
 typedCard.querySelectorAll('input').find(n=>n.getAttribute('aria-label')==='Physical group').value=String(nextCustomGroup++);await typedButton('Customize group').onclick();
 assert.equal(typedInput().value,invalid,'Redraw preserves invalid typed group count');assert(typedButton('Save to board').disabled);assert.equal(typedInput().getAttribute('aria-invalid'),'true');
 const beforeInvalidWrite=groupWrites;await typedButton('Save to board').onclick();assert.equal(groupWrites,beforeInvalidWrite,'Even a stale Save callback cannot submit invalid draft');
 const typedState=state;state={...state,boot_id:'typed-other-'+nextCustomGroup};typedGroups.update();await new Promise(setImmediate);state=typedState;typedGroups.update();
 assert.equal(typedInput().value,invalid,'Peer return restores the incomplete typed draft');assert(typedButton('Save to board').disabled);
 await typedButton('Decrease pixel count').onclick();assert.equal(typedInput().value,'4','Deliberate step recovers from last valid value');assert.equal(typedInput().getAttribute('aria-invalid'),'false');
 typedInput().value='5';typedInput().oninput();
}
assert(!typedButton('Save to board').disabled);await typedButton('Save to board').onclick();assert.equal(groupWrites,2);assert.equal(groupSaved.counts[0],5);assert(typedButton('Save to board').disabled,'Verified valid commit retires the typed draft');
typedGroups.dispose();assert.equal(layout.children.length,0);
// The correction editor must retain incomplete numeric drafts just like groups,
// and every row action must remain owned by its original render and peer.
features.clear();features.add('identity');features.add('pixel-corrections');active=true;
saved={...configuration,rules:[{kind:'black',pixel:0,offset:0,enabled:true},{kind:'shift',pixel:2,offset:1,enabled:true}]};state={board_id:'board',boot_id:'correction-drafts',pixel_corrections_revision:0};
const ownedCorrections=context.createFeatureWorkspaces(options);await new Promise(setImmediate);
const ownedCard=document.getElementById('pixelCorrectionsCard'),ownedButton=label=>ownedCard.querySelectorAll('button').find(b=>b.textContent===label),ownedPixel=()=>ownedCard.querySelectorAll('input').find(n=>n.getAttribute('aria-label')==='Pixel · 1');
const wireMap=ownedCard.querySelectorAll('div').find(n=>n.className==='correction-map'),wireDisclosure=ownedCard.querySelectorAll('details').find(n=>n.className==='mapping-disclosure'),wireSearch=ownedCard.querySelectorAll('input').find(n=>n.type==='search'),wireLabels=()=>wireMap.children.map(n=>`${Number(n.dataset.wire)+1} ← ${Number(n.dataset.source)<0?'Off':Number(n.dataset.source)+1}`),wireReads=reads,wireWrites=writes;
assert.equal(wireMap.children.length,0,'Closed map does not build hidden projection tiles');wireDisclosure.open=true;wireDisclosure.ontoggle();
assert.deepEqual(wireLabels(),['1 ← 1','2 ← 2','3 ← 3','4 ← 4','5 ← 5','6 ← 6'],'Applied view uses the board-provided map, not the edited rules');
await ownedButton('Draft map').onclick();assert.equal(ownedButton('Draft map').getAttribute('aria-pressed'),'true');
assert.deepEqual(wireLabels(),['1 ← Off','2 ← 2','3 ← Off','4 ← 3','5 ← 4','6 ← 5'],'Local draft projects ordered black and shift rules');
const previewToggle=ownedCard.querySelectorAll('input').find(n=>n.getAttribute('aria-label')==='Enable rule 1');previewToggle.checked=false;previewToggle.onchange();assert.equal(wireLabels()[0],'1 ← 1','Preview respects per-rule disabled state');previewToggle.checked=true;previewToggle.onchange();
wireSearch.value='6';wireSearch.oninput();assert.deepEqual(wireLabels(),['6 ← 5']);wireSearch.value='999';wireSearch.oninput();assert.equal(wireMap.children.length,0);assert(ownedCard.querySelectorAll('p').some(n=>n.textContent==='No matching pixels'));wireSearch.value='';wireSearch.oninput();
await ownedButton('Applied map').onclick();assert.equal(wireLabels()[0],'1 ← 1','Applied map stays distinct from the unsaved projection');await ownedButton('Draft map').onclick();
assert.equal(reads,wireReads);assert.equal(writes,wireWrites,'Switch/filter/edit local map sends no request or mutation');
state={...state,...rgbState};ownedCorrections.update();
const coloredDot=wireMap.children[3].children[1];assert.equal(coloredDot.textContent,'3');assert.equal(coloredDot.style.backgroundColor,'#0000ff','Projected color follows raw source 3, not output slot 4');assert.equal(coloredDot.style.color,'#ffffff');
assert.equal(wireMap.children[0].children[1].textContent,'×');assert.equal(wireMap.children[0].children[1].style.backgroundColor,'#000000');assert.equal(wireMap.children[0].getAttribute('aria-label'),'Wire 1 ← Off · #000000');
const stableTile=wireMap.children[3];state={...state,uptime_ms:999};ownedCorrections.update();assert.equal(wireMap.children[3],stableTile,'Unrelated telemetry preserves mapping DOM identity');
state={...state,pixel_frame:{...rgbFrame,revision:2,rgb:'00ff00'.repeat(6)}};ownedCorrections.update();assert.equal(wireMap.children[3],stableTile,'New RGB colors update existing circles without rebuilding the grid');assert.equal(wireMap.children[3].children[1].style.backgroundColor,'#00ff00');
state={...state,pixel_frame:{...rgbFrame,pending:true}};ownedCorrections.update();assert.equal(wireMap.children[3].children[1].dataset.known,'false');assert.equal(wireMap.children[3].children[1].style.backgroundColor,'','Pending output clears stale colors rather than inventing a new observation');
active=false;ownedCorrections.update();assert.equal(wireMap.children[3].children[1].dataset.known,'false');active=true;
assert.equal(reads,wireReads);assert.equal(writes,wireWrites,'Live diagram consumes existing status only');
ownedPixel().value='';ownedPixel().oninput();await ownedButton('Add rule').onclick();
assert.equal(ownedPixel().value,'','Correction redraw preserves incomplete pixel draft');assert(ownedButton('Save to board').disabled);
assert.equal(wireMap.children.length,0,'Invalid local draft clears prior preview instead of displaying stale valid mapping');assert(ownedCard.querySelectorAll('p').some(n=>n.textContent==='Complete invalid values to preview the draft'));
const samePeerRetiredRemove=ownedButton('Remove rule'),ownedOffset=()=>ownedCard.querySelectorAll('input').find(n=>n.getAttribute('aria-label')==='Offset · 2');
ownedOffset().value='';ownedOffset().oninput();await ownedButton('Move down').onclick();
assert.equal(ownedCard.querySelectorAll('input').find(n=>n.getAttribute('aria-label')==='Pixel · 2').value,'','Incomplete address follows its reordered rule');
assert.equal(ownedCard.querySelectorAll('input').find(n=>n.getAttribute('aria-label')==='Offset · 1').value,'','Incomplete offset follows its reordered rule');
const rowCountBeforeRetired=ownedCard.querySelectorAll('select').length;await samePeerRetiredRemove.onclick();assert.equal(ownedCard.querySelectorAll('select').length,rowCountBeforeRetired,'Same-peer retired correction row cannot remove current rule');
await ownedCard.querySelectorAll('button').filter(b=>b.textContent==='Move up')[1].onclick();ownedOffset().value='1';ownedOffset().oninput();
const oldRemove=ownedButton('Remove rule'),oldMove=ownedButton('Move down'),oldMore=ownedButton('Increase pixel'),oldPixel=ownedPixel(),oldKind=ownedCard.querySelector('select'),oldToggle=ownedCard.querySelectorAll('input').find(n=>n.getAttribute('aria-label')==='Enable rule 1'),oldPreview=ownedCard.querySelectorAll('details').find(n=>n.className==='correction-rule-preview');
const ownedState=state;state={...state,boot_id:'correction-replacement'};ownedCorrections.update();await new Promise(setImmediate);
const correctionBefore=()=>ownedCard.querySelectorAll('input,select').map(n=>[n.value,n.checked]),replacementValues=correctionBefore();
oldPixel.value='4';oldPixel.oninput();oldKind.value='remove';oldKind.onchange();oldToggle.checked=false;oldToggle.onchange();await oldMore.onclick();await oldMove.onclick();await oldRemove.onclick();
oldPreview.open=true;oldPreview.ontoggle();assert.equal(ownedCard.querySelectorAll('details').find(n=>n.className==='correction-rule-preview').open,false,'Retired disclosure callback cannot open or change replacement rule');
assert.deepEqual(correctionBefore(),replacementValues,'Retired correction callbacks cannot change replacement rows');assert(ownedButton('Save to board').disabled);
state=ownedState;ownedCorrections.update();assert.equal(ownedPixel().value,'','Original correction draft survives peer return');
assert.equal(ownedButton('Draft map').getAttribute('aria-pressed'),'true','Local map view belongs to the original peer model');assert.equal(wireMap.children.length,0);
const beforeInvalidCorrection=writes;await ownedButton('Save to board').onclick();assert.equal(writes,beforeInvalidCorrection);
await ownedButton('Increase pixel').onclick();assert.equal(ownedPixel().value,'2');
await ownedButton('Save to board').onclick();assert.equal(writes,beforeInvalidCorrection+1);assert(lastPayload.rules.every(rule=>!Object.hasOwn(rule,'pixelDraft')&&!Object.hasOwn(rule,'offsetDraft')),'Only normalized correction fields enter the board contract');
ownedCorrections.dispose();assert.equal(layout.children.length,0);
// The real editor wires the shared RAM lease owner: opening is passive, first
// edit previews by default, and only explicit Save changes persistent rules.
{
state={board_id:'preview',boot_id:'boot',pixel_corrections_revision:0};
saved=structuredClone(configuration);features.clear();for(const f of ['identity','pixel-corrections','pixel-corrections-preview'])features.add(f);
let live=false,activeSources=[...saved.sources],previewWrites=0,persistentWrites=0;
const previewResponse=()=>({...saved,active_sources:activeSources,preview:{active:live,lease_id:live?7:0,base_revision:saved.revision,remaining_ms:live?10000:0}});
const previewWorkspace=context.createFeatureWorkspaces({...options,
 api:async()=>previewResponse(),
 command:async(path,data)=>{const value=JSON.parse(data.configuration);
  if(path.endsWith('/preview')){previewWrites++;live=!value.clear;
   activeSources=live?Array.from(contracts.correctionSources(saved.pixels,value.enabled,value.rules)):[...saved.sources];
   return previewResponse();}
  assert.equal(path,'/api/pixels/corrections');assert.equal(live,false,'Release precedes durable save');persistentWrites++;
  saved={...saved,...value,revision:saved.revision+1,sources:Array.from(contracts.correctionSources(saved.pixels,value.enabled,value.rules))};return saved;
 }});
await new Promise(setImmediate);
const previewCard=document.getElementById('pixelCorrectionsCard'),previewButton=label=>previewCard.querySelectorAll('button').find(b=>b.textContent===label);
assert.equal(previewWrites,0);assert.equal(persistentWrites,0);
const previewToggle=previewCard.querySelectorAll('label').find(n=>n.children.some(c=>c.textContent==='Live preview')).children.find(n=>n.type==='checkbox');
assert.equal(previewToggle.checked,true);
await previewButton('Add rule').onclick();await new Promise(setImmediate);
assert.equal(previewWrites,1);assert.equal(persistentWrites,0);assert.equal(live,true);
await previewButton('Save to board').onclick();await new Promise(setImmediate);
assert.equal(previewWrites,2);assert.equal(persistentWrites,1);assert.equal(live,false);
previewWorkspace.dispose();
}
assert(!source.includes('new WebSocket'));assert(!source.includes('setInterval('));assert(!source.includes('innerHTML'));
assert(!source.includes('/api/audio'),'Existing sound controls remain the only implementation');
console.log('Unified Web feature workspaces: real DOM, correction revision/readback, typed 6/12-pixel diagnostics, live drafts, all-off/rainbow, peer-change rejection, capability gates and no duplicate transport/front panel passed.');
