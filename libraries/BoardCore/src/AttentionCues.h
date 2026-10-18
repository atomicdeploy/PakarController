#pragma once
#include <atomic>
#include <stdint.h>
#include <string.h>

// Finite, coalesced notices only. Async writers publish intent; only the loop
// may send UART. Acoustic delivery is never required to authorize an update.
namespace BoardCore { namespace Attention {
enum class Cue : uint8_t { None, Start, Success, Failure };
class Upload {
 public:
  void begin() { state_.store(Active | static_cast<uint32_t>(Cue::Start)); }
  void finish(bool success) {
    uint32_t before=state_.load();
    while ((before&Active) && !(before&Terminal)) {
      const uint32_t next=Active|Terminal|static_cast<uint32_t>(success?Cue::Success:Cue::Failure);
      if(state_.compare_exchange_weak(before,next)) return;
    }
  }
  Cue take() { return static_cast<Cue>(state_.fetch_and(~Pending)&Pending); }
 private:
  static constexpr uint32_t Pending=3,Active=4,Terminal=8;
  std::atomic<uint32_t> state_{0};
};
class Remote {
 public:
  Cue next(uint32_t job,const char *phase) {
    if(job!=job_) { job_=job; observed_=started_=terminal_=false; }
    if(!job || !phase || terminal_) return Cue::None;
    if(!strcmp(phase,"prepare")) {
      observed_=true;
      if(!started_) { started_=true; return Cue::Start; }
    }
    if(!strcmp(phase,"manifest") || !strcmp(phase,"download_esp") ||
       !strcmp(phase,"download_avr") || !strcmp(phase,"program_avr") ||
       !strcmp(phase,"program_esp") || !strcmp(phase,"awaiting_reboot") ||
       !strcmp(phase,"verify_pair") || !strcmp(phase,"release_pair")) observed_=true;
    if(!strcmp(phase,"complete") || !strcmp(phase,"failed") || !strcmp(phase,"cancelled")) {
      terminal_=true;
      if(observed_) return !strcmp(phase,"complete")?Cue::Success:Cue::Failure;
    }
    return Cue::None;
  }
 private:
  uint32_t job_=0;
  bool observed_=false,started_=false,terminal_=false;
};
} }
