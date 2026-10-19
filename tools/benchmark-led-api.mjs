// PCController netpolicy/http_url.go: explicit HTTP authority, no credentials,
// fragments or redirects. Unlike dependency network.mjs, NEVER retry mutations.
// Peer fencing/deadlines follow the actual shared assets/web/peer-http.js.
import {isIP} from 'node:net';
import {pathToFileURL} from 'node:url';
import {toolHeaders,assertToolPeerCompatible} from './peer-identity.mjs';

export async function runLedApiBenchmark(options={}) {
  const {origin,expectedBackend,durationSeconds=8,readers=2,hz=5,run=false,environment=process.env}=options;
  if(!run)throw Error('Explicit --run is required; this benchmark changes cells1 and80.');
  const url=new URL(origin);
  if(typeof origin!=='string'||/[\s\\#]/.test(origin)||!['http:','https:'].includes(url.protocol)||url.username||url.password||url.pathname!=='/'||url.search||url.hash||url.port==='0'||isIP(url.hostname)||!url.hostname.split('.').every(v=>/^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$/i.test(v)))throw Error('Use an explicit hostname-only HTTP(S) origin without credentials, path, query or fragment.');
  if(!['gpio-static','rmt-dma'].includes(expectedBackend)||!(durationSeconds>0&&durationSeconds<=30)||!Number.isInteger(readers)||readers<1||readers>4||!(hz>=1&&hz<=10))throw Error('Require --backend gpio-static|rmt-dma; seconds(0,30], readers1..4, hz1..10.');
  const user=environment.PAKAR_BENCH_USERNAME,password=environment.PAKAR_BENCH_PASSWORD;
  if((user===undefined)!==(password===undefined))throw Error('Set both PAKAR_BENCH_USERNAME and PAKAR_BENCH_PASSWORD, or neither.');
  const authorization=user===undefined?null:'Basic '+Buffer.from(user+':'+password).toString('base64');
  const stop=new AbortController(),started=performance.now(),latencies=[],phases=[],errors=[];
  let owner=null,initial=null,latest=null,phase='preflight',dispatched=0,completed=0,minHeap=null,failure=null,restored=false;
  const fail=code=>{if(!failure){failure=code;errors.push({phase,code});stop.abort()}throw Error(failure)};
  const deadline=setTimeout(()=>{if(!failure){failure='overall_deadline';errors.push({phase,code:failure});stop.abort()}},85000);
  const wait=ms=>new Promise(resolve=>{if(stop.signal.aborted)return resolve();const done=()=>{clearTimeout(timer);stop.signal.removeEventListener('abort',done);resolve()},timer=setTimeout(done,Math.max(0,ms));stop.signal.addEventListener('abort',done,{once:true})});
  async function request(path,body) {
    if(failure)throw Error(failure);const start=performance.now(),controller=new AbortController(),abort=()=>controller.abort();
    stop.signal.addEventListener('abort',abort,{once:true});const timer=setTimeout(abort,3000);
    try {
      const headers=toolHeaders({Accept:'application/json'});if(authorization)headers.Authorization=authorization;
      if(owner){headers['X-Pakar-Board']=owner.board;headers['X-Pakar-Boot']=owner.boot}
      if(body)dispatched++;
      const response=await fetch(new URL(path,url),{method:body?'POST':'GET',headers,body:body?new URLSearchParams(body):undefined,redirect:'error',cache:'no-store',signal:controller.signal});
      const chunks=[],reader=response.body.getReader();let bytes=0;
      for(;;){const part=await reader.read();if(part.done)break;bytes+=part.value.length;if(bytes>65536){controller.abort();return fail('response_too_large')}chunks.push(part.value)}
      const text=Buffer.concat(chunks,bytes).toString('utf8');
      if(!response.ok)return fail('http_'+response.status);return JSON.parse(text);
    }catch{if(!failure)fail('transport_or_invalid_json');throw Error(failure)}
    finally{latencies.push({phase,ms:performance.now()-start});clearTimeout(timer);stop.signal.removeEventListener('abort',abort)}
  }
  function validate(s,mode) {
    try{assertToolPeerCompatible(s)}catch{return fail('unsupported_peer_version')}
    const c=s?.capabilities,d=s?.led_output?.driver;
    if(s?.http_peer_fencing!==true||!/^[a-z0-9_-]{1,64}$/.test(s.board_id??'')||!/^[a-f0-9]{32}$/.test(s.boot_id??'')||/^0{32}$/.test(s.boot_id))return fail('missing_peer_identity');
    if(owner&&(s.board_id!==owner.board||s.boot_id!==owner.boot))return fail('peer_changed');
    if(c?.board_kind!=='table'||!Array.isArray(c.acknowledged_features)||!['table','leds','cell-batch'].every(v=>c.acknowledged_features.includes(v))||c.table?.columns!==10||c.table?.rows!==8||c.table?.pixels_per_cell!==6||s.pixels!==480||!Array.isArray(s.cells)||s.cells.length!==80||!s.cells.every(v=>['white','red','off'].includes(v)))return fail('unsupported_table_contract');
    if(d?.backend!==expectedBackend||d.ready!==true||d.dma!==(expectedBackend==='rmt-dma')||!['submitted','transmitted','revision','failed','error_code'].every(k=>Number.isInteger(d[k])&&d[k]>=0&&d[k]<=0xffffffff)||d.error_code!==0||d.failed!==0||!['static','effects'].includes(s.led_output.mode)||s.led_output.enabled!==true||!['none','rainbow','fire'].includes(s.effect))return fail('driver_not_ready');
    if(mode&&s.led_output.mode!==mode)return fail('mode_readback_mismatch');
    if(initial&&s.cells.some((v,i)=>i!==0&&i!==79&&v!==initial.cells[i]))return fail('concurrent_cell_change');
    const heap=s.free_heap_bytes??s.free_heap;if(Number.isFinite(heap)&&heap>=0)minHeap=minHeap===null?heap:Math.min(minHeap,heap);
    latest=s;return s;
  }
  const status=async mode=>validate(await request('/api/status'),mode);
  async function mutation(path,body,accepted,expectedMode) {
    await status(expectedMode);const ack=await request(path,body);if(!accepted(ack))fail('ack_mismatch');return ack;
  }
  async function cells(ids,state,effect) {
    const expectedMode=['static','effects'].includes(phase)?phase:undefined;
    await mutation('/api/cells',{ids:ids.join(','),state,effect},a=>a?.accepted===true&&a.count===ids.length,expectedMode);
    const after=await status(expectedMode);if(!ids.every(id=>after.cells[id-1]===state))fail('cell_readback_mismatch');return after;
  }
  async function mode(value) {
    await mutation('/api/led/output',{mode:value},a=>a?.mode===value&&a.enabled===true);return status(value);
  }
  const counters=s=>Object.fromEntries(['submitted','transmitted','revision','failed','error_code'].map(k=>[k,s.led_output.driver[k]]));
  const summary=samples=>{const sorted=samples.map(s=>s.ms).sort((a,b)=>a-b),p=v=>sorted.length?+sorted[Math.ceil(sorted.length*v)-1].toFixed(2):null;return {requests:sorted.length,p50_ms:p(.5),p95_ms:p(.95),p99_ms:p(.99)}};
  try {
    initial=structuredClone(await status());owner={board:initial.board_id,boot:initial.boot_id};
    for(const value of ['static','effects']) {
      phase=value;const before=await mode(value),baseline=counters(before),baseWrites=completed,metricsAt=latencies.length,phaseStart=performance.now(),end=phaseStart+durationSeconds*1000;
      const writer=async()=>{let index=0;while(!failure&&performance.now()<end){const tick=performance.now();await cells([1,80],index++%2?'white':'red',value==='static'?'immediate':'fade');completed++;if(tick+1000/hz>=end)break;await wait(1000/hz-(performance.now()-tick))}};
      const reader=async()=>{while(!failure&&performance.now()<end){await status(value);await wait(Math.min(200,end-performance.now()))}};
      const results=await Promise.allSettled([writer(),...Array.from({length:readers},reader)]);
      if(results.some(r=>r.status==='rejected'))throw Error(failure);
      const after=await status(value),finish=counters(after);
      if(finish.transmitted===baseline.transmitted||finish.revision===baseline.revision||finish.submitted===baseline.submitted||after.led_output.driver.active!==true)fail('no_led_progress');
      const elapsed=performance.now()-phaseStart,samples=latencies.slice(metricsAt);
      phases.push({mode:value,elapsed_ms:Math.round(elapsed),completed_writes:completed-baseWrites,writes_per_second:+((completed-baseWrites)*1000/elapsed).toFixed(2),requests_per_second:+(samples.length*1000/elapsed).toFixed(2),...summary(samples),driver_delta:Object.fromEntries(Object.keys(baseline).map(k=>[k,k==='error_code'?finish[k]-baseline[k]:(finish[k]-baseline[k])>>>0]))});
    }
    phase='restore';
    for(const state of ['white','red','off']){const ids=initial.cells.flatMap((v,i)=>v===state?[i+1]:[]);if(ids.length)await cells(ids,state,'immediate')}
    await mutation('/api/effect',{value:initial.effect},a=>a?.updated===true);await mode(initial.led_output.mode);
    const after=await status(initial.led_output.mode);if(after.effect!==initial.effect||!after.cells.every((v,i)=>v===initial.cells[i]))fail('restoration_readback_mismatch');restored=true;
  }catch{if(!failure){failure='benchmark_failed';errors.push({phase,code:failure});stop.abort()}}
  finally{clearTimeout(deadline)}
  return {ok:!failure,origin:url.origin,backend:expectedBackend,seconds_per_mode:durationSeconds,readers,max_read_hz_per_reader:5,write_hz:hz,elapsed_ms:Math.round(performance.now()-started),completed_writes:completed,mutations_dispatched:dispatched,errors,unknown:!!failure&&dispatched>0,restored,restoration:restored?'cell_targets_pattern_mode_read_back_not_raw_pixels':failure?'not_attempted_or_incomplete_after_error':'not_attempted',min_free_heap_bytes:minHeap,...summary(latencies),phases,not_optical_proof:true,meaning:'HTTP/readback and driver counters only. DMA repeats are not unique rendered FPS; no optical, timing or RF acceptance.'};
}

if(process.argv[1]&&import.meta.url===pathToFileURL(process.argv[1]).href) {
  const args=process.argv.slice(2),o={};
  try {
    for(let i=0;i<args.length;i++){const k=args[i];if(k==='--run'){o.run=true;continue}const name={'--origin':'origin','--backend':'expectedBackend','--seconds':'durationSeconds','--readers':'readers','--hz':'hz'}[k];if(!name||i+1===args.length)throw Error('Usage: node tools/benchmark-led-api.mjs --run --origin http://ledboard.local --backend rmt-dma [--seconds 8 --readers 2 --hz 5]. Credentials: PAKAR_BENCH_USERNAME/PASSWORD environment only.');o[name]=['origin','expectedBackend'].includes(name)?args[++i]:Number(args[++i])}
    const report=await runLedApiBenchmark(o);process.stdout.write(JSON.stringify(report,null,2)+'\n');if(!report.ok)process.exitCode=1;
  }catch(error){process.stderr.write(error.message+'\n');process.exitCode=1}
}
