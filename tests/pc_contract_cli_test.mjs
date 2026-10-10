import assert from 'node:assert/strict';
import http from 'node:http';
import {execFile} from 'node:child_process';
import {promisify} from 'node:util';
import {fileURLToPath} from 'node:url';
const run=promisify(execFile);
const exe=process.argv[2]||fileURLToPath(new URL('../desktop/target/release/pakarcontroller.exe',import.meta.url));
let posts=0,consumed=0,id=0,opcode=0,mode='normal';
let settings=Buffer.from([80,2,5,0,0,0,2,0,63,0x10,0x32,0x54,0,...Array(8).fill(0)]);
const server=http.createServer(async(req,res)=>{
  assert.equal(req.headers.authorization,'Basic '+Buffer.from('fixture:private-fixture-password').toString('base64'));
  let raw='';for await(const chunk of req)raw+=chunk;
  let value;
  if(req.method==='POST'){
    assert.equal(req.url,'/api/v1/avr/contracts');posts++;
    const form=new URLSearchParams(raw);opcode=Number(form.get('opcode'));id++;
    const payload=Buffer.from(form.get('payload_hex'),'hex');
    if(opcode===5){assert.equal(payload.length,21);settings=payload;}
    value={request_id:id,boot_id:'fixture-boot'};
  }else if(req.method==='DELETE'){
    const url=new URL(req.url,'http://fixture');
    assert.equal(url.pathname,'/api/v1/avr/contracts/result');
    assert.equal(url.searchParams.get('id'),String(id));
    assert.equal(url.searchParams.get('boot_id'),'fixture-boot');
    assert.equal(url.searchParams.get('opcode'),String(opcode));
    assert(!['reboot','timeout'].includes(mode));consumed++;value={consumed:true};
  }else{
    assert.equal(req.url,'/api/v1/avr/contracts/result?id='+id);
    value={request_id:id,boot_id:mode==='reboot'?'different-boot':'fixture-boot',request_opcode:opcode,
      state:mode==='timeout'?'outcome_unknown':'responded',
      response_opcode:mode==='reject'?0x82:opcode===4?0x91:0x80,
      payload_hex:mode==='reject'?Buffer.from([opcode,4]).toString('hex'):
        opcode===4?Buffer.concat([settings,Buffer.from([1])]).toString('hex'):
        Buffer.from([opcode,0]).toString('hex')};
  }
  res.writeHead(req.method==='POST'?202:200,{'Content-Type':'application/json'});res.end(JSON.stringify(value));
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const env={...process.env,PAKAR_ENDPOINT:'http://127.0.0.1:'+server.address().port,PAKAR_USER:'fixture',PAKAR_PASSWORD:'private-fixture-password'};
try{
  const read=await run(exe,['--command','avr','settings'],{env});assert.equal(JSON.parse(read.stdout).display_brightness,5);
  await run(exe,['--command','avr','set','display-brightness','0'],{env});assert.equal(settings[2],0);
  for(let i=0;i<12;i++) await run(exe,['--command','avr','settings'],{env});
  assert.equal(consumed,posts,'Every validated response consumed; settings do not exhaust slots');
  const before=posts;await assert.rejects(run(exe,['--command','avr','command','0x13'],{env}));assert.equal(posts,before);
  await run(exe,['--command','avr','command','DISPLAY_TEXT','00'.repeat(48)],{env});
  for(const failure of ['reject','timeout','reboot']){
    mode=failure;const before=posts,consumedBefore=consumed;
    await assert.rejects(run(exe,['--command','avr','command','BUZZER','b8011900'],{env}),e=>e.code===2);
    assert.equal(posts,before+1,'Never replay a rejected or uncertain mutation');
    assert.equal(consumed,consumedBefore+(failure==='reject'?1:0),'Never consume unknown/reboot-mismatched receipt');
  }
  console.log('Built CLI: authenticated contract queries, settings write/readback, 48-byte payload, reserved rejection, AVR error, timeout and reboot identity; no retries passed (HTTP fixture).');
}finally{await new Promise(resolve=>server.close(resolve));}
