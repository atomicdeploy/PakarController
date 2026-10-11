// Executes the actual WebAccess middleware with transport/auth/SDK doubles.
// Adapt PCController session's admission/identity mismatch assertions; no
// actual ESP/AVR, network, Basic-auth parsing or physical ISP success claimed.
#include "WebAccess.h"
#include "AsyncHttpService.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>

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
      request.headers["X-Pakar-Boot"].text = boot;
      request.sent = false; request.status = 0;
      dispatch(request);
      assert(admitted == before + 1 && request.status == 0);
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
    request.headers.clear(); request.headers["X-Pakar-Boot"].text = boot;
    dispatch(request); assert(request.status == 409 && admitted == before);
    request.headers["X-Pakar-Board"].text = "esp8266-aabbccddee00";
    dispatch(request); assert(request.status == 409 && admitted == before);
    request.headers.clear(); request.sent = false; request.status = 0;
    dispatch(request); assert(admitted == before + 1); // Explicitly unfenced legacy.
  }
  // Origin/auth still precede ownership failure; do not leak identity to an
  // unauthenticated caller or let matching epochs grant authorization.
  AsyncWebServerRequest denied;
  denied.headers["X-Pakar-Board"].text = board; denied.headers["X-Pakar-Boot"].text = otherBoot;
  denied.basicAllowed = false; unsigned before = admitted; dispatch(denied);
  assert(denied.status == 401 && admitted == before);
  denied.basicAllowed = true; denied.headers["Origin"].text = "http://other.local";
  dispatch(denied); assert(denied.status == 403 && admitted == before);
  denied.headers.clear(); denied.path = "/api/v1/status";
  dispatch(denied); assert(denied.status == 404 && admitted == before);
  denied.path = "/api/auth/info"; denied.basicAllowed = false;
  dispatch(denied); assert(admitted == before + 1); // Public sign-in bootstrap.
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
  assert(queued->status == 409 && hardwareCalls == 0);
  auto current = std::make_shared<AsyncWebServerRequest>();
  current->headers["X-Pakar-Board"].text = board; current->headers["X-Pakar-Boot"].text = otherBoot;
  access.middleware()->run(current.get(), [&]() { server.native().handlers["/api/avr/probe"](current.get()); });
  server.service(); server.service();
  assert(current->status == 200 && hardwareCalls == 1);
  puts("Actual WebAccess all-method epoch rejection, auth/origin ordering, explicit legacy and async admission/dispatch checks passed (transport doubles, not physical ISP).");
}
