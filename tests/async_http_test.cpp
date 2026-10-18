#include "AsyncHttpService.h"
#include <assert.h>
#include <stdio.h>
#include <thread>
#include <atomic>
#include <vector>
int main() {
  {
    AsyncHttpService fenced(80);unsigned effects=0;
    fenced.on("/mutate",HTTP_POST,[&](){++effects;fenced.send(200,"text/plain","done");});
    auto enqueue=[&](){auto r=std::make_shared<AsyncWebServerRequest>();r->path="/mutate";r->requestMethod=HTTP_POST;
      assert(fenced.native().dispatch(r.get()));return r;};
    auto before=enqueue();const auto epoch=fenced.ingressEpoch();
    fenced.setMaintenanceFence(true);assert(fenced.ingressEpoch()!=epoch);
    fenced.service();before->connection.poll();assert(before->status==409 && !effects);
    auto during=enqueue();fenced.setMaintenanceFence(false);
    fenced.service();during->connection.poll();assert(during->status==409 && !effects);
    auto after=enqueue();fenced.service();after->connection.poll();assert(after->status==200 && effects==1);
  }
  // Reproduce installed SDK3.12.1's actual bare-string/first-registration
  // collision, rather than a map lookup that always selects the exact key.
  {
    AsyncWebServer sdk(80);
    sdk.on("/api/avr/contracts",HTTP_GET,[](AsyncWebServerRequest*r){r->send(200,"text/plain","metadata");});
    sdk.on("/api/avr/contracts/result",HTTP_GET,[](AsyncWebServerRequest*r){r->send(200,"text/plain","receipt");});
    auto request=std::make_shared<AsyncWebServerRequest>();request->path="/api/avr/contracts/result";
    request->arguments["id"]="2";
    assert(sdk.dispatch(request.get()) && request->body=="metadata");
  }
  {
    AsyncHttpService exact(80);unsigned parentReads=0,parentWrites=0,childReads=0,childDeletes=0,rootReads=0;
    // Parents intentionally registered before descendants, as production is.
    exact.on("/",HTTP_GET,[&](){++rootReads;exact.send(200,"text/plain","root");});
    exact.on("/api/avr/contracts",HTTP_GET,[&](){++parentReads;exact.send(200,"text/plain","metadata");});
    exact.on("/api/avr/contracts",HTTP_POST,[&](){++parentWrites;exact.send(202,"text/plain","enqueued");});
    exact.on("/api/avr/contracts/result",HTTP_GET,[&](){++childReads;assert(exact.arg("id")=="2");exact.send(200,"text/plain","receipt2");});
    exact.on("/api/avr/contracts/result",HTTP_DELETE,[&](){++childDeletes;exact.send(200,"text/plain","deleted2");});
    auto dispatch=[&](const char*path,int method,const char*body,int status=200){
      auto request=std::make_shared<AsyncWebServerRequest>();request->path=path;request->requestMethod=method;request->arguments["id"]="2";
      assert(exact.native().dispatch(request.get()));exact.service();request->connection.poll();
      assert(request->status==status && request->body==body && request->responseStarts==1);
    };
    dispatch("/api/avr/contracts/result",HTTP_GET,"receipt2");
    dispatch("/api/avr/contracts",HTTP_GET,"metadata");
    dispatch("/api/avr/contracts",HTTP_POST,"enqueued",202);
    dispatch("/api/avr/contracts/result",HTTP_DELETE,"deleted2");
    dispatch("/",HTTP_GET,"root");
    for(const char*path:{"/api/avr/contracts/","/api/avr/contracts/result/extra","/api/avr/contracts2"}){
      auto request=std::make_shared<AsyncWebServerRequest>();request->path=path;
      assert(!exact.native().dispatch(request.get()) && request->status==404);
    }
    auto unsupported=std::make_shared<AsyncWebServerRequest>();unsupported->path="/api/avr/contracts/result";unsupported->requestMethod=HTTP_POST;
    assert(!exact.native().dispatch(unsupported.get()) && unsupported->status==404);
    assert(parentReads==1 && parentWrites==1 && childReads==1 && childDeletes==1 && rootReads==1);
  }
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
  // Larger than the installed S3 TCP send window (5744 bytes): use actual
  // callback factory and independently owned immutable content across polls.
  // The transport double bounds fills/ACKs; it is not an LwIP hardware test.
  String large;for(unsigned i=0;i<16384;++i)large+=static_cast<char>('A'+i%26);
  const String expectedLarge=large;
  server.on("/large",HTTP_GET,[&](){auto *response=server.beginResponse(206,"application/json",large);response->addHeader("Cache-Control","no-store");server.send(response);});
  auto largeRequest=std::make_shared<AsyncWebServerRequest>();
  server.native().handlers.at("/large")(largeRequest.get());server.service();large="changed after snapshot";
  assert(largeRequest->status==0);largeRequest->connection.poll();
  assert(largeRequest->status==206 && largeRequest->getResponse()->filler && largeRequest->body.size()==1024);
  assert(largeRequest->responseHeaders.at("Cache-Control")=="no-store");
  largeRequest->connection.writable=false;largeRequest->connection.poll();assert(largeRequest->body.size()==1024);
  {AsyncSdkFixture::Actor actor;largeRequest->getResponse()->_ack(largeRequest.get(),1024,1);}assert(largeRequest->body.size()==1024);
  largeRequest->connection.writable=true;largeRequest->connection.chunkLimit=333;
  while(largeRequest->body.size()<expectedLarge.size()){
    largeRequest->connection.poll();
    AsyncSdkFixture::Actor actor;largeRequest->getResponse()->_ack(largeRequest.get(),333,1);
  }
  assert(largeRequest->body==expectedLarge && largeRequest->responseStarts==1);
  const auto completed=largeRequest->body;largeRequest->connection.poll();assert(largeRequest->body==completed);
  auto *largeResponse=largeRequest->getResponse();uint8_t tail[4];
  assert(largeResponse->filler(tail,sizeof(tail),expectedLarge.length())==0);
  assert(largeResponse->filler(tail,0,0)==0);
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
  puts("Production async queue: SDK first-match descendant collision reproduced and literal routes exact; actor-only responses, outer-send tail, poll/ACK progression, response ownership, immutable peer address, bounded fairness, weak lifetime, reentrancy,20000 concurrent publications and exact accounting passed (transport doubles, not hardware soak).");
}
