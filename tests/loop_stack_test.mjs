import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const main=readFileSync(new URL('../firmware/esp8266/esp8266.ino',import.meta.url),'utf8');
function loopStack(source) {
 const loop=source.slice(source.indexOf('void loop() {'));
 assert.match(source,/__attribute__\(\(noinline\)\) void serviceStream\(\)/);
 assert.match(source,/__attribute__\(\(noinline\)\) void serviceQueuedWebSocketMessage\(\)/);
 assert.match(loop,/serviceQueuedWebSocketMessage\(\);/);
 assert.doesNotMatch(loop,/LoopMessages::Message\s|uint8_t\s+packet\[1300\]/);
}
loopStack(main);
assert.throws(()=>loopStack(main.replace('__attribute__((noinline)) void serviceStream','void serviceStream')));
assert.throws(()=>loopStack(main.replace('void loop() {','void loop() { LoopMessages::Message message;')));
console.log('Loop stack lifetime guards: message and packet workspaces cannot inline into cloud/telemetry call path. Static source guard; actual SDK frame sizes and live MQTT acceptance remain separate.');
