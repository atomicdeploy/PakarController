import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = file => readFileSync(new URL(`../${file}`, import.meta.url), 'utf8');
const service=read('firmware/esp32s3/src/BleService.h');
const routes=read('firmware/esp/src/BleRoutes.h');
const main=read('firmware/esp/application.ino');
const urboot=read('firmware/esp/src/UrbootRoutes.h');
const wire=read('libraries/BoardCore/src/BleWire.h');
const storage=read('firmware/esp32s3/src/BleStorage.h');

for(const suffix of ['10','11','12','13'])assert(service.includes(`a4b68d${suffix}-5c3f-4c5a-9f12-50616b617243`));
assert(service.includes('BLECharacteristic::PROPERTY_WRITE)'));
assert(!service.includes('PROPERTY_WRITE_NR'));
assert(service.includes('#if defined(CONFIG_NIMBLE_ENABLED)'));
assert(service.includes('desc->conn_handle'));
assert(service.includes('server_->advertiseOnDisconnect(false)'));
assert(service.includes('rejectPeers_[3]'));
assert(service.includes('peerCount_.load(std::memory_order_acquire)!=1'));
assert(service.includes('retiring_.store(true,std::memory_order_release)'));
assert(service.includes('ble_gatts_notify_custom(peer,tx_->getHandle(),packet)'));
assert(!service.includes('tx_->notify()'));
assert(service.includes('if(before&&!allowed)fence(true)'));
assert(service.includes('!allowed_.load()'));
assert(service.includes('if(info.length()>512)'));
assert(service.includes('setScanResponseData(scan)'));
assert(service.includes('name_.substring(0,29)'));
assert(service.includes('pending.write(bytes,sizeof(bytes))'));
assert(service.includes('memcmp(bytes,readback,sizeof(bytes))'));
assert(service.includes('fs_->rename(pendingPath,configPath)'));
assert(service.includes('lifecycle_.restore(nullptr,0,false)'));
assert(service.includes('if(lifecycle_.mayStartAtBoot())start()'));
const runtime=service.slice(service.indexOf('void service(uint32_t'),service.indexOf(' private:',service.indexOf('void service(uint32_t')));
assert(!runtime.includes('start();'),'Runtime BLE setting must never enter blocking SDK initialization');
assert(runtime.includes('if(connected()&&!retiring_.load(std::memory_order_acquire))fence(true)'));
assert(!service.includes('desired_=mounted && PAKAR_S3_BLE_ENABLED'));
for(const observed of ['requested_enabled','reboot_required','build_required','startup_attempted'])assert(routes.includes(`d["${observed}"]`));
assert(service.includes('if(!lifecycle_.canRequest(enabled))'));
assert(service.includes('return lifecycle_.available()&&lifecycle_.requested()'));
assert(service.includes('(!sending || static_cast<uint32_t>(now-applyAt_)>=2500U)'));
assert(storage.includes('heap_caps_malloc(sizeof(BleBulkStorage),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)'));
assert(storage.includes('BleIngress ingress[8]'));
assert(storage.includes('BoardCore::BleWire::Receiver receiver;'));
assert(storage.includes('BoardCore::BleWire::Sender sender;'));
assert(!service.includes('Ingress ingress_[8]'));
assert(!service.includes('BoardCore::BleWire::Receiver receiver_;'));
assert(service.indexOf('if(!bulk_.begin())')<service.indexOf('if(!BLEDevice::init(name_)'));
assert(service.includes('!started_ || !bulk_)return'));
assert(service.includes('if(!bulk_||!allowed_.load'));
assert(service.includes('if(bulk_){bulk_->receiver.reset();bulk_->sender.reset();}'));
// Inspect callback bodies only: no parser/FS/notification/device dispatch/wait.
const callbacks=service.slice(service.indexOf('class ServerCallbacks'),service.indexOf('bool pop('));
for(const forbidden of ['deserializeJson','serializeJson','LittleFS','notify(', 'handler_(', 'delay(', 'yield(', 'sendToAvr'])
  assert(!callbacks.includes(forbidden),`BLE callback performs ${forbidden}`);
assert(callbacks.includes('memcpy(slot.bytes,bytes,size)'));
assert(callbacks.includes('if(count_==8){++rejected_;return;}'));
assert(service.includes('handler_(request,ownerEpoch)'));
assert(service.indexOf('bulk_->receiver.ready() && !bulk_->sender.busy()')<service.indexOf('handler_(request,ownerEpoch)'));
for(const route of ['GET /api/status','GET /api/table/snapshot','GET /api/cells/mapping','GET /api/ble/clock',
  'POST /api/command','POST /api/cells','POST /api/avr/contracts','POST /api/firmware/fetch',
  'POST /api/firmware/apply','POST /api/firmware/cancel','POST /api/network','POST /api/cloud','POST /api/ble'])assert(routes.includes(`"${route}"`));
for(const forbidden of ['AsyncWebServerRequest','HTTPClient','localhost','127.0.0.1','WiFi.begin','grid.setCell(','grid.diagnostic('])
  assert(!routes.includes(forbidden),`BLE bypasses shared services: ${forbidden}`);
assert(routes.includes('commandReply(form["packet"].as<String>())'));
assert(routes.includes('handleCloudCommand(packet,result)'));
assert(routes.includes('updateCloudConfiguration(d.as<JsonObjectConst>(),result,false)'));
assert(routes.includes('remoteCommand(d,result)'));
assert(routes.includes('commandReceipts.reserve'));
assert(routes.includes('hash.add(lease.c_str(),lease.length())'));
assert(routes.includes('const String lease=String(epoch)+":"+pcBootId'));
assert(routes.includes('request["expires_at_ms"].as<uint64_t>()-now>8000'));
assert(routes.includes('commandClock()>=request["expires_at_ms"].as<uint64_t>()'));
assert(routes.includes('PakarCommand::stringValue(request,"board_id")!=pcBoardId'));
assert(routes.includes('PakarCommand::stringValue(request,"boot_id")!=pcBootId'));
assert(routes.includes('if(!accessConfig.ready()||accessConfig.enabled())'));
assert(routes.includes('if(!bleService.owns(epoch))'));
assert(routes.includes('if(BleRoutes::readOnly)return'));
assert(routes.includes('readback_required'));
assert(routes.includes('if(BleRoutes::servicing)return'));
for(const unavailable of ['firmware_binary','raw_stream','wifi_scan'])assert(routes.includes(`d["${unavailable}"]=false`));
assert(routes.includes('{"status","pc-contract-event","pc-contract-result","update-progress"}'));
assert(service.includes('eventCount_==32'));
assert(service.includes('static_cast<uint32_t>(now-event.at)>1500U'));
assert(routes.includes('d["event_loss_policy"]="disconnect-and-resynchronize"'));
assert(service.includes('BLE ordered event loss; mirror resynchronization required'));
assert(service.includes('std::lock_guard<std::mutex> guard(infoMutex_)'));
assert(service.includes('characteristic->setValue(bytes,length)'));
assert(!routes.includes('info["command_clock_ms"]'));
assert(routes.includes('ble-event'));
assert(!routes.includes('GET /firmware/download'));
assert(!routes.includes('POST /update'));
assert(main.includes('#include <firmware/esp32s3/src/BleService.h>'));
assert(main.includes('#include "src/BleRoutes.h"'));
assert(main.includes('bleService.begin(LittleFS,filesystemMounted,hostname,bleRequest)'));
assert(main.includes('body+=bleStatusJson()'));
// Local maintenance and correction-expiry work may precede this network guard.
// Check actual service ownership, not their former adjacency to loop().
function networkServicesGuarded(source){
  const loop=source.slice(source.indexOf('void loop() {'));
  const guards=[],seen=new Map(),calls=['serviceBle();','servicePeerRelay();'];
  for(const line of loop.split(/\r?\n/)){
    const enter=/^\s*#if\s+(.+)$/.exec(line);
    if(enter){guards.push(enter[1].trim());continue;}
    if(/^\s*#ifn?def\b/.test(line)){guards.push('unsupported');continue;}
    if(/^\s*#(?:else|elif)\b/.test(line)){guards[guards.length-1]='alternate';continue;}
    if(/^\s*#endif\b/.test(line)){guards.pop();continue;}
    for(const call of calls)if(line.includes(call)){
      seen.set(call,(seen.get(call)||0)+1);
      const both='defined(ARDUINO_ARCH_ESP32) && PAKAR_NETWORK_ENABLED';
      const s3=guards.includes('defined(ARDUINO_ARCH_ESP32)')||guards.includes(both);
      const network=guards.includes('PAKAR_NETWORK_ENABLED')||guards.includes(both);
      if(!s3||!network)return false;
    }
  }
  return calls.every(call=>seen.get(call)===1);
}
assert(networkServicesGuarded(main),'Offline builds exclude BLE and peer-relay service calls');
const loopOnly=main.slice(main.indexOf('void loop() {'));
assert(!networkServicesGuarded(loopOnly.replace('  serviceBle();','')),'Missing service remains a failure');
assert(!networkServicesGuarded(loopOnly.replace('  serviceBle();','')+'\nserviceBle();'),'A service outside the guard remains a failure');
const negatedNetwork=loopOnly.replace(
  /(#if )PAKAR_NETWORK_ENABLED(\r?\n(?:  servicePeerNetworkCommission\(\);\r?\n)?  servicePeerRelay\(\);)/,
  '$1!PAKAR_NETWORK_ENABLED$2');
assert.notEqual(negatedNetwork,loopOnly,'Negative fixture must actually change the network guard');
assert(!networkServicesGuarded(negatedNetwork),'Negated nested guard must fail');
assert(!networkServicesGuarded(negatedNetwork.replaceAll('\r\n','\n')),'LF negative fixture must fail');
assert(!networkServicesGuarded(negatedNetwork.replaceAll('\r\n','\n').replaceAll('\n','\r\n')),'CRLF negative fixture must fail');
assert(urboot.includes('serviceBleReadOnly();'));
assert(wire.includes('packetLimit(mtu)-kHeader'));
assert(wire.includes('static_cast<uint32_t>(now-started_)>=60000U'));
console.log('BLE firmware integration source guards passed (not SDK/RF/device proof)');
