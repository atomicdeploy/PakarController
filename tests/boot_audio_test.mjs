import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const esp = readFileSync(new URL('../firmware/esp/application.ino', import.meta.url), 'utf8');
const avr = readFileSync(new URL('../firmware/avr/avr.ino', import.meta.url), 'utf8');
for(const [source,owner] of [[avr,'link'],[esp,'hostLink']]) {
  const start=source.indexOf('case kSetMode:',source.indexOf(owner==='hostLink'?'void onHostFrame(':'void handleFrame('));
  const mode=source.slice(start,source.indexOf('case kCellAction:',start));
  assert.match(mode,new RegExp('if \\(mode == frame.payload\\[0\\]\\) \\{'+owner+'\\.ack\\(frame.sequence,frame.opcode\\);break;\\}'),
    'Restating the active role must ACK without resetting the winner');
}
assert.match(esp,/if\(mode==parseMode\(value\)\)\{result=F\("mode unchanged"\);return true;\}/);
assert.match(esp,/if\(mode==requested\)\{server.send\(200,[^\n]*changed[^\n]*return;\}/);
assert(!esp.includes('startupCuePlayed'), 'First link must not schedule a false update-success cue');
assert.match(esp, /const auto upload=uploadAttention.take\(\);[\s\S]*?if\(upload==Cue::Failure\)dispatch\(upload\)/,
  'Receiving complete cannot play success before verified reboot');
assert.match(esp, /uploadAttention.finish\(true\)/);
assert.match(esp, /uploadAttention.begin\(\)/);
const cues=esp.slice(esp.indexOf('bool updateCueAvailable('),esp.indexOf('String upgradeJson(bool event,bool phaseChanged)'));
assert.doesNotMatch(cues,/requestAndWaitForAck|serviceAvrFor|legacyCue/,'Attention must not block or replay an uncertain cue via another command');
assert.match(cues,/avrAudioCueCount>cue/);
assert.match(cues,/!programmingLatched/);
assert.match(cues,/avrOnline && avrVersionKnown/);
for(const name of ['Start','Success','Failure']) {
  const fn=cues.slice(cues.indexOf(`bool playUpdate${name}Cue()`));
  assert.match(fn.slice(0,fn.indexOf('\n}')),/updateCueAvailable\(cue\)/,'Every automatic cue uses the shared fresh-capability admission');
}
const lifecycle=esp.slice(esp.indexOf('void serviceUpgrade() {'),esp.indexOf('void upgradeBackground() {'));
assert.match(lifecycle,/localOtaReceipt.takeNotice\(\)/);
assert.match(lifecycle,/updateCueAvailable\(completionCue\)/);
assert.match(lifecycle,/!urbootRawOwned && pendingUpgradeCloud.length\(\)/);
assert.match(lifecycle,/if\(!urbootRawOwned\) \{/);
const urboot=readFileSync(new URL('../firmware/esp/src/UrbootRoutes.h',import.meta.url),'utf8');
const raw=urboot.slice(urboot.indexOf('bool runUrboot('),urboot.indexOf('void serviceUrbootJob()'));
assert.doesNotMatch(raw,/avrLink.service\(|shutdownMs/,'Raw bootloader UART must never use framed parser or cosmetic delays');
const job=urboot.slice(urboot.indexOf('void serviceUrbootJob()'),urboot.indexOf('void registerUrbootRoutes()'));
assert.ok(job.indexOf('shutdownMs')<job.indexOf('requestAndWaitForAck('));
assert.ok(job.indexOf('acquireFlashPause()')<job.indexOf('avrUart.acquireRaw()'));
assert.match(job,/if\(!grid.maintenancePresented\(\)\)/);
assert.match(job,/if\(outputHeld\)PakarS3::LedOutput::releaseFlashPause\(\)/);
const setup = avr.slice(avr.indexOf('void setup()'), avr.indexOf('void loop()'));
function checkWelcome(body) {
  assert.match(body,/const uint8_t flags=pcController\.settings\(\)\.values\(\)\.flags;/);
  assert.match(body,/if\s*\(!programmingLatch\.active\(\)\s*&&\s*\(flags & 2U\)\s*&&\s*!\(flags & 1U\)\s*&&\s*resetTelemetry\.allowWelcome\(\)\)\s*playBootMelody\(\);/,
    'Welcome requires enabled cue, silent mode OFF, no Prog and reset-loop suppression');
}
checkWelcome(setup);
const reset=avr.slice(avr.indexOf('void resetGame('),avr.indexOf('void chooseWinner('));
assert.ok(reset.indexOf('const bool roundWasWon=winner!=0;')<reset.indexOf('winner = 0;'),
  'Capture the real won state before clearing it');
assert.match(reset,/if\(roundWasWon\)\s*\{\s*buzzer.stop\(\);\s*if\(gameResetCue<kCueCount\)playAudioCue/,
  'Repeated ready-state reset must neither restart nor cancel the reset melody');
assert.match(reset,/shiftRegisters.allOutputsOff\(\)/,'Idempotent reset still repairs relay outputs');
for(const guard of ['!programmingLatch.active()','(flags & 2U)','!(flags & 1U)','resetTelemetry.allowWelcome()'])
  assert.throws(()=>checkWelcome(setup.replaceAll(guard,'true')),guard+' must remain required');
console.log('Boot audio source routing passed: AVR welcome; ESP update success only for updates. Not an audible timing test.');
