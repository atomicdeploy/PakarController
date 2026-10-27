#include "../firmware/esp/src/HostFrameIngress.h"
#include <assert.h>
#include <stdio.h>
#include <vector>
using namespace BoardCore::Protocol;

struct Rejection {Frame frame;Error error;};
struct Fixture {
  HostFrameIngress ingress;
  std::vector<Frame> applied,dispatched;
  std::vector<Rejection> rejected;
  bool cloud=false,maintenance=false,reenter=false,admitted=true;
  uint32_t now=10;
  Fixture(){ingress.begin(dispatch,reject,revoke,this);ingress.observeFence(1,false);}
  static void revoke(void* context){static_cast<Fixture*>(context)->admitted=false;}
  static void reject(const Frame& frame,Error error,void* context) {
    static_cast<Fixture*>(context)->rejected.push_back({frame,error});
  }
  static void dispatch(const Frame& frame,void* context) {
    auto& f=*static_cast<Fixture*>(context);
    assert(!f.cloud);f.dispatched.push_back(frame);
    if(frame.opcode==kHello)f.admitted=true;
    else if(!f.admitted){reject(frame,kUnsupported,context);return;}
    // Stand-in for the UNCHANGED onHostFrame maintenance guard. The queue
    // schedules admission, not a bypass around real device/boot/state checks.
    if(f.maintenance){reject(frame,kBusy,context);return;}
    f.applied.push_back(frame);
    if(f.reenter) {
      f.reenter=false;
      Frame nested{};nested.opcode=kCellAction;nested.sequence=92;
      nested.payloadLength=2;nested.payload[0]=80;nested.payload[1]=kCellOff;
      f.ingress.receive(nested,f.now,false);
      assert(!f.ingress.service(f.now,false));
      assert(f.applied.size()==1);
    }
  }
  void receive(Frame frame){ingress.receive(frame,now,cloud);}
  void service(){ingress.service(now,cloud);}
};
Frame request(uint8_t opcode=kTableCommand,uint8_t sequence=1) {
  Frame frame{};frame.opcode=opcode;frame.sequence=sequence;
  frame.payloadLength=kMaximumPayload;
  for(size_t i=0;i<kMaximumPayload;++i)frame.payload[i]=uint8_t(i+sequence);
  return frame;
}
int main() {
  static_assert(sizeof(HostFrameIngress)<=600,"Host-only queue RAM budget regressed");
  static_assert(HostFrameIngress::maximumAgeMs==1000,"Must expire before native2s ACK deadline");
  {
    Fixture f;auto frame=request();f.receive(frame);
    assert(f.applied.size()==1&&f.rejected.empty()&&f.ingress.pending()==0);
    assert(!f.ingress.service(f.now,false));
  }
  {
    Fixture f;f.cloud=true;auto frame=request();const Frame original=frame;
    f.receive(frame);frame.payload[0]=0;
    f.service();assert(f.applied.empty()&&f.rejected.empty()&&f.ingress.pending()==1);
    f.cloud=false;f.now+=17;
    // A new arrival cannot overtake an older cloud-deferred frame.
    f.receive(request(kPing,2));assert(f.applied.empty()&&f.ingress.pending()==2);
    f.service();f.service();f.service();
    assert(f.applied.size()==2&&f.applied[0].sequence==1&&f.applied[1].sequence==2);
    assert(f.applied[0].payload[0]==original.payload[0]);
  }
  {
    Fixture f;f.cloud=true;
    for(size_t i=0;i<HostFrameIngress::capacity;++i)f.receive(request(kCellAction,uint8_t(i)));
    f.receive(request(kCellAction,99));
    assert(f.rejected.size()==1&&f.rejected[0].error==kBusy&&f.rejected[0].frame.sequence==99);
    assert(f.ingress.pending()==HostFrameIngress::capacity&&f.applied.empty());
    f.cloud=false;
    for(size_t i=0;i<HostFrameIngress::capacity;++i)f.service();
    assert(f.applied.size()==HostFrameIngress::capacity&&f.ingress.pending()==0);
  }
  {
    Fixture f;f.cloud=true;f.receive(request());f.cloud=false;
    f.now+=999;f.service();assert(f.applied.size()==1&&f.rejected.empty());
    f.cloud=true;f.receive(request());f.cloud=false;
    f.now+=1000;f.service();assert(f.applied.size()==1&&f.rejected.size()==1&&f.rejected[0].error==kUnsafe);
  }
  {
    Fixture f;f.cloud=true;f.now=UINT32_MAX-50U;f.receive(request());
    f.now+=1000U;f.cloud=false;f.service();
    assert(f.applied.empty()&&f.rejected.size()==1&&f.rejected[0].error==kUnsafe);
  }
  {
    Fixture f;f.cloud=true;f.receive(request(kCellAction,17));
    f.receive(request(kHello,18));
    assert(!f.admitted);
    assert(f.applied.empty()&&f.ingress.pending()==1&&f.rejected.size()==1);
    assert(f.rejected[0].frame.sequence==17&&f.rejected[0].error==kUnsafe);
    f.receive(request(kHello,19)); // Retire old HELLO without aliasing its error.
    assert(f.ingress.pending()==1&&f.rejected.size()==1);
    f.cloud=false;f.service();
    assert(f.applied.size()==1&&f.applied[0].opcode==kHello&&f.applied[0].sequence==19);
  }
  {
    Fixture f;f.cloud=true;f.receive(request(kHello,1));
    assert(!f.admitted);f.now+=750;f.receive(request(kCellAction,2));
    f.now+=251;f.cloud=false;f.service();f.service();
    assert(f.applied.empty()&&!f.admitted&&f.rejected.size()==2);
    assert(f.rejected[0].error==kUnsafe&&f.rejected[1].error==kUnsupported);
  }
  {
    Fixture f;f.reenter=true;f.receive(request(kCellAction,91));
    assert(f.applied.size()==1&&f.ingress.pending()==1);
    f.service();f.service();
    assert(f.applied.size()==2&&f.applied[1].sequence==92&&f.rejected.empty());
  }
  {
    Fixture f;f.cloud=true;f.receive(request(kCellAction,1));
    f.ingress.observeFence(2,true); // Entry boundary retires old commands.
    assert(f.ingress.pending()==0&&f.rejected.size()==1&&f.rejected[0].error==kUnsafe);
    f.cloud=false;f.maintenance=true;f.receive(request(kCellAction,2));
    assert(f.applied.empty()&&f.rejected.back().error==kBusy); // Real guard remains.
    f.cloud=true;f.receive(request(kCellAction,3));
    f.ingress.observeFence(3,false);f.cloud=false;f.maintenance=false;f.service();
    assert(f.applied.empty()&&f.ingress.pending()==0&&f.rejected.back().error==kUnsafe);
    f.cloud=true;f.receive(request(kCellAction,4));
    f.ingress.observeFence(5,false); // Complete fenced job between observations.
    assert(f.ingress.pending()==0&&f.rejected.back().error==kUnsafe);
  }
  {
    Fixture f;Frame invalid=request();invalid.payloadLength=kMaximumPayload+1;
    f.receive(invalid);assert(f.applied.empty()&&f.ingress.pending()==0);
    assert(f.rejected.size()==1&&f.rejected[0].error==kBadPayload);
  }
  puts("Actual USB ingress helper: owned bounded FIFO, cloud-quiescent ordered exactly-once dispatch,1000ms expiry/rollover, backpressure, HELLO retirement, reentrant deferral and maintenance fencing; host model, not UART/cloud/LED hardware proof.");
}
