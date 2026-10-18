#pragma once
#include <cstddef>
#include <cstdint>

// A failed flash read is NOT end-of-file. The caller maps Retry to the SDK's
// RESPONSE_TRY_AGAIN sentinel, and Abort to an invalid response source.
namespace FirmwareExport {
enum class ReadResult { Complete, Retryable, Fatal };
enum class FillKind { Data, End, Retry, Abort };
struct FillResult { FillKind kind; size_t size; };
class ReadWindow {
 public:
  static constexpr uint8_t kMaximumFailures = 5;
  static constexpr uint32_t kFailureWindowMs = 2000;
  ReadWindow(uint32_t start, uint32_t length) : start_(start), length_(length) {}
  template<class Reader>
  FillResult fill(uint8_t *output, size_t maximum, uint32_t now, Reader &&read) {
    if (aborted_) return {FillKind::Abort,0};
    if (offset_ == length_) return {FillKind::End,0};
    if (!output || maximum == 0) return {FillKind::Retry,0};
    const size_t remaining=length_-offset_;
    const size_t count=maximum<remaining ? maximum : remaining;
    const auto result=read(start_+offset_,output,count);
    if (result==ReadResult::Complete) {
      offset_+=static_cast<uint32_t>(count);failures_=0;
      return {FillKind::Data,count};
    }
    if (failures_==0) firstFailure_=now;
    ++failures_;
    if (result==ReadResult::Fatal || failures_>=kMaximumFailures ||
        static_cast<uint32_t>(now-firstFailure_)>=kFailureWindowMs) {
      aborted_=true;return {FillKind::Abort,0};
    }
    return {FillKind::Retry,0};
  }
  bool valid() const { return !aborted_; }
  uint32_t offset() const { return offset_; }
 private:
  uint32_t start_,length_,offset_=0,firstFailure_=0;
  uint8_t failures_=0;
  bool aborted_=false;
};
} // namespace FirmwareExport
