// Runtime interchange, not an executable worksheet. The board owns assignments.
// File suffix admission adapts Pealayer web_ui/src/preferenceFiles.ts.
function cellAssignmentFileAllowed(name) {
  return ['json','csv'].some(extension => name.toLowerCase().endsWith(`.${extension}`));
}
function normalizeAssignedState(value) {
  if(typeof value!=='string')throw Error('Each assignment needs a color state');
  const token=value.trim().toLowerCase();
  const aliases={'#ff0000':'red','#ffffff':'white','#000000':'off',idle:'white',lost:'red'};
  const state=aliases[token]??token;
  if(!['red','white','won','off'].includes(state))throw Error('Expected idle, won, lost or off');
  return state;
}
function validateCellAssignments(rows) {
  if(!Array.isArray(rows)||rows.length<1||rows.length>80)throw Error('Import between 1 and 80 cell assignments');
  const seen=new Set();
  return rows.map(row=>{
    if(!row||typeof row!=='object'||!Number.isInteger(row.cell)||row.cell<1||row.cell>80)throw Error('Cell IDs must be whole numbers from 1 to 80');
    if(seen.has(row.cell))throw Error('A cell occurs more than once in the file');
    seen.add(row.cell);return {cell:row.cell,state:normalizeAssignedState(row.state)};
  }).sort((a,b)=>a.cell-b.cell);
}
function parseAssignmentCsv(text) {
  // Bounded RFC4180 subset: quoted separators/newlines/doubled quotes accepted;
  // formulas and extra data are never interpreted or silently discarded.
  const rows=[];let row=[],field='',quoted=false,closed=false;
  const pushField=()=>{row.push(field);field='';closed=false;if(row.length>2)throw Error('CSV needs exactly cell,state columns');};
  const pushRow=()=>{pushField();if(row.some(v=>v.trim()!==''))rows.push(row);row=[];if(rows.length>81)throw Error('CSV contains more than 80 assignments');};
  for(let i=0;i<text.length;i++){
    const c=text[i];
    if(quoted){if(c==='"'){if(text[i+1]==='"'){field+='"';i++;}else{quoted=false;closed=true;}}else field+=c;continue;}
    if(c==='"'){if(field||closed)throw Error('Invalid quoted CSV field');quoted=true;}
    else if(c===',')pushField();
    else if(c==='\n'||c==='\r'){if(c==='\r'&&text[i+1]==='\n')i++;pushRow();}
    else {if(closed)throw Error('Unexpected characters after a quoted CSV field');field+=c;}
  }
  if(quoted)throw Error('Unclosed quoted CSV field');
  if(field||row.length||closed)pushRow();
  if(!rows.length||rows[0].length!==2||rows[0][0].trim().toLowerCase()!=='cell'||rows[0][1].trim().toLowerCase()!=='state')throw Error('CSV header must be cell,state');
  return rows.slice(1).map(values=>{
    const id=values[0].trim();if(!/^[1-9][0-9]?$/.test(id)||values.length!==2)throw Error('CSV contains an invalid cell row');
    return {cell:Number(id),state:values[1]};
  });
}
function parseCellAssignmentFile(name,text) {
  if(!cellAssignmentFileAllowed(name))throw Error('Choose a JSON or CSV assignments file');
  if(typeof text!=='string'||new TextEncoder().encode(text).length>65536)throw Error('Assignments file exceeds 64 KiB');
  text=text.replace(/^\uFEFF/,'');
  if(name.toLowerCase().endsWith('.csv'))return validateCellAssignments(parseAssignmentCsv(text));
  const value=JSON.parse(text);
  if(value?.role!==undefined&&value.role!=='table-selection')throw Error('This file belongs to a different board role');
  return validateCellAssignments(Array.isArray(value)?value:value?.assignments);
}
function exportCellAssignments(cells,format) {
  if(!Array.isArray(cells)||cells.length!==80||!cells.every(v=>['white','red','idle','lost','won','off'].includes(v)))throw Error('Export needs all 80 observed cell states');
  const assignments=cells.map((state,index)=>({cell:index+1,state:({white:'idle',red:'lost'})[state]??state}));
  if(format==='csv')return 'cell,state\r\n'+assignments.map(row=>`${row.cell},${row.state}`).join('\r\n')+'\r\n';
  if(format==='json')return JSON.stringify({role:'table-selection',assignments},null,2)+'\n';
  throw Error('Unsupported export format');
}
function assignmentLockConflicts(assignments,cells,redLock) {
  return redLock===true?assignments.filter(row=>['white','idle'].includes(row.state)&&['red','lost'].includes(cells?.[row.cell-1])).map(row=>row.cell):[];
}
