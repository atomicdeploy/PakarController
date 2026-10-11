#pragma once
#include "FS.h"
#include <functional>
#include <memory>
#include <map>
using WebRequestMethodComposite = int;
constexpr int HTTP_GET = 1;
constexpr int HTTP_POST = 2, HTTP_HEAD = 4, HTTP_PUT = 8, HTTP_DELETE = 16;
struct AsyncWebHeader { String text; const String &value() const { return text; } };
struct AsyncWebServerResponse {
  int status; String body;
  void addHeader(const char *, const String &) {}
};
struct AsyncClient { bool alive = true; bool connected() const { return alive; } };
class AsyncWebServerRequest : public std::enable_shared_from_this<AsyncWebServerRequest> {
 public:
  std::weak_ptr<AsyncWebServerRequest> pause() { paused = true; return shared_from_this(); }
  AsyncClient *client() { return &connection; }
  bool isSent() const { return sent; }
  int method() const { return requestMethod; }
  String url() const { return path; }
  String host() const { return "ledboard.local"; }
  bool hasHeader(const char *name) const { return headers.count(name); }
  const AsyncWebHeader *getHeader(const char *name) const {
    auto found = headers.find(name); return found == headers.end() ? nullptr : &found->second;
  }
  bool hasArg(const char *) const { return false; }
  String arg(const char *) const { return String(); }
  void send(int code, const char *, const String &text) { status = code; sent = true; body = text; }
  AsyncWebServerResponse *beginResponse(int code, const char *, const String &text) {
    return new AsyncWebServerResponse{code, text};
  }
  void send(AsyncWebServerResponse *response) {
    send(response->status, "", response->body); delete response;
  }
  void send(int code, const char *, const uint8_t *, size_t) { status = code; sent = true; flashResponse = true; }
  void send(File &, const char *, const char *, bool) { status = 200; sent = true; }
  AsyncClient connection;
  int requestMethod = HTTP_GET; String path = "/api/avr/probe", body;
  bool basicAllowed = true;
  std::map<std::string, AsyncWebHeader> headers;
  int status = 0; bool paused = false, sent = false, flashResponse = false;
};
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
struct AsyncWebSocket { AsyncWebSocketClient *client(uint32_t) { return nullptr; } };
class AsyncWebServer {
 public:
  explicit AsyncWebServer(uint16_t) {}
  void on(const char *path, int, std::function<void(AsyncWebServerRequest *)> handler) { handlers[path] = handler; }
  void begin() {}
  void addMiddleware(AsyncMiddleware *) {}
  std::map<std::string, std::function<void(AsyncWebServerRequest *)>> handlers;
};
