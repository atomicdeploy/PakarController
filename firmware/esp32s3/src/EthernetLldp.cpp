#include "EthernetPort.h"
#if PAKAR_S3_ETHERNET_ENABLED
#include "EthernetLldp.h"
#include <esp_idf_version.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/tcpip.h>
#include <atomic>
#include <mutex>

// Reviewed Arduino3.3.12/IDF5.5.5: standard glue installs this callback before
// esp_eth_start. L2_TAP is absent in the shipping SDK, but forwarding the original
// callback also preserves it when present. GNU --wrap is application-only.
namespace {
using Input=esp_err_t(*)(esp_eth_handle_t,uint8_t*,uint32_t,void*,void*);
struct Runtime {
  std::mutex mutex;
  const void *owner=nullptr,*setupOwner=nullptr;
  TaskHandle_t setupTask=nullptr;
  esp_eth_handle_t handle=nullptr,observedHandle=nullptr;
  std::atomic<Input> forward{nullptr};
  bool setupAttempted=false,setupSuccess=false,setupPoisoned=false;
  bool enabled=true,receive=false,hooked=false,promiscuous=false,link=false,pending=false;
  bool advertised=false,attemptedTx=false,attemptedQueue=false;
  std::atomic<bool> rxActive{false};
  std::atomic<uint32_t> rxCheckedAt{0},rxDropped{0};
  uint32_t epoch=0,queuedEpoch=0,lastTx=0,lastQueue=0;
  uint32_t transmitted=0,failed=0,received=0,invalid=0,queueRejected=0;
  int error=0;
  uint8_t mac[6]{};
  char hostname[64]{};
  PakarS3::Lldp::Neighbors neighbors;
} state;
constexpr bool reviewedSdk=ESP_IDF_VERSION==ESP_IDF_VERSION_VAL(5,5,5);
bool taskContext(){return !xPortInIsrContext()&&xPortCanYield()&&xTaskGetSchedulerState()==taskSCHEDULER_RUNNING;}
void count(uint32_t &value){if(value!=UINT32_MAX)++value;}
esp_err_t observe(esp_eth_handle_t handle,uint8_t *buffer,uint32_t length,void *priv,void *info) {
  // Never allocate/free/retain an SDK buffer, change length/info, or perform IO.
  // Busy observation storage drops telemetry only; the actual network callback
  // still receives every frame once and remains its sole ownership authority.
  const auto forward=state.forward.load(std::memory_order_acquire);
  size_t offset=0;
  if(state.rxActive.load(std::memory_order_relaxed)&&PakarS3::Lldp::candidate(buffer,length,offset)) {
    const uint32_t now=millis();uint32_t previous=state.rxCheckedAt.load(std::memory_order_relaxed);
    if(uint32_t(now-previous)>=100U&&state.rxCheckedAt.compare_exchange_strong(previous,now)) {
      PakarS3::Lldp::Neighbor neighbor;const bool valid=PakarS3::Lldp::parse(buffer,length,now,neighbor);
      std::unique_lock<std::mutex> lock(state.mutex,std::try_to_lock);
      if(lock.owns_lock()&&state.handle==handle&&state.link&&state.rxActive.load(std::memory_order_relaxed)) {
        if(valid){state.neighbors.observe(neighbor,now);count(state.received);}else count(state.invalid);
      } else state.rxDropped.fetch_add(1,std::memory_order_relaxed);
    } else state.rxDropped.fetch_add(1,std::memory_order_relaxed);
  }
  // A trampoline is installed only with a non-null original callback, whose
  // lifetime is the SDK glue's lifetime. It is cleared only after SDK teardown.
  return forward?forward(handle,buffer,length,priv,info):ESP_ERR_INVALID_STATE;
}
void run(void*) {
  std::lock_guard<std::mutex> lock(state.mutex);
  state.pending=false;
  if(!state.handle||state.queuedEpoch!=state.epoch)return;
  const bool receive=state.enabled&&state.receive&&state.hooked;
  if(receive!=state.promiscuous) {
    bool enabled=receive;
    const auto result=esp_eth_ioctl(state.handle,ETH_CMD_S_PROMISCUOUS,&enabled);
    state.error=result;
    if(result==ESP_OK)state.promiscuous=enabled;
  }
  state.rxActive.store(receive&&state.promiscuous,std::memory_order_release);
  const uint32_t now=millis();
  if(!state.link)return;
  const bool due=state.enabled&&(!state.attemptedTx||uint32_t(now-state.lastTx)>=30000U);
  const bool withdraw=!state.enabled&&state.advertised;
  if(!due&&!withdraw)return;
  uint8_t bytes[192];const auto length=PakarS3::Lldp::frame(bytes,sizeof(bytes),state.mac,state.hostname,withdraw?0:120);
  state.lastTx=now;state.attemptedTx=true;
  const auto result=length?esp_eth_transmit(state.handle,bytes,length):ESP_ERR_INVALID_ARG;
  state.error=result;
  if(result==ESP_OK){count(state.transmitted);state.advertised=!withdraw;}else count(state.failed);
  // Runs on the same lwIP TCP/IP thread as normal native ETH output. The
  // installed SDK omits ETH_TRANSMIT_MUTEX: loop-task raw TX would race the
  // W5500 write-pointer/SEND transaction even though each SPI operation locks.
}
}
namespace PakarS3 {
EthernetLldpSetup::EthernetLldpSetup(const void *owner):owner_(owner) {
  if(!owner||!reviewedSdk||!taskContext())return;
  const auto task=xTaskGetCurrentTaskHandle();if(!task)return;
  std::lock_guard<std::mutex> lock(state.mutex);
  if(state.setupOwner||state.owner)return;
  state.owner=state.setupOwner=owner;state.setupTask=task;task_=task;active_=true;
  state.setupAttempted=state.setupSuccess=state.setupPoisoned=false;
  state.observedHandle=nullptr;state.forward.store(nullptr);
}
EthernetLldpSetup::~EthernetLldpSetup() {
  if(!active_)return;
  std::lock_guard<std::mutex> lock(state.mutex);
  if(state.setupOwner==owner_){state.setupOwner=nullptr;state.setupTask=nullptr;}
}
bool EthernetLldpSetup::admitted(esp_eth_handle_t handle)const {
  if(!active_||!taskContext()||xTaskGetCurrentTaskHandle()!=task_)return false;
  std::lock_guard<std::mutex> lock(state.mutex);
  return state.setupOwner==owner_&&state.setupAttempted&&state.setupSuccess&&!state.setupPoisoned&&state.observedHandle==handle;
}
void attachEthernetLldp(const void *owner,esp_eth_handle_t handle,const char *hostname,bool observed) {
  std::lock_guard<std::mutex> lock(state.mutex);
  if(state.owner&&state.owner!=owner)return;
  state.owner=owner;state.handle=handle;state.hooked=observed;state.enabled=true;state.receive=false;
  state.promiscuous=state.link=state.advertised=state.attemptedTx=false;state.error=0;
  ++state.epoch;state.neighbors.clear();
  snprintf(state.hostname,sizeof(state.hostname),"%s",hostname?hostname:"");
}
void renameEthernetLldp(const void *owner,const char *hostname) {
  std::lock_guard<std::mutex> lock(state.mutex);if(state.owner!=owner)return;
  if(!strcmp(state.hostname,hostname?hostname:""))return;
  snprintf(state.hostname,sizeof(state.hostname),"%s",hostname?hostname:"");state.attemptedTx=false;
}
void detachEthernetLldp(const void *owner) {
  std::lock_guard<std::mutex> lock(state.mutex);if(state.owner!=owner)return;
  state.rxActive.store(false,std::memory_order_release);state.handle=nullptr;++state.epoch;
  state.neighbors.clear();state.link=false;
  // A queued TCP/IP job retains only this permanent static Runtime. Its epoch
  // cancels it before any new driver's handle may occupy the same address.
}
void releaseEthernetLldp(const void *owner) {
  std::lock_guard<std::mutex> lock(state.mutex);if(state.owner!=owner)return;
  state.owner=nullptr;state.hooked=false;state.promiscuous=false;state.forward.store(nullptr);
}
bool configureEthernetLldp(const void *owner,bool enabled,bool receive) {
  std::lock_guard<std::mutex> lock(state.mutex);if(state.owner!=owner||!state.handle)return false;
  if(enabled&&(!reviewedSdk||(receive&&!state.hooked))){state.error=ESP_ERR_NOT_SUPPORTED;return false;}
  if(state.enabled!=enabled)state.attemptedTx=false;
  state.enabled=enabled;state.receive=receive;
  if(!enabled||!receive){state.rxActive.store(false);state.neighbors.clear();}
  return true; // Admission, not physical TX or completed promiscuous-mode IO.
}
void serviceEthernetLldp(const void *owner,bool link,const uint8_t mac[6],uint32_t now) {
  // The network job may be inside bounded SPI IO. Never make the LED loop wait
  // for that lifetime fence: an unchanged pending observation can wait one loop.
  std::unique_lock<std::mutex> lock(state.mutex,std::try_to_lock);
  if(!lock.owns_lock()||state.owner!=owner||!state.handle||!reviewedSdk)return;
  if(state.link!=link){state.neighbors.clear();state.attemptedTx=false;state.link=link;}
  if(mac)memcpy(state.mac,mac,6);
  state.neighbors.expire(now);
  const bool receive=state.enabled&&state.receive&&state.hooked;
  const bool due=link&&((state.enabled&&(!state.attemptedTx||uint32_t(now-state.lastTx)>=30000U))||(!state.enabled&&state.advertised));
  if(state.pending||(!due&&receive==state.promiscuous)||
      (state.attemptedQueue&&uint32_t(now-state.lastQueue)<1000U))return;
  state.attemptedQueue=true;state.lastQueue=now;state.pending=true;state.queuedEpoch=state.epoch;
  if(tcpip_try_callback(run,nullptr)!=ERR_OK){state.pending=false;count(state.queueRejected);}
}
void snapshotEthernetLldp(const void *owner,LldpStatus &out,uint32_t now) {
  out=LldpStatus{};std::lock_guard<std::mutex> lock(state.mutex);
  if(state.owner!=owner)return;
  out.enabled=state.enabled;out.receiveRequested=state.receive;out.receiveSupported=state.hooked;
  out.receiveActive=state.link&&state.rxActive.load();out.promiscuous=state.promiscuous;
  out.transmitSupported=reviewedSdk;out.jobPending=state.pending;
  out.transmitted=state.transmitted;out.transmitFailed=state.failed;out.received=state.received;
  out.invalid=state.invalid;out.dropped=state.rxDropped.load();out.queueRejected=state.queueRejected;
  out.lastError=state.error;out.neighborCount=state.neighbors.copy(out.neighbors,now);
}
}
extern "C" esp_err_t __real_esp_eth_update_input_path_info(esp_eth_handle_t,Input,void*);
extern "C" esp_err_t __wrap_esp_eth_update_input_path_info(esp_eth_handle_t handle,Input input,void *priv) {
  bool capture=false;
  if(reviewedSdk&&handle&&input&&priv&&taskContext()) {
    std::lock_guard<std::mutex> lock(state.mutex);
    if(state.setupOwner&&state.setupTask==xTaskGetCurrentTaskHandle()) {
      if(state.setupAttempted){state.setupPoisoned=true;}
      else {state.setupAttempted=true;state.observedHandle=handle;state.forward.store(input,std::memory_order_release);capture=true;}
    }
  }
  // Other tasks, handles and later registrations retain exact SDK semantics.
  const auto result=__real_esp_eth_update_input_path_info(handle,capture?observe:input,priv);
  if(capture){std::lock_guard<std::mutex> lock(state.mutex);state.setupSuccess=result==ESP_OK;}
  return result;
}
#endif
