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
assert.match(readFileSync(new URL('../firmware/esp32s3/src/SharedNtfyService.cpp',import.meta.url),'utf8'),
  /#include <firmware\/esp8266\/src\/NtfyService.cpp>/,'The shared ntfy service must actually link on S3');

assert.match(ispHeader, /SPIClass spi_\{FSPI\}/);
assert.match(ispHeader, /#define PAKAR_S3_ISP_ELECTRICAL_VERIFIED 0/);
assert.match(isp, /if \(!PAKAR_S3_ISP_ELECTRICAL_VERIFIED\)/);
assert.match(isp, /spi_\.begin\(PAKAR_S3_ISP_SCK, PAKAR_S3_ISP_MISO, PAKAR_S3_ISP_MOSI, -1\)/);
assert.doesNotMatch(isp, /\bSPI\.(begin|end|transfer)/);
assert.match(platform, /esp_partition_read\(running,offset,out,size\)/);
assert.match(platform, /next->address!=running->address/);
assert.match(platform, /Update\.abort\(\)/);
assert.match(platform, /esp_image_verify\(ESP_IMAGE_VERIFY/);
assert.match(transfer, /esp_partition_read\(running,address,output,length\)/);
const flashRead = transfer.slice(transfer.indexOf('FirmwareExport::ReadResult readFlashRange('), transfer.indexOf('int hexNibble('));
assert.doesNotMatch(flashRead, /ESP\.getSketchSize\s*\(/);
assert.match(flashRead, /address\)\+length>totalSize/);
assert.match(transfer, /new FirmwareExport::ReadResponse\(range.start,contentLength/);
assert.match(transfer, /identityProvider \? identityProvider\(\)/);
assert.match(transfer, /auto \*request=server->request\(\)/);
assert.match(transfer, /auto lease=acquireReader/);
assert.match(read('FirmwareReadResponse.h'),/return RESPONSE_TRY_AGAIN/);
assert.match(read('FirmwareReadResponse.h'),/window_\.valid\(\)/);
assert.match(remote, /header\.chip_id==ESP_CHIP_ID_ESP32S3/);
assert.match(remote, /FirmwarePlatform::abortUpdate\(\)/);
assert.match(remote, /setCACert\(pakarTrustAnchorPem\(\)\)/);
assert.match(remoteHeader, /#include <WiFiClient\.h>/);
const otaStage = readFileSync(new URL('../firmware/esp32s3/src/OtaStage.h', import.meta.url), 'utf8');
assert.match(otaStage, /OTA_WITH_SEQUENTIAL_WRITES/);
assert.match(otaStage, /active_=false;[^\n]*\n\s*ready_=esp_ota_end/);
assert.match(otaStage, /esp_ota_set_boot_partition\(partition_\)/);
assert.match(remote, /if\(!save\(\) \|\| !espStage_\.selectForBoot\(\)\)/);
assert.match(remote, /espStage_\.resume/);
assert.match(remote, /Persisted inactive OTA stage failed validation/);
assert.match(sketch, /remoteFirmware\.ownsEspStage\(\)/);
assert.match(read('ArtifactDownload.h'), /remoteFirmware\.readStagedEsp/);
const maintenance = read('MaintenanceRoutes.h');
const dnsLock = readFileSync(new URL('../firmware/esp32s3/src/DnsCoreLock.cpp', import.meta.url), 'utf8');
assert.match(dnsLock, /alreadyHeld=sys_thread_tcpip\(LWIP_CORE_LOCK_QUERY_HOLDER\)/);
assert.match(dnsLock, /if\(!alreadyHeld\) LOCK_TCPIP_CORE\(\);\s+__real_dns_clear_cache\(\);\s+if\(!alreadyHeld\) UNLOCK_TCPIP_CORE\(\);/);
const networkPath=read('NetworkPath.h');
const dnsRecovery=readFileSync(new URL('../firmware/esp32s3/src/DnsRecovery.h',import.meta.url),'utf8');
function checkDnsPath(main,path,recovery) {
  assert.match(main, /void configureNtp\(\) \{\s+NetworkPath::serviceDns\(millis\(\), !ntpConfigured\);/);
  assert.match(main, /if\(remoteFirmware.needsDownloadConnection\(\)\)\s+NetworkPath::serviceDns\(millis\(\),true\)/);
  assert.match(path, /if \(state\(\).selected == Interface::Wifi\)\s+PakarS3::serviceDhcpDns\(now, networkConfig.dhcpEnabled\(\), immediate\)/);
  assert.match(path, /#if PAKAR_S3_ETHERNET_ENABLED && PAKAR_S3_DNS_GATEWAY_FALLBACK/);
  assert.match(path, /if \(state\(\).selected == Interface::Ethernet && state\(\).ethernetDhcp\) \{/);
  assert.match(path, /if \(!usable\(ETH.dnsIP\(0\)\)\) \{[\s\S]*?usable\(backup\) \? backup : ETH.gatewayIP\(\);\s+if \(usable\(candidate\)\) ETH.dnsIP\(0, candidate\)/);
  assert.match(recovery, /if\(!dhcp \|\| WiFi.status\(\)!=WL_CONNECTED\) return false/);
}
checkDnsPath(sketch,networkPath,dnsRecovery);
assert.throws(()=>checkDnsPath(sketch,networkPath.replace(' && state().ethernetDhcp',''),dnsRecovery));
assert.throws(()=>checkDnsPath(sketch,networkPath.replace('networkConfig.dhcpEnabled()','true'),dnsRecovery));
assert.throws(()=>checkDnsPath(sketch,networkPath,dnsRecovery.replace('!dhcp || ','')));
assert.match(readFileSync(new URL('../tools/build-firmware.ps1', import.meta.url), 'utf8'), /compiler.c.elf.extra_flags=-Wl,--wrap=dns_clear_cache/);
assert.match(maintenance, /if\(plan.targets==RemotePolicy::Esp && !programmingLatched\) return true/);
assert.match(automationStore, /#if !defined\(ESP8266\) && !defined\(ARDUINO_ARCH_ESP8266\) && \\\r?\n\s*!defined\(ESP32\) && !defined\(ARDUINO_ARCH_ESP32\)/);
assert.match(remote, /link_mode"\)\]!="buffered"/);
assert.match(read('LedGrid.cpp'), /kLedDataPin = PAKAR_S3_LED_DATA/);
assert.match(read('LedGrid.cpp'), /FastLED\.addLeds<WS2811, kLedDataPin, RGB>/);
assert.match(read('LedGrid.h'), /#define PAKAR_LED_FRAME_INTERVAL_MS 0/);
const output=readFileSync(new URL('../firmware/esp32s3/src/LedOutput.cpp',import.meta.url),'utf8');
const outputHeader=readFileSync(new URL('../firmware/esp32s3/src/LedOutput.h',import.meta.url),'utf8');
function checkLedMailbox(source,header) {
  const start=source.indexOf('void LedOutput::submit('),end=source.indexOf('void LedOutput::work(){');
  assert.ok(start>=0&&end>start,'Inspect the actual producer and worker separately');
  const submit=source.slice(start,end),work=source.slice(end);
  assert.match(source,/queue_=xQueueCreateStatic\(1,sizeof\(Frame\),queueBytes_,&queueState_\)/,
    'A single complete-frame mailbox coalesces obsolete frames instead of growing a backlog');
  assert.match(header,/alignas\(4\) uint8_t queueBytes_\[sizeof\(Frame\)\]\{\}/);
  assert.match(header,/std::mutex producerGate_;/);
  assert.match(header,/Frame producer_\{\}/,'Producer scratch belongs to the persistent output object');
  assert.match(header,/Frame current_\{\}/,'The worker owns a distinct frame, never producer scratch');
  assert.doesNotMatch(submit,/\bFrame\s+\w+\s*[;={]/,'Do not put a full frame on the loop/TLS stack');
  assert.match(submit,/std::lock_guard<std::mutex>\s+\w+\(producerGate_\);[\s\S]*?::memcpy\(producer_\.rgb,rgb,sizeof\(producer_\.rgb\)\);[\s\S]*?producer_\.brightness=brightness;[\s\S]*?producer_\.enabled=enabled;[\s\S]*?producer_\.revision=revision;[\s\S]*?xQueueOverwrite\(queue_,&producer_\);/,
    'Serialize the entire RGB/brightness/enabled/revision copy through mailbox publication');
  assert.doesNotMatch(submit,/\b(?:current_|rmt_transmit|writeStaticWire|encodeWireBytes)\b/,
    'A producer must not edit or transmit the worker-owned frame');
  assert.equal([...work.matchAll(/xQueueReceive\(queue_,&current_,pdMS_TO_TICKS\(10\)\)/g)].length,2,
    'Both backends receive complete frames into separate worker-owned storage');
  const encoding=/encodeWireBytes\(current_\.rgb,pixels,current_\.brightness,current_\.enabled,payload\)/g;
  const dmaStart=work.indexOf('  rmt_channel_handle_t channel=');
  assert.ok(dmaStart>0,'Inspect static and DMA worker admission separately');
  assert.equal([...work.slice(0,dmaStart).matchAll(encoding)].length,1,
    'Static worker encodes its own immutable transmission payload');
  assert.equal([...work.slice(dmaStart).matchAll(encoding)].length,2,
    'DMA encodes a received frame, then may adopt a newer complete frame before admission');
  assert.match(work.slice(dmaStart),/std::lock_guard<std::mutex> lock\(wireGate_\);\s+if\(flashHolds_\.load\(\)\)continue;[\s\S]*?if\(!initialBlack && xQueueReceive\(queue_,&current_,0\)==pdTRUE\)\s+encodeWireBytes\(current_\.rgb,pixels,current_\.brightness,current_\.enabled,payload\);\s+inFlight_\.store\(true\);/,
    'Newest-frame adoption is under the flash admission gate, before immutable in-flight ownership');
}
checkLedMailbox(output,outputHeader);
assert.throws(()=>checkLedMailbox(output.replace('xQueueCreateStatic(1,','xQueueCreateStatic(2,'),outputHeader));
assert.throws(()=>checkLedMailbox(output.replace('std::lock_guard<std::mutex> lock(producerGate_);',''),outputHeader));
assert.throws(()=>checkLedMailbox(output.replace('xQueueOverwrite(queue_,&producer_)','xQueueOverwrite(queue_,&current_)'),outputHeader));
assert.throws(()=>checkLedMailbox(output.replace('::memcpy(producer_.rgb','::memcpy(current_.rgb'),outputHeader));
assert.throws(()=>checkLedMailbox(output.replace('producer_.enabled=enabled;',''),outputHeader));
assert.throws(()=>checkLedMailbox(output,outputHeader.replace('Frame producer_{};','')));
assert.throws(()=>checkLedMailbox(output.replaceAll('xQueueReceive(queue_,&current_,','xQueueReceive(queue_,&producer_,'),outputHeader));
assert.throws(()=>checkLedMailbox(output.replace('if(!initialBlack && xQueueReceive','if(xQueueReceive'),outputHeader));
assert.throws(()=>checkLedMailbox(output.replace('xQueueReceive(queue_,&current_,0)','xQueueReceive(queue_,&current_,1)'),outputHeader));
assert.match(output,/config.flags.with_dma=true/);
assert.match(output,/xTaskCreatePinnedToCore\(run,"pakar-led",4096,this,2,nullptr,1\)/);
assert.match(output,/rmt_tx_wait_all_done\(channel,100\)/);
assert.match(sketch,/loopMessages.pop\(message\)/);
assert.doesNotMatch(sketch,/handleWebSocketMessage\(client, info->opcode/);
assert.match(sketch, /const SceneState next = captureLedScene\(sceneStore.state\(\), grid/);
assert.match(sketch, /if \(complete\) applyLedBootPreferences\(grid, value\)/);
assert.match(sketch, /sceneStore.begin\(LittleFS, filesystemMounted, fallback\)/);
assert.match(sketch, /restoreLedScene\(grid, saved\)/);
assert.match(sketch, /grid.begin\(false\)/, 'Restore a valid scene before first LED publication');
assert.match(sketch, /esp_efuse_mac_get_default\(stationMac\)/,
  'S3 identity must not depend on an active station interface');
assert.match(sketch, /available &= ~BoardCore::Context::Isp/,
  'Disabled ISP must not appear among acknowledged feature IDs');
assert.match(sketch, /body \+= F\("buffered"\)/);
assert.match(sketch, /if \(!PAKAR_AVR_LINK_ENABLED \|\| urbootRawOwned\) return false/,
  'Quiet commissioning and exclusive raw bootloader ownership cannot send framed AVR requests');
assert.match(sketch, /avrUart\.begin\(onAvrFrame\)/);
assert.match(sketch, /PakarS3::AvrUart avrUart/);
const avrDrain=sketch.slice(sketch.indexOf('void serviceAvrFor(uint32_t durationMs) {'),sketch.indexOf('bool requestAndWaitForAck('));
assert.match(avrDrain,/if \(!PAKAR_AVR_LINK_ENABLED\) return;/,
  'Disabled AVR must return before time measurement, UART service, delays or yields; no download pacing tax');
assert.match(avrDrain,/#if defined\(ARDUINO_ARCH_ESP32\) && PAKAR_UART_BUFFERED[\s\S]*?link\.service\(\);\s+return;\s+#else/,
  'Buffered S3 drains once without imposing the legacy per-chunk sleep');
assert.match(avrDrain,/link\.service\(\);\s+delay\(1\);\s+yield\(\);/,
  'Enabled AVR retains its existing bounded UART drain behavior');
assert.match(sketch, /case kAvrRxDiagnostic:/);
assert.match(sketch, /passiveAvrRx\.service\(millis\(\)\)/);
const passive = readFileSync(new URL('../firmware/esp32s3/src/PassiveAvrRx.h', import.meta.url), 'utf8');
assert.doesNotMatch(passive, /Serial2\.(write|print|printf)\s*\(/);
assert.doesNotMatch(passive, /digitalWrite\s*\(|pinMode\s*\(/);
assert.match(passive, /uart_get_TxPin\(2\) == -1/);
assert.match(passive, /GPIO_NUM_17/);
assert.match(passive, /!PAKAR_AVR_LINK_ENABLED/);

console.log('S3 platform source guards passed: isolated gated ISP, partition-relative downloads, explicit OTA abort, TLS roots and shared uncapped LED engine. Not a firmware compile or physical test.');
