import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const esp = readFileSync(new URL('../firmware/esp8266/esp8266.ino', import.meta.url), 'utf8');
const avr = readFileSync(new URL('../firmware/avr/avr.ino', import.meta.url), 'utf8');
assert(!esp.includes('startupCuePlayed'), 'First link must not schedule a false update-success cue');
assert.match(esp, /if \(otaSuccessCuePending\) \{ otaSuccessCuePending = false; playUpdateSuccessCue\(\); \}/);
const setup = avr.slice(avr.indexOf('void setup()'), avr.indexOf('void loop()'));
assert.match(setup, /if \(!programmingLatch\.active\(\)\) playBootMelody\(\);/);
console.log('Boot audio source routing passed: AVR welcome; ESP update success only for updates. Not an audible timing test.');
