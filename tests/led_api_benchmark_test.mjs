// Real local HTTP, not mocked fetch. No board, Wi-Fi, LED or optical evidence.
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {runLedApiBenchmark} from '../tools/benchmark-led-api.mjs';

async function fixture(backend,scenario='success') {
  const original={version:'0.2.8',board_id:'fixture_board',boot_id:'a'.repeat(32),http_peer_fencing:true,pixels:480,free_heap_bytes:120000,effect:'rainbow',
    capabilities:{board_kind:'table',acknowledged_features:['table','leds','cell-batch'],table:{columns:10,rows:8,pixels_per_cell:6}},
    cells:Array.from({length:80},(_,i)=>i%3===0?'red':i%3===1?'white':'off'),
    led_output:{enabled:true,mode:'effects',driver:{backend,dma:backend==='rmt-dma',ready:true,active:true,submitted:10,transmitted:100,revision:10,failed:0,error_code:0}}};
  const state=structuredClone(original),posts=[];let cellWrites=0;
  const server=createServer(async(req,res)=>{
    assert.equal(req.headers.authorization,'Basic '+Buffer.from('owner:fixture_secret').toString('base64'));
    const url=new URL(req.url,'http://localhost');let reply;
    if(req.method==='GET'){
      assert.equal(url.pathname,'/api/status');state.free_heap_bytes--;
      if(scenario!=='stalled')state.led_output.driver.transmitted+=backend==='rmt-dma'?4:1;reply=state;
    }else{
      assert.equal(req.headers['x-pakar-board'],original.board_id);assert.equal(req.headers['x-pakar-boot'],original.boot_id);
      let text='';for await(const chunk of req)text+=chunk;const form=new URLSearchParams(text);posts.push({path:url.pathname,form:Object.fromEntries(form)});
      if(url.pathname==='/api/cells'){
        const ids=form.get('ids').split(',').map(Number);assert(ids.every(v=>v>=1&&v<=80));assert(['immediate','fade'].includes(form.get('effect')));
        if(++cellWrites<=2)assert.deepEqual(ids,[1,80],'Stress writer touches only the two authorized cells');
        for(const id of ids)state.cells[id-1]=form.get('state');state.effect='none';state.led_output.driver.submitted++;
        if(scenario!=='frozen-frame')state.led_output.driver.revision++;
        reply={accepted:true,count:scenario==='bad-count'?ids.length-1:ids.length};
        if(scenario==='boot-change')state.boot_id='b'.repeat(32);
        if(scenario==='read-fail')state.badRead=true;
      }else if(url.pathname==='/api/led/output'){state.led_output.mode=form.get('mode');reply=state.led_output}
      else if(url.pathname==='/api/effect'){state.effect=form.get('value');reply={updated:true}}
      else assert.fail('Unexpected mutation endpoint '+url.pathname);
    }
    res.setHeader('Content-Type','application/json');if(req.method==='GET'&&state.badRead){res.statusCode=503;res.end('{"error":"fixture"}')}else res.end(JSON.stringify(reply));
  });
  await new Promise(resolve=>server.listen(0,'localhost',resolve));
  try {
    const options={origin:'http://localhost:'+server.address().port,expectedBackend:scenario==='backend-mismatch'?(backend==='rmt-dma'?'gpio-static':'rmt-dma'):backend,durationSeconds:.06,readers:2,hz:5,run:true,environment:{PAKAR_BENCH_USERNAME:'owner',PAKAR_BENCH_PASSWORD:'fixture_secret'}};
    const report=await runLedApiBenchmark(options);assert(report.not_optical_proof);assert(!JSON.stringify(report).includes('fixture_secret'));
    if(scenario==='success'){
      assert(report.ok&&report.restored&&!report.unknown);assert.deepEqual(state.cells,original.cells);assert.equal(state.effect,original.effect);assert.equal(state.led_output.mode,original.led_output.mode);
      assert.deepEqual(report.phases.map(v=>v.mode),['static','effects']);assert.equal(report.completed_writes,2);
      assert(report.p99_ms>=report.p50_ms&&report.min_free_heap_bytes<120000);assert(report.phases.every(v=>v.completed_writes===1&&v.driver_delta.transmitted>0));
      assert.deepEqual(posts.filter(v=>v.path==='/api/cells').slice(2).map(v=>v.form.state),['white','red','off']);
    }else if(scenario==='backend-mismatch'){
      assert(!report.ok&&!report.unknown&&!report.restored);assert.equal(posts.length,0,'Wrong physical driver is rejected before any mutation');assert.equal(report.errors[0].code,'driver_not_ready');
    }else if(scenario==='stalled'||scenario==='frozen-frame'){
      assert(!report.ok&&report.unknown&&!report.restored);assert.equal(report.errors[0].code,'no_led_progress');
      assert.equal(cellWrites,1);assert.equal(posts.length,2,'Stalled or repeating old output is not successful and does not trigger replay');
    }else{
      assert(!report.ok&&report.unknown&&!report.restored);assert.equal(report.completed_writes,0);assert.equal(cellWrites,1,'No replay or automatic restoration after uncertainty');
      assert.equal(posts.length,2,'Only initial mode and the one uncertain batch were dispatched');
      assert.equal(report.errors[0].code,scenario==='bad-count'?'ack_mismatch':scenario==='boot-change'?'peer_changed':'http_503');
    }
    return report;
  }finally{server.closeAllConnections();await new Promise(resolve=>server.close(resolve))}
}
await assert.rejects(runLedApiBenchmark({origin:'http://localhost',expectedBackend:'rmt-dma'}),/--run/);
for(const origin of ['http://user:secret@localhost','http://127.0.0.1','http://localhost/#secret','http://localhost/path','http://localhost\\','http://localhost:0'])await assert.rejects(runLedApiBenchmark({origin,run:true,expectedBackend:'rmt-dma'}),/hostname-only/);
for(const extra of [{durationSeconds:31},{readers:5},{hz:11},{expectedBackend:'unknown'}])await assert.rejects(runLedApiBenchmark({origin:'http://localhost',run:true,expectedBackend:'rmt-dma',...extra}),/Require --backend/);
for(const backend of ['gpio-static','rmt-dma'])await fixture(backend);
for(const scenario of ['bad-count','boot-change','read-fail','backend-mismatch','stalled','frozen-frame'])await fixture('rmt-dma',scenario);
console.log('Real loopback API benchmark: both backends/modes, bounded concurrent reads/single writer, full readback, exact-count ACK, peer-change abort/no replay, restoration and secret-safe report PASS. Not physical LED/RF proof.');
