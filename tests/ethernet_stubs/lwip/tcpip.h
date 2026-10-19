#pragma once
#include <esp_eth_driver.h>
constexpr int ERR_OK=0;
inline int tcpip_try_callback(void(*work)(void*),void *arg) {
  ++ethernetSdk.queueCalls;
  if(!ethernetSdk.queueResult||ethernetSdk.pending)return -1;
  ethernetSdk.pending=work;ethernetSdk.pendingArg=arg;return ERR_OK;
}
