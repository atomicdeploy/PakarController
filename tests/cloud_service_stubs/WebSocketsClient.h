#pragma once
#include "WiFi.h"
#include <vector>
enum WStype_t{WStype_CONNECTED,WStype_TEXT,WStype_ERROR};
enum {WSC_NOT_CONNECTED,WSC_CONNECTED};
class WebSocketsClient {
 public:
  inline static WebSocketsClient *latest=nullptr;
  WebSocketsClient(){latest=this;_client.tcp=&socket_;}
  unsigned connectAttempts=0,receives=0,pings=0,timeouts=0;
  std::vector<String> sent;
  void (*callback)(WStype_t,uint8_t*,size_t)=nullptr;
  void onEvent(decltype(callback) f){callback=f;}
  void setReconnectInterval(unsigned long){}void enableHeartbeat(unsigned,unsigned,unsigned){}
  void setExtraHeaders(const char*){}
  template<class...Args>void begin(Args...){}
  template<class...Args>void beginSslWithCA(Args...){}
  void disconnect(){socket_.stop();_client.status=WSC_NOT_CONNECTED;}
  bool isConnected(){return _client.status==WSC_CONNECTED;}
  void loop(){if(!clientIsConnected(&_client)){++connectAttempts;socket_.live=true;_client.status=WSC_CONNECTED;if(callback)callback(WStype_CONNECTED,nullptr,0);}else ++receives;}
  bool sendTXT(const char *s){if(!isConnected())return false;sent.emplace_back(s);return true;}
  bool sendTXT(const String &s){return sendTXT(s.c_str());}
  void inject(const String &s){std::vector<uint8_t> p(s.begin(),s.end());callback(WStype_TEXT,p.data(),p.size());}
  void loseTcp(){socket_.stop();} // Deliberately leave stale WS_CONNECTED status.
 protected:
  Client socket_;
  struct WSclient_t{Client *tcp=nullptr;int status=WSC_NOT_CONNECTED;} _client;
  bool clientIsConnected(WSclient_t *c){if(c->tcp&&c->tcp->connected()&&c->status==WSC_CONNECTED)return true;c->status=WSC_NOT_CONNECTED;return false;}
  void handleClientData(){++receives;}void handleHBPing(){++pings;}
  void handleHBTimeout(WSclient_t*){++timeouts;}
};
