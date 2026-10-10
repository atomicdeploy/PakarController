import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const source=readFileSync(new URL('../firmware/esp8266/src/WebUi.h',import.meta.url),'utf8');
const nativeTable=readFileSync(new URL('../desktop/src/ui/cell_table.rs',import.meta.url),'utf8');
const nativeShell=readFileSync(new URL('../desktop/src/ui/shell.rs',import.meta.url),'utf8');
assert(nativeTable.includes('let emphasized = selected || matched;'));
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
    classes:new Set(),classList:{toggle(){}},appendChild(e){this.children.push(e);e.parent=this},append(...es){es.forEach(e=>this.appendChild(e))},replaceChildren(){this.children=[]},
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
const calls=[],actual={cells:Array(80).fill('white')},storage=new Map();let failId=0,cancelAfter=0;
const context=vm.createContext({...nodes,document,URLSearchParams,innerWidth:1280,innerHeight:800,
  localStorage:{getItem:k=>storage.get(k),setItem:(k,v)=>storage.set(k,v)},
  authenticated:true,loadMappingEditor(){},report(){},render(){},setTimeout:fn=>{fn();return 1},
  api:async(path,options)=>{if(!options)return actual;const id=Number(options.body.get('id'));calls.push(id);if(id===failId)throw Error('lost acknowledgement');actual.cells[id-1]=options.body.get('state');if(id===cancelAfter)vm.runInContext('cancelCellBatch()',context);return {accepted:true}}
});
vm.runInContext(cellCode,context);
const run=code=>vm.runInContext(code,context),ids=()=>Array.from(run('[...selectedCells]')).sort((a,b)=>a-b);
assert.equal(nodes.grid.children.length,80);assert.equal(nodes.managerRows.children.length,8);
assert.deepEqual(Array.from(run("cellMatches('1, 3–5 #80')")),[1,3,4,5,80]);
for(const query of ['0','81','5-2','3x','1--4','1#2',','])assert.throws(()=>run('cellMatches('+JSON.stringify(query)+')'));
run('selectCell(3);selectCell(7,{shiftKey:true})');assert.deepEqual(ids(),[3,4,5,6,7]);assert.equal(calls.length,0,'Selection must not mutate hardware');
assert.equal(nodes.cellSearch.value,'3–7','Range selection appears in Find cells');
run('selectCell(4,{ctrlKey:true})');assert.deepEqual(ids(),[3,5,6,7]);
assert.equal(nodes.cellSearch.value,'3, 5–7');
run('selectRow(1,{ctrlKey:true});selectRow(2,{ctrlKey:true})');assert(ids().includes(11)&&ids().includes(30));
nodes.additiveSelection.checked=true;run('selectRow(1)');assert(!ids().includes(11));run('saveTablePreferences()');assert.equal(JSON.parse(storage.get('pakar-table')).additive,true);
assert.equal(nodes.showCellIndicators.checked,false,'Indicators must default off');
nodes.showCellIndicators.checked=true;run('saveTablePreferences()');assert.equal(JSON.parse(storage.get('pakar-table')).indicators,true);
assert(source.includes('.grid:not(.show-cell-indicators) .cell::before{display:none}'));
assert(source.includes('.cell.selected:hover,.cell.selected:active,.cell.selected:focus-visible,.cell.matched:hover,.cell.matched:active,.cell.matched:focus-visible{border-color:var(--amber);background:var(--cell-surface,var(--surface-2));box-shadow:none;outline-color:transparent}'));
assert(!source.includes('.cell.red{background:color-mix'));
assert(!source.includes('.cell.selected{border-color:var(--accent)'));
assert.equal(run("cellNavigate(10,'ArrowRight')"),10);assert.equal(run("cellNavigate(11,'ArrowLeft')"),11);assert.equal(run("cellNavigate(37,'Home')"),31);assert.equal(run("cellNavigate(37,'End',true)"),80);
nodes.additiveSelection.checked=false;nodes.cellSearch.value='2,12';run('selectSearchMatches()');assert.deepEqual(ids(),[2,12]);
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
for(const cell of nodes.grid.children.filter(e=>e.className==='cell'))assert.equal(cell.classes.has('matched'),cell.classes.has('selected'));
nodes.grid.listeners.pointerup({pointerId:1});assert.equal(run('suppressCellClick'),true);
nodes.grid.children.find(b=>b.dataset.id===2).onclick({});assert.equal(run('suppressCellClick'),false);assert.deepEqual(ids(),[1,2]);
run('openCellMenu(1,1200,790)');assert.equal(nodes.cellMenu.hidden,false);assert.equal(nodes.cellMenu.children[0].attributes.role,'menuitem');assert(nodes.cellMenu.children[0].children.some(c=>c.tag==='svg'));
run('openCellManager()');assert.equal(nodes.cellManager.open,true);
run('selectedCells=new Set([1,2,3])');await run("applyCellBatch('red')");assert.deepEqual(calls,[1,2,3]);assert.match(nodes.bulkStatus.textContent,/Verified 3/);
calls.length=0;failId=2;await run("applyCellBatch('off')");assert.deepEqual(calls,[1,2]);assert.match(nodes.bulkStatus.textContent,/not retried/);assert.equal(nodes.bulkApply.disabled,false);
calls.length=0;failId=0;cancelAfter=1;await run("applyCellBatch('white')");assert.deepEqual(calls,[1]);assert.match(nodes.bulkStatus.textContent,/Cancelled unsent/);
assert.match(source,/border:2px solid var\(--line-strong\)/);assert.doesNotMatch(source,/\.cell\{[^}]*background:#f6f7ff/);
assert.match(source,/prefers-reduced-motion:reduce/);assert.match(source,/transition:background-color \.14s/);
console.log('Actual Web table: selection, rows, search, keyboard, marquee, icons/dialog, paced bulk readback, cancellation and no uncertain retry passed (DOM/HTTP doubles).');
