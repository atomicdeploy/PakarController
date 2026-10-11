#pragma once
#include <stdint.h>
#include <string.h>
namespace BoardCore {
// No hardware mutation until both bounded chunks match. A truncated start is
// abandoned after 200ms. New starts replace only incomplete assembly.
class PcContractAssembly {
 public:
  bool accept(uint8_t sequence, const uint8_t *p, uint8_t n, uint32_t now) {
    if (!p || !n) return false;
    if (p[0] == 0 && n == 48 && p[2] == 48) {
      sequence_ = sequence; at_ = now; active_ = true;
      data_[0] = p[1]; memcpy(data_ + 1, p + 3, 45); return false;
    }
    if (p[0] == 1 && n == 4 && active_ && sequence == sequence_ &&
        static_cast<uint32_t>(now - at_) < 200) {
      memcpy(data_ + 46, p + 1, 3); active_ = false; return true;
    }
    active_ = false; return false;
  }
  const uint8_t *data() const { return data_; }
 private:
  uint8_t data_[49] = {};
  uint8_t sequence_ = 0;
  uint32_t at_ = 0;
  bool active_ = false;
};
}
