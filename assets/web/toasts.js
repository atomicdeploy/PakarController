// Actual RayanLamp store::showToast/dismissToast and ToastRegion/Icon contracts
// (MIT, YektaYar). DOM replaces Preact; no second framework or persistent store.
// Preserve four-item queue/serial IDs/4.5s expiry; add bounded timer ownership,
// focus/hover lifetime, safe text nodes and idempotent teardown.
function createToastController({root, document:doc=globalThis.document,
  translate=(text)=>text, setTimer=globalThis.setTimeout,
  clearTimer=(id)=>globalThis.clearTimeout?.(id)}={}){
  if(!root||!doc)throw Error('Notification region unavailable');
  let toastSerial=0,items=[],stopped=false;
  const timers=new Map(),nodes=new Map(),paused=new Map();
  root.setAttribute('aria-live','polite');root.setAttribute('aria-atomic','false');
  root.setAttribute('tabindex','-1');
  function icon(name,size=20){
    // Exact donor SVG geometry/attributes for the four notification consumers.
    const geometry={check:['m4 12 5 5L20 6'],warning:['M10.3 3.7 2.4 18a2 2 0 0 0 1.75 3h15.7a2 2 0 0 0 1.75-3L13.7 3.7a2 2 0 0 0-3.4 0Z','M12 9v4M12 17h.01'],close:['m5 5 14 14M19 5 5 19'],info:['M12 11v6M12 7h.01']};
    const svg=doc.createElementNS('http://www.w3.org/2000/svg','svg');
    for(const[key,value]of Object.entries({width:size,height:size,viewBox:'0 0 24 24',fill:'none',stroke:'currentColor','stroke-width':'1.75','stroke-linecap':'round','stroke-linejoin':'round','aria-hidden':'true'}))svg.setAttribute(key,String(value));
    if(name==='info'){const circle=doc.createElementNS(svg.namespaceURI,'circle');for(const[key,value]of Object.entries({cx:12,cy:12,r:9}))circle.setAttribute(key,String(value));svg.appendChild(circle)}
    for(const d of geometry[name]){const path=doc.createElementNS(svg.namespaceURI,'path');path.setAttribute('d',d);svg.appendChild(path)}
    return svg;
  }
  function cancel(id){if(timers.has(id)){clearTimer(timers.get(id));timers.delete(id)}}
  function arm(item){cancel(item.id);if(!stopped&&!paused.has(item.id)&&items.includes(item))timers.set(item.id,setTimer(()=>dismissToast(item.id),item.duration))}
  function pause(id,reason){const reasons=paused.get(id)||new Set();reasons.add(reason);paused.set(id,reasons);cancel(id)}
  function resume(item,reason){const reasons=paused.get(item.id);reasons?.delete(reason);if(!reasons?.size)paused.delete(item.id);arm(item)}
  function render(){
    if(stopped)return;
    const focused=items.find(item=>nodes.get(item.id)?.row.contains?.(doc.activeElement));
    const hadFocus=root.contains?.(doc.activeElement);
    const ordered=[];
    for(const item of items){
      let node=nodes.get(item.id);
      if(!node){
        const row=doc.createElement('div'),symbol=doc.createElement('span'),copy=doc.createElement('span'),title=doc.createElement('strong'),message=doc.createElement('span'),dismiss=doc.createElement('button');
        row.className='app-toast toast-'+item.tone;row.setAttribute('role','status');row.dataset.toastId=String(item.id);
        symbol.className='toast-icon';symbol.appendChild(icon(item.tone==='error'?'warning':item.tone==='success'?'check':'info'));
        copy.className='toast-copy';title.textContent=item.title;copy.appendChild(title);
        if(item.message){message.textContent=item.message;copy.appendChild(message)}
        dismiss.type='button';dismiss.className='toast-dismiss';dismiss.appendChild(icon('close',16));
        dismiss.addEventListener('click',()=>dismissToast(item.id));
        row.addEventListener('pointerenter',()=>pause(item.id,'hover'));
        row.addEventListener('pointerleave',()=>resume(item,'hover'));
        row.addEventListener('focusin',()=>pause(item.id,'focus'));
        row.addEventListener('focusout',event=>{if(!row.contains?.(event.relatedTarget))resume(item,'focus')});
        row.addEventListener('keydown',event=>{if(event.key==='Escape'){event.preventDefault();dismissToast(item.id)}});
        row.append(symbol,copy,dismiss);node={row,dismiss};nodes.set(item.id,node);
      }
      node.dismiss.setAttribute('aria-label',translate('Dismiss {title}',{title:item.title}));
      ordered.push(node.row);
    }
    root.replaceChildren(...ordered);
    // Do not steal focus when a non-focused toast expires. Keep keyboard
    // dismissal reachable after removing the focused item or queue eviction.
    if(hadFocus){const destination=(focused&&nodes.get(focused.id)?.dismiss)||nodes.get(items[0]?.id)?.dismiss||root;destination.focus?.({preventScroll:true})}
    for(const id of nodes.keys())if(!items.some(item=>item.id===id))nodes.delete(id);
  }
  function dismissToast(id){
    if(stopped)return;cancel(id);paused.delete(id);
    const next=items.filter(item=>item.id!==id);if(next.length===items.length)return;
    items=next;render();
  }
  function showToast(tone,title,message,duration=4500){
    if(stopped)return null;
    const item={id:++toastSerial,tone:['info','success','error'].includes(tone)?tone:'info',title:String(title??'').slice(0,256),message:String(message??'').slice(0,4096),duration:Number.isFinite(duration)?Math.max(100,Math.min(duration,60000)):4500};
    const evicted=items.slice(0,Math.max(0,items.length-3));
    for(const old of evicted){cancel(old.id);paused.delete(old.id)}
    items=[...items.slice(-3),item];render();arm(item);return item.id;
  }
  function destroy(){if(stopped)return;stopped=true;for(const id of timers.keys())cancel(id);items=[];nodes.clear();paused.clear();root.replaceChildren()}
  return {showToast,dismissToast,refresh:render,destroy,snapshot:()=>items.map(({duration,...item})=>({...item}))};
}
let webToasts=null;
