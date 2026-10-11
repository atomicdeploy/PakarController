#pragma once
#include "AccessConfig.h"
#include <ApiPolicy.h>
#include <PeerFence.h>
#include <ESPAsyncWebServer.h>
extern "C" {
#include <osapi.h>
}

// PU850 Authentication/Sessions capability with complete authorization,
// expiring sessions and no password/token diagnostic dumps.
class WebAccess {
 public:
  WebAccess() : middleware_([this](AsyncWebServerRequest *request, ArMiddlewareNext next) {
    const String path = request->url();
    // Before login, uploads, catch-all handlers or dispatch: no aliases.
    if (BoardCore::ApiPolicy::numberedRoute(path.c_str())) {
      request->send(404, "application/json", "{\"error\":\"not_found\"}");
      return;
    }
    const bool publicRead = request->method() == HTTP_GET &&
        (path == F("/") || path == F("/api/auth/info") || path == F("/description.xml") ||
         path == F("/assets/fonts/Vazirmatn-Regular.ttf") || path == F("/assets/fonts/Vazirmatn-OFL.txt"));
    const bool login = path == F("/login") && request->method() == HTTP_POST;
    if (publicRead || login) { next(); return; }
    if (!accessConfig.ready()) { request->send(503, "text/plain", accessConfig.error()); return; }
    if (!sameOrigin(request)) { request->send(403, "text/plain", "Cross-origin request rejected"); return; }
    if (!allowed(request)) {
      request->send(401, "application/json", "{\"error\":\"login_required\",\"login\":\"/login\"}");
      return; // no browser Basic-auth dialog; API clients may still send Basic
    }
    if (!peerAllowed(request)) {
      request->send(409, "application/json", "{\"error\":\"peer_changed\",\"accepted\":false}"); return;
    }
    next();
  }) {}
  void refreshCredentials() {
    basic_.setAuthType(AsyncAuthType::AUTH_BASIC);
    basic_.setUsername(accessConfig.username()); basic_.setPassword(accessConfig.password());
    basic_.setRealm("PakarController");
  }
  AsyncMiddleware *middleware() { return &middleware_; }
  void configurePeerFence(const String &board, const String &boot) {
    boardId_ = board; bootId_ = boot;
  }
  bool peerFenceReady() const {
    return BoardCore::PeerFence::ready(boardId_.c_str(), bootId_.c_str());
  }
  bool peerAllowed(AsyncWebServerRequest *request) const {
    // Method is not an ownership exemption: ISP probe is a hardware-affecting
    // GET, and pinned readback must not silently read a different boot either.
    // Absent headers retain explicitly unfenced compatibility in PeerFence.
    const auto *board = request->getHeader("X-Pakar-Board");
    const auto *boot = request->getHeader("X-Pakar-Boot");
    return BoardCore::PeerFence::accepts(boardId_.c_str(), bootId_.c_str(),
        board ? board->value().c_str() : nullptr, boot ? boot->value().c_str() : nullptr);
  }
  bool allowed(AsyncWebServerRequest *request) {
    if (!accessConfig.ready()) return false;
    if (basic_.allowed(request)) return true;
    const String token = cookieToken(request);
    return sessions_.validate(token.c_str(), millis());
  }
  static bool sameOrigin(AsyncWebServerRequest *request) {
    if (!request->hasHeader("Origin")) return true; // native clients
    return request->getHeader("Origin")->value() == String(F("http://")) + request->host();
  }
  void revokeAll() { sessions_.clear(); refreshCredentials(); }
  uint8_t sessionCount() const {return sessions_.count(millis());}
  bool bindSocket(uint32_t id, AsyncWebServerRequest *request) {
    if (!request || !allowed(request) || !sameOrigin(request)) return false;
    for (auto &slot : sockets_) if (slot.id == 0) {
      slot.id = id; slot.basic = basic_.allowed(request);
      const String token = cookieToken(request);
      token.toCharArray(slot.token, sizeof(slot.token)); return true;
    }
    return false;
  }
  bool socketAllowed(uint32_t id) {
    for (auto &slot : sockets_) if (slot.id == id)
      return accessConfig.ready() && (slot.basic || sessions_.validate(slot.token, millis()));
    return false;
  }
  void forgetSocket(uint32_t id) { for (auto &slot : sockets_) if (slot.id == id) slot = Socket{}; }
  void expireSockets(AsyncWebSocket &ws) {
    for (auto &slot : sockets_) if (slot.id && !slot.basic && !sessions_.validate(slot.token, millis(), false)) {
      const uint32_t id = slot.id; slot = Socket{};
      if (auto *client = ws.client(id)) client->close(1008, "Session expired");
    }
  }
  void registerRoutes(AsyncWebServer &server, std::function<void()> credentialsChanged,
      std::function<bool()> busy) {
    refreshCredentials(); server.addMiddleware(&middleware_);
    server.on("/api/auth/info", HTTP_GET, [](AsyncWebServerRequest *request) {
      String body = F("{\"ready\":"); body += accessConfig.ready() ? F("true") : F("false");
      body += F(",\"username\":\""); body += accessConfig.username();
      body += F("\",\"default_credentials\":"); body += accessConfig.defaultsActive() ? F("true") : F("false");
      body += F(",\"login\":\"/login\",\"settings\":\"/api/auth\"}");
      auto response = request->beginResponse(200, "application/json", body);
      response->addHeader("Cache-Control", "no-store"); request->send(response);
    });
    server.on("/login", HTTP_POST, [this](AsyncWebServerRequest *request) {
      if (!accessConfig.ready()) { request->send(503, "text/plain", accessConfig.error()); return; }
      if (!sameOrigin(request)) { request->send(403, "text/plain", "Cross-origin login rejected"); return; }
      const uint32_t now = millis();
      if (failures_ >= 5 && static_cast<uint32_t>(now - failedAt_) < 15000UL) {
        request->send(429, "text/plain", "Too many login attempts; retry in 15 seconds"); return;
      }
      if (!accessConfig.verify(request->arg("username").c_str(), request->arg("password").c_str())) {
        if (static_cast<uint32_t>(now - failedAt_) >= 15000UL) failures_ = 0;
        ++failures_; failedAt_ = now; request->send(401, "text/plain", "Incorrect username or password"); return;
      }
      failures_ = 0;
      uint8_t bytes[16]; char token[33];
      if (os_get_random(bytes, sizeof(bytes)) != 0) { request->send(503, "text/plain", "Session entropy unavailable"); return; }
      const char hex[] = "0123456789abcdef";
      for (size_t i = 0; i < sizeof(bytes); ++i) { token[i * 2] = hex[bytes[i] >> 4]; token[i * 2 + 1] = hex[bytes[i] & 15]; }
      token[32] = 0;
      if (!sessions_.add(token, now)) { request->send(503, "text/plain", "Session limit reached; sign out another client"); return; }
      auto response = request->beginResponse(200, "application/json", "{\"authenticated\":true}");
      response->addHeader("Set-Cookie", String(F("PakarSession=")) + token + F("; Path=/; HttpOnly; SameSite=Strict; Max-Age=1800"));
      response->addHeader("Cache-Control", "no-store"); request->send(response);
    });
    server.on("/logout", HTTP_POST, [this, credentialsChanged](AsyncWebServerRequest *request) {
      sessions_.revoke(cookieToken(request).c_str());
      credentialsChanged(); // close established transports; other sessions may reconnect
      auto response = request->beginResponse(200, "application/json", "{\"signed_out\":true}");
      response->addHeader("Set-Cookie", "PakarSession=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
      response->addHeader("Cache-Control", "no-store"); request->send(response);
    });
    server.on("/api/auth", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "application/json", String(F("{\"username\":\"")) + accessConfig.username() + F("\",\"password_set\":true}"));
    });
    server.on("/api/auth", HTTP_POST, [this, credentialsChanged, busy](AsyncWebServerRequest *request) {
      if (busy()) { request->send(409, "text/plain", "Finish the current firmware operation before changing credentials"); return; }
      if (!accessConfig.change(request->arg("current_password"), request->arg("username"), request->arg("password"))) {
        request->send(400, "text/plain", accessConfig.error()); return;
      }
      revokeAll(); credentialsChanged();
      request->send(200, "application/json", "{\"updated\":true,\"reauthenticate\":true}");
    });
  }
 private:
  static String cookieToken(AsyncWebServerRequest *request) {
    if (!request->hasHeader("Cookie")) return String();
    String cookies = request->getHeader("Cookie")->value();
    while (cookies.length()) {
      const int separator = cookies.indexOf(';');
      String item = separator < 0 ? cookies : cookies.substring(0, separator); item.trim();
      if (item.startsWith(F("PakarSession="))) return item.substring(13);
      if (separator < 0) break;
      cookies.remove(0, separator + 1);
    }
    return String();
  }
  AsyncAuthenticationMiddleware basic_;
  String boardId_, bootId_;
  AsyncMiddlewareFunction middleware_;
  BoardCore::Access::Sessions sessions_;
  uint8_t failures_ = 0;
  uint32_t failedAt_ = 0;
  struct Socket { uint32_t id = 0; bool basic = false; char token[33] = {}; } sockets_[8];
};
