#include "../firmware/esp8266/src/FirmwareReadResponse.h"
#include <cassert>
#include <cstdio>
#include <vector>
uint32_t testNow=0;
using namespace FirmwareExport;
struct Lease { explicit Lease(unsigned &active):active_(active){++active_;} ~Lease(){--active_;} unsigned &active_; };
int main(){
  unsigned readers=0;
  uint8_t output[8]{};
  std::vector<uint32_t> addresses;
  unsigned calls=0;
  {
    auto lease=std::make_shared<Lease>(readers);
    auto response=std::make_unique<ReadResponse>(100,10,
      [&](uint32_t address,uint8_t *bytes,size_t length){
        addresses.push_back(address);
        if(++calls==1) return ReadResult::Retryable;
        for(size_t i=0;i<length;++i) bytes[i]=static_cast<uint8_t>(address+i);
        return ReadResult::Complete;
      },lease);
    lease.reset();
    assert(readers==1 && response->_sourceValid());
    assert(response->_fillBuffer(output,8)==RESPONSE_TRY_AGAIN);
    assert(response->_sourceValid());
    assert(response->_fillBuffer(output,8)==8 && output[0]==100);
    assert(response->_fillBuffer(output,8)==2 && output[0]==108);
    assert(response->_fillBuffer(output,8)==0);
    assert((addresses==std::vector<uint32_t>{100,100,108}));
    assert(readers==1); // completion/cancellation retain lease until SDK destruction
  }
  assert(readers==0);
  {
    auto response=std::make_unique<ReadResponse>(7,8,
      [](uint32_t address,uint8_t *,size_t){assert(address==7);return ReadResult::Retryable;},
      std::make_shared<Lease>(readers));
    for(unsigned i=0;i<ReadWindow::kMaximumFailures;++i){
      assert(response->_sourceValid());
      assert(response->_fillBuffer(output,8)==RESPONSE_TRY_AGAIN);
    }
    assert(!response->_sourceValid()); // real SDK sets RESPONSE_FAILED + closes, never EOF
    assert(response->_fillBuffer(output,8)==RESPONSE_TRY_AGAIN);
  }
  assert(readers==0);
  {
    ReadResponse response(0,8,[](uint32_t,uint8_t *,size_t){return ReadResult::Fatal;},std::make_shared<Lease>(readers));
    assert(response._fillBuffer(output,8)==RESPONSE_TRY_AGAIN);
    assert(!response._sourceValid());
  }
  {
    testNow=UINT32_MAX-20;
    ReadResponse response(0,8,[](uint32_t,uint8_t *,size_t){return ReadResult::Retryable;},std::make_shared<Lease>(readers));
    assert(response._fillBuffer(output,8)==RESPONSE_TRY_AGAIN && response._sourceValid());
    testNow+=ReadWindow::kFailureWindowMs;
    assert(response._fillBuffer(output,8)==RESPONSE_TRY_AGAIN && !response._sourceValid());
  }
  {
    ReadResponse response(0,8,[](uint32_t,uint8_t *,size_t){return ReadResult::Complete;},{});
    assert(!response._sourceValid()); // no admitted lease means no SDK payload read
  }
  assert(readers==0);
  puts("Production firmware response: same-offset transient retry, bounded abort, rollover and SDK-owned reader lease fixtures passed.");
}
