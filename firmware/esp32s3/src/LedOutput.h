#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include <atomic>
#include <mutex>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <driver/rmt_tx.h>
#include <driver/rmt_encoder.h>
#include "BoardPins.h"

namespace PakarS3 {
class LedOutput {
 public:
  static constexpr size_t pixels=480;
  bool begin();
  void submit(const CRGB *rgb,uint8_t brightness,bool enabled,uint32_t revision);
  bool canAccept()const{return queue_ && uxQueueMessagesWaiting(queue_)==0;}
  String json() const;
  static bool acquireFlashPause();
  static void releaseFlashPause();
 private:
  struct Frame {CRGB rgb[pixels];uint32_t revision;uint8_t brightness;bool enabled;};
  static void run(void *context){static_cast<LedOutput*>(context)->work();}
  void work();
  QueueHandle_t queue_=nullptr;
  StaticQueue_t queueState_{};
  alignas(4) uint8_t queueBytes_[sizeof(Frame)]{};
  // Worker-owned frame: producers can only overwrite the separate mailbox.
  Frame current_{};
  std::atomic<bool> ready_{false},active_{false};
  std::atomic<uint32_t> submitted_{0},transmitted_{0},failed_{0},revision_{0};
  std::atomic<int> error_{0};
  static std::mutex wireGate_;
  static std::atomic<uint32_t> flashHolds_;
  static std::atomic<bool> inFlight_;
};
}
