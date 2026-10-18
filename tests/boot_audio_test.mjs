import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const esp = readFileSync(new URL('../firmware/esp8266/esp8266.ino', import.meta.url), 'utf8');
const avr = readFileSync(new URL('../firmware/avr/avr.ino', import.meta.url), 'utf8');
assert(!esp.includes('startupCuePlayed'), 'First link must not schedule a false update-success cue');
assert.match(esp, /dispatch\(uploadAttention.take\(\)\)/);
assert.match(esp, /uploadAttention.finish\(true\)/);
assert.match(esp, /uploadAttention.begin\(\)/);
const cues=esp.slice(esp.indexOf('bool playUpdateStartCue()'),esp.indexOf('bool authenticate()'));
assert.doesNotMatch(cues,/requestAndWaitForAck|serviceAvrFor|legacyCue/,'Attention must not block or replay an uncertain cue via another command');
assert.match(cues,/avrAudioCueCount>cue/);
assert.match(cues,/!programmingLatched/);
assert.equal((cues.match(/avrOnline && avrVersionKnown/g)||[]).length,3,'Every automatic cue requires fresh HELLO metadata, not stale cached capabilities');
const setup = avr.slice(avr.indexOf('void setup()'), avr.indexOf('void loop()'));
function checkWelcome(body) {
  assert.match(body,/const uint8_t flags=pcController\.settings\(\)\.values\(\)\.flags;/);
  assert.match(body,/if\s*\(!programmingLatch\.active\(\)\s*&&\s*\(flags & 2U\)\s*&&\s*!\(flags & 1U\)\s*&&\s*resetTelemetry\.allowWelcome\(\)\)\s*playBootMelody\(\);/,
    'Welcome requires enabled cue, silent mode OFF, no Prog and reset-loop suppression');
}
checkWelcome(setup);
for(const guard of ['!programmingLatch.active()','(flags & 2U)','!(flags & 1U)','resetTelemetry.allowWelcome()'])
  assert.throws(()=>checkWelcome(setup.replaceAll(guard,'true')),guard+' must remain required');
console.log('Boot audio source routing passed: AVR welcome; ESP update success only for updates. Not an audible timing test.');
