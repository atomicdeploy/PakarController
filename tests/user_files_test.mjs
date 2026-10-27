import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {createHash,randomBytes} from 'node:crypto';
import vm from 'node:vm';
const source=await readFile(new URL('../assets/web/user-files.js',import.meta.url),'utf8');
const context=vm.createContext({Uint8Array,Uint32Array,DataView,Blob,TextEncoder});vm.runInContext(source,context);
const api=context.UserFiles;
for(const size of [0,1,5,55,56,63,64,65,511,512,513,131072]){
 const bytes=randomBytes(size),hash=createHash('sha256').update(bytes).digest('hex');assert.equal(api.sha256(bytes),hash);assert.deepEqual(Buffer.from(api.unhex(api.hex(bytes))),bytes);
 const entry={name:'binary.dat',size,sha256:hash,modified_ms:1720000000123};
 const archive=Buffer.from(await api.archive(entry,bytes).arrayBuffer());
 assert.equal(archive.subarray(512,512+30).toString().split('\n')[0],'24 mtime=1720000000.123');
 assert.equal(archive[156],120);assert.equal(archive[1024+156],48);
 assert.deepEqual(archive.subarray(1536,1536+size),bytes);
 for(const offset of [0,1024]){const h=Buffer.from(archive.subarray(offset,offset+512)),saved=parseInt(h.subarray(148,154).toString(),8);h.fill(32,148,156);assert.equal(h.reduce((n,b)=>n+b,0),saved);}
}
for(const name of ['../x','/access.bin','.hidden','a/b','a\\b','a%2fb','a..b',''])assert.equal(api.validName(name),false);
assert(api.validName('My-file_1.bin'));assert.throws(()=>api.unhex('0g'));
const service=await readFile(new URL('../firmware/esp/src/UsbApiService.h',import.meta.url),'utf8');
assert(service.indexOf('if(!maintenanceApiIdle())')<service.indexOf('return userFileRequest(body)'));
assert(service.indexOf('if(!accessConfig.ready()')<service.indexOf('return userFileRequest(body)'));
assert(service.includes('reply["private_file_access"]=false'));
const sd=await readFile(new URL('../firmware/esp/src/OptionalMicroSd.h',import.meta.url),'utf8');
assert(sd.includes('#define PAKAR_MICROSD_ENABLED 0'));assert(sd.includes('SD_MMC.begin("/sdcard",true,false'));
console.log('Web files: SHA-256 HTTP-origin fallback, arbitrary byte roundtrip, PAX timestamps/checksum, private path and guarded route tests passed.');

class Element{
 constructor(tag,text=''){this.tagName=tag;this.textContent=text;this.children=[];this.value='';this.disabled=false;this.files=[];}
 append(...children){this.children.push(...children);}replaceChildren(...children){this.children=children;}
 get childNodes(){return this.children;}get options(){return this.children;}setAttribute(){}remove(){}click(){}
}
const body=new Element('div'),card=new Element('section'),elements=[];
const node=(tag,cls,text)=>{const n=new Element(tag,text);elements.push(n);return n;};
const button=(label,run)=>{const n=node('button','',label);n.onclick=run;return n;};
const select=items=>{const n=node('select');for(const [value,label]of items){const o=node('option','',label);o.value=value;n.append(o);}n.value=items[0][0];return n;};
let state={board_id:'board-a',boot_id:'boot-a'},saved=[],rev=0,uploads=0,writes=0,token=44,staged,stall=false,release;
const workspace=api.create({body,card,node,button,select,snapshot:()=>state,peer:()=>({known:true,has:cap=>cap==='managed-files'}),pin:()=>({board:state.board_id,boot:state.boot_id}),api:async(path,options)=>{
 assert.equal(path,'/api/maintenance/request');const request=JSON.parse(options.body),b=request.body;assert.equal(request.board_id,state.board_id);
 if(stall){await new Promise(resolve=>release=resolve);return {accepted:true,files:[],revision:0,next_offset:0,more:false};}
 if(b.action==='list')return {accepted:true,files:saved,revision:rev,next_offset:saved.length,more:false};
 if(b.action==='begin'){uploads++;staged=b;return {accepted:true,token};}
 if(b.action==='write'){writes++;return {accepted:true,received:b.offset+b.hex.length/2};}
 if(b.action==='commit'){saved=[{name:staged.name,size:staged.size,sha256:staged.sha256,modified_ms:staged.modified_ms}];rev++;return {accepted:true,committed:true,sha256:staged.sha256,modified_ms:staged.modified_ms};}
 if(b.action==='cancel')return {accepted:true};
 throw Error('Unexpected '+b.action);
}});
workspace.update();assert.equal(uploads,0);assert(elements.find(n=>n.tagName==='select').options[1].disabled);
const reload=elements.find(n=>n.textContent==='Reload'),upload=elements.find(n=>n.textContent==='Upload file');assert(upload.disabled);
await reload.onclick();assert(!upload.disabled);const picker=elements.find(n=>n.type==='file'),bytes=randomBytes(1025);
picker.files=[{name:'test.bin',size:bytes.length,lastModified:1720000000123,arrayBuffer:async()=>bytes}];await picker.onchange();
assert.equal(uploads,1);assert.equal(writes,3);assert.equal(saved[0].sha256,api.sha256(bytes));assert.equal(saved[0].modified_ms,1720000000123);
// A late file index from the previous peer never populates the new board.
stall=true;const pending=reload.onclick();await new Promise(setImmediate);state={board_id:'board-b',boot_id:'boot-b'};workspace.update();release();await pending;
assert(upload.disabled);assert(elements.some(n=>n.textContent?.includes('Device session changed')));workspace.dispose();
console.log('Web Files workspace: explicit reads/writes, upload chunks, byte/date readback, optional-volume gate and late-peer reply rejection passed.');
