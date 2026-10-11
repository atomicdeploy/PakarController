import assert from 'node:assert/strict';
import http from 'node:http';
import {execFile} from 'node:child_process';
import {promisify} from 'node:util';
import {fileURLToPath} from 'node:url';
const run=promisify(execFile);
const exe=process.argv[2]||fileURLToPath(new URL('../desktop/target/release/pakarcontroller.exe',import.meta.url));
// Piped child is deliberately not a terminal. Reject before raw mode or HTTP.
let requests=0;
const server=http.createServer((_req,res)=>{requests++;res.writeHead(500);res.end();});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
try {
  await assert.rejects(run(exe,['--tui'],{
    env:{...process.env,PAKAR_ENDPOINT:'http://127.0.0.1:'+server.address().port},
    timeout:6000
  }),error=>{
    assert.equal(error.code,2);
    assert.equal(error.stdout,'');
    assert.match(error.stderr,/interactive terminal on stdin and stdout/);
    assert.doesNotMatch(error.stderr,/\u001b|\u009b/);
    return true;
  });
  assert.equal(requests,0,'TUI must reject redirected surfaces before contacting a device');
  const taskPort=server.address().port;
  await new Promise(resolve=>server.close(resolve));
  await assert.rejects(run(exe,['--status'],{
    env:{...process.env,PAKAR_ENDPOINT:'http://127.0.0.1:'+taskPort},timeout:6000
  }),error=>{
    assert.equal(error.code,2);
    assert.match(error.stderr,/client error|tcp connect|actively refused|connection refused|os error/i,
      'Shared status path must retain the transport cause, not only reqwest outer text');
    return true;
  });
  console.log('Built TUI: redirected-terminal rejection, zero HTTP requests and no control sequences passed. This does not certify interactive terminal or physical board acceptance.');
} finally {if(server.listening)await new Promise(resolve=>server.close(resolve));}
