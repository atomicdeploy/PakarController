import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = name => readFileSync(new URL(`../firmware/esp8266/src/${name}`, import.meta.url), 'utf8');
const ispHeader = read('AvrIsp.h');
const isp = read('AvrIsp.cpp');
const platform = read('FirmwarePlatform.h');
const transfer = read('FirmwareTransfer.cpp');
const remote = read('RemoteFirmware.cpp');
const remoteHeader = read('RemoteFirmware.h');
const automationStore = readFileSync(new URL('../libraries/BoardCore/src/pccontroller/AutomationStore.cpp', import.meta.url), 'utf8');
const sketch = readFileSync(new URL('../firmware/esp8266/esp8266.ino', import.meta.url), 'utf8');

assert.match(ispHeader, /SPIClass spi_\{FSPI\}/);
assert.match(ispHeader, /#define PAKAR_S3_ISP_ELECTRICAL_VERIFIED 0/);
assert.match(isp, /if \(!PAKAR_S3_ISP_ELECTRICAL_VERIFIED\)/);
assert.match(isp, /spi_\.begin\(PAKAR_S3_ISP_SCK, PAKAR_S3_ISP_MISO, PAKAR_S3_ISP_MOSI, -1\)/);
assert.doesNotMatch(isp, /\bSPI\.(begin|end|transfer)/);
assert.match(platform, /esp_partition_read\(running,offset,out,size\)/);
assert.match(platform, /next->address!=running->address/);
assert.match(platform, /Update\.abort\(\)/);
assert.match(platform, /esp_image_verify\(ESP_IMAGE_VERIFY/);
assert.match(transfer, /FirmwarePlatform::readRunning\(address,output,length\)/);
assert.match(remote, /header\.chip_id==ESP_CHIP_ID_ESP32S3/);
assert.match(remote, /FirmwarePlatform::abortUpdate\(\)/);
assert.match(remote, /setCACert\(pakarTrustAnchorPem\(\)\)/);
assert.match(remoteHeader, /#include <WiFiClient\.h>/);
assert.match(automationStore, /#if !defined\(ESP8266\) && !defined\(ARDUINO_ARCH_ESP8266\) && \\\r?\n\s*!defined\(ESP32\) && !defined\(ARDUINO_ARCH_ESP32\)/);
assert.match(remote, /link_mode"\)\]!="buffered"/);
assert.match(read('LedGrid.cpp'), /kLedDataPin = PAKAR_S3_LED_DATA/);
assert.match(read('LedGrid.cpp'), /FastLED\.addLeds<WS2811, kLedDataPin, RGB>/);
assert.match(read('LedGrid.h'), /#define PAKAR_LED_FRAME_INTERVAL_MS 0/);
assert.match(sketch, /const LedPreferences value = whiteBootPreferences\(grid.brightness\(\)\)/);
assert.match(sketch, /if \(complete\) applyLedBootPreferences\(grid, value\)/);
assert.doesNotMatch(sketch, /grid\.setPattern\(static_cast<LedGrid::Pattern>\(value.pattern\)\)/);
assert.match(sketch, /esp_efuse_mac_get_default\(stationMac\)/,
  'S3 identity must not depend on an active station interface');
assert.match(sketch, /available &= ~BoardCore::Context::Isp/,
  'Disabled ISP must not appear among acknowledged feature IDs');
assert.match(sketch, /body \+= F\("buffered"\)/);
assert.match(sketch, /if \(!PAKAR_AVR_LINK_ENABLED\) return false/,
  'Quiet commissioning cannot send normal AVR UART requests');
assert.match(sketch, /if \(PAKAR_AVR_LINK_ENABLED\) avrLink.begin/);
assert.match(sketch, /case kAvrRxDiagnostic:/);
assert.match(sketch, /passiveAvrRx\.service\(millis\(\)\)/);
const passive = readFileSync(new URL('../firmware/esp32s3/src/PassiveAvrRx.h', import.meta.url), 'utf8');
assert.doesNotMatch(passive, /Serial2\.(write|print|printf)\s*\(/);
assert.doesNotMatch(passive, /digitalWrite\s*\(|pinMode\s*\(/);
assert.match(passive, /uart_get_TxPin\(2\) == -1/);
assert.match(passive, /GPIO_NUM_17/);
assert.match(passive, /!PAKAR_AVR_LINK_ENABLED/);

console.log('S3 platform source guards passed: isolated gated ISP, partition-relative downloads, explicit OTA abort, TLS roots and shared uncapped LED engine. Not a firmware compile or physical test.');
