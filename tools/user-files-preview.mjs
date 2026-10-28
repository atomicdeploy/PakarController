// Isolated, loopback-only component fixture. No board endpoints or forwarding.
import http from 'node:http';
import {readFile} from 'node:fs/promises';
const root=new URL('../',import.meta.url),web=await readFile(new URL('web/index.html',root),'utf8');
const style=web.match(/<style>([\s\S]*?)<\/style>/)[1];
const components=await readFile(new URL('web/modules/components.js',root),'utf8');
const files=await readFile(new URL('assets/web/user-files.js',root),'utf8');
const html=`<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Files · isolated preview</title><style>${style}
body{padding:24px}.fixture{max-width:980px;margin:auto}.fixture .card{margin-top:16px}.fixture h1{font-size:24px}.fixture .card-body{padding:16px}
</style><body class="studio"><main class="fixture"><p class="status-note">ISOLATED UI FIXTURE · No board connected</p><h1>Files</h1><section class="card" id="userFilesCard"><h2>LittleFS</h2><div class="card-body" id="files"></div></section></main><script>${components}\n${files}
const entries=[{name:'table-scene.json',text:'{"cells":[]}\\n',modified_ms:1720000000123},{name:'commissioning-notes.txt',text:'Isolated fixture only.\\n',modified_ms:1720100000456}];
for(const f of entries){f.bytes=new TextEncoder().encode(f.text);f.size=f.bytes.length;f.sha256=UserFiles.sha256(f.bytes);}
const node=(tag,cls,text)=>{const n=document.createElement(tag);if(cls)n.className=cls;if(text!==undefined)n.textContent=text;return n;};
const select=items=>{const n=node('select');for(const [v,t]of items){const p=node('option','',t);p.value=v;n.append(p);}return n;};
const api=async(_path,request)=>{const b=JSON.parse(request.body).body;if(b.action==='list')return {accepted:true,revision:1,files:entries.map(({name,size,sha256,modified_ms})=>({name,size,sha256,modified_ms})),next_offset:entries.length,more:false};if(b.action==='read'){const f=entries.find(f=>f.name===b.name);return {accepted:true,offset:b.offset,hex:UserFiles.hex(f.bytes.subarray(b.offset,b.offset+b.count))};}return {accepted:false,error:'Preview is read-only'};};
const workspace=UserFiles.create({card:document.querySelector('section'),body:document.getElementById('files'),node,select,button:(label,run,icon)=>WebUI.button(label,run,{icon}),api,pin:()=>({board:'fixture',boot:'fixture'}),snapshot:()=>({board_id:'fixture',boot_id:'fixture'}),peer:()=>({known:true,has:cap=>cap==='managed-files'})});workspace.update();
</script></body></html>`;
const server=http.createServer((request,response)=>{if(request.url!=='/'){response.writeHead(404).end();return;}response.writeHead(200,{'Content-Type':'text/html; charset=utf-8','Cache-Control':'no-store'}).end(html);});
const port=Number(process.argv[2]||0);if(!Number.isInteger(port)||port<0||port>65535)throw Error('Invalid loopback port');
server.listen(port,'127.0.0.1',()=>console.log('Isolated Files fixture: http://127.0.0.1:'+server.address().port+'/'));
