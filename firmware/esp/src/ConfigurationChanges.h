#pragma once
#include <stdint.h>

namespace PakarConfiguration {
enum Scope : uint16_t { Identity=1, Network=2, Cloud=4, Auth=8, Shell=16,
  Ethernet=32, StatusRgb=64, Bluetooth=128, LedOutput=256, NetworkTools=512 };
enum class Phase : uint8_t { Saved, Applied, Failed };
inline const char *scopeName(uint16_t scope) {
  switch(scope) {
    case Identity:return "identity";case Network:return "network";case Cloud:return "cloud";
    case Auth:return "auth";case Shell:return "shell";case Ethernet:return "ethernet";
    case StatusRgb:return "status-rgb";case Bluetooth:return "bluetooth";
    case LedOutput:return "led-output";
    case NetworkTools:return "network-tools";
  }
  return "";
}
inline const char *phaseName(Phase phase) {
  return phase==Phase::Saved?"saved":phase==Phase::Applied?"applied":"failed";
}
// Loop-owner only. Adapt PU ConfirmSettings' durable-save versus subsequent
// application distinction without its blocking delay or credential broadcasts.
// Store commits mark bits only; the loop coalesces and builds nonsecret events.
class Journal {
 public:
  struct Event { uint32_t revision=0;uint16_t scopes=0;Phase phase=Phase::Saved; };
  void saved(uint16_t scopes){pending_[0]|=scopes&1023U;}
  void applied(uint16_t scopes){pending_[1]|=scopes&1023U;}
  void failed(uint16_t scopes){pending_[2]|=scopes&1023U;}
  uint32_t revision()const{return revision_;}
  bool peek(Event &event)const {
    if(revision_==UINT32_MAX)return false; // Never wrap/reuse an event revision.
    for(uint8_t i=0;i<3;++i)if(pending_[i]) {
      event.revision=revision_+1;event.scopes=pending_[i];event.phase=static_cast<Phase>(i);return true;
    }
    return false;
  }
  bool retire(const Event &event) {
    const auto phase=static_cast<uint8_t>(event.phase);
    if(phase>2 || !event.revision || event.revision!=revision_+1 || !event.scopes ||
        (pending_[phase]&event.scopes)!=event.scopes)return false;
    pending_[phase]&=~event.scopes;revision_=event.revision;return true;
  }
  void reset(){revision_=0;for(auto &scope:pending_)scope=0;}
 private:
  uint32_t revision_=0;uint16_t pending_[3]{};
};
static_assert(sizeof(Journal)<=12,"Configuration notification queue must stay bounded");
inline Journal &changes(){static Journal value;return value;}
}
