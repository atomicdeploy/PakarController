import {createSocket} from 'node:dgram';
import {spawn} from 'node:child_process';
import assert from 'node:assert/strict';
const socket=createSocket('udp4');
await new Promise((resolve,reject)=>{socket.once('error',reject);socket.bind(9876,'127.0.0.1',resolve);});
let requests=0;
socket.on('message',(bytes,peer)=>{
  ++requests;const request=bytes.toString();assert.match(request,/^[a-f0-9]{16} status$/);
  socket.send('0000000000000000 OK wrong',peer.port,peer.address);
  socket.send(request.slice(0,16)+' OK hostname=fixture boot_id=test',peer.port,peer.address);
});
try{
  const result=await new Promise((resolve,reject)=>{
    const child=spawn(process.execPath,['tools/udp-device-query.mjs','127.0.0.1','status'],{stdio:['ignore','pipe','pipe']});
    let out='',err='';child.stdout.on('data',b=>out+=b);child.stderr.on('data',b=>err+=b);
    const timer=setTimeout(()=>{child.kill();reject(Error('UDP client fixture exceeded deadline'));},6000);
    child.on('error',error=>{clearTimeout(timer);reject(error);});
    child.on('exit',code=>{clearTimeout(timer);resolve({code,out,err});});
  });
  assert.equal(result.code,0,result.err);assert.equal(requests,1);
  assert.equal(result.out.trim(),'OK hostname=fixture boot_id=test');
  console.log('UDP client loopback rejects uncorrelated reply and sends one read-only query.');
}finally{socket.close();}
