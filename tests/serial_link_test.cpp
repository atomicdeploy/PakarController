// Adapt actual PCController Tools/VirtualBoard/tests/uart_protocol_test.cpp
// representative/max, nested response, split RX, noise and rejection scenarios.
#include <SerialLink.h>
#include <assert.h>
#include <stdio.h>
#include <vector>
using namespace BoardCore;
using namespace BoardCore::Protocol;
struct FaultSerial final : HardwareSerial {
  size_t calls = 0, failAt = static_cast<size_t>(-1), acceptOnFailure = 0;
  size_t write(const uint8_t *data, size_t length) override {
    const bool fail = calls++ == failAt;
    return HardwareSerial::write(data, fail && acceptOnFailure < length ? acceptOnFailure : length);
  }
};
std::vector<uint8_t> encodeFrame(uint8_t opcode, const std::vector<uint8_t> &payload = {}) {
  Frame frame{}; frame.opcode = opcode; frame.sequence = 23;
  frame.payloadLength = static_cast<uint8_t>(payload.size());
  if (!payload.empty()) memcpy(frame.payload, payload.data(), payload.size());
  uint8_t encoded[kMaximumEncoded]{};
  const uint8_t size = encode(frame, encoded, sizeof(encoded)); assert(size);
  return {encoded, encoded + size};
}
struct Capture { SerialLink *link; std::vector<Frame> frames; bool maximumReply = false; };
void capture(const Frame &frame, void *context) {
  auto &value = *static_cast<Capture *>(context);
  const Frame before = frame;
  value.frames.push_back(frame);
  if (value.maximumReply) assert(value.link->send(kPcContractResponse, frame.sequence, frame.payload, frame.payloadLength));
  else assert(value.link->ack(frame.sequence, frame.opcode));
  assert(memcmp(&frame, &before, sizeof(frame)) == 0); // Nested TX cannot overwrite RX.
}
void drain(SerialLink &link, HardwareSerial &serial) {
  size_t passes = 0;
  while (serial.available() > 0) { assert(++passes < 100); link.service(); }
}
int main() {
  static_assert(UartFlow::unsolicited == (PAKAR_USE_BUSY_WIRE || PAKAR_UART_BUFFERED), "Flow profile");
  assert(!UartFlow::ready(false, 0, kMaximumEncoded));
  assert(UartFlow::ready(true, kMaximumEncoded - 1, kMaximumEncoded) == bool(PAKAR_USE_BUSY_WIRE));
  assert(UartFlow::ready(false, kMaximumEncoded, kMaximumEncoded) == bool(PAKAR_UART_BUFFERED));
  assert(UartFlow::receiveBudget(-1) == 0 && UartFlow::receiveBudget(0) == 0);
  assert(UartFlow::receiveBudget(5000) == 64);
  FaultSerial serial; SerialLink link(serial); Capture value{&link,{}};
  link.begin(38400, capture, &value); assert(serial.baud == 38400);
  std::vector<uint8_t> maximum(kMaximumPayload);
  for (size_t n = 0; n < maximum.size(); ++n) maximum[n] = n % 3 ? n : 0;
  serial.feed(encodeFrame(kPing, {0,0,1,0,2,0,0}));
  serial.feed(encodeFrame(0xFE, maximum)); drain(link, serial);
  assert(value.frames.size() == 2 && value.frames[1].opcode == 0xFE);
  assert(memcmp(value.frames[1].payload, maximum.data(), maximum.size()) == 0);
  // Actual donor split-RX/synchronous-TX isolation, without its macro scratch API.
  const auto split = encodeFrame(kDisplayText, {0,4,0,5,6,0});
  serial.feed({split.begin(), split.begin() + split.size()/2}); link.service();
  assert(value.frames.size() == 2); assert(link.ack(1, kPing));
  serial.feed({split.begin() + split.size()/2, split.end()}); drain(link, serial);
  assert(value.frames.size() == 3 && value.frames.back().payloadLength == 6);
  // Large hardware ring must yield, not drain 2048 bytes in one firmware loop.
  serial.feed(std::vector<uint8_t>(2048, 0x55)); const auto before = serial.reads;
  link.service(); assert(serial.reads - before == 64 && serial.available() > 0);
  drain(link, serial); const auto noiseErrors = link.errors(); assert(noiseErrors == 1);
  serial.feed({0}); serial.feed(encodeFrame(kGetStatus)); drain(link, serial);
  assert(value.frames.size() == 4); // Delimiter recovery after oversized noise.
  serial.feed({5,1,2,0});
  auto crc = encodeFrame(kGetStatus); crc[crc.size()-2] ^= 0x55; serial.feed(crc);
  drain(link, serial); assert(value.frames.size() == 4 && link.errors() == noiseErrors + 2);
  // Missing read/negative occupancy do not synthesize bytes or loop forever.
  serial.feed(encodeFrame(kGetStatus)); serial.negativeAvailable = true; link.service();
  serial.negativeAvailable = false; serial.missingRead = true; link.service();
  assert(value.frames.size() == 4); serial.missingRead = false; drain(link, serial);
  assert(value.frames.size() == 5);
  // Header succeeds, then a body fragment fails. This is an unknown partial
  // packet even when that particular failing write accepted zero bytes.
  serial.failAt = serial.calls + 1; assert(!link.send(kPing, 1));
  serial.failAt = static_cast<size_t>(-1);
  assert(!link.send(kPing,1,nullptr,1)); // Failed write is not accepted/replayed.
  const auto partialEnd = serial.written.size();
  assert(link.send(kPing, 2));
  assert(serial.written[partialEnd] == 0); // Boundary repair, not replay of sequence1.
  Frame repaired{};
  assert(decode(serial.written.data() + partialEnd + 1,
      static_cast<uint8_t>(serial.written.size() - partialEnd - 2), repaired));
  assert(repaired.sequence == 2);
  // Byte-exact compatibility with the existing encoder across every bounded
  // length, zeros/code-block boundaries and the maximum 48-byte tunnel reply.
  FaultSerial tx; SerialLink streaming(tx); streaming.begin(38400, nullptr);
  for (uint8_t n = 0; n <= kMaximumPayload; ++n) {
    for (uint8_t pattern = 0; pattern < 3; ++pattern) {
      std::vector<uint8_t> payload(n);
      for (uint8_t i = 0; i < n; ++i) payload[i] = pattern == 0 ? 0 : pattern == 1 ? 0xFF : i % 3 ? i : 0;
      tx.written.clear(); assert(streaming.send(kPcContractResponse, 23, payload.data(), n));
      assert(tx.written == encodeFrame(kPcContractResponse, payload));
    }
  }
  // Each fragment position can fail: no replay, no successful-frame counter,
  // and boundary repair only if any prefix was actually transmitted.
  const auto golden = encodeFrame(kPing, {0,1,0,2,0});
  FaultSerial probe; SerialLink probing(probe); probing.begin(38400, nullptr);
  assert(probing.send(kPing,23, std::vector<uint8_t>{0,1,0,2,0}.data(),5));
  const size_t fragments = probe.calls;
  for (size_t fail = 0; fail < fragments; ++fail) {
    FaultSerial broken; SerialLink sender(broken); sender.begin(38400, nullptr);
    const uint8_t payload[] = {0,1,0,2,0};
    broken.failAt = fail;
    assert(!sender.send(kPing,23,payload,sizeof(payload)));
    const size_t prefix = broken.written.size();
    assert(prefix < golden.size());
    for (size_t i = 0; i < prefix; ++i) assert(broken.written[i] == golden[i]);
#if defined(ESP8266) || defined(ESP32)
    assert(sender.framesTransmitted() == 0 && sender.bytesTransmitted() == prefix);
#endif
    if (prefix) {
      // Failure of the repair delimiter must keep recovery armed and must not
      // begin transmitting the newly requested independent frame.
      broken.failAt = broken.calls;
      assert(!sender.send(kPing,24));
      assert(broken.written.size() == prefix);
    }
    broken.failAt = static_cast<size_t>(-1);
    assert(sender.send(kPing,25));
    const size_t start = prefix + (prefix ? 1 : 0);
    if (prefix) assert(broken.written[prefix] == 0);
    Frame next{};
    assert(decode(broken.written.data()+start,
        static_cast<uint8_t>(broken.written.size()-start-1),next));
    assert(next.sequence == 25);
  }
  FaultSerial nestedSerial; SerialLink nested(nestedSerial); Capture nestedCapture{&nested,{},true};
  nested.begin(38400,capture,&nestedCapture);
  nestedSerial.feed(encodeFrame(kPing,maximum)); drain(nested,nestedSerial);
  assert(nestedCapture.frames.size() == 1);
  assert(nestedSerial.written == encodeFrame(kPcContractResponse,maximum));
  // A body write can itself accept a nonzero prefix before failing.
  FaultSerial partial; SerialLink partialLink(partial); partialLink.begin(38400,nullptr);
  const uint8_t data[] = {4,5,6}; partial.failAt = 3; partial.acceptOnFailure = 1;
  assert(!partialLink.send(kPing,23,data,sizeof(data)));
  const size_t partialSize = partial.written.size(); assert(partialSize > 1);
  partial.failAt = static_cast<size_t>(-1); assert(partialLink.send(kPing,26));
  assert(partial.written[partialSize] == 0);
  // Stream-owned S3 explicit pin/buffer setup must not be overwritten by begin.
  HardwareSerial configured; configured.baud = 115200;
  SerialLink external(static_cast<Stream &>(configured)); external.begin(38400,nullptr);
  assert(configured.baud == 115200);
#if defined(ESP8266) || defined(ESP32)
  assert(link.bytesReceived() > 2048 && link.framesReceived() == 5);
  assert(link.bytesTransmitted() > 0 && link.framesTransmitted() >= 6);
#endif
  puts("Actual shared SerialLink: bounded hardware RX, nested/split frame safety, noisy recovery, distinct profile readiness and ESP counters pass; no physical UART proof.");
}
