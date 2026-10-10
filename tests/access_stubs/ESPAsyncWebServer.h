#pragma once
#include "FS.h"
#include <functional>
#include <memory>
#include <map>
using WebRequestMethodComposite = int;
constexpr int HTTP_GET = 1;
struct AsyncClient { bool alive = true; bool connected() const { return alive; } };
class AsyncWebServerRequest : public std::enable_shared_from_this<AsyncWebServerRequest> {
 public:
  std::weak_ptr<AsyncWebServerRequest> pause() { paused = true; return shared_from_this(); }
  AsyncClient *client() { return &connection; }
  bool isSent() const { return sent; }
  bool hasArg(const char *) const { return false; }
  String arg(const char *) const { return String(); }
  void send(int code, const char *, const String &) { status = code; sent = true; }
  void send(int code, const char *, const uint8_t *, size_t) { status = code; sent = true; flashResponse = true; }
  void send(File &, const char *, const char *, bool) { status = 200; sent = true; }
  AsyncClient connection;
  int status = 0; bool paused = false, sent = false, flashResponse = false;
};
using AsyncWebServerRequestPtr = std::weak_ptr<AsyncWebServerRequest>;
class AsyncWebServer {
 public:
  explicit AsyncWebServer(uint16_t) {}
  void on(const char *path, int, std::function<void(AsyncWebServerRequest *)> handler) { handlers[path] = handler; }
  void begin() {}
  std::map<std::string, std::function<void(AsyncWebServerRequest *)>> handlers;
};
