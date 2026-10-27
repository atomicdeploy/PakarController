import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../'+p,import.meta.url),'utf8');
const avr=read('firmware/avr/avr.ino');
assert.match(avr,/BoardCore::Tasks taskManager/);
assert.match(avr,/resetTask = taskManager.addTask\(100, executeDeferredReset, nullptr, now\)/);
const safety=avr.slice(avr.indexOf('void applyProgrammingSafety()'),avr.indexOf('void showState() {'));
assert.match(safety,/cancelDeferredReset\(\)/,'Prog cancels old reset ownership');
assert.match(safety,/if \(programmingLatch.active\(\)\) return;[\s\S]*wdt_enable/,'Last reset dispatch must check the latch');
assert.match(avr,/taskManager.update\(now\)/);
assert.doesNotMatch(avr,/resetPendingAt|resetRequested/,'No parallel uncancellable reset timer');
assert.match(avr,/constexpr uint16_t kShiftPeriodMs = PAKAR_FIRST_WINS_ENABLED \? 1 : 5;/,
  'First-wins must not inherit the table role\'s five-millisecond input sampling delay');
assert.match(read('tools/test-protocol.ps1'),/test-tasks.ps1/);
assert.match(read('tools/test-protocol.sh'),/test-tasks.ps1/);
assert.match(read('tools/build-firmware.ps1'),/-Concern cooperative-tasks -Gate/);
console.log('AVR scheduler/reset/Prog wiring guards passed; source assertions, not physical watchdog proof.');
