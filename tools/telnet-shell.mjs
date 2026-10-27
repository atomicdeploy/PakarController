#!/usr/bin/env node
// portable-shell src/ps_io_socket.c callback read/write client adapted to Node's
// cross-platform TCP/stdin streams. MIT donor notice: THIRD_PARTY_NOTICES.md.
// No raw GPIO, reconnect/replay, password arguments, or production auto-connect.
import net from 'node:net';
import {pathToFileURL} from 'node:url';

// Incremental RFC 854 framing, unlike the donor's raw recv() socket callback.
// Bounded state; no growing subnegotiation payload or duplicate negotiation loop.
export function telnetDecoder(reply,receive){
  let state=0,verb=0;const peer=new Map(),local=new Map();
  return bytes=>{const text=[];
    for(const c of bytes){
      if(state===0){if(c===255)state=1;else text.push(c);}
      else if(state===1){
        if(c===255){text.push(c);state=0;}
        else if(c===250)state=3;
        else if(c>=251&&c<=254){verb=c;state=2;}
        else state=0;
      }else if(state===2){
        const remote=verb===251||verb===252, table=remote?peer:local;
        const enabled=verb===251||verb===253;
        const supported=remote?(c===1||c===3):c===3;
        const response=remote?(enabled&&supported?253:254):(enabled&&supported?251:252);
        if(table.get(c)!==response){table.set(c,response);reply(Buffer.from([255,response,c]));}
        state=0;
      }else if(state===3){if(c===255)state=4;}
      else if(state===4)state=c===240?0:3;
    }
    if(text.length)receive(Buffer.from(text));
  };
}
export function escapeTelnet(bytes){
  const out=[];for(const c of bytes){out.push(c);if(c===255)out.push(c);}return Buffer.from(out);
}
const plain=text=>text.replace(/\x1b\[[0-?]*[ -/]*[@-~]/g,'');
export function connectShell({host,port=23,execute,deadlineMs=8000,input=process.stdin,output=process.stdout}){
  if(typeof host!=='string'||!host.trim()||host.length>253||/[\s/\\\x00-\x1f]/.test(host))throw Error('Supply one explicit hostname or IP');
  if(!Number.isInteger(port)||port<1||port>65535)throw Error('Invalid TCP port');
  if(!Number.isInteger(deadlineMs)||deadlineMs<100||deadlineMs>60000)throw Error('Deadline must be 100..60000 ms');
  if(execute!==undefined&&(typeof execute!=='string'||!execute.trim()||execute.length>512||/[^\x20-\x7e]/.test(execute)))throw Error('Execute requires one printable command, at most 512 characters');
  const interactive=execute===undefined;
  if(interactive&&(!input.isTTY||!output.isTTY||typeof input.setRawMode!=='function'))throw Error('Interactive shell requires a terminal; use --execute for one bounded command');
  return new Promise((resolve,reject)=>{
    const socket=net.createConnection({host,port});socket.setNoDelay(true);
    let phase='greeting',finished=false,tail='',timer,failedCommand=false;
    const wasRaw=!!input.isRaw,wasFlowing=input.readableFlowing===true;
    const arm=()=>{clearTimeout(timer);timer=setTimeout(()=>finish(Error(phase==='greeting'?'Shell greeting deadline expired':'Command outcome unknown: no final prompt; not replayed')),deadlineMs);};
    const send=bytes=>{
      if(finished)return;
      if(socket.writableLength+bytes.length>65536){finish(Error('Terminal input backpressure limit; not replayed'));return;}
      if(!socket.write(bytes)&&interactive)input.pause();
    };
    const onInput=bytes=>send(escapeTelnet(bytes));
    const onEnd=()=>finish();
    const onDrain=()=>{if(!finished)socket.resume();};
    const finish=error=>{
      if(finished)return;finished=true;clearTimeout(timer);
      input.removeListener('data',onInput);input.removeListener('end',onEnd);output.removeListener('drain',onDrain);
      output.removeListener('error',finish);input.removeListener('error',finish);
      socket.destroy();
      if(interactive){input.setRawMode(wasRaw);if(wasFlowing)input.resume();else input.pause();
        // Restore the caller's screen even if the board disconnected in edit.
        if(!output.destroyed)output.write('\x1b[0m\x1b[?1049l');}
      if(error)reject(error);else resolve({completed:!interactive,transport:'tcp',replayed:false});
    };
    const decode=telnetDecoder(send,bytes=>{
      if(!output.write(bytes))socket.pause();
      if(interactive)return;
      tail=(tail+bytes.toString('utf8')).slice(-8192);
      const text=plain(tail);
      if(phase==='greeting'&&/Password:\s*$/.test(text)){finish(Error('Authentication required: open an interactive shell; command was not sent'));return;}
      if(phase==='command'&&/(?:^|\r?\n)ERR\s/.test(text))failedCommand=true;
      if(!/(?:^|\r?\n)[a-zA-Z0-9-]{1,63}> $/.test(text))return;
      if(phase==='greeting'){phase='command';tail='';send(Buffer.from(execute+'\r'));arm();}
      else finish(failedCommand?Error('Board rejected the command; see terminal output'):undefined);
    });
    socket.on('data',decode);
    socket.on('error',finish);
    socket.on('end',()=>finish(interactive?undefined:Error('Disconnected before final prompt; command not replayed')));
    socket.on('drain',()=>{if(interactive&&!finished)input.resume();});
    output.on('drain',onDrain);output.on('error',finish);input.on('error',finish);
    arm();
    if(interactive){
      socket.once('connect',()=>{clearTimeout(timer);phase='interactive';});
      try{input.setRawMode(true);input.on('data',onInput);input.on('end',onEnd);input.resume();}
      catch(error){finish(error);}
    }
  });
}
export async function main(args=process.argv.slice(2)){
  if(args.includes('--help')){console.log('PakarController shell\nnode tools/telnet-shell.mjs --host ledcontroller.local [--port 23] [--execute "status"] [--deadline-ms 8000]\nInteractive input supports password prompts, editing, history and edit/TUI. One-command mode never retries.');return;}
  const options={};for(let i=0;i<args.length;i+=2){const key={'--host':'host','--port':'port','--execute':'execute','--deadline-ms':'deadlineMs'}[args[i]];
    if(!key||args[i+1]===undefined||options[key]!==undefined)throw Error('Use --help; unknown, duplicate or missing option');
    options[key]=['port','deadlineMs'].includes(key)?Number(args[i+1]):args[i+1];}
  await connectShell(options);
}
if(process.argv[1]&&import.meta.url===pathToFileURL(process.argv[1]).href)main().catch(error=>{console.error(error.message);process.exitCode=1;});
