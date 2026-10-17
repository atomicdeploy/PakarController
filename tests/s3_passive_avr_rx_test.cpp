#include "PassiveAvrRx.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

using Capture = PakarS3::PassiveRxCapture<128>;
static void boundsAndRetention() {
  Capture capture;
  assert(!capture.snapshot().active && capture.snapshot().stored == 0);
  assert(!capture.begin(9600, 0));
  assert(capture.begin(38400, 10));
  for (unsigned i = 0; i < 180; ++i) capture.record(static_cast<uint8_t>(i));
  auto state = capture.snapshot();
  assert(state.received == 180 && state.stored == 128 && state.dropped == 52);
  // A new request cannot replace a still-owned capture or erase its bytes.
  assert(!capture.begin(115200, 20) && capture.snapshot().received == 180);
  uint8_t bytes[32] = {};
  assert(capture.readChunk(0, 32, bytes) == 32 && bytes[0] == 0 && bytes[31] == 31);
  assert(capture.readChunk(120, 32, bytes) == 8 && bytes[0] == 120 && bytes[7] == 127);
  memset(bytes, 0xCC, sizeof(bytes));
  assert(capture.readChunk(129, 1, bytes) == 0 && bytes[0] == 0xCC);
  assert(capture.readChunk(128, 1, bytes) == 0 && bytes[0] == 0xCC);
  assert(capture.readChunk(0, 33, bytes) == 0 && bytes[0] == 0xCC);
  assert(capture.readChunk(0, 1, nullptr) == 0);
  assert(capture.readChunk(0, 0, nullptr) == 0);
  capture.stop(30);
  state = capture.snapshot();
  assert(!state.active && state.stopReason == Capture::StopReason::Requested && state.elapsedMs == 20);
  capture.record(255);
  capture.service(9000);
  assert(capture.snapshot().received == 180 && capture.snapshot().dropped == 52);
  assert(capture.readChunk(127, 1, bytes) == 1 && bytes[0] == 127);
  // Failed requests preserve a stopped capture too; only a successful begin resets.
  assert(!capture.begin(0, 10000) && capture.snapshot().stored == 128);
  assert(capture.begin(115200, 10000));
  assert(capture.snapshot().stored == 0 && capture.snapshot().received == 0 && capture.snapshot().dropped == 0);
}
static void deadlineAndRollover() {
  Capture capture;
  assert(capture.begin(115200, UINT32_MAX - 100));
  capture.record(0xA5);
  capture.service(7898); // 7999ms elapsed across rollover.
  assert(capture.snapshot().active && capture.snapshot().elapsedMs == 7999);
  capture.service(7899);
  const auto ended = capture.snapshot();
  assert(!ended.active && ended.elapsedMs == 8000 && ended.stopReason == Capture::StopReason::Deadline);
  uint8_t byte = 0;
  assert(capture.readChunk(0, 1, &byte) == 1 && byte == 0xA5);
  capture.stop(9000, Capture::StopReason::UnsafePins);
  assert(capture.snapshot().stopReason == Capture::StopReason::Deadline && capture.snapshot().elapsedMs == 8000);
}
static void exactSmallCapacity() {
  PakarS3::PassiveRxCapture<4> capture;
  assert(capture.begin(38400, 1));
  for (unsigned i = 0; i < 6; ++i) capture.record(static_cast<uint8_t>(i + 1));
  assert(capture.snapshot().stored == 4 && capture.snapshot().dropped == 2);
  uint8_t bytes[8] = {};
  assert(capture.readChunk(3, 4, bytes) == 1 && bytes[0] == 4);
  assert(capture.readChunk(5, 1, bytes) == 0);
  capture.stop(2, decltype(capture)::StopReason::UnsafePins);
  assert(capture.snapshot().stopReason == decltype(capture)::StopReason::UnsafePins);
}
int main() {
  boundsAndRetention(); deadlineAndRollover(); exactSmallCapacity();
  puts("Production passive capture helper:128-byte bound, independent drop counts, chunk guards, retained bytes and8000ms rollover deadline passed. No SDK/GPIO/UART or physical tests.");
}
