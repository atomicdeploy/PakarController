#include "AsyncHttpService.h"
#include <assert.h>
#include <stdio.h>
int main() {
  AsyncHttpService server(80); int called = 0;
  server.on("/hardware", HTTP_GET, [&]() {
    ++called; server.service(); // a hardware handler may yield; don't replace current request
    server.send(200, "text/plain", "OK");
  });
  std::shared_ptr<AsyncWebServerRequest> requests[9];
  for (auto &request : requests) {
    request = std::make_shared<AsyncWebServerRequest>(); server.native().handlers["/hardware"](request.get());
  }
  assert(requests[8]->status == 503 && called == 0);
  requests[2].reset(); // disconnected queued continuation expires
  requests[3]->connection.alive = false;
  for (int i = 0; i < 8; ++i) server.service();
  assert(called == 6 && !server.processing());
  assert(requests[0]->status == 200 && requests[3]->status == 0);
  server.on("/disconnect", HTTP_GET, [&]() {
    server.request()->connection.alive = false; server.send(200, "text/plain", "not sent");
  });
  auto disconnected = std::make_shared<AsyncWebServerRequest>();
  server.native().handlers["/disconnect"](disconnected.get()); server.service();
  assert(disconnected->status == 0 && !server.processing());
  auto request = std::make_shared<AsyncWebServerRequest>();
  server.native().handlers["/hardware"](request.get()); server.service(); assert(called == 7);
  server.on("/", HTTP_GET, [&]() { server.send_P(200, "text/html", "embedded UI"); });
  auto root = std::make_shared<AsyncWebServerRequest>();
  server.native().handlers["/"](root.get()); server.service(); assert(root->status == 200 && root->flashResponse);
  puts("Production async dispatch queue bounds, disconnected weak requests, reentrancy and post-disconnect sends passed (transport test double).");
}
