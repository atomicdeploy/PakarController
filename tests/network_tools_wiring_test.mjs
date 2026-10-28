// Integration/source invariants, not device or Internet acceptance. The date
// policy/settings fixtures execute actual C++; the SDK build checks these APIs.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = p => readFileSync(new URL('../' + p, import.meta.url), 'utf8');
const app = read('firmware/esp/application.ino');
const binding = read('firmware/esp/src/NetworkToolsBinding.h');
const worker = read('firmware/esp/src/NetworkTools.h');
const usb = read('firmware/esp/src/UsbApiService.h');
const ntp = app.slice(app.indexOf('void configureNtp() {'), app.indexOf('bool applyStreamPacket('));
assert(ntp.indexOf('esp_netif_init()') < ntp.indexOf('esp_sntp_stop()'));
assert(ntp.includes('configTime(0, 0, ntpServerName)'));
assert(app.includes('char ntpServerName[254]'));
const setup = app.slice(app.indexOf('void setup() {'));
assert(setup.indexOf('networkToolsSettings.begin(') < setup.indexOf('configureNtp();'));
assert(setup.indexOf('configureNtp();') < setup.indexOf('networkConfig.configure('));
assert(!app.includes('WiFi.scanNetworks(false'));
assert(worker.includes('startPending()') && worker.includes('HTTPC_DISABLE_FOLLOW_REDIRECTS'));
assert(worker.includes('useBuiltinCACertBundle()') && !worker.includes('setInsecure'));
assert(worker.includes('NetworkClientSecure::connect(ip_,port,host,'));
assert(worker.includes('deadline_(millis()+9000U)'));
assert(worker.includes('length>64') && worker.includes('memchr(bytes,0,length)'));
for (const name of ['network-tools', 'configure-network-tools', 'network-probe', 'set-clock', 'wifi-scan-results']) {
  assert(app.includes('command=="' + name + '"'), name);
}
for (const route of ['/api/time', '/api/network/tools', '/api/network/probe', '/api/wifi/scan', '/api/network']) {
  assert(usb.includes('path=="' + route + '"'), route);
}
assert(binding.includes('publishPcEvent(body)') && binding.includes('cloudServices.requestTelemetry()'));
assert(read('firmware/esp/src/MaintenanceRoutes.h').includes('add("network_tools",networkToolsJson())'));
assert(read('mobile/lib/peer_context.dart').includes("'network-tools'"));
assert(read('mobile/lib/main.dart').includes("'Advanced…'"));
assert(!read('mobile/lib/main.dart').includes('Advanced MQTT / bridge'));
assert(read('web/index.html').includes('/api/wifi/scan?results=true'));
console.log('Network tools wiring: first-boot TCP/IP/SNTP order, stable names, async scans, verified bounded TLS, shared USB/cloud routes and Advanced capability gates passed; not live network proof.');
