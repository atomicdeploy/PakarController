// Physical spans are shared by the editor, diagnostics and captured RGB preview.
// The bounded stepper/local draft pattern is adapted from the reviewed Pealayer
// NumericValueControl; writes remain explicit, revision-fenced board operations.
(function(root){
  'use strict';
  const path='/api/pixels/groups';
  const integer=(n,min,max)=>Number.isSafeInteger(n)&&n>=min&&n<=max;
  function defaults(role){const keys=role==='first-wins';return {revision:0,default_count:keys?12:6,group_count:keys?18:80,capacity:keys?216:480,index_base:0,overrides:[]};}
  function parse(value,role){
    const expected=defaults(role);
    if(value?.partial===true){
      if(!Array.isArray(value.counts)||value.counts.length!==expected.group_count||value.counts.some(count=>!integer(count,1,255)))throw Error('Invalid compact pixel group layout');
      value={...value,overrides:value.overrides===undefined?value.counts.map((count,index)=>({group:index+1,count})):value.overrides};
    }
    if(!value||!integer(value.revision,0,0xffffffff)||value.default_count!==expected.default_count||value.group_count!==expected.group_count||value.capacity!==expected.capacity||value.index_base!==0||!Array.isArray(value.overrides)||value.overrides.length>expected.group_count)throw Error('Invalid pixel group layout');
    const counts=Array(expected.group_count).fill(expected.default_count),seen=new Set(),overrides=[];
    for(const rule of value.overrides){if(!rule||!integer(rule.group,1,counts.length)||!integer(rule.count,0,255)||seen.has(rule.group))throw Error('Invalid pixel group override');seen.add(rule.group);if(rule.count){counts[rule.group-1]=rule.count;overrides.push({group:rule.group,count:rule.count});}}
    let used=0;const starts=counts.map(count=>{const start=used;used+=count;return start;});
    if(used>expected.capacity)throw Error('Pixel groups exceed board output capacity');
    for(const [key,actual]of [['counts',counts],['starts',starts]])if(value[key]!==undefined&&(!Array.isArray(value[key])||value[key].length!==actual.length||value[key].some((n,i)=>n!==actual[i])))throw Error('Pixel group boundaries disagree');
    if(value.used_pixels!==undefined&&value.used_pixels!==used)throw Error('Pixel group total disagrees');
    return {...expected,revision:value.revision,overrides:overrides.sort((a,b)=>a.group-b.group),counts,starts,used_pixels:used};
  }
  function fromState(state,role){
    try{if(state?.pixel_groups!==undefined)return parse(state.pixel_groups,role);
      const features=Array.isArray(state?.capabilities)?state.capabilities:state?.capabilities?.acknowledged_features;
      if(features?.includes('pixel-group-layout'))return null;
      return parse(defaults(role),role);
    }catch{return null;}
  }
  function groupAt(layout,index){if(!layout||!integer(index,0,layout.used_pixels-1))return -1;return layout.starts.findIndex((start,i)=>index>=start&&index<start+layout.counts[i]);}
  function probe(layout,index,whole,direction=0){
    if(!layout||!integer(index,0,layout.used_pixels-1))return null;
    if(!whole){index+=direction;return integer(index,0,layout.used_pixels-1)?{index,count:1}:null;}
    const current=groupAt(layout,index),group=current+direction;
    return current>=0&&integer(group,0,layout.group_count-1)?{index:layout.starts[group],count:layout.counts[group]}:null;
  }
  function frameLayout(frame,role){
    const base=defaults(role);
    if(frame?.group_counts===undefined&&frame?.group_starts===undefined&&frame?.group_layout_revision===undefined)return parse(base,role);
    if(!integer(frame?.group_layout_revision,0,0xffffffff)||!Array.isArray(frame.group_counts)||!Array.isArray(frame.group_starts)||frame.group_counts.length!==base.group_count)throw Error('Invalid captured pixel group layout');
    return parse({...base,revision:frame.group_layout_revision,overrides:frame.group_counts.map((count,index)=>({group:index+1,count})),counts:frame.group_counts,starts:frame.group_starts},role);
  }
  function create(options){
    const {card,body,node,field,numeric,button,select,snapshot,peer,api,command,pin,report,reveal}=options;
    const models=new Map(),ranges=[],translate=options.translate||((value)=>value);let owner='',model=null,disposed=false,reloadPrompt=null,renderGeneration=0;
    const identity=s=>s?.board_id&&s?.boot_id?s.board_id+':'+s.boot_id:'';
    const current=key=>!disposed&&key===owner&&identity(snapshot())===key&&peer().known&&peer().has('pixel-group-layout');
    const requireCurrent=key=>{if(!current(key))throw Error('Device session changed. No command was replayed.');};
    const nav=node('nav','feature-tabs page-tabs');nav.append(button('Pixel diagnostics',()=>reveal(options.diagnostics),'diagnostics'),button('Pixel corrections',()=>reveal(options.corrections),'corrections'),node('span','active','Pixel groups'));
    const summary=node('p','status-note'),notice=node('p','status-note'),rows=node('div','pixel-group-rules'),actions=node('div','tools row');notice.setAttribute('role','status');
    const selection=numeric(1,1,80);selection.setAttribute('aria-label','Physical group');
    const add=button('Customize group',()=>{if(!ready())return;const group=Number(selection.value);if(!selection.checkValidity()||model.rules.some(r=>r.group===group))return;model.rules.push({group,count:model.saved.default_count});changed();draw();},'plus');
    const save=button('Save to board',()=>load(true),'save','primary'),reload=button('Reload',reloadGroups,'refresh');
    actions.append(field('Physical group',selection),add,save,reload);body.append(nav,summary,rows,actions,notice);
    const ready=()=>current(owner)&&model?.saved&&!model.busy;
    const dismissReload=()=>{const prompt=reloadPrompt;reloadPrompt=null;prompt?.close();};
    async function reloadGroups(){
      const key=owner,record=model;if(!ready()||reloadPrompt)return;requireCurrent(key);
      if(record.dirty){
        if(!root.WebUI?.dialog)throw Error('Group confirmation is unavailable. Edits were kept.');
        const approved=await new Promise(resolve=>{
          const prompt=root.WebUI.dialog({title:translate('Discard pixel group edits?'),closeLabel:translate('Close'),
            description:translate('Reload keeps the saved board configuration and discards only these edits.'),
            actions:[{label:translate('Cancel'),icon:'close',autofocus:true},
              {label:translate('Reload'),icon:'refresh',variant:'primary',action:()=>resolve(true)}]});
          reloadPrompt=prompt;
          prompt.element.addEventListener('close',()=>{if(reloadPrompt===prompt)reloadPrompt=null;resolve(false);},{once:true});
        });
        if(!approved)return;
      }
      requireCurrent(key);if(model!==record)throw Error('Device session changed. Edits were kept.');
      await load();
    }
    const validCount=rule=>rule.countDraft===undefined||rule.countDraft.trim()!==''&&integer(Number(rule.countDraft),1,255);
    function proposed(){if(model.rules.some(rule=>!validCount(rule)))throw Error('Enter a pixel count from 1 to 255');return parse({...model.saved,counts:undefined,starts:undefined,used_pixels:undefined,overrides:model.rules},peer().role);}
    function changed(){model.dirty=true;sync();for(const refresh of ranges)refresh();}
    function draw(){const generation=++renderGeneration,key=owner,record=model;rows.replaceChildren();ranges.length=0;if(!model?.saved)return;
      if(!model.rules.length)rows.append(node('div','empty-state','All groups use the default pixel count'));
      model.rules.forEach((rule,index)=>{
        const row=node('div','pixel-group-rule'),count=numeric(rule.countDraft??rule.count,1,255),range=node('span','muted');count.setAttribute('aria-label','Pixel count for group '+rule.group);
        const editable=()=>current(key)&&model===record&&generation===renderGeneration&&ready()&&record.rules[index]===rule;
        const edit=value=>{if(!editable())return;rule.count=value;rule.countDraft=String(value);changed();drawRange();};
        count.oninput=()=>{if(!editable())return;rule.countDraft=count.value;if(count.checkValidity()&&validCount(rule))rule.count=Number(count.value);changed();};
        const less=button('Decrease pixel count',()=>{if(editable()&&rule.count>1){count.value=String(rule.count-1);edit(rule.count-1);}},'minus'),more=button('Increase pixel count',()=>{if(editable()&&rule.count<255){count.value=String(rule.count+1);edit(rule.count+1);}},'plus');
        for(const b of [less,more]){b.classList.add('icon-button');b.title=b.textContent;b.setAttribute('aria-label',b.textContent+' · '+rule.group);const label=b.querySelector('.button-label');if(label)label.hidden=true;}
        function drawRange(){count.setAttribute('aria-invalid',String(!validCount(rule)));try{const value=proposed(),first=value.starts[rule.group-1];range.textContent=(first+1)+'–'+(first+rule.count);}catch{range.textContent='—';}less.dataset.boundary=String(rule.count<=1);more.dataset.boundary=String(rule.count>=255);less.disabled=!ready()||rule.count<=1;more.disabled=!ready()||rule.count>=255;}
        ranges.push(drawRange);const stepper=node('div','pixel-count-stepper');stepper.append(less,count,more);
        const reset=button('Use default',()=>{if(!editable())return;record.rules.splice(index,1);changed();draw();},'refresh');row.append(node('strong','','Group '+rule.group),field('Pixels',stepper),field('Physical pixels',range),reset);rows.append(row);drawRange();
      });sync();
    }
    function sync(){const enabled=ready();let valid=false;
      try{if(model?.saved){const value=proposed();summary.textContent=value.used_pixels+' / '+value.capacity+' pixels · '+value.default_count+' default';valid=[...rows.querySelectorAll('input')].every(n=>n.value!==''&&n.checkValidity());}}catch(error){summary.textContent=translate(error.message);}
      for(const n of rows.querySelectorAll('input,button'))n.disabled=!enabled||n.dataset.boundary==='true';
      selection.disabled=!enabled;add.disabled=!enabled||model.rules.length>=model.saved.group_count;save.disabled=!enabled||!valid||!model.dirty||model.conflict;reload.disabled=!current(owner)||model?.busy===true;
      if(model?.saved)selection.max=model.saved.group_count;
      if(model?.dirty)notice.textContent=model.conflict?'Board changed elsewhere. Reload before saving.':'Unsaved changes';
    }
    async function load(write=false){const key=owner,record=model;if(!record||record.busy)return;requireCurrent(key);if(write&&(!record.saved||record.conflict))throw Error('Reload pixel groups before saving');
      const wanted=write?proposed():null;record.busy=true;record.attempted=true;sync();notice.textContent=write?'Saving and verifying…':'Reading board…';
      try{const lease=pin?.();if(write)await command(path,{configuration:JSON.stringify({revision:record.saved.revision,overrides:wanted.overrides})});requireCurrent(key);
        const result=parse(await api(path,lease?{peer:lease}:{}),peer().role);requireCurrent(key);
        if(write&&(result.revision<record.saved.revision||result.counts.some((count,i)=>count!==wanted.counts[i])))throw Error('Pixel group readback differs. No write was replayed.');
        Object.assign(record,{saved:result,rules:result.overrides.map(r=>({...r})),dirty:false,conflict:false});draw();notice.textContent=write?'Saved and verified':'Stored on this board';
      }catch(error){if(write)record.conflict=true;if(current(key))notice.textContent=error.message;report(error);}finally{record.busy=false;if(current(key))sync();}
    }
    function update(value=snapshot()){const next=identity(value);if(next!==owner){dismissReload();owner=next;model=next?(models.get(next)||{saved:null,rules:[],dirty:false,busy:false,conflict:false,attempted:false}):null;if(model){models.set(next,model);if(models.size>8)models.delete(models.keys().next().value);}notice.textContent='';summary.textContent='';draw();}
      const acknowledged=peer().known&&peer().has('pixel-group-layout');if(!acknowledged){dismissReload();card.hidden=true;}else if(!options.registerPanel)card.hidden=false;
      if(model&&acknowledged){const observed=value.pixel_groups?.revision;
        if(model.saved&&integer(observed,0,0xffffffff)&&observed>model.saved.revision){if(model.dirty)model.conflict=true;else if(!model.busy)void load();}
        if(!model.attempted&&!model.busy)void load();}
      sync();
    }
    return {update,dirty:()=>[...models.values()].some(m=>m.dirty||m.busy),dispose(){dismissReload();disposed=true;card.remove();}};
  }
  root.PixelGroups={path,defaults,parse,fromState,groupAt,probe,frameLayout,create};
})(typeof window==='undefined'?globalThis:window);
