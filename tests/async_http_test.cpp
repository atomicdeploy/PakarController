#include "AsyncHttpService.h"
#include <assert.h>
#include <stdio.h>
#include <thread>
#include <atomic>
#include <vector>
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
  // Deterministic old failure: service ran after callback assignment but
  // before pause returned/published its weak request, clearing that callback.
  unsigned publication=0;
  server.on("/publish",HTTP_GET,[&](){++publication;server.send(200,"text/plain","OK");});
  auto publishing=std::make_shared<AsyncWebServerRequest>();publishing->onPause=[&](){server.service();};
  server.native().handlers.at("/publish")(publishing.get());server.service();
  assert(publication==1 && publishing->status==200);
  constexpr unsigned count=20000;unsigned served=0;
  server.on("/parallel",HTTP_GET,[&](){++served;assert(server.processing());server.service();server.send(200,"text/plain","OK");});
  std::vector<std::shared_ptr<AsyncWebServerRequest>> parallel;
  for(unsigned i=0;i<count;++i)parallel.push_back(std::make_shared<AsyncWebServerRequest>());
  std::atomic<bool> done{false};
  std::thread producer([&](){for(const auto&r:parallel){server.native().handlers.at("/parallel")(r.get());std::this_thread::yield();}done.store(true,std::memory_order_release);});
  while(!done.load(std::memory_order_acquire)){server.service();std::this_thread::yield();}
  producer.join();for(unsigned i=0;i<8;++i)server.service();
  unsigned accepted=0,rejected=0;
  for(const auto&r:parallel){if(r->status==200)++accepted;else if(r->status==503)++rejected;else assert(false);}
  assert(accepted==served && accepted+rejected==count && !server.processing());
  puts("Production async queue: bounds, weak lifetime, reentrancy, 20000 concurrent request publications and exact dispatch/rejection accounting passed (transport doubles, not hardware soak).");
}
