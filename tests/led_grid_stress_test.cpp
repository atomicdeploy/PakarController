#include "LedGrid.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <vector>

// Actual production renderer with host clock/FastLED doubles. These timings are
// desktop CPU costs, NOT ESP throughput, Wi-Fi latency or physical LED FPS.
int main(){
 using namespace BoardCore::Protocol;
 using Clock=std::chrono::steady_clock;
 constexpr unsigned iterations=10000;
 for(bool grouped:{true,false}){
  LedGrid grid;grid.begin();grid.setStaticMode(grouped);
  BoardCore::CellBatch selection;assert(BoardCore::CellBatch::parse("1-80",4,selection));
  std::vector<double> samples;samples.reserve(iterations);
  const auto started=Clock::now();
  for(unsigned round=0;round<iterations;++round){
   const auto at=Clock::now();
   const auto state=round%2?kCellWhite:kCellRed;
   assert(grid.setCells(selection,state,kEffectFade));
   pixelTestNow+=150;grid.service(pixelTestNow);
   if(!(round%64)){
    const auto frame=grid.pixelFrameJson();const auto rgb=frame.find("\"rgb\":\"")+7;
    for(unsigned p=0;p<LedGrid::kPixels;++p){
     const auto value=frame.substr(rgb+p*6,6);
     assert(value.substr(0,2)=="ff" && value.substr(2,2)==value.substr(4,2));
     if(grouped)assert(value==(state==kCellRed?"ff0000":"ffffff"));
    }
   }
   pixelTestNow+=150;grid.service(pixelTestNow);
   for(uint8_t id=1;id<=80;++id)assert(grid.state(id)==state);
   samples.push_back(std::chrono::duration<double,std::micro>(Clock::now()-at).count());
  }
  const double elapsed=std::chrono::duration<double,std::milli>(Clock::now()-started).count();
  std::sort(samples.begin(),samples.end());
  std::printf("{\"scope\":\"host-renderer-doubles-not-device-FPS\",\"mode\":\"%s\",\"batches\":%u,\"cells_per_batch\":80,\"elapsed_ms\":%.3f,\"batch_us\":{\"p50\":%.3f,\"p95\":%.3f,\"p99\":%.3f},\"errors\":0}\n",
   grouped?"static":"effects",iterations,elapsed,samples[iterations/2],samples[iterations*95/100],samples[iterations*99/100]);
 }
}
