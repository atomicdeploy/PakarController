#pragma once
#include <ESPAsyncWebServer.h>
#include <functional>
#include <array>
#if !defined(ESP8266) && !defined(ARDUINO_ARCH_ESP8266)
#include <mutex>
#include <atomic>
#endif

namespace PakarHttpDetail {
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
  AsyncWebServerRequest *request() { return current_.get(); }
  String remoteAddress() const { return currentMailbox_ ? currentMailbox_->remoteAddress : String(); }
  void on(const char *path, WebRequestMethodComposite method, std::function<void()> handler) {
    native_.on(path, method, [this, handler](AsyncWebServerRequest *request) {
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
        if(auto *active=owned->getResponse();active && client->canSend())active->_ack(owned.get(),0,0);
      });
      {
        PakarHttpDetail::Lock lock(queueMutex_);
        for (auto &slot : pending_) if (slot.request.expired()) {
          slot.handler = handler; slot.mailbox=mailbox;slot.request = paused; return;
        }
      }
      request->send(503, "text/plain", "Request queue is full; retry shortly");
    });
  }
  void service() {
    if (processing()) return;
    std::shared_ptr<AsyncWebServerRequest> request;
    std::shared_ptr<Mailbox> mailbox;
    std::function<void()> handler;
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
        mailbox=std::move(slot.mailbox);
        nextSlot_=(index+1)%pending_.size();break;
      }
    }
    if(!request) return;
    // No queue lock across handlers, response I/O or reentrant service calls.
    processing_.set(true);current_=request;currentMailbox_=mailbox;
    if(open()) {
      if(handler) handler();
      else send(503,"text/plain","Request dispatch state invalid");
    }
    current_.reset();currentMailbox_.reset();processing_.set(false); // bounded: one request per loop
  }
  bool hasArg(const char *name) const { return current_ && current_->hasArg(name); }
  String arg(const char *name) const { return current_ ? current_->arg(name) : String(); }
  void send(int code, const char *type, const String &body) {
    if (open()) send(current_->beginResponse(code,type,body));
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
    if (open()) send(current_->beginResponse(code,type,reinterpret_cast<const uint8_t *>(body),strlen_P(body)));
  }
  void streamFile(File &file, const char *type) {
    if (open()) send(current_->beginResponse(file,file.name(),type,true));
  }
  void begin() { native_.begin(); }
 private:
  struct Mailbox {
    PakarHttpDetail::Mutex mutex;
    bool closed=false,published=false;
    String remoteAddress;
    std::unique_ptr<AsyncWebServerResponse> response;
  };
  bool open() const {
    if(!current_ || !currentMailbox_)return false;
    PakarHttpDetail::Lock lock(currentMailbox_->mutex);
    return !currentMailbox_->closed && !currentMailbox_->published;
  }
  struct Pending { AsyncWebServerRequestPtr request;std::shared_ptr<Mailbox> mailbox;std::function<void()> handler; };
  std::array<Pending, 8> pending_{};
  size_t nextSlot_=0; // Access only while holding queueMutex_.
  PakarHttpDetail::Mutex queueMutex_;
  PakarHttpDetail::Processing processing_;
  std::shared_ptr<AsyncWebServerRequest> current_;
  std::shared_ptr<Mailbox> currentMailbox_;
  AsyncWebServer native_;
};
