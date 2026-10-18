#pragma once
#include "WiFi.h"
#include <vector>
class PubSubClient {
 public:
  struct Packet{String topic,payload;bool retained;};
  inline static PubSubClient *latest=nullptr;
  PubSubClient(){latest=this;}
  Client *client=nullptr;
  void (*callback)(char*,uint8_t*,unsigned)=nullptr;
  unsigned connectAttempts=0,loops=0,beginCalls=0,endCalls=0,disconnects=0;
  std::vector<String> clientIds;
  uint16_t bufferSize=0;size_t largestWrite=0,shortWriteLimit=SIZE_MAX;
  std::vector<Packet> packets;Packet pending{};size_t expected=0;
  void setCallback(decltype(callback) f){callback=f;}
  void setBufferSize(uint16_t n){bufferSize=n;}
  void setKeepAlive(unsigned){}void setSocketTimeout(unsigned){}
  void setServer(const char*,unsigned){}void setClient(Client &c){client=&c;}
  bool connected(){return client&&client->live;}int state(){return connected()?0:-1;}
  template<class...Args>bool connect(const char *id,Args...){clientIds.emplace_back(id);++connectAttempts;client->live=true;return true;}
  void disconnect(){++disconnects;if(client)client->stop();}
  bool subscribe(const char*){return connected();}
  bool loop(){++loops;return connected();}
  bool publish(const char *topic,const char *payload,bool retained){if(!connected())return false;packets.push_back({topic,payload,retained});return true;}
  bool beginPublish(const char *topic,unsigned n,bool retained){++beginCalls;if(!connected())return false;pending={topic,"",retained};expected=n;return true;}
  size_t write(const uint8_t *p,size_t n){largestWrite=std::max(largestWrite,n);n=min(n,shortWriteLimit);pending.payload.append(reinterpret_cast<const char*>(p),n);return n;}
  int endPublish(){++endCalls;if(pending.payload.size()!=expected)return 0;packets.push_back(pending);return 1;}
  void inject(const String &s){std::vector<uint8_t> p(s.begin(),s.end());char topic[]="command";callback(topic,p.data(),p.size());}
};
