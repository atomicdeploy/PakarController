#pragma once
#include <stdint.h>
#include <stddef.h>
#include <deque>
#include <vector>
class Stream {
 public:
  virtual ~Stream() = default;
  virtual int available() = 0;
  virtual int read() = 0;
  virtual size_t write(const uint8_t *, size_t) = 0;
};
class HardwareSerial : public Stream {
 public:
  std::deque<uint8_t> received;
  std::vector<uint8_t> written;
  bool negativeAvailable = false, missingRead = false, shortWrite = false;
  uint32_t baud = 0;
  size_t reads = 0;
  void begin(uint32_t value) { baud = value; }
  int available() override { return negativeAvailable ? -1 : static_cast<int>(received.size()); }
  int read() override {
    if (missingRead || received.empty()) return -1;
    ++reads; const auto value = received.front(); received.pop_front(); return value;
  }
  size_t write(const uint8_t *data, size_t length) override {
    const size_t accepted = shortWrite && length ? length - 1 : length;
    written.insert(written.end(), data, data + accepted); return accepted;
  }
  void feed(const std::vector<uint8_t> &bytes) { received.insert(received.end(), bytes.begin(), bytes.end()); }
};
