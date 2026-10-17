// Executes the actual WebAccess middleware with transport/auth/SDK doubles.
// Adapt PCController session's admission/identity mismatch assertions; no
// actual ESP/AVR, network, Basic-auth parsing or physical ISP success claimed.
#include "WebAccess.h"
#include "AsyncHttpService.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
#include <atomic>
#include <thread>

int main() {
  fs::FS filesystem;
  assert(accessConfig.begin(filesystem));
  WebAccess access;
  const char *board = "esp8266-aabbccddeeff";
  const char *boot = "123456789abcdef0123456789abcdef0";
  const char *otherBoot = "223456789abcdef0123456789abcdef0";
  access.configurePeerFence(board, boot);
  assert(access.peerFenceReady());
  unsigned admitted = 0;
  auto dispatch = [&](AsyncWebServerRequest &request) {
    access.middleware()->run(&request, [&]() { ++admitted; });
  };
  for (int method : {HTTP_GET, HTTP_HEAD, HTTP_POST, HTTP_PUT, HTTP_DELETE}) {
    for (const char *path : {"/api/avr/probe", "/api/status", "/api/avr/backup/flash", "/api/avr/image"}) {
      AsyncWebServerRequest request; request.requestMethod = method; request.path = path;
      request.headers["X-Pakar-Board"].text = board;
      request.headers["X-Pakar-Boot"].text = otherBoot;
      const unsigned before = admitted;
      dispatch(request);
      assert(request.status == 409 && admitted == before);
      assert(request.body == "{\"error\":\"peer_changed\",\"accepted\":false}");
      AsyncWebServerRequest valid; valid.requestMethod=method;valid.path=path;
      valid.headers["X-Pakar-Board"].text=board;valid.headers["X-Pakar-Boot"].text=boot;
      dispatch(valid);
      assert(admitted == before + 1 && valid.status == 0);
    }
  }
  for (int method : {HTTP_GET, HTTP_HEAD, HTTP_POST}) {
    for (const char *invalid : {"", "00000000000000000000000000000000", "123456789ABCDEF0123456789abcdef0"}) {
      AsyncWebServerRequest request; request.requestMethod = method;
      request.headers["X-Pakar-Board"].text = board;
      request.headers["X-Pakar-Boot"].text = invalid;
      unsigned before = admitted; dispatch(request);
      assert(request.status == 409 && admitted == before);
    }
    AsyncWebServerRequest request; request.requestMethod = method;
    request.headers["X-Pakar-Board"].text = board;
    unsigned before = admitted; dispatch(request);
    assert(request.status == 409 && admitted == before); // Partial pair.
    AsyncWebServerRequest partial;partial.requestMethod=method;
    partial.headers["X-Pakar-Boot"].text=boot;
    dispatch(partial);assert(partial.status==409 && admitted==before);
    AsyncWebServerRequest wrongBoard;wrongBoard.requestMethod=method;
    wrongBoard.headers["X-Pakar-Board"].text="esp8266-aabbccddee00";
    wrongBoard.headers["X-Pakar-Boot"].text=boot;
    dispatch(wrongBoard);assert(wrongBoard.status==409 && admitted==before);
    AsyncWebServerRequest legacy;legacy.requestMethod=method;
    dispatch(legacy);assert(legacy.status==0 && admitted==before+1); // Explicitly unfenced legacy.
  }
  // Origin/auth still precede ownership failure; do not leak identity to an
  // unauthenticated caller or let matching epochs grant authorization.
  AsyncWebServerRequest denied;
  denied.headers["X-Pakar-Board"].text = board; denied.headers["X-Pakar-Boot"].text = otherBoot;
  denied.basicAllowed = false; unsigned before = admitted; dispatch(denied);
  assert(denied.status == 401 && admitted == before);
  // Independent HTTP transactions need fresh SDK request/response ownership;
  // a request that has already sent401 cannot later send403 or404.
  AsyncWebServerRequest wrongOrigin;
  wrongOrigin.headers["X-Pakar-Board"].text=board;wrongOrigin.headers["X-Pakar-Boot"].text=otherBoot;
  wrongOrigin.headers["Origin"].text = "http://other.local";
  dispatch(wrongOrigin); assert(wrongOrigin.status == 403 && admitted == before);
  AsyncWebServerRequest numbered;
  numbered.path = "/api/v1/status";
  dispatch(numbered); assert(numbered.status == 404 && admitted == before);
  AsyncWebServerRequest bootstrap;
  bootstrap.path = "/api/auth/info"; bootstrap.basicAllowed = false;
  dispatch(bootstrap); assert(admitted == before + 1); // Public sign-in bootstrap.
  // Real async queue + real fence recheck: ownership can retire after enqueue,
  // and a handler must fail before the simulated hardware call, without replay.
  AsyncHttpService server(80); unsigned hardwareCalls = 0;
  server.on("/api/avr/probe", HTTP_GET, [&]() {
    if (!access.peerAllowed(server.request())) {
      server.send(409, "application/json", "{\"error\":\"peer_changed\",\"accepted\":false}"); return;
    }
    ++hardwareCalls; server.send(200, "application/json", "{\"signature\":\"fixture\"}");
  });
  auto queued = std::make_shared<AsyncWebServerRequest>();
  queued->headers["X-Pakar-Board"].text = board; queued->headers["X-Pakar-Boot"].text = boot;
  access.middleware()->run(queued.get(), [&]() { server.native().handlers["/api/avr/probe"](queued.get()); });
  assert(queued->paused && hardwareCalls == 0);
  access.configurePeerFence(board, otherBoot); server.service(); server.service();
  queued->connection.poll();
  assert(queued->status == 409 && hardwareCalls == 0);
  auto current = std::make_shared<AsyncWebServerRequest>();
  current->headers["X-Pakar-Board"].text = board; current->headers["X-Pakar-Boot"].text = otherBoot;
  access.middleware()->run(current.get(), [&]() { server.native().handlers["/api/avr/probe"](current.get()); });
  server.service(); server.service();
  current->connection.poll();
  assert(current->status == 200 && hardwareCalls == 1);
  // An immutable socket ID does not grant authorization after logout or a
  // credential reset. Admission is rechecked when a queued operation dispatches.
  AsyncWebServer routes(80); unsigned transportsClosed = 0;
  access.registerRoutes(routes, [&]() { ++transportsClosed; }, []() { return false; });
  AsyncWebServerRequest socketRequest;
  assert(!access.bindSocket(0, &socketRequest));
  assert(access.bindSocket(11, &socketRequest));
  assert(!access.bindSocket(11, &socketRequest));
  assert(access.socketAllowed(11));
  routes.handlers["/logout"](&socketRequest);
  assert(socketRequest.status == 200 && transportsClosed == 1);
  assert(!access.socketAllowed(11));
  assert(access.bindSocket(12, &socketRequest));
  access.revokeAll();
  assert(!access.socketAllowed(12));
  assert(access.bindSocket(13, &socketRequest));
  access.forgetSocket(13); assert(!access.socketAllowed(13));
  // Exercise actual shared pool from an async-callback producer and loop-side
  // reader/revoker. Each request double is immutable once the threads start.
  std::atomic<bool> producerDone{false};
  std::thread producer([&]() {
    for (uint32_t i = 1; i <= 20000; ++i) {
      access.bindSocket(i, &socketRequest);
      access.socketAllowed(i);
      access.forgetSocket(i);
    }
    producerDone.store(true, std::memory_order_release);
  });
  AsyncWebSocket websocket;
  while (!producerDone.load(std::memory_order_acquire)) {
    access.sessionCount(); access.expireSockets(websocket); access.revokeAll();
  }
  producer.join();
  access.revokeAll(); assert(!access.socketAllowed(20000));
  // Actual cookie-session route with deterministic SDK entropy/clock doubles.
  // Expiry invalidates admission BEFORE SDK close; its callback may reenter
  // access state without inverting the SDK client-list/access-state locks.
  pakarTestEntropyResult = 0; pakarTestMillis.store(100);
  auto login = [&]() {
    AsyncWebServerRequest request;
    request.arguments["username"] = "admin";
    request.arguments["password"] = "pakarcontroller";
    routes.handlers["/login"](&request);
    assert(request.status == 200);
    const String cookie = request.responseHeaders.at("Set-Cookie");
    assert(cookie.startsWith("PakarSession="));
    AsyncWebServerRequest holder;
    holder.basicAllowed = false;
    holder.headers["Cookie"].text = cookie.substring(0, 45);
    return holder;
  };
  auto expired = login();
  assert(access.allowed(&expired) && access.bindSocket(21, &expired));
  assert(access.sessionCount() == 1);
  websocket.onClose = [&](uint32_t id) {
    assert(!access.socketAllowed(id)); access.forgetSocket(id);
  };
  pakarTestMillis.store(100 + BoardCore::Access::Sessions::kIdleMs);
  access.expireSockets(websocket);
  assert(websocket.closed.size() == 1 && websocket.closed[0] == 21);
  assert(!access.allowed(&expired) && !access.socketAllowed(21) && access.sessionCount() == 0);
  auto signedOut = login();
  assert(access.bindSocket(22, &signedOut));
  unsigned queuedWrites = 0;
  auto queuedMutation = [&]() { if (access.socketAllowed(22)) ++queuedWrites; };
  routes.handlers["/logout"](&signedOut);
  queuedMutation();
  assert(queuedWrites == 0 && !access.allowed(&signedOut));
  auto revoked = login(); assert(access.bindSocket(23, &revoked));
  access.revokeAll();
  assert(!access.allowed(&revoked) && !access.socketAllowed(23));
  pakarTestEntropyResult = -1; pakarTestMillis.store(0);
  puts("Actual WebAccess all-method epoch rejection, auth/origin ordering, explicit legacy and async admission/dispatch checks passed (transport doubles, not physical ISP).");
}
