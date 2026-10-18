// Adapted actual PU850 Tool/upload.js logger/sanitizer/exit codes and
// Tool/build-all.ps1 staged elapsed summary. No extra npm/runtime dependency.
import {spawn} from 'node:child_process';
import {performance} from 'node:perf_hooks';
export const EXIT=Object.freeze({OK:0,USAGE:2,LOCAL_INPUT:3,NETWORK:4,PROTOCOL:5,INTEGRITY:6,REJECTED:7,POST_VERIFY:8,LOCAL_IO:9,INTERRUPTED:130});
export class ToolError extends Error {constructor(message,exitCode=2){super(message);this.exitCode=exitCode;}}
export function sanitizeText(value,maxLength=8192) {
  // OSC must be removed BEFORE the broad ESC family: the donor's alternation
  // consumed only the prefix of ESC]0;title, leaving terminal-title payload.
  return String(value??'').replace(/\x1B\][^\x07\x1B]*(?:\x07|\x1B\\|$)/g,'')
    .replace(/\x1B(?:\[[0-?]*[ -/]*[@-~]|[@-_])/g,'')
    .replace(/[\x00-\x08\x0B\x0C\x0E-\x1F\x7F-\x9F]/g,'').slice(0,maxLength);
}
export function createLogger({quiet=false,json=false,env=process.env,out=process.stdout,err=process.stderr}={}) {
  const color=!!out.isTTY&&!json&&env.NO_COLOR===undefined&&env.TERM!=='dumb';
  const emit=(kind,message,details={})=>{
    if(quiet&&!['error','warn','result'].includes(kind))return;
    const stream=kind==='error'||kind==='warn'?err:out,text=sanitizeText(message);
    if(json){stream.write(JSON.stringify({tool:'PakarController',kind,message:text,...details})+'\n');return;}
    const colors={info:96,step:96,detail:90,success:92,warn:93,error:91,result:92};
    const label=kind==='detail'?'  ':`[${kind.toUpperCase()}] `;
    stream.write((color&&stream.isTTY?`\x1b[${colors[kind]}m${label}\x1b[0m`:label)+text+'\n');
  };
  return Object.fromEntries(['info','step','detail','success','warn','error','result'].map(kind=>[kind,(text,details)=>emit(kind,text,details)]));
}
// Keep compiler diagnostics/carets intact; never eval or hide failed child exits.
export async function runChild(file,args,{cwd,env=process.env,logger=createLogger(),json=false,timeoutMs=0}={}) {
  const started=performance.now();
  return await new Promise((resolve,reject)=>{
    const child=spawn(file,args,{cwd,env,shell:false,windowsHide:true,stdio:['inherit',json?'pipe':'inherit',json?'pipe':'inherit']});
    let interrupted=false;
    const stop=signal=>{interrupted=true;child.kill(signal);};
    const sigint=()=>stop('SIGINT'),sigterm=()=>stop('SIGTERM');
    process.once('SIGINT',sigint);process.once('SIGTERM',sigterm);
    const timer=timeoutMs?setTimeout(()=>stop('SIGTERM'),timeoutMs):null;
    if(json)for(const [stream,name] of [[child.stdout,'stdout'],[child.stderr,'stderr']]){
      stream.setEncoding('utf8');stream.on('data',chunk=>logger.detail(chunk,{stream:name}));
    }
    const cleanup=()=>{if(timer)clearTimeout(timer);process.removeListener('SIGINT',sigint);process.removeListener('SIGTERM',sigterm);};
    child.once('error',error=>{cleanup();reject(new ToolError(error.code==='ENOENT'?`Required command '${file}' absent. Run pakar doctor; reuse the existing SDK/toolchain.`:sanitizeText(error.message),3));});
    child.once('close',(code,signal)=>{cleanup();resolve({exit_code:interrupted?130:code??1,signal,elapsed_ms:Math.round(performance.now()-started)});});
  });
}
