#pragma once
#include <Arduino.h>
#include <WebResponseImpl.h>
#include <functional>
#include <memory>
#include "FirmwareReadWindow.h"

namespace FirmwareExport {
// Adapted from ESPAsyncWebServer 3.12.1 AsyncCallbackResponse, but an exhausted
// retry budget invalidates the source instead of falsely signaling clean EOF.
// AsyncAbstractResponse owns TCP abort on its next service callback. No request
// pointer is retained, and the lease survives cancellation and partial sends.
class ReadResponse final : public AsyncAbstractResponse {
 public:
  using Reader=std::function<ReadResult(uint32_t,uint8_t *,size_t)>;
  ReadResponse(uint32_t start,uint32_t length,Reader reader,std::shared_ptr<void> lease)
      : window_(start,length),reader_(std::move(reader)),lease_(std::move(lease)) {
    _code=200;_contentLength=length;_contentType="application/octet-stream";
  }
  bool _sourceValid() const override { return lease_ && reader_ && window_.valid(); }
  size_t _fillBuffer(uint8_t *output,size_t maximum) override {
    const auto result=window_.fill(output,maximum,millis(),reader_);
    if(result.kind==FillKind::Data) return result.size;
    if(result.kind==FillKind::End) return 0;
    // Abort leaves the source invalid; returning 0 here would mark RESPONSE_END
    // and make a truncated fixed-length download look like an ordinary EOF.
    return RESPONSE_TRY_AGAIN;
  }
 private:
  ReadWindow window_;
  Reader reader_;
  std::shared_ptr<void> lease_;
};
} // namespace FirmwareExport
