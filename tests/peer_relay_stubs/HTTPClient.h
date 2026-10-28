#pragma once
#include "WiFi.h"
#include <deque>
#include <cassert>
constexpr int HTTPC_DISABLE_FOLLOW_REDIRECTS=0;
constexpr int HTTPC_ERROR_CONNECTION_REFUSED=-1,HTTPC_ERROR_NOT_CONNECTED=-4,HTTPC_ERROR_CONNECTION_LOST=-5;
struct TestReply {int code=200;String body,board="esp32s3-b81f3fbb0684",boot="1234567890abcdef1234567890abcdef";uint32_t delay=15;};
struct TestRequest {String url,method,body,address;std::map<String,String> headers;};
inline std::deque<TestReply> replies;inline std::vector<TestRequest> requests;
struct TestHttpStream {
  String body;size_t cursor=0;
  size_t available()const{return body.size()-cursor;}
  int read(uint8_t *out,size_t n){n=std::min(n,available());memcpy(out,body.data()+cursor,n);cursor+=n;return int(n);}
};
class HTTPClient {
 public:
  void setConnectTimeout(unsigned n){assert(n<=1000);}void setTimeout(unsigned n){assert(n<=1000);}
  void setFollowRedirects(int n){assert(n==0);}void setReuse(bool yes){assert(!yes);}
  bool begin(NetworkClient &client,const String &url){request_.url=url;client_=&client;return true;}
  void setUserAgent(const String &value){request_.headers["User-Agent"]=value;}
  void addHeader(const String &key,const String &value){if(key!="Host"&&key!="User-Agent")request_.headers[key]=value;}
  void collectHeaders(const char **,unsigned){}
  int GET(){return dispatch("GET","");}int POST(const String &body){return dispatch("POST",body);}
  String header(const String &key){return key=="X-Pakar-Board"?reply_.board:key=="X-Pakar-Boot"?reply_.boot:String();}
  int getSize()const{return int(reply_.body.size());}TestHttpStream *getStreamPtr(){return &stream_;}
  bool connected()const{return stream_.available()>0;}void end(){}
 private:
  TestRequest request_;TestReply reply_;TestHttpStream stream_;NetworkClient *client_=nullptr;
  int dispatch(const String &method,const String &body){
    const String host=request_.url.substring(7,request_.url.find('/',7));
    client_->connect(host.c_str(),80,700);request_.address=physicalAddress;request_.headers["Host"]=host;
    request_.method=method;request_.body=body;requests.push_back(request_);
    assert(!replies.empty());reply_=replies.front();replies.pop_front();testNow+=reply_.delay;stream_.body=reply_.body;return reply_.code;
  }
};
