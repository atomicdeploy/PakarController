import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../'+p,import.meta.url),'utf8');
const esp=read('firmware/esp/application.ino'),avr=read('firmware/avr/avr.ino');
const usb=read('firmware/esp/src/UsbApiService.h'),grid=read('firmware/esp/src/LedGrid.cpp');
assert.match(esp,/server.on\("\/api\/presentation",HTTP_GET/);
assert.match(esp,/server.on\("\/api\/presentation",HTTP_POST,[\s\S]+?commandReply\(message\)/);
assert.match(usb,/path=="\/api\/presentation"[\s\S]+?commandReply\(command,physicalUsb\)/);
assert.match(esp,/command=="presentation"[\s\S]+?applyHostPresentation\(commandRequest\)/);
assert.match(esp,/command=="pixel-corrections-preview"[\s\S]+?applyCorrectionPreview\(settings/);
const loop=esp.slice(esp.indexOf('void loop() {'));
assert(loop.indexOf('hostPresentation.model.service')<loop.indexOf('if(urbootQueued)'));
assert(loop.indexOf('serviceQueuedWebSocketMessage();')<loop.indexOf('servicePeerRelay();'));
assert(loop.indexOf('serviceQueuedWebSocketMessage();')<loop.indexOf('NetworkPath::service()'));
const render=grid.slice(grid.indexOf('void LedGrid::showStaged()'),grid.indexOf('void LedGrid::setOutputEnabled'));
assert(render.indexOf('presentation_->compose')<render.indexOf('corrected(pixels_)'));
assert.match(render,/previewing \|\| diagnosing \|\| presenting/);
assert.match(grid,/presentation_revision/);
for(const [opcode,next]of [['kBuzzer','kAudioStream'],['kAudioCue','kDisplayText']]){
  const action=avr.slice(avr.indexOf('case '+opcode+':'),avr.indexOf('case '+next+':'));
  assert(action.indexOf('buzzer.update(millis())')>=0);
  assert(action.indexOf('buzzer.update(millis())')<action.indexOf('link.ack('));
}
const relay=avr.slice(avr.indexOf('case kOutputMask:'),avr.indexOf('case kBuzzer:'));
assert(relay.indexOf('shiftRegisters.service()')<relay.indexOf('link.ack('));
assert.match(esp,/command=="outputs"/);
assert.match(esp,/reply\["state"\]="acknowledged";reply\["avr_acknowledged"\]=true/);
for(const opcode of ['kBuzzer','kAudioCue','kOutputMask']){
  assert(esp.includes(`return submitAvrCommand(${opcode},`));
  assert(esp.includes(`sendAvrHttpCommand(${opcode},`));
}
assert.match(esp,/requestAndWaitForAck\(opcode,payload,length,150\)/);
assert.match(esp,/entry->observe\(hostPresentation.model.revision,hostPresentation.model.lease\)/);
assert.match(esp,/entry->observationMatches\(pixelCorrections.activeRevision\(\),pixelCorrections.previewLease\(\)\)/);
assert.match(esp,/if\(!observed\)[\s\S]*?reply\["accepted"\]=false;reply\["original_accepted"\]=true/);
assert.match(esp,/reply\["acknowledged_mask"\]=request\["mask"\]/);
assert.match(read('libraries/BoardCore/src/CommandReceipts.h'),/avr_outcome_unknown/);
assert.match(usb,/if\(!path\.startsWith\("\/user\/"\)/); // Persistent presentation cannot bypass validated save.
console.log('Host presentation/common transport + immediate AVR scheduling source guards passed; not hardware latency evidence.');
