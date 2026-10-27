import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import vm from 'node:vm';
const source=await readFile(new URL('../assets/web/cell-assignments.js',import.meta.url),'utf8');
const ctx=vm.createContext({TextEncoder});vm.runInContext(source,ctx);
const invoke=(name,...args)=>JSON.parse(JSON.stringify(ctx[name](...args)));
const cells=Array.from({length:80},(_,i)=>['red','white','off'][i%3]);
for(const format of ['json','csv']){
  const file=invoke('exportCellAssignments',cells,format);
  assert.deepEqual(invoke('parseCellAssignmentFile','cells.'+format,file),cells.map((state,i)=>({cell:i+1,state})));
}
assert.deepEqual(invoke('parseCellAssignmentFile','CELLS.CSV','\uFEFFcell,state\r\n"2","#FF0000"\r\n1,off\r\n'),[{cell:1,state:'off'},{cell:2,state:'red'}]);
assert.deepEqual(invoke('assignmentLockConflicts',[{cell:1,state:'white'},{cell:2,state:'off'}],cells,true),[1]);
assert.deepEqual(invoke('assignmentLockConflicts',[{cell:1,state:'white'}],cells,false),[]);
for(const text of ['cell,state\n1,red\n1,white','cell,state\n0,red','cell,state\n81,red','cell,state\n1,=RED()','cell,state\n1,blue','cell,state\n1,red,off','cell,state\n"1"oops,red','cell,state\n1,"red'])
  assert.throws(()=>ctx.parseCellAssignmentFile('cells.csv',text),text);
for(const value of [{role:'first-wins',assignments:[{cell:1,state:'red'}]},{assignments:[]},{assignments:[{cell:'1',state:'red'}]},{assignments:Array.from({length:81},(_,i)=>({cell:i+1,state:'red'}))}])
  assert.throws(()=>ctx.parseCellAssignmentFile('cells.json',JSON.stringify(value)));
assert.throws(()=>ctx.parseCellAssignmentFile('cells.xlsx','fake workbook'));
assert.throws(()=>ctx.parseCellAssignmentFile('cells.csv',' '.repeat(65537)));
assert.throws(()=>ctx.exportCellAssignments(cells.slice(1),'csv'));
assert.throws(()=>ctx.exportCellAssignments([...cells.slice(1),'unknown'],'csv'));
const web=await readFile(new URL('../firmware/esp/src/WebUi.h',import.meta.url),'utf8');
assert(web.includes(source.trim()),'Web must embed the tested canonical codec');
assert(web.includes("peerHttp.assertOwner(assignmentImport.peer)"),'Import preview must retain the original board/boot owner');
assert(web.includes("'/api/cells/assignments'"));
assert(web.includes("'/api/cells/lock'"));
// Execute the actual confirmed-import handler, not a second transport.
const handler=web.slice(web.indexOf('async function applyAssignmentImport(){'),web.indexOf('async function setRedCellLock('));
for(const failure of [null,'receipt','readback','owner','network']){
  const owner={board:'fixture',boot:'a'.repeat(32)},calls=[],errors=[],button={},errorLabel={};let rendered=0,closed=0;
  const observed=Array(80).fill('white');observed[0]='red';
  const fixture=vm.createContext({URLSearchParams,
    peerHttp:{assertOwner(peer){assert.equal(peer,owner);if(failure==='owner')throw Error('Board changed');}},
    assignmentDialog:()=>({querySelector:selector=>selector==='#assignmentError'?errorLabel:button,close(){closed++;}}),
    renderAssignmentTools(){},peerAvailability:()=>({has:()=>true}),render(){rendered++;},setUiText(){},bulkStatus:{},report:error=>errors.push(error),
    api:async(path,options)=>{
      assert.equal(options.peer,owner);calls.push(path);
      if(path==='/api/status')return {cells:failure==='readback'?Array(80).fill('white'):observed};
      assert.equal(path,'/api/cells/assignments');assert.equal(options.method,'POST');
      assert.deepEqual(JSON.parse(options.body.get('assignments')),[{cell:1,state:'red'}]);
      if(failure==='network')throw Error('Response lost');
      return {accepted:failure!=='receipt',count:1};
    },owner,
  });
  vm.runInContext('let assignmentBusy=false,cellBatchBusy=false,assignmentImport={peer:owner,assignments:[{cell:1,state:"red"}]};'+handler,fixture);
  await fixture.applyAssignmentImport();
  assert.equal(vm.runInContext('assignmentBusy',fixture),false);
  assert.equal(calls.filter(path=>path!=='/api/status').length,failure==='owner'?0:1,'Never replay an uncertain import');
  assert.equal(rendered,failure===null?1:0);assert.equal(closed,failure===null?1:0);
  assert.equal(errors.length,failure===null?0:1);
}
console.log('Web assignments: JSON/CSV, strict validation, lock conflicts, actual atomic import ACK/readback, no replay and peer fencing passed.');
