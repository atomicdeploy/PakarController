#pragma once
#include "Lldp.h"
#include <esp_eth_driver.h>
namespace PakarS3 {
// Captures only the standard glue installed synchronously by our ETH.begin.
// The actual callback and priv remain authoritative; no private SDK writes.
class EthernetLldpSetup {
 public:
  explicit EthernetLldpSetup(const void *owner);
  ~EthernetLldpSetup();
  bool admitted(esp_eth_handle_t handle)const;
  EthernetLldpSetup(const EthernetLldpSetup&)=delete;
  EthernetLldpSetup& operator=(const EthernetLldpSetup&)=delete;
 private:
  const void *owner_=nullptr;
  void *task_=nullptr;
  bool active_=false;
};
void attachEthernetLldp(const void *owner,esp_eth_handle_t handle,const char *hostname,bool observed);
void renameEthernetLldp(const void *owner,const char *hostname);
void detachEthernetLldp(const void *owner); // Cancels queued work before driver deletion.
void releaseEthernetLldp(const void *owner); // Only after SDK confirms driver/netif removal.
bool configureEthernetLldp(const void *owner,bool enabled,bool receive);
void serviceEthernetLldp(const void *owner,bool link,const uint8_t mac[6],uint32_t now);
void snapshotEthernetLldp(const void *owner,LldpStatus &status,uint32_t now);
}
