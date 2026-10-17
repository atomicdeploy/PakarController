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
  void on(const char *path, WebRequestMethodComposite method, std::function<void()> handler) {
    native_.on(path, method, [this, handler](AsyncWebServerRequest *request) {
      auto paused=request->pause();
      {
        PakarHttpDetail::Lock lock(queueMutex_);
        for (auto &slot : pending_) if (slot.request.expired()) {
          slot.handler = handler; slot.request = paused; return;
        }
      }
      request->send(503, "text/plain", "Request queue is full; retry shortly");
    });
  }
  void service() {
    if (processing()) return;
    std::shared_ptr<AsyncWebServerRequest> request;
    std::function<void()> handler;
    {
      PakarHttpDetail::Lock lock(queueMutex_);
      for (auto &slot : pending_) {
        request=slot.request.lock();
        if (!request) { slot.handler = nullptr; continue; }
        handler=std::move(slot.handler);slot.request.reset();slot.handler=nullptr;break;
      }
    }
    if(!request) return;
    // No queue lock across handlers, response I/O or reentrant service calls.
    processing_.set(true);current_=request;
    if(request->client() && request->client()->connected()) {
      if(handler) handler();
      else request->send(503,"text/plain","Request dispatch state invalid");
    }
    current_.reset();processing_.set(false); // bounded: one request per loop
  }
  bool hasArg(const char *name) const { return current_ && current_->hasArg(name); }
  String arg(const char *name) const { return current_ ? current_->arg(name) : String(); }
  void send(int code, const char *type, const String &body) {
    if (connected()) current_->send(code, type, body);
  }
  void send_P(int code, const char *type, const char *body) {
    // Keep the large embedded UI in flash, not a per-client heap String.
    if (connected()) current_->send(code, type, reinterpret_cast<const uint8_t *>(body), strlen_P(body));
  }
  void streamFile(File &file, const char *type) {
    if (connected()) current_->send(file, file.name(), type, true);
  }
  void begin() { native_.begin(); }
 private:
  bool connected() const { return current_ && current_->client() && current_->client()->connected() && !current_->isSent(); }
  struct Pending { AsyncWebServerRequestPtr request; std::function<void()> handler; };
  std::array<Pending, 8> pending_{};
  PakarHttpDetail::Mutex queueMutex_;
  PakarHttpDetail::Processing processing_;
  std::shared_ptr<AsyncWebServerRequest> current_;
  AsyncWebServer native_;
};
