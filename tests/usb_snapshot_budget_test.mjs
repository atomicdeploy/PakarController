import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = path => readFileSync(new URL(`../${path}`, import.meta.url), 'utf8');
const main=read('firmware/esp8266/esp8266.ino');
const driver=read('firmware/esp32s3/src/LedOutput.cpp');
const ethernet=read('firmware/esp8266/src/EthernetManagement.h');
const status=main.slice(main.indexOf('String buildStatusJson(bool compact) {'),main.indexOf('void sendStatusJson()'));
const snapshot=main.slice(main.indexOf('String buildUsbTableSnapshot(bool includePixels) {'),main.indexOf('void configureNtp()'));
assert(main.includes('String buildStatusJson() { return buildStatusJson(false); }'));
const configChange=main.slice(main.indexOf('String configurationStateJson('),main.indexOf('void sendStatusJson()'));
for(const field of ['configuration-changed','board_id','boot_id','revision','phase','scopes','hostname','configured_hostname','fqdn','addresses','apply_pending'])
  assert(configChange.includes(`"${field}"`));
for(const secret of ['password','token','stationSsid','mqttUser'])assert(!configChange.includes(secret));
assert(configChange.includes('bytes>1536U')&&configChange.includes('serializeJson(d,body)!=bytes'));
assert(configChange.includes('for(uint8_t n=0;n<3;++n)'));
assert(configChange.indexOf('configurationStateJson(&event)')<configChange.indexOf('changes.retire(event)'));
assert(configChange.includes('publishPcEvent(body)')&&configChange.includes('sendHostStatus(0)'));
assert(configChange.includes('webSocket.textAll(buildStatusJson())')&&configChange.includes('cloudServices.requestTelemetry()'));
assert(main.includes('server.on("/api/configuration",HTTP_GET'));
const cloudStatus=main.slice(main.indexOf('String buildCloudStatusJson() {'),main.indexOf('void sendStatusJson()'));
assert(main.includes('#if defined(ARDUINO_ARCH_ESP32)\nString buildCloudStatusJson()'));
assert(cloudStatus.includes('const bool diagnostic=grid.diagnosticActive();'));
assert(cloudStatus.includes('buildStatusJson(diagnostic)'));
assert(cloudStatus.indexOf('if(!diagnostic')<cloudStatus.indexOf('grid.pixelFrameJson()'));
assert(cloudStatus.includes('bytes>16384U || !body.reserve(bytes)'));
assert(cloudStatus.indexOf('body.reserve(bytes)')<cloudStatus.indexOf('body.remove('));
assert(cloudStatus.includes('if(body.length()!=bytes)return String();'));
assert(main.includes('cloudServices.begin(hostname, handleCloudCommand, buildCloudStatusJson,buildTelemetryJson,pcBoardId,pcBootId)'));
assert(snapshot.includes('buildStatusJson(true)'));
assert(snapshot.includes('status.length()>10240'));
assert(snapshot.includes('body.length()>16384'));
assert(!snapshot.includes('deserializeJson'));
assert(status.includes('ethernetStatusJson(compact)'));
assert(status.includes('grid.outputJson(compact)'));
assert(read('firmware/esp8266/src/LedGrid.cpp').includes('output_.json(compact)'));
for(const field of ['board_id','boot_id','compatibility','command_clock_ms','capabilities',
  'application','acknowledged_features','audio_cues','status_rgb','tm1637_detection_known',
  'lcd','mirroring','front_panel','panel_presentation','update','upgrade','avr_maintenance','remote_firmware'])
  assert(status.includes(`\\"${field}\\"`),`Missing retained ${field}`);
assert(status.includes('applicationIdentityJson()'));
assert(main.includes('body += FPSTR(APP_ICON_PNG_BASE64)'));
assert(snapshot.includes('appendJsonEscaped(body,cellMapping.names[i])'));
assert(snapshot.includes('grid.pixelFrameJson()'));
assert(snapshot.includes('if(includePixels)body+=grid.pixelFrameJson();else body+=F("null")'));
assert(snapshot.indexOf('if(includePixels)')<snapshot.indexOf('grid.pixelFrameJson()'));
assert(snapshot.includes('budget=includePixels?16384U:13200U'));
assert(driver.includes('String LedOutput::json(bool compact)const'));
const detail=driver.slice(driver.indexOf('  if(!compact) {',driver.indexOf('String LedOutput::json')),driver.indexOf('  value+=F(",\\"submitted'));
for(const field of ['signal_capture','frame_timing','refill_irq','done_symbols'])assert(detail.includes(field));
for(const field of ['transmitted','failed','revision','error_code'])assert(!detail.includes(`\\"${field}\\"`));
assert(ethernet.indexOf('if(!compact) {')<ethernet.indexOf('PakarS3::LldpStatus lldp'));

const physical=Array.from({length:80},(_,i)=>i+1);
const pixels={revision:4294967295,brightness:255,pending:false,rgb:'ffffff'.repeat(480),physical};
const names=Array(80).fill('\\'.repeat(15));
const size=value=>Buffer.byteLength(JSON.stringify(value));
assert.equal(size(pixels),3189);
const cloudFields=',"pixel_preview_requested":true,"pixel_frame":';
assert.equal(Buffer.byteLength(cloudFields),46);
const diagnosticStatusBytes=10240;
assert(diagnosticStatusBytes+Buffer.byteLength(cloudFields)+size(pixels)<=16384);
assert(16384-Buffer.byteLength(cloudFields)-size(pixels)+1+Buffer.byteLength(cloudFields)+size(pixels)>16384);
assert.equal(size(names),2641);
assert.equal(size(physical),232);
const envelope=size({status:{},physical,names,pixels})-2;
assert.equal(envelope,6104);
assert.equal(envelope+10240,16344);
const coreEnvelope=size({status:{},physical,names,pixels:null})-2;
assert.equal(coreEnvelope+10240,13159);
assert(coreEnvelope+10240<=13200,'Core-only capture has its own smaller bounded allocation');
assert(envelope+10240<16384,'Worst accepted status plus escaped names must fit');
assert(envelope+10281>16384,'Fixture must exercise the transport boundary');

// Optional existing capture: read only; never embeds private network identities
// into a tracked fixture. This projection mirrors the guarded serializers above.
if(process.argv[2]) {
  const capture=JSON.parse(readFileSync(process.argv[2],'utf8').replace(/^\uFEFF/,''));
  const value=structuredClone(capture.snapshot);
  const before=size(value);
  for(const field of ['uplink_route_changes','uplink_resolver_changes','ble','state_persistence',
    'ws_queue_rejected','ws_queue_ready','local_ota_receipt','shell','mqtt_connection','cloud_io',
    'mqtt_messages_received','cloud_websocket_messages_received','uart_bytes_received',
    'uart_frames_received','usb_serial_bytes_received','usb_serial_frames_received'])delete value.status[field];
  if(value.status.ethernet)delete value.status.ethernet.lldp;
  for(const field of ['timing_profile','requested_t0h_ns','requested_t1h_ns','requested_bit_ns',
    'latch_us','pixels','minimum_frame_us','maximum_wire_fps_milli','dma_ring_symbols',
    'frame_timing','signal_capture','refill_cache_safe','refill_irq','done_symbols','expected_symbols'])
    delete value.status.led_output.driver[field];
  const statusBytes=size(value.status),after=size(value);
  assert(statusBytes<=10240,`Captured compact status is still too large: ${statusBytes}`);
  value.names=names;
  const worstNames=size(value);
  assert(worstNames<=16384,`Captured state with maximum names exceeds cap: ${worstNames}`);
  console.log(JSON.stringify({evidence:'read-only captured-state projection, not a firmware runtime test',before,after,statusBytes,worstNames}));
}
console.log('USB compact projection source guards and worst escaped-name byte budget passed');
