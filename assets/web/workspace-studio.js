// PCController card-layout-controls.tsx/dashboard-layout.ts/pointer-reorder.ts:
// preserve actual panels, edit-only pointer capture, guarded targets, RTL order.
// Pealayer ControlBar/ApplicationStatusBar: contextual compact application chrome.
function studioMove(order,source,target){const from=order.indexOf(source),to=order.indexOf(target);if(from<0||to<0||from===to)return order;const next=order.filter(id=>id!==source);next.splice(next.indexOf(target)+(from<to?1:0),0,source);return next}
function studioKeyOffset(key,rtl){return key==='ArrowUp'?-1:key==='ArrowDown'?1:key==='ArrowLeft'?(rtl?1:-1):key==='ArrowRight'?(rtl?-1:1):null}
// Adapt Pealayer App.tsx surfaceFromLocation/setActiveTab/followLocation.
// The existing Studio remains the only workspace owner; fragments never route
// commands, arbitrary selectors, remote origins or board modes.
function studioRoute({location,history,onNavigate}){
 // Network/Firmware can open a guarded settings modal rather than a workspace;
 // do not advertise a half-working deep link for those modal-only consumers.
 const valid=id=>typeof id==='string'&&/^(controls|diagnostics|effects|hardware|api|all)$/.test(id);
 const read=()=>{const id=typeof location?.hash==='string'?location.hash.match(/^#\/(controls|diagnostics|effects|hardware|api|all)$/)?.[1]:null;return id||null};
 return {read,write(id){if(!valid(id)||location?.hash==='#/'+id)return false;try{history.pushState(history.state,'','#/'+id);return true}catch{return false}},
   follow(){const id=read();if(id)onNavigate(id);else if(!location?.hash)onNavigate('controls')}};
}
function studioClassify(card){
 const id=String(card.id||'').toLowerCase(),cap=card.dataset.capability||'',name=String(card.dataset.panelId||card.dataset.panelLabel||'').toLowerCase();
 if(id==='keygamecard'||cap==='game'||cap==='key-game')return'controls';
 if(/diagnostic|correction|pixel-probe/.test(id+' '+name)||['pixel-diagnostic','pixel-corrections','pixel-group-layout'].includes(cap))return'diagnostics';
 if(id==='stateappearancecard'||/presentation|effects|color-test/.test(name)||cap==='state-presentation')return'effects';
 if(['ota','isp','firmware','maintenance','files'].includes(cap)||/firmware|update|vault|storage/.test(name))return'firmware';
 if(['network','mqtt','cloud','peers','discovery'].includes(cap)||/network|cloud|wi-fi|ethernet|access/.test(name))return'network';
 if(cap==='api'||id==='apiinspectorcard'||/api-explorer|api-console|api-inspector/.test(id+' '+name))return'api';
 if(['telemetry','inputs','outputs','display','lcd','text','buzzer','status-rgb'].includes(cap))return'hardware';
 return'controls'; // Unknown cards remain reachable; never classify them away.
}
function installStudio(){
 const workspace=document.getElementById('workspace'),layout=workspace?.querySelector('.layout');if(!layout||workspace.dataset.studioInstalled)return;
 workspace.dataset.studioInstalled='true';document.body.classList.add('studio');
 const paths={controls:'M3 3h7v7H3zM14 3h7v7h-7zM3 14h7v7H3zM14 14h7v7h-7z',diagnostics:'M10 3a7 7 0 1 0 0 14 7 7 0 0 0 0-14m5 12 6 6m-14-11h6m-3-3v6',effects:'m12 3 2 7 7 2-7 2-2 7-2-7-7-2 7-2z',hardware:'M7 7h10v10H7zM9 2v5m6-5v5M9 17v5m6-5v5M2 9h5m-5 6h5m10-6h5m-5 6h5',network:'M8 3h8v6H8zM2 16h6v5H2zM16 16h6v5h-6zM12 9v4M5 16v-3h14v3',firmware:'M12 3v12m-5-5 5 5 5-5M4 16v5h16v-5',api:'m8 6-6 6 6 6m8-12 6 6-6 6m-3-15-2 18',all:'M4 5h16M4 12h16M4 19h16',chevron:'m8 10 4 4 4-4',grip:'M8 5h.01M16 5h.01M8 12h.01M16 12h.01M8 19h.01M16 19h.01',sound:'M4 9h4l5-4v14l-5-4H4zM17 8a6 6 0 0 1 0 8',led:'M8 3h8l4 6-8 12L4 9zM4 9h16',display:'M3 4h18v13H3zM8 21h8m-4-4v4',edit:'m4 16 12-12 4 4L8 20H4z',check:'m5 12 4 4L19 6',refresh:'M20 7v5h-5M4 17v-5h5M6 7a7 7 0 0 1 12-2l2 2M4 17l2 2a7 7 0 0 0 12-2'};
 function icon(key){if(globalThis.WebUI?.icon){const s=WebUI.icon(({firmware:'download',all:'panels',led:'bolt',edit:'sliders'})[key]||key);s.classList.add('studio-icon');return s}const svg=document.createElementNS('http://www.w3.org/2000/svg','svg');svg.setAttribute('viewBox','0 0 24 24');svg.setAttribute('aria-hidden','true');svg.setAttribute('focusable','false');svg.classList.add('studio-icon');const p=document.createElementNS(svg.namespaceURI,'path');p.setAttribute('d',paths[key]||paths.controls);svg.append(p);return svg}
 function copy(node,key){if(typeof setUiText==='function')setUiText(node,key);else node.textContent=key}
 function make(tag,cls){const n=document.createElement(tag);if(cls)n.className=cls;return n}
 const top=workspace.querySelector('.top'),preferences=document.getElementById('appearanceOpen');if(!top)return;
 if(preferences){top.querySelector('.row')?.append(preferences);document.querySelector('.appearance-toolbar')?.remove()}
 for(const logo of workspace.querySelectorAll('.app-logo')){logo.src='/app-icon.png';logo.width=36;logo.height=36}
 const sidebar=make('aside','studio-sidebar'),brand=make('div','studio-wordmark'),brandImage=make('img'),brandCopy=make('div');brandImage.src='/app-icon.png';brandImage.width=30;brandImage.height=30;brandImage.alt='';const brandTitle=make('strong'),brandCaption=make('small');brandTitle.textContent='PakarController';copy(brandCaption,'Workspace');brandCopy.append(brandTitle,brandCaption);brand.append(brandImage,brandCopy);
 const nav=make('nav');nav.id='studioNav';nav.setAttribute('aria-label','Workspace');sidebar.append(brand,nav);
 const content=make('div','studio-content'),heading=make('div','studio-page-heading'),pageTitle=make('h2'),pageActions=make('div','studio-page-actions');pageTitle.id='studioPageTitle';heading.append(pageTitle,pageActions);content.append(heading);
 // Keep runtime insertion anchors alive. Dynamic panels mount on .layout.
 for(const node of [...workspace.children])if(node!==top&&node!==layout&&node.id!=='applicationActivity')content.append(node);
 content.append(layout);const activity=document.getElementById('applicationActivity');if(activity)content.append(activity);workspace.append(sidebar,top,content);
 const route=studioRoute({location:window.location,history:window.history,onNavigate:id=>navigate(id,false)});
 let selected='controls',requestedPage=route.read()||'controls',editing=false,drag=null,order=[];
 const pages=[['controls','Controls'],['diagnostics','Diagnostics'],['effects','Colors & effects'],['hardware','Hardware'],['network','Network'],['firmware','Firmware & storage'],['api','API'],['all','All panels']];
 const cards=()=>[...layout.querySelectorAll(':scope > section.card')];
 const settingsPage=id=>id==='network'?'wifi':id==='firmware'?'firmware':null;
 function settingsAvailable(id){const key=settingsPage(id);if(!key||typeof DEVICE_SETTINGS==='undefined'||typeof settingsPageAvailable!=='function')return false;const page=DEVICE_SETTINGS.pages.find(p=>p.id===key);return !!page&&settingsPageAvailable(page)}
 function apply(){
  const list=cards();order=list.map(c=>c.dataset.panelId).sort((a,b)=>(panelPreferences[a]?.order??999)-(panelPreferences[b]?.order??999));
  const available=new Set(list.filter(c=>!c.hidden&&panelPreferences[c.dataset.panelId]?.hidden!==true).map(studioClassify));
  // Preserve an explicit link until its acknowledged panels arrive. Never
  // unhide a capability or discard a draft merely to satisfy a URL.
  if(requestedPage==='all'||available.has(requestedPage))selected=requestedPage;
  if(selected!=='all'&&!available.has(selected)&&available.size)selected=available.has('controls')?'controls':[...available][0];
  for(const card of list){const pref=panelPreferences[card.dataset.panelId]||{};card.classList.toggle('studio-hidden',selected!=='all'&&studioClassify(card)!==selected);card.classList.toggle('studio-user-hidden',pref.hidden===true);card.style.order=String(order.indexOf(card.dataset.panelId));}
  for(const b of nav.querySelectorAll('[data-page]')){const active=b.dataset.page===selected,opensSettings=!available.has(b.dataset.page)&&settingsAvailable(b.dataset.page);b.setAttribute('aria-pressed',String(active));b.setAttribute('aria-current',active?'page':'false');b.hidden=b.dataset.page!=='all'&&b.dataset.page!=='controls'&&!available.has(b.dataset.page)&&!opensSettings;if(opensSettings)b.setAttribute('aria-haspopup','dialog');else b.removeAttribute('aria-haspopup')}
  if(pageTitle.dataset.page!==selected){copy(pageTitle,pages.find(p=>p[0]===selected)?.[1]||'Controls');pageTitle.dataset.page=selected}content.dataset.page=selected;
 }
 function navigate(id,record=true){if(!pages.some(page=>page[0]===id))return;if(!cards().some(c=>!c.hidden&&!panelPreferences[c.dataset.panelId]?.hidden&&studioClassify(c)===id)&&settingsAvailable(id)){Promise.resolve(openDeviceSettings(settingsPage(id))).catch(error=>{if(typeof report==='function')report(error)});return}requestedPage=id;selected=id;finish();editing=false;layout.classList.remove('is-editing');customize.setAttribute('aria-pressed','false');copy(customizeLabel,'Arrange panels');apply();if(record)route.write(selected);content.scrollTop=0}
 function move(source,target){order=studioMove(order,source,target);order.forEach((id,i)=>panelPreferences[id]={...panelPreferences[id],order:i});savePanelPreferences();apply()}
 function finish(){if(drag){drag.card.classList.remove('is-dragging');drag=null}}
 function enhance(){
  globalThis.WebUI?.enhanceControls?.(document);
  const candidates=[...layout.querySelectorAll('section.card'),...content.querySelectorAll(':scope > section.card')];
  for(const card of candidates){
   if(card.parentElement!==layout)layout.append(card);
   if(card.dataset.studioReady)continue;const title=card.querySelector('h2');if(!title)continue;
   card.dataset.studioReady='true';card.dataset.panelId||=card.id||'panel-'+cards().indexOf(card);card.classList.add('studio-card');
   if(card.dataset.panelId==='table'||card.id==='keyGameCard')card.classList.add('studio-primary');
   card.dataset.studioPage=studioClassify(card);
   let header=card.querySelector('.panel-heading');if(!header){header=make('header','panel-heading');header.append(title);card.prepend(header)}
   const kind=card.dataset.capability,existingIcon=title.querySelector(':scope > svg');header.prepend(existingIcon||icon(kind==='buzzer'?'sound':['display','lcd','text'].includes(kind)?'display':kind==='leds'?'led':studioClassify(card)));
   let collapse=header.querySelector('button');if(!collapse){collapse=make('button');collapse.type='button';collapse.setAttribute('aria-expanded','true');collapse.onclick=()=>{const id=card.dataset.panelId;panelPreferences[id]={...panelPreferences[id],collapsed:!panelPreferences[id]?.collapsed};card.classList.toggle('collapsed',panelPreferences[id].collapsed);collapse.setAttribute('aria-expanded',String(!panelPreferences[id].collapsed));savePanelPreferences()};header.append(collapse)}
   collapse.classList.add('studio-square');collapse.replaceChildren(icon('chevron'));if(!collapse.getAttribute('aria-label'))collapse.setAttribute('aria-label','Collapse or expand '+title.textContent);
   const handle=make('button','studio-square studio-grip');handle.type='button';handle.append(icon('grip'));handle.setAttribute('aria-label',uiTr('Move panel')+' '+title.textContent);header.insertBefore(handle,collapse);
   handle.onpointerdown=e=>{if(!editing||e.button!==0||!e.isPrimary)return;e.preventDefault();handle.setPointerCapture(e.pointerId);drag={id:card.dataset.panelId,card,last:null};card.classList.add('is-dragging')};
   handle.onpointermove=e=>{if(!drag||!handle.hasPointerCapture(e.pointerId))return;const target=document.elementFromPoint(e.clientX,e.clientY)?.closest('.studio-card'),id=target?.dataset.panelId;if(target?.parentElement===layout&&id&&id!==drag.id&&id!==drag.last){drag.last=id;move(drag.id,id)}};
   handle.onpointerup=e=>{if(handle.hasPointerCapture(e.pointerId))handle.releasePointerCapture(e.pointerId);finish()};handle.onpointercancel=finish;handle.onlostpointercapture=finish;
   handle.onkeydown=e=>{const offset=studioKeyOffset(e.key,document.documentElement.dir==='rtl');if(!editing||offset===null)return;e.preventDefault();const visible=cards().filter(c=>!c.hidden&&!c.classList.contains('studio-hidden')&&!c.classList.contains('studio-user-hidden')).sort((a,b)=>order.indexOf(a.dataset.panelId)-order.indexOf(b.dataset.panelId)).map(c=>c.dataset.panelId),target=visible[visible.indexOf(card.dataset.panelId)+offset];if(target)move(card.dataset.panelId,target)};
   if(panelPreferences[card.dataset.panelId]?.collapsed){card.classList.add('collapsed');collapse.setAttribute('aria-expanded','false')}
  }
  const activity=document.getElementById('applicationActivity');if(activity&&(activity.parentElement!==content||content.lastElementChild!==activity))content.append(activity);
  apply();
 }
 for(const[id,label]of pages){const b=make('button');b.type='button';b.dataset.page=id;if(typeof setUiAttribute==='function'){setUiAttribute(b,'aria-label',label);setUiAttribute(b,'title',label);}else b.setAttribute('aria-label',label);const span=make('span');copy(span,label);b.append(icon(id),span);b.onclick=()=>navigate(id);nav.append(b)}
 nav.onkeydown=e=>{if(!['ArrowUp','ArrowDown','ArrowLeft','ArrowRight','Home','End'].includes(e.key))return;const list=[...nav.querySelectorAll('button:not([hidden])')],i=list.indexOf(document.activeElement);if(i<0)return;e.preventDefault();const offset=studioKeyOffset(e.key,document.documentElement.dir==='rtl')||0;list[e.key==='Home'?0:e.key==='End'?list.length-1:(i+offset+list.length)%list.length]?.focus()};
 const customize=make('button','studio-customize');customize.type='button';customize.id='studioCustomize';customize.setAttribute('aria-pressed','false');const customizeLabel=make('span');copy(customizeLabel,'Arrange panels');customize.append(icon('grip'),customizeLabel);customize.onclick=()=>{editing=!editing;finish();layout.classList.toggle('is-editing',editing);customize.setAttribute('aria-pressed',String(editing));copy(customizeLabel,editing?'Done':'Arrange panels')};pageActions.append(customize);
 enhance();WebTableLayout.attach(document.getElementById('grid'));window.studioReveal=target=>{const card=typeof target==='string'?document.querySelector('.layout [data-capability="'+CSS.escape(target)+'"]'):target;requestedPage=card?studioClassify(card):'all';selected=requestedPage;apply();route.write(selected)};
 window.addEventListener('hashchange',route.follow);window.addEventListener('popstate',route.follow);
 window.addEventListener('keydown',e=>{if(e.key==='Escape'&&editing){editing=false;finish();layout.classList.remove('is-editing');customize.setAttribute('aria-pressed','false');copy(customizeLabel,'Arrange panels')}});
 let queued=false;new MutationObserver(records=>{if(!records.some(r=>r.type==='childList'||r.attributeName==='hidden'))return;if(queued)return;queued=true;requestAnimationFrame(()=>{queued=false;enhance()})}).observe(content,{childList:true,subtree:true,attributes:true,attributeFilter:['hidden']});
 if('serviceWorker'in navigator&&window.isSecureContext)navigator.serviceWorker.register('/sw.js').catch(()=>{});
}
if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',installStudio,{once:true});else installStudio();
