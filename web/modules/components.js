/* Reusable DOM controls for the shared Web application. Semantic control density,
 * portal composition and finite toast layout follow Pealayer's appearance.ts,
 * ControlBar.tsx, DesignerFieldLabel.tsx and shared-toasts.css. This is a DOM
 * adaptation, not an import of the donor's media transport or React/Ant runtime. */
(() => {
  'use strict';
  const paths = {
    folder:'M3 6V4h7l2 3h9v13H3zM3 8h18',
    controls:'M3 3h7v7H3zM14 3h7v7h-7zM3 14h7v7H3zM14 14h7v7h-7z',
    table:'M3 4h18v16H3zM3 9h18M3 14h18M9 4v16M15 4v16',
    mapping:'M3 4h6v6H3zM13 4h8v6h-8zM3 14h10v6H3zM17 14h4v6h-4z',
    diagnostics:'M10.5 3a7.5 7.5 0 1 0 0 15 7.5 7.5 0 0 0 0-15m5.3 12.8L21 21M7 10.5h7m-3.5-3.5v7',
    search:'M10.5 3a7.5 7.5 0 1 0 0 15 7.5 7.5 0 0 0 0-15m5.3 12.8L21 21',
    effects:'m12 3 2.2 6.8L21 12l-6.8 2.2L12 21l-2.2-6.8L3 12l6.8-2.2z',
    fire:'M13 2c1 5-4 6-3 10-2-1-3-3-3-4-4 4-4 9-1 12 4 4 12 1 12-5 0-4-2-7-5-13zM12 14c-3 2-3 6 0 7 3-1 3-4 0-7z',
    hardware:'M6 6h12v12H6zM9 2v4m6-4v4M9 18v4m6-4v4M2 9h4m-4 6h4m12-6h4m-4 6h4',
    settings:'M9 3h6l.7 3 2.5 1.5 3-.9 3 5.2-2.3 2.1v2.9l2.3 2.1-3 5.2-3-.9-2.5 1.5-.7 3H9l-.7-3-2.5-1.5-3 .9-3-5.2 2.3-2.1v-2.9L-.2 9.8l3-5.2 3 .9L8.3 4z',
    sliders:'M4 5h16M4 12h16M4 19h16M8 3v4m8 3v4M10 17v4',
    network:'M8 4h8v5H8zM2 16h6v5H2zM16 16h6v5h-6zM12 9v4M5 16v-3h14v3',
    wifi:'M3 8a15 15 0 0 1 18 0M6 12a10 10 0 0 1 12 0m-9 4a5 5 0 0 1 6 0m-3 4h.01',
    bluetooth:'m7 7 10 10-5 4V3l5 4L7 17',
    usb:'M12 3v15a3 3 0 0 1-3 3m3-18-3 3m3-3 3 3M6 8v3l6 3m6-6v3l-6 3M4 6h4v2H4zm12-1h4v3h-4z',
    plug:'M8 3v5m8-5v5M6 8h12v4a6 6 0 0 1-12 0zm6 10v4',
    chevron:'m8 10 4 4 4-4',down:'m7 10 5 5 5-5',up:'m7 14 5-5 5 5',left:'m14 6-6 6 6 6',right:'m10 6 6 6-6 6',
    refresh:'M20 7v5h-5M4 17v-5h5M6 7a7 7 0 0 1 12-2l2 2M4 17l2 2a7 7 0 0 0 12-2',
    reset:'M4 4v6h6M4 10a8 8 0 1 1 2 8',
    close:'m6 6 12 12M6 18 18 6',check:'m5 12 4 4L19 6',plus:'M12 5v14M5 12h14',minus:'M5 12h14',
    more:'M5 12h.01M12 12h.01M19 12h.01',grip:'M9 5h.01M15 5h.01M9 12h.01M15 12h.01M9 19h.01M15 19h.01',
    eye:'M2 12s4-7 10-7 10 7 10 7-4 7-10 7-10-7-10-7zm13 0a3 3 0 1 1-6 0 3 3 0 0 1 6 0',
    eyeOff:'m3 3 18 18M10 5a12 12 0 0 1 12 7 15 15 0 0 1-4 5M6 6a15 15 0 0 0-4 6s4 7 10 7c1.4 0 2.8-.4 4-1M9 9a4 4 0 0 0 6 6',
    fit:'M9 3H3v6m12-6h6v6M3 15v6h6m12-6v6h-6M8 8h8v8H8z',
    lock:'M6 10h12v11H6zM8 10V7a4 4 0 0 1 8 0v3m-4 5v2',
    unlock:'M6 10h12v11H6zM8 10V7a4 4 0 0 1 7.5-2m-3.5 10v2',
    power:'M12 2v10M6 5a9 9 0 1 0 12 0',
    sound:'M3 9h4l5-5v16l-5-5H3zm13-1a6 6 0 0 1 0 8m3-11a10 10 0 0 1 0 14',
    melody:'M9 18V5l11-2v13M9 9l11-2M9 18a3 3 0 1 1-3-3c1.7 0 3 1.3 3 3m11-2a3 3 0 1 1-3-3c1.7 0 3 1.3 3 3',
    play:'m8 4 12 8-12 8z',stop:'M6 6h12v12H6z',
    info:'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18m0 8v6m0-10h.01',
    warning:'m12 3 10 18H2zm0 6v5m0 3h.01',
    download:'M12 3v12m-5-5 5 5 5-5M4 16v5h16v-5',upload:'M12 16V4m-5 5 5-5 5 5M4 16v5h16v-5',
    save:'M4 3h13l4 4v14H3V3zm3 0v6h10V3M7 21v-8h10v8',
    copy:'M8 8h13v13H8zM16 8V3H3v13h5',
    trash:'M3 6h18M9 6V3h6v3M5 6l1 15h12l1-15M10 10v7m4-7v7',
    code:'m8 6-6 6 6 6m8-12 6 6-6 6m-3-15-2 18',
    terminal:'m4 6 6 6-6 6m9 0h7',api:'m8 8-4 4 4 4m8-8 4 4-4 4m-3-11-2 14',
    history:'M3 11a9 9 0 1 1 3 8M3 4v7h7m2-5v6l4 2',
    monitor:'M3 4h18v13H3zm9 13v4m-5 0h10',
    sun:'M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8m0-6v2m0 16v2M2 12h2m16 0h2M5 5l1 1m12 12 1 1M5 19l1-1M18 6l1-1',
    moon:'M20 15a9 9 0 0 1-11-11 9 9 0 1 0 11 11',
    select:'M8 3H3v5m13-5h5v5M3 16v5h5m13-5v5h-5m-8-9 3 3 6-6',
    invert:'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18m0 0v18',
    palette:'M12 3a9 9 0 1 0 0 18h1a2 2 0 0 0 1-4 1.5 1.5 0 0 1 1-3h3a3 3 0 0 0 3-3 9 9 0 0 0-9-8M7 10h.01M10 6h.01M15 7h.01M6 14h.01',
    bolt:'m13 2-9 12h7l-1 8 10-13h-7z',
    corrections:'M4 6h16M4 12h16M4 18h16M8 4v4m8 2v4M10 16v4',
    display:'M3 5h18v14H3zm3 3h4v3H6zm8 0h4v3h-4zM6 14h4m4 0h4',
    game:'M7 6h10a4 4 0 0 1 4 4v7a2 2 0 0 1-3 1l-3-3H9l-3 3a2 2 0 0 1-3-1v-7a4 4 0 0 1 4-4m0 4v4m-2-2h4m6-1h.01m3 2h.01',
    keys:'M3 4h18v16H3zM6 8h2m3 0h2m3 0h2M6 12h2m3 0h2m3 0h2M7 16h10',
    pointer:'m5 3 14 9-6 1-3 6zM13 13l5 8',
    circle:'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18',
    selectAll:'M8 3H3v5m13-5h5v5M3 16v5h5m13-5v5h-5M8 8h8v8H8z',
    panels:'M3 4h18v16H3zM9 4v16M9 14h12',
    alert:'m12 3 10 18H2zm0 6v5m0 3h.01',
    activity:'M2 12h5l3-8 4 16 3-8h5',
    preview:'M2 12s4-7 10-7 10 7 10 7-4 7-10 7-10-7-10-7zm13 0a3 3 0 1 1-6 0 3 3 0 0 1 6 0',
  };
  // Gear with conservative 24-unit bounds, shared with menu settings actions.
  paths.settings='M10 3h4l.6 2.5 2.1 1.2 2.5-.8 2 3.4-1.9 1.8v2.5l1.9 1.8-2 3.4-2.5-.8-2.1 1.2L14 21h-4l-.6-2.5-2.1-1.2-2.5.8-2-3.4 1.9-1.8v-2.5L2.8 8.6l2-3.4 2.5.8 2.1-1.2zM12 9a3 3 0 1 0 0 6 3 3 0 0 0 0-6';
  const node=(tag,cls,text)=>{const n=document.createElement(tag);if(cls)n.className=cls;if(text!==undefined)n.textContent=String(text);return n;};
  const icon=name=>{const s=document.createElementNS('http://www.w3.org/2000/svg','svg');s.classList.add('icon');for(const[key,value]of Object.entries({viewBox:'0 0 24 24',width:'18',height:'18',fill:'none',stroke:'currentColor','stroke-width':'1.8','stroke-linecap':'round','stroke-linejoin':'round','aria-hidden':'true',focusable:'false'}))s.setAttribute(key,value);const p=document.createElementNS(s.namespaceURI,'path');p.setAttribute('d',paths[name]||paths.controls);s.append(p);return s;};
  // PCController components.tsx::Button keeps the glyph and copy in distinct
  // children. Move existing text nodes instead of recreating them: the shared
  // localizer owns those nodes, and translations must never erase the glyph.
  function decorateControl(control,name,{iconOnly=false}={}) {
    if(!control||!['BUTTON','SUMMARY'].includes(control.tagName))return null;
    let label=control.querySelector(':scope > [data-control-label]');
    if(!label){
      label=node('span','button-label');label.setAttribute('data-control-label','');
      for(const child of [...control.childNodes])if(child.nodeType!==1||child.tagName.toLowerCase()!=='svg')label.append(child);
      control.append(label);
    }
    control.classList.add('button');control.dataset.controlIcon=name;
    if(control.classList.contains('primary'))control.classList.add('button--primary');
    if(!control.querySelector(':scope > svg'))control.prepend(icon(name));
    if(iconOnly){
      control.classList.add('icon-button');label.hidden=true;
      if(!control.getAttribute('aria-label'))control.setAttribute('aria-label',label.textContent.trim()==='×'?'Close':label.textContent.trim());
    }
    return label;
  }
  function setControlText(control,text){
    const label=control?.querySelector?.(':scope > [data-control-label]');
    if(!label)return false;
    const previous=label.textContent;
    if(previous!==String(text))label.textContent=String(text);
    if(control.classList.contains('icon-button')||control.getAttribute('aria-label')===previous)control.setAttribute('aria-label',String(text));
    return true;
  }
  const actionIcons={refresh:'refresh',openDeviceSettings:'hardware',openCellManager:'sliders',selectAllCells:'selectAll',clearCellSelection:'close',resetCellOrder:'reset',resetGame:'reset',showDisplay:'display',showMarquee:'display',playTone:'play',playCue:'melody',saveMapping:'save',setCell:'palette',setPattern:'check',startStream:'play',stopStream:'stop',scanWifi:'wifi',applyNetwork:'save',applyCloud:'save',remoteRefresh:'refresh',remoteAction:'download',loadTelemetry:'activity',uploadAvr:'upload',programAvr:'hardware',closeDeviceSettings:'close',settingsReload:'refresh',settingsReveal:'panels',applyCellBatch:'check',reconnectWebSocket:'plug',pauseWebSocket:'power',signOut:'power',bootstrap:'plug'};
  const controlIcons={appearanceOpen:'settings',appearanceClose:'close',appearanceCancel:'close',connectionRetry:'plug',displayStop:'stop',nameSave:'save',remoteStage:'download',remoteApply:'download',remoteCancel:'close',boardSilentRefresh:'refresh',rainbowTest:'palette',networkSave:'save',cloudSave:'save',ntfySave:'save',ethernetRead:'refresh',ethernetSave:'save',settingsReload:'refresh',audioPlay:'melody',stateSave:'save',stateReload:'refresh',statePreviewStop:'stop'};
  function enhanceControls(root=document){
    for(const control of root.querySelectorAll('button')){
      if(control.matches('.cell,[data-cell],[data-row],.key-live-grid button,[role=switch],.web-menu-item')||control.querySelector(':scope > svg'))continue;
      const action=(control.getAttribute('onclick')||'').match(/^\s*([\w]+)\s*\(/)?.[1];
      const close=control.textContent.trim()==='×';
      const name=controlIcons[control.id]||actionIcons[action]||(close?'close':null)||
        (control.type==='submit'&&control.closest('form')?'check':null);
      if(name)decorateControl(control,name,{iconOnly:close});
    }
    for(const control of root.querySelectorAll('.connection-menu > summary')){
      if(control.querySelector(':scope > svg'))continue;
      decorateControl(control,control.parentElement.querySelector('#webPanelsMenu')?'panels':'wifi');
    }
  }
  function button(label,onClick,options={}) {
    const b=node('button',`button ${options.variant?'button--'+options.variant:''} ${options.iconOnly?'icon-button':''} ${options.className||''}`.trim());
    b.type='button';b.disabled=options.disabled===true;
    if(options.icon)b.append(icon(options.icon));
    if(!options.iconOnly){const caption=node('span','button-label',label);caption.setAttribute('data-control-label','');b.append(caption);}
    b.setAttribute('aria-label',options.label||label);if(options.title||options.iconOnly)b.title=options.title||label;
    b.addEventListener('click',event=>Promise.resolve().then(()=>onClick?.(event)).catch(error=>toast(error.message||String(error),{tone:'error'})));
    return b;
  }
  function badge(label,tone='neutral'){const b=node('span','badge',label);b.dataset.tone=tone;return b;}
  let openMenu=null;
  function menu(anchor,items,{label='Actions'}={}) {
    openMenu?.();const panel=node('div','web-menu');panel.setAttribute('role','menu');panel.setAttribute('aria-label',label);const previous=document.activeElement;
    const close=(restore=true)=>{panel.remove();document.removeEventListener('pointerdown',outside,true);document.removeEventListener('keydown',keys,true);window.removeEventListener('resize',resize);if(restore&&previous?.isConnected)previous.focus();anchor?.setAttribute?.('aria-expanded','false');if(openMenu===close)openMenu=null;};
    const run=item=>{close();return item.action?.();};
    for(const item of items){if(item.separator){panel.append(node('div','web-menu-separator'));continue;}if(item.heading){panel.append(node('div','web-menu-label',item.heading));continue;}const b=button(item.label,()=>run(item),{className:'web-menu-item',disabled:item.disabled});const copy=node('span',item.description?'menu-copy':'',item.label);if(item.description)copy.append(node('small','',item.description));b.replaceChildren(icon(item.icon||'controls'),copy,node('span','shortcut',typeof item.checked==='boolean'?(item.checked?'✓':''):item.shortcut||''));b.setAttribute('role',typeof item.checked==='boolean'?'menuitemcheckbox':'menuitem');if(typeof item.checked==='boolean')b.setAttribute('aria-checked',String(item.checked));if(item.danger)b.dataset.danger='true';panel.append(b);}
    const outside=e=>{if(!panel.contains(e.target)&&!anchor?.contains?.(e.target))close(false);};
    const keys=e=>{const list=[...panel.querySelectorAll('button:not(:disabled)')],i=list.indexOf(document.activeElement);if(e.key==='Escape'){e.preventDefault();close();}else if(['ArrowDown','ArrowUp','Home','End'].includes(e.key)){e.preventDefault();const next=e.key==='Home'?0:e.key==='End'?list.length-1:(i+(e.key==='ArrowDown'?1:-1)+list.length)%list.length;list[next]?.focus();}else if(e.key==='Tab')close(false);};
    const resize=()=>close(false);document.body.append(panel);const r=anchor?.getBoundingClientRect?.()||{left:16,bottom:16};const pr=panel.getBoundingClientRect();panel.style.left=Math.max(8,Math.min(r.left,innerWidth-pr.width-8))+'px';panel.style.top=Math.max(8,Math.min(r.bottom+6,innerHeight-pr.height-8))+'px';anchor?.setAttribute?.('aria-expanded','true');document.addEventListener('pointerdown',outside,true);document.addEventListener('keydown',keys,true);window.addEventListener('resize',resize);panel.querySelector('button:not(:disabled)')?.focus();openMenu=close;return {element:panel,close};
  }
  let dialogSequence=0;
  function dialog({title,description,content,actions=[],className='',closeLabel='Close',initialFocus}) {
    const d=node('dialog',`web-dialog ${className}`),h=node('header','web-dialog-header'),copy=node('div'),heading=node('h2','',title),body=node('div','web-dialog-body'),footer=node('div','web-dialog-actions'),previous=document.activeElement;
    heading.id=`web-dialog-${++dialogSequence}`;d.setAttribute('aria-labelledby',heading.id);copy.append(heading);
    if(description){const detail=node('p','',description);detail.id=heading.id+'-description';d.setAttribute('aria-describedby',detail.id);copy.append(detail);}
    let pending=false,retired=false,focusTarget=initialFocus;
    const close=()=>{if(!retired&&d.open)d.close();};
    h.append(copy,button(closeLabel,close,{icon:'close',iconOnly:true,variant:'ghost',className:'small'}));if(content)body.append(content);
    for(const action of actions){
      const control=button(action.label,async()=>{
        if(pending||retired||!d.open)return;
        pending=true;d.setAttribute('aria-busy','true');
        const disabled=[...footer.children].map(b=>b.disabled);for(const b of footer.children)b.disabled=true;
        try{const result=await action.action?.(d);if(action.close!==false&&result!==false)close();}
        finally{pending=false;d.removeAttribute('aria-busy');for(const [i,b] of [...footer.children].entries())b.disabled=disabled[i];}
      },{icon:action.icon,variant:action.variant,disabled:action.disabled});
      footer.append(control);if(action.autofocus&&!focusTarget)focusTarget=control;
    }
    d.append(h,body);if(actions.length)d.append(footer);document.body.append(d);
    d.addEventListener('close',()=>{retired=true;d.remove();if(previous?.isConnected)previous.focus();},{once:true});
    d.addEventListener('click',e=>{if(e.target===d){const r=d.getBoundingClientRect();if(e.clientX<r.left||e.clientX>r.right||e.clientY<r.top||e.clientY>r.bottom)close();}});
    d.showModal();if(focusTarget&&d.contains(focusTarget)&&!focusTarget.disabled)focusTarget.focus();return {element:d,body,close};
  }
  let toastRegion=null;const toasts=new Map();
  function toast(message,{tone='info',key=String(message),duration=6000}={}) {
    if(toasts.has(key))return toasts.get(key);
    if(!toastRegion){toastRegion=node('div','web-toast-region');toastRegion.setAttribute('aria-live','polite');document.body.append(toastRegion);}
    while(toastRegion.children.length>=4)toastRegion.firstChild?.dismiss?.();
    const t=node('div','web-toast');t.dataset.tone=tone;t.setAttribute('role',tone==='error'?'alert':'status');let timeout;
    const close=()=>{clearTimeout(timeout);toasts.delete(key);t.remove();};t.dismiss=close;t.append(icon(tone==='error'||tone==='warning'?'warning':tone==='success'?'check':'info'),node('div','',message),button('Dismiss',close,{icon:'close',iconOnly:true,variant:'ghost'}));toastRegion.append(t);toasts.set(key,{element:t,close});if(duration>0)timeout=setTimeout(close,duration);return toasts.get(key);
  }
  function segmented(options,value,onChange,{label='Choose an option'}={}) {
    const group=node('div','segmented');group.setAttribute('role','group');group.setAttribute('aria-label',label);for(const option of options){const o=Array.isArray(option)?{value:option[0],label:option[1]}:option;const b=button(o.label,()=>{for(const p of group.children)p.setAttribute('aria-pressed',String(p===b));return onChange(o.value);},{icon:o.icon});b.setAttribute('aria-pressed',String(o.value===value));group.append(b);}return group;
  }
  function toggle(label,value,onChange) {
    const row=node('div','web-switch-row'),copy=node('span','',label),b=node('button','web-switch');b.type='button';b.setAttribute('role','switch');b.setAttribute('aria-label',label);b.setAttribute('aria-checked',String(value));b.addEventListener('click',()=>{const next=b.getAttribute('aria-checked')!=='true';b.setAttribute('aria-checked',String(next));Promise.resolve().then(()=>onChange(next)).catch(e=>{b.setAttribute('aria-checked',String(!next));toast(e.message||String(e),{tone:'error'});});});row.append(copy,b);return row;
  }
  function tooltip(element,text){element.title=text;let tip,timer;const hide=()=>{clearTimeout(timer);tip?.remove();tip=null;};const show=()=>{hide();timer=setTimeout(()=>{tip=node('div','web-tooltip',text);document.body.append(tip);const r=element.getBoundingClientRect(),t=tip.getBoundingClientRect();tip.style.left=Math.max(8,Math.min(r.left+(r.width-t.width)/2,innerWidth-t.width-8))+'px';tip.style.top=Math.max(8,r.top-t.height-7)+'px';},450);};element.addEventListener('mouseenter',show);element.addEventListener('focus',show);element.addEventListener('mouseleave',hide);element.addEventListener('blur',hide);element.addEventListener('pointerdown',hide);return hide;}
  globalThis.WebUI=Object.freeze({icon,button,badge,menu,dialog,toast,segmented,switch:toggle,tooltip,decorateControl,setControlText,enhanceControls});
})();
