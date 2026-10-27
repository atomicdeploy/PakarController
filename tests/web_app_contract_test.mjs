// Execute the canonical application helpers: no second application bootstrap,
// mock device authority, browser layout claim or physical acceptance.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import vm from 'node:vm';
const read=path=>readFile(new URL('../'+path,import.meta.url),'utf8');
const source=await read('web/index.html');
// Syntax extraction only, never HTML sanitization. Match the forgiving end tags
// a browser accepts so a malformed closing tag cannot hide an unchecked script.
const scripts=html=>[...html.matchAll(/<script\b[^>]*>([\s\S]*?)<\/script[^>]*>/gi)];
for(const closing of ['</script>','</script >','</SCRIPT>','</script ignored="yes">']){
  assert.equal(scripts('<SCRIPT type="text/javascript">throw new Error("checked")'+closing).length,1);
  assert.throws(()=>new vm.Script(scripts('<script>const = broken'+closing)[0][1]));
}
for(const script of scripts(source))new vm.Script(script[1]);
const plain=value=>JSON.parse(JSON.stringify(value));
const layoutContext=vm.createContext({});
vm.runInContext(await read('web/modules/table-layout.js'),layoutContext);
const layout=layoutContext.WebTableLayout;
for(const id of [1,21,41,61])assert.equal(layout.position(id).x,0);
for(const id of [11,31,51,71])assert.equal(layout.position(id).x,1);
assert.equal(layout.position(1).y,0);assert.equal(layout.position(11).y,.5);
const occupied=new Set();
for(let id=1;id<=80;id++){
  const p=layout.position(id);assert(Number.isInteger(p.x)&&p.x>=0&&p.x<20);
  assert.equal(p.y,Math.floor((id-1)/20)+(p.x%2?.5:0));
  const key=p.x+','+p.y;assert(!occupied.has(key));occupied.add(key);
}
for(const bad of [0,81,-1,1.5,'1',null])assert.throws(()=>layout.position(bad));
assert.equal(layout.neighbor(1,'ArrowRight'),11);assert.equal(layout.neighbor(11,'ArrowLeft'),1);
assert.equal(layout.neighbor(1,'ArrowDown'),21);assert.equal(layout.neighbor(21,'ArrowUp'),1);
assert.equal(layout.neighbor(1,'ArrowUp'),1);assert.equal(layout.neighbor(1,'ArrowLeft'),1);
assert.equal(layout.neighbor(80,'ArrowDown'),80);assert.equal(layout.neighbor(1,'Enter'),1);

const selectionContext=vm.createContext({selectedCells:new Set()});
vm.runInContext((await read('assets/web/rayanlamp.js')).split('const fixedHapticPatterns')[0],selectionContext);
for(const name of ['parseSelectionQuery','selectionQuery','semanticCell']){
  const line=source.split(/\r?\n/).find(line=>line.startsWith('function '+name+'('));assert(line,name);vm.runInContext(line,selectionContext);
}
const canonical=query=>{selectionContext.selectedCells=new Set(selectionContext.parseSelectionQuery(query));return selectionContext.selectionQuery();};
assert.equal(canonical('1,2,3, ۸۰'),'1–3, 80');assert.equal(canonical('٣، ٢، ۱، ۱-۳'),'1–3');
assert.equal(canonical('80, 1, 9–11'),'1, 9–11, 80');assert.equal(canonical(''),'');
for(const bad of ['0','81','3-2','garbage','1-99','1.5','-1','1-2-3'])assert.throws(()=>canonical(bad));
for(const value of ['1,2,3','80,1,12,11','۵، ۶، ۷، ۷','1-80','']){const once=canonical(value);assert.equal(canonical(once),once);}
assert.equal(selectionContext.semanticCell('red'),'lost');assert.equal(selectionContext.semanticCell('white'),'idle');
assert.equal(selectionContext.semanticCell('won'),'won');assert.equal(selectionContext.semanticCell(0xff0000),null,'RGB is not semantic state');
assert(source.includes('selection:[...selectedCells].sort((a,b)=>a-b)'),'One workspace persists the shared selection');
assert(source.includes('additiveSelection.checked=p.additive===true'),'Multi-select is opt-in');
assert(source.includes('showCellIndicators.checked=p.indicators!==false'),'Indicators remain on by default');
assert(source.includes("form.dataset.dirty!=='true'&&!form.contains(document.activeElement)"),'Live first-wins updates preserve edited drafts');
assert(source.includes('if(socket!==active)return'),'Stale socket owners cannot replace current state');
assert(source.includes('semanticCell(actual.cells[id-1])===semanticCell(value)'),'Cell confirmation compares semantic state, not approximate RGB');
assert(source.includes('ack.count!==ids.length'),'A partial cell batch is never presented as complete');
assert(source.includes("fallback:()=>globalThis.pakarApplicationUi?.httpFallback()!==false"),'Fallback is a live preference on the one command transport');
assert(!/\/api\/v\d/.test(source),'API routes remain unversioned');

const workspaceSource=await read('web/modules/workspace.js'),workspaceContext=vm.createContext({});
vm.runInContext(workspaceSource,workspaceContext);const workspace=workspaceContext.WebWorkspace;
assert.deepEqual(plain(workspace.normalize(null,['table','selection','palette'])),{order:['table','selection','palette'],collapsed:[],hidden:[]});
assert.deepEqual(plain(workspace.normalize({order:['palette','palette','absent'],collapsed:['table','table','absent'],hidden:['selection','unknown']},['table','selection','palette'])),{order:['palette','table','selection'],collapsed:['table'],hidden:['selection']});
assert.deepEqual(plain(workspace.normalize({order:'corrupt',collapsed:3,hidden:{}},['table'])),{order:['table'],collapsed:[],hidden:[]});
const original={order:['table','selection','palette'],collapsed:['palette'],hidden:['selection']};
assert.deepEqual(plain(workspace.move(original,'table','palette')),{...original,order:['selection','palette','table']});
assert.deepEqual(plain(workspace.move(original,'palette','table')),{...original,order:['palette','table','selection']});
assert.deepEqual(original.order,['table','selection','palette'],'Reordering leaves original state immutable');
assert.equal(workspace.move(original,'unknown','table'),original);assert.equal(workspace.move(original,'table','table'),original);
for(const rtl of [false,true]){assert.equal(workspace.offset('ArrowUp',rtl),-1);assert.equal(workspace.offset('ArrowDown',rtl),1);assert.equal(workspace.offset('ArrowLeft',rtl),rtl?1:-1);assert.equal(workspace.offset('ArrowRight',rtl),rtl?-1:1);assert.equal(workspace.offset('Enter',rtl),null);}
assert(workspaceSource.includes("'pakar-web:layout:'+owner+':'+container.id"),'Layout is peer and page scoped');
assert(workspaceSource.includes('if(!editing||e.button!==0||!e.isPrimary)return'),'Pointer reorder is editing-only');
console.log('Canonical Web contracts: scripts parse; 80-cell geometry/navigation, multilingual canonical selection, semantic readback, preference defaults, dirty drafts, peer guards and imported layout helpers passed. Not browser or hardware acceptance.');
