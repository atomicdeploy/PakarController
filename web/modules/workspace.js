// Actual PCController dashboard-layout.ts / pointer-reorder.ts rules, adapted to DOM.
(function(g){
function normalize(v,ids){const unique=a=>[...new Set(Array.isArray(a)?a:[])].filter(i=>ids.includes(i));const order=unique(v?.order);order.push(...ids.filter(i=>!order.includes(i)));return{order,collapsed:unique(v?.collapsed),hidden:unique(v?.hidden)}}
function move(v,source,target){const a=v.order.indexOf(source),b=v.order.indexOf(target);if(a<0||b<0||a===b)return v;const order=v.order.filter(i=>i!==source);order.splice(order.indexOf(target)+(a<b?1:0),0,source);return{...v,order}}
function offset(k,rtl){return k==='ArrowUp'?-1:k==='ArrowDown'?1:k==='ArrowLeft'?(rtl?1:-1):k==='ArrowRight'?(rtl?-1:1):null}
function attach(container,{owner,button,icon}){
 const cards=[...container.children].filter(c=>c.classList.contains('card'));if(!cards.length)return null;
 const ids=cards.map((c,i)=>c.dataset.panelId||String(i)),key='pakar-web:layout:'+owner+':'+container.id;
 let layout=normalize(null,ids),editing=false,drag=null;try{layout=normalize(JSON.parse(localStorage.getItem(key)||'null'),ids)}catch{}
 const save=()=>{try{localStorage.setItem(key,JSON.stringify(layout))}catch{}};
 function apply(){cards.forEach((c,i)=>{const id=ids[i],collapsed=layout.collapsed.includes(id);c.style.order=layout.order.indexOf(id);c.hidden=layout.hidden.includes(id);c.classList.toggle('is-collapsed',collapsed);c.querySelector('.card-body').hidden=collapsed;c.querySelector('.panel-collapse').setAttribute('aria-expanded',String(!collapsed))});container.classList.toggle('is-editing',editing)}
 function finish(){if(drag)drag.card.classList.remove('is-dragging');drag=null;save()}
 cards.forEach((c,i)=>{const id=ids[i],header=c.querySelector('.card-header');c.dataset.layoutCard=id;const actions=document.createElement('div');actions.className='card-actions';
 const grip=button('',()=>{},'square panel-grip');grip.append(icon('grip'));grip.setAttribute('aria-label','Move '+c.querySelector('h2').textContent);
 grip.onpointerdown=e=>{if(!editing||e.button!==0||!e.isPrimary)return;e.preventDefault();grip.setPointerCapture(e.pointerId);drag={id,card:c,last:null};c.classList.add('is-dragging')};
 grip.onpointermove=e=>{if(!drag||!grip.hasPointerCapture(e.pointerId))return;const t=document.elementFromPoint(e.clientX,e.clientY)?.closest('[data-layout-card]');if(t?.parentElement!==container)return;const next=t.dataset.layoutCard;if(next&&next!==id&&next!==drag.last){drag.last=next;layout=move(layout,id,next);apply()}};
 grip.onpointerup=e=>{if(grip.hasPointerCapture(e.pointerId))grip.releasePointerCapture(e.pointerId);finish()};grip.onpointercancel=finish;grip.onlostpointercapture=finish;
 grip.onkeydown=e=>{const n=offset(e.key,document.documentElement.dir==='rtl');if(!editing||n===null)return;e.preventDefault();const visible=layout.order.filter(v=>!layout.hidden.includes(v)),target=visible[visible.indexOf(id)+n];if(target){layout=move(layout,id,target);apply();save()}};
 const collapse=button('',()=>{layout.collapsed=layout.collapsed.includes(id)?layout.collapsed.filter(v=>v!==id):[...layout.collapsed,id];apply();save()},'square panel-collapse');collapse.append(icon('chevron'));collapse.setAttribute('aria-label','Collapse or expand '+c.querySelector('h2').textContent);actions.append(grip,collapse);header.append(actions)});
 apply();return{toggle(){editing=!editing;finish();apply();return editing},reset(){layout=normalize(null,ids);apply();save()},panels:cards.map((c,i)=>({id:ids[i],label:c.querySelector('h2').textContent})),hidden:()=>layout.hidden,toggleHidden(id){layout.hidden=layout.hidden.includes(id)?layout.hidden.filter(v=>v!==id):[...layout.hidden,id];apply();save()}};
}
g.WebWorkspace={normalize,move,offset,attach};
})(globalThis);
