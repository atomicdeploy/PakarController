import assert from 'node:assert/strict';
import net from 'node:net';
import {PassThrough} from 'node:stream';
import {connectShell,telnetDecoder,escapeTelnet} from '../tools/telnet-shell.mjs';
let replies=[],received=[];
const decode=telnetDecoder(b=>replies.push([...b]),b=>received.push(...b));
for(const byte of [255,251,1,255,251,1,255,253,3,255,253,42,255,250,24,65,255,255,66,255,240,120,255,255])decode([byte]);
assert.deepEqual(replies,[[255,253,1],[255,251,3],[255,252,42]]);
assert.deepEqual(received,[120,255]);assert.deepEqual([...escapeTelnet([120,255])],[120,255,255]);
assert.throws(()=>connectShell({host:'board',execute:'reset\nreset'}),/one printable/);
assert.throws(()=>connectShell({host:'board',port:0,execute:'status'}),/port/);
async function session(kind){
  let commands=0;
  const server=net.createServer(socket=>{
    socket.on('error',()=>{});
    socket.write(Buffer.from([255,251,1]));
    // Deliberately fragmented negotiation/prompt, as on real TCP streams.
    socket.write('PakarController @ fixture\r\n\x1b[36m');
    setImmediate(()=>socket.write(kind==='auth'?'Password: ':'fixture\x1b[0m> '));
    let pending='';const input=telnetDecoder(()=>{},bytes=>{
      pending+=bytes.toString();
      if(pending.includes('\r')){
        ++commands;assert.equal(pending,'status\r');pending='';
        if(kind==='timeout')return;
        socket.write(kind==='reject'?'ERR Busy\r\n':'Status fixture\r\n');
        setImmediate(()=>socket.write('fixture> '));
      }
    });socket.on('data',input);
  });
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const output=new PassThrough();let text='';output.on('data',b=>text+=b);
  try{
    const call=connectShell({host:'127.0.0.1',port:server.address().port,execute:'status',deadlineMs:1000,output,input:new PassThrough()});
    if(kind==='auth')await assert.rejects(call,/not sent/);
    else if(kind==='reject')await assert.rejects(call,/rejected/);
    else if(kind==='timeout')await assert.rejects(call,/not replayed/);
    else assert.deepEqual(await call,{completed:true,transport:'tcp',replayed:false});
    assert.equal(commands,kind==='auth'?0:1);
    assert.equal(output.listenerCount('drain'),0);
    if(kind==='ok')assert.match(text,/Status fixture/);
  }finally{await new Promise(resolve=>server.close(resolve));}
}
for(const kind of ['ok','auth','reject','timeout'])await session(kind);
// Exercise the actual interactive stream owner: password bytes go to the peer,
// not stdout; disconnect restores raw mode and removes input listeners.
const ttyIn=new PassThrough(),ttyOut=new PassThrough(),rawModes=[];
ttyIn.isTTY=ttyOut.isTTY=true;ttyIn.isRaw=false;
ttyIn.setRawMode=value=>{rawModes.push(value);ttyIn.isRaw=value;};
let transcript='',answered=false,secret='';
const ttyServer=net.createServer(socket=>{
  socket.on('error',()=>{});socket.write('Password: ');
  socket.on('data',bytes=>{secret+=bytes.toString();if(secret.endsWith('\r'))socket.end('Authenticated\r\n');});
});
await new Promise(resolve=>ttyServer.listen(0,'127.0.0.1',resolve));
ttyOut.on('data',bytes=>{transcript+=bytes.toString();if(!answered&&transcript.includes('Password: ')){answered=true;ttyIn.write('test-only-secret\r');}});
try{
  assert.deepEqual(await connectShell({host:'127.0.0.1',port:ttyServer.address().port,input:ttyIn,output:ttyOut}),{completed:false,transport:'tcp',replayed:false});
  assert.equal(secret,'test-only-secret\r');assert.ok(!transcript.includes('test-only-secret'));
  assert.deepEqual(rawModes,[true,false]);assert.equal(ttyIn.listenerCount('data'),0);
  assert.ok(ttyIn.isPaused()); // Do not keep a completed CLI alive on stdin.
  assert.ok(transcript.endsWith('\x1b[0m\x1b[?1049l'));
}finally{await new Promise(resolve=>ttyServer.close(resolve));}
console.log('TCP/stdin shell: fragmented Telnet, exactly-once command, auth refusal, rejection and deadline loopback passed.');
