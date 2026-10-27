#pragma once
#include <ESPAsyncWebServer.h>
#include <functional>
#include <array>
#include <cstring>
#if !defined(ESP8266) && !defined(ARDUINO_ARCH_ESP8266)
#include <mutex>
#include <atomic>
#endif

namespace PakarHttpDetail {
// Installed ESPAsyncWebServer3.12.1 may mark its final fill RESPONSE_END
// before a short AsyncClient::add has drained that buffer. Its request ACK
// handler then closes the response. Own known-length snapshots explicitly:
// offsets describe bytes admitted by TCP, never bytes merely filled/copied.
class SnapshotResponse final : public AsyncWebServerResponse {
 public:
  using PendingOutput=std::function<void(bool)>;
  SnapshotResponse(int code,const char *type,std::shared_ptr<const String> payload,PendingOutput pending)
      :payload_(std::move(payload)),pending_(std::move(pending)) {
    configure(code,type,payload_?payload_->length():0);
  }
  SnapshotResponse(int code,const char *type,const char *flash,size_t length,PendingOutput pending)
      :flash_(flash),pending_(std::move(pending)) {configure(code,type,length);}
  bool _sourceValid() const override {return payload_?payload_->length()==_contentLength:flash_||!_contentLength;}
  void _respond(AsyncWebServerRequest *request) override {
    headOnly_=request->method()==HTTP_HEAD;
    _assembleHead(header_,request->version());_state=RESPONSE_HEADERS;
    _ack(request,0,0);
  }
  size_t _ack(AsyncWebServerRequest *request,size_t length,uint32_t) override {
    if(!request||!request->client()||!request->client()->connected()||_state>=RESPONSE_END)return 0;
    _ackedLength+=length;
    size_t admitted=0;
    // At most eight1024-byte writes per actor pass; no whole flash-UI copy and
    // no filling another buffer after TCP has refused the current bytes.
    for(unsigned slice=0;slice<8;++slice) {
      auto *client=request->client();const size_t room=client->space();
      if(!room)break;
      const bool header=headerOffset_<header_.length();
      const size_t remaining=header?header_.length()-headerOffset_:
          headOnly_?0:_contentLength-bodyOffset_;
      if(!remaining)break;
      size_t count=remaining<room?remaining:room;
      if(count>1024)count=1024;
      const char *bytes=header?header_.c_str()+headerOffset_:
          payload_?payload_->c_str()+bodyOffset_:nullptr;
      uint8_t flashBytes[512];
      if(!bytes) {
        if(count>sizeof(flashBytes))count=sizeof(flashBytes);
        memcpy_P(flashBytes,flash_+bodyOffset_,count);bytes=reinterpret_cast<const char*>(flashBytes);
      }
      const size_t written=client->add(bytes,count,ASYNC_WRITE_FLAG_COPY);
      if(written>count){_state=RESPONSE_FAILED;client->close();return admitted;}
      if(header)headerOffset_+=written;else {bodyOffset_+=written;_sentLength+=written;}
      admitted+=written;_writtenLength+=written;
      if(written<count)break;
    }
    if(admitted||outputPending_)outputPending_=!request->client()->send();
    if(pending_)pending_(outputPending_);
    const bool queued=headerOffset_==header_.length()&&(headOnly_||bodyOffset_==_contentLength);
    if(queued&&!outputPending_) {
      // WAIT_ACK is deliberately not finished. The SDK must not close until
      // all actually admitted header/body bytes have their real TCP ACK.
      _state=_ackedLength>=_writtenLength?RESPONSE_END:RESPONSE_WAIT_ACK;
    } else _state=headerOffset_<header_.length()?RESPONSE_HEADERS:RESPONSE_CONTENT;
    return admitted;
  }
 private:
  void configure(int code,const char *type,size_t length) {
    _code=code;_contentType=type;_contentLength=length;_sendContentLength=code!=304;_chunked=false;
    addHeader("Connection","close",false);
  }
  std::shared_ptr<const String> payload_;
  const char *flash_=nullptr;
  PendingOutput pending_;
  String header_;
  size_t headerOffset_=0,bodyOffset_=0;
  bool headOnly_=false,outputPending_=false;
};

#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
// ESP8266 callbacks/loop cooperate on one core. Never yield while publishing.
struct Mutex { void lock() {} void unlock() {} };
struct Processing { bool value=false;bool get()const{return value;}void set(bool v){value=v;} };
#else
// Shared_ptr counters do not protect the shared request/callback pair on S3.
using Mutex=std::mutex;
struct Processing {
  std::atomic<bool> value{false};
  bool get()const{return value.load(std::memory_order_acquire);}
  void set(bool v){value.store(v,std::memory_order_release);}
};
#endif
class Lock {
 public:explicit Lock(Mutex &mutex):mutex_(mutex){mutex_.lock();}
 ~Lock(){mutex_.unlock();}
 Lock(const Lock&)=delete;Lock&operator=(const Lock&)=delete;
 private:Mutex &mutex_;
};
}

// Async HTTP transport, with hardware-affecting handlers deferred to loop().
// Request continuations are weak: disconnects never leave dangling requests.
class AsyncHttpService {
 public:
  explicit AsyncHttpService(uint16_t port) : native_(port) {}
  AsyncWebServer &native() { return native_; }
  bool processing() const { return processing_.get(); }
  uint32_t ingressEpoch() { PakarHttpDetail::Lock lock(queueMutex_);return ingressEpoch_; }
  // Called only by loop ownership, before and after maintenance. Callback
  // publications carry this epoch, so none can survive either boundary.
  void setMaintenanceFence(bool active) {
    PakarHttpDetail::Lock lock(queueMutex_);
    if(maintenanceFence_!=active){maintenanceFence_=active;++ingressEpoch_;}
  }
  AsyncWebServerRequest *request() { return current_.get(); }
  String remoteAddress() const { return currentMailbox_ ? currentMailbox_->remoteAddress : String(); }
  String jsonBody() const {
    if(!current_ || current_->contentType()!=F("application/json") || !current_->_tempObject)return String();
    const auto *body=static_cast<const JsonBody*>(current_->_tempObject);
    return body->used==body->total && strlen(body->text)==body->total ? String(body->text) : String();
  }
  void on(const char *path, WebRequestMethodComposite method, std::function<void()> handler) {
    // SDK3.12.1 bare strings also match descendants (BackwardCompatible) and
    // dispatch picks the first registered match. Literal API routes must not
    // let /contracts swallow /contracts/result or a parent mutation own a child.
    native_.on(AsyncURIMatcher::exact(path), method, [this, handler](AsyncWebServerRequest *request) {
      auto mailbox=std::make_shared<Mailbox>();
      mailbox->remoteAddress=request->client()->remoteIP().toString();
      request->onDisconnect([mailbox](){
        std::unique_ptr<AsyncWebServerResponse> abandoned;
        {PakarHttpDetail::Lock lock(mailbox->mutex);mailbox->closed=true;abandoned=std::move(mailbox->response);}
        // Response destruction (including an owned File) is outside the lock.
      });
      auto paused=request->pause();
      // Only the SDK's AsyncTCP actor may attach/start/advance a response.
      // Sending from loop can race the original _onData -> _send tail and ACK
      // callbacks: SDK3.12.1 sets _sent only AFTER _response->_respond returns.
      // Public AsyncClient::onPoll is an authoritative service-task event, not
      // a sleep/delay guess. The weak request never extends a dead connection.
      request->client()->onPoll([paused,mailbox](void *,AsyncClient *client){
        auto owned=paused.lock();if(!owned || !client || !client->connected())return;
        std::unique_ptr<AsyncWebServerResponse> response;
        {PakarHttpDetail::Lock lock(mailbox->mutex);if(mailbox->closed)return;response=std::move(mailbox->response);}
        if(response){owned->send(response.release());return;}
        // Exact behavior adapted from installed ESPAsyncWebServer3.12.1
        // WebRequest.cpp::_onPoll241-245; SDK onAck/disconnect remain intact.
        bool pendingOutput=false;
        {PakarHttpDetail::Lock lock(mailbox->mutex);pendingOutput=mailbox->pendingOutput;}
        if(auto *active=owned->getResponse();active && (client->canSend()||pendingOutput))active->_ack(owned.get(),0,0);
      });
      {
        PakarHttpDetail::Lock lock(queueMutex_);
        for (auto &slot : pending_) if (slot.request.expired()) {
          slot.handler = handler; slot.mailbox=mailbox;slot.epoch=ingressEpoch_;slot.request = paused; return;
        }
      }
      request->send(503, "text/plain", F("Request queue is full; retry shortly"));
    },nullptr,[](AsyncWebServerRequest *request,uint8_t *data,size_t length,size_t index,size_t total){
      if((request->url()!=F("/api/cells/assignments")&&request->url()!=F("/api/cells/lock")&&
          request->url()!=F("/api/peers/request")&&request->url()!=F("/api/maintenance/request")) ||
          request->contentType()!=F("application/json") || !total || total>4096 || index>total || length>total-index)return;
      if(!index && !request->_tempObject){
        auto *body=static_cast<JsonBody*>(malloc(sizeof(JsonBody)+total));
        if(!body)return;
        body->used=0;body->total=total;body->text[0]=0;request->_tempObject=body;
      }
      auto *body=static_cast<JsonBody*>(request->_tempObject);
      if(!body || body->total!=total || body->used!=index){if(body)body->used=SIZE_MAX;return;}
      memcpy(body->text+index,data,length);body->used+=length;body->text[body->used]=0;
      // ESPAsyncWebServer3.12.1 WebRequest::~AsyncWebServerRequest frees this
      // malloc-owned object on completion/disconnect; never borrow body bytes.
    });
  }
  void service() {
    if (processing()) return;
    std::shared_ptr<AsyncWebServerRequest> request;
    std::shared_ptr<Mailbox> mailbox;
    std::function<void()> handler;
    bool stale=false;
    {
      PakarHttpDetail::Lock lock(queueMutex_);
      // Producers reuse the first free slot. Always starting at slot zero can
      // therefore starve older status/mapping requests in the other slots.
      // Advance only when taking a live request: every occupied slot gets a
      // turn within eight dispatches, without holding the lock across I/O.
      for (size_t offset=0;offset<pending_.size();++offset) {
        const size_t index=(nextSlot_+offset)%pending_.size();
        auto &slot=pending_[index];
        request=slot.request.lock();
        if (!request) { slot.handler = nullptr;slot.mailbox.reset();continue; }
        handler=std::move(slot.handler);slot.request.reset();slot.handler=nullptr;
        stale=slot.epoch!=ingressEpoch_;
        mailbox=std::move(slot.mailbox);
        nextSlot_=(index+1)%pending_.size();break;
      }
    }
    if(!request) return;
    // No queue lock across handlers, response I/O or reentrant service calls.
    processing_.set(true);current_=request;currentMailbox_=mailbox;
    if(open()) {
      if(stale)send(409,"text/plain","Maintenance boundary changed; request not executed");
      else if(handler) handler();
      else send(503,"text/plain","Request dispatch state invalid");
    }
    current_.reset();currentMailbox_.reset();processing_.set(false); // bounded: one request per loop
  }
  bool hasArg(const char *name) const { return current_ && current_->hasArg(name); }
  String arg(const char *name) const { return current_ ? current_->arg(name) : String(); }
  AsyncWebServerResponse *beginResponse(int code,const char *type,const String &body) {
    if(!current_)return nullptr;
    // Retain exactly one immutable complete-message snapshot. The project
    // response, not SDK fill accounting, owns partial-add/backpressure offsets.
    auto payload=std::make_shared<const String>(body);
    if(payload->length()!=body.length())
      return current_->beginResponse(503,"application/json","{\"error\":\"Response snapshot memory unavailable\"}");
    return new PakarHttpDetail::SnapshotResponse(code,type,std::move(payload),pendingOutputObserver());
  }
  void send(int code, const char *type, const String &body) {
    if (open()) send(beginResponse(code,type,body));
  }
  // Takes ownership. Loop creates an independent response; only the SDK actor
  // consumes it. Late/disconnected/duplicate responses are destroyed unsent.
  void send(AsyncWebServerResponse *response) {
    std::unique_ptr<AsyncWebServerResponse> owned(response);
    if(!currentMailbox_ || !response)return;
    {PakarHttpDetail::Lock lock(currentMailbox_->mutex);
      if(currentMailbox_->closed || currentMailbox_->published)return;
      currentMailbox_->response=std::move(owned);currentMailbox_->published=true;}
  }
  void send_P(int code, const char *type, const char *body) {
    // Keep the large embedded UI in flash, not a per-client heap String.
    if (open()) send(new PakarHttpDetail::SnapshotResponse(code,type,body,strlen_P(body),pendingOutputObserver()));
  }
  void send_P(int code,const char *type,const uint8_t *body,size_t length,const char *encoding) {
    // Binary gzip contains NUL bytes: retain its exact length and the same
    // short-write/ACK ownership as other immutable flash snapshots.
    if(!open())return;
    auto *response=new PakarHttpDetail::SnapshotResponse(code,type,reinterpret_cast<const char*>(body),length,pendingOutputObserver());
    if(encoding&&*encoding)response->addHeader("Content-Encoding",encoding);
    send(response);
  }
  void streamFile(File &file, const char *type) {
    if (open()) send(current_->beginResponse(file,file.name(),type,true));
  }
  void begin() { native_.begin(); }
 private:
  struct JsonBody {size_t used,total;char text[1];};
  struct Mailbox {
    PakarHttpDetail::Mutex mutex;
    bool closed=false,published=false,pendingOutput=false;
    String remoteAddress;
    std::unique_ptr<AsyncWebServerResponse> response;
  };
  PakarHttpDetail::SnapshotResponse::PendingOutput pendingOutputObserver() {
    std::weak_ptr<Mailbox> mailbox=currentMailbox_;
    return [mailbox](bool pending){if(auto owned=mailbox.lock()){
      PakarHttpDetail::Lock lock(owned->mutex);owned->pendingOutput=pending;
    }};
  }
  bool open() const {
    if(!current_ || !currentMailbox_)return false;
    PakarHttpDetail::Lock lock(currentMailbox_->mutex);
    return !currentMailbox_->closed && !currentMailbox_->published;
  }
  struct Pending { AsyncWebServerRequestPtr request;std::shared_ptr<Mailbox> mailbox;std::function<void()> handler;uint32_t epoch=0; };
  std::array<Pending, 8> pending_{};
  size_t nextSlot_=0; // Access only while holding queueMutex_.
  PakarHttpDetail::Mutex queueMutex_;
  uint32_t ingressEpoch_=0;bool maintenanceFence_=false;
  PakarHttpDetail::Processing processing_;
  std::shared_ptr<AsyncWebServerRequest> current_;
  std::shared_ptr<Mailbox> currentMailbox_;
  AsyncWebServer native_;
};
