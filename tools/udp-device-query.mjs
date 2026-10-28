// Optional public diagnostic query only; never retries a mutation or opens a shell.
import {createSocket} from 'node:dgram';
import {lookup} from 'node:dns/promises';
import {randomBytes} from 'node:crypto';
const [host,command='status',...extra]=process.argv.slice(2);
if(!host||extra.length||!['hello','status'].includes(command)){
  console.error('Usage: node tools/udp-device-query.mjs HOSTNAME [hello|status]');process.exit(2);
}
const {address}=await lookup(host,{family:4});
const nonce=randomBytes(8).toString('hex'),socket=createSocket('udp4');
try{
  const reply=await new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>reject(Error('No correlated UDP reply within 3 seconds; endpoint may be disabled')),3000);
    const finish=(error,value)=>{clearTimeout(timer);error?reject(error):resolve(value);};
    socket.on('error',error=>finish(error));
    socket.on('message',(bytes,peer)=>{
      if(peer.address!==address||peer.port!==9876||bytes.length>383)return;
      const text=bytes.toString('ascii');if(!text.startsWith(nonce+' '))return;
      finish(null,text.slice(17));
    });
    socket.send(`${nonce} ${command}`,9876,address,error=>{if(error)finish(error);});
  });
  console.log(reply);if(!reply.startsWith('OK '))process.exitCode=1;
}catch(error){console.error(error.message);process.exitCode=1;}finally{socket.close();}
