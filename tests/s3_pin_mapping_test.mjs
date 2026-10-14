import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = path => readFileSync(new URL(`../${path}`, import.meta.url), 'utf8');
const header = read('firmware/esp32s3/src/BoardPins.h');
const expected = {AVR_RX:18, AVR_TX:17, LED_DATA:4, ISP_RESET:10, ISP_SCK:12, ISP_MISO:13, ISP_MOSI:11};
for (const [signal, pin] of Object.entries(expected)) {
  assert.match(header, new RegExp(`^#define PAKAR_S3_${signal} ${pin}$`, 'm'), signal);
  assert.match(header, new RegExp(`#ifndef PAKAR_S3_${signal}`), 'pin overrides remain available');
}
assert.equal(new Set(Object.values(expected)).size, Object.keys(expected).length);
assert(Object.values(expected).every(pin => pin >= 4 && pin <= 18));
assert.match(header, /static_assert\(safePins\(\)/);
const uart = read('firmware/esp32s3/src/AvrUart.h');
assert.match(uart, /uart_\(1\)/);
assert.match(uart, /uart_\.begin\(kAvrBaud, SERIAL_8N1, PAKAR_S3_AVR_RX, PAKAR_S3_AVR_TX\)/);
const wiring = read('docs/ESP32-S3-MIGRATION.md');
assert(wiring.includes('| GPIO17 / UART1 TX | AVR D0 / PD0 / RXD | S3 to AVR |'));
assert(wiring.includes('| GPIO18 / UART1 RX | AVR D1 / PD1 / TXD | AVR to S3 |'));
console.log('S3 pin source guards: canonical TX17/RX18, crossed AVR UART, explicit begin argument order, unchanged ISP/LED pins and unique conservative defaults. Not an S3 compile or physical test.');
