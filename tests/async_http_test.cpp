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
  requests[3]->disconnect();
  for (int i = 0; i < 8; ++i){server.service();for(const auto&r:requests)if(r)r->connection.poll();}
  assert(called == 6 && !server.processing());
  assert(requests[0]->status == 200 && requests[3]->status == 0);
  server.on("/disconnect", HTTP_GET, [&]() {
    server.request()->disconnect(); server.send(200, "text/plain", "not sent");
  });
  auto disconnected = std::make_shared<AsyncWebServerRequest>();
  server.native().handlers["/disconnect"](disconnected.get()); server.service();
  assert(disconnected->status == 0 && !server.processing());
  auto request = std::make_shared<AsyncWebServerRequest>();
  server.native().handlers["/hardware"](request.get()); server.service();assert(request->status==0);request->connection.poll();assert(called == 7 && request->status==200);
  server.on("/", HTTP_GET, [&]() { server.send_P(200, "text/html", "embedded UI"); });
  auto root = std::make_shared<AsyncWebServerRequest>();
  server.native().handlers["/"](root.get()); server.service();assert(root->status==0);root->connection.poll();assert(root->status == 200 && root->flashResponse);
  // Deterministic old failure: service ran after callback assignment but
  // before pause returned/published its weak request, clearing that callback.
  unsigned publication=0;
  server.on("/publish",HTTP_GET,[&](){++publication;server.send(200,"text/plain","OK");});
  auto publishing=std::make_shared<AsyncWebServerRequest>();publishing->onPause=[&](){server.service();};
  server.native().handlers.at("/publish")(publishing.get());server.service();publishing->connection.poll();
  assert(publication==1 && publishing->status==200);
  // Loop response publication occurs while the actual SDK-model middleware
  // callback has not returned. Its outer _send tail must still see paused.
  // The previous implementation called request.send from this other thread,
  // violating the strict SDK actor assertion and racing the outer _send tail.
  auto reentry=std::make_shared<AsyncWebServerRequest>();
  reentry->afterHandler=[&](){std::thread loop([&](){server.service();});loop.join();assert(reentry->responseStarts==0 && reentry->paused);};
  server.native().handlers.at("/publish")(reentry.get());
  assert(reentry->status==0 && reentry->responseStarts==0);
  reentry->connection.poll();assert(reentry->status==200 && reentry->responseStarts==1);
  auto *active=reentry->getResponse();assert(active);
  reentry->connection.poll();assert(active->polls==1); // Preserve original SDK _onPoll backpressure progress.
  {AsyncSdkFixture::Actor actor;active->_ack(reentry.get(),4,1);} // ACK stays on the same SDK actor.
  assert(active->polls==2 && reentry->responseStarts==1);
  // Disconnect after loop publication destroys the unsent response exactly
  // once and prevents actor-side send; response construction alone is no ACK.
  auto cancelled=std::make_shared<AsyncWebServerRequest>();
  server.native().handlers.at("/publish")(cancelled.get());
  const auto beforeResponses=AsyncSdkFixture::liveResponses.load();server.service();
  assert(AsyncSdkFixture::liveResponses==beforeResponses+1 && cancelled->status==0);
  cancelled->disconnect();assert(AsyncSdkFixture::liveResponses==beforeResponses);
  cancelled->connection.poll();assert(cancelled->status==0 && cancelled->responseStarts==0);
  server.on("/duplicate",HTTP_GET,[&](){assert(server.remoteAddress()=="192.0.2.10");server.send(200,"text/plain","first");server.send(new AsyncWebServerResponse(201,"second"));});
  auto duplicate=std::make_shared<AsyncWebServerRequest>();server.native().handlers.at("/duplicate")(duplicate.get());
  server.service();duplicate->connection.poll();assert(duplicate->status==200 && duplicate->body=="first" && duplicate->responseStarts==1);
  // Deterministic starvation regression: refill the first free slot before
  // every dispatch while seven older requests are already waiting. A fixed
  // slot-zero scan serves newcomers forever; round-robin must serve the old
  // requests within the eight-slot bound, preserving weak ownership.
  {
    AsyncHttpService fair(80);
    std::vector<AsyncWebServerRequest*> order;
    fair.on("/fair",HTTP_GET,[&](){order.push_back(fair.request());fair.send(200,"text/plain","OK");});
    std::vector<std::shared_ptr<AsyncWebServerRequest>> older,newer;
    for(unsigned i=0;i<8;++i){older.push_back(std::make_shared<AsyncWebServerRequest>());fair.native().handlers.at("/fair")(older.back().get());}
    fair.service();older[0]->connection.poll();assert(order.size()==1 && order[0]==older[0].get());
    for(unsigned i=1;i<8;++i){
      newer.push_back(std::make_shared<AsyncWebServerRequest>());
      fair.native().handlers.at("/fair")(newer.back().get());
      fair.service();older[i]->connection.poll();assert(order.size()==i+1 && order[i]==older[i].get());
    }
    for(const auto&r:older)assert(r->status==200);
    for(unsigned i=0;i<8;++i)fair.service();
    for(const auto&r:newer)r->connection.poll();
    for(const auto&r:newer)assert(r->status==200);
    assert(order.size()==15 && !fair.processing());
  }
  constexpr unsigned count=20000;unsigned served=0;
  server.on("/parallel",HTTP_GET,[&](){++served;assert(server.processing());server.service();server.send(200,"text/plain","OK");});
  std::vector<std::shared_ptr<AsyncWebServerRequest>> parallel;
  for(unsigned i=0;i<count;++i)parallel.push_back(std::make_shared<AsyncWebServerRequest>());
  std::atomic<bool> done{false};
  std::thread producer([&](){for(const auto&r:parallel){server.native().handlers.at("/parallel")(r.get());r->connection.poll();std::this_thread::yield();}done.store(true,std::memory_order_release);});
  while(!done.load(std::memory_order_acquire)){server.service();std::this_thread::yield();}
  producer.join();for(unsigned i=0;i<8;++i)server.service();for(const auto&r:parallel)r->connection.poll();
  unsigned accepted=0,rejected=0;
  for(const auto&r:parallel){if(r->status==200)++accepted;else if(r->status==503)++rejected;else assert(false);}
  assert(accepted==served && accepted+rejected==count && !server.processing());
  puts("Production async queue: SDK actor-only response publication with real outer-send tail model, poll/ACK progression, cancelled/duplicate response ownership, immutable peer address, bounded fairness, weak lifetime, reentrancy,20000 concurrent publications and exact accounting passed (transport doubles, not hardware soak).");
}
