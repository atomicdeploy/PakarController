// Adapt Pakar mobile command_wire.dart: same intent ID across WS/HTTP recovery.
function createCommandTransport({snapshot,socket,http,valid,fallback=true,now=()=>Date.now(),random=()=>crypto.getRandomValues(new Uint8Array(16))}){
  const names={'/api/cells':'cells','/api/cell':'cell','/api/game/press':'game-press','/api/game/reset':'reset','/api/game/settings':'game-settings','/api/effect':'effect','/api/mode':'mode','/api/brightness':'brightness','/api/led/output':'led-output','/api/cells/lock':'cell-lock','/api/cells/assignments':'cell-assignments','/api/led/diagnostic':'pixel-diagnostic','/api/led/presentation':'state-presentation','/api/led/preview':'color-preview'};
  const pending=new Map();
  function receive(reply){const p=pending.get(reply?.request_id);if(!p)return false;if(reply.type!=='command-result'||reply.board_id!==p.board||reply.boot_id!==p.boot)return false;p.reply=reply;p.resolve(reply);return true}
  const integer=(value,key,min=0,max=Number.MAX_SAFE_INTEGER)=>{
    if(typeof value==='string'&&!/^(?:0|[1-9]\d*)$/.test(value))throw Error('Invalid '+key);
    const n=typeof value==='number'?value:Number(value);
    if(!['number','string'].includes(typeof value)||!Number.isSafeInteger(n)||n<min||n>max)throw Error('Invalid '+key);
    return n;
  };
  const boolean=(value,key)=>{if(value===true||value==='true')return true;if(value===false||value==='false')return false;throw Error('Invalid '+key)};
  const structured=(value,key,array=false)=>{
    let result;try{result=typeof value==='string'?JSON.parse(value):JSON.parse(JSON.stringify(value));}catch{throw Error('Invalid '+key)}
    if(result===null||typeof result!=='object'||Array.isArray(result)!==array)throw Error('Invalid '+key);
    return result;
  };
  function typedValues(path,values){
    const result={...values};
    for(const key of ['id','key','index','count','rgb','group_layout_revision'])if(key in result)result[key]=integer(result[key],key);
    if(path==='/api/brightness')result.value=integer(result.value,'brightness',0,255);
    for(const key of ['enabled','off'])if(key in result)result[key]=boolean(result[key],key);
    if(path==='/api/led/diagnostic'){
      for(const[key,[min,max]]of Object.entries({period_ms:[100,10000],duty_percent:[1,99],min_brightness:[0,255],max_brightness:[0,255]}))if(key in result)result[key]=integer(result[key],key,min,max);
      if('effect' in result&&!['solid','blink','breathe','rainbow'].includes(result.effect))throw Error('Invalid diagnostic effect');
      if('composite' in result&&!['clear','overlay'].includes(result.composite))throw Error('Invalid diagnostic composite');
      if((result.min_brightness??0)>(result.max_brightness??255))throw Error('Invalid diagnostic brightness range');
    }
    if(path==='/api/led/presentation')result.settings=structured(result.settings,'presentation settings');
    if(path==='/api/led/preview'){
      if('cancel' in result)result.cancel=boolean(result.cancel,'cancel');
      if(result.cancel!==true){result.rgb=integer(result.rgb,'rgb',0,0xffffff);result.duration_ms=integer(result.duration_ms,'duration_ms',1,10000);}
    }
    if(path==='/api/cells/assignments'){
      const rows=structured(result.assignments,'cell assignments',true),seen=new Set();
      if(!rows.length||rows.length>80)throw Error('Provide 1–80 cell assignments');
      result.assignments=rows.map(row=>{
        const cell=integer(row?.cell,'cell',1,80),state=typeof row?.state==='string'?row.state.trim().toLowerCase():'';
        if(seen.has(cell)||!['idle','white','lost','red','won','off'].includes(state))throw Error('Invalid or repeated cell assignment');
        seen.add(cell);return {cell,state};
      });
    }
    if(path==='/api/game/settings'){
      const limits={enabled_mask:[1,511],idle_rgb:[0,0xffffff],win_rgb:[0,0xffffff],lose_rgb:[0,0xffffff],auto_reset_ms:[0,86400000],chase_period_ms:[400,3000]};
      for(const[key,[min,max]]of Object.entries(limits))if(key in result)result[key]=integer(result[key],key,min,max);
      for(const key of ['extended_effects','chase_neighbors','chase_invert'])if(key in result)result[key]=boolean(result[key],key);
      if('presentation' in result)result.presentation=structured(result.presentation,'game presentation');
      for(const key of ['winner_cue','reset_cue'])if(key in result&& (typeof result[key]!=='string'||!/^[a-z0-9][a-z0-9-]{0,39}$/.test(result[key])))throw Error('Invalid '+key);
    }
    return result;
  }
  async function send(path,values){
    const observed=snapshot();if(!names[path]||!valid()||!Number.isSafeInteger(observed.command_clock_ms)||observed.command_clock_ms<0||observed.command_clock_ms>Number.MAX_SAFE_INTEGER-8000)return null;
    // Capture immutable identity values, not a mutable snapshot object.
    const s={board_id:observed.board_id,boot_id:observed.boot_id,command_clock_ms:observed.command_clock_ms};
    const request_id=Array.from(random(),x=>x.toString(16).padStart(2,'0')).join('');
    if(!/^[a-f0-9]{32}$/.test(request_id)||pending.has(request_id))throw Error('Command identity unavailable');
    const packet={...typedValues(path,values),type:'command',request_id,board_id:s.board_id,boot_id:s.boot_id,expires_at_ms:s.command_clock_ms+8000,command:names[path]};
    const encoded=JSON.stringify(packet),allowFallback=typeof fallback==='function'?fallback()!==false:fallback!==false;
    const same=()=>snapshot().board_id===s.board_id&&snapshot().boot_id===s.boot_id;
    const start=now();let timer,deadlineTimer,httpController;
    const replyPromise=new Promise(resolve=>pending.set(request_id,{resolve,board:s.board_id,boot:s.boot_id}));
    try{
      let reply;const ws=socket();
      if(ws?.readyState===1&&ws.bufferedAmount<16384){
        let sent=false;try{ws.send(encoded);sent=true;}catch{/* An uncertain send reuses this exact intent only. */}
        if(sent){reply=await Promise.race([replyPromise,new Promise(resolve=>{timer=setTimeout(()=>resolve(null),800)})]);clearTimeout(timer)}
        else reply=pending.get(request_id)?.reply;
      }
      if(!same())throw Error('Board connection changed; command was not replayed');
      if(!reply){
        if(!allowFallback)throw Error('No device confirmation; HTTP fallback is disabled. Inspect device state before another action.');
        if(now()-start>7000)throw Error('Command deadline expired');
        httpController=new AbortController();
        reply=await Promise.race([replyPromise,http('/api/command',{method:'POST',body:new URLSearchParams({packet:encoded}),signal:httpController.signal}),
          new Promise((_,reject)=>{deadlineTimer=setTimeout(()=>reject(Error('Command confirmation deadline expired; inspect device state')),Math.max(1,8000-(now()-start)))})]);
      }
      if(!same()||reply?.type!=='command-result'||reply.request_id!==request_id||reply.board_id!==s.board_id||reply.boot_id!==s.boot_id)throw Error('Uncorrelated command receipt; inspect device state');
      if(reply.accepted!==true)throw Error(reply.error||'Command rejected');
      if((path==='/api/game/reset'||path==='/api/game/press')&&reply.avr_acknowledged!==true)throw Error('AVR did not acknowledge the command');
      if(reply.status?.board_id!==s.board_id||reply.status?.boot_id!==s.boot_id)throw Error('Command readback missing');
      return reply;
    }finally{clearTimeout(timer);clearTimeout(deadlineTimer);httpController?.abort();pending.delete(request_id)}
  }
  return {send,receive};
}
