#pragma once
#include "Arduino.h"
struct esp_ip4_addr_t {uint32_t addr=0;};
constexpr int ESP_OK=0;
inline uint32_t mdnsAddress=0xc901a8c0; // 192.168.1.201
inline unsigned mdnsCalls=0;inline String mdnsLabel;inline uint32_t mdnsBudget=0;
inline int mdns_query_a(const char *host,uint32_t budget,esp_ip4_addr_t *out){
  ++mdnsCalls;mdnsLabel=host;mdnsBudget=budget;testNow+=20;out->addr=mdnsAddress;return mdnsAddress?0:-1;
}
