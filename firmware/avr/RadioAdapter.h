#pragma once
#include <BoardFeatures.h>
#include <Protocol.h>
#if PAKAR_PC_RF
#include <RCSwitch.h>
#include <util/atomic.h>

// Adapted from PCController Project/Runtime/RadioRuntime.inc.h:
// serviceRadio/transmitRadio. Raw events retain its typed wire representation;
// no unlearned radio code is granted permission to reset or operate outputs.
class RadioAdapter {
 public:
  void begin(uint8_t txPin,int rxInterrupt) {
    rxInterrupt_=rxInterrupt;
    radio_.enableTransmit(txPin);
    radio_.setRepeatTransmit(3);
    radio_.enableReceive(rxInterrupt_);
  }
  template<class Sink> void service(Sink &sink) {
    uint32_t code=0; uint8_t bits=0, protocol=0; uint16_t pulse=0;
    // AVR multi-byte reads must be one coherent ISR snapshot. Do not detach
    // and reattach INT0 on every loop; that loses in-flight receive timings.
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      if(radio_.available()) {
        code=radio_.getReceivedValue(); bits=radio_.getReceivedBitlength();
        protocol=radio_.getReceivedProtocol(); pulse=radio_.getReceivedDelay();
        radio_.resetAvailable();
      }
    }
    // Capture is observational, including during Prog. No learning, repeat
    // suppression or action policy lives on AVR. Preserve decoder metadata;
    // the transmit protocol whitelist must not filter received evidence.
    if(code && bits && bits<=32 && protocol)
      sink.radioReceived(code,bits,protocol,pulse);
  }
  BoardCore::Protocol::Error transmit(const uint8_t *p,uint8_t n,bool activeRound) {
    using namespace BoardCore::Protocol;
    if(n!=8)return kBadPayload;
    const uint32_t code=static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1])<<8 |
        static_cast<uint32_t>(p[2])<<16 | static_cast<uint32_t>(p[3])<<24;
    const uint16_t pulse=p[6] | static_cast<uint16_t>(p[7])<<8;
    if(!code || !p[4] || p[4]>32 || !p[5] || p[5]>12 ||
        (pulse && (pulse<50 || pulse>1000)))return kBadPayload;
    // rc-switch TX is synchronous. Never stall an armed first-press race.
    // Explicit Busy is safer than enqueueing a late, surprising RF action.
    if(activeRound)return kBusy;
    radio_.disableReceive();radio_.setProtocol(p[5]);
    if(pulse)radio_.setPulseLength(pulse);
    radio_.send(code,p[4]);
    radio_.enableReceive(rxInterrupt_);
    return kNoError;
  }
 private:
  RCSwitch radio_;
  int rxInterrupt_=-1;
};
#endif
