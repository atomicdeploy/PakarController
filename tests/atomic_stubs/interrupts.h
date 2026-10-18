#pragma once
#include <mutex>
// Host-only serialization double. SDK compile/link separately checks the real
// ESP8266 interrupt guard; this does not emulate RF/interrupt timing.
namespace esp8266 {
inline std::recursive_mutex interruptMutex;
class InterruptLock {
 public:
  InterruptLock() { interruptMutex.lock(); }
  ~InterruptLock() { interruptMutex.unlock(); }
  InterruptLock(const InterruptLock&)=delete;
  InterruptLock& operator=(const InterruptLock&)=delete;
};
}
