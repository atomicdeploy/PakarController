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
#if !defined(PAKAR_S3_LED_USE_DMA) && defined(ARDUINO_ARCH_ESP32)
// Only LED application units depend on this canonical, ignored build profile.
// Changing the backend must not invalidate unrelated SDK/FastLED objects.
#if __has_include(<build/config/esp32s3/LedBackend.h>)
#include <build/config/esp32s3/LedBackend.h>
#endif
#endif
#ifndef PAKAR_S3_LED_USE_DMA
#define PAKAR_S3_LED_USE_DMA 0
#endif
static_assert(PAKAR_S3_LED_USE_DMA==0 || PAKAR_S3_LED_USE_DMA==1,"LED DMA selection must be boolean");

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
