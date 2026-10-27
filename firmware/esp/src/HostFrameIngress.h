#pragma once
#include <Protocol.h>
#include <stddef.h>
#include <stdint.h>

#ifndef PAKAR_HOST_FRAME_CAPACITY
#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
#define PAKAR_HOST_FRAME_CAPACITY 2
#else
#define PAKAR_HOST_FRAME_CAPACITY 8
#endif
#endif

// ESP host ingress only. Adapt SerialLink's owned complete COBS/CRC frame and
// AsyncHttpService/LoopMessages' bounded loop-owner publication discipline.
// No serial handle, worker task, heap allocation or acceptance ACK lives here.
class HostFrameIngress {
 public:
  using Frame=BoardCore::Protocol::Frame;
  using Error=BoardCore::Protocol::Error;
  using Dispatch=void (*)(const Frame&,void*);
  using Reject=void (*)(const Frame&,Error,void*);
  using SessionBoundary=void (*)(void*);
  static constexpr size_t capacity=PAKAR_HOST_FRAME_CAPACITY;
  static constexpr uint32_t maximumAgeMs=1000;
  static_assert(capacity>=1 && capacity<=8,"USB ingress capacity outside1..8");

  void begin(Dispatch dispatch,Reject reject,SessionBoundary session,void* context=nullptr) {
    dispatch_=dispatch;reject_=reject;session_=session;context_=context;
    head_=count_=0;dispatching_=false;fenceKnown_=false;
  }

  // Observe loop-owned maintenance boundaries before consuming more input.
  // The existing HTTP epoch also catches a complete fenced operation that
  // entered and left between two host service calls.
  void observeFence(uint32_t epoch,bool maintenance) {
    if(fenceKnown_ && (epoch_!=epoch || maintenance_!=maintenance))discard();
    epoch_=epoch;maintenance_=maintenance;fenceKnown_=true;
  }

  void receive(const Frame& frame,uint32_t now,bool deferred) {
    if(frame.payloadLength>BoardCore::Protocol::kMaximumPayload) {
      reject(frame,BoardCore::Protocol::kBadPayload);return;
    }
    // A new HELLO is a session boundary, not authorization inherited from the
    // old caller. Its eventual dispatch retains onHostFrame's real admission.
    if(frame.opcode==BoardCore::Protocol::kHello) {
      // Revoke now, even if this HELLO later expires before dispatch. Otherwise
      // a following fresh frame could inherit the previous caller's admission.
      if(session_)session_(context_);
      discard();
    }
    if(!deferred && !dispatching_ && !count_) {
      dispatch(frame);return;
    }
    if(count_==capacity) {reject(frame,BoardCore::Protocol::kBusy);return;}
    auto& entry=entries_[(head_+count_)%capacity];
    entry.frame=frame;entry.receivedAt=now;++count_;
  }

  // At most one item per call. Remove before dispatch, so reentrant reception
  // can only append and no nested service can apply the same frame twice.
  bool service(uint32_t now,bool deferred) {
    if(deferred || dispatching_ || !count_)return false;
    const Entry entry=take();
    if(uint32_t(now-entry.receivedAt)>=maximumAgeMs)
      reject(entry.frame,BoardCore::Protocol::kUnsafe);
    else dispatch(entry.frame);
    return true;
  }

  void discard() {
    const size_t pending=count_;
    for(size_t i=0;i<pending;++i) {
      const Entry entry=take();
      // An error for an obsolete HELLO could alias the new caller's HELLO
      // sequence and poison its handshake. It grants no mutation authority.
      if(entry.frame.opcode!=BoardCore::Protocol::kHello)
        reject(entry.frame,BoardCore::Protocol::kUnsafe);
    }
  }

  size_t pending()const{return count_;}

 private:
  struct Entry {Frame frame{};uint32_t receivedAt=0;};
  Entry take() {
    const Entry entry=entries_[head_];
    head_=(head_+1)%capacity;--count_;return entry;
  }
  void dispatch(const Frame& frame) {
    if(!dispatch_) {reject(frame,BoardCore::Protocol::kUnsafe);return;}
    dispatching_=true;dispatch_(frame,context_);dispatching_=false;
  }
  void reject(const Frame& frame,Error error) {
    if(reject_)reject_(frame,error,context_);
  }
  Entry entries_[capacity]{};
  Dispatch dispatch_=nullptr;Reject reject_=nullptr;SessionBoundary session_=nullptr;void* context_=nullptr;
  size_t head_=0,count_=0;
  uint32_t epoch_=0;bool dispatching_=false,fenceKnown_=false,maintenance_=false;
};
