// Canonical Web file workspace. Adapted Pealayer ServerFilePicker's bounded
// listing, filter, explicit actions and stale-request suppression; no media paths.
(function(root){
  'use strict';
  const hex=bytes=>Array.from(bytes,n=>n.toString(16).padStart(2,'0')).join('');
  const unhex=value=>{if(typeof value!=='string'||value.length%2||!/^[0-9a-f]*$/.test(value))throw Error('Invalid file bytes');return Uint8Array.from(value.match(/../g)||[],n=>parseInt(n,16));};
  // SHA-256 also works on an ESP's plain-HTTP origin (SubtleCrypto need not).
  // No passwords/signatures are handled here; this is transfer byte integrity.
  function sha256(bytes){
    const k=[0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2];
    const h=[0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19];
    const data=new Uint8Array(Math.ceil((bytes.length+9)/64)*64);data.set(bytes);data[bytes.length]=128;
    const view=new DataView(data.buffer);view.setUint32(data.length-8,Math.floor(bytes.length/0x20000000));view.setUint32(data.length-4,bytes.length*8);
    const r=(n,s)=>(n>>>s)|(n<<(32-s)),w=new Uint32Array(64);
    for(let offset=0;offset<data.length;offset+=64){
      for(let i=0;i<16;i++)w[i]=view.getUint32(offset+4*i);
      for(let i=16;i<64;i++){const a=w[i-15],b=w[i-2];w[i]=w[i-16]+(r(a,7)^r(a,18)^(a>>>3))+w[i-7]+(r(b,17)^r(b,19)^(b>>>10));}
      let [a,b,c,d,e,f,g,z]=h;
      for(let i=0;i<64;i++){const t=(z+(r(e,6)^r(e,11)^r(e,25))+((e&f)^(~e&g))+k[i]+w[i])|0,u=((r(a,2)^r(a,13)^r(a,22))+((a&b)^(a&c)^(b&c)))|0;z=g;g=f;f=e;e=(d+t)|0;d=c;c=b;b=a;a=(t+u)|0;}
      [a,b,c,d,e,f,g,z].forEach((n,i)=>h[i]=(h[i]+n)|0);
    }
    return h.map(n=>(n>>>0).toString(16).padStart(8,'0')).join('');
  }
  const validName=name=>typeof name==='string'&&/^[A-Za-z0-9_-][A-Za-z0-9_.-]{0,63}$/.test(name)&&!name.includes('..');
  // PAX mtime retains original millisecond precision, without altering payload.
  function archive(entry,bytes){
    if(!validName(entry.name)||bytes.length!==entry.size||!Number.isSafeInteger(entry.modified_ms)||entry.modified_ms<0||sha256(bytes)!==entry.sha256)throw Error('Invalid archive metadata or bytes');
    const enc=new TextEncoder(),chunks=[];
    function add(name,payload,type,mtime){const header=new Uint8Array(512);const text=(offset,s)=>header.set(enc.encode(s),offset),oct=(offset,len,n)=>text(offset,Math.floor(n).toString(8).padStart(len-1,'0')+'\0');
      text(0,name);oct(100,8,420);oct(108,8,0);oct(116,8,0);oct(124,12,payload.length);oct(136,12,Math.min(mtime,0x1ffffffff));text(148,'        ');text(156,type);text(257,'ustar\0');text(263,'00');oct(148,7,header.reduce((a,b)=>a+b,0));header[155]=32;
      chunks.push(header,payload,new Uint8Array((512-payload.length%512)%512));}
    const value='mtime='+(entry.modified_ms/1000).toFixed(3)+'\n';let length=value.length+3;while(length!==String(length).length+1+value.length)length=String(length).length+1+value.length;
    add('PaxHeader',enc.encode(length+' '+value),'x',Math.floor(entry.modified_ms/1000));add(entry.name,bytes,'0',Math.floor(entry.modified_ms/1000));chunks.push(new Uint8Array(1024));return new Blob(chunks,{type:'application/x-tar'});
  }
  function create(o){
    const {body,node,button,select,api,pin,snapshot,peer}=o;
    let disposed=false,busy=false,generation=0,owner='',files=[],revision=0,loaded=false,prompt=null;
    const volume=select([['littlefs','LittleFS'],['sd','microSD']]),filter=node('input'),rows=node('div','file-browser-list'),notice=node('p','status-note'),progress=node('progress');
    filter.type='search';filter.placeholder='Filter files';filter.setAttribute('aria-label','Filter files');progress.max=1;progress.value=0;progress.hidden=true;notice.setAttribute('role','status');
    const picker=node('input');picker.type='file';picker.hidden=true;
    const key=()=>{const s=snapshot();return s?.board_id&&s?.boot_id?s.board_id+':'+s.boot_id:'';};
    const assertCurrent=ctx=>{if(disposed||ctx.generation!==generation||ctx.owner!==key()||ctx.volume!==volume.value||!peer().has('managed-files'))throw Error('Device session changed; transfer stopped');};
    const context=()=>({generation,owner:key(),volume:volume.value,peer:pin()});
    async function call(ctx,action,params={}){assertCurrent(ctx);const s=snapshot();const result=await api('/api/maintenance/request',{method:'POST',peer:ctx.peer,headers:{'Content-Type':'application/json'},body:JSON.stringify({board_id:s.board_id,boot_id:s.boot_id,method:'POST',path:'/api/storage/files',body:{volume:ctx.volume,action,...params}})});assertCurrent(ctx);if(result.accepted!==true)throw Error(result.error||'File operation rejected');return result;}
    function sync(){refresh.disabled=busy||!key();upload.disabled=busy||!loaded;volume.disabled=busy;progress.hidden=!busy;}
    async function task(run){if(busy)return;busy=true;notice.textContent='';progress.value=0;sync();try{const ctx=context();await run(ctx);}catch(e){notice.textContent=e.message;}finally{busy=false;sync();}}
    function draw(){rows.replaceChildren();for(const entry of files.filter(f=>f.name.toLowerCase().includes(filter.value.toLowerCase()))){const row=node('div','tools file-browser-row'),label=node('span','file-browser-name',entry.name),detail=node('span','muted',entry.size+' B · '+new Date(entry.modified_ms).toLocaleString());label.title=entry.name;
      const save=archiveMode=>task(async ctx=>{const parts=[];let offset=0;while(offset<entry.size){const n=Math.min(512,entry.size-offset),r=await call(ctx,'read',{name:entry.name,sha256:entry.sha256,offset,count:n}),bytes=unhex(r.hex);if(bytes.length!==n||r.offset!==offset)throw Error('File chunk mismatch');parts.push(bytes);offset+=n;progress.value=offset/Math.max(1,entry.size);}const bytes=new Uint8Array(entry.size);let at=0;for(const part of parts){bytes.set(part,at);at+=part.length;}if(sha256(bytes)!==entry.sha256)throw Error('File integrity mismatch');assertCurrent(ctx);const blob=archiveMode?archive(entry,bytes):new Blob([bytes],{type:'application/octet-stream'}),url=URL.createObjectURL(blob),a=node('a');a.href=url;a.download=entry.name+(archiveMode?'.tar':'');a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);notice.textContent='Download verified';});
      const download=button('Download',()=>root.WebUI.menu(download,[{label:'Download file',icon:'download',action:()=>save(false)},{label:'Download archive (.tar)',icon:'download',action:()=>save(true)}],{label:'Download options'}),'download');
      download.setAttribute('aria-haspopup','menu');row.append(label,detail,download);rows.append(row);}
      if(!rows.children.length)rows.append(node('p','muted','No files'));}
    async function list(ctx){let offset=0,found=[],r,seenRevision;do{r=await call(ctx,'list',{offset});if(!Array.isArray(r.files)||r.files.length>8||!Number.isInteger(r.revision)||r.next_offset!==offset+r.files.length||r.more&&!r.files.length||seenRevision!==undefined&&seenRevision!==r.revision)throw Error('File index changed; reload');seenRevision=r.revision;found.push(...r.files);if(found.length>32)throw Error('File index too large');offset=r.next_offset;}while(r.more);
      for(const f of found)if(!validName(f.name)||!Number.isInteger(f.size)||f.size<0||f.size>131072||!Number.isSafeInteger(f.modified_ms)||f.modified_ms<0||!/^[0-9a-f]{64}$/.test(f.sha256))throw Error('Invalid file index');files=found;revision=r.revision;loaded=true;draw();}
    const refresh=button('Reload',()=>task(list),'refresh'),upload=button('Upload file',()=>picker.click(),'upload');
    picker.onchange=()=>task(async ctx=>{const file=picker.files?.[0];picker.value='';if(!file)return;if(!validName(file.name)||file.size>131072)throw Error('Use a simple filename (letters, numbers, dash, underscore, dot), up to 128 KiB');
      const existing=files.find(f=>f.name===file.name);
      if(existing){if(!root.WebUI?.dialog)throw Error('Replacement confirmation is unavailable');
        const approved=await new Promise(resolve=>{prompt=root.WebUI.dialog({title:'Replace '+file.name+'?',description:'The previous saved file will be replaced after byte verification.',actions:[{label:'Cancel',icon:'close',autofocus:true},{label:'Replace',icon:'upload',variant:'primary',action:()=>resolve(true)}]});prompt.element.addEventListener('close',()=>{prompt=null;resolve(false);},{once:true});});
        if(!approved)return;assertCurrent(ctx);
      }
      const bytes=new Uint8Array(await file.arrayBuffer());assertCurrent(ctx);const hash=sha256(bytes),start=await call(ctx,'begin',{name:file.name,size:bytes.length,sha256:hash,modified_ms:file.lastModified,revision,...(existing?{replace_sha256:existing.sha256}:{})});
      let done=false;try{for(let offset=0;offset<bytes.length;offset+=512){const part=bytes.subarray(offset,offset+512),r=await call(ctx,'write',{token:start.token,offset,hex:hex(part)});if(r.received!==offset+part.length)throw Error('Upload offset mismatch');progress.value=r.received/Math.max(1,bytes.length);}const r=await call(ctx,'commit',{token:start.token});if(!r.committed||r.sha256!==hash||r.modified_ms!==file.lastModified)throw Error('File commit not verified');done=true;notice.textContent='File saved · bytes and date verified';}
      finally{try{await call(ctx,'cancel',{token:start.token});}catch{/* Expiring lease; never resume on another peer. */}}
      if(done)await list(ctx);
    });
    filter.oninput=draw;volume.onchange=()=>{generation++;loaded=false;files=[];draw();void task(list);};
    const tools=node('div','tools row');tools.append(volume,refresh,upload,filter);body.append(tools,picker,progress,notice,rows);
    return {update(){const next=key();if(next!==owner){owner=next;generation++;prompt?.close();files=[];loaded=false;draw();}const available=peer().known&&peer().has('managed-files');volume.options[1].disabled=!peer().has('microsd-files');if(!available)o.card.hidden=true;else if(!o.registerPanel)o.card.hidden=false;sync();},dispose(){disposed=true;generation++;prompt?.close();o.card.remove();}};
  }
  root.UserFiles={sha256,hex,unhex,validName,archive,create};
})(typeof window==='undefined'?globalThis:window);
