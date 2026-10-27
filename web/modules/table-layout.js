// Shared physical presentation from Flutter table_layout.dart. Logical IDs and
// electrical mapping remain independent; reordering only changes UI slots.
(function(root){
 function position(id){if(!Number.isInteger(id)||id<1||id>80)throw Error('Invalid cell');const n=id-1,x=2*(n%10)+Math.floor(n/10)%2;return{x,y:Math.floor(n/20)+(x%2?.5:0)};}
 function neighbor(id,key){const p=position(id),dy=key==='ArrowUp'?-1:key==='ArrowDown'?1:0,dx=key==='ArrowLeft'?-1:key==='ArrowRight'?1:0;if(!dx&&!dy)return id;return Array.from({length:80},(_,i)=>i+1).find(next=>{const n=position(next);return n.x===p.x+dx&&Math.floor(n.y)===Math.floor(p.y)+dy})||id;}
 // Adapt Pealayer timelineZoomAtPointer to a two-dimensional, fit-relative view.
 function zoomAt(view,factor,point){if(!Number.isFinite(factor)||factor<=0)factor=1;const zoom=Math.max(.5,Math.min(8,view.zoom*factor)),ratio=zoom/view.zoom;return{zoom,x:point.x-(point.x-view.x)*ratio,y:point.y-(point.y-view.y)*ratio};}
 function attach(grid){
  if(!grid||grid.dataset.zoomReady)return;grid.dataset.zoomReady='true';
  const host=grid.parentElement,viewport=document.createElement('div'),bar=document.createElement('div');
  viewport.className='table-viewport';bar.className='row table-zoom-tools';
  host.insertBefore(bar,grid);host.insertBefore(viewport,grid);viewport.append(grid);
  let view={zoom:1,x:0,y:0},fit=1,autoFit=true,gesture=false,previous=null;
  const pointers=new Map(),caption=document.createElement('output');
  function paint(){const w=1000*fit*view.zoom,h=250*fit*view.zoom;view.x=Math.max(24-w,Math.min(viewport.clientWidth-24,view.x));view.y=Math.max(24-h,Math.min(viewport.clientHeight-24,view.y));grid.style.transform=`translate(${view.x}px,${view.y}px) scale(${fit*view.zoom})`;caption.textContent=Math.round(view.zoom*100)+'%';}
  function reset(){autoFit=true;view={zoom:1,x:(viewport.clientWidth-1000*fit)/2,y:(viewport.clientHeight-250*fit)/2};paint();}
  function change(factor,p){autoFit=false;view=zoomAt(view,factor,p);paint();}
  function button(label,tip,action,glyph,iconOnly=false){const b=document.createElement('button');b.type='button';b.textContent=label;b.onclick=action;root.WebUI?.decorateControl?.(b,glyph,{iconOnly});if(!iconOnly&&typeof setUiText==='function')setUiText(b,label);if(typeof setUiAttribute==='function'){setUiAttribute(b,'aria-label',tip);setUiAttribute(b,'title',tip);}else{b.setAttribute('aria-label',tip);b.title=tip;}bar.append(b);}
  button('Fit','Fit table to panel',reset,'fit');button('−','Zoom out',()=>change(.8,{x:viewport.clientWidth/2,y:viewport.clientHeight/2}),'minus',true);bar.append(caption);button('+','Zoom in',()=>change(1.25,{x:viewport.clientWidth/2,y:viewport.clientHeight/2}),'plus',true);
  new ResizeObserver(()=>{if(!viewport.clientWidth||!viewport.clientHeight)return;const prior=fit;fit=Math.min(viewport.clientWidth/1000,viewport.clientHeight/250);if(autoFit)reset();else{view.x*=fit/prior;view.y*=fit/prior;paint();}}).observe(viewport);
  const local=e=>{const r=viewport.getBoundingClientRect();return{x:e.clientX-r.left,y:e.clientY-r.top}};
  viewport.addEventListener('wheel',e=>{if(!e.ctrlKey&&!e.metaKey)return;e.preventDefault();e.stopPropagation();change(Math.exp(-e.deltaY*(e.deltaMode===1?16:e.deltaMode===2?viewport.clientHeight:1)*.002),local(e));},{passive:false});
  function metrics(){const p=[...pointers.values()];return p.length>1?{x:(p[0].x+p[1].x)/2,y:(p[0].y+p[1].y)/2,d:Math.hypot(p[0].x-p[1].x,p[0].y-p[1].y)}:p[0];}
  viewport.addEventListener('pointerdown',e=>{if(e.pointerType!=='touch'&&e.button!==1)return;if(!pointers.size)gesture=false;pointers.set(e.pointerId,local(e));previous=metrics();if(pointers.size>1||e.button===1){gesture=true;e.preventDefault();e.stopPropagation();viewport.setPointerCapture(e.pointerId);}},{capture:true});
  viewport.addEventListener('pointermove',e=>{if(!pointers.has(e.pointerId))return;pointers.set(e.pointerId,local(e));const p=metrics();if(previous&&!gesture&&pointers.size===1&&Math.hypot(p.x-previous.x,p.y-previous.y)<6)return;if(previous&&(pointers.size>1||view.zoom>1||e.buttons===4)){gesture=true;e.preventDefault();e.stopPropagation();const next=zoomAt(view,previous.d&&p.d?p.d/previous.d:1,previous);view={...next,x:next.x+p.x-previous.x,y:next.y+p.y-previous.y};autoFit=false;paint();viewport.setPointerCapture(e.pointerId);}previous=p;},{capture:true});
  for(const name of ['pointerup','pointercancel','lostpointercapture'])viewport.addEventListener(name,e=>{pointers.delete(e.pointerId);previous=metrics();if(gesture)e.stopPropagation();},{capture:true});
  viewport.addEventListener('click',e=>{if(gesture){e.preventDefault();e.stopPropagation();gesture=false;}},{capture:true});
  return{reset};
 }
 root.WebTableLayout={position,neighbor,zoomAt,attach};
})(globalThis);
