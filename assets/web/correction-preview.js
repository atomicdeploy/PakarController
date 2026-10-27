// Adapted from the fully read mobile/lib/pixel_correction_preview.dart.
// This controller owns only an explicit RAM preview lease, never a transport,
// socket, renderer, saved settings or automatic startup. The canonical editor
// must inject its existing pinned send/read functions and current owner key.
(function(root){
  'use strict';
  function createPixelCorrectionPreview({owner,allowed,configuration,pixels,project,send,read,onChange=()=>{},now=()=>Date.now(),setTimer=setTimeout,clearTimer=clearTimeout}){
    const ttl=10000,releases=new Map();let disposed=false,generation=0,captured=null,queued=false,debounce=null,renew=null,expiry=null;
    const state={enabled:false,pending:false,lease:null,observed:null,error:null};
    const snapshot=()=>({...state});
    const notify=()=>{if(!disposed)try{onChange(snapshot());}catch{/* Rendering cannot strand lease cleanup. */}};
    const same=identity=>!!identity&&owner()?.key===identity.key;
    const active=epoch=>!disposed&&state.enabled&&generation===epoch&&allowed()&&same(captured);
    function cancelTimers(){for(const id of [debounce,renew,expiry])if(id!==null)clearTimer(id);debounce=renew=expiry=null;}
    function canonical(){
      const value=configuration(),count=pixels();
      if(!value||!Number.isInteger(value.revision)||value.revision<0||value.revision>0xffffffff||typeof value.enabled!=='boolean'||!Array.isArray(value.rules))throw Error('Invalid preview configuration');
      const rules=value.rules.map(r=>({kind:r.kind,pixel:r.pixel,offset:r.offset??0,enabled:r.enabled??true}));
      // Reuse the canonical correction projection/validation, not another engine.
      const sources=project(count,value.enabled,rules);
      if(!Number.isInteger(count)||count<1||count>480||!Array.isArray(sources)||sources.length!==count||sources.some(n=>!Number.isInteger(n)||n< -1||n>=count))throw Error('Invalid correction projection');
      return {value:{revision:value.revision,enabled:value.enabled,rules},sources};
    }
    async function release(identity,lease,revision){
      if(!same(identity)||!Number.isInteger(lease)||lease<1||lease>0xffffffff)return false;
      const key=identity.key+':'+lease+':'+revision;if(releases.has(key))return releases.get(key);
      const operation=(async()=>{try{
        const reply=await send({clear:true,lease_id:lease,revision},identity);
        return same(identity)&&!reply?.error&&reply?.revision===revision&&reply?.preview?.active===false&&reply.preview.lease_id===0&&reply.preview.base_revision===revision&&reply.preview.remaining_ms===0;
      }catch{return false;}})();
      releases.set(key,operation);if(releases.size>32)releases.delete(releases.keys().next().value);return operation;
    }
    async function stop(){
      const lease=state.lease,revision=state.observed?.revision,identity=captured,inFlight=state.pending,epoch=++generation;
      state.enabled=false;state.lease=null;state.observed=null;queued=false;cancelTimers();
      if(lease!==null)state.pending=true;notify();
      if(lease!==null){const confirmed=await release(identity,lease,revision);
        if(!inFlight)state.pending=false;
        if(!confirmed&&generation===epoch&&same(identity))state.error='Preview release unconfirmed. The board lease will expire; no command was replayed.';
        notify();return confirmed;
      }return true;
    }
    async function transmit(){
      if(disposed||!state.enabled||state.pending)return;
      if(!allowed()||!same(captured)){await stop();return;}
      if(renew!==null)clearTimer(renew);renew=null;
      const epoch=generation,identity=captured,issued=state.lease,started=now();let received=issued,revision=state.observed?.revision;
      state.pending=true;state.error=null;queued=false;notify();
      try{
        const wanted=canonical();
        if(issued!==null&&wanted.value.revision!==revision)throw Error('Saved corrections changed. Preview stopped without replay.');
        revision=wanted.value.revision;
        const reply=await send({...wanted.value,ttl_ms:ttl,...(issued===null?{}:{lease_id:issued})},identity),receipt=reply?.preview;
        if(reply?.error)throw Error(String(reply.error));
        if(receipt?.active!==true||!Number.isInteger(receipt.lease_id)||receipt.lease_id<1||receipt.lease_id>0xffffffff)throw Error('Preview lease receipt missing');
        if(issued!==null&&receipt.lease_id!==issued)throw Error('Preview owner changed. No foreign preview was modified.');
        received=receipt.lease_id;
        if(!active(epoch)){await release(identity,received,revision);return;}
        const result=await read(identity),preview=result?.preview,actual=result?.active_sources;
        if(!active(epoch)){await release(identity,received,revision);return;}
        if(result?.revision!==revision||preview?.active!==true||preview.lease_id!==received||preview.base_revision!==revision||
          !Number.isInteger(preview.remaining_ms)||preview.remaining_ms<1||preview.remaining_ms>ttl||
          !Array.isArray(actual)||actual.length!==wanted.sources.length||actual.some((n,i)=>n!==wanted.sources[i]))throw Error('Preview readback differs. No preview was confirmed.');
        const remaining=Math.min(preview.remaining_ms,ttl-Math.max(0,now()-started));
        if(remaining<500)throw Error('Preview confirmation arrived too late. No preview was confirmed.');
        state.lease=received;state.observed={revision,lease_id:received,remaining_ms:remaining};
        if(expiry!==null)clearTimer(expiry);
        expiry=setTimer(()=>{state.error='Preview connection expired. No changes were saved.';void stop();},remaining);
        renew=setTimer(()=>{renew=null;if(state.pending)queued=true;else void transmit();},Math.floor(remaining/2));
      }catch(error){
        if(received!==null)await release(identity,received,revision);
        if(!disposed&&generation===epoch){state.enabled=false;state.lease=null;state.observed=null;state.error=error.message;cancelTimers();}
      }finally{
        state.pending=false;notify();
        if(queued&&active(epoch)){queued=false;edited();}
      }
    }
    function edited(){
      if(disposed||!state.enabled)return;if(debounce!==null)clearTimer(debounce);
      debounce=setTimer(()=>{debounce=null;if(state.pending)queued=true;else void transmit();},280);
    }
    async function start(){
      if(disposed||state.enabled||state.pending||!allowed())return false;
      const identity=owner();if(!identity||typeof identity.key!=='string'||!identity.key.length||identity.key.length>512)return false;
      captured=Object.freeze({...identity});state.enabled=true;state.lease=null;state.observed=null;state.error=null;generation++;
      await transmit();return state.enabled&&state.observed!==null;
    }
    function committed(){state.enabled=false;state.lease=null;state.observed=null;state.error=null;generation++;queued=false;cancelTimers();notify();}
    return {start,edited,stop,committed,snapshot,observe(){if(state.enabled&&(!allowed()||!same(captured)))void stop();},dispose(){const cleanup=stop();disposed=true;return cleanup;}};
  }
  root.createPixelCorrectionPreview=createPixelCorrectionPreview;
})(typeof window==='undefined'?globalThis:window);
