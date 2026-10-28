import {spawnSync} from 'node:child_process';
import {mkdirSync,readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import assert from 'node:assert/strict';
const executable=resolve('build/native/scp_download_test.exe');
const output=resolve('build/native/scp-client-fixture');mkdirSync(output,{recursive:true});
for(const [name,size] of [['test.bin',513],['empty.bin',0]]){
  // -S uses the compiled production protocol fixture as the SSH byte pipe;
  // no host, network connection, key, password or board is contacted.
  const path=resolve(output,name);
  const result=spawnSync('scp',['-O','-S',executable,`fixture:/user/${name}`,path],{encoding:'utf8',timeout:10000});
  assert.equal(result.status,0,result.error?.message||result.stderr);
  assert.deepEqual(readFileSync(path),Buffer.from(Array.from({length:size},(_,i)=>i&255)));
}
console.log('Real OpenSSH SCP client accepted binary/empty production-protocol downloads over a stdio fixture; not an encrypted SSH/ESP session.');
