import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import vm from 'node:vm';
const source = await readFile(new URL('../firmware/esp/src/WebUi.h', import.meta.url), 'utf8');
const code = source.split('// BEGIN BOARD SILENT')[1].split('let boardSoundEpoch=')[0];
const {parseBoardSound, boardSoundTransaction} = vm.runInNewContext(code + '\n({parseBoardSound,boardSoundTransaction})', {URLSearchParams,setTimeout});
function payload(flags=2,persisted=1){return Buffer.from([0x50,flags,5,0,0,0,2,0,63,0x10,0x32,0x54,0,...Array(8).fill(0),persisted]).toString('hex')}
for (const bad of ['',payload(8),payload(2,2),'ff'.repeat(22),payload().slice(2)]) assert.throws(()=>parseBoardSound(bad));
for (const mode of ['normal','bad-ack','bad-readback','reboot','retired','rejected']) {
  let settings=payload(),writes=0,reads=0,guards=0,id=0,current=null;
  const request=async(path,options={})=>{
    if(options.method==='POST') {
      const op=Number(options.body.get('opcode'));id++;
      if(op===5){writes++;const next=options.body.get('payload_hex');assert.equal(next.slice(4),settings.slice(4,-2));
        settings=next+(mode==='bad-readback'?'00':'01');}
      else {assert.equal(op,4);reads++;}
      current={request_id:id,boot_id:'boot',request_opcode:op,state:'responded',response_opcode:op===4?0x91:0x80,payload_hex:op===4?settings:'0500'};
      if(mode==='bad-ack'&&op===5)current.payload_hex='0400';
      if(mode==='rejected'&&op===5){current.response_opcode=0x82;current.payload_hex='0506';}
      return {request_id:id,boot_id:mode==='reboot'?'other':'boot'};
    }
    if(options.method==='DELETE')return {consumed:true};
    if(path==='/api/status')return {};
    return current;
  };
  const guard=()=>{if(mode==='retired'&&++guards>3)throw Error('Retired')};
  const result=boardSoundTransaction(request,guard,true,'boot');
  if(mode==='normal'){
    const value=await result;assert(value.silent&&value.persisted);assert.equal(writes,1);assert.equal(reads,2);
    await boardSoundTransaction(request,()=>{},true,'boot');assert.equal(writes,1,'already saved must not rewrite EEPROM');
    const valueOff=await boardSoundTransaction(request,()=>{},false,'boot');assert.equal(valueOff.silent,false);assert.equal(writes,2);
  }else {await assert.rejects(result);assert(writes<=1,'never replay uncertain write');}
}
assert(source.includes('id="boardSoundPreferences" hidden'));
assert(source.includes("currentPeer().has('settings')&&currentPeer().has('buzzer')"));
console.log('Board Silent Web contract: strict settings, preserve other bytes, ACK/readback, persistence, rejection, peer retirement and no replay passed (simulated, not hardware).');
