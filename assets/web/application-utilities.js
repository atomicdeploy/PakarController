// Shared application utilities, extracted from the redesigned workspace.
// Installation is explicit. This module owns no device, transport, polling,
// command dispatch, application bootstrap, or board-derived state.
(function (root) {
  'use strict';
  const storageKey='pakar-web-workspace-preferences';
  const defaults={density:'comfortable',labels:'colors',autoHideErrors:true,httpFallback:true};
  function normalizePreferences(value){
    const v=value&&typeof value==='object'?value:{};
    return {density:v.density==='compact'?'compact':'comfortable',labels:v.labels==='states'?'states':'colors',
      autoHideErrors:v.autoHideErrors!==false,httpFallback:v.httpFallback!==false};
  }
  function stateCaption(state,mode='colors'){
    const semantic=({white:'idle',red:'lost',idle:'idle',lost:'lost',won:'won',off:'off'})[state];
    return (mode==='states'?{idle:'Idle',lost:'Lost',won:'Won',off:'Off'}:{idle:'White',lost:'Red',won:'Green',off:'Off'})[semantic]||'Unknown';
  }
  function appendActivity(events,message,tone='info',now=Date.now()){
    const text=String(message??'').replace(/[\u0000-\u0008\u000b\u000c\u000e-\u001f\u007f]/g,'').slice(0,1024);
    if(!text.trim())return events;
    const level=['error','warning','success','info'].includes(tone)?tone:'info',last=events[0];
    const next=last?.message===text&&last.tone===level?
      [{...last,count:Math.min(9999,last.count+1),time:now},...events.slice(1)]:
      [{message:text,tone:level,time:now,count:1},...events];
    return next.slice(0,60);
  }
  function install({getPanels=()=>[],revealPanel=()=>{},translate=v=>v,onPreferences=()=>{},storage}={}){
    const doc=root.document;if(!doc)return null;
    if(doc.getElementById('applicationActivity'))return root.pakarApplicationUi||null;
    if(storage===undefined)try{storage=root.localStorage;}catch{storage=null;}
    let preferences={...defaults},events=[],disposed=false,palette=null;
    try{preferences=normalizePreferences(JSON.parse(storage?.getItem(storageKey)||'null'));}catch{}
    const node=(tag,className,text)=>{const n=doc.createElement(tag);if(className)n.className=className;if(text!==undefined)n.textContent=translate(text);return n;};
    const glyph=name=>root.WebUI?.icon?.(name);
    function button(text,icon,action,className=''){
      const b=node('button',className);b.type='button';const image=glyph(icon);if(image)b.append(image);const caption=node('span','button-label',text);caption.setAttribute('data-control-label','');root.setUiText?.(caption,text);b.append(caption);b.addEventListener('click',action);return b;
    }
    function setControlCaption(control,state){
      const text=translate(stateCaption(state,preferences.labels));
      if(control.tagName==='OPTION'){if(control.textContent!==text)control.textContent=text;return;}
      const tone=({white:'idle',red:'lost',idle:'idle',lost:'lost',won:'won',off:'off'})[state];
      if(tone)control.dataset.cellTone=tone;
      if(root.WebUI?.setControlText?.(control,text))return;
      let copy=control.querySelector('[data-cell-label-copy]')||control.querySelector('[data-control-label]');
      if(!copy){for(const child of [...control.childNodes])if(child.nodeType===3)child.remove();copy=node('span');copy.dataset.cellLabelCopy='';control.append(copy);}
      if(copy.textContent!==text)copy.textContent=text;
    }
    function refreshLabels(){
      for(const control of doc.querySelectorAll('[data-select-semantic-state]')){
        control.dataset.cellTone=control.dataset.selectSemanticState;
        const values=()=>({state:translate(stateCaption(control.dataset.selectSemanticState,preferences.labels))});
        if(root.setUiText)root.setUiText(control,'Select {state}',values);
        else{const label=translate('Select {state}').replace('{state}',values().state);if(!root.WebUI?.setControlText?.(control,label))control.textContent=label;}
      }
      for(const option of doc.querySelectorAll('#bulkState option'))setControlCaption(option,option.value);
      for(const control of doc.querySelectorAll('[data-semantic-state]'))setControlCaption(control,control.dataset.semanticState);
      for(const control of doc.querySelectorAll('button[onclick]')){
        const match=/^\s*(?:setCell|applyCellBatch)\('([a-z]+)'\)\s*;?\s*$/.exec(control.getAttribute('onclick')||'');
        if(match)setControlCaption(control,match[1]);
      }
    }
    function apply(){doc.documentElement.dataset.controlDensity=preferences.density;refreshLabels();onPreferences({...preferences});}
    function persist(next){preferences=normalizePreferences(next);let saved=Boolean(storage);try{const text=JSON.stringify(preferences);if(storage?.getItem(storageKey)!==text)storage?.setItem(storageKey,text);}catch{saved=false;}apply();return saved;}

    const drawer=node('details','application-activity');drawer.id='applicationActivity';
    const heading=node('summary'),activityIcon=glyph('activity');if(activityIcon)heading.append(activityIcon);
    const headingText=node('span','','Activity'),count=node('span','application-activity-count','0');count.id='applicationActivityCount';heading.append(headingText,count);
    const clear=button('Clear activity','close',()=>{events=[];paintEvents();},'application-activity-clear');
    const list=node('div','application-activity-list');list.setAttribute('role','log');list.setAttribute('aria-live','off');
    const controls=node('div','application-activity-tools');controls.append(clear);drawer.append(heading,controls,list);
    (doc.getElementById('workspace')||doc.querySelector('main')||doc.body).append(drawer);
    function paintEvents(){
      count.textContent=String(events.length);list.replaceChildren();
      for(const event of events){const row=node('div','application-event '+event.tone),image=glyph(event.tone==='error'?'alert':event.tone==='success'?'check':'activity');if(image)row.append(image);
        const time=node('time','',new Date(event.time).toLocaleTimeString());time.dateTime=new Date(event.time).toISOString();
        const copy=node('span','application-event-copy');copy.textContent=event.message;row.append(time,copy);
        if(event.count>1)row.append(node('span','application-activity-count','×'+event.count));list.append(row);
      }
    }
    function record(message,tone='info'){
      if(disposed)return;events=appendActivity(events,message,tone);paintEvents();if(tone==='error')drawer.open=true;
    }
    function confirmed(message){if(message)record(message,'success');if(preferences.autoHideErrors)drawer.open=false;}

    const form=doc.getElementById('appearanceForm'),open=doc.getElementById('appearanceOpen');
    const preferenceFields=new Map();let fields;
    function syncFields(){for(const[key,input]of preferenceFields)if(input.type==='checkbox')input.checked=preferences[key];else input.value=preferences[key];}
    const onSubmit=()=>{if(!form.checkValidity())return;const next={};for(const[key,input]of preferenceFields)next[key]=input.type==='checkbox'?input.checked:input.value;
      if(!persist(next)){const error=doc.getElementById('appearanceError');if(error)error.textContent=translate('Applied for this session only; browser storage is unavailable.');}};
    if(form){
      fields=node('fieldset','application-preferences');fields.append(node('legend','','Workspace'));
      for(const[key,label,options]of [['density','Control density',[['comfortable','Comfortable'],['compact','Compact']]],['labels','Cell action labels',[['colors','Colors'],['states','States']]]]){
        const row=node('label','settings-field'),input=node('select');input.id='application-'+key;
        for(const[value,title]of options){const option=node('option','',title);option.value=value;input.append(option);}
        row.append(node('span','',label),input);fields.append(row);preferenceFields.set(key,input);
      }
      for(const[key,label]of [['autoHideErrors','Hide activity after a confirmed command'],['httpFallback','Allow HTTP fallback for WebSocket commands']]){
        const row=node('label','application-check'),input=node('input');input.id='application-'+key;input.type='checkbox';row.append(input,node('span','',label));fields.append(row);preferenceFields.set(key,input);
      }
      const footer=form.querySelector('footer');if(footer)footer.before(fields);else form.append(fields);syncFields();
      open?.addEventListener('click',syncFields);form.addEventListener('submit',onSubmit);
    }

    function showPalette(){
      if(disposed||!root.WebUI?.dialog)return;
      if(palette){palette.close();palette=null;}
      const content=node('div','application-command-palette'),search=node('input'),results=node('div','application-command-results');
      search.type='search';search.placeholder=translate('Search panels…');search.setAttribute('aria-label',translate('Search panels'));content.append(search,results);
      const fill=()=>{results.replaceChildren();const query=search.value.trim().toLocaleLowerCase(),panels=getPanels();
        for(const panel of panels){if(!panel||typeof panel.id!=='string'||typeof panel.label!=='string'||panel.available===false||!translate(panel.label).toLocaleLowerCase().includes(query))continue;
          results.append(button(panel.label,panel.icon||'panels',()=>{palette?.close();palette=null;revealPanel(panel.id);},'application-command-item'));
        }
        if(!results.children.length)results.append(node('p','hint','No matching panels'));
      };
      search.addEventListener('input',fill);search.addEventListener('keydown',event=>{if(event.key==='ArrowDown'){event.preventDefault();results.querySelector('button')?.focus();}});
      results.addEventListener('keydown',event=>{if(!['ArrowUp','ArrowDown'].includes(event.key))return;event.preventDefault();const items=[...results.querySelectorAll('button')],index=items.indexOf(doc.activeElement),target=items[index+(event.key==='ArrowUp'?-1:1)];if(target)target.focus();else if(event.key==='ArrowUp')search.focus();});
      fill();palette=root.WebUI.dialog({title:translate('Go to panel'),closeLabel:translate('Close'),content,initialFocus:search,actions:[{label:translate('Close')}]});
    }
    const shortcut=event=>{if((event.ctrlKey||event.metaKey)&&event.key.toLowerCase()==='k'){event.preventDefault();showPalette();}};
    doc.addEventListener('keydown',shortcut);
    const jump=button('Go to','search',showPalette,'application-command-open');jump.title=translate('Go to panel')+' · Ctrl+K';jump.setAttribute('aria-keyshortcuts','Control+K Meta+K');
    (doc.querySelector('#workspace .top .row')||doc.querySelector('#workspace .top'))?.append(jump);
    const storageChanged=event=>{if(event.key!==storageKey)return;try{preferences=normalizePreferences(JSON.parse(event.newValue||'null'));apply();if(!doc.getElementById('appearanceDialog')?.open)syncFields();}catch{}};
    root.addEventListener?.('storage',storageChanged);apply();
    const api={record,confirmed,refreshLabels,showPalette,stateCaption:state=>translate(stateCaption(state,preferences.labels)),httpFallback:()=>preferences.httpFallback,
      preferences:()=>({...preferences}),dispose(){if(disposed)return;disposed=true;palette?.close();doc.removeEventListener('keydown',shortcut);root.removeEventListener?.('storage',storageChanged);open?.removeEventListener('click',syncFields);form?.removeEventListener('submit',onSubmit);drawer.remove();fields?.remove();jump.remove();if(root.pakarApplicationUi===api)root.pakarApplicationUi=null;}};
    root.pakarApplicationUi=api;return api;
  }
  root.PakarApplicationUtilities=Object.freeze({normalizePreferences,stateCaption,appendActivity,install});
})(globalThis);
