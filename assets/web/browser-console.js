// Actual RayanLamp webui/src/browser-console.ts (MIT, YektaYar), adapted.
// Allowlisted operational events only: no global console interception, raw
// responses, frames, request bodies, credentials or persistent log storage.
const browserEventPresentation={
  'app.start':'🚀','transport.ready':'🔌','transport.error':'⚠️',
  'device.command':'⚡','websocket.connected':'📡','websocket.disconnected':'📡',
  'message.received':'💬','audio.cue':'🔊','audio.error':'🔇','device.event':'📟',
  'diagnostics.loaded':'🩺','diagnostics.error':'⚠️','settings.saved':'✅','settings.error':'⚠️'
};
const browserMetadataAllowlist=new Set(['source','action','state','attempt','elapsed_ms','count','endpoint','bytes','error_code']);
// Include camelCase keys missed by the donor's token-boundary-only expression.
const browserSensitiveKey=/(authorization|cookie|credential|csrf|password|passwd|passphrase|secret|session|token|api[_-]?key)/i;
function redactBrowserText(value) {
  const clean=String(value).replace(/[\r\n\t]+/g,' ')
    .replace(/[\u0000-\u0008\u000b\u000c\u000e-\u001f\u007f-\u009f\u202a-\u202e\u2066-\u2069]/g,'')
    .replace(/(\b[a-z][a-z0-9+.-]*:\/\/)[^/\s:@]+:[^/\s@]+@/gi,'$1[REDACTED]@')
    .replace(/\bBearer\s+[A-Za-z0-9._~+/-]+=*/gi,'Bearer [REDACTED]')
    .replace(/\beyJ[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]{8,}\b/g,'[REDACTED JWT]')
    .replace(/([?&](?:authorization|csrf|password|secret|session|token|api[_-]?key)=)[^&#\s]*/gi,'$1[REDACTED]');
  return clean.length<=500?clean:clean.slice(0,500)+'…';
}
function browserErrorMetadata(error) {
  if(!(error instanceof Error))return {error_code:'unknown_error'};
  const value=error.code??(Number.isFinite(error.status)?'http_'+error.status:error.name||'Error');
  return {error_code:typeof value==='number'&&Number.isFinite(value)?value:
    typeof value==='string'?redactBrowserText(value):'unknown_error'};
}
function sanitizeBrowserValue(value,seen=new WeakSet(),depth=0) {
  if(value===null||typeof value==='boolean')return value;
  if(typeof value==='number')return Number.isFinite(value)?value:'[Nonfinite number]';
  if(typeof value==='string')return redactBrowserText(value);
  if(typeof value==='bigint')return redactBrowserText(value+'n');
  if(typeof value==='undefined')return undefined;
  if(typeof value==='symbol')return redactBrowserText(value.description??'symbol');
  if(typeof value==='function')return '[Function]';
  if(value instanceof Error)return browserErrorMetadata(value);
  if(depth>=4)return '[Maximum depth]';
  if(seen.has(value))return '[Circular]';
  seen.add(value);
  try {
    if(Array.isArray(value)){
      const result=value.slice(0,20).map(v=>sanitizeBrowserValue(v,seen,depth+1));
      if(value.length>20)result.push('['+(value.length-20)+' more items]');
      return result;
    }
    if(ArrayBuffer.isView(value))return '[Typed buffer '+value.byteLength+' bytes]';
    if(value instanceof ArrayBuffer)return '[ArrayBuffer '+value.byteLength+' bytes]';
    if(value instanceof Date)return Number.isFinite(value.getTime())?value.toISOString():'[Invalid date]';
    if(typeof Element!=='undefined'&&value instanceof Element)return '<'+redactBrowserText(value.tagName.toLowerCase())+'>';
    const result=Object.create(null),entries=Object.entries(value);
    for(const[key,item]of entries.slice(0,30)){
      if(key==='__proto__'||key==='constructor'||key==='prototype')continue;
      result[redactBrowserText(key)]=browserSensitiveKey.test(key)?'[REDACTED]':sanitizeBrowserValue(item,seen,depth+1);
    }
    if(entries.length>30)result.omitted=entries.length-30;
    return result;
  } finally {seen.delete(value)}
}
function sanitizeBrowserMetadata(metadata) {
  const result=Object.create(null);
  if(metadata&&typeof metadata==='object')for(const key of browserMetadataAllowlist){
    if(Object.hasOwn(metadata,key))result[key]=sanitizeBrowserValue(metadata[key]);
  }
  return result;
}
function browserConsoleStyle(level,detail=false) {
  // The already-shared Pealayer catalog is the only color engine. DevTools
  // needs literal CSS colors, so resolve the current page palette at emission.
  const root=typeof document==='undefined'?null:document.documentElement;
  const colors=typeof PEALAYER_PALETTES==='undefined'?null:
    PEALAYER_PALETTES[root?.dataset?.palette==='native'?'native':'studio'][root?.dataset?.theme==='light'?'light':'dark'];
  const role={debug:'muted',info:'blue',success:'green',warn:'amber',error:'red'}[level];
  const foreground=detail?(colors?.text??'inherit'):(colors?.canvas??'inherit');
  const background=detail?(colors?.['surface-1']??'transparent'):(colors?.[role]??'transparent');
  return `color:${foreground};background:${background};font:${detail?'500':'600'} 11px/1.8 system-ui,Segoe UI,sans-serif;padding:2px 8px;border-radius:${detail?'0 999px 999px 0':'999px 0 0 999px'}`;
}
class StyledBrowserConsole {
  constructor(sink=globalThis.console,now=()=>Date.now()){this.sink=sink;this.now=now;this.recent=new Map();this.incidents=new Map();this.started=false}
  recover(source,metadata={}) {
    let count=0;
    for(const [key,value]of this.incidents)if(value.source===source){count+=value.count;this.incidents.delete(key)}
    if(!count)return false;
    return this.event('success','transport.ready','Transport recovered; repeated diagnostics were coalesced',{...metadata,count});
  }
  start(metadata){
    if(this.started)return false;
    this.started=true;
    try {
      this.sink?.groupCollapsed?.('%c 🚀 PAKARCONTROLLER %c Browser operational trace%c',browserConsoleStyle('info'),browserConsoleStyle('info',true),'color:inherit;background:transparent');
      this.event('info','app.start','Web controller initialized',metadata);
      this.sink?.debug?.('Privacy: allowlisted metadata only; credentials are redacted.');
      this.sink?.log?.('Filter by event, level or source. Heartbeats and unchanged polls stay quiet.');
      this.sink?.groupEnd?.();
      return true;
    } catch {return false}
  }
  event(level,event,message,metadata,options={}) {
    try {
      if(!['debug','info','success','warn','error'].includes(level)||!Object.hasOwn(browserEventPresentation,event))return false;
      const now=this.now();if(!Number.isFinite(now)||!Number.isFinite(new Date(now).getTime()))return false;
      const detail=redactBrowserText(message),safe=sanitizeBrowserMetadata(metadata);
      // A category alone must not swallow a different HTTP/close code or cause.
      const signature=JSON.stringify([level,event,detail,safe.error_code??null]);
      if(options.incidentKey){
        const source=redactBrowserText(options.incidentKey),key=JSON.stringify([source,signature]);
        const previous=this.incidents.get(key);
        if(previous){previous.count=Math.min(Number.MAX_SAFE_INTEGER,previous.count+1);return false}
        if(this.incidents.size>=128)this.incidents.delete(this.incidents.keys().next().value);
        this.incidents.set(key,{source,count:1});
      }
      if(options.dedupeKey){
        const key=JSON.stringify([redactBrowserText(options.dedupeKey),signature]),windowMs=Number.isFinite(options.dedupeWindowMs)?Math.max(0,Math.min(60000,options.dedupeWindowMs)):1000;
        const previous=this.recent.get(key);
        if(previous!==undefined&&now>=previous&&now-previous<windowMs)return false;
        if(this.recent.size>=256&&!this.recent.has(key))this.recent.delete(this.recent.keys().next().value);
        this.recent.set(key,now);
      }
      const method=level==='success'?'info':level,call=this.sink?.[method];
      if(typeof call!=='function')return false;
      call.call(this.sink,`%c ${browserEventPresentation[event]} ${level.toUpperCase()} · ${event} %c ${detail.replaceAll('%','%%')}%c`,
        browserConsoleStyle(level),browserConsoleStyle(level,true),'color:inherit;background:transparent',
        {...safe,timestamp:new Date(now).toISOString(),level,event,detail});
      return true;
    } catch {return false} // Diagnostics must never break an actual device action.
  }
}
const browserConsole=new StyledBrowserConsole();
function traceBrowserEvent(level,event,message,metadata,options){return browserConsole.event(level,event,message,metadata,options)}
browserConsole.start({source:'web'});
