import {spawnSync} from 'node:child_process';
import {mkdirSync,writeFileSync,readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import assert from 'node:assert/strict';
const executable=resolve('build/native/file_transfer_test.exe');
const root=resolve('build/native/transfer-client-fixture');mkdirSync(root,{recursive:true});
for(const [name,size] of [['test.bin',513],['empty.bin',0]]){
  const file=resolve(root,name);writeFileSync(file,Buffer.from(Array.from({length:size},(_,i)=>i&255)));
  const result=spawnSync('scp',['-O','-S',executable,file,`fixture:/user/${name}`],{encoding:'utf8',timeout:15000});
  assert.equal(result.status,0,result.error?.message||result.stderr);
}
const bytes=Buffer.from(Array.from({length:65539},(_,i)=>i&255));
const source=resolve(root,'sftp-source.bin'),target=resolve(root,'sftp-result.bin'),batch=resolve(root,'batch.txt');writeFileSync(source,bytes);
const quote=value=>`"${value.replaceAll('\\','/')}"`;
writeFileSync(batch,`put ${quote(source)} /user/roundtrip.bin\nls -l /user\nget /user/roundtrip.bin ${quote(target)}\nbye\n`);
const result=spawnSync('sftp',['-D',`${quote(executable)} --sftp`,'-b',batch],{encoding:'utf8',timeout:30000});
assert.equal(result.status,0,result.error?.message||result.stderr);assert.deepEqual(readFileSync(target),bytes);
console.log('Real OpenSSH clients: SCP binary/empty uploads and default-buffer SFTP 65KiB put/list/get byte roundtrip passed over production protocol stdio fixtures. No encrypted SSH or ESP socket/hardware acceptance.');
