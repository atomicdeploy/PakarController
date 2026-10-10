// Actual RayanLamp webui/src/sync.ts scheduleReconnect/prepareReconnect/stop
// adapted to Pakar's authenticated status GET, not a Go CSRF/session endpoint.
// api.ts request's AbortController/12-second bound/finally cleanup is retained
// for that read only. No blind session refresh or mutating-request retry.
// MIT notice: desktop/licenses/RayanLamp-MIT.txt. Never replay a device write.
function createRealtimeRetry({enabled,prepare,connect,onError=()=>{},
  setTimer=(callback,delay)=>globalThis.setTimeout(callback,delay),
  clearTimer=id=>globalThis.clearTimeout?.(id),random=()=>Math.random(),
  makeAbort=()=>new AbortController()}){
  let retry=0,timer=0,epoch=0,preparing=null,request=null,requestDeadline=0;
  function cancel(reset=true){epoch++;if(timer)clearTimer(timer);timer=0;preparing=null;request?.abort();request=null;if(requestDeadline)clearTimer(requestDeadline);requestDeadline=0;if(reset)retry=0}
  function schedule(){
    if(!enabled()||timer||preparing!==null)return false;
    // Actual donor backoff/jitter; cap the exponent too for long-lived pages.
    const base=Math.min(10_000,500*2**retry);
    retry=Math.min(5,retry+1);
    const delay=base*(0.8+random()*0.4),owner=epoch;
    timer=setTimer(async()=>{
      if(owner!==epoch)return;
      timer=0;if(!enabled())return;
      preparing=owner;
      const controller=makeAbort();request=controller;
      const deadline=setTimer(()=>controller.abort(),12_000);requestDeadline=deadline;
      try{
        await prepare(controller.signal);
        if(owner===epoch&&enabled())connect();
      }catch(error){
        if(owner===epoch&&enabled()){
          preparing=null;
          try{onError(error)}catch{}
          if(owner===epoch)schedule();
        }
      }finally{clearTimer(deadline);if(request===controller){request=null;requestDeadline=0}if(preparing===owner)preparing=null}
    },delay);
    return true;
  }
  return {schedule,cancel};
}
const webSocketReconnect=createRealtimeRetry({
  enabled:()=>authenticated&&!webSocketPaused,
  prepare:signal=>refreshAuthenticated({signal}),
  connect:()=>connectWs(),
  onError:error=>traceBrowserEvent('warn','websocket.disconnected',
    'Session status refresh failed; reconnect scheduled',
    {source:'websocket',endpoint:'/api/v1/status',...browserErrorMetadata(error)},
    {dedupeKey:'websocket.prepare',dedupeWindowMs:3000})
});
