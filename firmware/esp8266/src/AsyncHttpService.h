#pragma once
#include <ESPAsyncWebServer.h>
#include <functional>
#include <array>

// Async HTTP transport, with hardware-affecting handlers deferred to loop().
// Request continuations are weak: disconnects never leave dangling requests.
class AsyncHttpService {
 public:
  explicit AsyncHttpService(uint16_t port) : native_(port) {}
  AsyncWebServer &native() { return native_; }
  bool processing() const { return bool(current_); }
  AsyncWebServerRequest *request() { return current_.get(); }
  void on(const char *path, WebRequestMethodComposite method, std::function<void()> handler) {
    native_.on(path, method, [this, handler](AsyncWebServerRequest *request) {
      for (auto &slot : pending_) if (slot.request.expired()) {
        slot.handler = handler; slot.request = request->pause(); return;
      }
      request->send(503, "text/plain", "Request queue is full; retry shortly");
    });
  }
  void service() {
    if (current_) return;
    for (auto &slot : pending_) {
      auto request = slot.request.lock();
      if (!request) { slot.handler = nullptr; continue; }
      auto handler = slot.handler; slot.request.reset(); slot.handler = nullptr;
      current_ = request;
      if (request->client() && request->client()->connected()) handler();
      current_.reset(); return; // bounded work per loop, keeps UART/effects serviced
    }
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
  std::shared_ptr<AsyncWebServerRequest> current_;
  AsyncWebServer native_;
};
