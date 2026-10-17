#pragma once

namespace PakarS3 {
// Owned by LedOutput: wait for a complete frame boundary before flash can
// suppress the SDK's non-cache-safe GDMA refill interrupt. A successful acquire
// has exactly one release, including nested/multi-writer acquisitions. Before
// the LED worker exists, these hooks must be safe no-ops. Neither hook may write
// flash; acquire must fail rather than wait on its own worker or a dead worker.
bool acquireLedFlashPause();
void releaseLedFlashPause();
// Reject contexts where waiting is unsafe before entering the worker hook.
bool acquireTaskLedFlashPause();
class ScopedLedFlashPause {
 public:
  ScopedLedFlashPause():held_(acquireTaskLedFlashPause()){}
  ~ScopedLedFlashPause(){if(held_)releaseLedFlashPause();}
  explicit operator bool()const{return held_;}
  ScopedLedFlashPause(const ScopedLedFlashPause&)=delete;
  ScopedLedFlashPause& operator=(const ScopedLedFlashPause&)=delete;
 private:
  bool held_;
};
}
