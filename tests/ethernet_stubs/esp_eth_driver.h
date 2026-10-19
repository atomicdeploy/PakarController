#pragma once
#include <stdint.h>
#include <stddef.h>
#include <map>
#include <vector>
#include <functional>
using esp_err_t=int;
using esp_eth_handle_t=void*;
constexpr int ESP_OK=0,ESP_ERR_INVALID_STATE=0x103,ESP_ERR_INVALID_ARG=0x102,ESP_ERR_NOT_SUPPORTED=0x106;
constexpr int ETH_CMD_G_MAC_ADDR=0,ETH_CMD_S_PROMISCUOUS=8;
using EthernetInput=esp_err_t(*)(esp_eth_handle_t,uint8_t*,uint32_t,void*,void*);
struct EthernetSdkFixture {
  struct Input {EthernetInput callback=nullptr;void *priv=nullptr;};
  std::map<void*,Input> inputs;
  bool inputResult=true,ioctlResult=true,transmitResult=true,queueResult=true,promiscuous=false,inTcpip=false;
  unsigned inputCalls=0,forwardCalls=0,txCalls=0,ioctlCalls=0,queueCalls=0;
  uint8_t *forwardBuffer=nullptr;uint32_t forwardLength=0;void *forwardInfo=nullptr,*forwardPriv=nullptr;
  std::vector<uint8_t> transmitted;
  void(*pending)(void*)=nullptr;void *pendingArg=nullptr;
  std::function<void()> onTransmit;
  void drain(){auto work=pending;auto arg=pendingArg;pending=nullptr;pendingArg=nullptr;if(work){inTcpip=true;work(arg);inTcpip=false;}}
};
inline EthernetSdkFixture ethernetSdk;
extern "C" esp_err_t esp_eth_update_input_path_info(esp_eth_handle_t,EthernetInput,void*);
extern "C" esp_err_t esp_eth_ioctl(esp_eth_handle_t,int,void*);
extern "C" esp_err_t esp_eth_transmit(esp_eth_handle_t,void*,size_t);
inline esp_err_t ethernetFixtureInput(esp_eth_handle_t,uint8_t *buffer,uint32_t length,void *priv,void *info) {
  ++ethernetSdk.forwardCalls;ethernetSdk.forwardBuffer=buffer;ethernetSdk.forwardLength=length;
  ethernetSdk.forwardPriv=priv;ethernetSdk.forwardInfo=info;return 77;
}
