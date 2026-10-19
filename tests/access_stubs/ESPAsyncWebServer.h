#pragma once
#include "FS.h"
#include <functional>
#include <memory>
#include <map>
#include <vector>
#include <cassert>
#include <algorithm>
#include <deque>
namespace AsyncSdkFixture {
inline thread_local bool actor=false;
struct Actor {bool before=actor;Actor(){actor=true;}~Actor(){actor=before;}};
inline std::atomic<unsigned> liveResponses{0};
}
using WebRequestMethodComposite = int;
using AwsResponseFiller=std::function<size_t(uint8_t*,size_t,size_t)>;
constexpr int HTTP_GET = 1;
constexpr int HTTP_POST = 2, HTTP_HEAD = 4, HTTP_PUT = 8, HTTP_DELETE = 16;
constexpr uint8_t ASYNC_WRITE_FLAG_COPY=1;
enum WebResponseState {RESPONSE_SETUP,RESPONSE_HEADERS,RESPONSE_CONTENT,RESPONSE_WAIT_ACK,RESPONSE_END,RESPONSE_FAILED};
struct AsyncWebHeader { String text; const String &value() const { return text; } };
class DefaultHeaders {
 public:
  std::map<std::string,String> headers;
  static DefaultHeaders &Instance(){static DefaultHeaders value;return value;}
  void addHeader(const char *name,const String &value){headers[name]=value;}
};
struct AsyncWebServerResponse {
 protected:
  int _code=0;String _contentType;size_t _contentLength=0;
  bool _sendContentLength=true,_chunked=false;
  size_t _headLength=0,_sentLength=0,_ackedLength=0,_writtenLength=0;
  WebResponseState _state=RESPONSE_SETUP;
 public:
  int status; String body;
  std::map<std::string, String> headers;
  bool flash=false;
  AwsResponseFiller filler;
  size_t length=0,offset=0;
  unsigned polls=0;
  AsyncWebServerResponse():status(0){++AsyncSdkFixture::liveResponses;}
  AsyncWebServerResponse(int code,const String &text):_code(code),status(code),body(text){++AsyncSdkFixture::liveResponses;}
  virtual ~AsyncWebServerResponse(){--AsyncSdkFixture::liveResponses;}
  virtual size_t _ack(class AsyncWebServerRequest *,size_t,uint32_t);
  virtual void _respond(class AsyncWebServerRequest *);
  virtual bool _sourceValid()const{return true;}
  virtual bool _finished()const{return _state>RESPONSE_WAIT_ACK;}
  bool _failed()const{return _state==RESPONSE_FAILED;}
  int code()const{return _code;}
  void _assembleHead(String &out,uint8_t){
    out="HTTP/1.1 "+std::to_string(_code)+" Fixture\r\n";
    if(_sendContentLength)out+="Content-Length: "+std::to_string(_contentLength)+"\r\n";
    if(!_contentType.empty())out+="Content-Type: "+_contentType+"\r\n";
    for(const auto &h:headers)out+=h.first+": "+h.second+"\r\n";
    out+="\r\n";_headLength=out.length();
  }
  void start(class AsyncWebServerRequest *);
  size_t advance(class AsyncWebServerRequest *);
  void setCode(int code){status=_code=code;}
  void addHeader(const char *name, const String &value,bool replace=true) { if(replace||!headers.count(name))headers[name] = value; }
};
struct AsyncClient {
  struct Address {String toString()const{return "192.0.2.10";}};
  bool alive = true;
  bool writable=true;
  size_t chunkLimit=1024;
  size_t windowCapacity=5744,inFlight=0,addCalls=0,sendCalls=0,acked=0;
  std::deque<size_t> addLimits;
  std::deque<bool> sendResults;
  String wire,sentWire;
  std::function<void()> onSend,onClose;
  bool connected() const { return alive; }
  bool canSend()const{return space()!=0;}
  size_t space()const{return alive&&writable?std::min(chunkLimit,windowCapacity-inFlight):0;}
  size_t add(const char *bytes,size_t length,uint8_t flags=ASYNC_WRITE_FLAG_COPY){
    assert(AsyncSdkFixture::actor&&flags==ASYNC_WRITE_FLAG_COPY);++addCalls;
    size_t count=std::min(length,space());
    if(!addLimits.empty()){count=std::min(count,addLimits.front());addLimits.pop_front();}
    wire.append(bytes,count);inFlight+=count;return count;
  }
  bool send(){
    assert(AsyncSdkFixture::actor);++sendCalls;
    if(!sendResults.empty()){const bool result=sendResults.front();sendResults.pop_front();if(!result)return false;}
    sentWire=wire;if(onSend)onSend();return true;
  }
  void close(){if(alive){alive=false;if(onClose)onClose();}}
  size_t acknowledge(size_t bytes){const size_t count=std::min(bytes,inFlight);inFlight-=count;acked+=count;return count;}
  Address remoteIP()const{return {};}
  std::function<void(void*,AsyncClient*)> pollCallback;
  void onPoll(std::function<void(void*,AsyncClient*)> fn){pollCallback=std::move(fn);}
  void poll(){AsyncSdkFixture::Actor actor;if(alive&&pollCallback)pollCallback(nullptr,this);}
};
class AsyncWebServerRequest : public std::enable_shared_from_this<AsyncWebServerRequest> {
 public:
  AsyncWebServerRequest(){
    // Normal fixtures model an introduced supported app. Negative admission
    // cases explicitly remove/replace this header; never relax production.
    headers["X-Pakar-Version"].text="0.2.8";
    connection.onSend=[this](){const auto boundary=connection.sentWire.find("\r\n\r\n");
      if(boundary!=String::npos)body=connection.sentWire.substr(boundary+4);};
    connection.onClose=[this](){if(disconnectCallback)disconnectCallback();};
  }
  AsyncWebServerRequest(AsyncWebServerRequest&&)=default;
  AsyncWebServerRequest&operator=(AsyncWebServerRequest&&)=default;
  std::weak_ptr<AsyncWebServerRequest> pause() { if(onPause)onPause();paused = true;strictLifecycle=true;return shared_from_this(); }
  std::function<void()> onPause;
  std::function<void()> afterHandler,onResponseStart,disconnectCallback;
  void onDisconnect(std::function<void()> fn){disconnectCallback=std::move(fn);}
  void disconnect(){AsyncSdkFixture::Actor actor;connection.close();}
  AsyncWebServerResponse *getResponse()const{return response_.get();}
  // Model the real SDK's post-middleware _send tail and its late _sent write.
  // Strict paused requests may only start/advance output on the network actor.
  void sdkSend(){
    if(sent||paused||!response_)return;
    if(strictLifecycle)assert(AsyncSdkFixture::actor);
    ++responseStarts;if(onResponseStart)onResponseStart();
    status=response_->code();responseHeaders=response_->headers;flashResponse=response_->flash;
    response_->start(this);sent=true;
  }
  AsyncClient *client() { return &connection; }
  uint8_t version()const{return 1;}
  // Actual SDK WebRequest.cpp248-265: close only after response completion.
  // In particular a response that marks a FILLED final block complete is
  // closed here before its unadmitted tail can ever be retried.
  void sdkAck(size_t bytes){
    AsyncSdkFixture::Actor actor;const size_t count=connection.acknowledge(bytes);
    if(!response_)return;
    if(!response_->_finished())response_->_ack(this,count,0);
    if(response_->_finished())connection.close();
  }
  bool isSent() const { return sent; }
  int method() const { return requestMethod; }
  String url() const { return path; }
  String host() const { return "ledboard.local"; }
  bool hasHeader(const char *name) const { return headers.count(name); }
  const AsyncWebHeader *getHeader(const char *name) const {
    auto found = headers.find(name); return found == headers.end() ? nullptr : &found->second;
  }
  bool hasArg(const char *name) const { return arguments.count(name); }
  String arg(const char *name) const {
    auto found = arguments.find(name); return found == arguments.end() ? String() : found->second;
  }
  void send(int code, const char *, const String &text) {send(new AsyncWebServerResponse(code,text));}
  AsyncWebServerResponse *beginResponse(int code, const char *, const String &text) {
    return new AsyncWebServerResponse(code,text);
  }
  AsyncWebServerResponse *beginResponse(const char *,size_t length,AwsResponseFiller filler){
    auto *response=new AsyncWebServerResponse(200,"");response->length=length;response->filler=std::move(filler);return response;
  }
  AsyncWebServerResponse *beginResponse(int code,const char *,const uint8_t *,size_t){
    auto* response=new AsyncWebServerResponse(code,"");response->flash=true;return response;
  }
  AsyncWebServerResponse *beginResponse(File &,const char *,const char *,bool){
    return new AsyncWebServerResponse(200,"");
  }
  void send(AsyncWebServerResponse *response) {
    if(strictLifecycle)assert(AsyncSdkFixture::actor);
    response_.reset(response);
    if(paused){paused=false;sdkSend();}
    else if(!strictLifecycle)sdkSend();
  }
  void send(int code,const char *type,const uint8_t *data,size_t bytes){send(beginResponse(code,type,data,bytes));}
  void send(File &file,const char *path,const char *type,bool download){send(beginResponse(file,path,type,download));}
  AsyncClient connection;
  int requestMethod = HTTP_GET; String path = "/api/avr/probe", body;
  bool basicAllowed = true;
  std::map<std::string, AsyncWebHeader> headers;
  std::map<std::string, String> arguments, responseHeaders;
  int status = 0; bool paused = false, sent = false, flashResponse = false;
  bool strictLifecycle=false;
  unsigned responseStarts=0;
 private:
  std::unique_ptr<AsyncWebServerResponse> response_;
};
inline size_t AsyncWebServerResponse::advance(AsyncWebServerRequest *request){
  assert(AsyncSdkFixture::actor);
  if(!filler || !request->client()->canSend() || offset>=length)return 0;
  const size_t maximum=std::min(length-offset,request->connection.chunkLimit);
  std::vector<uint8_t> data(maximum);const size_t bytes=filler(data.data(),maximum,offset);
  assert(bytes<=maximum);request->body.append(reinterpret_cast<const char*>(data.data()),bytes);offset+=bytes;return bytes;
}
inline void AsyncWebServerResponse::start(AsyncWebServerRequest *request){
  assert(AsyncSdkFixture::actor || !request->strictLifecycle);
  _respond(request);
}
inline void AsyncWebServerResponse::_respond(AsyncWebServerRequest *request){
  if(filler){request->body.clear();advance(request);}else request->body=body;
  _state=RESPONSE_END;
}
inline size_t AsyncWebServerResponse::_ack(AsyncWebServerRequest *request,size_t,uint32_t){
  assert(AsyncSdkFixture::actor);++polls;return advance(request);
}
using AsyncWebServerRequestPtr = std::weak_ptr<AsyncWebServerRequest>;
using ArMiddlewareNext = std::function<void()>;
class AsyncMiddleware {
 public:
  virtual ~AsyncMiddleware() = default;
  virtual void run(AsyncWebServerRequest *, ArMiddlewareNext) = 0;
};
class AsyncMiddlewareFunction : public AsyncMiddleware {
 public:
  explicit AsyncMiddlewareFunction(std::function<void(AsyncWebServerRequest *, ArMiddlewareNext)> fn) : fn_(fn) {}
  void run(AsyncWebServerRequest *request, ArMiddlewareNext next) override { fn_(request, next); }
 private:
  std::function<void(AsyncWebServerRequest *, ArMiddlewareNext)> fn_;
};
enum class AsyncAuthType { AUTH_BASIC };
class AsyncAuthenticationMiddleware {
 public:
  void setAuthType(AsyncAuthType) {}
  void setUsername(const char *) {}
  void setPassword(const char *) {}
  void setRealm(const char *) {}
  bool allowed(AsyncWebServerRequest *request) { return request->basicAllowed; }
};
struct AsyncWebSocketClient { void close(int, const char *) {} };
struct AsyncWebSocket {
  // Production expiration must use the SDK's lock-held ID API. Deliberately
  // omit client(id): compiling this fixture rejects reintroducing raw leases.
  std::vector<uint32_t> closed;
  std::function<void(uint32_t)> onClose;
  void close(uint32_t id, uint16_t, const char *) {
    closed.push_back(id); if (onClose) onClose(id);
  }
};
// Installed SDK3.12.1 WebServer.cpp: a bare string uses BackwardCompatible
// (exact OR descendant), explicit exact() does not. Dispatch is first-match.
class AsyncURIMatcher {
 public:
  AsyncURIMatcher(const char *path):path_(path){}
  static AsyncURIMatcher exact(const String &path){return AsyncURIMatcher(path,true);}
  bool matches(AsyncWebServerRequest *request)const{
    const auto path=request->url();
    return path==path_ || (!exact_ && path.startsWith((path_+"/").c_str()));
  }
  const String &path()const{return path_;}
 private:
  AsyncURIMatcher(const String &path,bool exact):path_(path),exact_(exact){}
  String path_;bool exact_=false;
};
class AsyncWebServer {
 public:
  explicit AsyncWebServer(uint16_t) {}
  void on(const char *path, int method, std::function<void(AsyncWebServerRequest *)> handler) {
    on(AsyncURIMatcher(path),method,std::move(handler));
  }
  void on(AsyncURIMatcher uri, int method, std::function<void(AsyncWebServerRequest *)> handler) {
    auto wrapped=[handler](AsyncWebServerRequest *request){
      AsyncSdkFixture::Actor actor;handler(request);
      if(request->afterHandler)request->afterHandler();
      request->sdkSend(); // Real _onData always calls _send after middleware.
    };
    handlers[uri.path()]=wrapped;
    routes.push_back({std::move(uri),method,std::move(wrapped)});
  }
  bool dispatch(AsyncWebServerRequest *request){
    for(const auto &route:routes)
      if((route.method&request->method()) && route.uri.matches(request)){
        route.handler(request);return true;
      }
    AsyncSdkFixture::Actor actor;request->send(404,"text/plain","Not found");return false;
  }
  void begin() {}
  void addMiddleware(AsyncMiddleware *) {}
  std::map<std::string, std::function<void(AsyncWebServerRequest *)>> handlers;
 private:
  struct Route {
    AsyncURIMatcher uri;int method;
    std::function<void(AsyncWebServerRequest *)> handler;
  };
  std::vector<Route> routes;
};
