import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const source=readFileSync(new URL('../firmware/esp8266/src/WebUi.h',import.meta.url),'utf8');
const nativeTable=readFileSync(new URL('../desktop/src/ui/cell_table.rs',import.meta.url),'utf8');
const nativeShell=readFileSync(new URL('../desktop/src/ui/shell.rs',import.meta.url),'utf8');
assert(nativeTable.includes('let emphasized = selected;'));
assert(!nativeTable.includes('matched: bool'));
assert(!nativeTable.includes('matches.contains'));
const managerCode=nativeTable.slice(nativeTable.indexOf('pub fn draw_cell_manager'));
assert(!managerCode.includes('ui.selectable_label('),'Manager must not allocate state-dependent selectable Frames');
assert(managerCode.includes('ui.horizontal_top('),'Wrapped rows keep their header at the top');
assert(managerCode.includes('manager_selector(ui,'));
assert(nativeTable.includes('ui.allocate_space(egui::vec2(width, dialog::ACTION_HEIGHT))'));
assert(nativeTable.includes('ui.painter().with_clip_rect(rect).text('));
const managerSlot=source.match(/\.manager-row button\{box-sizing:[^}]+\}/)[0];
for(const rule of ['box-sizing:border-box','flex:0 0 32px','width:32px','height:28px','padding:0','border:2px solid'])assert(managerSlot.includes(rule),rule);
assert(source.includes('.manager-row button:focus-visible{outline:1px solid var(--blue);outline-offset:-5px}'));
assert(source.includes('.manager-row button[aria-pressed=true]:focus-visible{outline-color:transparent;border-color:var(--amber);background:var(--surface-2)}'));
// Every state override keeps slot metrics intact; only paint can change.
for(const rule of source.matchAll(/([^{}]*\.manager-row button[^{}]*:[^{}]*)\{([^}]+)\}/g)){
  assert(!/(?:^|;)(?:width|height|padding|margin|border-width|transform|font-size):/.test(rule[2]),rule[0]);
}
assert(!nativeShell.includes('navigate(&matches'));
for(const alias of ['selected_cells','highlighted_cells','search_matches'])assert(nativeShell.includes(`"${alias}":self.table.selection.cells()`));
assert(!nativeTable.includes('blend(base_fill, v.error_fg_color'));
assert(!nativeTable.includes('fill = blend(fill, v.selection.bg_fill'));
assert(nativeShell.includes('if search.changed() { self.apply_cell_search_selection(); }'));
assert(nativeShell.includes('if self.sync_cell_selection_search() { ctx.request_repaint(); }'));
const script=source.match(/<script>([\s\S]*?)<\/script>/)[1];
const cellCode=script.slice(script.indexOf('let selectedCells='),script.indexOf('for(let i=0;i<8;i++)'));
assert(cellCode.length>5000);
let document;
function element(tag='div') {
  return {tag,value:'',checked:false,children:[],dataset:{},style:{},attributes:{},listeners:{},className:'',hidden:false,disabled:false,
    classes:new Set(),classList:{toggle(){}},appendChild(e){if(e.parent)e.parent.children=e.parent.children.filter(c=>c!==e);this.children.push(e);e.parent=this},append(...es){es.forEach(e=>this.appendChild(e))},replaceChildren(){this.children=[]},
    setAttribute(k,v){this.attributes[k]=v},setCustomValidity(v){this.validation=v},addEventListener(k,fn){this.listeners[k]=fn},
    querySelectorAll(selector){const nodes=this.children.flatMap(e=>[e,...e.querySelectorAll(selector)]);return nodes.filter(e=>selector==='.cell'?e.className==='cell':selector==='.marquee'?e.className==='marquee':selector==='[data-cell]'?e.dataset.cell!==undefined:selector==='[data-row]'?e.dataset.row!==undefined:selector==='button:not(:disabled)'?e.tag==='button'&&!e.disabled:false)},
    querySelector(selector){if(selector.startsWith('[data-id=')){return this.children.find(e=>Number(e.dataset.id)===Number(selector.match(/\d+/)[0]))}return this.querySelectorAll(selector)[0]},
    focus(){document.activeElement=this},showModal(){this.open=true},close(){this.open=false},contains(e){return e===this||this.children.some(c=>c.contains(e))},remove(){this.parent.children=this.parent.children.filter(e=>e!==this)},
    getBoundingClientRect(){if(this.dataset.id){const n=Number(this.dataset.id)-1;return {left:(n%10)*60,top:Math.floor(n/10)*43.2+(n%2===0?21.6:0),width:53,height:36.2}}return {left:0,top:0,width:600,height:370}},setPointerCapture(){},hasPointerCapture(){return false}};
}
const createElement=tag=>{const e=element(tag);e.classList.toggle=(name,on)=>on?e.classes.add(name):e.classes.delete(name);return e};
document={activeElement:null,createElement,createElementNS:(_ns,tag)=>createElement(tag),createTextNode:text=>Object.assign(createElement('text'),{textContent:text}),addEventListener(){}};
const nodes=Object.fromEntries([...source.matchAll(/id="([^"]+)"/g)].map(m=>[m[1],element()]));
nodes.animateCells.checked=true;nodes.bulkEffect.value='fade';
const calls=[],actual={cells:Array(80).fill('white')},storage=new Map();let failId=0,cancelAfter=0,storageWrites=0;
const fixturePeer=Object.freeze({board:'fixture',boot:'a'.repeat(32),generation:1});
const context=vm.createContext({...nodes,peerHttp:{pin:()=>fixturePeer},document,URLSearchParams,innerWidth:1280,innerHeight:800,
  localStorage:{getItem:k=>storage.get(k),setItem:(k,v)=>{storageWrites++;storage.set(k,v)}},
  state:{cells:[]},mapping:{physical:[],names:[]},currentPeer:()=>({has:feature=>feature==='table'}),authenticated:true,loadMappingEditor(){},report(){},render(){},setTimeout:fn=>{fn();return 1},
  api:async(path,options)=>{assert.equal(options.peer,fixturePeer,'Whole batch must retain its original peer');if(path==='/api/status')return actual;const id=Number(options.body.get('id'));calls.push(id);if(id===failId)throw Error('lost acknowledgement');actual.cells[id-1]=options.body.get('state');if(id===cancelAfter)vm.runInContext('cancelCellBatch()',context);return {accepted:true}}
});
vm.runInContext(readFileSync(new URL('../assets/web/rayanlamp.js',import.meta.url),'utf8')+'\n'+readFileSync(new URL('../assets/web/localization.js',import.meta.url),'utf8')+'\n'+cellCode,context);
const run=code=>vm.runInContext(code,context),ids=()=>Array.from(run('[...selectedCells]')).sort((a,b)=>a-b);
const pixelContract=JSON.parse(readFileSync(new URL('../assets/cells/pixel-contract.json',import.meta.url),'utf8'));
const pixelNative=readFileSync(new URL('../desktop/src/ui/cell_pixels.rs',import.meta.url),'utf8');
assert(pixelNative.includes('assets/cells/pixel-contract.json'));
pixelContract.ccw_unit_centers.forEach((p,sector)=>{const got=Array.from(run(`pixelPoint(${sector},1)`));assert(Math.abs(got[0]-50-p[0])<0.00001);assert(Math.abs(got[1]-50-p[1])<0.00001)});
const physical=Array.from({length:80},(_,i)=>i+1);[physical[0],physical[79]]=[physical[79],physical[0]];
const rgb='000000'.repeat(pixelContract.first_index)+pixelContract.colors.join('');
const frame={rgb,physical,revision:123,brightness:180};
context.mapping={physical,names:['Table A']};context.state={cells:Array(80).fill('off'),pixel_preview_requested:true,pixel_frame:frame};
nodes.cellShape.value='circle';nodes.pixelDiagnostics.checked=true;run('renderCellPixels()');
const face=nodes.grid.children[0].children[0];
assert.equal(face.tag,'svg');
assert.deepEqual(face.children.filter(n=>n.tag==='polygon').map(n=>n.attributes.fill),pixelContract.colors.map(c=>'#'+c));
assert.deepEqual(face.children.filter(n=>n.tag==='text'&&n.attributes.class==='pixel-index').map(n=>n.textContent),[474,475,476,477,478,479]);
assert.match(nodes.grid.children[0].title,/pixels 474–479/);
assert.match(nodes.grid.children[0].title,/pre-brightness/);
assert(face.children.some(n=>n.tag==='circle'),'Circle outline');
assert(face.children.some(n=>n.attributes.class==='pixel-label'&&n.textContent==='Table…'));
assert.match(nodes.grid.children[0].title,/Table A/,'Full name stays in accessible tooltip');
const signatureBefore=nodes.grid.children[0].dataset.pixelSignature;
run('renderCellPixels()');assert.equal(nodes.grid.children[0].children[0],face,'No identical per-cell SVG rebuild');
context.state={cells:Array(80).fill('red')};run('renderCellPixels()');assert.equal(nodes.grid.children[0].dataset.pixelSignature,signatureBefore,'WebSocket cell state is not fabricated RGB');
nodes.cellShape.value='hexagon';run('saveTablePreferences()');
assert(nodes.grid.children[0].children[0].children.some(n=>n.tag==='polygon'&&n.attributes.class==='pixel-outline'));
assert.equal(JSON.parse(storage.get('pakar-table')).shape,'hexagon');
assert(run('validPixelFrame(state.pixel_frame)')==null);
for(const bad of [{...frame,rgb:'ff'}, {...frame,rgb:'z'.repeat(2880)}, {...frame,physical:Array(80).fill(1)}, {...frame,brightness:256}])assert.equal(run('validPixelFrame('+JSON.stringify(bad)+')'),false);
context.state={pixel_preview_requested:true,pixel_frame:null,cells:[]};run('renderCellPixels()');
assert(nodes.grid.children[0].children[0].children.some(n=>n.attributes.class==='pixel-unknown'));
assert.equal(nodes.grid.children[0].children[0].children.find(n=>n.attributes.class==='pixel-index').textContent,474,'Mapping diagnostics survive missing RGB');
context.state={pixel_preview_requested:true,pixel_frame:frame,cells:[]};run('renderCellPixels();pixelSampleAt=Date.now()-6000;renderCellPixels()');
assert(nodes.grid.children[0].children[0].children.some(n=>n.attributes.class==='pixel-unknown'),'Stale sample is never live RGB');
context.mapping={physical:[],names:[]};context.state={cells:[]};run('pixelSample=null;renderCellPixels()');
assert.equal(nodes.grid.children[0].children[0].children.find(n=>n.attributes.class==='pixel-index').textContent,'?','No guessed physical indices');
nodes.cellShape.value='rounded';nodes.pixelDiagnostics.checked=false;run('saveTablePreferences()');
assert.equal(nodes.grid.children[0].children.length,0);assert.equal(nodes.grid.children[0].textContent,'1');
assert.equal(calls.length,0,'Shape and diagnostic controls never write LEDs');
const contract=JSON.parse(readFileSync(new URL('../assets/cells/selection-contract.json',import.meta.url),'utf8'));
assert(nativeTable.includes('assets/cells/selection-contract.json'));
for(const step of contract.steps){
  switch(step.op){
    case 'find': nodes.cellSearch.value=step.query;nodes.cellSearch.listeners.input();assert.equal(Boolean(nodes.cellSearch.validation),step.invalid===true);if(!step.invalid)nodes.cellSearch.listeners.blur();break;
    case 'click': run(`selectCell(${step.id},{ctrlKey:${!!step.toggle},shiftKey:${!!step.extend}})`);break;
    case 'row':run(`selectRow(${step.row})`);break;
    case 'next':run('navigateSelection()');break;
    case 'clear':run('clearCellSelection()');break;
    default:throw Error('Unknown shared selection operation '+step.op);
  }
  assert.deepEqual(ids(),step.ids,JSON.stringify(step));
  assert.equal(run('selectionQuery()'),step.canonical);
  if(step.active!==undefined)assert.equal(run('cellFocus'),step.active);
  for(const cell of nodes.grid.children){const selected=step.ids.includes(Number(cell.dataset.id));assert.equal(cell.classes.has('selected'),selected);assert.equal(cell.attributes['aria-pressed'],String(selected));assert(!cell.classes.has('matched'),'No second highlight state');}
  assert.equal(nodes.selectionSummary.textContent,step.ids.length+' selected');
}
assert.equal(nodes.grid.children.length,80);assert.equal(nodes.managerRows.children.length,8);
nodes.cellSearch.value='023, 3, #3 21-22';nodes.cellSearch.listeners.input();nodes.cellSearch.listeners.blur();assert.equal(nodes.cellSearch.value,'3, 21–23');
const keyEvent=key=>({key,shiftKey:false,preventDefault(){}});
nodes.cellSearch.listeners.keydown(keyEvent('Enter'));assert.equal(run('cellFocus'),21);
nodes.cellSearch.listeners.keydown(keyEvent('Enter'));assert.equal(run('cellFocus'),22);assert.deepEqual(ids(),[3,21,22,23]);
nodes.cellSearch.value='3-';nodes.cellSearch.listeners.input();nodes.cellSearch.listeners.keydown(keyEvent('F3'));assert.equal(run('cellFocus'),23);assert.deepEqual(ids(),[3,21,22,23]);assert.equal(nodes.cellSearch.value,'3-');
nodes.cellSearch.listeners.keydown(keyEvent('Escape'));assert.deepEqual(ids(),[]);assert.equal(nodes.cellSearch.value,'');
assert.deepEqual(Array.from(run("parseSelectionQuery('1, 3–5 #80')")),[1,3,4,5,80]);
for(const query of ['0','81','5-2','3x','1--4','1#2',',','##3','+3'])assert.throws(()=>run('parseSelectionQuery('+JSON.stringify(query)+')'));
run('selectCell(3);selectCell(7,{shiftKey:true})');assert.deepEqual(ids(),[3,4,5,6,7]);assert.equal(calls.length,0,'Selection must not mutate hardware');
assert.equal(nodes.cellSearch.value,'3–7','Range selection appears in Find cells');
run('selectCell(4,{ctrlKey:true})');assert.deepEqual(ids(),[3,5,6,7]);
assert.equal(nodes.cellSearch.value,'3, 5–7');
run('selectRow(1,{ctrlKey:true});selectRow(2,{ctrlKey:true})');assert(ids().includes(11)&&ids().includes(30));
nodes.additiveSelection.checked=true;run('selectRow(1)');assert(!ids().includes(11));run('saveTablePreferences()');assert.equal(JSON.parse(storage.get('pakar-table')).additive,true);
assert.equal(nodes.showCellIndicators.checked,false,'Indicators must default off');
nodes.showCellIndicators.checked=true;run('saveTablePreferences()');assert.equal(JSON.parse(storage.get('pakar-table')).indicators,true);
assert(source.includes('.grid:not(.show-cell-indicators) .cell::before{display:none}'));
assert(source.includes('.cell.selected:hover,.cell.selected:active,.cell.selected:focus-visible{border-color:var(--amber);background:var(--cell-surface,var(--surface-2));box-shadow:none;outline-color:transparent}'));
assert(!source.includes('.cell.matched'));
assert(!source.includes('Select matches'));
assert(!source.includes('.cell.red{background:color-mix'));
assert(!source.includes('.cell.selected{border-color:var(--accent)'));
assert.equal(run("cellNavigate(10,'ArrowRight')"),10);assert.equal(run("cellNavigate(11,'ArrowLeft')"),11);assert.equal(run("cellNavigate(37,'Home')"),31);assert.equal(run("cellNavigate(37,'End',true)"),80);
nodes.additiveSelection.checked=false;nodes.cellSearch.value='2,12';run('updateSelectionFromQuery()');assert.deepEqual(ids(),[2,12]);
nodes.cellSearch.value='21–23, 80';nodes.cellSearch.listeners.input();assert.deepEqual(ids(),[21,22,23,80]);
nodes.cellSearch.value='21-';nodes.cellSearch.listeners.input();assert.deepEqual(ids(),[21,22,23,80]);assert(nodes.cellSearch.validation);
run('paintSelection(true)');assert.equal(nodes.cellSearch.value,'21-','Status repaint must not clobber a draft');
assert.match(source,/function render\(s\)[^\n]*paintSelection\(true\)/);
run('selectCell(21)');assert.equal(nodes.cellSearch.value,'21');assert.equal(nodes.cellSearch.validation,'');
nodes.cellSearch.value='bad';nodes.cellSearch.listeners.input();run('selectCell(21)');assert.equal(nodes.cellSearch.value,'21','Reselecting the same cell replaces a stale draft');
run('selectAllCells()');assert.equal(nodes.cellSearch.value,'1–80');
run('clearCellSelection()');assert.equal(nodes.cellSearch.value,'');
run('selectRow(1)');assert.equal(nodes.cellSearch.value,'11–20');
run('selectCell(11)');nodes.grid.children[10].onkeydown({key:'ArrowRight',preventDefault(){}});assert.equal(nodes.cellSearch.value,'12');
nodes.grid.listeners.pointerdown({button:0,pointerType:'mouse',clientX:0,clientY:0,pointerId:1});
nodes.grid.listeners.pointermove({clientX:115,clientY:58,pointerId:1});assert.deepEqual(ids(),[1,2]);
assert.equal(nodes.cellSearch.value,'1–2','Marquee selection updates Find cells');
for(const cell of nodes.grid.children.filter(e=>e.className==='cell'))assert.equal(cell.classes.has('selected'),ids().includes(Number(cell.dataset.id)));
nodes.grid.listeners.pointerup({pointerId:1});assert.equal(run('suppressCellClick'),true);
nodes.grid.children.find(b=>b.dataset.id===2).onclick({});assert.equal(run('suppressCellClick'),false);assert.deepEqual(ids(),[1,2]);
run('openCellMenu(1,1200,790)');assert.equal(nodes.cellMenu.hidden,false);assert.equal(nodes.cellMenu.children[0].attributes.role,'menuitem');assert(nodes.cellMenu.children[0].children.some(c=>c.tag==='svg'));
run('openCellManager()');assert.equal(nodes.cellManager.open,true);
run('selectedCells=new Set([1,2,3])');await run("applyCellBatch('red')");assert.deepEqual(calls,[1,2,3]);assert.match(nodes.bulkStatus.textContent,/Verified 3/);
calls.length=0;failId=2;await run("applyCellBatch('off')");assert.deepEqual(calls,[1,2]);assert.match(nodes.bulkStatus.textContent,/not retried/);assert.equal(nodes.bulkApply.disabled,false);
calls.length=0;failId=0;cancelAfter=1;await run("applyCellBatch('white')");assert.deepEqual(calls,[1]);assert.match(nodes.bulkStatus.textContent,/Cancelled unsent/);
assert.match(source,/border:2px solid var\(--line-strong\)/);assert.doesNotMatch(source,/\.cell\{[^}]*background:#f6f7ff/);
assert.match(source,/prefers-reduced-motion:reduce/);assert.match(source,/transition:background-color \.14s/);
const orderContract=JSON.parse(readFileSync(new URL('../assets/cells/order-contract.json',import.meta.url),'utf8'));
assert(nativeTable.includes('assets/cells/order-contract.json'));
run('paintSelection()');
const initialSelection=ids(),queryBefore=nodes.cellSearch.value,physicalBefore=JSON.stringify(actual);
const visualOrder=()=>nodes.grid.querySelectorAll('.cell').map(b=>Number(b.dataset.id));
calls.length=0;
for(const step of orderContract.steps){
  run(`setEditingOrder(${step.editing})`);
  const previousWrites=storageWrites;
  assert.equal(run(`reorderCell(${step.source},${step.target},${step.before})`),step.changed);
  assert.equal(storageWrites-previousWrites,step.changed?1:0,'Save exactly once on a real completed reorder');
  const expected=step.expected.flatMap(part=>Array.isArray(part)?Array.from({length:part[1]-part[0]+1},(_,i)=>part[0]+i):[part]);
  assert.deepEqual(Array.from(run('cellOrder')),expected);
  assert.deepEqual(visualOrder(),expected,'Grid DOM order follows the persisted model');
  assert.deepEqual(nodes.managerRows.children.flatMap(row=>row.children.filter(b=>b.dataset.cell!==undefined).map(b=>Number(b.dataset.cell))),expected);
  assert.equal(new Set(expected).size,80);
  assert.deepEqual(ids(),initialSelection);assert.equal(nodes.cellSearch.value,queryBefore);
  for(const b of nodes.grid.querySelectorAll('.cell'))assert.equal(b.draggable,step.editing);
}
assert.equal(run("cellNavigate(80,'ArrowRight')"),1);
assert.equal(run("cellNavigate(1,'Home',true)"),80);
assert.deepEqual(JSON.parse(storage.get('pakar-table')).order,visualOrder());
const savedOrder=visualOrder();run('setEditingOrder(false);resetCellOrder()');assert.deepEqual(visualOrder(),savedOrder);
const dragged=nodes.grid.children.find(b=>b.dataset.id===80),target=nodes.grid.children.find(b=>b.dataset.id===3);
let prevented=0;const transfer={setData(){},setDragImage(){}};
const dragEvent={clientX:0,clientY:0,dataTransfer:transfer,preventDefault(){prevented++}};
dragged.ondragstart(dragEvent);assert.equal(prevented,1);assert.equal(run('orderDrag'),null);
run('setEditingOrder(true)');dragged.ondragstart(dragEvent);assert.equal(run('orderDrag'),80);assert.equal(transfer.effectAllowed,'move');
target.ondragover(dragEvent);assert(target.classes.has('order-before'));assert.equal(transfer.dropEffect,'move');
target.ondrop(dragEvent);assert.equal(run('orderDrag'),null);assert.equal(visualOrder().at(-2),80);
const afterDrop=visualOrder();dragged.ondragstart(dragEvent);run('setEditingOrder(false)');target.ondrop(dragEvent);assert.deepEqual(visualOrder(),afterDrop,'Leaving mode cancels an in-flight drop');
run('setEditingOrder(true)');dragged.ondragstart(dragEvent);dragged.ondragend();assert.equal(run('orderDrag'),null);assert.deepEqual(visualOrder(),afterDrop,'Cancelled drag never saves a reorder');
nodes.grid.listeners.pointerdown({button:0,pointerType:'mouse',clientX:0,clientY:0,pointerId:2});assert.equal(run('cellDrag'),null,'Edit order excludes marquee gestures');
assert.deepEqual(Array.from(run('normalizedCellOrder([80,80,0,81,"2",1])')).slice(0,3),[80,1,2]);
run('resetCellOrder();setEditingOrder(false)');assert.deepEqual(visualOrder(),Array.from({length:80},(_,i)=>i+1));
const managerCells=nodes.managerRows.querySelectorAll('[data-cell]');
run('setEditingOrder(true)');managerCells[0].ondragstart(dragEvent);managerCells[3].ondrop(dragEvent);assert.deepEqual(visualOrder().slice(0,4),[2,3,1,4]);
run('selectRow(0)');assert.deepEqual(ids(),Array.from({length:10},(_,i)=>i+1),'Row selection uses reordered visible membership');
run('selectedCells=new Set('+JSON.stringify(initialSelection)+');paintSelection();resetCellOrder();setEditingOrder(false)');
assert.deepEqual(ids(),initialSelection);assert.equal(nodes.cellSearch.value,queryBefore);assert.equal(JSON.stringify(actual),physicalBefore);assert.equal(calls.length,0,'Reordering must not call a device API');
console.log('Actual Web table: one authoritative selection, shared native/Web transition fixture, normalization/drafts/navigation, rows/keyboard/marquee, bulk readback and no uncertain retry passed (DOM/HTTP doubles; native execution separate).');
