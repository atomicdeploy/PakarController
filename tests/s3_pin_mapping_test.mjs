import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = path => readFileSync(new URL(`../${path}`, import.meta.url), 'utf8');
const header = read('firmware/esp32s3/src/BoardPins.h');
const expected = {AVR_RX:18, AVR_TX:17, LED_DATA:4, ISP_RESET:10, ISP_SCK:12, ISP_MISO:13, ISP_MOSI:11,
  W5500_SCK:5, W5500_MISO:6, W5500_MOSI:7, W5500_CS:8, W5500_RESET:9, W5500_IRQ:14,
  LCD_SDA:15, LCD_SCL:16};
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
const lcd=read('firmware/esp32s3/src/LcdPort.cpp');
assert.match(lcd,/wire_\.begin\(PAKAR_S3_LCD_SDA,PAKAR_S3_LCD_SCL,100000U\)/);
assert.match(lcd,/wire_\.setTimeOut\(5\)/);
assert.match(read('firmware/esp32s3/src/LcdPort.h'),/TwoWire wire_\{1\}/);
const wiring = read('docs/ESP32-S3-MIGRATION.md');
assert(wiring.includes('| GPIO17 / UART1 TX | AVR D0 / PD0 / RXD | S3 to AVR |'));
assert(wiring.includes('| GPIO18 / UART1 RX | AVR D1 / PD1 / TXD | AVR to S3 |'));
for (const [signal, pin] of [['SCK',5],['MISO',6],['MOSI',7],['CS',8],['RSTn',9],['INTn',14]]) {
  assert(wiring.includes(`| ${signal} | GPIO${pin} |`), `W5500 ${signal} wiring drift`);
}
assert.match(wiring, /SPI3\/HSPI/);
assert.match(wiring, /SPI2\/FSPI/);
console.log('S3 pin source guards: TX17/RX18, explicit UART argument order, unchanged ISP/LED pins, isolated W5500 reservations and unique conservative defaults. Not an S3 compile or physical test.');
