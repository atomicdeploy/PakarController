#pragma once
#include "UpgradeLifecycle.h"

namespace PakarUpdate {
// One volatile latest notice, never an on-flash spool or a command channel.
// Adapt the shared finite stage/result lifecycle, not async OTA callbacks.
class NtfyNotice {
 public:
  struct Notice {
    Source source=Source::None; uint32_t operation=0;
    char phase[32]{}; int16_t percent=-1; bool verified=false;
  };
  void clear() { pending_=seen_=false; }
  bool observe(const Lifecycle &l, uint32_t now) {
    if (!l.operation() || l.source()==Source::None) return false;
    const bool terminal=!strcmp(l.phase(),"complete") || !strcmp(l.phase(),"failed") ||
      !strcmp(l.phase(),"aborted") || !strcmp(l.phase(),"cancelled");
    const int16_t step=l.known()?l.percent()/25*25:-1;
    const bool same=seen_ && last_.source==l.source() && last_.operation==l.operation();
    const int16_t previousStep=last_.percent<0?-1:last_.percent/25*25;
    if (same && !strcmp(last_.phase,l.phase()) && previousStep==step && last_.verified==l.verified()) return false;
    // Never announce an unverified 'complete' as success.
    if (terminal && !strcmp(l.phase(),"complete") && !l.verified()) return false;
    if (same && !terminal && strcmp(l.phase(),"awaiting_reboot") && strcmp(l.phase(),"ready-to-reboot") &&
        uint32_t(now-observedAt_)<30000U) return false;
    last_.source=l.source();last_.operation=l.operation();
    strcpy(last_.phase,l.phase());last_.percent=l.known()?l.percent():-1;last_.verified=l.verified();
    observedAt_=now;seen_=pending_=true;return true;
  }
  bool take(Notice &notice,uint32_t now) {
    if (!pending_) return false;
    pending_=false;
    if (uint32_t(now-observedAt_)>300000U) return false;
    notice=last_;return true;
  }
  bool pending() const { return pending_; }
 private:
  Notice last_;uint32_t observedAt_=0;bool seen_=false,pending_=false;
};
}
