import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {parseHex,decodeJump,mergeUrboot,encodeHex} from '../tools/prepare-avr-image.mjs';
const word=(memory,at,value)=>{memory.set(at,value&255);memory.set(at+1,value>>>8)};
const jump=(memory,at,target)=>{word(memory,at,0x940c);word(memory,at+2,target/2)};
function fixture(){
  const application=new Map(Array.from({length:512},(_,i)=>[i,(i*13)&255]));
  jump(application,0,0x100);jump(application,100,0x180);
  const boot=new Map(Array.from({length:384},(_,i)=>[0x7e80+i,(i*3)&255]));
  boot.set(0x7ffa,3);boot.set(0x7ffb,25);return {application,boot};
}
{
  const {application,boot}=fixture(),before=new Map(application),result=mergeUrboot(application,boot,0x180);
  assert.equal(decodeJump(result.merged,0),0x7e80);
  assert.equal(decodeJump(result.merged,100),0x100);
  assert.equal(result.binary.length,32768);
  assert.equal(result.binary[0],0x3f);assert.equal(result.binary[1],0xcf);
  for(const [at,value]of before)if(at>=4&&(at<100||at>=104))assert.equal(result.binary[at],value);
  for(const [at,value]of boot)assert.equal(result.binary[at],value);
  assert.deepEqual(application,before,'Application input is never patched in place');
  assert.equal(result.binary[1000],255,'Erased gaps never retain old boot bytes');
  const roundtrip=parseHex(encodeHex(result.binary));assert.equal(roundtrip.size,32768);
  for(let i=0;i<32768;i++)assert.equal(roundtrip.get(i),result.binary[i]);
}
for(const mutate of [
  f=>f.application.set(0x7e80,0),
  f=>f.application.delete(0),
  f=>jump(f.application,0,0x7e80),
  f=>jump(f.application,100,0x120),
  f=>f.application.delete(103),
  f=>f.boot.set(0x7ffa,4),
  f=>f.boot.set(0x7ffb,0),
  f=>f.boot.delete(0x7fff),
  f=>{f.boot.startAddress=0x7e82},
]){const f=fixture();mutate(f);assert.throws(()=>mergeUrboot(f.application,f.boot,0x180));}
{
  const startRecord=parseHex(':020000000102FB\n:0400000300007E80FB\n:00000001FF');
  assert.equal(startRecord.size,2);assert.equal(startRecord.startAddress,0x7e80);
  const text=encodeHex(Buffer.alloc(32768,255));
  assert.throws(()=>parseHex(text.replace(':10000000',':10000001')),'Checksum corruption');
  assert.throws(()=>parseHex(text.replace(':00000001FF','')),'Missing EOF');
  assert.throws(()=>parseHex(text+':00000001FF\r\n'),'Data after EOF');
  const read=path=>readFileSync(new URL('../'+path,import.meta.url),'utf8');
  const chain=[read('build.sh'),read('pakar.sh'),read('tools/pakar.mjs'),read('tools/build-firmware.ps1')];
  const checkChain=([shell,launcher,dispatcher,source])=>{
    assert(shell.includes('pakar.sh'),'Shell/CI must delegate to the shared launcher');
    assert(launcher.includes('tools/pakar.mjs'),'Shared launcher must use the same dispatcher');
    assert(dispatcher.includes("name='build-firmware'"),'Build command must reach the guarded compiler');
    assert(source.includes("'prepare-avr-image.mjs'"),'Canonical build must patch and verify the boot chain');
    assert(source.includes('32768'),'Canonical build must enforce the complete flash size');
  };
  checkChain(chain);
  for(const [index,token] of [[0,'pakar.sh'],[1,'tools/pakar.mjs'],[2,"name='build-firmware'"],[3,"'prepare-avr-image.mjs'"]]){
    const broken=[...chain];broken[index]=broken[index].replaceAll(token,'removed');assert.throws(()=>checkChain(broken));
  }
}
console.log('AVR boot image: reset -> stock Urboot -> original app, unused-vector guard, unchanged code/boot bytes, erased gaps, checksum/EOF bounds and full HEX round trip passed. Structural proof only, not live boot/UART/ISP.');
