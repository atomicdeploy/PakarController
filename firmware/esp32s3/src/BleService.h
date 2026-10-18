#pragma once
#include <stddef.h>
#include <stdint.h>

#if defined(ARDUINO_ARCH_ESP32) && __has_include(<build/config/esp32s3/LedBackend.h>)
// Shared, ignored application-only build policy; no SDK-wide flag/cache churn.
#include <build/config/esp32s3/LedBackend.h>
#endif

#ifndef PAKAR_S3_BLE_ENABLED
#define PAKAR_S3_BLE_ENABLED 1
#endif
#if PAKAR_S3_BLE_ENABLED != 0 && PAKAR_S3_BLE_ENABLED != 1
#error "PAKAR_S3_BLE_ENABLED must be 0 or 1"
#endif

namespace PakarS3 {
// Pure boot policy: a saved request is not proof that the SDK/radio is running.
// Keep it outside the ESP32 includes so host fixtures exercise this real owner.
class BleLifecyclePolicy {
 public:
  explicit constexpr BleLifecyclePolicy(bool available=PAKAR_S3_BLE_ENABLED):available_(available) {}
  bool restore(const uint8_t *bytes,size_t length,bool exists) {
    requested_=false;
    if(!exists)return true; // Missing settings are deliberately Wi-Fi-first.
    if(!bytes||length!=4||bytes[0]!='B'||bytes[1]!='L'||bytes[2]>1||
        bytes[3]!=static_cast<uint8_t>(0xa5^bytes[2]))return false;
    requested_=bytes[2]!=0;return true;
  }
  static void encode(bool enabled,uint8_t *bytes) {
    bytes[0]='B';bytes[1]='L';bytes[2]=static_cast<uint8_t>(enabled);
    bytes[3]=static_cast<uint8_t>(0xa5^bytes[2]);
  }
  void request(bool enabled) {requested_=enabled;}
  bool canRequest(bool enabled) const {return !enabled||available_;}
  bool requested() const {return requested_;}
  bool available() const {return available_;}
  bool mayStartAtBoot() const {return available_&&requested_&&!attempted_;}
  void startupAttempted() {attempted_=true;}
  bool attempted() const {return attempted_;}
  bool buildRequired() const {return requested_&&!available_;}
  bool rebootRequired(bool started) const {
    // Disabling advertising does not release SDK pools/tasks. Do not claim
    // memory recovery until a subsequent boot actually omits initialization.
    return available_&&(requested_?!started:attempted_);
  }
 private:
  bool available_,requested_=false,attempted_=false;
};
} // namespace PakarS3

#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <BleWire.h>
#include "BleStorage.h"
#include <BLEDevice.h>
#if defined(CONFIG_BLUEDROID_ENABLED)
#include <BLE2902.h>
#else
#include <services/gap/ble_svc_gap.h>
#include <host/ble_hs_mbuf.h>
#endif
#include <atomic>
#include <mutex>
#include <new>

namespace PakarS3 {
class BleService {
 public:
  static constexpr const char *serviceUuid="a4b68d10-5c3f-4c5a-9f12-50616b617243";
  static constexpr const char *rxUuid="a4b68d11-5c3f-4c5a-9f12-50616b617243";
  static constexpr const char *txUuid="a4b68d12-5c3f-4c5a-9f12-50616b617243";
  static constexpr const char *infoUuid="a4b68d13-5c3f-4c5a-9f12-50616b617243";
  using Handler=String (*)(const String &,uint32_t);
  void begin(fs::FS &fs,bool mounted,const String &name,Handler handler) {
    fs_=&fs;mounted_=mounted;handler_=handler;lifecycle_.restore(nullptr,0,false);
    if(mounted && fs.exists(configPath)) {
      File file=fs.open(configPath,"r");uint8_t bytes[4]{};
      const bool complete=file && file.size()==4 && file.read(bytes,4)==4;
      if(file)file.close();
      if(!lifecycle_.restore(complete?bytes:nullptr,complete?sizeof(bytes):0,true))
        error_="Invalid BLE configuration; disabled";
    }
    name_=name;
    if(lifecycle_.buildRequired())error_="BLE is excluded from this build; saved preference retained";
    // SDK init waits for host synchronization. It is permitted only here at
    // boot, never from a runtime settings action/main-loop service pass.
    if(lifecycle_.mayStartAtBoot())start();
  }
  // Authentication is deliberately not transported over an unencrypted GATT
  // channel. An enabled/damaged AccessConfig closes admission before ingress;
  // public INFO and the auth-info RPC expose only the fail-closed explanation.
  void authorization(bool allowed) {
    const bool before=allowed_.exchange(allowed,std::memory_order_acq_rel);
    if(before&&!allowed)fence(true);
  }
  bool configure(bool enabled,String &error) {
    if(!mounted_||!fs_){error="BLE configuration unavailable";return false;}
    if(!lifecycle_.canRequest(enabled)){error="BLE is excluded from this build; cannot enable";return false;}
    uint8_t bytes[4];BleLifecyclePolicy::encode(enabled,bytes);
    File pending=fs_->open(pendingPath,"w");
    bool ok=pending && pending.write(bytes,sizeof(bytes))==sizeof(bytes);if(pending){pending.flush();pending.close();}
    uint8_t readback[4]{};File check=fs_->open(pendingPath,"r");
    ok=ok && check && check.size()==sizeof(bytes) && check.read(readback,sizeof(readback))==sizeof(readback) && !memcmp(bytes,readback,sizeof(bytes));
    if(check)check.close();
    if(!ok || !fs_->rename(pendingPath,configPath)){error="BLE configuration verification failed";return false;}
    lifecycle_.request(enabled);applyAt_=millis()+250U;applyPending_=true;
    error_=lifecycle_.buildRequired()?String("BLE is excluded from this build; saved preference retained"):String();return true;
  }
  void setInfo(const String &info) {
    // A readable GATT attribute is at most512bytes, regardless of framed limit.
    if(info.length()>512){error_="BLE INFO exceeds GATT attribute limit";return;}
    if(info_==info)return;info_=info;
    // The installed SDK's read mux is NOT its setValue semaphore. Never mutate
    // an SDK String concurrently with host reads. Only onRead publishes these
    // pre-serialized bounded bytes, on the same BLE host actor as the read.
    std::lock_guard<std::mutex> guard(infoMutex_);infoLength_=info.length();memcpy(infoBytes_,info.c_str(),infoLength_);
  }
  bool connected() const { return owner_.load(std::memory_order_acquire)!=noOwner; }
  bool started() const { return started_; }
  uint16_t mtu() const {const uint16_t peer=owner_.load();return server_&&peer!=noOwner?server_->getPeerMTU(peer):23;}
  bool enabled() const { return lifecycle_.available()&&lifecycle_.requested(); }
  bool requestedEnabled() const {return lifecycle_.requested();}
  bool rebootRequired() const {return lifecycle_.rebootRequired(started_);}
  bool buildRequired() const {return lifecycle_.buildRequired();}
  bool startupAttempted() const {return lifecycle_.attempted();}
  bool applyPending() const { return applyPending_; }
  uint32_t epoch() const { return epoch_.load(std::memory_order_acquire); }
  bool owns(uint32_t epoch) const { return bulk_ && started_ && connected() && !retiring_.load(std::memory_order_acquire) &&
    enabled() && epoch==this->epoch() && allowed_.load(std::memory_order_acquire); }
  const String &error() const { return error_; }
  uint32_t rejected() const { return rejected_.load(std::memory_order_relaxed); }
  uint32_t eventsLost() const { return eventsLost_; }
  uint8_t eventsPending() const { return eventCount_; }
  bool publish(const String &body) {
    if(!bulk_||!started_||!connected()||!bootstrapped_||!allowed_.load()||retiring_.load())return false;
    if(body.isEmpty()||body.length()>2048||eventCount_==32){
      ++eventsLost_;error_="BLE ordered event loss; mirror resynchronization required";fence(true);return false;
    }
    auto &event=events_[(eventHead_+eventCount_)%32];event.body=body;event.at=millis();
    if(event.body.length()!=body.length()){
      ++eventsLost_;error_="BLE event memory unavailable; mirror resynchronization required";fence(true);return false;
    }
    ++eventCount_;return true;
  }
  bool canPush(uint32_t now) const {
    return bulk_ && enabled() && started_ && connected() && !retiring_.load() && bootstrapped_ && allowed_.load(std::memory_order_acquire) &&
      !maintenance_ && !eventCount_ && !bulk_->sender.busy() && !bulk_->receiver.ready() && static_cast<uint32_t>(now-lastPush_)>=2000U;
  }
  bool push(const String &body,uint32_t now) {
    if(!canPush(now)||!queue(body))return false;lastPush_=now;return true;
  }
  void service(uint32_t now,bool maintenance,const String &name) {
    if(name_!=name) {name_=name;renamePending_=true;renameAt_=now;}
    if(maintenance_!=maintenance) {maintenance_=maintenance;fence(false);}
    if(observedEpoch_!=epoch()) {flush();observedEpoch_=epoch();}
    // Extra clients cannot consume/receive another owner's reply. No generic
    // notify is issued until their loop-owned disconnect has completed.
    for(auto &slot:rejectPeers_) {
      const uint16_t reject=slot.exchange(noOwner,std::memory_order_acq_rel);
      if(server_ && reject!=noOwner)server_->disconnect(reject);
    }
    const bool sending=bulk_ && bulk_->sender.busy();
    if(renamePending_ && connected() && (!sending||static_cast<uint32_t>(now-renameAt_)>=2500U))fence(true);
    if(renamePending_ && !connected()) {renamePending_=false;if(started_)advertise();}
    if(applyPending_ && static_cast<int32_t>(now-applyAt_)>=0 &&
       (!sending || static_cast<uint32_t>(now-applyAt_)>=2500U)) {
      applyPending_=false;
      if(!enabled()) {fence(true);if(advertising_)advertising_->stop();}
      else if(started_&&!connected())advertise();
      // A newly enabled but unstarted stack remains pending a reboot. Calling
      // BLEDevice::init here could indefinitely block HTTP and USB recovery.
    }
    // Disabling keeps its bounded reply/fragment ACK path alive until complete
    // or the explicit2500ms teardown deadline; it cannot admit another action.
    if(!enabled()&&!applyPending_) {
      // A GAP connect already in flight when advertising stopped can arrive
      // after teardown. Retire that late owner on the next loop pass as well.
      if(connected()&&!retiring_.load(std::memory_order_acquire))fence(true);
      return;
    }
    if(!started_ || !bulk_)return;
    if(restartAdvertising_.exchange(false,std::memory_order_acq_rel) && !connected() && !renamePending_)advertise();
    bulk_->receiver.expire(now);
    Ingress item;
    // A bounded per-loop slice prevents a large phone write burst monopolizing
    // the renderer/UART/network loop. ATT write-response is never command ACK.
    for(unsigned n=0;n<4 && pop(item);++n) {
      if(item.epoch!=epoch() || item.peer!=owner_.load(std::memory_order_acquire))continue;
      BoardCore::BleWire::Frame frame;
      if(!BoardCore::BleWire::parse(item.bytes,item.length,frame)){++rejected_;continue;}
      if(frame.kind==BoardCore::BleWire::Ack) {bulk_->sender.acknowledge(item.bytes,item.length);continue;}
      uint8_t ack[BoardCore::BleWire::kHeader];
      const auto result=bulk_->receiver.accept(item.bytes,item.length,now,ack);
      if(result==BoardCore::BleWire::Receiver::Rejected){++rejected_;continue;}
      notify(ack,sizeof(ack));
    }
    // Completion deliberately remains loop-owned, after all callback buffers
    // have been copied and the owner/deadline will be checked by the adapter.
    if(bulk_->receiver.ready() && !bulk_->sender.busy() && !hasRejectedPeer() && !retiring_.load()) {
      const uint32_t ownerEpoch=epoch();
      String request;const size_t size=bulk_->receiver.size();
      if(request.reserve(size+1))request.concat(reinterpret_cast<const char*>(bulk_->receiver.data()),size);
      bulk_->receiver.consumed();
      if(request.length()!=size){error_="BLE request memory unavailable";fence(true);return;}
      String reply=handler_?handler_(request,ownerEpoch):String();
      if(connected() && epoch()==ownerEpoch && reply.length()) {bootstrapped_=true;queue(reply);}
    }
    if(!bulk_->sender.busy())response_=String();
    // Commands are admitted before queued unsolicited events. Events retain
    // order. Any gap (including a missing tone-stop) retires the stream so its
    // client immediately stops/resynchronizes mirroring; never silently skip a
    // note and continue. A slow minimum-MTU snapshot is not exact audio proof.
    while(!bulk_->sender.busy()&&eventCount_) {
      auto &event=events_[eventHead_];
      if(static_cast<uint32_t>(now-event.at)>1500U){error_="BLE ordered event expired; mirror resynchronization required";fence(true);break;}
      if(!queue(event.body)){error_="BLE ordered event publication failed";fence(true);break;}
      event.body=String();eventHead_=(eventHead_+1)%32;--eventCount_;
    }
    if(connected() && !retiring_.load() && !hasRejectedPeer() && peerCount_.load(std::memory_order_acquire)==1) {
      size_t length=0;const uint16_t peer=owner_.load();
      const uint16_t mtu=server_?server_->getPeerMTU(peer):23;
      if(const auto *packet=bulk_->sender.poll(now,mtu,length))notify(packet,length);
      if(bulk_->sender.failed()){error_="BLE fragment acknowledgement timed out; outcome unknown";fence(true);}
    }
  }
 private:
  static constexpr uint16_t noOwner=0xffff;
  static constexpr const char *configPath="/ble.cfg",*pendingPath="/ble.pending";
  using Ingress=BleIngress;
  class ServerCallbacks final : public BLEServerCallbacks {
   public: explicit ServerCallbacks(BleService &owner):owner_(owner){}
#if defined(CONFIG_NIMBLE_ENABLED)
    void onConnect(BLEServer *,ble_gap_conn_desc *desc) override {owner_.connectedCallback(desc->conn_handle);}
    void onDisconnect(BLEServer *,ble_gap_conn_desc *desc) override {owner_.disconnectedCallback(desc->conn_handle);}
#else
    void onConnect(BLEServer *,esp_ble_gatts_cb_param_t *p) override {owner_.connectedCallback(p->connect.conn_id);}
    void onDisconnect(BLEServer *,esp_ble_gatts_cb_param_t *p) override {owner_.disconnectedCallback(p->disconnect.conn_id);}
#endif
   private: BleService &owner_;
  } serverCallbacks_{*this};
  class RxCallbacks final : public BLECharacteristicCallbacks {
   public: explicit RxCallbacks(BleService &owner):owner_(owner){}
#if defined(CONFIG_NIMBLE_ENABLED)
    void onWrite(BLECharacteristic *characteristic,ble_gap_conn_desc *desc) override {
      owner_.ingress(desc->conn_handle,characteristic->getData(),characteristic->getLength());
    }
    void onSubscribe(BLECharacteristic *,ble_gap_conn_desc *desc,uint16_t value) override {
      if(desc->conn_handle==owner_.owner_.load())owner_.subscribed_.store((value&1U)!=0,std::memory_order_release);
    }
#else
    void onWrite(BLECharacteristic *characteristic,esp_ble_gatts_cb_param_t *p) override {
      owner_.ingress(p->write.conn_id,characteristic->getData(),characteristic->getLength());
    }
#endif
   private: BleService &owner_;
  } rxCallbacks_{*this};
  class InfoCallbacks final : public BLECharacteristicCallbacks {
   public: explicit InfoCallbacks(BleService &owner):owner_(owner){}
    void onRead(BLECharacteristic *characteristic) override {
      uint8_t bytes[512];uint16_t length=0;
      {std::lock_guard<std::mutex> guard(owner_.infoMutex_);length=owner_.infoLength_;memcpy(bytes,owner_.infoBytes_,length);}
      // Byte publication only: no JSON, filesystem, notification or hardware.
      // This is the sole SDK value writer after service start, on its host actor.
      characteristic->setValue(bytes,length);
    }
   private: BleService &owner_;
  } infoCallbacks_{*this};
  void connectedCallback(uint16_t peer) {
    ++peerCount_;uint16_t expected=noOwner;
    if(!owner_.compare_exchange_strong(expected,peer,std::memory_order_acq_rel)) {
      for(auto &slot:rejectPeers_) {
        uint16_t empty=noOwner;if(slot.compare_exchange_strong(empty,peer,std::memory_order_acq_rel))return;
      }
      // SDK limit is three; an unexpected fourth connection fails closed.
      retiring_.store(true,std::memory_order_release);return;
    }
    subscribed_.store(false,std::memory_order_release);retiring_.store(false,std::memory_order_release);++epoch_;
  }
  void disconnectedCallback(uint16_t peer) {
    const uint8_t count=peerCount_.load();if(count) --peerCount_;
    uint16_t expected=peer;
    if(owner_.compare_exchange_strong(expected,noOwner,std::memory_order_acq_rel)) {subscribed_.store(false);++epoch_;restartAdvertising_.store(true);}
  }
  void ingress(uint16_t peer,const uint8_t *bytes,size_t size) {
    // No JSON, FS, notification, wait, or peripheral operation in this callback.
    // Auth-enabled devices intentionally accept no RPC ingress at all.
    if(!bulk_||!allowed_.load(std::memory_order_acquire)||retiring_.load(std::memory_order_acquire)||peer!=owner_.load()||!bytes||!size||size>512) {++rejected_;return;}
    std::lock_guard<std::mutex> guard(mutex_);
    if(count_==8){++rejected_;return;}
    auto &slot=bulk_->ingress[(head_+count_)%8];slot.peer=peer;slot.epoch=epoch();slot.length=size;
    memcpy(slot.bytes,bytes,size);++count_;
  }
  bool pop(Ingress &item) {
    std::lock_guard<std::mutex> guard(mutex_);if(!bulk_||!count_)return false;
    item=bulk_->ingress[head_];head_=(head_+1)%8;--count_;return true;
  }
  void flush() {
    {std::lock_guard<std::mutex> guard(mutex_);head_=count_=0;}
    if(bulk_){bulk_->receiver.reset();bulk_->sender.reset();}
    response_=String();bootstrapped_=false;
    eventsLost_+=eventCount_;for(auto &event:events_)event.body=String();eventHead_=eventCount_=0;
  }
  void fence(bool disconnect) {
    if(disconnect)retiring_.store(true,std::memory_order_release);
    ++epoch_;flush();observedEpoch_=epoch();
    if(disconnect && server_) {const auto peer=owner_.load();if(peer!=noOwner)server_->disconnect(peer);}
  }
  bool queue(const String &body) {
    if(!bulk_||bulk_->sender.busy()||body.isEmpty()||body.length()>65535)return false;
    response_=body;if(response_.length()!=body.length()){error_="BLE response memory unavailable";return false;}
    if(!++nextMessage_)++nextMessage_;
    return bulk_->sender.begin(nextMessage_,reinterpret_cast<const uint8_t*>(response_.c_str()),response_.length());
  }
  void notify(const uint8_t *bytes,size_t length) {
    if(!tx_||!connected()||retiring_.load()||!allowed_.load()||peerCount_.load(std::memory_order_acquire)!=1||hasRejectedPeer())return;
    const uint16_t peer=owner_.load(std::memory_order_acquire);const uint32_t generation=epoch();
    // Arduino3.3.12 BLECharacteristic::notify iterates ALL subscribed clients.
    // Use its actual underlying host API for only the captured owner, so a
    // concurrent extra connection can never observe another client's payload.
#if defined(CONFIG_NIMBLE_ENABLED)
    if(!subscribed_.load(std::memory_order_acquire))return;
    auto *packet=ble_hs_mbuf_from_flat(bytes,length);
    if(!packet){error_="BLE notification memory unavailable";return;}
    if(epoch()!=generation||owner_.load()!=peer||retiring_.load()||!allowed_.load()){os_mbuf_free_chain(packet);return;}
    if(ble_gatts_notify_custom(peer,tx_->getHandle(),packet)!=0)error_="BLE owner notification not admitted";
#else
    auto *cccd=static_cast<BLE2902*>(tx_->getDescriptorByUUID(BLEUUID(static_cast<uint16_t>(0x2902))));
    if(!cccd||!cccd->getNotifications())return;
    if(epoch()!=generation||owner_.load()!=peer||retiring_.load()||!allowed_.load())return;
    if(esp_ble_gatts_send_indicate(server_->getGattsIf(),peer,tx_->getHandle(),length,const_cast<uint8_t*>(bytes),false)!=ESP_OK)
      error_="BLE owner notification not admitted";
#endif
  }
  bool hasRejectedPeer() const {for(const auto &slot:rejectPeers_)if(slot.load()!=noOwner)return true;return false;}
  void advertise() {
    if(!advertising_||!enabled()||connected())return;
    advertising_->stop();BLEAdvertisementData data,scan;
#if defined(CONFIG_NIMBLE_ENABLED)
    ble_svc_gap_device_name_set(name_.c_str());
#else
    esp_ble_gap_set_device_name(name_.c_str());
#endif
    data.setFlags(0x06);data.setCompleteServices(BLEUUID(serviceUuid));
    scan.setName(name_.substring(0,29));
    if(!advertising_->setAdvertisementData(data)||!advertising_->setScanResponseData(scan)||!advertising_->start())
      error_="BLE advertisement failed";
  }
  void start() {
#if PAKAR_S3_BLE_ENABLED
    if(started_||!lifecycle_.mayStartAtBoot())return;
    // App buffers must not compete with the SDK's internal-only BLE pools and
    // Wi-Fi/OTA heap. Allocate before creating any host task or callback owner.
    if(!bulk_.begin()){error_="BLE PSRAM storage unavailable; internal RAM retained for Wi-Fi/OTA";return;}
    lifecycle_.startupAttempted();
    if(!BLEDevice::init(name_)){error_="BLE stack initialization failed";return;}
    if(BLEDevice::setMTU(247)!=ESP_OK)error_="Preferred BLE MTU unavailable; negotiated MTU remains authoritative";
    server_=BLEDevice::createServer();
    if(!server_){error_="BLE server allocation failed";return;}
    server_->setCallbacks(&serverCallbacks_);
#if !defined(CONFIG_BT_NIMBLE_EXT_ADV) || defined(CONFIG_BLUEDROID_ENABLED)
    server_->advertiseOnDisconnect(false);
#endif
    auto *service=server_->createService(serviceUuid);
    if(!service){error_="BLE service allocation failed";return;}
    rx_=service->createCharacteristic(rxUuid,BLECharacteristic::PROPERTY_WRITE);
    tx_=service->createCharacteristic(txUuid,BLECharacteristic::PROPERTY_NOTIFY);
    infoChar_=service->createCharacteristic(infoUuid,BLECharacteristic::PROPERTY_READ);
    if(!rx_||!tx_||!infoChar_){error_="BLE characteristic allocation failed";return;}
    rx_->setCallbacks(&rxCallbacks_);tx_->setCallbacks(&rxCallbacks_);infoChar_->setCallbacks(&infoCallbacks_);
#if defined(CONFIG_BLUEDROID_ENABLED)
    auto *cccd=new(std::nothrow) BLE2902();if(!cccd){error_="BLE CCCD allocation failed";return;}tx_->addDescriptor(cccd);
#endif
    if(info_.length())infoChar_->setValue(info_); // Before GATT start, no reader.
    if(!service->start()){error_="BLE service failed to start";return;}server_->start();
    if(!server_->isStarted()){error_="BLE GATT server failed to start";return;}
    advertising_=BLEDevice::getAdvertising();
    if(!advertising_){error_="BLE advertiser unavailable";return;}
    started_=true;advertise();
#endif
  }
  fs::FS *fs_=nullptr;Handler handler_=nullptr;
  BLEServer *server_=nullptr;BLECharacteristic *rx_=nullptr,*tx_=nullptr,*infoChar_=nullptr;BLEAdvertising *advertising_=nullptr;
  std::atomic<uint16_t> owner_{noOwner};std::atomic<uint16_t> rejectPeers_[3]{{noOwner},{noOwner},{noOwner}};
  std::atomic<uint32_t> epoch_{1},rejected_{0};
  std::atomic<uint8_t> peerCount_{0};std::atomic<bool> allowed_{false},restartAdvertising_{false},retiring_{false},subscribed_{false};
  std::mutex mutex_;BleBulkOwner bulk_;uint8_t head_=0,count_=0;
  std::mutex infoMutex_;uint8_t infoBytes_[512]{};uint16_t infoLength_=0;
  struct Event {String body;uint32_t at=0;};Event events_[32];
  uint8_t eventHead_=0,eventCount_=0;uint32_t eventsLost_=0;
  String name_,info_,response_,error_;uint32_t observedEpoch_=1,lastPush_=0,applyAt_=0,renameAt_=0;uint16_t nextMessage_=0;
  BleLifecyclePolicy lifecycle_;
  bool mounted_=false,started_=false,maintenance_=false,bootstrapped_=false,applyPending_=false,renamePending_=false;
};
} // namespace PakarS3
#endif
