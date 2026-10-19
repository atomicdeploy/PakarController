#include <esp_eth_driver.h>
#include <cassert>
#include <cstring>
extern "C" esp_err_t esp_eth_update_input_path_info(esp_eth_handle_t handle,EthernetInput input,void *priv) {
  ++ethernetSdk.inputCalls;
  if(!ethernetSdk.inputResult)return ESP_ERR_INVALID_STATE;
  ethernetSdk.inputs[handle]={input,priv};return ESP_OK;
}
extern "C" esp_err_t esp_eth_ioctl(esp_eth_handle_t handle,int command,void *value) {
  assert(handle&&value);++ethernetSdk.ioctlCalls;
  if(command==ETH_CMD_G_MAC_ADDR){const uint8_t mac[]={2,0,0,0,0,1};memcpy(value,mac,6);return ESP_OK;}
  assert(command==ETH_CMD_S_PROMISCUOUS&&ethernetSdk.inTcpip);
  if(!ethernetSdk.ioctlResult)return ESP_ERR_NOT_SUPPORTED;
  ethernetSdk.promiscuous=*static_cast<bool*>(value);return ESP_OK;
}
extern "C" esp_err_t esp_eth_transmit(esp_eth_handle_t handle,void *bytes,size_t length) {
  assert(handle&&bytes&&ethernetSdk.inTcpip);++ethernetSdk.txCalls;
  if(ethernetSdk.onTransmit)ethernetSdk.onTransmit();
  auto p=static_cast<uint8_t*>(bytes);ethernetSdk.transmitted.assign(p,p+length);
  return ethernetSdk.transmitResult?ESP_OK:ESP_ERR_INVALID_STATE;
}
