import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../'+path,import.meta.url),'utf8');
const driver=read('firmware/esp32s3/src/LedOutput.cpp');
const header=read('firmware/esp32s3/src/LedOutput.h');
const grid=read('firmware/esp8266/src/LedGrid.cpp');
const wire=read('firmware/esp32s3/src/LedWire.h');
function driverContract(source) {
 const work=source.slice(source.indexOf('void LedOutput::work(){'));
 const reset=work.indexOf('gpio_reset_pin('),low=work.indexOf('gpio_set_level(');
 const output=work.indexOf('gpio_set_direction('),allocate=work.indexOf('heap_caps_malloc(');
 assert.ok(reset>=0&&reset<low&&low<output&&output<allocate,'Idle LOW preloaded before output/allocation');
 assert.match(work.slice(0,allocate),/if\(pinError\)\{error_\.store\(pinError\);vTaskDelete\(nullptr\);return;\}/);
 assert.match(source,/xQueueCreateStatic\(1,sizeof\(Frame\)/);
 assert.match(source,/xQueueOverwrite\(queue_,&producer_\)/);
 assert.match(work,/encodeWireBytes\(current_\.rgb/);
 assert.match(work,/flags\.eot_level=0/);
 assert.match(work,/rmt_tx_wait_all_done\(channel,100\)/);
 assert.match(work,/esp_rom_delay_us\(resetMicros\)/);
 assert.doesNotMatch(source,/#include <driver\/rmt\.h>|rmt_write_items\(|rmt_item32_t|ESP_ERROR_CHECK\(/);
}
driverContract(driver);
assert.throws(()=>driverContract(driver.replace('if(!pinError)pinError=gpio_set_level','if(!pinError)pinError=missing_set_level')));
assert.match(header,/Frame current_\{\}/);assert.match(header,/Frame producer_\{\}/);
assert.match(wire,/resetMicros=300/);
assert.match(wire,/PAKAR_S3_LED_SWAP_RED_BLUE/);
assert.match(grid,/bool LedGrid::stagePixels/);assert.match(grid,/String LedGrid::pixelFrameJson/);
assert.match(grid,/void LedGrid::setOutputEnabled/);
assert.doesNotMatch(grid,/WS2811Effect|effectTask|vTaskDelete\(|\bdelay\(/);
assert.match(read('tools/build-firmware.ps1'),/-Concern esp32-ws2811 -Gate/);
assert.match(read('tools/test-led-worker.ps1'),/-Concern esp32-ws2811 -Gate/);
const manifest=JSON.parse(read('docs/file-comparison-evidence.json'));
const entries=manifest.files.filter(f=>f.origin==='esp32WS2811');
assert.equal(entries.filter(f=>f.path.startsWith('src/')).length,14);
for(const effect of ['Aurora','Autumn','Circus','SnowSparkle'])for(const extension of ['cpp','h']) {
 const entry=entries.find(f=>f.path===`src/Effects/${effect}.${extension}`);
 assert.equal(entry?.verdict,'parity-gap');
 assert.ok(entry.omissions.some(o=>o.disposition==='pending'),'Optional effects may not be falsely certified imported');
}
console.log('Pinned esp32WS2811 mapping: modern single worker/mailbox, immutable frame, safe LOW initialization, explicit latch and nonduplicated engine guarded; four additional effects remain honestly pending. Not physical proof.');
